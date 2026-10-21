"""resources/Ceres.jpg from the Dawn FC global mosaic (DLR, Feb 2016, 140 m/pixel).

    curl -LO https://planetarymaps.usgs.gov/mosaic/Ceres_Dawn_FC_DLR_global_59ppd_Feb2016.cub
    python3 tools/ceres_texture.py Ceres_Dawn_FC_DLR_global_59ppd_Feb2016.cub resources/Ceres.jpg

The cube is simple cylindrical, planetocentric, positive east, longitude -180..180,
the same layout as the other maps (Occator's bright spots land at 239 E, 20 N).
"""
import sys
import numpy as np
from PIL import Image

cub, out = sys.argv[1:3]
# From the cube's label: 8-bit, one band, tiles of 267 x 128 after a 64 KiB label.
S, L, TS, TL, START = 21093, 10546, 267, 128, 65536
tx, ty = -(-S // TS), -(-L // TL)
raw = np.fromfile(cub, np.uint8, count=tx * ty * TS * TL, offset=START)
if raw.size != tx * ty * TS * TL:
    sys.exit(cub + ': truncated')
img = raw.reshape(ty, tx, TL, TS).transpose(0, 2, 1, 3).reshape(ty * TL, tx * TS)[:L, :S]
a = np.asarray(Image.fromarray(img).resize((4096, 2048), Image.LANCZOS), np.float32)

# Dawn never saw the south pole below -85.5 deg in daylight: mirror the band above it.
dark = np.nonzero(a.mean(1) < 10)[0]
dark = dark[dark > 1024]
if dark.size:
    r0 = dark.min()
    a[r0:] = a[r0 - 1:2 * r0 - 2049:-1]

# Same mean brightness as the albedo map this replaces, so Ceres keeps its exposure.
a = np.clip(a * (63.6 / a.mean()), 0, 255)
Image.fromarray(a.astype(np.uint8)).convert('RGB').save(out, quality=85, optimize=True)
