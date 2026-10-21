# Project: Solar System Real-Time Keplerian Dynamics, Trajectories & Eclipse Shadows

## Architecture
The application is an OpenGL 3.3 Core Profile Solar System simulator written in C++17, built with SFML 3.0.2 (windowing, events, font atlas) and GLM 1.0.3 (linear algebra). It strictly adheres to a header-only architecture in `SolarSystem/` with a single translation unit (`SolarSystem/main.cc`).

### Data Flow & Systems:
1. **Simulation Clock & Time**: Tracks elapsed simulation time from real UTC epoch (`std::chrono::system_clock::now()`), calculating Julian Date $JD$ and centuries $T$ from J2000.0.
2. **Ephemerides & Mechanics**: Evaluates NASA/JPL Keplerian orbital elements at $T$, solves Kepler's equation $M = E - e \sin E$ using double-precision Newton-Raphson, and computes true 3D coordinates in the ecliptic frame.
3. **Floating Origin Engine**: Preserves `exactPosition` in `glm::dvec3`. Re-centers around `currentTarget` $(0,0,0)$ on the CPU before casting to `glm::vec3` model matrices, preventing float32 precision loss across astronomical distances ($10^{10}$ km).
4. **Trajectory Renderer**: Evaluates 3D Keplerian ellipses in double precision, subtracting target origin on the CPU to render jitter-free `GL_LINE_LOOP` paths with logarithmic depth buffering.
5. **Eclipse Shaders**: Occluder-relative conical umbra/penumbra shadow tests in `base.frag` for solar eclipses (moons shadowing parent planets) and lunar eclipses (planets shadowing moons), maintaining 60 FPS without shadow maps.
6. **HUD & Controls**: 2D text and glyph overlays displaying real-time UTC date/time and control hints (including 'O' key for orbits).

---

## Code Layout
- `SolarSystem/main.cc`: Application entry point and main simulation loop.
- `SolarSystem/AstronomicalData.hpp`: Celestial constants, physical radii, and NASA/JPL Keplerian orbital elements.
- `SolarSystem/CelestialBody.hpp`: Scene graph node, Keplerian position evaluation, and uniform binding for draw calls.
- `SolarSystem/SolarSystem.hpp`: System builder, body registry, time stepping, origin shifting, and trajectory management.
- `SolarSystem/Camera.hpp`: Floating origin orbital camera.
- `SolarSystem/InputHandler.hpp`: Keyboard and mouse event dispatching (including 'O' key toggle).
- `SolarSystem/Hud.hpp`: 2D text overlay, status display with UTC date/time, and control bar hints.
- `SolarSystem/Shader.hpp`: Shader compilation, linking, and cached uniform locations.
- `SolarSystem/base.vert`: 3D vertex shader with logarithmic depth buffer.
- `SolarSystem/base.frag`: 3D fragment shader with Blinn-Phong lighting and dynamic conical eclipse shadowing.

