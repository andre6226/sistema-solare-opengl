#!/usr/bin/env python3
"""Builds resources/moons/<Name>.bin: Chebyshev tables of each moon's offset from
its mean-element orbit (SolarSystem/SatelliteOrbits.hpp) so that model + table
matches the JPL satellite ephemerides (Horizons) to under 1 km, 1900-2200.

File layout, little-endian: b'MOONCHEB', float64 first JD (TDB), float64 segment
length (days), int32 segment count, int32 coefficients per axis, then per
segment x, y, z coefficient runs as float32 (km, ICRF).
Usage: satellite_tables.py <cache dir> SolarSystem/SatelliteOrbits.hpp resources/moons"""
import os, re, struct, sys
import numpy as np
from multiprocessing import Pool
from numpy.polynomial import chebyshev as cheb
import satellite_fit as sf

TARGET_KM = 0.8          # leaves margin under the 1 km promise
JD_START = 2415020.5     # 1900-01-01
JD_STOP = 2524589.5      # 2199-12-29: JPL's Triton and Charon end there; later the last segment is held
CHUNK = 60000            # Horizons rows per request


def fitted_params(header):
    """The mean-element parameters, back in satellite_fit's own form."""
    out = {}
    for m in re.finditer(r'\{"(\w+)", ([^{]*), \{(.*?)\}\}', open(header).read()):
        v = [float(x) for x in m.group(2).split(',') if x.strip()]
        ra, dec, a, e, varpi, wdot, inc, node, odot, lam0, n, n2 = v[:12]
        p = [ra, dec, a, e * np.sin(varpi), e * np.cos(varpi), wdot, np.tan(inc / 2) * np.sin(node),
             np.tan(inc / 2) * np.cos(node), odot, lam0, n, n2]
        for lib in re.findall(r'\{?([^{}]+)', m.group(3)):  # the last term's '}' went with the '}}'
            if not lib.strip(', '):
                continue
            p += [float(x) for x in lib.split(',') if x.strip()]
        out[m.group(1)] = np.array(p)
    return out


def segment_fit(t, r, start, seg, nc, pad, only=None):
    """Per-segment Chebyshev fit; pad extra samples on each side pin the segment edges."""
    coeffs = {}
    for s in (np.unique(np.floor((t - start) / seg).astype(int)) if only is None else only):
        lo = max(np.searchsorted(t, start + s * seg) - pad, 0)
        hi = np.searchsorted(t, start + (s + 1) * seg) + pad
        x = 2.0 * (t[lo:hi] - (start + s * seg)) / seg - 1.0
        coeffs[s] = np.array([cheb.chebfit(x, r[lo:hi, a], nc - 1) for a in range(3)], dtype=np.float32)
    return coeffs


def evaluate(coeffs, start, seg, t):
    u = (t - start) / seg
    s = np.floor(u).astype(int)
    x = 2.0 * (u - s) - 1.0
    return np.array([[cheb.chebval(x[i], coeffs[s[i]][a].astype(float)) for a in range(3)] for i in range(len(t))])


# Download grid per moon: segment in orbital periods, coefficients (2 samples each).
# Picked once on 40 orbits of 2023; pinned so that the cached Horizons replies stay valid.
SAMPLING = {'Phobos': (2, 14), 'Deimos': (2, 14), 'Io': (1, 12), 'Europa': (1, 12), 'Ganymede': (0.25, 8),
            'Callisto': (0.25, 12), 'Mimas': (0.5, 8), 'Enceladus': (1, 12), 'Dione': (1, 12), 'Rhea': (1, 12),
            'Titan': (0.5, 10), 'Iapetus': (0.125, 12), 'Miranda': (1, 10), 'Ariel': (1, 14), 'Umbriel': (1, 12),
            'Titania': (0.5, 10), 'Oberon': (0.5, 12), 'Triton': (1, 8), 'Charon': (2, 8)}


def build(cache, moon, p, out_dir):
    name, target, centre, period = moon[:4]
    seg, nc = SAMPLING[name][0] * period, SAMPLING[name][1]
    samples = 2 * nc                       # per download segment, ~2x oversampled
    step = seg / samples
    total = int(np.ceil((JD_STOP - JD_START) / seg)) * samples
    chunk = (CHUNK // samples) * samples   # whole segments per request
    t, r = [], []
    for first in range(0, total, chunk):
        n = min(chunk, total - first)
        a = JD_START + (first + 0.5) * step  # mid-steps, so no row sits on a segment edge
        try:
            tc, xyz = sf.horizons(cache, target, centre, a, a + (n - 1) * step, n - 1)
        except RuntimeError as e:  # Charon's JPL coverage ends right at JD_STOP: stop the last request there
            if 'No ephemeris' not in str(e):
                raise
            n = int((JD_STOP - a) / step) + 1
            tc, xyz = sf.horizons(cache, target, centre, a, a + (n - 1) * step, n - 1)
        t.append(tc)
        r.append(xyz - sf.model(p, tc))
    t, r = np.concatenate(t), np.concatenate(r)
    # The 2023 sample underrates 300 years of residual: pick the stored layout on
    # JPL points off the download grid, refitting the downloaded samples.
    th, xyz = sf.horizons(cache, target, centre, JD_START + 0.1234567, JD_STOP - 0.3712, 1499)
    rh = xyz - sf.model(p, th)
    start = JD_START - sf.J2000
    best = None
    for split in (1, 2, 3, 4):
        for k in (6, 8, 10, 12, 14):
            if samples / split + 2 < k + 2:
                continue
            sub = seg / split
            c = segment_fit(t, r, start, sub, k, 1, np.unique(((th - start) / sub).astype(int)))
            worst = np.linalg.norm(evaluate(c, start, sub, th) - rh, axis=1).max()
            size = (JD_STOP - JD_START) / sub * 3 * k * 4
            if worst < TARGET_KM and (best is None or size < best[2]):
                best = (sub, k, size, worst)
    assert best, name + ': no layout under target'
    sub, k, size, worst = best
    count = int(np.ceil((JD_STOP - JD_START) / sub))
    coeffs = segment_fit(t, r, start, sub, k, 1)
    missing = [s for s in range(count) if s not in coeffs]
    assert not missing, (name, missing[:5])
    with open(os.path.join(out_dir, name + '.bin'), 'wb') as f:
        f.write(b'MOONCHEB')
        f.write(struct.pack('<2d2i', JD_START, sub, count, k))
        for s in range(count):
            f.write(coeffs[s].tobytes())
    return '%-10s segment %.4f d  %2d coeffs  %6.1f MB  selection max %.3f km' % (name, sub, k, size / 1e6, worst)


def main(cache, header, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    params = fitted_params(header)
    only = os.environ.get('ONLY', '').split(',')
    moons = [m for m in sf.MOONS if only == [''] or m[0] in only]
    with Pool(min(6, len(moons))) as pool:
        for line in pool.starmap(build, [(cache, m, params[m[0]], out_dir) for m in moons]):
            print(line, flush=True)


if __name__ == '__main__':
    main(*sys.argv[1:4])
