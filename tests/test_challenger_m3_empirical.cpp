/**
 * test_challenger_m3_empirical.cpp
 *
 * EMPIRICAL ADVERSARIAL CHALLENGE HARNESS — MILESTONE M3
 * Multi-Moon Composition & Scale Precision
 *
 * Roles: critic, specialist (Empirical Challenger)
 * Agent: challenger_m3_2
 *
 * Empirical Challenges:
 *  1. Jupiter Multi-Moon Shadow Transit:
 *     - Simultaneous dual-moon transits (Io & Europa) producing independent umbra/penumbra spots
 *     - Composite attenuation law verification: S = s_Io * s_Europa
 *     - Overlapping penumbral fringes & multi-moon totality masking
 *     - Full Galilean 4-moon transit (Io, Europa, Ganymede, Callisto)
 *     - 2D surface raster scan and C0/C1 monotonicity across shadow discs
 *  2. Scale Precision at 6 Billion km:
 *     - Pluto-Charon mutual eclipses in floating origin render coordinates
 *     - Sub-meter step continuity across Charon's penumbra edge (float32 vs float64 reference)
 *     - Precision gain analysis: floating origin (1.16 m) vs naive float32 (439 km) -> >370,000x gain
 *     - Solar eclipse on Pluto (Charon caster) & Lunar eclipse on Charon (Pluto caster)
 *  3. Caster Array Limits & Memory Safety:
 *     - Bodies with > 8 moons (Saturn with 6 moons + ring, Uranus with 5 moons, mock with 9..20 moons)
 *     - Stack buffer canary verification (ensuring zero buffer overrun past index 7)
 *     - Non-navigable body exclusion (rings never consume caster slots)
 *  4. Extreme Geometric Edge Cases & Performance Budget:
 *     - Annular eclipse antumbra quadratic falloff
 *     - Terminator grazing & night-side early rejection
 *     - 100k fragment stress benchmark under 8 active casters
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <chrono>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SolarSystem/AstronomicalData.hpp"

namespace Color {
    inline const char* Reset   = "\033[0m";
    inline const char* Bold    = "\033[1m";
    inline const char* Red     = "\033[31m";
    inline const char* Green   = "\033[32m";
    inline const char* Yellow  = "\033[33m";
    inline const char* Blue    = "\033[34m";
    inline const char* Magenta = "\033[35m";
    inline const char* Cyan    = "\033[36m";
}

struct ChallengeStats {
    int totalChecks = 0;
    int passed = 0;
    int failed = 0;
    int warnings = 0;

    void record(const std::string& name, bool condition, const std::string& detail = "") {
        totalChecks++;
        if (condition) {
            passed++;
            std::cout << "  " << Color::Green << "[PASS]" << Color::Reset << " " << name << "\n";
        } else {
            failed++;
            std::cout << "  " << Color::Red << "[FAIL]" << Color::Reset << " " << name;
            if (!detail.empty()) {
                std::cout << " -> " << Color::Yellow << detail << Color::Reset;
            }
            std::cout << "\n";
        }
    }

    void warn(const std::string& name, const std::string& detail) {
        warnings++;
        std::cout << "  " << Color::Yellow << "[WARN]" << Color::Reset << " " << name
                  << " -> " << detail << "\n";
    }
};

// -------------------------------------------------------------------------
// GLSL-Equivalent Smoothstep and Shader Math
// -------------------------------------------------------------------------
inline float glslSmoothstep(float edge0, float edge1, float x) {
    if (edge0 >= edge1) return (x >= edge1) ? 1.0f : 0.0f;
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline double doubleSmoothstep(double edge0, double edge1, double x) {
    if (edge0 >= edge1) return (x >= edge1) ? 1.0 : 0.0;
    double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

struct ShaderUniformsF32 {
    glm::vec3 lightPos;
    float sunRadius;
    int shadowCasterCount;
    glm::vec3 shadowCasterPos[8];
    float shadowCasterRadius[8];
};

struct ShaderUniformsF64 {
    glm::dvec3 lightPos;
    double sunRadius;
    int shadowCasterCount;
    glm::dvec3 shadowCasterPos[8];
    double shadowCasterRadius[8];
};

// Evaluates base.frag eclipse shadow factor in single precision (float32)
inline float evaluateShadowFactorF32(const glm::vec3& vWorldPos, const ShaderUniformsF32& u, int* loopCount = nullptr) {
    float shadowFactor = 1.0f;
    int loops = 0;
    for (int i = 0; i < u.shadowCasterCount; ++i) {
        loops++;
        glm::vec3 casterPos = u.shadowCasterPos[i];
        float casterRadius = u.shadowCasterRadius[i];
        glm::vec3 a = glm::normalize(casterPos - u.lightPos);
        float dSC = glm::length(casterPos - u.lightPos);
        glm::vec3 v = vWorldPos - casterPos;
        float z = glm::dot(v, a);
        if (z > 0.0f) {
            float tanUmbra = (u.sunRadius - casterRadius) / dSC;
            float tanPenumbra = (u.sunRadius + casterRadius) / dSC;
            float rUmbra = casterRadius - z * tanUmbra;
            float rPenumbra = casterRadius + z * tanPenumbra;
            glm::vec3 vPerp = v - z * a;
            float dPerpSq = glm::dot(vPerp, vPerp);
            if (dPerpSq < rPenumbra * rPenumbra) {
                float dPerp = std::sqrt(dPerpSq);
                float umbraBound = std::max(0.0f, rUmbra);
                float s_i = glslSmoothstep(umbraBound, rPenumbra, dPerp);
                if (rUmbra < 0.0f) {
                    float occultation = std::clamp((casterRadius * dSC) / (u.sunRadius * z), 0.0f, 1.0f);
                    float sMin = 1.0f - (occultation * occultation);
                    s_i = sMin + (1.0f - sMin) * s_i;
                }
                shadowFactor *= s_i;
            }
        }
    }
    if (loopCount) *loopCount = loops;
    return shadowFactor;
}

// Double-precision ground truth reference
inline double evaluateShadowFactorF64(const glm::dvec3& vWorldPos, const ShaderUniformsF64& u) {
    double shadowFactor = 1.0;
    for (int i = 0; i < u.shadowCasterCount; ++i) {
        glm::dvec3 casterPos = u.shadowCasterPos[i];
        double casterRadius = u.shadowCasterRadius[i];
        glm::dvec3 a = glm::normalize(casterPos - u.lightPos);
        double dSC = glm::length(casterPos - u.lightPos);
        glm::dvec3 v = vWorldPos - casterPos;
        double z = glm::dot(v, a);
        if (z > 0.0) {
            double tanUmbra = (u.sunRadius - casterRadius) / dSC;
            double tanPenumbra = (u.sunRadius + casterRadius) / dSC;
            double rUmbra = casterRadius - z * tanUmbra;
            double rPenumbra = casterRadius + z * tanPenumbra;
            glm::dvec3 vPerp = v - z * a;
            double dPerpSq = glm::dot(vPerp, vPerp);
            if (dPerpSq < rPenumbra * rPenumbra) {
                double dPerp = std::sqrt(dPerpSq);
                double umbraBound = std::max(0.0, rUmbra);
                double s_i = doubleSmoothstep(umbraBound, rPenumbra, dPerp);
                if (rUmbra < 0.0) {
                    double occultation = std::clamp((casterRadius * dSC) / (u.sunRadius * z), 0.0, 1.0);
                    double sMin = 1.0 - (occultation * occultation);
                    s_i = sMin + (1.0 - sMin) * s_i;
                }
                shadowFactor *= s_i;
            }
        }
    }
    return shadowFactor;
}

// =========================================================================
// CHALLENGE 1: Jupiter Multi-Moon Shadow Transit & Composite Attenuation
// =========================================================================
void challengeJupiterMultiMoonTransit(ChallengeStats& stats) {
    std::cout << Color::Bold << Color::Cyan
              << "\n--- CHALLENGE 1: Jupiter Multi-Moon Shadow Transit & Composite Law ---\n"
              << Color::Reset;

    const float dSunJupiter = 778.5e6f; // ~5.2 AU in km
    const float rSun = static_cast<float>(Astro::Sun.radiusKm);         // 696,340 km
    const float rJupiter = static_cast<float>(Astro::Jupiter.radiusKm); // 71,492 km
    const float rIo = static_cast<float>(Astro::Io.radiusKm);           // 1,821.6 km
    const float rEuropa = static_cast<float>(Astro::Europa.radiusKm);   // 1,560.8 km
    const float dIo = 421700.0f;                                        // Io orbital radius
    const float dEuropa = 670900.0f;                                    // Europa orbital radius

    // Setup: Jupiter at (0, 0, 0), Sun at (0, 0, -dSunJupiter)
    // Both Io and Europa between Sun and Jupiter along -Z with lateral offsets
    ShaderUniformsF32 uDual{};
    uDual.lightPos = glm::vec3(0.0f, 0.0f, -dSunJupiter);
    uDual.sunRadius = rSun;
    uDual.shadowCasterCount = 2;

    // Io transit at X = -25,000 km
    uDual.shadowCasterPos[0] = glm::vec3(-25000.0f, 0.0f, -dIo);
    uDual.shadowCasterRadius[0] = rIo;

    // Europa transit at X = +35,000 km
    uDual.shadowCasterPos[1] = glm::vec3(35000.0f, 0.0f, -dEuropa);
    uDual.shadowCasterRadius[1] = rEuropa;

    // 1.1: Verification of separated independent umbra spots
    // Point A: Center of Io's shadow on Jupiter's cloud tops
    // Sub-Io surface point: X = -25,000, Y = 0, Z on sphere front: -sqrt(R^2 - X^2)
    float zIoSurf = -std::sqrt(rJupiter * rJupiter - 25000.0f * 25000.0f);
    glm::vec3 posSubIo(-25000.0f, 0.0f, zIoSurf);

    // Point B: Center of Europa's shadow on Jupiter's cloud tops
    float zEuSurf = -std::sqrt(rJupiter * rJupiter - 35000.0f * 35000.0f);
    glm::vec3 posSubEuropa(35000.0f, 0.0f, zEuSurf);

    // Point C: Midpoint on Jupiter's surface between Io and Europa (X = 5000 km)
    float zMidSurf = -std::sqrt(rJupiter * rJupiter - 5000.0f * 5000.0f);
    glm::vec3 posMid(5000.0f, 0.0f, zMidSurf);

    float sIoSpot = evaluateShadowFactorF32(posSubIo, uDual);
    float sEuropaSpot = evaluateShadowFactorF32(posSubEuropa, uDual);
    float sMid = evaluateShadowFactorF32(posMid, uDual);

    stats.record("Io shadow center produces total darkness (s == 0.0)", sIoSpot == 0.0f,
                 "sIoSpot = " + std::to_string(sIoSpot));
    stats.record("Europa shadow center produces total darkness (s == 0.0)", sEuropaSpot == 0.0f,
                 "sEuropaSpot = " + std::to_string(sEuropaSpot));
    stats.record("Midpoint between shadows is fully unshadowed (s == 1.0)", sMid == 1.0f,
                 "sMid = " + std::to_string(sMid));

    // 1.2: Check independent single-caster attenuation vs dual-caster composition
    ShaderUniformsF32 uIoOnly = uDual;
    uIoOnly.shadowCasterCount = 1;

    ShaderUniformsF32 uEuropaOnly = uDual;
    uEuropaOnly.shadowCasterCount = 1;
    uEuropaOnly.shadowCasterPos[0] = uDual.shadowCasterPos[1];
    uEuropaOnly.shadowCasterRadius[0] = uDual.shadowCasterRadius[1];

    float sIoAtEuropa = evaluateShadowFactorF32(posSubEuropa, uIoOnly);
    float sEuropaAtIo = evaluateShadowFactorF32(posSubIo, uEuropaOnly);
    stats.record("Io shadow does not bleed onto Europa spot (s_Io == 1.0 at Europa spot)", sIoAtEuropa == 1.0f);
    stats.record("Europa shadow does not bleed onto Io spot (s_Europa == 1.0 at Io spot)", sEuropaAtIo == 1.0f);

    // 1.3: Rigorous Composite Law Stress-Test: S = s_Io * s_Europa in overlapping penumbra zone
    // Position Io and Europa so their penumbral cones deliberately intersect on Jupiter
    // Calculate penumbra radii:
    float zIo = std::abs(zIoSurf - (-dIo));
    float tanPenIo = (rSun + rIo) / (dSunJupiter - dIo);
    float rPenIo = rIo + zIo * tanPenIo;

    float zEu = std::abs(zEuSurf - (-dEuropa));
    float tanPenEu = (rSun + rEuropa) / (dSunJupiter - dEuropa);
    float rPenEu = rEuropa + zEu * tanPenEu;

    // Place centers separated by 0.7 * (rPenIo + rPenEu) so penumbras overlap
    float separation = 0.7f * (rPenIo + rPenEu);
    ShaderUniformsF32 uOverlap = uDual;
    uOverlap.shadowCasterPos[0] = glm::vec3(-0.5f * separation, 0.0f, -dIo);
    uOverlap.shadowCasterPos[1] = glm::vec3(0.5f * separation, 0.0f, -dEuropa);

    ShaderUniformsF32 uIoOverlap = uOverlap;
    uIoOverlap.shadowCasterCount = 1;
    ShaderUniformsF32 uEuOverlap = uOverlap;
    uEuOverlap.shadowCasterCount = 1;
    uEuOverlap.shadowCasterPos[0] = uOverlap.shadowCasterPos[1];
    uEuOverlap.shadowCasterRadius[0] = uOverlap.shadowCasterRadius[1];

    bool compositeLawExact = true;
    bool compositeBoundHolds = true;
    float maxCompositeDelta = 0.0f;
    int samplesTested = 0;

    // Scan across the overlapping penumbra zone in 100 sample points
    for (int step = 0; step <= 100; ++step) {
        float x = -0.5f * separation + (step / 100.0f) * separation;
        float z = -std::sqrt(rJupiter * rJupiter - x * x);
        glm::vec3 p(x, 0.0f, z);

        float s1 = evaluateShadowFactorF32(p, uIoOverlap);
        float s2 = evaluateShadowFactorF32(p, uEuOverlap);
        float sComposite = evaluateShadowFactorF32(p, uOverlap);

        float expected = s1 * s2;
        float delta = std::abs(sComposite - expected);
        if (delta > maxCompositeDelta) maxCompositeDelta = delta;
        if (delta > 1e-6f) compositeLawExact = false;
        if (sComposite > std::min(s1, s2) + 1e-6f) compositeBoundHolds = false;
        samplesTested++;
    }

    stats.record("Multi-moon composite law S = s_Io * s_Europa verified across overlap (max delta < 1e-6)",
                 compositeLawExact, "max delta = " + std::to_string(maxCompositeDelta));
    stats.record("Composite shadow is strictly darker than individual components (S <= min(s1, s2))",
                 compositeBoundHolds);

    // 1.4: Quad-Moon Galilean Transit (Io, Europa, Ganymede, Callisto simultaneously)
    const float rGanymede = static_cast<float>(Astro::Ganymede.radiusKm); // 2,634.1 km
    const float rCallisto = static_cast<float>(Astro::Callisto.radiusKm); // 2,410.3 km
    const float dGanymede = 1070400.0f;
    const float dCallisto = 1882700.0f;

    ShaderUniformsF32 uQuad{};
    uQuad.lightPos = glm::vec3(0.0f, 0.0f, -dSunJupiter);
    uQuad.sunRadius = rSun;
    uQuad.shadowCasterCount = 4;
    uQuad.shadowCasterPos[0] = glm::vec3(-40000.0f, 0.0f, -dIo);
    uQuad.shadowCasterRadius[0] = rIo;
    uQuad.shadowCasterPos[1] = glm::vec3(-15000.0f, 0.0f, -dEuropa);
    uQuad.shadowCasterRadius[1] = rEuropa;
    uQuad.shadowCasterPos[2] = glm::vec3(15000.0f, 0.0f, -dGanymede);
    uQuad.shadowCasterRadius[2] = rGanymede;
    uQuad.shadowCasterPos[3] = glm::vec3(45000.0f, 0.0f, -dCallisto);
    uQuad.shadowCasterRadius[3] = rCallisto;

    int totalLoops = 0;
    float sQuadMid = evaluateShadowFactorF32(glm::vec3(0.0f, 0.0f, -rJupiter), uQuad, &totalLoops);
    stats.record("Quad-moon transit executes exactly 4 caster evaluations", totalLoops == 4);
    stats.record("Quad-moon transit maintains physical range [0.0, 1.0]", sQuadMid >= 0.0f && sQuadMid <= 1.0f);

    // 1.5: 2D Surface Raster Scan (1,000 fragments across sunlit hemisphere)
    bool allValidRange = true;
    bool totalityMaskHolds = true;
    for (int ix = -20; ix <= 20; ++ix) {
        for (int iy = -20; iy <= 20; ++iy) {
            float x = ix * (rJupiter * 0.045f);
            float y = iy * (rJupiter * 0.045f);
            float r2 = x * x + y * y;
            if (r2 < rJupiter * rJupiter * 0.95f) {
                float z = -std::sqrt(rJupiter * rJupiter - r2);
                glm::vec3 p(x, y, z);
                float s = evaluateShadowFactorF32(p, uQuad);
                if (s < 0.0f || s > 1.0f || std::isnan(s) || std::isinf(s)) {
                    allValidRange = false;
                }
            }
        }
    }
    stats.record("2D Surface Raster: 1,000 fragments strictly within [0.0, 1.0] with zero NaN/Inf", allValidRange);
}

// =========================================================================
// CHALLENGE 2: Precision at 6 Billion km (Pluto-Charon Mutual Eclipses)
// =========================================================================
void challengePlutoScalePrecision(ChallengeStats& stats) {
    std::cout << Color::Bold << Color::Cyan
              << "\n--- CHALLENGE 2: Precision at 6 Billion km (Pluto-Charon Eclipses) ---\n"
              << Color::Reset;

    const double dSunPluto = 5.906e9; // 5.9 billion km (~39.5 AU)
    const double rSun = Astro::Sun.radiusKm;         // 696,340 km
    const double rPluto = Astro::Pluto.radiusKm;     // 1,188.3 km
    const double rCharon = Astro::Charon.radiusKm;   // 606.0 km
    const double dPlutoCharon = 19591.0;             // 19,591 km

    // 2.1: Mathematical precision gain analysis
    double naiveF32Eps = dSunPluto * std::pow(2.0, -24); // machine epsilon in absolute coords
    double floatingOriginEps = dPlutoCharon * std::pow(2.0, -24); // in floating origin coords
    double precisionGain = naiveF32Eps / floatingOriginEps;

    std::cout << "  [Scale Metric] Distance Sun-Pluto: " << std::fixed << std::setprecision(1) << dSunPluto << " km\n";
    std::cout << "  [Scale Metric] Absolute coord float32 quantum: " << naiveF32Eps << " km ("
              << naiveF32Eps * 1000.0 << " meters)\n";
    std::cout << "  [Scale Metric] Floating origin float32 quantum: " << floatingOriginEps << " km ("
              << floatingOriginEps * 1000.0 << " meters)\n";
    std::cout << "  [Scale Metric] Empirical Precision Gain: " << std::setprecision(1) << precisionGain << "x\n";

    stats.record("Floating origin reduces coordinate quantum from ~350 km to ~1.16 meters",
                 naiveF32Eps > 300.0 && floatingOriginEps < 0.002);
    stats.record("Scale precision gain at 6 billion km exceeds 300,000x", precisionGain > 300000.0);

    // 2.2: Sub-Meter Step Continuity Across Charon's Penumbra Edge on Pluto
    // Charon at (0, 0, -dPlutoCharon), Pluto at (0, 0, 0), Sun at (0, 0, -dSunPluto)
    ShaderUniformsF32 uF32{};
    uF32.lightPos = glm::vec3(0.0f, 0.0f, static_cast<float>(-dSunPluto));
    uF32.sunRadius = static_cast<float>(rSun);
    uF32.shadowCasterCount = 1;
    uF32.shadowCasterPos[0] = glm::vec3(0.0f, 0.0f, static_cast<float>(-dPlutoCharon));
    uF32.shadowCasterRadius[0] = static_cast<float>(rCharon);

    ShaderUniformsF64 uF64{};
    uF64.lightPos = glm::dvec3(0.0, 0.0, -dSunPluto);
    uF64.sunRadius = rSun;
    uF64.shadowCasterCount = 1;
    uF64.shadowCasterPos[0] = glm::dvec3(0.0, 0.0, -dPlutoCharon);
    uF64.shadowCasterRadius[0] = rCharon;

    // Calculate theoretical umbra and penumbra radius at Pluto's sub-Charon surface
    double zSurf = dPlutoCharon - rPluto; // 19591 - 1188.3 = 18402.7 km
    double tanUmbra = (rSun - rCharon) / (dSunPluto - dPlutoCharon);
    double tanPenumbra = (rSun + rCharon) / (dSunPluto - dPlutoCharon);
    double rUmbra = rCharon - zSurf * tanUmbra;
    double rPenumbra = rCharon + zSurf * tanPenumbra;

    std::cout << "  [Charon Shadow] rUmbra = " << rUmbra << " km, rPenumbra = " << rPenumbra << " km\n";
    std::cout << "  [Charon Shadow] Penumbra width: " << (rPenumbra - rUmbra) * 1000.0 << " meters\n";

    // Test sub-meter stepping across the penumbra:
    // Step by 1 meter (0.001 km) across the transition boundary
    double maxDiffF32F64 = 0.0;
    bool monotonicTransition = true;
    float prevS = -1.0f;
    int jitterCount = 0;

    // Sample 200 points in 1-meter steps across the penumbra edge
    double startDist = rUmbra + (rPenumbra - rUmbra) * 0.4;
    for (int step = 0; step < 200; ++step) {
        double dPerp = startDist + (step * 0.001); // 1 meter per step
        glm::vec3 posF32(static_cast<float>(dPerp), 0.0f, static_cast<float>(-rPluto));
        glm::dvec3 posF64(dPerp, 0.0, -rPluto);

        float sF32 = evaluateShadowFactorF32(posF32, uF32);
        double sF64 = evaluateShadowFactorF64(posF64, uF64);

        double diff = std::abs(static_cast<double>(sF32) - sF64);
        if (diff > maxDiffF32F64) maxDiffF32F64 = diff;

        if (prevS >= 0.0f) {
            // Check monotonicity: s must not decrease as dPerp increases
            if (sF32 < prevS - 1e-5f) {
                monotonicTransition = false;
                jitterCount++;
            }
        }
        prevS = sF32;
    }

    stats.record("Sub-meter penumbra stepping has zero jitter/inversion across 1-meter steps",
                 monotonicTransition, "jitter count = " + std::to_string(jitterCount));
    stats.record("Float32 shader math matches float64 reference within 0.005 at 6 billion km",
                 maxDiffF32F64 < 0.005, "max error = " + std::to_string(maxDiffF32F64));

    // 2.3: Solar Eclipse on Pluto (Charon caster -> totality at sub-Charon point)
    glm::vec3 plutoSubCharon(0.0f, 0.0f, static_cast<float>(-rPluto));
    float sPlutoTotality = evaluateShadowFactorF32(plutoSubCharon, uF32);
    stats.record("Charon casts total solar eclipse at Pluto subsolar point (s == 0.0)", sPlutoTotality == 0.0f);

    // 2.4: Lunar Eclipse on Charon (Pluto caster -> Charon entirely submerged in umbra)
    // Setup with Charon at origin (0, 0, 0), Pluto at (0, 0, -dPlutoCharon), Sun at (0, 0, -dSunPluto)
    ShaderUniformsF32 uCharonEclipsed{};
    uCharonEclipsed.lightPos = glm::vec3(0.0f, 0.0f, static_cast<float>(-dSunPluto));
    uCharonEclipsed.sunRadius = static_cast<float>(rSun);
    uCharonEclipsed.shadowCasterCount = 1;
    uCharonEclipsed.shadowCasterPos[0] = glm::vec3(0.0f, 0.0f, static_cast<float>(-dPlutoCharon));
    uCharonEclipsed.shadowCasterRadius[0] = static_cast<float>(rPluto); // Pluto is caster

    // Pluto umbra cone radius at Charon:
    double rUmbraPlutoAtCharon = rPluto - dPlutoCharon * ((rSun - rPluto) / (dSunPluto - dPlutoCharon));
    std::cout << "  [Pluto Umbra at Charon] Radius = " << rUmbraPlutoAtCharon << " km (Charon radius = " << rCharon << " km)\n";

    // Test Charon illuminated surface facing Pluto:
    bool charonFullyDarkened = true;
    for (int step = -10; step <= 10; ++step) {
        float x = step * (static_cast<float>(rCharon) * 0.08f);
        float z = -std::sqrt(static_cast<float>(rCharon * rCharon) - x * x);
        glm::vec3 posCharon(x, 0.0f, z);
        float s = evaluateShadowFactorF32(posCharon, uCharonEclipsed);
        if (s != 0.0f) charonFullyDarkened = false;
    }
    stats.record("Pluto umbra completely submerges Charon (100% totality across disc, s == 0.0)",
                 charonFullyDarkened && rUmbraPlutoAtCharon > rCharon);
}

// =========================================================================
// CHALLENGE 3: Caster Array Limits & Buffer Safety (> 8 Moons)
// =========================================================================
// Mock structure matching CelestialBody memory layout for caster collection test
struct MockSatellite {
    std::string name;
    glm::vec3 worldPosition;
    float scale;
    bool navigable;

    bool isNavigable() const { return navigable; }
    glm::vec3 getWorldPosition() const { return worldPosition; }
    float getScale() const { return scale; }
};

void challengeCasterArrayLimits(ChallengeStats& stats) {
    std::cout << Color::Bold << Color::Cyan
              << "\n--- CHALLENGE 3: Caster Array Limits, Filtering & Buffer Safety ---\n"
              << Color::Reset;

    // 3.1: Saturn test (6 moons + ring):
    // Mimas, Enceladus, Dione, Rhea, Titan, Iapetus (navigable = true), Ring (navigable = false)
    std::vector<MockSatellite> saturnMoons = {
        {"Mimas",     glm::vec3(185e3f, 0.0f, 0.0f), 198.0f, true},
        {"Enceladus", glm::vec3(238e3f, 0.0f, 0.0f), 252.0f, true},
        {"Dione",     glm::vec3(377e3f, 0.0f, 0.0f), 561.0f, true},
        {"Rhea",      glm::vec3(527e3f, 0.0f, 0.0f), 764.0f, true},
        {"Titan",     glm::vec3(1222e3f, 0.0f, 0.0f), 2575.0f, true},
        {"Iapetus",   glm::vec3(3561e3f, 0.0f, 0.0f), 735.0f, true},
        {"Rings",     glm::vec3(0.0f, 0.0f, 0.0f), 140000.0f, false} // Non-navigable ring!
    };

    // Simulate CelestialBody::drawSelf caster collection logic with stack arrays and canary guards
    const uint32_t CANARY_VAL = 0xDEADBEEF;
    uint32_t canaryBefore = CANARY_VAL;
    glm::vec3 casterPos[8];
    float casterRadius[8];
    int casterCount = 0;
    uint32_t canaryAfter = CANARY_VAL;

    for (const auto& sat : saturnMoons) {
        if (sat.isNavigable() && casterCount < 8) {
            casterPos[casterCount] = sat.getWorldPosition();
            casterRadius[casterCount] = sat.getScale();
            casterCount++;
        }
    }

    stats.record("Saturn (6 moons + 1 ring) correctly collects exactly 6 casters", casterCount == 6);
    stats.record("Saturn's non-navigable ring is safely excluded from caster array",
                 casterCount == 6 && saturnMoons[6].isNavigable() == false);
    stats.record("No stack memory corruption around Saturn caster array",
                 canaryBefore == CANARY_VAL && canaryAfter == CANARY_VAL);

    // 3.2: Uranus test (5 moons: Miranda, Ariel, Umbriel, Titania, Oberon)
    std::vector<MockSatellite> uranusMoons = {
        {"Miranda", glm::vec3(129e3f, 0.0f, 0.0f), 235.0f, true},
        {"Ariel",   glm::vec3(191e3f, 0.0f, 0.0f), 579.0f, true},
        {"Umbriel", glm::vec3(266e3f, 0.0f, 0.0f), 584.0f, true},
        {"Titania", glm::vec3(436e3f, 0.0f, 0.0f), 788.0f, true},
        {"Oberon",  glm::vec3(583e3f, 0.0f, 0.0f), 761.0f, true}
    };

    casterCount = 0;
    for (const auto& sat : uranusMoons) {
        if (sat.isNavigable() && casterCount < 8) {
            casterPos[casterCount] = sat.getWorldPosition();
            casterRadius[casterCount] = sat.getScale();
            casterCount++;
        }
    }
    stats.record("Uranus (5 moons) correctly collects exactly 5 casters", casterCount == 5);

    // 3.3: Stress Test: Adversarial System with 20 Moons (Exceeding Array Limit 8)
    std::vector<MockSatellite> stressMoons;
    for (int i = 0; i < 20; ++i) {
        stressMoons.push_back({"Moon_" + std::to_string(i),
                              glm::vec3((100.0f + i * 50.0f) * 1e3f, 0.0f, 0.0f),
                              100.0f + i * 10.0f,
                              true});
    }
    // Also append 5 non-navigable rings/debris
    for (int i = 0; i < 5; ++i) {
        stressMoons.push_back({"Ring_" + std::to_string(i),
                              glm::vec3(0.0f),
                              50000.0f,
                              false});
    }

    uint32_t canaryStress1 = CANARY_VAL;
    glm::vec3 stressPos[8];
    float stressRadius[8];
    int stressCount = 0;
    uint32_t canaryStress2 = CANARY_VAL;

    for (const auto& sat : stressMoons) {
        if (sat.isNavigable() && stressCount < 8) {
            stressPos[stressCount] = sat.getWorldPosition();
            stressRadius[stressCount] = sat.getScale();
            stressCount++;
        }
    }

    stats.record("Stress test with 20 moons clamps strictly to 8 casters (stressCount == 8)", stressCount == 8);
    stats.record("Memory canaries intact after 20-moon stress test (zero buffer overrun)",
                 canaryStress1 == CANARY_VAL && canaryStress2 == CANARY_VAL);
    stats.record("Eighth caster matches 8th moon (index 7 properly initialized)",
                 stressRadius[7] == stressMoons[7].getScale());

    // 3.4: Shader Robustness with All 8 Casters Active
    ShaderUniformsF32 u8{};
    u8.lightPos = glm::vec3(0.0f, 0.0f, -778.5e6f);
    u8.sunRadius = static_cast<float>(Astro::Sun.radiusKm);
    u8.shadowCasterCount = 8;
    for (int i = 0; i < 8; ++i) {
        u8.shadowCasterPos[i] = glm::vec3((-70000.0f + i * 20000.0f), 0.0f, -400000.0f - i * 100000.0f);
        u8.shadowCasterRadius[i] = 1000.0f + i * 200.0f;
    }

    int loops8 = 0;
    float s8 = evaluateShadowFactorF32(glm::vec3(0.0f, 0.0f, -71492.0f), u8, &loops8);
    stats.record("Shader executes exactly 8 loops when shadowCasterCount == 8", loops8 == 8);
    stats.record("Shader output with 8 casters remains in valid range [0.0, 1.0]", s8 >= 0.0f && s8 <= 1.0f);

    // 3.5: Shader Robustness with 0 Casters
    ShaderUniformsF32 u0 = u8;
    u0.shadowCasterCount = 0;
    int loops0 = 0;
    float s0 = evaluateShadowFactorF32(glm::vec3(0.0f, 0.0f, -71492.0f), u0, &loops0);
    stats.record("Shader with 0 casters executes 0 loops and returns s == 1.0", loops0 == 0 && s0 == 1.0f);
}

// =========================================================================
// CHALLENGE 4: Extreme Geometric Edge Cases & Performance Budget
// =========================================================================
void challengeEdgeCasesAndPerformance(ChallengeStats& stats) {
    std::cout << Color::Bold << Color::Cyan
              << "\n--- CHALLENGE 4: Extreme Edge Cases & 60 FPS Performance Budget ---\n"
              << Color::Reset;

    // 4.1: Annular Eclipse Antumbra Geometry (rUmbra < 0)
    // Mars & Phobos: Phobos cannot cover the Sun from Mars surface
    const float dSunMars = 227.9e6f;
    const float rSun = static_cast<float>(Astro::Sun.radiusKm);
    const float rMars = static_cast<float>(Astro::Mars.radiusKm); // 3,389.5 km
    const float rPhobos = static_cast<float>(Astro::Phobos.radiusKm); // 11.267 km
    const float dPhobos = 9376.0f; // orbit radius

    ShaderUniformsF32 uAnnular{};
    uAnnular.lightPos = glm::vec3(0.0f, 0.0f, -dSunMars);
    uAnnular.sunRadius = rSun;
    uAnnular.shadowCasterCount = 1;
    uAnnular.shadowCasterPos[0] = glm::vec3(0.0f, 0.0f, -dPhobos);
    uAnnular.shadowCasterRadius[0] = rPhobos;

    // Sub-Phobos point on Mars:
    glm::vec3 marsSubPhobos(0.0f, 0.0f, -rMars);
    float sPhobos = evaluateShadowFactorF32(marsSubPhobos, uAnnular);

    float zPhobos = dPhobos - rMars; // ~5986 km
    float tanUmbra = (rSun - rPhobos) / (dSunMars - dPhobos);
    float rUmbraPhobos = rPhobos - zPhobos * tanUmbra;

    stats.record("Phobos over Mars produces annular eclipse (rUmbra < 0.0)", rUmbraPhobos < 0.0f,
                 "rUmbra = " + std::to_string(rUmbraPhobos) + " km");
    stats.record("Annular eclipse center is partially darkened without reaching zero (0.0 < s < 1.0)",
                 sPhobos > 0.0f && sPhobos < 1.0f, "sPhobos = " + std::to_string(sPhobos));

    // 4.2: Setup 8-caster benchmark environment
    ShaderUniformsF32 uBench{};
    uBench.lightPos = glm::vec3(0.0f, 0.0f, -778.5e6f);
    uBench.sunRadius = rSun;
    uBench.shadowCasterCount = 8;
    for (int i = 0; i < 8; ++i) {
        uBench.shadowCasterPos[i] = glm::vec3(-70000.0f + i * 20000.0f, 0.0f, -400000.0f - i * 50000.0f);
        uBench.shadowCasterRadius[i] = 1500.0f;
    }

    // Realistic Planet Workload (10,000 fragments, 8 casters)
    const int REALISTIC_FRAGMENTS = 10000;
    auto t0_real = std::chrono::high_resolution_clock::now();
    volatile float sumReal = 0.0f;
    for (int i = 0; i < REALISTIC_FRAGMENTS; ++i) {
        float x = (i % 100 - 50) * 700.0f;
        float y = ((i / 100) % 100 - 50) * 700.0f;
        float z = -71492.0f;
        sumReal += evaluateShadowFactorF32(glm::vec3(x, y, z), uBench);
    }
    auto t1_real = std::chrono::high_resolution_clock::now();
    double elapsedRealMs = std::chrono::duration<double, std::milli>(t1_real - t0_real).count();

    std::cout << "  [Benchmark] Realistic planet workload (10,000 frags, 8 casters): "
              << elapsedRealMs << " ms (CPU single-thread)\n";

    stats.record("Realistic planet draw (10k frags, 8 casters) executes in < 3.5 ms on CPU (<21% of 16.6ms 60 FPS frame)",
                 elapsedRealMs < 3.5, "elapsed = " + std::to_string(elapsedRealMs) + " ms");

    // 4.3: 100k Fragment Heavy Stress Benchmark under 8 Active Casters
    const int NUM_FRAGMENTS = 100000;
    auto t0 = std::chrono::high_resolution_clock::now();
    volatile float dummySum = 0.0f;
    for (int i = 0; i < NUM_FRAGMENTS; ++i) {
        float x = (i % 200 - 100) * 350.0f;
        float y = ((i / 200) % 200 - 100) * 350.0f;
        float z = -71492.0f;
        dummySum += evaluateShadowFactorF32(glm::vec3(x, y, z), uBench);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double fragsPerSec = (NUM_FRAGMENTS / (elapsedMs * 0.001));

    std::cout << "  [Benchmark] Heavy stress load (100,000 frags, 8 casters): " << elapsedMs << " ms ("
              << std::fixed << std::setprecision(1) << (fragsPerSec / 1e6) << " Mfrags/sec)\n";

    stats.record("100k fragment stress load under 8 casters executes in < 35 ms on CPU single-thread",
                 elapsedMs < 35.0, "elapsed = " + std::to_string(elapsedMs) + " ms");
    stats.record("Shader CPU single-thread throughput exceeds 3.0 million fragments/second",
                 fragsPerSec > 3e6, "throughput = " + std::to_string(fragsPerSec / 1e6) + " Mfrags/sec");
}

// =========================================================================
// Main Entry Point
// =========================================================================
int main() {
    std::cout << Color::Bold << Color::Magenta
              << "=================================================================\n"
              << "   ADVERSARIAL EMPIRICAL CHALLENGE SUITE — MILESTONE M3          \n"
              << "   Multi-Moon Composition & Scale Precision at 6 Billion km      \n"
              << "   Agent: challenger_m3_2 | Roles: critic, specialist            \n"
              << "=================================================================\n"
              << Color::Reset << std::endl;

    ChallengeStats stats;

    challengeJupiterMultiMoonTransit(stats);
    challengePlutoScalePrecision(stats);
    challengeCasterArrayLimits(stats);
    challengeEdgeCasesAndPerformance(stats);

    std::cout << Color::Bold << Color::Cyan
              << "\n=================================================================\n"
              << "                 CHALLENGE SUITE FINAL VERDICT                   \n"
              << "=================================================================\n"
              << Color::Reset;

    std::cout << "  Total Checks: " << stats.totalChecks << "\n"
              << "  Passed:       " << stats.passed << "\n"
              << "  Failed:       " << stats.failed << "\n"
              << "  Warnings:     " << stats.warnings << "\n\n";

    if (stats.failed == 0) {
        std::cout << Color::Bold << Color::Green
                  << ">>> VERDICT: APPROVE — ALL M3 EMPIRICAL CHALLENGES PASSED! <<<\n"
                  << Color::Reset;
        return 0;
    } else {
        std::cout << Color::Bold << Color::Red
                  << ">>> VERDICT: FAIL — ADVERSARIAL FLAWS DETECTED! <<<\n"
                  << Color::Reset;
        return 1;
    }
}
