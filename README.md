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
| 23 planetary moons | JPL satellite ephemerides (Horizons) | JPL samples the tables never saw, 1900–2200 | mean orbit + correction tables: **< 1 km** for every moon (worst: Miranda, 0.92 km), 159 MB |
| Asteroids and dwarf planets (Ceres, Pallas, Vesta, Ida, Eros, Gaspra, Haumea, Eris, Makemake) | JPL small-body ephemerides (Horizons) | 20 short JPL requests per body the tables never saw, 1900–2200 | **< 0.3 km**, median tens of metres, 2 MB |
| Comet 1P/Halley | JPL small-body ephemeris, solution #75 with non-gravitational forces (Horizons) | 2 JPL positions off the table grid (perihelion 1986, 1908) | **< 0.6 km**, 2.6 MB |
| Main asteroid belt | osculating elements of the ~58,000 numbered main-belt asteroids brighter than H 15 (JPL Small-Body Database) | — | Kepler orbits about the Sun: a picture of the belt, not an ephemeris |
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

Textures, tables and shape models are stored with Git LFS: install `git-lfs`
before cloning (or run `git lfs pull` after), or they arrive as 130-byte pointers.

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
* **Navigation:** click a body's dot or label to fly there: the camera turns to
  it, eases out, rushes through the empty middle with a widening field of view
  and eases into orbit on its lit side, 3 to 9 seconds whatever the distance.
  `UP` parent, `DOWN` first moon, `LEFT`/`RIGHT` siblings jump straight there.
* **Time:** `T` opens the date picker (1900–2200, UTC); `SHIFT`/`CTRL` speed
  time up or down, `SPACE` back to real time.
* **Orbits:** `O` toggles the orbit lines. The focused body's own orbit is hidden.
* **Command line** (run from `build/`): `--date 2026-08-12T17:46:00` (UTC),
  `--focus Moon` (camera on the Sun's side of the body), `--azimuth`/`--elevation`
  (degrees, turning the camera from there), `--distance` (km), and
  `--screenshot out.png`, which freezes time, saves the frame and exits:

  ```
  cd build && ./SolarSystem.bin --date 2025-09-07T18:11:00 --focus Moon --screenshot moon.png
  ```

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
is mapped on a log scale, as the eye sees it next to the umbra, with 1,000:1
of contrast: the sliver of a partial eclipse, lit by 0.04–3% of the Sun, reads
5–50% bright instead of black, and the umbra's edge stays a soft ramp, not a step.

A shadow's edge is as soft as physics makes it. The Sun's disc gives the penumbra:
about 3,500 km wide where Earth's shadow meets the Moon, 5 km where Charon's meets
Pluto at 34 AU, so Pluto's eclipses (the next season is around 2109) have edges that
look cut with a knife. Air softens it further: sunlight grazing a caster's limb
fades out over a band of atmosphere, 0–150 km for Earth (its mean is Danjon's
1/85), a few hundred km for the giants and Titan's haze, none for airless bodies.

Moons also shade each other. Every moon takes its planet and its sister moons as
casters, so a moon's shadow falls on its neighbour: around Jupiter's equinox in
2021 Io's shadow crossed Europa every 3.55 days, from late March to early June,
most centrally on 2021-04-25 at 11:24 UTC (`mutual_io_europa` in the visual tests).

### How surfaces take light

Bodies without an atmosphere worth the name (Mercury, the Moon and every other
moon but Titan, the asteroids, Pluto and the dwarf planets) are shaded with the
Lommel–Seeliger law of dusty, airless surfaces, and with no specular glint: a full
Moon or a full Io stays bright out to its limb instead of darkening there as a
Lambert sphere does. Venus, Earth, Mars, the giants and Titan keep Lambert, whose
darkening limb is what their photos show.

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

Three moons stretch the scheme. Hyperion tumbles chaotically and is locked in a
4:3 resonance with Titan: its mean orbit misses JPL by up to 17°, so its table
carries corrections of several hundred thousand km, in 5-day segments (2.5 MB,
0.37 km). Nereid's orbit is the most eccentric of any known moon (e 0.75, 1.4 to
9.7 million km from Neptune): the fit only converges when it starts from that
eccentricity, and its table runs in 5.6-day segments to keep up at periapsis
(0.18 km). Proteus, 210 km across and 118,000 km out, needs a segment per orbit
(9.4 MB). Hyperion and Nereid have no rotation model: Hyperion spins on its mean
13 days, Nereid on the 11.5 hours Kepler measured. Their maps, and Proteus's, are
one even colour from their albedo: no spacecraft mapped them whole.

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

### Comet Halley and the asteroid belt

Halley moves on JPL's own solution, non-gravitational forces included, sampled
every 6 hours: inside 0.6 AU of the Sun a day is too coarse to fit. Its nucleus is
a 15 x 7 x 7 km ellipsoid and as dark as coal. From 3 AU in, a coma and a tail grow
away from the Sun, about 10 million km long at 1 AU and longer as the inverse square
of the distance. The tail is straight and points straight away from the Sun: no
curved dust tail, no separate ion tail.

