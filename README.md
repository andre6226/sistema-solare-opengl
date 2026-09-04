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
  example among Jupiter's moons. The current target is printed to the terminal.
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
| `CelestialBody.hpp` | A single scene-graph node: transform, shading type, children |
| `SolarSystem.hpp` | Builds the hierarchy, owns all resources, handles target selection |
| `AstronomicalData.hpp` | Real measurements and their conversion to render units |
| `Camera.hpp` | Orbital camera with adaptive zoom and near plane |
| `InputHandler.hpp` | Event translation into camera and simulation commands |
| `base.vert` / `base.frag` | The single shader program used for every body |
