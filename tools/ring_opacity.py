"""Opacity of resources/SaturnRings.png from Cassini's measured optical depth.

    python3 tools/ring_opacity.py tools/ring_profile/RSS_2005_123_X43_E_TAU_10KM.TAB resources/SaturnRings.png

Source: NASA PDS Rings Node, CORSS_8001, Rev007E_RSS_2005_123_X43_E (Cassini radio
occultation, X band, 3 May 2005, 10 km resolution), pds-rings.seti.org/holdings/volumes/
CORSS_8xxx/CORSS_8001/data/Rev007/Rev007E/Rev007E_RSS_2005_123_X43_E/. Each texel's alpha
becomes the share of light a face-on ring blocks there, 1 - <e^-tau>, which is what
base.frag reads tau back from. The colours stay; texels the old texture left dark but
the profile fills (the C ring's inner edge) take the colour of the nearest lit texel.
"""
import sys
import numpy as np
from PIL import Image

INNER, OUTER = 0.502 * 140920.0, 140920.0   # the texture's span, base.frag RING_INNER
TAU_SATURATED = 5.0                          # where the signal is lost, the B ring core

tab, png = sys.argv[1:3]
radius, tau, threshold = np.loadtxt(tab, delimiter=',', usecols=(0, 6, 8)).T
# Above the threshold the measured tau is noise: the ring is at least that opaque.
tau = np.where(tau > threshold, TAU_SATURATED, np.clip(tau, 0.0, None))

im = np.asarray(Image.open(png).convert('RGBA')).astype(float)
h, w = im.shape[:2]
edges = INNER + (OUTER - INNER) * np.arange(w + 1) / w
alpha = np.zeros(w)
for i in range(w):
    inside = (radius >= edges[i]) & (radius < edges[i + 1])
    if inside.any():
        alpha[i] = 1.0 - np.exp(-tau[inside]).mean()

rgb = im[..., :3].mean(axis=0)               # rows are identical: one colour per radius
lit = np.flatnonzero(np.linalg.norm(rgb / 255.0, axis=1) >= 0.15)
rgb = rgb[lit[np.abs(lit[None, :] - np.arange(w)[:, None]).argmin(axis=1)]]

out = np.empty((h, w, 4), np.uint8)
out[..., :3] = np.round(rgb)[None]
out[..., 3] = np.round(255.0 * alpha)[None]
Image.fromarray(out, 'RGBA').save(png)

def mean_tau(r0, r1):
    sel = (radius >= r0) & (radius < r1)
    return -np.log(np.exp(-tau[sel]).mean())

# Self-check against the rings' known structure.
assert mean_tau(78000, 84000) < 0.3, 'C ring should be thin'
assert mean_tau(105000, 110000) > 2.0, 'B ring core should be opaque'
assert mean_tau(133450, 133600) < 0.1, 'Encke gap should be empty'
for name, r0, r1 in (('C', 74490, 91980), ('B', 91980, 117580), ('Cassini', 117580, 122170),
                     ('A', 122170, 136780)):
    print(f'{name:8s} tau {mean_tau(r0, r1):.2f}  alpha {1 - np.exp(-mean_tau(r0, r1)):.2f}')
