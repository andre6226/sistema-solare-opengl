# Solar System — Quick Guide

A real-time solar system renderer in modern OpenGL (3.3 core), built with SFML
for windowing and GLM for the maths. Bodies are arranged in a scene graph, so
moons inherit their planet's frame of reference, and orbital and physical data
come from real measurements compressed to a viewable scale.

## Build

From the project root:

```
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
```

SFML and GLM are fetched automatically by CMake, so the first configure needs a
network connection.

## Run

```
./build/SolarSystem.bin
```

Textures and shaders are loaded through relative paths. The build copies both
`resources/` and `SolarSystem/` next to the binary, so the program can be
launched either from the project root or from `build/`.

## Controls

* **Mouse:** hold the **left button** to orbit the view, use the **wheel** to
  zoom in and out on the current target. Zoom is multiplicative and its lower
  bound adapts to the size of the body in view.
* **Navigation:** `UP` selects the parent (towards the Sun), `DOWN` the first
  child (towards a moon), `LEFT`/`RIGHT` cycle through the siblings — for
  example among Jupiter's moons. The on-screen panel shows where you are in the
  hierarchy and the real measurements for the body in view.
* **Time:** `SHIFT` speeds the orbits up, `CTRL` slows them down, `SPACE` resets
  to normal speed. The multiplier is clamped to the range 0.001x–1000x.

## Source layout

| File | Role |
| --- | --- |
| `main.cc` | Window setup, uniform lookup, render loop |
| `Setup.hpp` | Window and OpenGL context ownership |
| `Shader.hpp` | Shader compilation with error reporting, uniform location cache |
| `Texture.hpp` | Texture loading and OpenGL handle ownership |
| `Geometry.hpp` | Base interface for primitives owning GPU buffers |
| `Sphere.hpp` / `Quad.hpp` | UV sphere and quad meshes |
| `ShapeModel.hpp` | Loader for measured shape models (OBJ, PDS plate, PDS lat/lon grid) |
| `CelestialBody.hpp` | A single scene-graph node: transform, shading type, children |
| `SolarSystem.hpp` | Builds the hierarchy, owns all resources, handles target selection |
| `AstronomicalData.hpp` | Real measurements and their conversion to render units |
| `Camera.hpp` | Orbital camera with adaptive zoom and near plane |
| `InputHandler.hpp` | Event translation into camera and simulation commands |
| `base.vert` / `base.frag` | The single shader program used for every body |
| `TextRenderer.hpp` | Core-profile 2D text and quad overlay renderer |
| `Hud.hpp` | Overlay layout: target panel, status readout, control hints |
| `hud.vert` / `hud.frag` | Shader program for the overlay |

## Shape models

Phobos and Deimos are not spheres, so they are not drawn as spheres. Their
geometry comes from the shape models measured by P. C. Thomas from Viking
imagery, archived at the PDS Small Bodies Node:

    resources/models/Phobos.tab    91 x 181 grid, 2 degree spacing
    resources/models/Deimos.tab    37 x  73 grid, 5 degree spacing

Each row is `latitude longitude radius`, the radius being in kilometres. The
loader also reads Wavefront OBJ and the PDS vertex/facet plate format, and tells
the two `.tab` layouts apart by how many numbers the first row carries, so no
caller has to declare the format. The same archive publishes Gaspra, Ida,
Mathilde and Vesta in exactly this layout, which is the intended route for
adding asteroids.

A model is centred on its bounding box and normalised so its farthest vertex
sits at radius 1, which keeps the existing render scale and the camera's minimum
zoom correct. Winding is not trusted: facet normals are compared against their
outward direction and the whole mesh is flipped if the majority disagree.
Vertex normals are area-weighted, and the vertices straddling the texture's
longitude wrap are duplicated so the seam does not smear.

### Lining the texture up with the geometry

A shape model carries a real longitude system and its colour map carries its
own, and the two rarely agree. On a plain sphere the mismatch is invisible, since
a sphere looks identical rotated; once the geometry has features, the map has to
sit on them, or craters land on ridges.

The Thomas models index **west** longitude, while the body maps used here are
laid out east-longitude with 180 degrees at the left edge, the usual convention
for planetary texture maps. That is a half-map rotation, applied through
`ShapeModel`'s `textureLongitudeOffsetDegrees` parameter rather than hard-coded,
because every shape/texture pairing has its own answer.

The value was measured, not guessed. Fitting a triaxial ellipsoid to the shape
data and subtracting it leaves a topography map; cross-correlating that against
the texture over every longitude shift peaks at 176 degrees for Phobos, at 5.1
times the background correlation, and at 200 degrees for Deimos, at 2.5 times on
a coarser 5 degree grid. Both bracket the nominal 180. As an independent check,
the ellipsoid fit recovers semi-axes of 13.01 / 9.13 / 11.45 km for Phobos
against the published 13.5 / 9.1 / 11.1, and the deepest basin in the residual
sits at 49 W, 1 N, which is Stickney.

If the files are missing the program still runs, falling back to a sphere and
saying so on stderr.

## The sky

The star map is an all-sky panorama in **galactic** coordinates, not equatorial
ones: the Milky Way runs dead flat along its centre line, which is only true in
that frame. Draped straight onto the sky sphere it laid the galactic plane on
top of the planets' orbital plane, when the two are inclined about 60 degrees to
each other, so nothing in the sky sat where it belongs.

`Astro::galacticToWorld()` composes the IAU 1958 galactic frame (north galactic
pole and galactic centre at their J2000 equatorial positions) with the obliquity
of the ecliptic and this program's axis convention. It is built from two
measured directions rather than Euler angles, which sidesteps every sign trap,
and the published pair is re-orthonormalised because it is not exactly
perpendicular. As a check, the resulting galactic/orbital plane inclination
comes out at 60.2 degrees.

`Astro::skyTextureToGalactic()` handles the panorama's own layout, which was
measured rather than assumed. Searching for the Large Magellanic Cloud - the
brightest extended source well clear of the galactic plane - across the four
possible layouts, only one puts a source at its catalogue position, at 7 times
the local background and 10 times better than any alternative: galactic
longitude decreases to the right, and latitude runs upside down. Composed with
the sphere's own parameterisation this is a proper rotation, so it lives in the
model matrix and needs no special case in the shader.

Aiming the camera along the computed direction of the LMC does land on it.

## Texture filtering

Longitude wraps around a sphere, latitude does not. `GL_REPEAT` on the T axis
made the filter blend the top row of a map into its bottom one, smearing the
south pole across the north; it is `GL_CLAMP_TO_EDGE`.

Where a UV sphere's meridians converge, one texel footprint covers a wide,
razor-thin strip, and an isotropic filter must pick a single mip level for both
axes. That compromise is what drew the radial streaks at the poles, so
anisotropic filtering is enabled at the driver's maximum. It is an extension
under OpenGL 3.3, probed at runtime and skipped where absent.

## A note on the HUD

The overlay is drawn with its own core-profile shader, not with SFML's 2D
renderer. `sf::RenderWindow::draw()` depends on state that a core profile does
not provide, and `pushGLStates()`/`popGLStates()` does not round-trip everything
it touches — the depth test and the bound vertex array among them. Linux and
Windows drivers are lenient enough to mask the problem; macOS exposes no
compatibility profile above OpenGL 3.2, so there the depth buffer really does
come back corrupted. SFML is used only to rasterise glyphs into an atlas on the
CPU; every draw call is ours, and `TextRenderer::begin()`/`end()` saves and
restores the exact state the overlay disturbs.
