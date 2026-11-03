#!/usr/bin/env python3
"""Names on the sky: the IAU proper names of the brighter stars and the 88 constellation
figures with their names, as resources/sky_labels.txt for SolarSystem/SkyLabels.hpp.

    python3 tools/sky_labels.py tools/star_catalog/names resources/sky_labels.txt

The folder holds IAU-CSN.txt (IAU WGSN list, pas.rochester.edu/~emamajek/WGSN/) and
constellations.json, constellations.lines.json (d3-celestial, github.com/ofrohn/d3-celestial,
BSD-3). All coordinates are J2000 RA/Dec in degrees, one entry per line:

    S ra dec V name        a star's name
    C ra dec name          a constellation's name, where its label goes
    L ra1 dec1 ra2 dec2    one segment of a constellation figure
"""
import json
import re
import sys
import unicodedata

NAME_MAG_LIMIT = 4.5   # fainter named stars are mostly exoplanet hosts nobody sees


def ascii(text):
    # TextRenderer draws ASCII only: Chasoň -> Chason
    return unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()


def stars(path):
    out = []
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        tok = line.split()
        date = next((i for i, t in enumerate(tok) if re.fullmatch(r"\d{4}-\d\d-\d\d", t)), None)
        if date is None:   # a header line that lost its '#'
            continue
        ra, dec, v = float(tok[date - 2]), float(tok[date - 1]), tok[date - 6]
        if v == "_" or float(v) > NAME_MAG_LIMIT:
            continue
        out.append((float(v), ra, dec, ascii(line[18:36].strip())))
    return sorted(out)


def constellations(folder):
    names = json.load(open(f"{folder}/constellations.json", encoding="utf-8"))["features"]
    lines = json.load(open(f"{folder}/constellations.lines.json", encoding="utf-8"))["features"]
    labels = [(f["geometry"]["coordinates"], f["properties"]["name"]) for f in names]
    segments = [(a, b) for f in lines for strip in f["geometry"]["coordinates"]
                for a, b in zip(strip, strip[1:])]
    return labels, segments


def self_check(named, labels, segments):
    by_name = {n: (v, ra, dec) for v, ra, dec, n in named}
    assert named[0][3] == "Sirius" and abs(by_name["Sirius"][1] - 101.287) < 0.01
    assert abs(by_name["Polaris"][2] - 89.264) < 0.01 and "Vega" in by_name and "Rigil Kentaurus" in by_name
    assert all(n.isascii() for *_, n in named)
    assert len(labels) == 89 and any(n == "Orion" for _, n in labels)   # Serpens in two parts
    assert len(segments) > 600


def main():
    folder, out = sys.argv[1], sys.argv[2]
    named = stars(f"{folder}/IAU-CSN.txt")
    labels, segments = constellations(folder)
    self_check(named, labels, segments)
    with open(out, "w") as f:
        for v, ra, dec, name in named:
            f.write(f"S {ra % 360:.5f} {dec:.5f} {v:.2f} {name}\n")
        for (ra, dec), name in labels:
            f.write(f"C {ra % 360:.3f} {dec:.3f} {name}\n")
        for (ra1, dec1), (ra2, dec2) in segments:
            f.write(f"L {ra1 % 360:.4f} {dec1:.4f} {ra2 % 360:.4f} {dec2:.4f}\n")
    print(f"{len(named)} star names, {len(labels)} constellations, {len(segments)} segments -> {out}")


if __name__ == "__main__":
    main()