---

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | NASA/JPL Keplerian Elements Table | NASA/JPL Standish orbital elements ($a, e, i, L, \bar{\omega}, \Omega$) and rates for 38 bodies | M1 | ORIGINAL_REQUEST §R1 |
| 2 | System UTC Clock Synchronization | Real-time UTC initialization from `system_clock::now()` to Julian centuries from J2000.0 | M1 | ORIGINAL_REQUEST §R1 |
| 3 | Newton-Raphson Kepler Solver | Double-precision solver for $M = E - e \sin E$ with quadratic convergence ($|\Delta E| < 10^{-12}$) | M1 | ORIGINAL_REQUEST §R1 |
| 4 | 3D True Ecliptic Positioning | Derivation of 3D orbital plane coordinates and transformation to world frame | M1 | ORIGINAL_REQUEST §R1 |
| 5 | Eccentric Distance Variation | Physical variation between perihelion $a(1-e)$ and aphelion $a(1+e)$ (e.g. Mercury, Pluto) | M1 | ORIGINAL_REQUEST §Acceptance |
| 6 | 3D Elliptical Orbit Geometry | High-resolution 3D ellipse generation (180+ vertices) matching Keplerian parameters | M2 | ORIGINAL_REQUEST §R2 |
| 7 | Floating Origin Orbit Rendering | CPU-side double precision origin shifting to eliminate line jitter/clipping | M2 | ORIGINAL_REQUEST §R2 |
| 8 | Active Trajectory Filtering | Display all Sun-orbiting bodies + moons of currently focused planetary system | M2 | ORIGINAL_REQUEST §R2 |
| 9 | 'O' Key Toggle Control | Interactive toggling of trajectory line visibility via 'O' key in InputHandler | M2 | ORIGINAL_REQUEST §R2 |
| 10 | Exact Body-Orbit Alignment | Guaranteeing bodies remain exactly on their trajectory lines (zero sagitta alignment) | M2 | ORIGINAL_REQUEST §R2 |
| 11 | Conical Eclipse Umbra Projection | Ray-cone intersection in `base.frag` projecting umbra disc onto daylight surface | M3 | ORIGINAL_REQUEST §R3 |
| 12 | Conical Penumbra Soft Edge | Linear penumbra cone divergence with smoothstep edge falloff in `base.frag` | M3 | ORIGINAL_REQUEST §R3 |
| 13 | Lunar Eclipse Surface Darkening | Darkening of lunar surfaces entering parent planet shadow cone | M3 | ORIGINAL_REQUEST §R3 |
| 14 | Occluder-Relative Precision Gain | Eliminating float32 jitter via occluder-relative subtraction in shader math | M3 | Survey §3.1 |
| 15 | Multi-Moon Shadow Composition | Supporting up to 8 shadow casters per planet with uniform arrays | M3 | Survey §3.4 |
| 16 | 60 FPS Night-Side Optimization | Skipping eclipse math on unlit hemisphere (`diff <= 0`) and squared distance checks | M3 | ORIGINAL_REQUEST §Acceptance |
| 17 | HUD Real-Time UTC Readout | Formatted UTC calendar string (`YYYY-MM-DD HH:MM:SS UTC`) displayed in HUD | M4 | ORIGINAL_REQUEST §R4 |
| 18 | HUD 'O' Key Control Hint | Adding `"O  orbits"` to HUD bottom controls bar | M4 | ORIGINAL_REQUEST §R4 |
| 19 | AGENTS.md Header-Only Discipline | Zero new dependencies in `CMakeLists.txt` and clean header-only diffs | M4 | ORIGINAL_REQUEST §R4 |
| 20 | E2E Acceptance Test Suite | Comprehensive opaque-box test suite across Tiers 1-4 | E2E Track | ORIGINAL_REQUEST §Acceptance |
| 21 | Final Verification & Hardening | 100% pass of E2E tests + Tier 5 adversarial edge-case verification | M5 | Project Pattern §Final |

---

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Keplerian Ephemerides & Mechanics | R1: AstronomicalData.hpp elements, UTC Julian clock, Newton-Raphson solver, 3D positions | none | DONE |
| M2 | 3D Elliptical Orbits & Key 'O' | R2: Orbit geometry, floating origin CPU subtraction, 'O' key toggle, active filter | M1 | DONE |
| M3 | Dynamic Eclipse Shadowing | R3: base.frag conical umbra/penumbra math, lunar eclipse darkening, uniforms | M1 | DONE |
| M4 | HUD Readouts & Integration | R4: Real-time UTC HUD clock, 'O' hint, header-only cleanup, system integration | M2, M3 | DONE |
| M5 | E2E Test Pass & Hardening | Final Milestone: Pass 100% of E2E test suite + Tier 5 adversarial hardening | M4, E2E Track | DONE |
| E2E | E2E Testing Track | Independent opaque-box test suite (Tiers 1-4) published to TEST_READY.md | none | DONE |

---

## Interface Contracts

### M1 (Ephemerides) ↔ Engine (`CelestialBody.hpp` / `SolarSystem.hpp`)
```cpp
namespace Astro {
    struct KeplerianElements {
        double a0, aDot;       // AU or km, AU/cy or km/cy
        double e0, eDot;       // eccentricity, 1/cy
        double i0, iDot;       // degrees, deg/cy
        double L0, LDot;       // degrees, deg/cy
        double wBar0, wBarDot; // degrees, deg/cy
        double node0, nodeDot; // degrees, deg/cy
        bool isDistanceKm = false;
    };

    double solveKepler(double M_rad, double e);
    double unixToJulianCenturies(double unixSeconds);
}
```
- `CelestialBody::updateKeplerian(double julianCenturies, const glm::dmat4& parentMatrix)` computes `exactPosition`.

### M2 (Orbit Trajectories) ↔ `SolarSystem.hpp`
```cpp
class OrbitRenderer {
public:
    void update(const CelestialBody* focusedBody, const glm::dvec3& origin, double julianCenturies);
    void draw(const glm::mat4& view, const glm::mat4& projection) const;
    void toggle() { visible = !visible; }
    bool isVisible() const { return visible; }
};
```

### M3 (Eclipse Shaders) ↔ `Shader.hpp` / `CelestialBody.hpp`
```cpp
// base.frag uniforms:
uniform float sunRadius;
uniform int shadowCasterCount;
uniform vec3 shadowCasterPos[8];
uniform float shadowCasterRadius[8];
```
- Multiplier `float shadowFactor` computed per-fragment in `base.frag` modulating `diffuse` and `specular`.

### M4 (HUD & Integration) ↔ `Hud.hpp` / `main.cc`
```cpp
struct HudFrame {
    // ...
    std::string utcTime; // "YYYY-MM-DD HH:MM:SS UTC"
};
```
