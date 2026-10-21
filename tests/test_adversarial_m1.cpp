/**
 * test_adversarial_m1.cpp
 * Adversarial Empirical Stress Test Suite for Milestone M1
 *
 * Authored by: challenger_m1_2 (Empirical Challenger)
 * Role: critic, specialist
 *
 * Stress-tests:
 * 1. Time evolution:
 *    - Long time spans (+/- 10, 50, 100, 1000 simulated years)
 *    - Solver convergence, max residuals, and assert limits across all bodies
 *    - Physical distance bounds [a(1-e), a(1+e)] forward and backward
 *    - Accumulation drift over 10^5 frame updates (dt = 1/60s)
 *    - Time reversibility / hysteresis under negative dt
 * 2. J2000.0 Epoch JPL ground truth comparison
 * 3. Retrograde motion verification:
 *    - Venus axial rotation direction (is omega . North < 0?)
 *    - Triton orbital angular momentum (is h . Neptune_axis < 0? is h . North < 0?)
 * 4. Floating Origin coordinates exactPosition and modelMatrix precision collapse check
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

#include "SolarSystem/AstronomicalData.hpp"
#include "SolarSystem/CelestialBody.hpp"

// ANSI formatting for clear empirical challenge reporting
namespace Color {
    inline const char* Reset   = "\033[0m";
    inline const char* Bold    = "\033[1m";
    inline const char* Red     = "\033[31m";
    inline const char* Green   = "\033[32m";
    inline const char* Yellow  = "\033[33m";
    inline const char* Cyan    = "\033[36m";
}

int main() {
    std::cout << Color::Bold << Color::Cyan
              << "=================================================================\n"
              << "   ADVERSARIAL EMPIRICAL STRESS TEST SUITE — MILESTONE M1        \n"
              << "   Agent: challenger_m1_2 | Role: critic, specialist             \n"
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
            std::cout << "  " << Color::Red << Color::Bold << "[FAIL] " << Color::Reset << Color::Red << name;
            if (!detail.empty()) std::cout << " -> " << detail;
            std::cout << Color::Reset << "\n";
        }
    };

    const std::vector<const Astro::BodyData*> allBodies = {
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

    // =========================================================================
    // SECTION 1: TIME EVOLUTION STRESS TESTING
    // =========================================================================
    std::cout << Color::Bold << "\n--- SECTION 1: Time Evolution Stress Testing ---\n" << Color::Reset;

    // 1.1 Long time spans: +/- 10, +/- 50, +/- 100, +/- 1000 years
    {
        const std::vector<double> testCenturies = {
            -10.0, -1.0, -0.5, -0.1, 0.0, 0.1, 0.5, 1.0, 10.0
        };

        bool allResidualsOk = true;
        bool allBoundsOk = true;
        double globalMaxResidual = 0.0;

        for (double T : testCenturies) {
            for (const Astro::BodyData* b : allBodies) {
                const auto& el = b->orbit;
                if (el.a0 <= 0.0) continue;

                // Evaluate position and basis
                Astro::KeplerianBasis basis = Astro::evaluateKeplerianBasis(el, T);
                // M_rad was wrapped to [-pi, pi] in solveKepler
                double wrappedM = std::fmod(basis.M_rad, glm::two_pi<double>());
                if (wrappedM > glm::pi<double>()) wrappedM -= glm::two_pi<double>();
                if (wrappedM < -glm::pi<double>()) wrappedM += glm::two_pi<double>();
                double trueRes = std::abs(basis.E_rad - basis.e * std::sin(basis.E_rad) - wrappedM);
                if (trueRes > glm::pi<double>()) trueRes = std::abs(trueRes - glm::two_pi<double>());

                if (trueRes > globalMaxResidual) globalMaxResidual = trueRes;
                if (trueRes > 1.0e-11) {
                    allResidualsOk = false;
                }

                // Check distance within physical bounds for +/- 100 yr
                if (std::abs(T) <= 1.0) {
                    double r_km = glm::length(Astro::evaluateKeplerianPosition(el, T));
                    double a_curr = el.isDistanceKm ? (el.a0 + el.aDot * T) : ((el.a0 + el.aDot * T) * Astro::AU_KM);
                    double e_curr = std::clamp(el.e0 + el.eDot * T, 0.0, 0.9999);
                    double rPeri = a_curr * (1.0 - e_curr);
                    double rAp   = a_curr * (1.0 + e_curr);
                    double tol = 0.01; // 10 meters in km
                    if (r_km < rPeri - tol || r_km > rAp + tol) {
                        allBoundsOk = false;
                        std::cout << "    Bound breach: " << b->name << " at T=" << T 
                                  << " r=" << r_km << " [rPeri=" << rPeri << ", rAp=" << rAp << "]\n";
                    }
                }
            }
        }

        record("Kepler solver residual < 1e-11 across +/-1000 simulated years", allResidualsOk,
               "max residual observed: " + std::to_string(globalMaxResidual));
        record("Physical distance bounds [a(1-e), a(1+e)] across +/-100 simulated years", allBoundsOk);
    }

    // 1.2 Simulation step accumulation drift (100,000 steps at dt = 1/60 s)
    {
        double exactSeconds = 0.0;
        double accumulatedFloatSeconds = 0.0;
        const float dtFloat = 1.0f / 60.0f;
        const int steps = 100000;

        for (int i = 0; i < steps; ++i) {
            exactSeconds += static_cast<double>(dtFloat);
            accumulatedFloatSeconds += static_cast<double>(dtFloat); // in SolarSystem::update: simTimeSeconds += static_cast<double>(deltaTime)
        }

        double expectedTotal = static_cast<double>(steps) * static_cast<double>(dtFloat);
        double driftSec = std::abs(accumulatedFloatSeconds - expectedTotal);
        double driftT = std::abs(Astro::unixToJulianCenturies(accumulatedFloatSeconds) - Astro::unixToJulianCenturies(expectedTotal));

        record("100,000 frame accumulation drift (simTimeSeconds += dt)", driftSec < 1.0e-8,
               "drift: " + std::to_string(driftSec) + " sec, " + std::to_string(driftT) + " cy");
    }

    // 1.3 Backward time stepping & reversibility
    {
        Astro::ScaleConverter conv;
        CelestialBody earth("Earth", nullptr, nullptr, 
                            conv.getOrbitRadius(Astro::Earth), 
                            conv.getOrbitalSpeed(Astro::Earth), 
                            conv.getRotationSpeed(Astro::Earth), 
                            conv.getAxialTilt(Astro::Earth), 
                            conv.getScale(Astro::Earth));
        earth.setData(&Astro::Earth);

        double t0 = 0.25; // 25 years after J2000
        float dt = 0.1f;
        earth.update(t0, 0.0f);
        glm::dvec3 pos0 = earth.getExactPosition();

        // Advance 1000 steps forward
        double t = t0;
        for (int i = 0; i < 1000; ++i) {
            t += static_cast<double>(dt) / Astro::SECONDS_PER_JULIAN_CENTURY;
            earth.update(t, dt);
        }
        // Advance 1000 steps backward
        for (int i = 0; i < 1000; ++i) {
            t -= static_cast<double>(dt) / Astro::SECONDS_PER_JULIAN_CENTURY;
            earth.update(t, -dt);
        }

        glm::dvec3 posBack = earth.getExactPosition();
        double returnDistError = glm::length(posBack - pos0);

        record("Time reversibility / zero hysteresis after 1000 forward/backward steps", 
               returnDistError < 1.0e-6, "pos diff: " + std::to_string(returnDistError) + " km");
    }

    // =========================================================================
    // SECTION 2: J2000.0 EPOCH POSITION MATCHING JPL GROUND TRUTH
    // =========================================================================
    std::cout << Color::Bold << "\n--- SECTION 2: J2000.0 Epoch Position Matching JPL Ground Truth ---\n" << Color::Reset;
    {
        // Standish (1998) Table 1 & Table 2 ground truth positions at J2000.0 (in AU and ecliptic frame)
        // Verified against JPL Horizons osculating vectors at J2000.0 (JD 2451545.0)
        struct PlanetBenchmark {
            const Astro::BodyData* body;
            double expectedDistanceAU;
            double tolAU;
        };

        const std::vector<PlanetBenchmark> benchmarks = {
            {&Astro::Mercury, 0.466475, 1.0e-4},
            {&Astro::Venus,   0.720229, 1.0e-4},
            {&Astro::Earth,   0.983307, 1.0e-4}, // Earth near perihelion on Jan 1.5
            {&Astro::Mars,    1.391159, 1.0e-4},
            {&Astro::Jupiter, 4.967306, 1.0e-4},
            {&Astro::Saturn,  9.172322, 1.0e-4},
            {&Astro::Uranus,  19.921688, 1.0e-4},
            {&Astro::Neptune, 30.117391, 1.0e-4},
            {&Astro::Pluto,   30.230343, 1.0e-4}
        };

        bool allGroundTruthOk = true;
        for (const auto& bm : benchmarks) {
            glm::dvec3 pos_km = Astro::evaluateKeplerianPosition(bm.body->orbit, 0.0);
            double distAU = glm::length(pos_km) / Astro::AU_KM;
            double diff = std::abs(distAU - bm.expectedDistanceAU);
            if (diff > bm.tolAU) {
                allGroundTruthOk = false;
                std::cout << "    JPL mismatch: " << bm.body->name << " actual=" << distAU 
                          << " expected=" << bm.expectedDistanceAU << " diff=" << diff << "\n";
            }
        }
        record("All 9 major planets match JPL Standish J2000.0 distance within 1e-4 AU", allGroundTruthOk);

        // Check Earth's ecliptic plane z-coordinate at J2000.0
        // Standish (1998) specifies Earth i0 = -0.00001531 deg (-0.055 arcsec), producing an exact physical offset
        // of z_ecl = r * sin(i) * sin(w+v) ~= 1.47e8 km * sin(-0.00001531 deg) = -38.66 km (< 0.0000003 AU).
        glm::dvec3 earthPos = Astro::evaluateKeplerianPosition(Astro::Earth.orbit, 0.0);
        // Note: in world frame, Y_world = Z_ecliptic
        double earthZ_diff = std::abs(earthPos.y - (-38.663464));
        record("Earth orbital plane ecliptic z-distance matches Standish i0 offset within 1 km", earthZ_diff < 1.0,
               "y_world: " + std::to_string(earthPos.y) + " km (theoretical: -38.66 km, offset < 0.0000003 AU)");
    }

    // =========================================================================
    // SECTION 3: RETROGRADE MOTION BODIES (Venus rotation, Triton orbit)
    // =========================================================================
    std::cout << Color::Bold << "\n--- SECTION 3: Retrograde Motion Bodies (Venus rotation, Triton orbit) ---\n" << Color::Reset;

    // 3.1 Venus rotation
    {
        Astro::ScaleConverter conv;
        CelestialBody venus("Venus", nullptr, nullptr, 
                            conv.getOrbitRadius(Astro::Venus), 
                            conv.getOrbitalSpeed(Astro::Venus), 
                            conv.getRotationSpeed(Astro::Venus), 
                            conv.getAxialTilt(Astro::Venus), 
                            conv.getScale(Astro::Venus));
        venus.setData(&Astro::Venus);

        CelestialBody earth("Earth", nullptr, nullptr, 
                            conv.getOrbitRadius(Astro::Earth), 
                            conv.getOrbitalSpeed(Astro::Earth), 
                            conv.getRotationSpeed(Astro::Earth), 
                            conv.getAxialTilt(Astro::Earth), 
                            conv.getScale(Astro::Earth));
        earth.setData(&Astro::Earth);

        // Update both by dt = 1.0s
        earth.update(0.0, 0.0f);
        glm::mat4 earthModel0 = earth.getModelMatrix();
        earth.update(0.0, 1.0f);
        glm::mat4 earthModel1 = earth.getModelMatrix();

        venus.update(0.0, 0.0f);
        glm::mat4 venusModel0 = venus.getModelMatrix();
        venus.update(0.0, 1.0f);
        glm::mat4 venusModel1 = venus.getModelMatrix();

        // Check motion of a point on the equator: local p = (1, 0, 0)
        glm::vec3 p_local(1.0f, 0.0f, 0.0f);
        glm::vec3 earthP0 = glm::vec3(earthModel0 * glm::vec4(p_local, 1.0f)) - earth.getWorldPosition();
        glm::vec3 earthP1 = glm::vec3(earthModel1 * glm::vec4(p_local, 1.0f)) - earth.getWorldPosition();
        glm::vec3 earthV = earthP1 - earthP0;
        glm::vec3 earthSpin = glm::cross(earthP0, earthV);

        glm::vec3 venusP0 = glm::vec3(venusModel0 * glm::vec4(p_local, 1.0f)) - venus.getWorldPosition();
        glm::vec3 venusP1 = glm::vec3(venusModel1 * glm::vec4(p_local, 1.0f)) - venus.getWorldPosition();
        glm::vec3 venusV = venusP1 - venusP0;
        glm::vec3 venusSpin = glm::cross(venusP0, venusV);

        // World ecliptic north is +Y (0, 1, 0)
        float earthSpinProj = glm::dot(earthSpin, glm::vec3(0.0f, 1.0f, 0.0f));
        float venusSpinProj = glm::dot(venusSpin, glm::vec3(0.0f, 1.0f, 0.0f));

        std::cout << "    Earth spin projection on Ecliptic North (+Y): " << earthSpinProj 
                  << (earthSpinProj > 0 ? " (PROGRADE)" : " (RETROGRADE)") << "\n";
        std::cout << "    Venus spin projection on Ecliptic North (+Y): " << venusSpinProj 
                  << (venusSpinProj < 0 ? " (RETROGRADE)" : " (PROGRADE)") << "\n";
        std::cout << "    Venus data: rotationPeriodDays = " << Astro::Venus.rotationPeriodDays 
                  << ", axialTiltDegrees = " << Astro::Venus.axialTiltDegrees << "\n";
        std::cout << "    ScaleConverter::getRotationSpeed(Venus) = " << conv.getRotationSpeed(Astro::Venus) 
                  << ", getAxialTilt(Venus) = " << conv.getAxialTilt(Astro::Venus) << " rad (" 
                  << glm::degrees(conv.getAxialTilt(Astro::Venus)) << " deg)\n";

        bool venusRetrograde = (venusSpinProj < 0.0f);
        record("Venus retrograde rotation (spin vector points South: dot(omega, North) < 0)", 
               venusRetrograde, 
               "venusSpinProj = " + std::to_string(venusSpinProj));

        // Uranus & Pluto retrograde check
        CelestialBody uranus("Uranus", nullptr, nullptr, 
                             conv.getOrbitRadius(Astro::Uranus), conv.getOrbitalSpeed(Astro::Uranus), 
                             conv.getRotationSpeed(Astro::Uranus), conv.getAxialTilt(Astro::Uranus), 
                             conv.getScale(Astro::Uranus));
        uranus.setData(&Astro::Uranus);
        uranus.update(0.0, 0.0f);
        uranus.applyOrigin(uranus.getExactPosition());
        glm::mat4 u0 = uranus.getModelMatrix();
        uranus.update(0.0, 1.0f);
        uranus.applyOrigin(uranus.getExactPosition());
        glm::mat4 u1 = uranus.getModelMatrix();
        glm::vec3 uP0 = glm::vec3(u0 * glm::vec4(p_local, 1.0f)) - uranus.getWorldPosition();
        glm::vec3 uP1 = glm::vec3(u1 * glm::vec4(p_local, 1.0f)) - uranus.getWorldPosition();
        float uranusSpinProj = glm::dot(glm::cross(uP0, uP1 - uP0), glm::vec3(0.0f, 1.0f, 0.0f));
        std::cout << "    Uranus spin projection on Ecliptic North (+Y): " << uranusSpinProj 
                  << (uranusSpinProj < 0 ? " (RETROGRADE)" : " (PROGRADE)") << "\n";

        CelestialBody plutoBody("Pluto", nullptr, nullptr, 
                                conv.getOrbitRadius(Astro::Pluto), conv.getOrbitalSpeed(Astro::Pluto), 
                                conv.getRotationSpeed(Astro::Pluto), conv.getAxialTilt(Astro::Pluto), 
                                conv.getScale(Astro::Pluto));
        plutoBody.setData(&Astro::Pluto);
        plutoBody.update(0.0, 0.0f);
        plutoBody.applyOrigin(plutoBody.getExactPosition());
        glm::mat4 pl0 = plutoBody.getModelMatrix();
        plutoBody.update(0.0, 1.0f);
        plutoBody.applyOrigin(plutoBody.getExactPosition());
        glm::mat4 pl1 = plutoBody.getModelMatrix();
        glm::vec3 plP0 = glm::vec3(pl0 * glm::vec4(p_local, 1.0f)) - plutoBody.getWorldPosition();
        glm::vec3 plP1 = glm::vec3(pl1 * glm::vec4(p_local, 1.0f)) - plutoBody.getWorldPosition();
        float plutoSpinProj = glm::dot(glm::cross(plP0, plP1 - plP0), glm::vec3(0.0f, 1.0f, 0.0f));
        std::cout << "    Pluto spin projection on Ecliptic North (+Y): " << plutoSpinProj 
                  << (plutoSpinProj < 0 ? " (RETROGRADE)" : " (PROGRADE)") << "\n";
    }

    // 3.2 Triton orbit
    {
        Astro::ScaleConverter conv;
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

        // Compare with a standard prograde moon: Jupiter's Io
        CelestialBody jupiter("Jupiter", nullptr, nullptr, 
                              conv.getOrbitRadius(Astro::Jupiter), 
                              conv.getOrbitalSpeed(Astro::Jupiter), 
                              conv.getRotationSpeed(Astro::Jupiter), 
                              conv.getAxialTilt(Astro::Jupiter), 
                              conv.getScale(Astro::Jupiter));
        jupiter.setData(&Astro::Jupiter);

        CelestialBody io("Io", nullptr, nullptr, 
                         conv.getOrbitRadius(Astro::Io), 
                         conv.getOrbitalSpeed(Astro::Io), 
                         conv.getRotationSpeed(Astro::Io), 
                         conv.getAxialTilt(Astro::Io), 
                         conv.getScale(Astro::Io));
        io.setData(&Astro::Io);
        jupiter.addSatellite(&io);

        // Update at T = 0.0 and T = 1e-5 cy
        double T0 = 0.0;
        double dT = 1.0e-5; // ~31.5 seconds

        neptune.update(T0, 0.0f);
        glm::dvec3 tritonRel0 = triton.getExactPosition() - neptune.getExactPosition();
        neptune.update(T0 + dT, 1.0f);
        glm::dvec3 tritonRel1 = triton.getExactPosition() - neptune.getExactPosition();
        glm::dvec3 tritonV = (tritonRel1 - tritonRel0) / dT;
        glm::dvec3 tritonH = glm::cross(tritonRel0, tritonV);

        jupiter.update(T0, 0.0f);
        glm::dvec3 ioRel0 = io.getExactPosition() - jupiter.getExactPosition();
        jupiter.update(T0 + dT, 1.0f);
        glm::dvec3 ioRel1 = io.getExactPosition() - jupiter.getExactPosition();
        glm::dvec3 ioV = (ioRel1 - ioRel0) / dT;
        glm::dvec3 ioH = glm::cross(ioRel0, ioV);

        // Neptune's spin axis / local North pole in world frame
        // Neptune axial tilt is 28.32 degrees around Z in CelestialBody update
        glm::dmat4 neptuneRot = glm::rotate(glm::dmat4(1.0), static_cast<double>(conv.getAxialTilt(Astro::Neptune)), glm::dvec3(0.0, 0.0, 1.0));
        glm::dvec3 neptunePole = glm::dvec3(neptuneRot * glm::dvec4(0.0, 1.0, 0.0, 0.0));

        double tritonProjNeptune = glm::dot(tritonH, neptunePole);
        double tritonProjEcliptic = glm::dot(tritonH, glm::dvec3(0.0, 1.0, 0.0));
        double ioProjEcliptic = glm::dot(ioH, glm::dvec3(0.0, 1.0, 0.0));

        std::cout << "    Io angular momentum dot Ecliptic North (+Y): " << ioProjEcliptic 
                  << (ioProjEcliptic > 0 ? " (PROGRADE)" : " (RETROGRADE)") << "\n";
        std::cout << "    Triton angular momentum dot Neptune Spin Pole: " << tritonProjNeptune 
                  << (tritonProjNeptune < 0 ? " (RETROGRADE)" : " (PROGRADE)") << "\n";
        std::cout << "    Triton angular momentum dot Ecliptic North: " << tritonProjEcliptic 
                  << (tritonProjEcliptic < 0 ? " (RETROGRADE)" : " (PROGRADE)") << "\n";
        std::cout << "    Triton orbit elements: i0 = " << Astro::Triton.orbit.i0 
                  << ", LDot = " << Astro::Triton.orbit.LDot << "\n";

        bool tritonRetrograde = (tritonProjNeptune < 0.0);
        record("Triton retrograde orbit (angular momentum opposes Neptune spin: dot(h, S) < 0)", 
               tritonRetrograde, 
               "tritonProjNeptune = " + std::to_string(tritonProjNeptune));

        // Hypothesis Verification / Proposed Mitigation Check:
        // If LDot is positive (+2237366.0) while keeping i0 = 156.88 deg:
        Astro::BodyData tritonFixed = Astro::Triton;
        tritonFixed.orbit.LDot = std::abs(Astro::Triton.orbit.LDot); // standard Keplerian positive mean motion
        CelestialBody tritonFixedNode("TritonFixed", nullptr, nullptr, 
                                      conv.getOrbitRadius(tritonFixed), conv.getOrbitalSpeed(tritonFixed), 
                                      conv.getRotationSpeed(tritonFixed), conv.getAxialTilt(tritonFixed), 
                                      conv.getScale(tritonFixed));
        tritonFixedNode.setData(&tritonFixed);
        neptune.addSatellite(&tritonFixedNode);

        neptune.update(T0, 0.0f);
        glm::dvec3 tritonFixRel0 = tritonFixedNode.getExactPosition() - neptune.getExactPosition();
        neptune.update(T0 + dT, 1.0f);
        glm::dvec3 tritonFixRel1 = tritonFixedNode.getExactPosition() - neptune.getExactPosition();
        glm::dvec3 tritonFixV = (tritonFixRel1 - tritonFixRel0) / dT;
        glm::dvec3 tritonFixH = glm::cross(tritonFixRel0, tritonFixV);
        (void)tritonFixH;
        double tritonFixProj = glm::dot(glm::cross(tritonFixRel0, tritonFixV), neptunePole);
        std::cout << "    [Hypothesis Check] Triton with LDot > 0: dot(h, S) = " << tritonFixProj 
                  << (tritonFixProj < 0 ? " -> PROVEN RETROGRADE!" : " -> STILL PROGRADE") << "\n";

        // Hypothesis Verification for Venus:
        // If rotationSpeed > 0 around axialTilt = 177.36 deg:
        CelestialBody venusFixedNode("VenusFixed", nullptr, nullptr, 
                                     conv.getOrbitRadius(Astro::Venus), conv.getOrbitalSpeed(Astro::Venus), 
                                     std::abs(conv.getRotationSpeed(Astro::Venus)), conv.getAxialTilt(Astro::Venus), 
                                     conv.getScale(Astro::Venus));
        venusFixedNode.setData(&Astro::Venus);
        venusFixedNode.update(0.0, 0.0f);
        glm::mat4 vf0 = venusFixedNode.getModelMatrix();
        venusFixedNode.update(0.0, 1.0f);
        glm::mat4 vf1 = venusFixedNode.getModelMatrix();
        glm::vec3 p_local(1.0f, 0.0f, 0.0f);
        glm::vec3 vfP0 = glm::vec3(vf0 * glm::vec4(p_local, 1.0f)) - venusFixedNode.getWorldPosition();
        glm::vec3 vfP1 = glm::vec3(vf1 * glm::vec4(p_local, 1.0f)) - venusFixedNode.getWorldPosition();
        float vfSpinProj = glm::dot(glm::cross(vfP0, vfP1 - vfP0), glm::vec3(0.0f, 1.0f, 0.0f));
        std::cout << "    [Hypothesis Check] Venus with positive spin: dot(omega, North) = " << vfSpinProj 
                  << (vfSpinProj < 0 ? " -> PROVEN RETROGRADE!" : " -> STILL PROGRADE") << "\n";
    }

    // =========================================================================
    // SECTION 4: FLOATING ORIGIN PRECISION & MODELMATRIX STABILITY
    // =========================================================================
    std::cout << Color::Bold << "\n--- SECTION 4: Floating Origin Precision & ModelMatrix Stability ---\n" << Color::Reset;
    {
        Astro::ScaleConverter conv;
        // Construct Sun -> Pluto -> Charon hierarchy
        CelestialBody sun("Sun", nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, conv.getScale(Astro::Sun));
        sun.setData(&Astro::Sun);

        CelestialBody pluto("Pluto", nullptr, nullptr, 
                            conv.getOrbitRadius(Astro::Pluto), 
                            conv.getOrbitalSpeed(Astro::Pluto), 
                            conv.getRotationSpeed(Astro::Pluto), 
                            conv.getAxialTilt(Astro::Pluto), 
                            conv.getScale(Astro::Pluto));
        pluto.setData(&Astro::Pluto);
        sun.addSatellite(&pluto);

        CelestialBody charon("Charon", nullptr, nullptr, 
                             conv.getOrbitRadius(Astro::Charon), 
                             conv.getOrbitalSpeed(Astro::Charon), 
                             conv.getRotationSpeed(Astro::Charon), 
                             conv.getAxialTilt(Astro::Charon), 
                             conv.getScale(Astro::Charon));
        charon.setData(&Astro::Charon);
        pluto.addSatellite(&charon);

        // Also add distant Eris (a = 67.8 AU ~ 1.01e10 km, perihelion 37.9 AU, aphelion 97.7 AU)
        CelestialBody eris("Eris", nullptr, nullptr, 
                           conv.getOrbitRadius(Astro::Eris), 
                           conv.getOrbitalSpeed(Astro::Eris), 
                           conv.getRotationSpeed(Astro::Eris), 
                           conv.getAxialTilt(Astro::Eris), 
                           conv.getScale(Astro::Eris));
        eris.setData(&Astro::Eris);
        sun.addSatellite(&eris);

        // Update hierarchy at J2000.0
        sun.update(0.0, 0.0f);

        // Check 4.1: exactPosition preserves double precision at 10^10 km
        glm::dvec3 erisExact = eris.getExactPosition();
        double erisDist = glm::length(erisExact);
        record("Eris exactPosition preserved at 10^10 km (> 1.4e10 km at J2000)", erisDist > 1.4e10,
               "dist = " + std::to_string(erisDist) + " km");

        // Check 4.2: Floating Origin centering on Pluto
        sun.applyOrigin(pluto.getExactPosition());
        glm::vec3 plutoWorldPos = pluto.getWorldPosition();
        double plutoWorldDist = glm::length(plutoWorldPos);
        record("Pluto centered exactly at world (0,0,0) under floating origin", plutoWorldDist == 0.0f,
               "dist = " + std::to_string(plutoWorldDist));

        // Check 4.3: Charon world position precision when focused on Pluto
        glm::vec3 charonWorldPos = charon.getWorldPosition();
        double charonDist = glm::length(charonWorldPos);
        // Charon distance to Pluto is ~ 19591 km
        double charonExactDist = glm::length(charon.getExactPosition() - pluto.getExactPosition());
        double floatErrorKm = std::abs(static_cast<double>(charonDist) - charonExactDist);
        std::cout << "    Charon exact dist to Pluto: " << std::fixed << std::setprecision(3) << charonExactDist << " km\n";
        std::cout << "    Charon float32 world dist:  " << std::fixed << std::setprecision(3) << charonDist << " km\n";
        std::cout << "    Discrepancy:                " << std::scientific << floatErrorKm << " km (" 
                  << floatErrorKm * 1000.0 << " meters)\n";
        record("Charon float32 quantization error < 5 meters when origin = Pluto", floatErrorKm < 0.005,
               "error = " + std::to_string(floatErrorKm * 1000.0) + " meters");

        // Compare with what error would be if origin remained at Sun (no floating origin)
        double unshiftedQuantization = erisDist * std::pow(2.0, -24);
        std::cout << "    Sun-origin float32 quantization at Eris distance: ~" 
                  << std::fixed << std::setprecision(1) << unshiftedQuantization << " km (precision collapse!)\n";

        // Check 4.4: modelMatrix condition and orthogonality
        glm::mat4 plutoModel = pluto.getModelMatrix();
        // Check that translation matches worldPosition
        glm::vec3 plutoTrans(plutoModel[3]);
        record("Pluto modelMatrix translation strictly matches worldPosition", plutoTrans == plutoWorldPos);

        // Check upper 3x3 rotation block is orthonormal: R^T * R == I
        float scale = pluto.getScale();
        glm::mat3 R = glm::mat3(plutoModel) / scale;
        glm::mat3 I = glm::transpose(R) * R;
        float diffI = 0.0f;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                float expected = (r == c) ? 1.0f : 0.0f;
                diffI = std::max(diffI, std::abs(I[r][c] - expected));
            }
        }
        record("Pluto modelMatrix orientation block strictly orthonormal (|R^T*R - I| < 1e-6)", diffI < 1.0e-6,
               "max diff: " + std::to_string(diffI));
    }

    std::cout << Color::Bold << Color::Cyan
              << "\n=================================================================\n"
              << "   ADVERSARIAL STRESS TEST SUMMARY: "
              << (failedTests == 0 ? Color::Green : Color::Red)
              << (totalTests - failedTests) << "/" << totalTests << " PASSED"
              << (failedTests == 0 ? " — ALL CHECKS APPROVED!" : " — FAILURES DETECTED!")
              << Color::Bold << Color::Cyan << "\n"
              << "=================================================================\n"
              << Color::Reset << std::endl;

    return failedTests;
}
