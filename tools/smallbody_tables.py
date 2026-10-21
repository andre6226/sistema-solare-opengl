#!/usr/bin/env python3
"""Builds resources/bodies/<Name>.bin: Chebyshev tables of the position of the
asteroids and dwarf planets from the JPL small-body ephemerides (Horizons),
1900-2200, to under 1 km. Barycentric, because heliocentric positions carry the
Sun's own wobble (Mercury alone moves it ~10 km every 88 days): the app subtracts
the Sun from DE440.

File layout, little-endian: b'BODYCHEB', float64 first JD (TDB), float64 segment
length (days), int32 segment count, int32 coefficients per axis, then per segment
x, y, z coefficient runs as float64 (km, ICRF, from the solar-system barycentre).
Usage: smallbody_tables.py <cache dir> resources/bodies"""
import os, struct, sys
import numpy as np
from multiprocessing import Pool
import satellite_fit as sf
import satellite_tables as st

JD_START = 2415020.5     # 1900-01-01
JD_STOP = 2524959.5      # 2201-01-01, the end of the app's clock
# Horizons integrates small bodies per request: over spans much longer than this
# its answers drift by up to 15 km, so each request covers at most this many days.
CHUNK_DAYS = 10000
TARGET_KM = 0.8
# name, Horizons id, sampling step (days), candidate segment lengths (days)
BODIES = [('Ceres', '1;', 1, (8, 16, 32, 64)), ('Pallas', '2;', 1, (8, 16, 32, 64)),
          ('Vesta', '4;', 1, (8, 16, 32, 64)), ('Ida', '243;', 1, (8, 16, 32, 64)),
          ('Eros', '433;', 1, (8, 16, 32, 64)), ('Gaspra', '951;', 1, (8, 16, 32, 64)),
          ('Haumea', '136108;', 8, (256, 512, 1024, 2048)), ('Eris', '136199;', 8, (256, 512, 1024, 2048)),
          ('Makemake', '136472;', 8, (256, 512, 1024, 2048)),
          # JPL's 1P solution #75; inside 0.6 AU of the Sun a day is too coarse to sample.
          ('Halley', 'DES=1P;CAP;NOFRAG', 0.25, (2, 4, 8, 16, 32))]


def build(cache, body, out_dir):
    name, target, step, segs = body
    total = int(np.ceil((JD_STOP - JD_START) / step)) + 1   # last sample on or after JD_STOP
    t, r = [], []
    chunk = int(CHUNK_DAYS / step)
    for first in range(0, total, chunk):
        n = min(chunk, total - first)
        a = JD_START + first * step
        tc, xyz = sf.horizons(cache, target, 0, a, a + (n - 1) * step, n - 1)
        t.append(tc)
        r.append(xyz)
    t, r = np.concatenate(t), np.concatenate(r)
    # Cheapest layout under target on the downloaded samples themselves: they
    # outnumber the coefficients 6 to 40 times, and a single Horizons request long
    # enough to serve as an independent check would drift more than the fit.
    start = JD_START - sf.J2000
    best = None
    for seg in segs:
        for k in (6, 8, 10, 12, 14, 16):
            if seg / step < 2 * k:
                continue
            c = st.segment_fit(t, r, start, seg, k, 1, None, np.float64)
            worst = np.linalg.norm(st.evaluate(c, start, seg, t) - r, axis=1).max()
            size = (JD_STOP - JD_START) / seg * 3 * k * 8
            if worst < TARGET_KM and (best is None or size < best[0]):
                best = (size, seg, k, worst)
    assert best, name + ': no layout under target'
    size, seg, k, worst = best
    count = int(np.ceil((JD_STOP - JD_START) / seg))
    coeffs = st.segment_fit(t, r, start, seg, k, 1, None, np.float64)
    assert all(s in coeffs for s in range(count)), name
    with open(os.path.join(out_dir, name + '.bin'), 'wb') as f:
        f.write(b'BODYCHEB')
        f.write(struct.pack('<2d2i', JD_START, seg, count, k))
        for s in range(count):
            f.write(coeffs[s].tobytes())
    return '%-9s segment %4d d  %2d coeffs  %5.2f MB  fit max %.3f km' % (name, seg, k, size / 1e6, worst)


def main(cache, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    with Pool(len(BODIES)) as pool:
        for line in pool.starmap(build, [(cache, b, out_dir) for b in BODIES]):
            print(line, flush=True)


if __name__ == '__main__':
    main(*sys.argv[1:3])
