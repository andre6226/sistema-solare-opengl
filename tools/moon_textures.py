"""Moon maps and Earth's surface and clouds for resources/, from NASA/USGS global mosaics.

    python3 tools/moon_textures.py <download dir> resources

Files expected in <download dir> (planetarymaps.usgs.gov/mosaic/ unless noted):
  world.200408.3x21600x10800.jpg   Blue Marble Next Generation, August 2004, no clouds
                                   (eoimages.gsfc.nasa.gov/images/imagerecords/74000/74117/)
  cloud_combined_8192.tif          Blue Marble clouds (.../imagerecords/57000/57747/)
  lroc_color_poles_8k.tif          LRO colour Moon, CGI Moon Kit (svs.gsfc.nasa.gov/4720)
  Io_GalileoSSI-Voyager_Global_Mosaic_ClrMerge_1km.tif
  Europa_Voyager_GalileoSSI_global_mosaic_500m.tif
  Ganymede_Voyager_GalileoSSI_Global_ClrMosaic_1435m.tif
  Callisto_Voyager_GalileoSSI_global_mosaic_1km.tif
  Enceladus_Cassini_mosaic_global_110m.tif
  Tethys_Cassini_mosaic_global_293m.tif
  Phobos_Viking_Mosaic_40ppd_DLRcontrol.tif

Every output is simple cylindrical, east to the right, -180 deg at the left edge. The
USGS GeoTIFFs of Europa, Ganymede and Callisto have their central meridian at 180 deg
(left edge at 0 E), so they are rolled half a turn; the others already start at -180.
"""
import os
import sys
import numpy as np
from PIL import Image, ImageFilter

Image.MAX_IMAGE_PIXELS = None
src, res = sys.argv[1:3]

# name, file, width, roll half a turn, keep the source brightness
JOBS = [
    ('Earth', 'world.200408.3x21600x10800.jpg', 8192, False, True),
    ('Moon', 'lroc_color_poles_8k.tif', 4096, False, True),
    ('Io', 'Io_GalileoSSI-Voyager_Global_Mosaic_ClrMerge_1km.tif', 4096, False, False),
    ('Europa', 'Europa_Voyager_GalileoSSI_global_mosaic_500m.tif', 4096, True, False),
    ('Ganymede', 'Ganymede_Voyager_GalileoSSI_Global_ClrMosaic_1435m.tif', 4096, True, False),
    ('Callisto', 'Callisto_Voyager_GalileoSSI_global_mosaic_1km.tif', 4096, True, False),
    ('Enceladus', 'Enceladus_Cassini_mosaic_global_110m.tif', 4096, False, False),
    ('Tethys', 'Tethys_Cassini_mosaic_global_293m.tif', 4096, False, False),
    ('Phobos', 'Phobos_Viking_Mosaic_40ppd_DLRcontrol.tif', 4096, False, False),
]

# Mean colour of the maps these replaced, so exposure and tint stay as they were, with
# the grey capped at 170: brighter, a detailed map clips into white. Tethys's old map
# was Mimas's.
MEAN_RGB = {
    'Io': (192.4, 179.6, 138.0), 'Europa': (177.8, 172.8, 159.3), 'Ganymede': (172.9, 170.6, 166.5),
    'Callisto': (156.8, 151.4, 144.2), 'Enceladus': (155.5, 155.8, 155.4), 'Tethys': (171.1, 170.3, 168.6),
    'Phobos': (112.4, 111.5, 109.5),
}


def fill_gaps(a, valid):
    """Fills pixels no spacecraft imaged with the blurred mean of the imaged ones around them."""
    out = a.copy()
    todo = ~valid
    for radius in (4, 16, 64, 256):
        if not todo.any():
            break
        w = np.asarray(Image.fromarray((valid * 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(radius)), np.float32)
        for c in range(a.shape[2]):
            v = np.asarray(Image.fromarray(np.clip(a[..., c] * valid, 0, 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(radius)), np.float32)
            hit = todo & (w > 8)
            out[..., c][hit] = v[hit] * 255 / w[hit]
        todo &= ~(w > 8)
    out[todo] = a[valid].mean(0)
    return out


for name, f, width, roll, keep in JOBS:
    im = Image.open(os.path.join(src, f))
    grey = im.mode in ('L', 'I;16', 'F')
    im = im.convert('L' if grey else 'RGB').resize((width, width // 2), Image.LANCZOS)
    a = np.asarray(im, np.float32)
    a = a[..., None] if grey else a
    if roll:
        a = np.roll(a, width // 2, axis=1)
    valid = a.max(2) >= 2   # 0 marks no data in the USGS mosaics
    if not keep and not valid.all():
        a = fill_gaps(a, valid)
    if not keep:
        old = np.array(MEAN_RGB[name])
        a = a.mean(2, keepdims=True) if grey else a
        a = a * (old / a.reshape(-1, a.shape[2]).mean(0))
    a = np.broadcast_to(a, a.shape[:2] + (3,))
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(res, name + '.jpg'), quality=88, optimize=True)
    print(name, 'done')

# Clouds: white where cloudy, black where clear; the shader uses the grey level as opacity.
# The composite keeps a faint imprint of the ground under clear sky (coastlines up to ~60
# of 255): drawn over the real surface it shows every coast twice, so it is cut away.
c = np.asarray(Image.open(os.path.join(src, 'cloud_combined_8192.tif')).convert('L').resize((8192, 4096), Image.LANCZOS), np.float32)
c = Image.fromarray(np.clip((c - 40.0) * 255.0 / 215.0, 0, 255).astype(np.uint8))
c.save(os.path.join(res, 'EarthClouds.jpg'), quality=95, optimize=True)   # lower, 8x8 JPEG blocks show in thin cloud
print('EarthClouds done')