The main belt is ~58,000 points, one per numbered asteroid brighter than H 15 (all
the ones larger than about 3 km), each on its own Kepler ellipse solved on the GPU.
No planet perturbs them, so they drift a few degrees per century from the real
asteroids. They show only from outside the belt, fading in from 3.3 to 6.7 AU from
the Sun: from within it, 58,000 dots would fill a sky where no asteroid can be seen
with the naked eye.

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
shadow edges are sharp; the Sun's disc would blur them by about 100 km. The lit
rings also shine on the band they shadow, a few percent of sunlight, so its cloud
belts still show.

Saturn's own shadow on the rings is cast by the oblate globe, 10% flatter at the
poles: the ray to the Sun is tested against the ellipsoid, not a sphere, which
would make the shadow 6% too long. The shadow is half an ellipse whose length grows
as the Sun drops towards the ring plane: in 2017, with the Sun 27° up, it ended in
the A ring; near the 2025 equinox it ran across all of them. Rings in the shadow
keep a faint glow from Saturn's day side. From the face away from the Sun the rings only glow with
the light that diffuses through them: the opaque B ring goes nearly black, the thin
C ring and the Cassini division stay faintly bright, as in Cassini's views of the
unlit side. Since the 2025 equinox the Sun lights the south face, so from the north
the rings look dark until 2039.


## Earth's clouds, lights and air

The Blue Marble cloud map lies over a cloud-free surface, fixed to the ground. Only
open water glints, and a Rayleigh haze, thicker towards the limb, turns the oceans
blue. Each cloud casts its shadow from 8 km up: the map is read again shifted
towards the Sun, by a texel at most at noon and up to 200 km where the Sun is
setting.

On the night side the city lights of the Black Marble (2016, Suomi NPP VIIRS) fade
in through twilight, dimmed under cloud. In that map the moonlit ground is blue and
the lights are yellow, so the lights are what is redder than blue.

Earth, Venus and Mars have a lit rim of air at the limb, brightest on the day side
and reaching a little past the terminator, where the high air still sees the Sun.
It is blue on Earth, pale yellow and thick on Venus (whose map shows its cloud tops),
thin and dusty orange on Mars.

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
| `tools/asteroid_belt.py` | JPL Small-Body Database query API | `resources/asteroid_belt.bin` |
| `tools/moon_textures.py` | Blue Marble surface and clouds, Black Marble 2016 night lights (NASA Earth Observatory), LRO colour Moon (NASA SVS CGI Moon Kit), USGS global mosaics of Io, Europa, Ganymede, Callisto, Enceladus, Tethys, Phobos | `resources/{Earth,EarthClouds,EarthNight,Moon,Io,...}.jpg`, plus the plain maps of Hyperion, Proteus, Nereid and Halley |
| `tools/asteroid_texture.py` | Dawn FC global mosaics of Ceres and Vesta (`planetarymaps.usgs.gov/mosaic/`), NEAR MSI global basemap of Eros (PDS Small Bodies Node) | `resources/{Ceres,Vesta,Eros}.jpg` |

## Tests

```
bash tests/run_tests.sh
```

Includes the eclipse check against the NASA catalogue and the Earth
orientation check. With `build/SolarSystem.bin` built and `xvfb-run` or a
display available, it also renders the scenes in `tests/visual/render_refs.sh`
(solar and lunar eclipses, Saturn's rings, Io's shadow on Jupiter) and compares
them with `tests/visual/ref/` by mean pixel difference; otherwise that step
prints SKIP. After an intended visual change, look at the new images and
refresh the references with `tests/visual/render_refs.sh tests/visual/ref`.

## License

Source code under the MIT License, see [LICENSE](LICENSE). JPL/NASA ephemeris
data are public domain. Third-party files (the `glad/` loader, textures, font
and shape models in `resources/`) remain under the terms of their authors.
The maps of Ceres and Vesta are Dawn Framing Camera global mosaics by DLR
(NASA/JPL-Caltech/UCLA/MPS/DLR/IDA), distributed by the USGS Astrogeology Science
Center; Eros's is the NEAR Shoemaker MSI global basemap by M. Robinson
(NASA/JHUAPL), from the PDS Small Bodies Node. Earth's surface and clouds are
NASA's Blue Marble (Reto Stöckli, NASA Earth Observatory), its night lights the
Black Marble 2016 (NASA Earth Observatory, Suomi NPP VIIRS data), the Moon is the LRO
colour map of NASA's SVS CGI Moon Kit (Ernie Wright), and the maps of Io, Europa,
Ganymede, Callisto, Enceladus, Tethys and Phobos are Galileo, Voyager, Cassini and
Viking global mosaics from the USGS Astrogeology Science Center.
