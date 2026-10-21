#!/usr/bin/env python3
"""Fits compact mean-element orbits for the app's planetary moons to the JPL
satellite ephemerides (via the Horizons API) over 1900-2200, and writes
SolarSystem/SatelliteOrbits.hpp.

Model, per moon, planetocentric ICRF: a fixed Laplace-plane pole; inside it a
Kepler ellipse whose node and periapsis precess linearly; mean longitude
lambda0 + n t + n2 t^2 plus up to three resonance librations A sin(phi + nu t).
Usage: satellite_fit.py <cache dir> SolarSystem/SatelliteOrbits.hpp"""
import json, os, sys, time, urllib.parse, urllib.request
from multiprocessing import Pool
import numpy as np
from scipy.optimize import least_squares

J2000 = 2451545.0
JD_FIRST, JD_LAST = 2451544.5, 2524589.5   # stages grow from 2000-01-01; JPL's Triton and Charon stop 2199-12-29
JD_1900 = 2415020.5

# name, NAIF id, centre id, period (d), initial Laplace pole RA/Dec (deg)
MOONS = [('Phobos', 401, 499, 0.3187, 317.7, 52.9), ('Deimos', 402, 499, 1.2625, 316.6, 53.5),
         ('Io', 501, 599, 1.7627, 268.1, 64.5), ('Europa', 502, 599, 3.5255, 268.1, 64.5),
         ('Ganymede', 503, 599, 7.1556, 268.2, 64.6), ('Callisto', 504, 599, 16.6904, 268.7, 64.8),
         ('Mimas', 601, 699, 0.9424, 40.6, 83.5), ('Enceladus', 602, 699, 1.3702, 40.6, 83.5),
         ('Dione', 604, 699, 2.7369, 40.6, 83.5), ('Rhea', 605, 699, 4.5175, 40.6, 83.5),
         ('Titan', 606, 699, 15.9454, 36.4, 84.0), ('Iapetus', 608, 699, 79.3310, 288.7, 78.9),
         ('Miranda', 705, 799, 1.4135, 257.3, -15.2), ('Ariel', 701, 799, 2.5204, 257.3, -15.2),
         ('Umbriel', 702, 799, 4.1442, 257.3, -15.2), ('Titania', 703, 799, 8.7059, 257.3, -15.2),
         ('Oberon', 704, 799, 13.4632, 257.3, -15.2), ('Triton', 801, 899, 5.8770, 299.8, 43.1),
         ('Charon', 901, 999, 6.3872, 133.0, -6.2)]


def horizons(cache, target, centre, start, stop, intervals):
    path = os.path.join(cache, '%s_%d_%.1f_%.1f_%d.json' % (target, centre, start, stop, intervals))
    if not os.path.exists(path):
        params = {'format': 'json', 'COMMAND': "'%s'" % target, 'EPHEM_TYPE': "'VECTORS'",
                  'CENTER': "'500@%d'" % centre, 'START_TIME': "'JD%.6f'" % start, 'STOP_TIME': "'JD%.6f'" % stop,
                  'STEP_SIZE': "'%d'" % intervals, 'REF_PLANE': "'FRAME'", 'REF_SYSTEM': "'ICRF'",
                  'VEC_TABLE': "'1'", 'CSV_FORMAT': "'YES'", 'OUT_UNITS': "'KM-S'", 'TIME_TYPE': "'TDB'"}
        url = 'https://ssd.jpl.nasa.gov/api/horizons.api?' + urllib.parse.urlencode(params)
        for attempt in range(8):
            try:
                text = json.load(urllib.request.urlopen(url, timeout=600))['result']
                break
            except Exception:
                time.sleep(30 * (attempt + 1))
        else:
            raise RuntimeError('Horizons failed for %s' % target)
        if '$$SOE' not in text:
            raise RuntimeError(text[-2000:])
        open(path, 'w').write(text)
    text = open(path).read()
    rows = text.split('$$SOE')[1].split('$$EOE')[0].strip().splitlines()
    data = np.array([[float(f) for i, f in enumerate(r.split(',')[:5]) if i != 1] for r in rows])
    return data[:, 0] - J2000, data[:, 1:4]


def unit(ra, dec):
    return np.array([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra), np.sin(dec)])


def laplace_axes(ra, dec):
    z = unit(ra, dec)
    x = np.cross([0.0, 0.0, 1.0], z)
    x /= np.linalg.norm(x)
    return x, np.cross(z, x), z


