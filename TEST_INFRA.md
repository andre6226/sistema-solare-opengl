# TEST_INFRA — End-to-End Test Infrastructure Specification

**Document Version:** 1.0.0  
**Project:** Solar System Real-Time Keplerian Dynamics, Trajectories & Eclipse Shadows  
**Adherence:** `ORIGINAL_REQUEST.md`, `PROJECT.md`, `AGENTS.md` ("Ponytail, lazy senior dev mode")

---

## 1. Test Philosophy

The testing architecture for the Solar System simulation is designed around four inviolable core principles:

1. **Opaque-Box Requirement-Driven Verification:**
   Tests evaluate the observable outputs, physical/astronomical invariants, shader math formulations, and contract interfaces specified in `ORIGINAL_REQUEST.md` and `PROJECT.md`. Tests do NOT bind to private implementation details or volatile internal helper structures. If an internal implementation changes or refactors, tests verify that the external physical behavior remains mathematically exact.

2. **Zero New External Dependencies:**
   In strict compliance with `AGENTS.md`, the test suite relies exclusively on ISO C++17 standard libraries and the existing header-only linear algebra library (`glm 1.0.3` already vendored via FetchContent). No heavy testing frameworks (e.g. GoogleTest, Catch2, Boost.Test) are introduced into `CMakeLists.txt`. A lightweight, high-performance, assert-based test harness (`tests/test_infra.hpp`) provides immediate assertion feedback, detailed error diagnostics, and formatted metric reporting with zero build overhead.

3. **Autonomous and Headless Runnable Execution:**
   All unit, edge, combinatorial, and scenario tests execute headlessly without requiring an active X11/Wayland display server, GPU driver initialization, or windowing context. This guarantees 100% determinism in CI/CD pipelines, remote servers, and containerized build environments.

4. **Progressive and Tiered Invariant Checking:**
   Tests are partitioned into four hierarchical tiers:
   - **Tier 1 (Unit & Mathematical Invariants):** Validates nominal values, Standish orbital tables, time conversion formulas, Newton-Raphson quadratic convergence, and conical ray equations.
   - **Tier 2 (Boundary, Edge & Adversarial Conditions):** Validates asymptotic limits, near-parabolic eccentricities ($e \to 0.99$), degenerate orbital inclinations ($i = 0^\circ, 90^\circ, 180^\circ$), floating origin coordinate precision across $10^{10}$ km, night-side shadow culling, and annular vs total eclipse geometries.
   - **Tier 3 (Pairwise & Combinatorial Integration):** Validates multi-feature interactions, such as orbital propagation coupled with floating origin shifts, 'O' key toggle state machines with moon hierarchy filtering, and multi-moon penumbra blending on planetary surfaces.
   - **Tier 4 (Real-World Astronomical Scenarios):** Validates historical and observed astronomical events, including the August 21, 2017 Great American Solar Eclipse, the May 16, 2022 Total Lunar Eclipse, Jupiter triple-moon shadow transits, and 100-year drift-free ephemeris propagation.

---

## 2. Feature Inventory Mapping (21 Features)

The test suite directly maps to all 21 features declared in `PROJECT.md`:

