#!/usr/bin/env python3
"""Builds resources/asteroid_belt.bin from the JPL Small-Body Database: the osculating
elements of every numbered main-belt asteroid brighter than H 15 (~58,000).

    curl -G https://ssd-api.jpl.nasa.gov/sbdb_query.api --data-urlencode 'fields=a,e,i,om,w,ma,epoch,H' \\
         --data-urlencode 'sb-kind=a' --data-urlencode 'sb-ns=n' --data-urlencode 'sb-class=MBA' \\
         --data-urlencode 'sb-cdata={"AND":["H|LT|15"]}' -o mba_h15.json
    python3 tools/asteroid_belt.py mba_h15.json resources/asteroid_belt.bin

Kepler orbits about the Sun only, so a few degrees off after a century: a picture of the
belt and its Kirkwood gaps, not an ephemeris. File, little-endian: b'BELTKEP1', float64
epoch JD (TDB), int32 count, then per body float32 a (AU), e, i, node, periapsis, mean
anomaly at epoch (rad, ecliptic J2000) and mean motion (rad/day)."""
import json, struct, sys
import numpy as np

K = 0.01720209895   # Gauss's constant, rad/day for a = 1 AU

d = json.load(open(sys.argv[1]))
f = d['fields']
rows = np.array([[float(r[f.index(k)]) for k in ('a', 'e', 'i', 'om', 'w', 'ma', 'epoch')] for r in d['data']])
epochs = np.unique(rows[:, 6])
assert len(epochs) == 1, 'one epoch expected, got %d' % len(epochs)
a, e = rows[:, 0], rows[:, 1]
assert (a > 1.5).all() and (a < 5.5).all() and (e < 1).all(), 'not main-belt elements'
out = np.column_stack([a, e, np.radians(rows[:, 2:6]), K / a ** 1.5]).astype('<f4')
with open(sys.argv[2], 'wb') as fh:
    fh.write(b'BELTKEP1')
    fh.write(struct.pack('<di', epochs[0], len(out)))
    fh.write(out.tobytes())
print(len(out), 'asteroids, epoch JD', epochs[0])