def solve_kepler(M, e):
    E = M + e * np.sin(M)
    for _ in range(12):
        E -= (E - e * np.sin(E) - M) / (1 - e * np.cos(E))
    return E


# p = [ra, dec, a, h, k, varpi_dot, p, q, node_dot, lambda0, n, n2, (A, phi, nu)*]
def model(p, t):
    ra, dec, a, h, k, wdot, pp, qq, odot, lam0, n, n2 = p[:12]
    lam = lam0 + n * t + n2 * t * t
    for j in range(12, len(p), 3):
        lam = lam + p[j] * np.sin(p[j + 1] + p[j + 2] * t)
    e = np.hypot(h, k)
    varpi = np.arctan2(h, k) + wdot * t
    inc = 2 * np.arctan(np.hypot(pp, qq))
    node = np.arctan2(pp, qq) + odot * t
    w = varpi - node
    E = solve_kepler(lam - varpi, e)
    xo, yo = a * (np.cos(E) - e), a * np.sqrt(1 - e * e) * np.sin(E)
    cO, sO, cw, sw, ci, si = np.cos(node), np.sin(node), np.cos(w), np.sin(w), np.cos(inc), np.sin(inc)
    P = np.array([cO * cw - sO * sw * ci, sO * cw + cO * sw * ci, sw * si])
    Q = np.array([-cO * sw - sO * cw * ci, -sO * sw + cO * cw * ci, cw * si])
    local = P * xo + Q * yo
    X, Y, Z = laplace_axes(ra, dec)
    return np.outer(local[0], X) + np.outer(local[1], Y) + np.outer(local[2], Z)


def fit(p, t, xyz, scale, free_pole=True):
    # On short arcs the Laplace pole trades off against inclination and node (one
    # plane, many descriptions), so it stays at JPL's value until the node has moved.
    if free_pole:
        res = lambda q: ((model(q, t) - xyz) / scale).ravel()
        return least_squares(res, p, method='lm', x_scale='jac', max_nfev=4000).x
    res = lambda q: ((model(np.concatenate([p[:2], q]), t) - xyz) / scale).ravel()
    return np.concatenate([p[:2], least_squares(res, p[2:], method='lm', x_scale='jac', max_nfev=4000).x])


def errors_deg(p, t, xyz):
    return np.degrees(np.linalg.norm(model(p, t) - xyz, axis=1) / np.linalg.norm(xyz, axis=1))


def longitude_residual(p, t, xyz):
    """Data minus model along the orbit, rad, unwrapped in time: exact however large it gets."""
    r = model(p, t)
    h = np.cross(r, model(p, t + 1e-3))
    h /= np.linalg.norm(h, axis=1)[:, None]
    return np.unwrap(np.arctan2(np.einsum('ij,ij->i', np.cross(r, xyz), h), np.einsum('ij,ij->i', r, xyz)))


def fix_longitude(p, t, xyz, max_librations=3):
    """Absorbs the along-track residual into lambda0, n, n2 and resonance librations,
    found afresh: ones fitted on a shorter span extrapolate badly."""
    p = p[:12].copy()
    for _ in range(max_librations + 1):
        d = longitude_residual(p, t, xyz)
        poly = np.polyfit(t, d, 2)
        p[9] += poly[2]; p[10] += poly[1]; p[11] += poly[0]
        rest = d - np.polyval(poly, t)
        if np.abs(rest).max() < np.radians(0.05) or (len(p) - 12) // 3 >= max_librations:
            break
        span = t.max() - t.min()
        nus = 2 * np.pi / np.geomspace(20.0, 2.0 * span, 4000)
        power = [abs(np.mean(rest * np.exp(-1j * nu * t))) for nu in nus]
        nu = nus[int(np.argmax(power))]
        amp = 2 * np.mean(rest * np.exp(-1j * nu * t))
        if abs(amp) < np.radians(0.02):
            break
        # rest ~ Re(amp e^{i(-nu t)})... written as A sin(phi + nu t)
        p = np.concatenate([p, [abs(amp), -np.angle(amp) + np.pi / 2, nu]])
        p = fit_longitude_only(p, t, xyz)
    return p


def fit_longitude_only(p, t, xyz):
    base = longitude_residual(p, t, xyz) + lam_terms(p, t)
    def res(q):
        full = p.copy(); full[9:12] = q[:3]; full[12:] = q[3:]
        return base - lam_terms(full, t)
    q = least_squares(res, np.concatenate([p[9:12], p[12:]]), method='lm', x_scale='jac').x
    p = p.copy(); p[9:12] = q[:3]; p[12:] = q[3:]
    return p