| Feature # | Feature Name | Scope / Target | Target Tier Coverage | Authoritative Specification Source |
| :--- | :--- | :--- | :--- | :--- |
| **F01** | NASA/JPL Keplerian Elements Table | Elements ($a, e, i, L, \bar{\omega}, \Omega$) for 38 bodies | Tier 1, Tier 2, Tier 4 | NASA JPL Standish (1992) / Horizons |
| **F02** | System UTC Clock Synchronization | Real-time UTC to Julian centuries $T$ | Tier 1, Tier 2, Tier 3, Tier 4 | `ORIGINAL_REQUEST.md` §R1 |
| **F03** | Newton-Raphson Kepler Solver | Double-precision solver for $M = E - e \sin E$ | Tier 1, Tier 2, Tier 3 | `PROJECT.md` §Interface Contracts |
| **F04** | 3D True Ecliptic Positioning | Basis vectors $\mathbf{P}, \mathbf{Q}$ and coordinate frame mapping | Tier 1, Tier 2, Tier 3 | Standish & Williams, Ecliptic World Frame |
| **F05** | Eccentric Distance Variation | Physical variation between perihelion $a(1-e)$ and aphelion $a(1+e)$ | Tier 1, Tier 2, Tier 4 | Kepler's First Law |
| **F06** | 3D Elliptical Orbit Geometry | High-resolution 3D ellipse generation ($\ge 180$ vertices) | Tier 1, Tier 2, Tier 3 | `ORIGINAL_REQUEST.md` §R2 |
| **F07** | Floating Origin Orbit Rendering | CPU-side origin subtraction eliminating float32 jitter | Tier 1, Tier 2, Tier 3, Tier 4 | `PROJECT.md` §Architecture |
| **F08** | Active Trajectory Filtering | Display Sun-orbiting bodies + moons of focused system | Tier 1, Tier 2, Tier 3, Tier 4 | `ORIGINAL_REQUEST.md` §R2 |
| **F09** | 'O' Key Toggle Control | Interactive toggling state machine for trajectory lines | Tier 1, Tier 2, Tier 3 | `ORIGINAL_REQUEST.md` §R2 |
| **F10** | Exact Body-Orbit Alignment | Body center on rendered orbit line (zero sagitta alignment) | Tier 1, Tier 2, Tier 3, Tier 4 | Geometry Survey §5.4 |
| **F11** | Conical Eclipse Umbra Projection | Ray-cone intersection projecting umbra disc onto daylight hemisphere | Tier 1, Tier 2, Tier 3, Tier 4 | `ORIGINAL_REQUEST.md` §R3 |
| **F12** | Conical Penumbra Soft Edge | Diverging penumbra cone with smoothstep edge falloff | Tier 1, Tier 2, Tier 3, Tier 4 | Survey Eclipse §3.2 |
| **F13** | Lunar Eclipse Surface Darkening | Darkening of moon surface entering parent planet shadow cone | Tier 1, Tier 2, Tier 3, Tier 4 | `ORIGINAL_REQUEST.md` §R3 |
| **F14** | Occluder-Relative Precision Gain | Eliminating float32 jitter via occluder-relative ray math | Tier 1, Tier 2, Tier 4 | Survey Eclipse §3.1 |
| **F15** | Multi-Moon Shadow Composition | Uniform array handling and multiplicative shadow blending ($\prod s_i$) | Tier 1, Tier 2, Tier 3, Tier 4 | Survey Eclipse §3.4 |
| **F16** | 60 FPS Night-Side Optimization | Early rejection for unlit hemisphere ($\text{diff} \le 0$) and bounding distance | Tier 1, Tier 2, Tier 3 | Survey Eclipse §4 |
| **F17** | HUD Real-Time UTC Readout | `YYYY-MM-DD HH:MM:SS UTC` calendar string formatting | Tier 1, Tier 2, Tier 3, Tier 4 | `ORIGINAL_REQUEST.md` §R4 |
| **F18** | HUD 'O' Key Control Hint | Verification of `"O  orbits"` in bottom control bar | Tier 1, Tier 2 | `ORIGINAL_REQUEST.md` §R4 |
| **F19** | AGENTS.md Header-Only Discipline | Zero new external dependencies in CMake, header-only in `SolarSystem/` | Tier 1, Tier 2 | `AGENTS.md` |
| **F20** | E2E Acceptance Test Suite | Comprehensive opaque-box test runner across Tiers 1-4 | Tier 1, Tier 2, Tier 3, Tier 4 | `PROJECT.md` §Feature Inventory |
| **F21** | Final Verification & Hardening | 100% pass rate across all tiers and adversarial edge cases | Tier 1, Tier 2, Tier 3, Tier 4 | `PROJECT.md` §Milestones |

---

## 3. Directory Layout & Test Runner Invocation

### 3.1 Directory Layout
```
/home/andrea/Documenti/Sistema_solare/sistema-solare-opengl/
├── CMakeLists.txt              # Standard application CMake configuration
├── PROJECT.md                  # Project specification and feature index
├── TEST_INFRA.md               # This document
├── TEST_READY.md               # Readiness report & verification status
├── SolarSystem/                # Header-only application source tree
└── tests/                      # Independent E2E Test Suite
    ├── CMakeLists.txt          # Dedicated CMake configuration for test runner
    ├── run_tests.sh            # One-step executable test runner shell script
    ├── test_infra.hpp          # Lightweight test framework, metrics & reference oracles
    ├── test_tier1_units.cpp    # Tier 1: Unit & mathematical invariant test suites
    ├── test_tier2_edges.cpp    # Tier 2: Boundary, edge & adversarial condition test suites
    ├── test_tier3_pairwise.cpp # Tier 3: Combinatorial & pairwise interaction test suites
    ├── test_tier4_scenarios.cpp# Tier 4: Real-world astronomical scenario test suites
    └── test_main.cpp           # Main runner aggregating all tiers and reporting metrics
```

