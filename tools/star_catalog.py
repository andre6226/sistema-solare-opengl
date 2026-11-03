#!/usr/bin/env python3
"""The naked-eye sky: the Yale Bright Star Catalogue (BSC5, 9,110 entries to V ~6.5)
as resources/stars.bin for SolarSystem/StarField.hpp.

    python3 tools/star_catalog.py tools/star_catalog/bsc5/bsc5.dat resources/stars.bin

bsc5.dat: CDS catalogue V/50 (cdsarc.cds.unistra.fr/ftp/V/50/catalog.gz), mirrored as
tdc-www.harvard.edu/catalogs/bsc5.dat.gz. Per star, float32: the J2000 direction in
ICRF (x, y, z; FK5 J2000 is within 0.05" of it), its proper motion as the rate of
change of that direction (rad/yr, x, y, z), V magnitude, and the colour of a black
body at the temperature its B-V gives (linear sRGB, brightest channel 1).
"""
import math
import struct
import sys

from umbra_light import cmf, planck, to_rgb   # CIE 1931 fit, Planck, XYZ -> linear sRGB

ARCSEC = math.pi / (180 * 3600)


def field(line, first, last):
    """Columns first..last, 1-based and inclusive as in the catalogue's ReadMe."""
    return line[first - 1:last].strip()


def temperature(bv):
    """Effective temperature from B-V (Ballesteros 2012)."""
    return 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62))


def colour(bv):
    t = temperature(min(max(bv, -0.4), 2.0))
    xyz = [0.0, 0.0, 0.0]
    for l in range(380, 781, 5):
        s = planck(l, t)
        c = cmf(l)
        for i in range(3):
            xyz[i] += s * c[i]
    rgb = [max(v, 0.0) for v in to_rgb(xyz)]
    m = max(rgb)
    return [v / m for v in rgb]


def parse(path):
    stars = []
    with open(path, encoding="latin-1") as f:
        for line in f:
            line = line.rstrip("\n").ljust(197)
            if not field(line, 76, 77) or not field(line, 103, 107):
                continue   # entries withdrawn from the catalogue (novae, non-stellar)
            hr = int(field(line, 1, 4))
            ra = math.radians(15 * (int(field(line, 76, 77)) + int(field(line, 78, 79)) / 60
                                    + float(field(line, 80, 83)) / 3600))
            dec = math.radians((int(field(line, 85, 86)) + int(field(line, 87, 88)) / 60
                                + int(field(line, 89, 90)) / 3600) * (-1 if field(line, 84, 84) == "-" else 1))
            vmag = float(field(line, 103, 107))
            bv = float(field(line, 110, 114) or 0.6)   # a few have no B-V: sunlike white
            pm_ra = float(field(line, 149, 154) or 0) * ARCSEC   # mu_alpha cos(delta)
            pm_de = float(field(line, 155, 160) or 0) * ARCSEC
            d = (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))
            east = (-math.sin(ra), math.cos(ra), 0.0)
            north = (-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra), math.cos(dec))
            pm = tuple(pm_ra * east[i] + pm_de * north[i] for i in range(3))
            stars.append((hr, d, pm, vmag, colour(bv)))
    return stars


def self_check(stars):
    by_hr = {s[0]: s for s in stars}
    assert len(stars) > 9000, len(stars)
    # Sirius (HR 2491): RA 6h45m08.9s, Dec -16 42 58, V -1.46, blue-white: ~9,600 K is
    # #CAD8FF on screen (linear 0.6, 0.69, 1), as in Charity's black-body colour table.
    hr, d, pm, v, c = by_hr[2491]
    ra = math.degrees(math.atan2(d[1], d[0])) / 15
    dec = math.degrees(math.asin(d[2]))
    assert abs(ra - (6 + 45 / 60 + 8.9 / 3600)) < 1e-4 and abs(dec + (16 + 42 / 60 + 58 / 3600)) < 1e-3 and v == -1.46
    assert c[2] == 1.0 and 0.5 < c[0] < 0.7, c
    # Arcturus (HR 5340) moves 2.28"/yr, along the sky; Betelgeuse (HR 2061) is orange-red.
    hr, d, pm, v, c = by_hr[5340]
    assert abs(math.sqrt(sum(x * x for x in pm)) / ARCSEC - 2.28) < 0.01
    assert abs(sum(d[i] * pm[i] for i in range(3))) < 1e-12
    assert by_hr[2061][4][2] < 0.6 < by_hr[2061][4][0]
    # Rigel (HR 1713) is blue-white: blue its brightest channel.
    assert by_hr[1713][4][2] == 1.0


def main():
    stars = parse(sys.argv[1])
    self_check(stars)
    with open(sys.argv[2], "wb") as out:
        out.write(b"STARS001")
        out.write(struct.pack("<i", len(stars)))
        for hr, d, pm, v, c in stars:
            out.write(struct.pack("<10f", *d, *pm, v, *c))
    print(f"{len(stars)} stars -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