def lam_terms(p, t):
    lam = p[9] + p[10] * t + p[11] * t * t
    for j in range(12, len(p), 3):
        lam = lam + p[j] * np.sin(p[j + 1] + p[j + 2] * t)
    return lam


def fit_moon(cache, name, target, centre, period, ra0, dec0):
    stages = [(JD_FIRST, 30 * period, 1500), (JD_FIRST, 730.0, 3000), (JD_FIRST, 365.25 * 30, 6000),
              (JD_FIRST, JD_LAST - JD_FIRST, 12000), (JD_1900, JD_LAST - JD_1900, 18000)]
    p = None
    for start, span, count in stages:
        t, xyz = horizons(cache, target, centre, start, start + span, count)
        a = np.median(np.linalg.norm(xyz, axis=1))
        if p is None:
            best = None
            for lam0 in np.linspace(0, 2 * np.pi, 12, endpoint=False):
                for pq in ([0.01, 0.01], [3.0, 3.0]):    # prograde and retrograde starts
                    trial = fit(np.array([np.radians(ra0), np.radians(dec0), a, 0.001, 0.001, 0.0, *pq, 0.0, lam0,
                                          2 * np.pi / period, 0.0]), t, xyz, a, free_pole=False)
                    err = errors_deg(trial, t, xyz).max()
                    if best is None or err < best[0]:
                        best = (err, trial)
            p = best[1]
        else:
            p = fix_longitude(p, t, xyz)
            p = fit(p, t, xyz, a, free_pole=span > 365.25 * 100)
    # Independent check on a dense, unseen stretch late in the span.
    tv, xv = horizons(cache, target, centre, 2506000.5, 2506000.5 + 40 * period, 2000)
    return p, errors_deg(p, t, xyz), errors_deg(p, tv, xv)


def main(cache, out):
    os.makedirs(cache, exist_ok=True)
    rows = []
    only = os.environ.get('ONLY', '').split(',')
    moons = [m for m in MOONS if only == [''] or m[0] in only]
    with Pool(min(8, len(moons))) as pool:
        results = pool.starmap(fit_moon, [(cache, *m) for m in moons])
    for (name, *_), (p, err, check) in zip(moons, results):
        print('%-10s max %.3f deg  rms %.3f deg  unseen-2149 max %.3f deg  librations %d'
              % (name, err.max(), np.sqrt(np.mean(err ** 2)), check.max(), (len(p) - 12) // 3), flush=True)
        p = [float(x) for x in p]   # plain floats: numpy 2 reprs as np.float64(...)
        ra, dec, a, h, k, wdot, pp, qq, odot, lam0, n, n2 = p[:12]
        terms = ', '.join('{%r, %r, %r}' % tuple(p[j:j + 3]) for j in range(12, len(p), 3))
        rows.append('        {"%s", %r, %r, %r, %r, %r, %r, %r, %r, %r, %r, %r, %r, %r, {%s}},  // max error %.2f deg' % (
            name, ra, dec, a, np.hypot(h, k), np.arctan2(h, k), wdot, 2 * np.arctan(np.hypot(pp, qq)),
            np.arctan2(pp, qq), odot, lam0, n, n2, max(err.max(), check.max()), terms, max(err.max(), check.max())))
    with open(out, 'w') as f:
        f.write('''#pragma once
#include <vector>

// GENERATED by tools/satellite_fit.py: mean-element fits to the JPL satellite
// ephemerides (Horizons) over 1900-2200. Do not edit.
namespace Astro {

    struct Libration { double amplitude, phase, rate; };   // rad, rad, rad/day on the mean longitude

    // Planetocentric ICRF. Angles in rad, rates in rad/day, a in km, t in days from J2000 TDB.
    struct SatelliteOrbit {
        const char* name;
        double poleRa, poleDec;        // Laplace plane pole
        double a, e;
        double periapsisLongitude0, periapsisRate;
        double inclination;
        double node0, nodeRate;        // in the Laplace plane, from its ascending node on the ICRF equator
        double meanLongitude0, meanMotion, meanMotionDrift;
        double fitErrorDeg;
        std::vector<Libration> librations;
    };

    inline const std::vector<SatelliteOrbit>& satelliteOrbits() {
        static const std::vector<SatelliteOrbit> table = {
%s
        };
        return table;
    }
}
''' % '\n'.join(rows))


if __name__ == '__main__':
    main(*sys.argv[1:3])
