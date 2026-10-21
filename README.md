# Solar System — AI-slopped but NASA-accurate edition

> This branch is the original [Fundamentals of Computer Graphics](https://github.com/andre6226/sistema-solare-opengl)
> project (University of Genoa) after an AI pair-programming session that went a
> little overboard. The renderer is the same; the sky it draws is now the real one,
> to the second, from **1900 to 2200**. See `main` for the course version.

A real-time solar system renderer in modern OpenGL (3.3 core), built with SFML and
GLM. Pick any date between 1900 and 2200 and every body sits where it really was,
or will be, turned the way it really faces.

## What "accurate" means here

| What | Source | Checked against | Result |
|---|---|---|---|
| Sun, planets, Moon | JPL **DE440** ephemeris | JPL `testpo.440` | 2 mm worst case |
| Solar eclipses | DE440 + light time + aberration | NASA Five Millennium Catalog, 23 total/annular eclipses 1901–2200 | greatest eclipse within **0.5 s** (TD), gamma to 4 decimals |
| Earth's rotation | IAU 2006 precession, DE440 nutation, Earth Rotation Angle | subsolar point vs equation of time and declination | < 0.3° |
| Clock (UTC ↔ TT) | ΔT measured by USNO, 1900–2026 | — | exact for the past, a forecast after 2026 |
| Poles, spin, shape | IAU WGCCRE 2015 (NAIF `pck00011.tpc`) | — | oblate Saturn, Jupiter, Earth; real prime meridians |
| Moon's orientation | DE440 lunar librations | — | the near side faces Earth, with libration |
| 19 planetary moons | JPL satellite ephemerides (Horizons) | held-out JPL samples | mean orbit + correction tables, target < 1 km (*tables being generated*) |
| Saturn's rings | texture measured on the Cassini division and Encke gap | — | C ring to F ring at the real radii |

**The one thing no model fixes:** after 2026 the time shown is UTC predicted
with a forecast of ΔT, the drift of Earth's rotation. Eclipses stay exact in
dynamical time, but their UTC clock time can be off by about a minute in 2100
and by several minutes by 2200. Nobody can do better until it happens.

## Gallery

![Earth](pictures/Earth.png)

*Earth*

![Moon and Earth](pictures/MoonAndEarth.png)

*Moon, Earth and Sun*

![Phobos and Mars](pictures/PhobosAndMars.png)

*Phobos and Mars*

![Saturn](pictures/Saturn.png)

*Saturn*

## Build and run

```
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
./build/SolarSystem.bin
```

SFML and GLM are fetched by CMake on the first configure. The build copies
`resources/` (including the ~28 MB ephemeris and the moon tables) and the shaders
next to the binary.

## Controls

* **Mouse:** left button to orbit the view, wheel to zoom.
* **Navigation:** `UP` parent, `DOWN` first moon, `LEFT`/`RIGHT` siblings.
* **Time:** `T` opens the date picker (1900–2200, UTC); `SHIFT`/`CTRL` speed
  time up or down, `SPACE` back to real time.
* **Orbits:** `O` toggles the orbit lines. The focused body's own orbit is hidden.

## Try it

* **2026-08-12 17:46 UTC:** total solar eclipse over Spain; look at Earth and
  find the Moon's shadow.
* **2027-08-02 10:07 UTC:** the long eclipse over North Africa and Egypt.
* **1919-05-29 13:08 UTC:** the eclipse that confirmed general relativity.
* **1999-08-11 11:03 UTC:** the last total eclipse over central Europe in the 20th century.

## Regenerating the data

Everything in `resources/` that comes from NASA/JPL is rebuilt by the scripts
in `tools/`, from public files:

| Script | Input | Output |
|---|---|---|
| `tools/de440_extract.py` | DE440 ASCII files from `ssd.jpl.nasa.gov/ftp/eph/planets/ascii/de440/` | `resources/de440_1900_2200.bin` |
| `tools/pck_extract.py` | `pck00011.tpc` from NAIF | `SolarSystem/IauRotation.hpp` |
| `tools/satellite_fit.py` | Horizons API | `SolarSystem/SatelliteOrbits.hpp` |
| `tools/satellite_tables.py` | Horizons API | `resources/moons/*.bin` |

## Tests

```
bash tests/run_tests.sh
```

Includes the eclipse check against the NASA catalogue and the Earth
orientation check.

## License

Source code under the MIT License, see [LICENSE](LICENSE). JPL/NASA ephemeris
data are public domain. Third-party files (the `glad/` loader, textures, font
and shape models in `resources/`) remain under the terms of their authors.
