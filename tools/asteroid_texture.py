"""resources/<Body>.jpg from spacecraft global mosaics, 4096 x 2048.

    python3 tools/asteroid_texture.py Ceres Ceres_Dawn_FC_DLR_global_59ppd_Feb2016.cub resources/Ceres.jpg
    python3 tools/asteroid_texture.py Vesta Vesta_Dawn_FC_HAMO_Mosaic_Global_74ppd.tif resources/Vesta.jpg
    python3 tools/asteroid_texture.py Eros f4_16_00n000.png resources/Eros.jpg

Sources (planetarymaps.usgs.gov/mosaic/ for Dawn, NASA PDS Small Bodies Node for NEAR):
  Ceres  Dawn FC global mosaic, DLR, Feb 2016, 140 m/pixel (ISIS cube, 214 MB)
  Vesta  Dawn FC HAMO global mosaic, DLR, 60 m/pixel, Claudia Double Prime = IAU 2015
         (W0 285.39), the system of SolarSystem/IauRotation.hpp (TIFF, 357 MB)
  Eros   NEAR MSI global basemap F4_16_00N000, M. Robinson (sbnarchive.psi.edu/pds4/near/
         near_msi_digital_image_maps/browse/basemaps/pngs/f4_16_00n000.png, 6.8 MB)
All end up simple cylindrical, positive east, -180 deg at the left edge like the other maps.
"""
import sys
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
body, src, out = sys.argv[1:4]
# Mean grey of the albedo maps these replace, so each body keeps its exposure in the app.
MEAN = {'Ceres': 63.6, 'Vesta': 123.4, 'Eros': 89.4}[body]

if src.endswith('.cub'):
    # Ceres's cube label: 8-bit, one band, tiles of 267 x 128 after a 64 KiB label.
    S, L, TS, TL, START = 21093, 10546, 267, 128, 65536
    tx, ty = -(-S // TS), -(-L // TL)
    raw = np.fromfile(src, np.uint8, count=tx * ty * TS * TL, offset=START)
    if raw.size != tx * ty * TS * TL:
        sys.exit(src + ': truncated')
    img = Image.fromarray(raw.reshape(ty, tx, TL, TS).transpose(0, 2, 1, 3).reshape(ty * TL, tx * TS)[:L, :S])
else:
    img = Image.open(src).convert('L')
a = np.asarray(img.resize((4096, 2048), Image.LANCZOS), np.float32)
if body == 'Eros':
    a = np.roll(a, 2048, axis=1)   # the NEAR map runs 0..360 E from its left edge

# Polar caps never seen in daylight: Ceres has no data below -85.5 deg, Vesta's north
# above ~65 N was imaged under a grazing Sun. Those rows (south: empty, north: darker
# than 60% of the mean grey) are replaced by the band next to them, mirrored.
rows = a.mean(1)
south = np.nonzero(rows < 10)[0]
south = south[south > 1648]
north = np.nonzero(rows < 0.6 * a.mean())[0]
north = north[north < 400]
if south.size:
    r0 = south.min()
    a[r0:] = a[r0 - 1:2 * r0 - 2049:-1]
if north.size:
    r1 = north.max() + 1
    a[:r1] = a[2 * r1 - 1:r1 - 1:-1]

a = np.clip(a * (MEAN / a.mean()), 0, 255)
Image.fromarray(a.astype(np.uint8)).convert('RGB').save(out, quality=85, optimize=True)
