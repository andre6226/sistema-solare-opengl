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
| 20 planetary moons | JPL satellite ephemerides (Horizons) | JPL samples the tables never saw, 1900–2200 | mean orbit + correction tables: **< 1 km** for every moon (worst: Miranda, 0.92 km), 145 MB |
| Asteroids and dwarf planets (Ceres, Pallas, Vesta, Ida, Eros, Gaspra, Haumea, Eris, Makemake) | JPL small-body ephemerides (Horizons) | 20 short JPL requests per body the tables never saw, 1900–2200 | **< 0.3 km**, median tens of metres, 2 MB |
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
`resources/` (including the ~28 MB ephemeris and the moon and small-body tables)
and the shaders next to the binary on every build, changed files only.

## Controls

* **Mouse:** left button to orbit the view, wheel to zoom, from skimming the
  surface out to ~100 AU, where the whole solar system fits on screen.
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

## How it works

### Sun, planets and the Moon: JPL DE440

[DE440](https://ssd.jpl.nasa.gov/planets/eph_export.html) is the ephemeris JPL
uses to fly spacecraft: Chebyshev polynomials fitted to a numerical integration
of the whole solar system, with every body, relativity and the Moon's interior.
`tools/de440_extract.py` cuts the public ASCII release down to 1900–2200 and
writes it as raw doubles (`resources/de440_1900_2200.bin`, 28 MB);
`SolarSystem/Ephemeris.hpp` evaluates the polynomials. Positions come out
relative to the solar-system barycentre and are turned into heliocentric ones by
subtracting the Sun at the same instant. That is an exact change of origin, so
the Sun sits still at the centre of the scene while every relative position
stays JPL's. Earth is separated from the Earth–Moon barycentre with the DE440
mass ratio. DE440's Pluto is likewise the Pluto–Charon barycentre, 2,100 km
outside Pluto: the app moves it to Pluto's centre using Charon's position and
mass, which puts both within 0.3 km of JPL.

### Time

The date picker speaks UTC; the ephemeris runs on TDB, which keeps step with the
atomic clocks. They differ by ΔT (TT − UT1), 64 s in 2000 and 69 s today, and it
changes because Earth's spin is irregular. ΔT comes from the USNO measurements
for each year from 1900 to 2026 (`Astro::deltaTSeconds`). After that it follows
tidal braking (+32 s per century²) from today's value and trend. That forecast is
the only error source in the whole program that cannot be removed.

### Eclipses and the speed of light

The shadow that reaches Earth now was cast by sunlight that left the Sun
8.3 minutes ago and passed the Moon 1.3 seconds ago. In that time Earth and
Moon have moved about 15,000 km along their orbit. Drawing everything at the
same instant puts the shadow some 40 km off and makes every eclipse 33 s late
against the NASA catalogue. So for the shadow geometry Earth is placed one solar
light time back along its path (the 20.5″ annual aberration: the Sun then sits
where Earth sees it), and the Moon one lunar light time back
(`earthForShadows`/`moonForShadows`). With both, 23 catalogue eclipses from 1901
to 2200 come out within 0.5 s. The Sun's own drift round the barycentre during
those 8 minutes (~6 km) moves the shadow by about 20 m.

How dark each point is comes from the share of the Sun's disc the Moon (or
Earth) hides as seen from there: the overlap of the two angular discs, which gives
the umbra, the penumbra and the annular ring without special cases. Earth's
shadow is 1/85 wider than the planet, for the opaque lower atmosphere (Danjon's
rule, the one NASA's lunar eclipse catalogue uses): the umbral magnitude of
2026-08-28 comes out 0.933 against NASA's 0.930. Inside it, the Moon gets the
sunlight Earth's atmosphere bends in: deep red near the axis, copper towards the
edge. That glow is a chosen colour, not a computed one. On the Moon the lit share
is gamma-encoded for the eye, or the sliver of a partial eclipse, lit by 3% of
the Sun, would look black.

### Which way things face

* **Earth:** body-fixed frame from IAU 2006 precession, the IAU 1980 nutation
  carried by DE440 and Greenwich sidereal time from the Earth Rotation Angle,
  i.e. the standard chain that takes ITRS to the celestial frame. The mesh is
  an oblate spheroid (6378.137 × 6356.752 km): the 21 km of flattening moves
  the shadow track by tens of km near the limb.
* **Moon:** the lunar libration angles integrated in DE440, so the near side
  faces Earth and nods by the real ±7°.
* **Everything else:** the IAU WGCCRE 2015 models (pole right ascension and
  declination, prime meridian angle and their periodic terms) and triaxial radii,
  generated from NAIF's `pck00011.tpc` into `SolarSystem/IauRotation.hpp` by
  `tools/pck_extract.py`. Saturn is 10% flatter at the poles than at the equator
  and Jupiter 6.5%.

### The planets' moons

JPL keeps a separate high-precision ephemeris for each moon system (MAR099,
JUP365, SAT441, URA182, NEP097, PLU060), served by the
[Horizons API](https://ssd.jpl.nasa.gov/horizons/). They are far too large to
ship, so each moon is stored in two layers:

1. **A mean orbit** (`tools/satellite_fit.py` → `SolarSystem/SatelliteOrbits.hpp`):
   a Kepler ellipse in the moon's Laplace plane, with nodal and apsidal
   precession, a slow drift of the mean motion (Phobos is spiralling into Mars),
   and up to three resonance librations found automatically from the residuals.
   Mimas, locked in a resonance with Tethys, swings ±44° about its mean position
   every 70 years, and Tethys ±2.1° on the same beat. The fit grows from a 30-orbit arc to 2 years, 30 years and
   finally 300 years, so the orbital phase is never lost. The along-track error
   is measured as an unwrapped angle, so even large errors are corrected exactly.
   On the short arcs the Laplace pole is held at JPL's published value: there it
   cannot be told apart from inclination and node, and leaving it free once sent
   Triton's orbit plane 19° astray. Over 300 years these orbits stay within
   0.004°–0.4° of JPL.
2. **A correction table** (`tools/satellite_tables.py` → `resources/moons/*.bin`):
   Chebyshev polynomials of what is left between JPL and the mean orbit, in
   segments from half an orbit to two orbits long. The correction is a smooth
   curve of a few thousand km at most, so it packs far tighter than raw positions
   would. Horizons is sampled twice per coefficient over the full 300 years (22 million
   rows in all); each segment is fitted together with one sample
   from each neighbour, which pins its ends (without that, the edges were off by
   up to 8 km). The segment length and number of coefficients are then chosen per
   moon as the cheapest pair that stays under 0.8 km on 1,500 JPL positions off
   the sampling grid, and the result is checked on another 1,200. JPL's tables
   for Triton and Charon end on 2199-12-29, so the correction tables stop there
   too. Through 2200 the moons follow the mean orbit with the last correction
   held fixed, and are only as good as the mean orbit (0.004°–0.4°).

Orbit lines are drawn from the mean orbit; the body itself gets mean orbit +
correction.

### Asteroids and dwarf planets

They are not in DE440, so their positions come from JPL's small-body
ephemerides through Horizons (`tools/smallbody_tables.py` →
`resources/bodies/*.bin`): Chebyshev polynomials in float64, in 64-day segments
for the asteroids and 2,048-day segments for the dwarf planets. The tables hold
the position from the solar-system barycentre, and the app subtracts the DE440
Sun: heliocentric positions carry the Sun's own wobble (Mercury alone moves it
~10 km every 88 days), which would need far shorter segments. Horizons integrates
these orbits on request, and one request spanning centuries drifts by up to
15 km, so the samples come in requests of at most 10,000 days. The fixed Kepler
ellipses these tables replace were off by tens of degrees (Ceres by 80° in 2026).

### Orbit lines

Each planet's line is its osculating ellipse, the orbit it would follow if every
other body vanished this instant, computed from the DE440 position and velocity.
The line therefore always passes exactly through the planet. From Saturn
outwards the ellipse is taken about the solar-system barycentre: the Sun
wobbles round it under Jupiter's pull, and heliocentric ellipses of the slow
outer planets would wobble with it.

### Saturn's rings

The ring texture was measured against the real ring system: the C ring's inner
edge, the Cassini division, the Encke gap and the A ring's outer edge all land
on a straight line in pixels. That puts the texture between 70,750 and
140,920 km from Saturn's centre, from inside the C ring out to the F ring, and
the rings are drawn at that size. They were 10% too small before.

They lie in Saturn's equator (its IAU pole) and cast their shadow on the globe:
for each point of Saturn, the ray towards the Sun crosses the ring plane at some
radius, and the light left is what the ring texture lets through there. The
shadow edges are sharp; the Sun's disc would blur them by about 100 km.

Tethys has no map of its own yet: it borrows Mimas's texture.

## Regenerating the data

Everything in `resources/` that comes from NASA/JPL is rebuilt by the scripts
in `tools/`, from public files:

| Script | Input | Output |
|---|---|---|
| `tools/de440_extract.py` | DE440 ASCII files from `ssd.jpl.nasa.gov/ftp/eph/planets/ascii/de440/` | `resources/de440_1900_2200.bin` |
| `tools/pck_extract.py` | `pck00011.tpc` from NAIF | `SolarSystem/IauRotation.hpp` |
| `tools/satellite_fit.py` | Horizons API | `SolarSystem/SatelliteOrbits.hpp` |
| `tools/satellite_tables.py` | Horizons API | `resources/moons/*.bin` |
| `tools/smallbody_tables.py` | Horizons API | `resources/bodies/*.bin` |

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