### 3.2 Test Runner Invocation

The test suite can be built and executed through either of two standard mechanisms:

#### Method A: Direct Shell Invocation (Recommended & Fastest)
```bash
chmod +x tests/run_tests.sh
./tests/run_tests.sh
```

#### Method B: Standalone CMake Build
```bash
cmake -S tests -B build/tests
cmake --build build/tests
./build/tests/e2e_test_runner
```

#### Method C: Direct Compiler One-Liner
```bash
g++ -std=c++17 -O2 -I. -Ibuild/_deps/glm-src \
    tests/test_tier1_units.cpp \
    tests/test_tier2_edges.cpp \
    tests/test_tier3_pairwise.cpp \
    tests/test_tier4_scenarios.cpp \
    tests/test_main.cpp \
    -o build/e2e_test_runner && ./build/e2e_test_runner
```

---

## 4. Coverage Thresholds & Tier Specifications

### Tier 1: Unit & Mathematical Invariants (Threshold: $\ge 5$ tests per feature)
- Minimum assertions per feature: $\ge 5$.
- Total Tier 1 test cases: $\ge 105$ tests across all 21 features.
- Verification focuses:
  - Exact Standish parameters for all 9 major planets, 5 asteroids, 4 dwarf planets, and major moons.
  - Julian date conversion: $JD(1970-01-01) = 2440587.5$, $JD(\text{J}2000.0) = 2451545.0$, century scale factor $3155760000.0\text{ s}$.
  - Newton-Raphson convergence: $|\Delta E| < 10^{-12}$ in $\le 5$ iterations, $M=0 \implies E=0$, $M=\pi \implies E=\pi$.
  - In-plane ellipse equations: $x' = a(\cos E - e)$, $y' = a\sqrt{1-e^2}\sin E$.
  - Basis vectors $\mathbf{P}$ and $\mathbf{Q}$ orthonormality: $\|\mathbf{P}\| = 1$, $\|\mathbf{Q}\| = 1$, $\mathbf{P} \cdot \mathbf{Q} = 0$.
  - Umbra cone semi-angle $\tan \alpha_u = (R_S - R_C)/d_{SC}$ and penumbra semi-angle $\tan \alpha_p = (R_S + R_C)/d_{SC}$.
  - Coordinate system mapping: Ecliptic $+Z \to \text{World } +Y$, Ecliptic $+X \to \text{World } +X$, Ecliptic $+Y \to \text{World } -Z$.
  - HUD string formatting and calendar accuracy.
  - Verification that zero extraneous libraries exist in `CMakeLists.txt`.

### Tier 2: Boundary, Edge & Adversarial Conditions (Threshold: $\ge 5$ tests per feature)
- Minimum assertions per feature: $\ge 5$.
- Total Tier 2 test cases: $\ge 105$ tests across all 21 features.
- Verification focuses:
  - High eccentricity limits: $e = 0.95, 0.99$ (cometary/extreme orbits) without division-by-zero or oscillation.
  - Zero eccentricity limits: $e = 0.0$ (perfect circular orbits).
  - Inclination extremes: $i = 0^\circ$ (coplanar), $i = 90^\circ$ (polar), $i = 177.36^\circ$ (retrograde Venus), $i = 156.88^\circ$ (retrograde Triton).
  - Anomaly wrap-around: $M \in [-100\pi, +100\pi]$ normalized correctly to $[-\pi, \pi]$.
  - Floating origin precision: distances at $10^{10}\text{ km}$ (Eris) shifted to local target $(0,0,0)$ retain sub-millimeter precision.
  - Eclipse geometry boundaries:
    - Grazing penumbral contact ($d_\perp = r_p$).
    - Grazing umbral contact ($d_\perp = r_u$).
    - Annular eclipse condition ($r_u < 0$ when occluder cannot fully cover the Sun).
    - Fragment located between Sun and occluder ($z \le 0$, no shadow cast backwards).
    - Fragment on unlit side of planet ($\text{diff} \le 0$, early rejection).
  - Multi-caster capacity: array bounds checking for $N = 0, 1, 4, 8$ casters.
  - 'O' key toggle state idempotence under rapid repeated switching.

