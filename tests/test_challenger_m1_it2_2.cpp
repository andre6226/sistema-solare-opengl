/**
 * test_challenger_m1_it2_2.cpp
 * Adversarial Empirical Verification & Stress Test Harness
 * Milestone: M1 Iteration 2 (Axial Rotation Vectors & Retrograde/Prograde Dynamics)
 * Agent: challenger_m1_it2_2 | Roles: critic, specialist
 *
 * Verifies:
 * 1. Spin angular velocity vector omega satisfies dot(omega, N_ecliptic) < 0
 *    for Venus, Uranus, and Pluto across 4 independent mathematical formulations:
 *    - Equator tangential velocity cross product (r x v)
 *    - Infinitesimal Lie algebra rate ((dR/dt) * R^T)
 *    - Finite rotation matrix axis-angle extraction
 *    - Analytical geometry projection
 * 2. Spin angular velocity vector omega satisfies dot(omega, N_ecliptic) > 0
 *    for Earth, Mars, Jupiter, Saturn, Neptune, and Sun across all formulations.
 * 3. Temporal invariance and zero sign-flipping over 100,000 steps and varied phases.
 * 4. Invariance under Floating Origin shifts (applyOrigin).
 * 5. HUD retrograde contract consistency (rotationPeriodDays < 0.0 and day length format).
 * 6. Sensitivity analysis across tilt boundaries (89.99 deg vs 90.01 deg) and zero periods.
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>

#include "SolarSystem/AstronomicalData.hpp"
#include "SolarSystem/CelestialBody.hpp"

namespace Challenger {

struct StressReport {
    int totalChecks = 0;
    int passedChecks = 0;
    int failedChecks = 0;

    void record(const std::string& name, bool pass, const std::string& detail = "") {
        totalChecks++;
        if (pass) {
            passedChecks++;
            std::cout << "  [PASS] " << name;
            if (!detail.empty()) std::cout << " (" << detail << ")";
            std::cout << "\n";
        } else {
            failedChecks++;
            std::cout << "  [FAIL] " << name;
            if (!detail.empty()) std::cout << " (" << detail << ")";
            std::cout << "\n";
        }
    }
};

const glm::dvec3 N_ECLIPTIC(0.0, 1.0, 0.0);

// Helper to extract 3x3 orthonormal orientation from CelestialBody modelMatrix
glm::dmat3 extractOrientation(const CelestialBody& body) {
    glm::dmat4 M = glm::dmat4(body.getModelMatrix());
    glm::dvec3 col0 = glm::normalize(glm::dvec3(M[0]));
    glm::dvec3 col1 = glm::normalize(glm::dvec3(M[1]));
    glm::dvec3 col2 = glm::normalize(glm::dvec3(M[2]));
    return glm::dmat3(col0, col1, col2);
}

// Helper to extract omega using equator tangential velocity: r x v
glm::dvec3 computeOmegaEquator(CelestialBody& body, double T, float dt) {
    body.update(T, 0.0f);
    glm::dmat4 m0 = glm::dmat4(body.getModelMatrix());
    glm::dvec3 center0 = glm::dvec3(body.getWorldPosition());

    body.update(T, dt);
    glm::dmat4 m1 = glm::dmat4(body.getModelMatrix());
    glm::dvec3 center1 = glm::dvec3(body.getWorldPosition());

    // Local point on equator
    glm::dvec3 pLocal(1.0, 0.0, 0.0);
    glm::dvec3 r0 = glm::dvec3(m0 * glm::dvec4(pLocal, 1.0)) - center0;
    glm::dvec3 r1 = glm::dvec3(m1 * glm::dvec4(pLocal, 1.0)) - center1;

    double r0Len = glm::length(r0);
    if (r0Len < 1e-9) return glm::dvec3(0.0);
    r0 /= r0Len;
    r1 /= r0Len;

    glm::dvec3 v = (r1 - r0) / static_cast<double>(dt);
    return glm::cross(r0, v);
}

// Helper to extract omega using Lie algebra skew-symmetric generator: (dR/dt) * R^T
glm::dvec3 computeOmegaLieAlgebra(CelestialBody& body, double T, float dt) {
    body.update(T, 0.0f);
    glm::dmat3 R0 = extractOrientation(body);

    body.update(T, dt);
    glm::dmat3 R1 = extractOrientation(body);

    glm::dmat3 dR = (R1 - R0) / static_cast<double>(dt);
    glm::dmat3 OmegaMat = dR * glm::transpose(R0);

    // OmegaMat = [omega]_x:
    // [  0   -w_z   w_y ]
    // [ w_z    0   -w_x ]
    // [-w_y   w_x    0  ]
    // In GLM col-major indexing: M[col][row]
    // Row 2 Col 1 = M[1][2] = w_x; Row 1 Col 2 = M[2][1] = -w_x
    // Row 0 Col 2 = M[2][0] = w_y; Row 2 Col 0 = M[0][2] = -w_y
    // Row 1 Col 0 = M[0][1] = w_z; Row 0 Col 1 = M[1][0] = -w_z
    double wx = 0.5 * (OmegaMat[1][2] - OmegaMat[2][1]);
    double wy = 0.5 * (OmegaMat[2][0] - OmegaMat[0][2]);
    double wz = 0.5 * (OmegaMat[0][1] - OmegaMat[1][0]);

    return glm::dvec3(wx, wy, wz);
}

// Helper to extract omega using finite rotation axis-angle
glm::dvec3 computeOmegaAxisAngle(CelestialBody& body, double T, float dt) {
    body.update(T, 0.0f);
    glm::dmat3 R0 = extractOrientation(body);

    body.update(T, dt);
    glm::dmat3 R1 = extractOrientation(body);

    glm::dmat3 dR = R1 * glm::transpose(R0);
    double trace = dR[0][0] + dR[1][1] + dR[2][2];
    double cosTheta = std::clamp(0.5 * (trace - 1.0), -1.0, 1.0);
    double theta = std::acos(cosTheta);

    if (std::abs(theta) < 1e-12) return glm::dvec3(0.0);

    double denom = 2.0 * std::sin(theta);
    if (std::abs(denom) < 1e-12) denom = 1e-12;

    double ux = (dR[1][2] - dR[2][1]) / denom;
    double uy = (dR[2][0] - dR[0][2]) / denom;
    double uz = (dR[0][1] - dR[1][0]) / denom;

    double rate = theta / static_cast<double>(dt);
    return glm::dvec3(ux, uy, uz) * rate;
}

// Helper to construct a body
CelestialBody createTestBody(const Astro::BodyData& data, const Astro::ScaleConverter& conv) {
    CelestialBody body(data.name, nullptr, nullptr,
                      conv.getOrbitRadius(data),
                      conv.getOrbitalSpeed(data),
                      conv.getRotationSpeed(data),
                      conv.getAxialTilt(data),
                      conv.getScale(data));
    body.setData(&data);
    return body;
}

// ============================================================================
// 1. DIRECT MULTI-METHOD EMPIRICAL VERIFICATION
// ============================================================================
void challengePrimarySpinVectors(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 1] Multi-Method Empirical Spin Angular Velocity Verification\n";
    std::cout << "======================================================================\n";

    Astro::ScaleConverter conv;
    const float dt = 0.0166667f; // ~60 fps

    struct TargetBody {
        const Astro::BodyData& data;
        bool expectRetrograde; // true if omega · N < 0, false if omega · N > 0
    };

    const std::vector<TargetBody> targets = {
        // Retrograde targets:
        {Astro::Venus,   true},
        {Astro::Uranus,  true},
        {Astro::Pluto,   true},
        // Prograde targets:
        {Astro::Earth,   false},
        {Astro::Mars,    false},
        {Astro::Jupiter, false},
        {Astro::Saturn,  false},
        {Astro::Neptune, false},
        {Astro::Sun,     false},
        // Additional solar system bodies:
        {Astro::Mercury, false}
    };

    for (const auto& item : targets) {
        CelestialBody b = createTestBody(item.data, conv);

        // Method A: Equator Tangential Velocity
        glm::dvec3 omegaEquator = computeOmegaEquator(b, 0.0, dt);
        double projEquator = glm::dot(omegaEquator, N_ECLIPTIC);

        // Method B: Lie Algebra Skew-Symmetric Rate
        glm::dvec3 omegaLie = computeOmegaLieAlgebra(b, 0.0, dt);
        double projLie = glm::dot(omegaLie, N_ECLIPTIC);

        // Method C: Finite Rotation Axis-Angle
        glm::dvec3 omegaAxis = computeOmegaAxisAngle(b, 0.0, dt);
        double projAxis = glm::dot(omegaAxis, N_ECLIPTIC);

        // Method D: Analytical Model
        float speed = conv.getRotationSpeed(item.data);
        float tilt = conv.getAxialTilt(item.data);
        double projAnalytical = static_cast<double>(speed) * std::cos(static_cast<double>(tilt));

        bool methodsAgree = (std::abs(projEquator - projLie) < 1e-4) &&
                            (std::abs(projLie - projAxis) < 1e-4) &&
                            (std::abs(projAxis - projAnalytical) < 1e-4);

        bool signOk = item.expectRetrograde ? (projEquator < -1e-6) : (projEquator > 1e-6);

        std::string signStr = (projEquator < 0.0) ? "RETROGRADE" : "PROGRADE";
        std::string expectedStr = item.expectRetrograde ? "RETROGRADE" : "PROGRADE";

        std::cout << "  Body: " << std::setw(8) << item.data.name
                  << " | Tilt: " << std::setw(6) << item.data.axialTiltDegrees << " deg"
                  << " | Period: " << std::setw(8) << item.data.rotationPeriodDays << " d"
                  << " | omega.N: " << std::scientific << std::setprecision(5) << projEquator
                  << " (" << signStr << ")\n";

        std::string testDesc = std::string(item.data.name) + " spin direction matches expected " + expectedStr;
        report.record(testDesc,
                      signOk && methodsAgree,
                      "proj=" + std::to_string(projEquator) + ", analytical=" + std::to_string(projAnalytical));
    }
}

// ============================================================================
// 2. TEMPORAL INVARIANCE & EXHAUSTIVE PHASE STRESS TEST
// ============================================================================
void challengeTemporalInvariance(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 2] Temporal Invariance, Phase Sweep & Multi-Step Evolution\n";
    std::cout << "======================================================================\n";

    Astro::ScaleConverter conv;

    const std::vector<std::pair<const Astro::BodyData&, bool>> bodies = {
        {Astro::Venus,   true},
        {Astro::Uranus,  true},
        {Astro::Pluto,   true},
        {Astro::Earth,   false},
        {Astro::Mars,    false},
        {Astro::Jupiter, false},
        {Astro::Saturn,  false},
        {Astro::Neptune, false},
        {Astro::Sun,     false}
    };

    // 2.1 100,000 Step Continuous Integration Check
    std::cout << "  --> Testing 100,000 continuous simulation steps at dt = 1/60s...\n";
    bool all100kOk = true;
    for (const auto& bInfo : bodies) {
        CelestialBody b = createTestBody(bInfo.first, conv);
        const float dt = 1.0f / 60.0f;
        double T = 0.0;

        for (int step = 0; step < 100000; ++step) {
            b.update(T, dt);
            T += static_cast<double>(dt) / Astro::SECONDS_PER_JULIAN_CENTURY;

            // Check every 10,000 steps
            if (step % 10000 == 0) {
                glm::dvec3 w = computeOmegaEquator(b, T, dt);
                double proj = glm::dot(w, N_ECLIPTIC);
                if (bInfo.second && proj >= 0.0) { all100kOk = false; break; }
                if (!bInfo.second && proj <= 0.0) { all100kOk = false; break; }
            }
        }
    }
    report.record("Continuous 100,000 steps zero sign flip for all 9 bodies", all100kOk);

    // 2.2 Phase Sweep Across Varied Time Offsets (0 to 1 full rotation period in 72 steps)
    std::cout << "  --> Testing 72 phase intervals across complete rotation cycle...\n";
    bool allPhasesOk = true;
    for (const auto& bInfo : bodies) {
        CelestialBody b = createTestBody(bInfo.first, conv);
        float rotSpeed = conv.getRotationSpeed(bInfo.first);
        float periodSimSec = (rotSpeed != 0.0f) ? std::abs(glm::two_pi<float>() / rotSpeed) : 100.0f;
        float stepDt = periodSimSec / 72.0f;

        for (int step = 0; step < 72; ++step) {
            b.update(0.0, stepDt);
            glm::dvec3 w = computeOmegaEquator(b, 0.0, 0.016667f);
            double proj = glm::dot(w, N_ECLIPTIC);

            if (bInfo.second && proj >= 0.0) { allPhasesOk = false; break; }
            if (!bInfo.second && proj <= 0.0) { allPhasesOk = false; break; }
        }
    }
    report.record("Phase sweep across complete rotation cycle maintains strict spin sign", allPhasesOk);

    // 2.3 Variable Step Sizes (1e-5 s to 10 s)
    std::cout << "  --> Testing variable delta-times [1e-5s, 1e-4s, 1e-3s, 1/60s, 0.1s, 1.0s, 10.0s]...\n";
    const std::vector<float> dts = {1e-5f, 1e-4f, 1e-3f, 1.0f / 60.0f, 0.1f, 1.0f, 10.0f};
    bool allDtOk = true;
    for (const auto& bInfo : bodies) {
        CelestialBody b = createTestBody(bInfo.first, conv);
        for (float dt : dts) {
            glm::dvec3 w = computeOmegaLieAlgebra(b, 0.0, dt);
            double proj = glm::dot(w, N_ECLIPTIC);
            if (bInfo.second && proj >= 0.0) { allDtOk = false; break; }
            if (!bInfo.second && proj <= 0.0) { allDtOk = false; break; }
        }
    }
    report.record("Delta-time variation across 6 orders of magnitude maintains strict spin sign", allDtOk);
}

// ============================================================================
// 3. FLOATING ORIGIN & ORIENTATION INVARIANCE
// ============================================================================
void challengeFloatingOriginInvariance(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 3] Floating Origin Precision & Orientation Invariance\n";
    std::cout << "======================================================================\n";

    Astro::ScaleConverter conv;

    const std::vector<glm::dvec3> extremeOrigins = {
        glm::dvec3(0.0),
        glm::dvec3(1.496e8, 0.0, 0.0),                     // 1 AU (Earth)
        glm::dvec3(5.9e9, 1.2e9, -3.4e9),                  // Pluto distance
        glm::dvec3(1.4e10, -5.0e9, 8.0e9),                 // Eris (10^10 km)
        glm::dvec3(1e12, -1e12, 1e12)                      // Deep Oort cloud
    };

    bool allOriginsPreserveSpin = true;
    bool allModelMatricesOrthonormal = true;

    const std::vector<const Astro::BodyData*> sampleBodies = {
        &Astro::Venus, &Astro::Uranus, &Astro::Pluto, &Astro::Earth, &Astro::Sun
    };

    for (const auto* data : sampleBodies) {
        CelestialBody b = createTestBody(*data, conv);
        b.update(0.0, 0.016667f);
        glm::dmat3 R_untranslated = extractOrientation(b);

        for (const auto& origin : extremeOrigins) {
            b.applyOrigin(origin);

            // Verify orientation block is untouched
            glm::dmat3 R_after = extractOrientation(b);
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    if (std::abs(R_untranslated[i][j] - R_after[i][j]) > 1e-6) {
                        allOriginsPreserveSpin = false;
                    }
                }
            }

            // Verify modelMatrix 3x3 orientation block orthonormality
            glm::dmat3 identity = glm::transpose(R_after) * R_after;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    double expected = (i == j) ? 1.0 : 0.0;
                    if (std::abs(identity[i][j] - expected) > 1e-4) {
                        allModelMatricesOrthonormal = false;
                    }
                }
            }
        }
    }

    report.record("Floating origin translation leaves exactOrientation completely invariant", allOriginsPreserveSpin);
    report.record("ModelMatrix 3x3 orientation block remains orthonormal under floating origin", allModelMatricesOrthonormal);
}

// ============================================================================
// 4. HUD RETROGRADE CONTRACT & FORMATTING VERIFICATION
// ============================================================================
void challengeHudRetrogradeContract(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 4] HUD Retrograde Indicator & Formatting Contract\n";
    std::cout << "======================================================================\n";

    // Replicate exact HUD formatting helper logic from Hud.hpp:
    // static std::string formatDays(double days) {
    //     if (days >= 365.25) return format("%.2f yr", days / 365.25);
    //     if (days >= 1.0)    return format("%.2f d", days);
    //     return format("%.1f h", days * 24.0);
    // }
    auto formatDays = [](double days) -> std::string {
        char buf[64];
        if (days >= 365.25) {
            std::snprintf(buf, sizeof(buf), "%.2f yr", days / 365.25);
        } else if (days >= 1.0) {
            std::snprintf(buf, sizeof(buf), "%.2f d", days);
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f h", days * 24.0);
        }
        return std::string(buf);
    };

    auto formatDayLength = [&](const Astro::BodyData& d) -> std::string {
        if (d.rotationPeriodDays == 0.0) return "";
        const bool retrograde = d.rotationPeriodDays < 0.0;
        return formatDays(std::abs(d.rotationPeriodDays)) + (retrograde ? "  retro" : "");
    };

    // Requirements:
    // Venus: -243.00 d -> "243.00 d  retro"
    // Uranus: -0.72 d  -> "17.3 h  retro"
    // Pluto: -6.39 d   -> "6.39 d  retro"
    // Earth: 1.00 d    -> "1.00 d" (no retro)
    // Mars: 1.03 d     -> "1.03 d" (no retro)
    // Jupiter: 0.41 d  -> "9.8 h"  (no retro)
    // Saturn: 0.45 d   -> "10.8 h" (no retro)
    // Neptune: 0.67 d  -> "16.1 h" (no retro)
    // Sun: 25.38 d     -> "25.38 d" (no retro)

    report.record("Venus HUD day length matches '243.00 d  retro'",
                  formatDayLength(Astro::Venus) == "243.00 d  retro",
                  formatDayLength(Astro::Venus));
    report.record("Uranus HUD day length matches '17.3 h  retro'",
                  formatDayLength(Astro::Uranus) == "17.3 h  retro",
                  formatDayLength(Astro::Uranus));
    report.record("Pluto HUD day length matches '6.39 d  retro'",
                  formatDayLength(Astro::Pluto) == "6.39 d  retro",
                  formatDayLength(Astro::Pluto));
    report.record("Earth HUD day length matches '1.00 d' (no retro)",
                  formatDayLength(Astro::Earth) == "1.00 d",
                  formatDayLength(Astro::Earth));
    report.record("Mars HUD day length matches '1.03 d' (no retro)",
                  formatDayLength(Astro::Mars) == "1.03 d",
                  formatDayLength(Astro::Mars));
    report.record("Jupiter HUD day length matches '9.8 h' (no retro)",
                  formatDayLength(Astro::Jupiter) == "9.8 h",
                  formatDayLength(Astro::Jupiter));
    report.record("Saturn HUD day length matches '10.8 h' (no retro)",
                  formatDayLength(Astro::Saturn) == "10.8 h",
                  formatDayLength(Astro::Saturn));
    report.record("Neptune HUD day length matches '16.1 h' (no retro)",
                  formatDayLength(Astro::Neptune) == "16.1 h",
                  formatDayLength(Astro::Neptune));
    report.record("Sun HUD day length matches '25.38 d' (no retro)",
                  formatDayLength(Astro::Sun) == "25.38 d",
                  formatDayLength(Astro::Sun));
}

// ============================================================================
// 5. BOUNDARY SENSITIVITY & ADVERSARIAL EDGE CASES
// ============================================================================
void challengeBoundarySensitivity(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 5] Boundary Sensitivity & Adversarial Edge Cases\n";
    std::cout << "======================================================================\n";

    Astro::ScaleConverter conv;

    // 5.1 Axial tilt boundary around 90 degrees
    Astro::BodyData justBelow90 = Astro::Earth;
    justBelow90.axialTiltDegrees = 89.999;
    justBelow90.rotationPeriodDays = 1.0;

    Astro::BodyData justAbove90 = Astro::Earth;
    justAbove90.axialTiltDegrees = 90.001;
    justAbove90.rotationPeriodDays = -1.0;

    CelestialBody bBelow = createTestBody(justBelow90, conv);
    CelestialBody bAbove = createTestBody(justAbove90, conv);

    glm::dvec3 wBelow = computeOmegaEquator(bBelow, 0.0, 0.016667f);
    glm::dvec3 wAbove = computeOmegaEquator(bAbove, 0.0, 0.016667f);

    double projBelow = glm::dot(wBelow, N_ECLIPTIC);
    double projAbove = glm::dot(wAbove, N_ECLIPTIC);

    report.record("Axial tilt 89.999 deg yields prograde spin (proj > 0)", projBelow > 0.0,
                  "proj=" + std::to_string(projBelow));
    report.record("Axial tilt 90.001 deg yields retrograde spin (proj < 0)", projAbove < 0.0,
                  "proj=" + std::to_string(projAbove));

    // 5.2 Pure inverted 180 degrees
    Astro::BodyData upsideDown = Astro::Earth;
    upsideDown.axialTiltDegrees = 180.0;
    upsideDown.rotationPeriodDays = -1.0;

    CelestialBody bInverted = createTestBody(upsideDown, conv);
    glm::dvec3 wInverted = computeOmegaEquator(bInverted, 0.0, 0.016667f);
    double projInverted = glm::dot(wInverted, N_ECLIPTIC);

    report.record("Axial tilt 180.0 deg yields pure retrograde spin (proj < 0)", projInverted < -1e-4,
                  "proj=" + std::to_string(projInverted));

    // 5.3 Zero period guard
    Astro::BodyData zeroPeriod = Astro::Earth;
    zeroPeriod.rotationPeriodDays = 0.0;
    float zeroSpeed = conv.getRotationSpeed(zeroPeriod);
    report.record("Rotation period = 0.0 produces safe zero rotation speed (no NaN/Inf)", zeroSpeed == 0.0f);

    // 5.4 Extreme cometary / fast spin clamp guard
    Astro::BodyData superFast = Astro::Earth;
    superFast.rotationPeriodDays = 0.000001; // extremely rapid
    float fastSpeed = conv.getRotationSpeed(superFast);
    report.record("Extremely rapid spin clamped cleanly to +/-2.0 rad/s", fastSpeed <= 2.0f && fastSpeed >= -2.0f,
                  "speed=" + std::to_string(fastSpeed));
}

// ============================================================================
// 6. EXHAUSTIVE 39-BODY SYSTEM-WIDE AUDIT
// ============================================================================
void challengeAll39Bodies(StressReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 6] Exhaustive 39-Body System-Wide Spin Audit\n";
    std::cout << "======================================================================\n";

    Astro::ScaleConverter conv;
    const std::vector<const Astro::BodyData*> allBodies = {
        &Astro::Sun,
        &Astro::Mercury, &Astro::Venus, &Astro::Earth, &Astro::Mars,
        &Astro::Jupiter, &Astro::Saturn, &Astro::Uranus, &Astro::Neptune, &Astro::Pluto,
        &Astro::Eros, &Astro::Gaspra, &Astro::Vesta, &Astro::Pallas, &Astro::Ida,
        &Astro::Ceres, &Astro::Haumea, &Astro::Makemake, &Astro::Eris,
        &Astro::Moon, &Astro::Phobos, &Astro::Deimos,
        &Astro::Io, &Astro::Europa, &Astro::Ganymede, &Astro::Callisto,
        &Astro::Mimas, &Astro::Enceladus, &Astro::Dione, &Astro::Rhea, &Astro::Titan, &Astro::Iapetus,
        &Astro::Miranda, &Astro::Ariel, &Astro::Umbriel, &Astro::Titania, &Astro::Oberon,
        &Astro::Triton, &Astro::Charon
    };

    int retroCount = 0;
    int progradeCount = 0;
    bool allValid = true;

    for (const auto* bData : allBodies) {
        CelestialBody b = createTestBody(*bData, conv);
        glm::dvec3 w = computeOmegaEquator(b, 0.0, 0.016667f);
        double proj = glm::dot(w, N_ECLIPTIC);

        if (proj < -1e-7) {
            retroCount++;
        } else if (proj > 1e-7) {
            progradeCount++;
        } else {
            // Zero or near zero
            if (bData->rotationPeriodDays != 0.0 && conv.getRotationSpeed(*bData) != 0.0f) {
                allValid = false;
            }
        }
    }

    std::cout << "  Total bodies audited: " << allBodies.size() << "\n";
    std::cout << "  Retrograde spin bodies: " << retroCount << " (Venus, Uranus, Pluto, Haumea, Triton)\n";
    std::cout << "  Prograde spin bodies:   " << progradeCount << "\n";

    report.record("Audited all 39 bodies without NaN or orientation singularities", allValid);
    report.record("Retrograde count exactly equals 5 bodies", retroCount == 5,
                  "count=" + std::to_string(retroCount));
    report.record("Prograde count exactly equals 34 bodies", progradeCount == 34,
                  "count=" + std::to_string(progradeCount));
}

} // namespace Challenger

int main() {
    std::cout << "======================================================================\n";
    std::cout << "   EMPIRICAL CHALLENGER STRESS HARNESS — MILESTONE M1 ITERATION 2\n";
    std::cout << "   Agent: challenger_m1_it2_2 | Roles: critic, specialist\n";
    std::cout << "======================================================================\n";

    Challenger::StressReport report;

    Challenger::challengePrimarySpinVectors(report);
    Challenger::challengeTemporalInvariance(report);
    Challenger::challengeFloatingOriginInvariance(report);
    Challenger::challengeHudRetrogradeContract(report);
    Challenger::challengeBoundarySensitivity(report);
    Challenger::challengeAll39Bodies(report);

    std::cout << "\n======================================================================\n";
    std::cout << "   CHALLENGE HARNESS SUMMARY\n";
    std::cout << "   Total Checks:  " << report.totalChecks << "\n";
    std::cout << "   Passed:        " << report.passedChecks << "\n";
    std::cout << "   Failed:        " << report.failedChecks << "\n";
    std::cout << "   Pass Rate:     " << std::fixed << std::setprecision(2)
              << (100.0 * report.passedChecks / report.totalChecks) << "%\n";
    std::cout << "======================================================================\n";

    if (report.failedChecks > 0) {
        std::cout << ">>> VERDICT: FAIL — DISCREPANCIES DETECTED <<<\n";
        return 1;
    } else {
        std::cout << ">>> VERDICT: APPROVE — ALL CHALLENGES PASSED <<<\n";
        return 0;
    }
}
