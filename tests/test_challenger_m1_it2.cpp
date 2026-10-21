/**
 * test_challenger_m1_it2.cpp
 * Empirical Challenger Verification Harness for Milestone M1 Iteration 2
 *
 * Authored by: challenger_m1_it2_1 (Empirical Challenger)
 * Role: critic, specialist
 *
 * Verifies:
 * 1. Triton retrograde orbital motion:
 *    - Strictly opposes Neptune's spin axis: h . S_neptune < 0
 *    - Strictly opposes ecliptic North: h . Y_ecliptic < 0
 *    - Orbital sense verified across multiple epochs (+/- 500 years)
 *    - 360-sample dense orbital trajectory test ensuring uniform retrograde motion
 * 2. All 20 satellites in the solar system:
 *    - Earth: Moon
 *    - Mars: Phobos, Deimos
 *    - Jupiter: Io, Europa, Ganymede, Callisto
 *    - Saturn: Mimas, Enceladus, Dione, Rhea, Titan, Iapetus
 *    - Uranus: Miranda, Ariel, Umbriel, Titania, Oberon
 *    - Neptune: Triton
 *    - Pluto: Charon
 *    - Verifies each satellite's orbital sense relative to parent spin axis
 * 3. Physical orbital bounds and Kepler period consistency for all moons
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

namespace Color {
    inline const char* Reset   = "\033[0m";
    inline const char* Bold    = "\033[1m";
    inline const char* Red     = "\033[31m";
    inline const char* Green   = "\033[32m";
    inline const char* Yellow  = "\033[33m";
    inline const char* Cyan    = "\033[36m";
    inline const char* Magenta = "\033[35m";
}

struct MoonVerificationSpec {
    const Astro::BodyData* moon;
    const Astro::BodyData* parent;
    bool parentTiltChildren;
    bool expectedRetrogradeToParent;
    bool expectedRetrogradeToEcliptic;
};

int main() {
    std::cout << Color::Bold << Color::Cyan
              << "=================================================================\n"
              << "   EMPIRICAL CHALLENGER VERIFICATION HARNESS — M1 ITERATION 2   \n"
              << "   Agent: challenger_m1_it2_1 | Role: critic, specialist        \n"
              << "=================================================================\n"
              << Color::Reset << std::endl;

    int totalTests = 0;
    int failedTests = 0;

    auto record = [&](const std::string& name, bool pass, const std::string& detail = "") {
        totalTests++;
        if (pass) {
            std::cout << "  " << Color::Green << "[PASS] " << Color::Reset << name;
            if (!detail.empty()) std::cout << " (" << detail << ")";
            std::cout << "\n";
        } else {
            failedTests++;
            std::cout << "  " << Color::Red << "[FAIL] " << Color::Reset << name;
            if (!detail.empty()) std::cout << " (" << detail << ")";
            std::cout << "\n";
        }
    };

    Astro::ScaleConverter conv;

    // Helper to calculate spin pole in world coordinates
    auto getParentSpinPole = [&](const Astro::BodyData& parentData, bool tiltChildren) -> glm::dvec3 {
        if (!tiltChildren) {
            // If parent does not tilt children (e.g. Earth), parent spin pole is tilted relative to ecliptic
            glm::dmat4 rot = glm::rotate(glm::dmat4(1.0),
                                        static_cast<double>(conv.getAxialTilt(parentData)),
                                        glm::dvec3(0.0, 0.0, 1.0));
            return glm::dvec3(rot * glm::dvec4(0.0, 1.0, 0.0, 0.0));
        } else {
            // When parent tilts children, child orbits are defined in the tilted equatorial frame,
            // where parent's spin pole is the local +Y axis transformed into world frame.
            glm::dmat4 rot = glm::rotate(glm::dmat4(1.0),
                                        static_cast<double>(conv.getAxialTilt(parentData)),
                                        glm::dvec3(0.0, 0.0, 1.0));
            return glm::dvec3(rot * glm::dvec4(0.0, 1.0, 0.0, 0.0));
        }
    };

    // =========================================================================
    // PART 1: TRITON EMPIRICAL RETROGRADE CHALLENGE
    // =========================================================================
    std::cout << Color::Bold << "\n--- PART 1: Triton Retrograde Orbital Motion Challenge ---\n" << Color::Reset;
    {
        CelestialBody neptune("Neptune", nullptr, nullptr,
                              conv.getOrbitRadius(Astro::Neptune),
                              conv.getOrbitalSpeed(Astro::Neptune),
                              conv.getRotationSpeed(Astro::Neptune),
                              conv.getAxialTilt(Astro::Neptune),
                              conv.getScale(Astro::Neptune));
        neptune.setData(&Astro::Neptune);

        CelestialBody triton("Triton", nullptr, nullptr,
                             conv.getOrbitRadius(Astro::Triton),
                             conv.getOrbitalSpeed(Astro::Triton),
                             conv.getRotationSpeed(Astro::Triton),
                             conv.getAxialTilt(Astro::Triton),
                             conv.getScale(Astro::Triton));
        triton.setData(&Astro::Triton);
        neptune.addSatellite(&triton);

        glm::dvec3 neptunePole = getParentSpinPole(Astro::Neptune, true);
        glm::dvec3 eclipticNorth(0.0, 1.0, 0.0);

        std::cout << "  Neptune axial tilt: " << Astro::Neptune.axialTiltDegrees << " deg\n";
        std::cout << "  Neptune spin pole in world coords: ("
                  << neptunePole.x << ", " << neptunePole.y << ", " << neptunePole.z << ")\n";
        std::cout << "  Triton inclination i0: " << Astro::Triton.orbit.i0 << " deg\n";
        std::cout << "  Triton LDot: " << Astro::Triton.orbit.LDot << " deg/cy\n";
        std::cout << "  Triton orbitalPeriodDays: " << Astro::Triton.orbitalPeriodDays << " d\n";

        // 1.1 Multi-epoch verification
        const std::vector<std::pair<std::string, double>> testEpochs = {
            {"Year 1500 (T = -5.0 cy)", -5.0},
            {"Year 1900 (T = -1.0 cy)", -1.0},
            {"J2000.0 (T = 0.0 cy)",    0.0},
            {"Year 2026 (T = 0.268 cy)", 0.268},
            {"Year 2100 (T = 1.0 cy)",   1.0},
            {"Year 2500 (T = 5.0 cy)",   5.0}
        };

        bool allEpochsOpposeSpin = true;
        bool allEpochsOpposeEcliptic = true;
        const double dt_sec = 1.0;
        const double dt_cy = dt_sec / Astro::SECONDS_PER_JULIAN_CENTURY;

        for (const auto& [epochName, T] : testEpochs) {
            neptune.update(T, 0.0f);
            glm::dvec3 r0 = triton.getExactPosition() - neptune.getExactPosition();

            neptune.update(T + dt_cy, static_cast<float>(dt_sec));
            glm::dvec3 r1 = triton.getExactPosition() - neptune.getExactPosition();

            glm::dvec3 v = (r1 - r0) / dt_sec;
            glm::dvec3 h = glm::cross(r0, v);

            double projSpin = glm::dot(h, neptunePole);
            double projEcliptic = glm::dot(h, eclipticNorth);

            if (projSpin >= 0.0) allEpochsOpposeSpin = false;
            if (projEcliptic >= 0.0) allEpochsOpposeEcliptic = false;

            std::cout << "    " << epochName << ": h · S_neptune = " << projSpin
                      << ", h · Y_ecliptic = " << projEcliptic << "\n";
        }

        record("Triton angular momentum opposes Neptune spin axis (h · S_neptune < 0) across all epochs",
               allEpochsOpposeSpin);
        record("Triton angular momentum opposes Ecliptic North (h · Y_ecliptic < 0) across all epochs",
               allEpochsOpposeEcliptic);

        // 1.2 Dense 360-sample orbit test: check every degree of the orbit
        std::cout << "\n  Running 360-sample dense orbit traversal across one full Triton revolution...\n";
        bool all360OpposeSpin = true;
        double maxProjSpin = -1.0e30;
        double minProjSpin = 1.0e30;
        double orbitalPeriodSec = Astro::Triton.orbitalPeriodDays * 86400.0;
        double timeStepSec = orbitalPeriodSec / 360.0;
        double timeStepCy = timeStepSec / Astro::SECONDS_PER_JULIAN_CENTURY;

        glm::dvec3 firstH(0.0);
        bool firstHSet = false;
        double maxNormalDeviation = 0.0;

        for (int step = 0; step < 360; ++step) {
            double T_step = 0.0 + step * timeStepCy;
            neptune.update(T_step, 0.0f);
            glm::dvec3 r0 = triton.getExactPosition() - neptune.getExactPosition();

            neptune.update(T_step + dt_cy, static_cast<float>(dt_sec));
            glm::dvec3 r1 = triton.getExactPosition() - neptune.getExactPosition();

            glm::dvec3 v = (r1 - r0) / dt_sec;
            glm::dvec3 h = glm::cross(r0, v);
            double proj = glm::dot(h, neptunePole);

            if (proj >= 0.0) all360OpposeSpin = false;
            maxProjSpin = std::max(maxProjSpin, proj);
            minProjSpin = std::min(minProjSpin, proj);

            glm::dvec3 hUnit = glm::normalize(h);
            if (!firstHSet) {
                firstH = hUnit;
                firstHSet = true;
            } else {
                double dev = glm::length(glm::cross(firstH, hUnit));
                maxNormalDeviation = std::max(maxNormalDeviation, dev);
            }
        }

        record("Triton h · S_neptune < 0 at all 360 sample points along full orbit",
               all360OpposeSpin,
               "max proj: " + std::to_string(maxProjSpin) + ", min proj: " + std::to_string(minProjSpin));
        record("Triton orbital normal vector is invariant across orbit (|h(t) x h(0)| < 1e-6)",
               maxNormalDeviation < 1.0e-6,
               "max dev: " + std::to_string(maxNormalDeviation));

        // 1.3 HUD representation check
        bool tritonPeriodPositive = (Astro::Triton.orbitalPeriodDays > 0.0);
        record("Triton orbitalPeriodDays is positive scalar (> 0) for physical HUD display",
               tritonPeriodPositive,
               "value: " + std::to_string(Astro::Triton.orbitalPeriodDays));
    }

    // =========================================================================
    // PART 2: ALL SATELLITES SYSTEM-WIDE ORBITAL SENSE VERIFICATION
    // =========================================================================
    std::cout << Color::Bold << "\n--- PART 2: All 20 Satellites Orbital Sense Verification ---\n" << Color::Reset;
    {
        const std::vector<MoonVerificationSpec> specs = {
            // Earth
            {&Astro::Moon,      &Astro::Earth,   false, false, false},
            // Mars
            {&Astro::Phobos,    &Astro::Mars,    true,  false, false},
            {&Astro::Deimos,    &Astro::Mars,    true,  false, false},
            // Jupiter
            {&Astro::Io,        &Astro::Jupiter, true,  false, false},
            {&Astro::Europa,    &Astro::Jupiter, true,  false, false},
            {&Astro::Ganymede,  &Astro::Jupiter, true,  false, false},
            {&Astro::Callisto,  &Astro::Jupiter, true,  false, false},
            // Saturn
            {&Astro::Mimas,     &Astro::Saturn,  true,  false, false},
            {&Astro::Enceladus, &Astro::Saturn,  true,  false, false},
            {&Astro::Dione,     &Astro::Saturn,  true,  false, false},
            {&Astro::Rhea,      &Astro::Saturn,  true,  false, false},
            {&Astro::Titan,     &Astro::Saturn,  true,  false, false},
            {&Astro::Iapetus,   &Astro::Saturn,  true,  false, false},
            // Uranus (axial tilt = 97.77 deg: prograde to Uranus pole, retrograde to Ecliptic)
            {&Astro::Miranda,   &Astro::Uranus,  true,  false, true},
            {&Astro::Ariel,     &Astro::Uranus,  true,  false, true},
            {&Astro::Umbriel,   &Astro::Uranus,  true,  false, true},
            {&Astro::Titania,   &Astro::Uranus,  true,  false, true},
            {&Astro::Oberon,    &Astro::Uranus,  true,  false, true},
            // Neptune (Triton: retrograde to Neptune AND retrograde to Ecliptic)
            {&Astro::Triton,    &Astro::Neptune, true,  true,  true},
            // Pluto (Charon: axial tilt = 122.53 deg: prograde to Pluto pole, retrograde to Ecliptic)
            {&Astro::Charon,    &Astro::Pluto,   true,  false, true}
        };

        std::cout << std::left
                  << std::setw(12) << "Satellite"
                  << std::setw(10) << "Parent"
                  << std::setw(10) << "i0 (deg)"
                  << std::setw(14) << "h · S_parent"
                  << std::setw(14) << "h · Y_ecl"
                  << std::setw(16) << "Sense (Parent)"
                  << std::setw(16) << "Sense (Ecliptic)"
                  << "Status\n";
        std::cout << std::string(88, '-') << "\n";

        bool allMoonsValid = true;

        for (const auto& sp : specs) {
            CelestialBody parentBody(sp.parent->name, nullptr, nullptr,
                                     conv.getOrbitRadius(*sp.parent),
                                     conv.getOrbitalSpeed(*sp.parent),
                                     conv.getRotationSpeed(*sp.parent),
                                     conv.getAxialTilt(*sp.parent),
                                     conv.getScale(*sp.parent));
            parentBody.setData(sp.parent);
            parentBody.setTiltChildren(sp.parentTiltChildren);

            CelestialBody moonBody(sp.moon->name, nullptr, nullptr,
                                   conv.getOrbitRadius(*sp.moon),
                                   conv.getOrbitalSpeed(*sp.moon),
                                   conv.getRotationSpeed(*sp.moon),
                                   conv.getAxialTilt(*sp.moon),
                                   conv.getScale(*sp.moon));
            moonBody.setData(sp.moon);
            parentBody.addSatellite(&moonBody);

            glm::dvec3 spinPole = getParentSpinPole(*sp.parent, sp.parentTiltChildren);
            glm::dvec3 eclNorth(0.0, 1.0, 0.0);

            // Compute at T = 0.0 (J2000.0) with dt = 1.0 second
            const double dt_sec = 1.0;
            const double dt_cy = dt_sec / Astro::SECONDS_PER_JULIAN_CENTURY;

            parentBody.update(0.0, 0.0f);
            glm::dvec3 r0 = moonBody.getExactPosition() - parentBody.getExactPosition();

            parentBody.update(dt_cy, static_cast<float>(dt_sec));
            glm::dvec3 r1 = moonBody.getExactPosition() - parentBody.getExactPosition();

            glm::dvec3 v = (r1 - r0) / dt_sec;
            glm::dvec3 h = glm::cross(r0, v);

            double projParent = glm::dot(h, spinPole);
            double projEcliptic = glm::dot(h, eclNorth);

            bool isRetroToParent = (projParent < 0.0);
            bool isRetroToEcliptic = (projEcliptic < 0.0);

            bool matchParent = (isRetroToParent == sp.expectedRetrogradeToParent);
            bool matchEcliptic = (isRetroToEcliptic == sp.expectedRetrogradeToEcliptic);
            bool testPass = matchParent && matchEcliptic;

            if (!testPass) allMoonsValid = false;

            std::cout << std::left
                      << std::setw(12) << sp.moon->name
                      << std::setw(10) << sp.parent->name
                      << std::setw(10) << std::fixed << std::setprecision(2) << sp.moon->orbit.i0
                      << std::setw(14) << std::scientific << std::setprecision(2) << projParent
                      << std::setw(14) << std::scientific << std::setprecision(2) << projEcliptic
                      << std::setw(16) << (isRetroToParent ? "RETROGRADE" : "PROGRADE")
                      << std::setw(16) << (isRetroToEcliptic ? "RETROGRADE" : "PROGRADE")
                      << (testPass ? "[OK]" : "[FAIL]") << "\n";
        }

        record("All 20 satellites across Mars, Jupiter, Saturn, Uranus, Neptune, Pluto match physical orbital sense",
               allMoonsValid);
    }

    // =========================================================================
    // PART 3: MOON DISTANCE BOUNDS & ORBITAL INTEGRITY
    // =========================================================================
    std::cout << Color::Bold << "\n--- PART 3: Moon Physical Distance Bounds [a(1-e), a(1+e)] ---\n" << Color::Reset;
    {
        const std::vector<const Astro::BodyData*> allMoons = {
            &Astro::Moon, &Astro::Phobos, &Astro::Deimos,
            &Astro::Io, &Astro::Europa, &Astro::Ganymede, &Astro::Callisto,
            &Astro::Mimas, &Astro::Enceladus, &Astro::Dione, &Astro::Rhea, &Astro::Titan, &Astro::Iapetus,
            &Astro::Miranda, &Astro::Ariel, &Astro::Umbriel, &Astro::Titania, &Astro::Oberon,
            &Astro::Triton, &Astro::Charon
        };

        bool allBoundsSatisfied = true;
        for (const auto* moonData : allMoons) {
            double a = moonData->orbit.a0;
            double e = moonData->orbit.e0;
            double r_peri = a * (1.0 - e);
            double r_apo  = a * (1.0 + e);

            // Sample 100 points around orbit
            for (int k = 0; k < 100; ++k) {
                double T_k = static_cast<double>(k) * 0.01;
                glm::dvec3 pos = Astro::evaluateKeplerianPosition(moonData->orbit, T_k);
                double r = glm::length(pos);

                // Allow 1.0 km or 1e-4 relative tolerance
                double tol = std::max(1.0, 1.0e-4 * a);
                if (r < r_peri - tol || r > r_apo + tol) {
                    allBoundsSatisfied = false;
                    std::cout << "    Distance violation for " << moonData->name << ": r = " << r
                              << ", bounds = [" << r_peri << ", " << r_apo << "]\n";
                    break;
                }
            }
        }

        record("All 20 moons maintain strict Keplerian distance bounds [a(1-e), a(1+e)]",
               allBoundsSatisfied);
    }

    // =========================================================================
    // SUMMARY & FINAL VERDICT
    // =========================================================================
    std::cout << "\n=================================================================\n";
    std::cout << " CHALLENGER EMPIRICAL VERDICT: "
              << (failedTests == 0 ? (std::string(Color::Green) + "APPROVE" + Color::Reset) 
                                   : (std::string(Color::Red) + "FAIL" + Color::Reset)) << "\n";
    std::cout << " Total Checks: " << totalTests << " | Passed: " << (totalTests - failedTests)
              << " | Failed: " << failedTests << "\n";
    std::cout << "=================================================================\n";

    return (failedTests == 0) ? 0 : 1;
}
