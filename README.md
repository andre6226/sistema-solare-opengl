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

Note that the texture's longitude origin is assumed to line up with the model's;
the geometry is measured data, but the rotational alignment of the colour map
against it is not guaranteed.

If the files are missing the program still runs, falling back to a sphere and
saying so on stderr.

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
