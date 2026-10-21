#!/usr/bin/env python3
"""Cuts the JPL DE440 ASCII ephemeris down to 1900-2200 as a little-endian binary
the app reads (SolarSystem/Ephemeris.hpp).
Source files: https://ssd.jpl.nasa.gov/ftp/eph/planets/ascii/de440/
Usage: de440_extract.py <dir with header.440 + ascp*.440> <out.bin>"""
import glob, os, struct, sys

JD_FROM, JD_TO = 2415012.5, 2525000.5   # a record either side of 1900-01-01 .. 2201-01-01

def numbers(path):
    for line in open(path):
        for tok in line.split():
            yield float(tok.replace('D', 'E'))

def main(src, out):
    head = open(os.path.join(src, 'header.440')).read().split('GROUP')
    group = {g.split()[0]: g.split()[1:] for g in head[1:]}
    ncoeff = int(head[0].split()[3])
    n = int(group['1040'][0])
    consts = dict(zip(group['1040'][1:1 + n], (float(v.replace('D', 'E')) for v in group['1041'][1:1 + n])))
    ipt = [int(v) for v in group['1050']]
    ipt = [(ipt[i], ipt[15 + i], ipt[30 + i]) for i in range(15)]

    records = {}
    for path in sorted(glob.glob(os.path.join(src, 'ascp*.440'))):
        it = numbers(path)
        for _rec_no in it:
            next(it)                                  # coefficient count
            rec = [next(it) for _ in range(ncoeff)]
            if rec[1] > JD_FROM and rec[0] < JD_TO:
                records[rec[0]] = rec
    starts = sorted(records)
    for a, b in zip(starts, starts[1:]):
        assert b - a == 32.0, (a, b)

    au, day = consts['AU'], 86400.0
    gm = [consts[k] * au ** 3 / day ** 2 for k in ('GMS', 'GM1', 'GM2', 'GMB', 'GM4', 'GM5', 'GM6', 'GM7', 'GM8', 'GM9')]
    with open(out, 'wb') as f:
        f.write(b'DE440BIN')
        f.write(struct.pack('<3d2i', starts[0], records[starts[-1]][1], 32.0, len(starts), ncoeff - 2))
        f.write(struct.pack('<39i', *[v for t in ipt[:13] for v in t]))
        f.write(struct.pack('<12d', consts['EMRAT'], au, *gm))
        for s in starts:
            f.write(struct.pack('<%dd' % (ncoeff - 2), *records[s][2:]))
    print(len(starts), 'records', starts[0], '->', records[starts[-1]][1])

if __name__ == '__main__':
    main(*sys.argv[1:3])
