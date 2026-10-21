#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include "SolarSystem/AstronomicalData.hpp"

// Independent implementation of GLSL shader math from base.frag
inline float smoothstep(float edge0, float edge1, float x) {
    if (edge0 >= edge1) return (x >= edge1) ? 1.0f : 0.0f;
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct Caster {
    glm::vec3 pos;
    float radius;
};

inline float computeShadowFactor(const glm::vec3& fragWorldPos,
                                 const glm::vec3& fragNormal,
                                 const glm::vec3& lightPos,
                                 float sunRadius,
                                 const std::vector<Caster>& casters)
{
    glm::vec3 normal = glm::normalize(fragNormal);
    glm::vec3 lightDir = glm::normalize(lightPos - fragWorldPos);
    float diff = std::max(glm::dot(normal, lightDir), 0.0f);

    float shadowFactor = 1.0f;
    if (diff > 0.0f) {
        for (const auto& caster : casters) {
            glm::vec3 casterPos = caster.pos;
            float casterRadius = caster.radius;
            glm::vec3 a = glm::normalize(casterPos - lightPos);
            float dSC = glm::length(casterPos - lightPos);
            glm::vec3 v = fragWorldPos - casterPos;
            float z = glm::dot(v, a);
            if (z > 0.0f) {
                float tanUmbra = (sunRadius - casterRadius) / dSC;
                float tanPenumbra = (sunRadius + casterRadius) / dSC;
                float rUmbra = casterRadius - z * tanUmbra;
                float rPenumbra = casterRadius + z * tanPenumbra;
                glm::vec3 vPerp = v - z * a;
                float dPerpSq = glm::dot(vPerp, vPerp);
                if (dPerpSq < rPenumbra * rPenumbra) {
                    float dPerp = std::sqrt(dPerpSq);
                    float umbraBound = std::max(0.0f, rUmbra);
                    float s_i = smoothstep(umbraBound, rPenumbra, dPerp);
                    if (rUmbra < 0.0f) {
                        float occultation = std::clamp((casterRadius * dSC) / (sunRadius * z), 0.0f, 1.0f);
                        float sMin = 1.0f - (occultation * occultation);
                        s_i = sMin + (1.0f - sMin) * s_i;
                    }
                    shadowFactor *= s_i;
                }
            }
        }
    }
    return shadowFactor;
}

int main() {
    std::cout << "--- AUDITOR INDEPENDENT STRESS TEST: ECLIPSE MATH ---\n";
    int checks = 0, passed = 0;

    glm::vec3 lightPos(0.0f);
    float sunRadius = static_cast<float>(Astro::Sun.radiusKm);

    // 1. Solar eclipse syzygy test
    float dEarth = 149600000.0f;
    float rEarth = static_cast<float>(Astro::Earth.radiusKm);
    glm::vec3 earthCenter(0.0f, 0.0f, dEarth);
    glm::vec3 subsolar = earthCenter - glm::vec3(0.0f, 0.0f, rEarth);
    glm::vec3 normFacingSun(0.0f, 0.0f, -1.0f);

    float dMoon = 363300.0f;
    float rMoon = static_cast<float>(Astro::Moon.radiusKm);
    glm::vec3 moonCenter = earthCenter - glm::vec3(0.0f, 0.0f, dMoon);

    std::vector<Caster> casters = {{moonCenter, rMoon}};

    // Check totality at center
    float sTotality = computeShadowFactor(subsolar, normFacingSun, lightPos, sunRadius, casters);
    checks++;
    if (sTotality == 0.0f) {
        passed++;
        std::cout << "  [PASS] Totality at subsolar point s = " << sTotality << "\n";
    } else {
        std::cout << "  [FAIL] Expected 0.0, got " << sTotality << "\n";
    }

    // Check continuous sweep of shadow spot across Earth
    bool continuous = true;
    float prevS = -1.0f;
    for (int step = 0; step <= 100; ++step) {
        float xOffset = -5000.0f + (10000.0f * step) / 100.0f;
        glm::vec3 pos = subsolar + glm::vec3(xOffset, 0.0f, 0.0f);
        float s = computeShadowFactor(pos, normFacingSun, lightPos, sunRadius, casters);
        if (s < 0.0f || s > 1.0f) continuous = false;
    }
    checks++;
    if (continuous) {
        passed++;
        std::cout << "  [PASS] Continuous sweep across surface bounded in [0, 1]\n";
    } else {
        std::cout << "  [FAIL] Unbounded shadowFactor during sweep\n";
    }

    // 2. Off-axis Moon: when Moon moves, shadow spot moves
    glm::vec3 moonShifted = moonCenter + glm::vec3(2000.0f, 0.0f, 0.0f);
    std::vector<Caster> shiftedCasters = {{moonShifted, rMoon}};
    float sSubsolarAfterShift = computeShadowFactor(subsolar, normFacingSun, lightPos, sunRadius, shiftedCasters);
    checks++;
    if (sSubsolarAfterShift > 0.0f) {
        passed++;
        std::cout << "  [PASS] Off-axis Moon moves shadow off subsolar point (s = " << sSubsolarAfterShift << " > 0)\n";
    } else {
        std::cout << "  [FAIL] Subsolar still dark after Moon shift\n";
    }

    // 3. Annular eclipse (Apogee Moon d = 405,000 km)
    glm::vec3 moonApogee = earthCenter - glm::vec3(0.0f, 0.0f, 405000.0f);
    std::vector<Caster> apogeeCaster = {{moonApogee, rMoon}};
    float sAnnular = computeShadowFactor(subsolar, normFacingSun, lightPos, sunRadius, apogeeCaster);
    checks++;
    if (sAnnular > 0.0f && sAnnular < 0.2f) {
        passed++;
        std::cout << "  [PASS] Annular eclipse (antumbra) produces partial darkening: s = " << sAnnular << "\n";
    } else {
        std::cout << "  [FAIL] Expected annular attenuation in (0, 0.2), got " << sAnnular << "\n";
    }

    // 4. Lunar eclipse test: Earth casting shadow on Moon
    std::vector<Caster> earthCaster = {{earthCenter, rEarth}};
    glm::vec3 moonInEclipse = earthCenter + glm::vec3(0.0f, 0.0f, 384400.0f);
    glm::vec3 moonSubsolar = moonInEclipse - glm::vec3(0.0f, 0.0f, rMoon);
    float sLunar = computeShadowFactor(moonSubsolar, normFacingSun, lightPos, sunRadius, earthCaster);
    checks++;
    if (sLunar == 0.0f) {
        passed++;
        std::cout << "  [PASS] Lunar eclipse: Moon completely in Earth's umbra s = " << sLunar << "\n";
    } else {
        std::cout << "  [FAIL] Expected 0.0, got " << sLunar << "\n";
    }

    // 5. Zero casters produces 1.0 everywhere
    std::vector<Caster> emptyCasters;
    float sEmpty = computeShadowFactor(subsolar, normFacingSun, lightPos, sunRadius, emptyCasters);
    checks++;
    if (sEmpty == 1.0f) {
        passed++;
        std::cout << "  [PASS] Zero casters produces unshadowed s = 1.0\n";
    } else {
        std::cout << "  [FAIL] Expected 1.0 with 0 casters, got " << sEmpty << "\n";
    }

    // 6. Night-side gating: norm facing away from Sun
    glm::vec3 normAwayFromSun(0.0f, 0.0f, 1.0f);
    float sNight = computeShadowFactor(subsolar, normAwayFromSun, lightPos, sunRadius, casters);
    checks++;
    if (sNight == 1.0f) {
        passed++;
        std::cout << "  [PASS] Night side returns s = 1.0 (relying on diff <= 0 for lighting)\n";
    } else {
        std::cout << "  [FAIL] Expected 1.0 on night side, got " << sNight << "\n";
    }

    std::cout << "\nRESULT: " << passed << " / " << checks << " PASSED\n";
    return (passed == checks) ? 0 : 1;
}
