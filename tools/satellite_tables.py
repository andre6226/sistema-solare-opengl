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
from numpy.polynomial import chebyshev as cheb
import satellite_fit as sf

TARGET_KM = 0.8          # leaves margin under the 1 km promise
JD_START = 2415020.5     # 1900-01-01
JD_STOP = 2524593.5      # 2199-12-29, where Triton and Charon end
CHUNK = 60000            # Horizons rows per request


def fitted_params(header):
    """The mean-element parameters, back in satellite_fit's own form."""
    out = {}
    for m in re.finditer(r'\{"(\w+)", ([^{]*), \{(.*?)\}\}', open(header).read()):
        v = [float(x) for x in m.group(2).split(',') if x.strip()]
        ra, dec, a, e, varpi, wdot, inc, node, odot, lam0, n, n2 = v[:12]
        p = [ra, dec, a, e * np.sin(varpi), e * np.cos(varpi), wdot, np.tan(inc / 2) * np.sin(node),
             np.tan(inc / 2) * np.cos(node), odot, lam0, n, n2]
        for lib in re.findall(r'\{([^{}]*)\}', m.group(3)):
            p += [float(x) for x in lib.split(',')]
        out[m.group(1)] = np.array(p)
    return out


def segment_fit(t, r, start, seg, nc):
    idx = np.floor((t - start) / seg).astype(int)
    coeffs, worst = {}, 0.0
    for s in np.unique(idx):
        k = idx == s
        x = 2.0 * (t[k] - (start + s * seg)) / seg - 1.0
        c = np.array([cheb.chebfit(x, r[k, a], nc - 1) for a in range(3)], dtype=np.float32)
        worst = max(worst, np.abs(np.array([cheb.chebval(x, c[a].astype(float)) for a in range(3)]).T - r[k]).max())
        coeffs[s] = c
    return coeffs, worst


def choose_layout(cache, moon, p):
    """Cheapest (segment, coefficients) meeting the target on a dense sample."""
    name, target, centre, period = moon[:4]
    t, xyz = sf.horizons(cache, target, centre, 2460000.5, 2460000.5 + 40 * period, 8000)
    r = xyz - sf.model(p, t)
    best = None
    for seg in period * np.array([4.0, 2.0, 1.0, 0.5, 0.25, 0.125]):
        for nc in (8, 10, 12, 14):
            start = 2460000.5
            _, worst = segment_fit(t[t < start + seg * int(40 * period / seg)], r[t < start + seg * int(40 * period / seg)], start, seg, nc)
            size = (JD_STOP - JD_START) / seg * 3 * nc * 4
            if worst < TARGET_KM and (best is None or size < best[2]):
                best = (seg, nc, size)
    return best


def main(cache, header, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    params = fitted_params(header)
    only = os.environ.get('ONLY', '').split(',')
    for moon in sf.MOONS:
        name, target, centre = moon[:3]
        if only != [''] and name not in only:
            continue
        p = params[name]
        seg, nc, size = choose_layout(cache, moon, p)
        count = int(np.ceil((JD_STOP - JD_START) / seg))
        samples = 2 * nc                       # per segment, ~2x oversampled
        step = seg / samples
        total = count * samples
        coeffs, worst = {}, 0.0
        chunk = (CHUNK // samples) * samples   # whole segments per request
        for first in range(0, total, chunk):
            n = min(chunk, total - first)
            a = JD_START + (first + 0.5) * step  # sample mid-steps so no row sits on a segment edge
            t, xyz = sf.horizons(cache, target, centre, a, a + (n - 1) * step, n - 1)
            c, w = segment_fit(t, xyz - sf.model(p, t), JD_START - sf.J2000, seg, nc)
            coeffs.update(c)
            worst = max(worst, w)
        missing = [s for s in range(count) if s not in coeffs]
        assert not missing, (name, missing[:5])
        with open(os.path.join(out_dir, name + '.bin'), 'wb') as f:
            f.write(b'MOONCHEB')
            f.write(struct.pack('<2d2i', JD_START, seg, count, nc))
            for s in range(count):
                f.write(coeffs[s].tobytes())
        print('%-10s segment %.4f d  %2d coeffs  %6.1f MB  worst %.3f km' % (name, seg, nc, size / 1e6, worst), flush=True)


if __name__ == '__main__':
    main(*sys.argv[1:4])
