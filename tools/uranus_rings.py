"""resources/UranusRings.png: the ten narrow rings of Uranus and the dusty zeta ring.

    python3 tools/uranus_rings.py \
        tools/ring_profile/uranus/uranus_occultation_ring_fit_rfrench_20201201.tab resources/UranusRings.png

Radii: semimajor axes from French's 2020 fit to Earth-based and Voyager stellar
occultations (PDS Rings Node, uranus_occs_earthbased bundle, uranus_occ_support).
Widths and normal optical depths: mean values of French et al. 1991 ("The Uranian
ring system", Uranus, U. Arizona Press). shortcut: rings drawn circular at their
semimajor axis; epsilon (e 0.008) swings 400 km in and out and from 20 to 96 km wide,
needs the ring shader to take per-ring ellipses if that is ever to show.
Each texel stores 1 - <e^-tau> over its span, as tools/ring_opacity.py does for Saturn,
so a 2 km ring in a 1.6 km texel keeps its share of blocked light.
"""
import sys
import numpy as np
from PIL import Image

INNER, OUTER = 37850.0, 51250.0     # zeta's inner edge to past epsilon: SolarSystem.hpp setRingProfile
WIDTH = 8192                        # 1.6 km a texel
# name in the fit file -> (width km, normal tau)
RINGS = {'six': (1.6, 0.3), 'five': (1.9, 0.5), 'four': (2.4, 0.3), 'alpha': (7.7, 0.4),
         'beta': (8.3, 0.2), 'eta': (1.6, 0.4), 'gamma': (2.8, 1.5), 'delta': (6.1, 0.4),
         'lambda': (1.5, 0.1), 'epsilon': (58.0, 1.2)}
ZETA = (37850.0, 41350.0, 0.001)    # dust, outside the occultation fits
LAMBDA = 50023.9                    # seen only by Voyager 2, not in the Earth-based fit
# Particles as dark as charcoal: albedo ~0.04 (Voyager 2), grey; stored sRGB.
ALBEDO = 0.04

tab, png = sys.argv[1:3]
radius = {}
for line in open(tab).read().splitlines()[1:]:
    f = [x.strip() for x in line.split(',')]
    radius[f[0]] = float(f[1])
radius['lambda'] = LAMBDA
assert set(RINGS) <= set(radius), set(RINGS) - set(radius)

# Fine grid, 0.05 km, then averaged per texel.
fine = 20
r = INNER + (OUTER - INNER) * (np.arange(WIDTH * fine) + 0.5) / (WIDTH * fine)
tau = np.where((r >= ZETA[0]) & (r < ZETA[1]), ZETA[2], 0.0)
for name, (w, t) in RINGS.items():
    tau = np.where(np.abs(r - radius[name]) <= w / 2, t, tau)
alpha = 1.0 - np.exp(-tau).reshape(WIDTH, fine).mean(axis=1)

grey = round(255 * (1.055 * ALBEDO ** (1 / 2.4) - 0.055))
out = np.empty((4, WIDTH, 4), np.uint8)
out[..., :3] = grey
out[..., 3] = np.maximum(np.round(255.0 * alpha), (alpha > 0) * 1.0)[None]   # zeta keeps its 1/255
Image.fromarray(out, 'RGBA').save(png)

# Self-check: each ring's blocked light survives the texel averaging (within a texel's rounding).
for name, (w, t) in RINGS.items():
    span = (OUTER - INNER) / WIDTH
    lo, hi = int((radius[name] - w / 2 - span - INNER) / span), int((radius[name] + w / 2 + span - INNER) / span) + 1
    stored = (out[0, lo:hi, 3] / 255.0).sum() * span
    assert abs(stored - w * (1 - np.exp(-t))) < 3 * span * 0.5 / 255 * (hi - lo) + 0.02 * w, (name, stored)
print('radii', {k: radius[k] for k in RINGS}, 'grey', grey)