### Tier 3: Pairwise Combinations & Integration Tests
- Validates combinatorial interactions across orthogonal system dimensions:
  1. **Keplerian Propagation $\times$ Floating Origin Re-centering**: Body moving along 3D eccentric trajectory while target origin dynamically shifts between Sun, Earth, Moon, and Jupiter.
  2. **'O' Key State $\times$ Target Hierarchy**: Orbit line visibility toggling while switching focus between Sun (all planetary lines, no moon lines), Earth (Earth orbit + Moon line), and Jupiter (Jupiter orbit + 4 Galilean moon lines).
  3. **High-Inclination Geometry $\times$ 3D Orbit Mesh Generation**: Verification of vertex generation for Pallas ($i = 34.8^\circ$), Pluto ($i = 17.1^\circ$), and Eris ($i = 44.0^\circ$) matching true 3D spatial orientations.
  4. **Multi-Moon Array $\times$ Eclipse Soft Penumbra**: Blending simultaneous overlapping shadows (e.g. Io and Europa casting adjacent/overlapping penumbrae onto Jupiter's daylight cloud tops).
  5. **Night-Side Lambertian Culling $\times$ Eclipse Cone Intersection**: Ensuring zero shadow attenuation on dark hemispheres even when geometric shadow axis passes through the night side.
  6. **Time Acceleration Scale $\times$ UTC Calendar HUD**: Time rates ($0\times, 1\times, 10\times, 1000\times, -10\times$) advancing UTC date/time and Keplerian anomaly with zero numerical truncation or jitter.

### Tier 4: Real-World Astronomical Scenarios
- Validates canonical astronomical benchmarks:
  1. **Scenario 1: Great American Solar Eclipse (August 21, 2017 18:26 UTC)**
     - Sun, Moon, and Earth alignment verified.
     - Moon distance $d_{EM} \approx 372,000\text{ km}$, within umbra cone length $L_u \approx 377,000\text{ km}$.
     - Umbra shadow spot diameter on Earth's daylight surface $\approx 110\text{ km}$.
     - Shadow factor at path center reaches totality ($s = 0.0$).
  2. **Scenario 2: Total Lunar Eclipse / Blood Moon (May 16, 2022 04:12 UTC)**
     - Earth located directly between Sun and Moon.
     - Earth's umbra cone radius at lunar distance $\approx 4,600\text{ km}$, easily engulfing the entire Moon ($R_M = 1,737\text{ km}$).
     - Moon fragment illumination drops to ambient totality.
  3. **Scenario 3: Jovian System Multi-Moon Shadow Transit**
     - Simultaneous transit of Io ($a = 421,700\text{ km}$) and Europa ($a = 671,100\text{ km}$) across Jupiter's equatorial daylight band.
     - Multi-caster shadow array produces two distinct high-contrast umbral discs.
  4. **Scenario 4: Mercury & Pluto Physical Extremes**
     - Mercury perihelion ($0.3075\text{ AU}$) to aphelion ($0.4667\text{ AU}$) distance ratio $\approx 1.517$.
     - Pluto perihelion ($29.66\text{ AU}$) to aphelion ($49.30\text{ AU}$) distance ratio $\approx 1.662$.
     - True orbital radius matches $r = a(1 - e\cos E)$ within $10^{-6}\text{ AU}$.
  5. **Scenario 5: 100-Year Drift-Free Temporal Propagation**
     - Advancing simulation from J2000.0 to J2100.0 ($T = 1.0\text{ cy}$).
     - Earth mean longitude $L$ advances by $36,000^\circ \pm 0.1^\circ$ (100 complete revolutions).
     - Julian date calculation maintains microsecond precision without float drift.
  6. **Scenario 6: Zero Sagitta Alignment across 38 Bodies**
     - For all 38 bodies, computed 3D position at time $t$ matches vertex 0 of the generated orbit line within $10^{-9}\text{ km}$.

---

## 5. Pass/Fail Criteria & Verification Protocol

- **Build Invariant:** The test runner must compile cleanly with `-Wall -Wextra -Werror` with zero warnings or errors.
- **Exit Code Invariant:** The test runner executable must return exit code `0` on successful pass of all test suites.
- **Tolerance Limits:**
  - Double precision residuals: $|E - e\sin E - M| < 1.0 \times 10^{-12}$.
  - Distance deviations: $\Delta r / r < 1.0 \times 10^{-6}$.
  - Normalization deviations: $|\|\mathbf{P}\| - 1.0| < 1.0 \times 10^{-12}$, $|\mathbf{P} \cdot \mathbf{Q}| < 1.0 \times 10^{-12}$.
  - Shadow factor: $s \in [0.0, 1.0]$ under all lighting configurations.
- **Reporting:** Upon completion, the runner outputs a comprehensive summary table detailing pass/fail counts per tier and per feature, followed by publishing `TEST_READY.md`.
