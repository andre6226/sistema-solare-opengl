# Solar System — Quick Guide

A real-time solar system renderer in modern OpenGL (3.3 core), built with SFML for windowing and GLM for the maths. Bodies are arranged in a scene graph, so moons inherit their planet's frame of reference, and orbital and physical data come from real measurements compressed to a viewable scale. The project was developed as part of the final examination project for the Fundamentals of Computer Graphics course in the Computer Science degree program at the University of Genoa.

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

## Shape models

Phobos and Deimos are not spheres, so they are not drawn as spheres. Their
geometry comes from the shape models measured by P. C. Thomas from Viking
imagery, archived at the PDS Small Bodies Node:

    resources/models/Phobos.tab    91 x 181 grid, 2 degree spacing
    resources/models/Deimos.tab    37 x  73 grid, 5 degree spacing
