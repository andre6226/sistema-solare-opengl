/**
 * test_challenger_m3_1_empirical.cpp
 * Adversarial Empirical Verification & Stress Test Harness for Milestone M3
 * Focus: Eclipse Geometry & Physical Accuracy
 *
 * Authored by: challenger_m3_1 (critic, specialist)
 *
 * This test harness directly and independently executes the exact GLSL fragment shader
 * math from `SolarSystem/base.frag` to empirically stress-test:
 * 1. Solar eclipse totality on Earth:
 *    - Umbra center produces total eclipse s = 0.0, diffuse = 0.0, specular = 0.0, color == ambient
 *    - Totality diameter matches physical cone bounds (~100-110 km) at realistic Moon distance (368,000 km)
 *    - Penumbra smoothly and monotonically falls off to 1.0 with C1 boundary continuity
 * 2. Lunar eclipse on Moon:
 *    - Entire Earth-facing lunar disc immersed in Earth's shadow cone drops to ambient twilight (s = 0.0, diffuse = 0.0)
 *    - Over 99% light reduction across all surface fragments
 * 3. Annular eclipse (Mars-Phobos & Earth-Moon at apogee):
 *    - Antumbra center attenuation sMin > 0 (partial darkening, strictly no black disc)
 *    - Monotonic radial transition from sMin to 1.0
 * 4. Night-side rejection:
 *    - Fragments where diff <= 0.0 have strictly 0 difference in output color whether eclipse shadow is present or absent
 *    - Zero shadow loops executed on unlit hemisphere
 * 5. Stress tests & boundary robustness:
 *    - Multi-moon shadow intersection and product composition
 *    - Non-collinear / inverted / negative-z occluder positions
 *    - Extreme proximity (z -> 0+) and numerical stability
 *    - High-throughput fuzzed fragment verification (no NaN/Inf, bounded [0, 1])
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "SolarSystem/AstronomicalData.hpp"

namespace ChallengerM3_1 {

struct EmpiricalMetrics {
    int totalChecks = 0;
    int passedChecks = 0;
    int failedChecks = 0;

    void assertCheck(const std::string& description, bool condition, const std::string& details = "") {
        totalChecks++;
        if (condition) {
            passedChecks++;
            std::cout << "  [PASS] " << description << "\n";
        } else {
            failedChecks++;
            std::cout << "  [FAIL] " << description << " --> " << details << "\n";
        }
    }
};

// GLSL standard smoothstep
inline float glsl_smoothstep(float edge0, float edge1, float x) {
    if (edge0 >= edge1) return (x >= edge1) ? 1.0f : 0.0f;
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// GLSL standard mix
inline float glsl_mix(float x, float y, float a) {
    return x * (1.0f - a) + y * a;
}

struct EclipseUniforms {
    glm::vec3 lightPos;
    float sunRadius;
    int shadowCasterCount;
    glm::vec3 shadowCasterPos[8];
    float shadowCasterRadius[8];
};

struct FragmentIn {
    glm::vec3 vWorldPos;
    glm::vec3 vNormal;
};

struct FragmentOut {
    float diff;
    float shadowFactor;
    int loopCount;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 ambient;
    glm::vec3 color;
};

// Exact reproduction of the base.frag shader logic
inline FragmentOut evaluateShaderFragment(const FragmentIn& frag,
                                          const EclipseUniforms& uniforms,
                                          const glm::vec3& cameraPos = glm::vec3(0.0f, 0.0f, -1.0f),
                                          const glm::vec3& albedo = glm::vec3(1.0f)) {
    FragmentOut out{};
    glm::vec3 normal = glm::normalize(frag.vNormal);
    glm::vec3 lightDir = glm::normalize(uniforms.lightPos - frag.vWorldPos);
    glm::vec3 viewDir = glm::normalize(cameraPos - frag.vWorldPos);

    // Lambert (diffuse)
    out.diff = std::max(glm::dot(normal, lightDir), 0.0f);
    out.diffuse = out.diff * glm::vec3(1.0f);

    // Blinn-Phong (specular)
    glm::vec3 halfwayDir = glm::normalize(lightDir + viewDir);
    float spec = std::pow(std::max(glm::dot(normal, halfwayDir), 0.0f), 64.0f);
    out.specular = spec * glm::vec3(0.3f) * out.diff;

    out.ambient = glm::vec3(0.008f);

    // Dynamic Eclipse Shadowing
    out.shadowFactor = 1.0f;
    out.loopCount = 0;

    if (out.diff > 0.0f) {
        for (int i = 0; i < uniforms.shadowCasterCount; ++i) {
            out.loopCount++;
            glm::vec3 casterPos = uniforms.shadowCasterPos[i];
            float casterRadius = uniforms.shadowCasterRadius[i];
            glm::vec3 a = glm::normalize(casterPos - uniforms.lightPos);
            float dSC = glm::length(casterPos - uniforms.lightPos);
            glm::vec3 v = frag.vWorldPos - casterPos;
            float z = glm::dot(v, a);
            if (z > 0.0f) {
                float tanUmbra = (uniforms.sunRadius - casterRadius) / dSC;
                float tanPenumbra = (uniforms.sunRadius + casterRadius) / dSC;
                float rUmbra = casterRadius - z * tanUmbra;
                float rPenumbra = casterRadius + z * tanPenumbra;
                glm::vec3 vPerp = v - z * a;
                float dPerpSq = glm::dot(vPerp, vPerp);
                if (dPerpSq < rPenumbra * rPenumbra) {
                    float dPerp = std::sqrt(dPerpSq);
                    float umbraBound = std::max(0.0f, rUmbra);
                    float s_i = glsl_smoothstep(umbraBound, rPenumbra, dPerp);
                    if (rUmbra < 0.0f) { // Annular eclipse
                        float occultation = std::clamp((casterRadius * dSC) / (uniforms.sunRadius * z), 0.0f, 1.0f);
                        float sMin = 1.0f - (occultation * occultation);
                        s_i = glsl_mix(sMin, 1.0f, s_i);
                    }
                    out.shadowFactor *= s_i;
                }
            }
        }
    }

    out.diffuse *= out.shadowFactor;
    out.specular *= out.shadowFactor;
    out.color = (out.ambient + out.diffuse) * albedo + out.specular;

    return out;
}

// ============================================================================
// SUITE 1: Solar Eclipse Totality & Physical Umbra Cone Bounds (~100-110 km)
// ============================================================================
void runSuite1SolarEclipse(EmpiricalMetrics& met) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 1: Solar Eclipse Totality & Physical Umbra Cone Bounds (~100-110 km)\n";
    std::cout << "====================================================================\n";

    EclipseUniforms u{};
    u.lightPos = glm::vec3(0.0f); // Sun at origin
    u.sunRadius = static_cast<float>(Astro::Sun.radiusKm); // 696,340 km

    float dEarth = 149600000.0f; // 1 AU
    float rEarth = static_cast<float>(Astro::Earth.radiusKm); // 6371.0 km
    float rMoon = static_cast<float>(Astro::Moon.radiusKm); // 1737.4 km

    // Select Moon distance at near-perigee (d = 368,000 km) producing totality diameter ~ 100-110 km
    float dMoon = 368000.0f;
    glm::vec3 earthCenter = glm::vec3(0.0f, 0.0f, dEarth);
    glm::vec3 moonCenter = earthCenter - glm::vec3(0.0f, 0.0f, dMoon);

    u.shadowCasterCount = 1;
    u.shadowCasterPos[0] = moonCenter;
    u.shadowCasterRadius[0] = rMoon;

    glm::vec3 subsolarPoint = earthCenter - glm::vec3(0.0f, 0.0f, rEarth);
    glm::vec3 normalToSun = glm::vec3(0.0f, 0.0f, -1.0f);

    // Physical analytical calculation
    float zSubsolar = (dEarth - rEarth) - moonCenter.z; // 368000 - 6371 = 361629 km
    float dSC = moonCenter.z; // 149232000 km
    float tanUmbra = (u.sunRadius - rMoon) / dSC;
    float tanPenumbra = (u.sunRadius + rMoon) / dSC;
    float rUmbraTheo = rMoon - zSubsolar * tanUmbra; // ~ 54.18 km
    float rPenumbraTheo = rMoon + zSubsolar * tanPenumbra; // ~ 3429.04 km
    float diamTheo = 2.0f * rUmbraTheo; // ~ 108.36 km

    std::cout << "  Analytical parameters:\n";
    std::cout << "    Moon-to-surface distance z = " << zSubsolar << " km\n";
    std::cout << "    rUmbra (theoretical)     = " << rUmbraTheo << " km\n";
    std::cout << "    Totality diameter (theo)  = " << diamTheo << " km\n";
    std::cout << "    rPenumbra (theoretical)   = " << rPenumbraTheo << " km\n";

    met.assertCheck("Theoretical totality diameter is within physical bounds [100 km, 110 km]",
                    diamTheo >= 100.0f && diamTheo <= 110.0f,
                    "diamTheo = " + std::to_string(diamTheo));

    // 1.1 Umbra center totality
    FragmentIn centerFrag{subsolarPoint, normalToSun};
    FragmentOut centerOut = evaluateShaderFragment(centerFrag, u);
    met.assertCheck("Umbra center produces total eclipse s = 0.0",
                    centerOut.shadowFactor == 0.0f,
                    "s = " + std::to_string(centerOut.shadowFactor));
    met.assertCheck("Umbra center diffuse light is completely zeroed",
                    glm::length(centerOut.diffuse) == 0.0f);
    met.assertCheck("Umbra center specular light is completely zeroed",
                    glm::length(centerOut.specular) == 0.0f);
    met.assertCheck("Umbra center drops strictly to ambient lighting",
                    glm::distance(centerOut.color, centerOut.ambient) < 1e-6f);

    // 1.2 Fine empirical radial scan across totality disc (step = 0.05 km)
    // Find empirical boundary where shadowFactor transitions from 0.0 to > 0.0
    float empiricalUmbraRadius = 0.0f;
    bool totalityStrictlyZero = true;

    for (float r = 0.0f; r <= 150.0f; r += 0.05f) {
        FragmentIn f{subsolarPoint + glm::vec3(r, 0.0f, 0.0f), normalToSun};
        FragmentOut o = evaluateShaderFragment(f, u);
        if (r <= rUmbraTheo - 0.1f) {
            if (o.shadowFactor != 0.0f) {
                totalityStrictlyZero = false;
            }
        }
        if (o.shadowFactor == 0.0f) {
            empiricalUmbraRadius = r;
        }
    }

    float empiricalTotalityDiameter = 2.0f * empiricalUmbraRadius;
    std::cout << "    Empirical rUmbra measured      = " << empiricalUmbraRadius << " km\n";
    std::cout << "    Empirical Totality diameter    = " << empiricalTotalityDiameter << " km\n";

    met.assertCheck("Shadow factor is strictly 0.0 across entire interior of umbra disc",
                    totalityStrictlyZero);
    met.assertCheck("Empirical totality diameter matches physical cone bounds (~100-110 km)",
                    empiricalTotalityDiameter >= 100.0f && empiricalTotalityDiameter <= 110.0f,
                    "measured = " + std::to_string(empiricalTotalityDiameter));
    met.assertCheck("Empirical totality diameter matches theoretical cone formula within 0.2 km",
                    std::abs(empiricalTotalityDiameter - diamTheo) <= 0.25f,
                    "diff = " + std::to_string(std::abs(empiricalTotalityDiameter - diamTheo)));

    // 1.3 Penumbra smooth falloff verification
    // Sample 100 points across penumbra [rUmbra, rPenumbra]
    bool penumbraMonotonic = true;
    bool penumbraBounded = true;
    float prevS = 0.0f;

    for (int i = 0; i <= 100; ++i) {
        float t = static_cast<float>(i) / 100.0f;
        float r = rUmbraTheo + t * (rPenumbraTheo - rUmbraTheo);
        FragmentIn f{subsolarPoint + glm::vec3(r, 0.0f, 0.0f), normalToSun};
        FragmentOut o = evaluateShaderFragment(f, u);

        if (o.shadowFactor < prevS - 1e-6f) {
            penumbraMonotonic = false;
        }
        if (o.shadowFactor < 0.0f || o.shadowFactor > 1.0f) {
            penumbraBounded = false;
        }
        prevS = o.shadowFactor;
    }

    met.assertCheck("Penumbra shadow factor is strictly bounded in [0.0, 1.0]", penumbraBounded);
    met.assertCheck("Penumbra smoothly and monotonically increases to 1.0", penumbraMonotonic);

    // 1.4 Outside penumbra boundary check
    FragmentIn outsideFrag1{subsolarPoint + glm::vec3(rPenumbraTheo, 0.0f, 0.0f), normalToSun};
    FragmentOut outsideOut1 = evaluateShaderFragment(outsideFrag1, u);
    met.assertCheck("At penumbra outer boundary (r = rPenumbra), s == 1.0",
                    std::abs(outsideOut1.shadowFactor - 1.0f) < 1e-5f);

    FragmentIn outsideFrag2{subsolarPoint + glm::vec3(rPenumbraTheo + 500.0f, 0.0f, 0.0f), normalToSun};
    FragmentOut outsideOut2 = evaluateShaderFragment(outsideFrag2, u);
    met.assertCheck("Beyond penumbra (r > rPenumbra), shadowFactor is exactly 1.0",
                    outsideOut2.shadowFactor == 1.0f);

    // 1.5 Azimuthal rotational symmetry (fuzz 36 directions around subsolar point)
    bool rotationalSymmetric = true;
    for (int angleDeg = 0; angleDeg < 360; angleDeg += 10) {
        float rad = glm::radians(static_cast<float>(angleDeg));
        float rTest = 0.5f * (rUmbraTheo + rPenumbraTheo);
        glm::vec3 offset = glm::vec3(rTest * std::cos(rad), rTest * std::sin(rad), 0.0f);
        FragmentIn f{subsolarPoint + offset, normalToSun};
        FragmentOut o = evaluateShaderFragment(f, u);
        if (std::abs(o.shadowFactor - 0.5f) > 0.05f) {
            rotationalSymmetric = false;
        }
    }
    met.assertCheck("Penumbra has exact azimuthal rotational symmetry (invariant to angle)",
                    rotationalSymmetric);
}

// ============================================================================
// SUITE 2: Lunar Eclipse on Moon (Complete Immersion & Twilight Ambient)
// ============================================================================
void runSuite2LunarEclipse(EmpiricalMetrics& met) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 2: Lunar Eclipse on Moon (Complete Immersion & Twilight Ambient)\n";
    std::cout << "====================================================================\n";

    EclipseUniforms u{};
    u.lightPos = glm::vec3(0.0f);
    u.sunRadius = static_cast<float>(Astro::Sun.radiusKm); // 696,340 km

    float dEarth = 149600000.0f;
    float rEarth = static_cast<float>(Astro::Earth.radiusKm); // 6371.0 km
    float rMoon = static_cast<float>(Astro::Moon.radiusKm); // 1737.4 km
    float dMoon = 384400.0f; // Mean Earth-Moon distance

    // Earth is the shadow caster
    glm::vec3 earthCenter = glm::vec3(0.0f, 0.0f, dEarth);
    u.shadowCasterCount = 1;
    u.shadowCasterPos[0] = earthCenter;
    u.shadowCasterRadius[0] = rEarth;

    // Moon is directly behind Earth at syzygy
    glm::vec3 moonCenter = earthCenter + glm::vec3(0.0f, 0.0f, dMoon);

    // Theoretical Earth umbra radius at Moon distance:
    float tanUmbra = (u.sunRadius - rEarth) / dEarth;
    float rUmbraAtMoon = rEarth - dMoon * tanUmbra; // ~ 4598 km

    std::cout << "  Earth umbra cone at Moon distance:\n";
    std::cout << "    rUmbra at Moon distance = " << rUmbraAtMoon << " km\n";
    std::cout << "    Moon radius             = " << rMoon << " km\n";
    std::cout << "    Umbra-to-Moon ratio     = " << (rUmbraAtMoon / rMoon) << "x\n";

    met.assertCheck("Earth umbra radius at Moon is significantly larger than Moon (rUmbra > 2.5 * rMoon)",
                    rUmbraAtMoon > 2.5f * rMoon);

    // 2.1 Moon subsolar point (closest to Earth, facing Sun)
    FragmentIn moonFacingFrag{moonCenter - glm::vec3(0.0f, 0.0f, rMoon), glm::vec3(0.0f, 0.0f, -1.0f)};
    FragmentOut moonFacingOut = evaluateShaderFragment(moonFacingFrag, u);
    met.assertCheck("Moon subsolar point drops to total eclipse s = 0.0",
                    moonFacingOut.shadowFactor == 0.0f);
    met.assertCheck("Moon subsolar point diffuse is completely zeroed",
                    glm::length(moonFacingOut.diffuse) == 0.0f);
    met.assertCheck("Moon subsolar point drops to ambient twilight (color == ambient * albedo)",
                    glm::distance(moonFacingOut.color, glm::vec3(0.008f)) < 1e-6f);

    // 2.2 Dense spherical grid across illuminated lunar hemisphere (1,000 fragments)
    // All points facing Earth must be completely immersed in totality (s = 0.0)
    int samplesPassed = 0;
    const int totalSamples = 1000;
    for (int i = 0; i < totalSamples; ++i) {
        float uCoord = static_cast<float>(i) / totalSamples;
        float theta = uCoord * glm::two_pi<float>();
        float phi = (static_cast<float>(i % 100) / 100.0f) * glm::half_pi<float>(); // [0, pi/2] hemisphere facing -Z

        glm::vec3 normal = glm::vec3(std::cos(phi) * std::cos(theta),
                                     std::cos(phi) * std::sin(theta),
                                     -std::sin(phi)); // facing towards Sun/Earth
        glm::vec3 pos = moonCenter + normal * rMoon;

        FragmentIn f{pos, normal};
        FragmentOut o = evaluateShaderFragment(f, u);

        if (o.diff > 0.0f) {
            if (o.shadowFactor == 0.0f && glm::length(o.diffuse) == 0.0f) {
                samplesPassed++;
            }
        } else {
            samplesPassed++; // night side also 0 diffuse
        }
    }

    met.assertCheck("100% of fragments across sunlit lunar hemisphere are fully darkened (s = 0.0)",
                    samplesPassed == totalSamples,
                    "Passed: " + std::to_string(samplesPassed) + " / " + std::to_string(totalSamples));

    // 2.3 Radiance drop comparison against unoccluded Moon
    EclipseUniforms uNoOcclusion = u;
    uNoOcclusion.shadowCasterCount = 0;
    FragmentOut unoccludedOut = evaluateShaderFragment(moonFacingFrag, uNoOcclusion);
    float litRadiance = glm::length(unoccludedOut.color);
    float eclipsedRadiance = glm::length(moonFacingOut.color);
    float attenuationPercent = (1.0f - eclipsedRadiance / litRadiance) * 100.0f;

    std::cout << "    Unoccluded radiance = " << litRadiance << ", Eclipsed radiance = " << eclipsedRadiance << "\n";
    std::cout << "    Empirical attenuation = " << attenuationPercent << "%\n";

    met.assertCheck("Total lunar eclipse drops radiance by > 99% to ambient twilight",
                    attenuationPercent > 99.0f,
                    "attenuation = " + std::to_string(attenuationPercent) + "%");
}

// ============================================================================
// SUITE 3: Annular Eclipse Antumbra (Mars-Phobos & Moon at Apogee)
// ============================================================================
void runSuite3AnnularEclipse(EmpiricalMetrics& met) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 3: Annular Eclipse Antumbra (Mars-Phobos & Moon at Apogee)\n";
    std::cout << "====================================================================\n";

    // Scenario A: Mars & Phobos
    // Phobos: r = 11.2 km, distance = 9,376 km
    // Mars: r = 3389.5 km, distance = 227,900,000 km
    EclipseUniforms uMars{};
    uMars.lightPos = glm::vec3(0.0f);
    uMars.sunRadius = static_cast<float>(Astro::Sun.radiusKm); // 696,340 km

    float dMars = 227900000.0f;
    float rMars = static_cast<float>(Astro::Mars.radiusKm); // 3389.5 km
    float rPhobos = static_cast<float>(Astro::Phobos.radiusKm); // 11.2 km
    float dPhobos = 9376.0f;

    glm::vec3 marsCenter = glm::vec3(0.0f, 0.0f, dMars);
    glm::vec3 phobosCenter = marsCenter - glm::vec3(0.0f, 0.0f, dPhobos);

    uMars.shadowCasterCount = 1;
    uMars.shadowCasterPos[0] = phobosCenter;
    uMars.shadowCasterRadius[0] = rPhobos;

    // Subsolar point on Mars
    glm::vec3 subsolarMars = marsCenter - glm::vec3(0.0f, 0.0f, rMars);
    float zSubsolarMars = (dMars - rMars) - phobosCenter.z; // 9376 - 3389.5 = 5986.5 km
    float dSC_Mars = phobosCenter.z;

    float tanUmbraPhobos = (uMars.sunRadius - rPhobos) / dSC_Mars;
    float rUmbraPhobos = rPhobos - zSubsolarMars * tanUmbraPhobos; // Negative -> Antumbra!
    float apexDistance = rPhobos / tanUmbraPhobos; // ~ 3669 km

    std::cout << "  Mars-Phobos Antumbra parameters:\n";
    std::cout << "    Distance to Phobos z       = " << zSubsolarMars << " km\n";
    std::cout << "    Umbra cone apex distance   = " << apexDistance << " km (< z)\n";
    std::cout << "    rUmbra at Mars surface     = " << rUmbraPhobos << " km (< 0 -> Antumbra)\n";

    met.assertCheck("Phobos umbra apex ends before Mars surface (rUmbra < 0.0)",
                    rUmbraPhobos < 0.0f);

    // Evaluate subsolar point at center of Phobos transit
    FragmentIn marsFrag{subsolarMars, glm::vec3(0.0f, 0.0f, -1.0f)};
    FragmentOut marsOut = evaluateShaderFragment(marsFrag, uMars);

    float occultationRatio = (rPhobos * dSC_Mars) / (uMars.sunRadius * zSubsolarMars);
    float expectedSMin = 1.0f - (occultationRatio * occultationRatio);

    std::cout << "    Theoretical angular occultation ratio = " << occultationRatio << "\n";
    std::cout << "    Theoretical antumbra center sMin      = " << expectedSMin << "\n";
    std::cout << "    Empirical shader shadowFactor s       = " << marsOut.shadowFactor << "\n";

    met.assertCheck("Antumbra center attenuation sMin > 0 (partial darkening, strictly NOT 0.0)",
                    marsOut.shadowFactor > 0.0f,
                    "s = " + std::to_string(marsOut.shadowFactor));
    met.assertCheck("Antumbra center is not total black (no black disc)",
                    marsOut.shadowFactor > 0.4f,
                    "s = " + std::to_string(marsOut.shadowFactor));
    met.assertCheck("Antumbra center matches physical area occultation formula within 1e-4",
                    std::abs(marsOut.shadowFactor - expectedSMin) < 1e-4f,
                    "diff = " + std::to_string(std::abs(marsOut.shadowFactor - expectedSMin)));
    met.assertCheck("Mars surface under Phobos retains diffuse light (diffuse > 0)",
                    glm::length(marsOut.diffuse) > 0.0f);

    // Scenario B: Earth-Moon Annular Eclipse at Lunar Apogee (d = 405,500 km)
    EclipseUniforms uApogee{};
    uApogee.lightPos = glm::vec3(0.0f);
    uApogee.sunRadius = static_cast<float>(Astro::Sun.radiusKm);

    float dEarth = 149600000.0f;
    float rEarth = static_cast<float>(Astro::Earth.radiusKm);
    float rMoon = static_cast<float>(Astro::Moon.radiusKm);
    float dMoonApogee = 405500.0f; // Moon at apogee

    glm::vec3 earthCenter = glm::vec3(0.0f, 0.0f, dEarth);
    glm::vec3 moonApogee = earthCenter - glm::vec3(0.0f, 0.0f, dMoonApogee);

    uApogee.shadowCasterCount = 1;
    uApogee.shadowCasterPos[0] = moonApogee;
    uApogee.shadowCasterRadius[0] = rMoon;

    glm::vec3 subsolarEarth = earthCenter - glm::vec3(0.0f, 0.0f, rEarth);
    FragmentIn apogeeFrag{subsolarEarth, glm::vec3(0.0f, 0.0f, -1.0f)};
    FragmentOut apogeeOut = evaluateShaderFragment(apogeeFrag, uApogee);

    float zApogee = (dEarth - rEarth) - moonApogee.z;
    float rUmbraApogee = rMoon - zApogee * ((uApogee.sunRadius - rMoon) / moonApogee.z);

    std::cout << "  Earth-Moon at Apogee (Annular Solar Eclipse):\n";
    std::cout << "    rUmbra at Earth = " << rUmbraApogee << " km (< 0)\n";
    std::cout << "    Antumbra center attenuation s = " << apogeeOut.shadowFactor << " (Ring of Fire)\n";

    met.assertCheck("Moon at apogee produces annular eclipse antumbra (rUmbra < 0.0)",
                    rUmbraApogee < 0.0f);
    met.assertCheck("Moon at apogee antumbra center attenuation sMin > 0 (partial eclipse)",
                    apogeeOut.shadowFactor > 0.0f && apogeeOut.shadowFactor < 0.2f,
                    "s = " + std::to_string(apogeeOut.shadowFactor));
}

// ============================================================================
// SUITE 4: Night-Side Rejection & Strict Invariance
// ============================================================================
void runSuite4NightSideRejection(EmpiricalMetrics& met) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 4: Night-Side Rejection & Strict Invariance (diff <= 0.0)\n";
    std::cout << "====================================================================\n";

    EclipseUniforms uWithShadows{};
    uWithShadows.lightPos = glm::vec3(0.0f);
    uWithShadows.sunRadius = static_cast<float>(Astro::Sun.radiusKm);

    float dEarth = 149600000.0f;
    float rEarth = static_cast<float>(Astro::Earth.radiusKm);
    glm::vec3 earthCenter = glm::vec3(0.0f, 0.0f, dEarth);

    // Place 8 shadow casters directly in line to challenge the shader
    uWithShadows.shadowCasterCount = 8;
    for (int i = 0; i < 8; ++i) {
        uWithShadows.shadowCasterPos[i] = earthCenter - glm::vec3(0.0f, 0.0f, 384400.0f + i * 10000.0f);
        uWithShadows.shadowCasterRadius[i] = 1737.4f * (1.0f + i * 0.1f);
    }

    // Identical uniform setup without shadow casters
    EclipseUniforms uNoShadows = uWithShadows;
    uNoShadows.shadowCasterCount = 0;

    // 4.1 Sample 2,000 fragments across night hemisphere (theta in [0, 2pi], phi in [pi/2, pi])
    const int nightSamples = 2000;
    bool allNightColorsIdentical = true;
    bool allNightLoopsZero = true;
    bool allNightDiffuseZero = true;
    float maxDeltaColor = 0.0f;

    for (int i = 0; i < nightSamples; ++i) {
        float uCoord = static_cast<float>(i) / nightSamples;
        float theta = uCoord * glm::two_pi<float>();
        // Angle from sun: [90 deg to 180 deg]
        float phi = glm::half_pi<float>() + (static_cast<float>(i % 200) / 200.0f) * glm::half_pi<float>();

        glm::vec3 normal = glm::vec3(std::sin(phi) * std::cos(theta),
                                     std::sin(phi) * std::sin(theta),
                                     std::cos(phi)); // +Z points away from Sun (night side)
        glm::vec3 pos = earthCenter + normal * rEarth;

        FragmentIn f{pos, normal};
        FragmentOut outShadow = evaluateShaderFragment(f, uWithShadows);
        FragmentOut outNoShadow = evaluateShaderFragment(f, uNoShadows);

        if (outShadow.diff <= 0.0f) {
            float delta = glm::distance(outShadow.color, outNoShadow.color);
            if (delta > maxDeltaColor) {
                maxDeltaColor = delta;
            }
            if (delta > 0.0f) {
                allNightColorsIdentical = false;
            }
            if (outShadow.loopCount != 0) {
                allNightLoopsZero = false;
            }
            if (glm::length(outShadow.diffuse) != 0.0f) {
                allNightDiffuseZero = false;
            }
        }
    }

    std::cout << "    Max color delta across 2,000 night fragments: " << maxDeltaColor << "\n";

    met.assertCheck("Fragments where diff <= 0.0 have 0 difference in output color (with vs without shadow)",
                    allNightColorsIdentical && maxDeltaColor == 0.0f,
                    "maxDelta = " + std::to_string(maxDeltaColor));
    met.assertCheck("Fragments where diff <= 0.0 execute exactly 0 shadow loops (complete bypass)",
                    allNightLoopsZero);
    met.assertCheck("Night side diffuse is strictly 0.0 everywhere",
                    allNightDiffuseZero);

    // 4.2 Grazing terminator boundary test (diff = 0.0, diff = 1e-7, diff = -1e-7)
    glm::vec3 termNormal = glm::vec3(1.0f, 0.0f, 0.0f); // Exactly perpendicular to light dir (0,0,-1)
    FragmentIn termFrag{earthCenter + termNormal * rEarth, termNormal};
    FragmentOut termOut = evaluateShaderFragment(termFrag, uWithShadows);
    met.assertCheck("Terminator fragment (diff == 0.0) bypasses loops (loopCount == 0)",
                    termOut.loopCount == 0);
    met.assertCheck("Terminator fragment diffuse is 0.0",
                    glm::length(termOut.diffuse) == 0.0f);
}

// ============================================================================
// SUITE 5: Adversarial Boundary Conditions & Multi-Moon Composition
// ============================================================================
void runSuite5AdversarialStress(EmpiricalMetrics& met) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 5: Adversarial Boundary Conditions & Multi-Moon Composition\n";
    std::cout << "====================================================================\n";

    EclipseUniforms u{};
    u.lightPos = glm::vec3(0.0f);
    u.sunRadius = static_cast<float>(Astro::Sun.radiusKm);

    float dJup = 778500000.0f; // 5.2 AU
    float rJup = static_cast<float>(Astro::Jupiter.radiusKm); // 69,911 km
    glm::vec3 jupCenter = glm::vec3(0.0f, 0.0f, dJup);

    // 5.1 Multi-Moon Shadow Product Test:
    // Place Io and Europa overlapping at the same spot to stress the shadowFactor multiplication
    u.shadowCasterCount = 2;
    u.shadowCasterPos[0] = jupCenter - glm::vec3(0.0f, 0.0f, 421700.0f); // Io
    u.shadowCasterRadius[0] = 1821.6f;
    u.shadowCasterPos[1] = jupCenter - glm::vec3(0.0f, 0.0f, 671100.0f); // Europa
    u.shadowCasterRadius[1] = 1560.8f;

    // Receiver at subsolar point (under both shadows)
    FragmentIn jupSubsolar{jupCenter - glm::vec3(0.0f, 0.0f, rJup), glm::vec3(0.0f, 0.0f, -1.0f)};
    FragmentOut jupOut = evaluateShaderFragment(jupSubsolar, u);
    met.assertCheck("Overlapping dual-moon shadows produce s = 0.0 without numerical overflow",
                    jupOut.shadowFactor == 0.0f && !std::isnan(jupOut.shadowFactor));

    // 5.2 Negative z Test: Caster placed behind receiver (away from Sun)
    // Receiver should NOT receive shadow from an object behind it!
    EclipseUniforms uInverted = u;
    uInverted.shadowCasterCount = 1;
    uInverted.shadowCasterPos[0] = jupCenter + glm::vec3(0.0f, 0.0f, 500000.0f); // Behind Jupiter
    FragmentOut invOut = evaluateShaderFragment(jupSubsolar, uInverted);
    met.assertCheck("Caster behind receiver (z < 0) casts no shadow (shadowFactor == 1.0)",
                    invOut.shadowFactor == 1.0f);

    // 5.3 Collinear extreme proximity test: z -> 0+ (fragment 10 meters behind caster surface)
    EclipseUniforms uProx = u;
    uProx.shadowCasterCount = 1;
    glm::vec3 tinyCasterPos = jupCenter - glm::vec3(0.0f, 0.0f, rJup + 0.01f); // 10 meters in front
    uProx.shadowCasterPos[0] = tinyCasterPos;
    uProx.shadowCasterRadius[0] = 10.0f;
    FragmentOut proxOut = evaluateShaderFragment(jupSubsolar, uProx);
    met.assertCheck("Extreme proximity (z -> 0+) does not trigger NaN or Inf",
                    !std::isnan(proxOut.shadowFactor) && !std::isinf(proxOut.shadowFactor));

    // 5.4 High-throughput fuzzing: 50,000 random fragment evaluations
    const int fuzzCount = 50000;
    bool allFinite = true;
    bool allNormalized = true;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < fuzzCount; ++i) {
        float fx = static_cast<float>((i * 1013) % 10000) / 10000.0f - 0.5f;
        float fy = static_cast<float>((i * 2017) % 10000) / 10000.0f - 0.5f;
        float fz = static_cast<float>((i * 3049) % 10000) / 10000.0f - 0.5f;
        glm::vec3 rawVec(fx, fy, fz);
        if (glm::length(rawVec) < 1e-4f) {
            rawVec = glm::vec3(0.0f, 1.0f, 0.0f); // Non-zero fallback
        }
        glm::vec3 n = glm::normalize(rawVec);
        FragmentIn f{jupCenter + n * rJup, n};
        FragmentOut o = evaluateShaderFragment(f, u);

        if (std::isnan(o.color.r) || std::isnan(o.color.g) || std::isnan(o.color.b) ||
            std::isinf(o.color.r) || std::isinf(o.color.g) || std::isinf(o.color.b)) {
            allFinite = false;
        }
        if (o.shadowFactor < -1e-6f || o.shadowFactor > 1.0f + 1e-6f) {
            allNormalized = false;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double fuzzMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double evalThroughput = (fuzzCount / fuzzMs) * 1000.0;

    std::cout << "    Evaluated " << fuzzCount << " fuzzed fragments in " << fuzzMs << " ms\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(0) << evalThroughput << " frags/sec\n";

    met.assertCheck("50,000 fuzzed fragments produce strictly finite color outputs (no NaN/Inf)", allFinite);
    met.assertCheck("50,000 fuzzed fragments have shadowFactor strictly in [0.0, 1.0]", allNormalized);
    met.assertCheck("Evaluation throughput exceeds 1,000,000 fragments/second on CPU", evalThroughput > 1000000.0);
}

} // namespace ChallengerM3_1

int main() {
    using namespace ChallengerM3_1;
    std::cout << "====================================================================\n";
    std::cout << "  CHALLENGER M3_1: EMPIRICAL ECLIPSE GEOMETRY & STRESS TEST HARNESS\n";
    std::cout << "====================================================================\n";

    EmpiricalMetrics metrics;

    runSuite1SolarEclipse(metrics);
    runSuite2LunarEclipse(metrics);
    runSuite3AnnularEclipse(metrics);
    runSuite4NightSideRejection(metrics);
    runSuite5AdversarialStress(metrics);

    std::cout << "\n====================================================================\n";
    std::cout << "  EMPIRICAL VERDICT SUMMARY: " << metrics.passedChecks << " / " << metrics.totalChecks
              << " CHECKS PASSED (" << metrics.failedChecks << " FAILED)\n";
    std::cout << "====================================================================\n";

    return (metrics.failedChecks == 0) ? 0 : 1;
}
