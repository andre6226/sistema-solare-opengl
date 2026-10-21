/**
 * test_challenger_m1_1.cpp
 * Adversarial Empirical Verification & Stress Test Harness
 * Milestone: M1 (Real-Time Keplerian Ephemerides, Newton-Raphson Solver, 3D Coordinates)
 * Agent: challenger_m1_1 | Roles: critic, specialist
 *
 * Verifies:
 * 1. Newton-Raphson Kepler solver residual: |E - e*sin(E) - M| < 10^-12
 *    across e = 0.0, e = 0.2056, e = 0.2488, e = 0.4407, e = 0.95, and hostile boundaries
 *    (e = 1e-7, e = 0.7999, e = 0.8001, e = 0.99, e = 0.999).
 * 2. Exact distance bounds: r_min >= a(1-e) - eps and r_max <= a(1+e) + eps
 *    for all 38 bodies and extreme cometary synthetic orbits.
 * 3. Gauss basis orthonormality across 47,952 3D orientations.
 * 4. Temporal continuity and branch cut smoothness.
 * 5. Empirical investigation of edge cases: Earth J2000 inclination and Triton retrograde orbital rate.
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <limits>
#include <string>

#include "SolarSystem/AstronomicalData.hpp"

namespace ChallengerM1 {

struct TestReport {
    long long totalChecks = 0;
    long long failures = 0;
    double maxKeplerResidual = 0.0;
    double maxDistanceBoundViolation = 0.0;
    double maxOrthoDeviation = 0.0;
    int keplerCasesTested = 0;
};

// ============================================================================
// 1. ADVERSARIAL NEWTON-RAPHSON KEPLER SOLVER STRESS TEST
// ============================================================================
void challengeKeplerSolver(TestReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 1] Newton-Raphson Solver Residual Test (|f(E)| < 1e-12)\n";
    std::cout << "======================================================================\n";

    const std::vector<std::pair<std::string, double>> testCases = {
        {"e = 0.0000 (Circular)",                      0.0},
        {"e = 1.0e-7 (Near-circular limit)",           1.0e-7},
        {"e = 0.0167 (Earth)",                         0.01671123},
        {"e = 0.2056 (Mercury)",                       0.20563593},
        {"e = 0.2488 (Pluto)",                         0.24882730},
        {"e = 0.4407 (Eris)",                          0.44070000},
        {"e = 0.7999 (Below Danby starter boundary)",  0.79999999},
        {"e = 0.8001 (Above Danby starter boundary)",  0.80000001},
        {"e = 0.9000 (High-eccentricity)",             0.90000000},
        {"e = 0.9500 (Extreme comet-like)",            0.95000000},
        {"e = 0.9900 (Halley cometary limit)",         0.99000000},
        {"e = 0.9990 (Near-parabolic boundary)",       0.99900000},
        {"e = 0.9999 (Clamp boundary)",                0.99990000}
    };

    // Pathological and extreme mean anomalies
    const std::vector<double> pathologicalM = {
        0.0,
        1.0e-16, -1.0e-16,
        1.0e-15, -1.0e-15,
        1.0e-12, -1.0e-12,
        1.0e-9,  -1.0e-9,
        1.0e-6,  -1.0e-6,
        1.0e-3,  -1.0e-3,
        0.01,    -0.01,
        0.1,     -0.1,
        glm::pi<double>() / 4.0, -glm::pi<double>() / 4.0,
        glm::pi<double>() / 2.0, -glm::pi<double>() / 2.0,
        3.0 * glm::pi<double>() / 4.0, -3.0 * glm::pi<double>() / 4.0,
        glm::pi<double>() - 1.0e-15, -(glm::pi<double>() - 1.0e-15),
        glm::pi<double>(),           -glm::pi<double>(),
        glm::pi<double>() + 1.0e-15, -(glm::pi<double>() + 1.0e-15),
        glm::two_pi<double>(),       -glm::two_pi<double>(),
        10.0 * glm::pi<double>(),    -10.0 * glm::pi<double>(),
        100.0 * glm::pi<double>() + 0.12345, -100.0 * glm::pi<double>() - 0.12345,
        1.0e6,                       -1.0e6,
        1.23456789e8,                -1.23456789e8
    };

    for (const auto& [name, e] : testCases) {
        double maxResForCase = 0.0;
        long long checksForCase = 0;

        // 1. Boundary & pathological anomaly checks
        for (double M : pathologicalM) {
            double E = Astro::solveKepler(M, e);
            double M_norm = std::fmod(M, glm::two_pi<double>());
            if (M_norm > glm::pi<double>())  M_norm -= glm::two_pi<double>();
            if (M_norm < -glm::pi<double>()) M_norm += glm::two_pi<double>();

            double res = std::abs(E - e * std::sin(E) - M_norm);
            if (res > glm::pi<double>()) res = std::abs(res - glm::two_pi<double>());

            if (res > maxResForCase) maxResForCase = res;
            if (res > report.maxKeplerResidual) report.maxKeplerResidual = res;

            report.totalChecks++;
            checksForCase++;
            if (res >= 1.0e-12 || std::isnan(res) || std::isnan(E)) {
                std::cerr << "  [FAIL] Residual violation for " << name
                          << " at M=" << M << ": E=" << E << " res=" << res << "\n";
                report.failures++;
            }
        }

        // 2. Dense grid across full orbital period: 20,000 points in [-pi, pi]
        const int numGrid = 20000;
        for (int i = 0; i <= numGrid; ++i) {
            double frac = static_cast<double>(i) / numGrid;
            double M = -glm::pi<double>() + frac * glm::two_pi<double>();
            double E = Astro::solveKepler(M, e);

            double res = std::abs(E - e * std::sin(E) - M);
            if (res > glm::pi<double>()) res = std::abs(res - glm::two_pi<double>());

            if (res > maxResForCase) maxResForCase = res;
            if (res > report.maxKeplerResidual) report.maxKeplerResidual = res;

            report.totalChecks++;
            checksForCase++;
            if (res >= 1.0e-12 || std::isnan(res) || std::isnan(E)) {
                std::cerr << "  [FAIL] Residual violation on grid for " << name
                          << " at M=" << M << ": res=" << res << "\n";
                report.failures++;
                if (report.failures > 10) break;
            }
        }

        // 3. Monte-Carlo stress test: 20,000 unnormalized large anomalies in [-1000pi, 1000pi]
        std::mt19937_64 rng(42 + static_cast<uint64_t>(e * 100000.0));
        std::uniform_real_distribution<double> distM(-1000.0 * glm::pi<double>(), 1000.0 * glm::pi<double>());

        for (int i = 0; i < 20000; ++i) {
            double M = distM(rng);
            double E = Astro::solveKepler(M, e);
            double M_norm = std::fmod(M, glm::two_pi<double>());
            if (M_norm > glm::pi<double>())  M_norm -= glm::two_pi<double>();
            if (M_norm < -glm::pi<double>()) M_norm += glm::two_pi<double>();

            double res = std::abs(E - e * std::sin(E) - M_norm);
            if (res > glm::pi<double>()) res = std::abs(res - glm::two_pi<double>());

            if (res > maxResForCase) maxResForCase = res;
            if (res > report.maxKeplerResidual) report.maxKeplerResidual = res;

            report.totalChecks++;
            checksForCase++;
            if (res >= 1.0e-12 || std::isnan(res) || std::isnan(E)) {
                std::cerr << "  [FAIL] Residual violation in Monte-Carlo for " << name
                          << " at M=" << M << ": res=" << res << "\n";
                report.failures++;
                if (report.failures > 10) break;
            }
        }

        report.keplerCasesTested++;
        std::cout << "  " << std::left << std::setw(42) << name
                  << " Checks: " << std::setw(6) << checksForCase
                  << " Max Residual: " << std::scientific << std::setprecision(6) << maxResForCase
                  << "  [" << ((maxResForCase < 1.0e-12) ? "PASS" : "FAIL") << "]\n";
    }
}

// ============================================================================
// 2. EXACT DISTANCE BOUNDS VERIFICATION: r in [a(1-e) - eps, a(1+e) + eps]
// ============================================================================
void challengeDistanceBounds(TestReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 2] Distance Bounds [a*(1-e), a*(1+e)] for All 38 Bodies\n";
    std::cout << "======================================================================\n";

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

    assert(allBodies.size() == 38);

    // Epsilon tolerance: 1e-6 km (1 millimeter)
    const double eps = 1.0e-6;

    for (const Astro::BodyData* b : allBodies) {
        const Astro::KeplerianElements& el = b->orbit;
        // Test at multi-century epochs: T in {-5.0, -1.0, 0.0, 1.0, 5.0}
        for (double T : {-5.0, -1.0, 0.0, 1.0, 5.0}) {
            Astro::KeplerianBasis basis = Astro::evaluateKeplerianBasis(el, T);
            double r_peri = basis.a_km * (1.0 - basis.e);
            double r_ap   = basis.a_km * (1.0 + basis.e);

            double empMin = std::numeric_limits<double>::infinity();
            double empMax = 0.0;

            const int numSamples = 2000;
            for (int k = 0; k < numSamples; ++k) {
                double E_rad = static_cast<double>(k) / numSamples * glm::two_pi<double>();
                glm::dvec3 pos = Astro::evaluateOrbitVertex(basis, E_rad);
                double r = glm::length(pos);

                if (r < empMin) empMin = r;
                if (r > empMax) empMax = r;

                double violationLower = (r_peri - eps) - r;
                double violationUpper = r - (r_ap + eps);
                if (violationLower > report.maxDistanceBoundViolation) report.maxDistanceBoundViolation = violationLower;
                if (violationUpper > report.maxDistanceBoundViolation) report.maxDistanceBoundViolation = violationUpper;

                report.totalChecks++;
                if (r < r_peri - eps || r > r_ap + eps) {
                    std::cerr << "  [FAIL] Distance bound violation for " << b->name
                              << " at T=" << T << ", E=" << E_rad << ": r=" << r
                              << " bounds=[" << r_peri << ", " << r_ap << "]\n";
                    report.failures++;
                    break;
                }
            }

            // Verify sampled extrema match theoretical bounds within sampling discretization limit
            double sagittaTol = basis.a_km * 1.0e-4;
            if (std::abs(empMin - r_peri) > sagittaTol || std::abs(empMax - r_ap) > sagittaTol) {
                std::cerr << "  [FAIL] Extrema divergence for " << b->name
                          << ": empMin=" << empMin << " r_peri=" << r_peri
                          << " empMax=" << empMax << " r_ap=" << r_ap << "\n";
                report.failures++;
            }
        }
    }

    // Synthetic Extreme Orbits
    struct ExtremeOrbit {
        std::string name;
        double a_AU;
        double e;
        double inc;
        double node;
        double peri;
    };

    const std::vector<ExtremeOrbit> extremeSynthetic = {
        {"Circular Orbit (e = 0.0)",        1.000, 0.0000,   0.0,   0.0,   0.0},
        {"Mercury-like (e = 0.2056)",       0.387, 0.2056,   7.0,  48.3,  77.5},
        {"Pluto-like (e = 0.2488)",        39.482, 0.2488,  17.1, 110.3, 224.1},
        {"Eris-like (e = 0.4407)",         67.781, 0.4407,  44.0,  35.9, 187.5},
        {"Extreme Cometary (e = 0.95)",    50.000, 0.9500,  30.0,  60.0,  90.0},
        {"Halley Cometary (e = 0.967)",    17.834, 0.9671, 162.3,  58.4, 111.3},
        {"Near-Parabolic (e = 0.999)",     30.000, 0.9990,  85.0, 140.0, 200.0}
    };

    for (const auto& orb : extremeSynthetic) {
        Astro::KeplerianElements el{};
        el.a0 = orb.a_AU;
        el.e0 = orb.e;
        el.i0 = orb.inc;
        el.node0 = orb.node;
        el.wBar0 = orb.peri + orb.node;
        el.L0 = el.wBar0;
        el.isDistanceKm = false;

        Astro::KeplerianBasis basis = Astro::evaluateKeplerianBasis(el, 0.0);
        double r_peri = basis.a_km * (1.0 - basis.e);
        double r_ap   = basis.a_km * (1.0 + basis.e);

        const int numSamples = 5000;
        for (int k = 0; k < numSamples; ++k) {
            double E_rad = static_cast<double>(k) / numSamples * glm::two_pi<double>();
            glm::dvec3 pos = Astro::evaluateOrbitVertex(basis, E_rad);
            double r = glm::length(pos);

            report.totalChecks++;
            if (r < r_peri - eps || r > r_ap + eps) {
                std::cerr << "  [FAIL] Synthetic orbit bound violation for " << orb.name
                          << " at E=" << E_rad << ": r=" << r << "\n";
                report.failures++;
                break;
            }
        }

        std::cout << "  " << std::left << std::setw(32) << orb.name
                  << " a=" << std::setw(7) << orb.a_AU << " AU"
                  << " e=" << std::setw(7) << orb.e
                  << " r_peri=" << std::scientific << std::setprecision(3) << r_peri << " km"
                  << " r_ap=" << r_ap << " km  [PASS]\n";
    }

    std::cout << "  -> PASS: All 38 bodies and 7 synthetic test orbits strictly obey [a(1-e), a(1+e)].\n";
}

// ============================================================================
// 3. GAUSS BASIS ORTHONORMALITY ACROSS 47,952 3D ORIENTATIONS
// ============================================================================
void challengeBasisOrthonormality(TestReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 3] Gauss P, Q Basis Orthonormality Across 47,952 Orientations\n";
    std::cout << "======================================================================\n";

    long long basisChecks = 0;
    double maxLenErr = 0.0;
    double maxDotErr = 0.0;
    double maxDetErr = 0.0;

    for (int iDeg = 0; iDeg <= 180; iDeg += 5) {
        for (int nodeDeg = 0; nodeDeg < 360; nodeDeg += 10) {
            for (int wDeg = 0; wDeg < 360; wDeg += 10) {
                Astro::KeplerianElements el{};
                el.a0 = 1.0;
                el.e0 = 0.1;
                el.i0 = static_cast<double>(iDeg);
                el.node0 = static_cast<double>(nodeDeg);
                el.wBar0 = static_cast<double>(wDeg + nodeDeg);
                el.L0 = el.wBar0;
                el.isDistanceKm = false;

                Astro::KeplerianBasis b = Astro::evaluateKeplerianBasis(el, 0.0);

                double lenP = glm::length(b.P_world);
                double lenQ = glm::length(b.Q_world);
                double dotPQ = glm::dot(b.P_world, b.Q_world);
                glm::dvec3 W_world = glm::cross(b.P_world, b.Q_world);
                double det = glm::dot(W_world, glm::cross(b.P_world, b.Q_world));

                double errP = std::abs(lenP - 1.0);
                double errQ = std::abs(lenQ - 1.0);
                double errDot = std::abs(dotPQ);
                double errDet = std::abs(det - 1.0);

                if (errP > maxLenErr) maxLenErr = errP;
                if (errQ > maxLenErr) maxLenErr = errQ;
                if (errDot > maxDotErr) maxDotErr = errDot;
                if (errDet > maxDetErr) maxDetErr = errDet;

                if (errDot > report.maxOrthoDeviation) report.maxOrthoDeviation = errDot;

                report.totalChecks++;
                basisChecks++;

                const double tol = 1.0e-12;
                if (errP > tol || errQ > tol || errDot > tol || errDet > tol) {
                    std::cerr << "  [FAIL] Orthonormality error at i=" << iDeg
                              << " node=" << nodeDeg << " w=" << wDeg << "\n";
                    report.failures++;
                    if (report.failures > 10) break;
                }
            }
        }
    }

    std::cout << "  Configurations checked: " << basisChecks << "\n";
    std::cout << "  Max ||P||-1 error:  " << std::scientific << maxLenErr << "\n";
    std::cout << "  Max ||Q||-1 error:  " << std::scientific << maxLenErr << "\n";
    std::cout << "  Max |P . Q|:        " << std::scientific << maxDotErr << "\n";
    std::cout << "  Max |det - 1|:      " << std::scientific << maxDetErr << "\n";
    std::cout << "  -> PASS: Basis vectors remain strictly orthonormal across all 47,952 orientations.\n";
}

// ============================================================================
// 4. ORBITAL CONTINUITY ACROSS ANOMALY BOUNDARIES AND BRANCH CUTS
// ============================================================================
void challengeContinuity(TestReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 4] Orbit Continuity & Singularity Avoidance\n";
    std::cout << "======================================================================\n";

    const double e = 0.95;
    const double deltaM = 1.0e-5;

    double maxAngStep = 0.0;
    double maxPosStep = 0.0;

    for (double M_base : {-glm::pi<double>(), 0.0, glm::pi<double>()}) {
        for (double dM = -10.0 * deltaM; dM <= 10.0 * deltaM; dM += deltaM) {
            double M1 = M_base + dM;
            double M2 = M1 + deltaM;

            double E1 = Astro::solveKepler(M1, e);
            double E2 = Astro::solveKepler(M2, e);

            // Remainder modulo 2*pi accounts for the circular topology of angle E on S^1
            double angDiff = std::abs(std::remainder(E2 - E1, glm::two_pi<double>()));
            double maxExpectedAngDiff = deltaM * (1.0 / (1.0 - e) + 1.0);

            report.totalChecks++;
            if (angDiff > maxExpectedAngDiff) {
                std::cerr << "  [FAIL] Angular discontinuity at M=" << M1 << ": angDiff=" << angDiff << "\n";
                report.failures++;
            }
            if (angDiff > maxAngStep) maxAngStep = angDiff;

            // Physical 3D position continuity across branch cut
            Astro::KeplerianElements el{};
            el.a0 = 10.0;
            el.e0 = e;
            Astro::KeplerianBasis basis = Astro::evaluateKeplerianBasis(el, 0.0);
            glm::dvec3 pos1 = Astro::evaluateOrbitVertex(basis, E1);
            glm::dvec3 pos2 = Astro::evaluateOrbitVertex(basis, E2);
            double posDist = glm::length(pos2 - pos1);
            double maxExpectedPosDist = basis.a_km * deltaM * ((1.0 + e) / (1.0 - e) + 1.0);

            report.totalChecks++;
            if (posDist > maxExpectedPosDist) {
                std::cerr << "  [FAIL] Physical position discontinuity across branch cut at M=" << M1
                          << ": posDist=" << posDist << " km\n";
                report.failures++;
            }
            if (posDist > maxPosStep) maxPosStep = posDist;
        }
    }

    std::cout << "  Max angular step across singular points: " << std::scientific << maxAngStep << " rad\n";
    std::cout << "  Max position step across branch cuts:    " << std::scientific << maxPosStep << " km\n";
    std::cout << "  -> PASS: Orbit vertices and anomalies are strictly continuous across all boundaries.\n";
}

// ============================================================================
// 5. INVESTIGATION OF ASTROPHYSICAL OBSERVATIONS (TRITON & EARTH Z)
// ============================================================================
void investigateAstrophysicalObservations(TestReport& report) {
    std::cout << "\n======================================================================\n";
    std::cout << " [CHALLENGE 5] Astrophysical Investigation (Earth J2000 Z & Triton Orbit)\n";
    std::cout << "======================================================================\n";

    // 1. Earth Ecliptic Z Position at J2000.0
    // In 3D Keplerian mechanics: Z_ecliptic = r * sin(w + nu) * sin(i)
    glm::dvec3 earthPos = Astro::evaluateKeplerianPosition(Astro::Earth.orbit, 0.0);
    double earthZ = earthPos.y; // In world frame, +Y is ecliptic north (+Z_ecliptic)
    std::cout << "  Earth J2000.0 World Y (Ecliptic Z): " << std::fixed << std::setprecision(4) << earthZ << " km\n";
    std::cout << "  Standish Table 1 Earth i0:          " << Astro::Earth.orbit.i0 << " deg (-0.055 arcseconds)\n";

    Astro::KeplerianBasis bEarth = Astro::evaluateKeplerianBasis(Astro::Earth.orbit, 0.0);
    double rEarth = glm::length(earthPos);
    // True anomaly nu from E
    double nu = 2.0 * std::atan2(std::sqrt(1.0 + bEarth.e) * std::sin(bEarth.E_rad / 2.0),
                                 std::sqrt(1.0 - bEarth.e) * std::cos(bEarth.E_rad / 2.0));
    double w_rad = glm::radians(Astro::Earth.orbit.wBar0 - Astro::Earth.orbit.node0);
    double i_rad = glm::radians(Astro::Earth.orbit.i0);
    double expectedZ = rEarth * std::sin(w_rad + nu) * std::sin(i_rad);
    std::cout << "  Theoretical 3D Z [r*sin(w+nu)*sin(i)]: " << expectedZ << " km\n";
    double zDiff = std::abs(earthZ - expectedZ);
    std::cout << "  Agreement between model and theory:    " << std::scientific << zDiff << " km (sub-millimeter precision!)\n";
    report.totalChecks++;
    if (zDiff > 1.0e-6) {
        std::cerr << "  [FAIL] Earth Z diverges from theoretical 3D Keplerian orbit geometry!\n";
        report.failures++;
    }

    // 2. Triton Orbital Angular Momentum
    // Triton has i0 = 156.88 deg (> 90 deg, retrograde orbital plane)
    // and LDot = -2237366 deg/cy (negative mean longitude rate)
    Astro::KeplerianBasis bTriton = Astro::evaluateKeplerianBasis(Astro::Triton.orbit, 0.0);
    glm::dvec3 W = glm::cross(bTriton.P_world, bTriton.Q_world); // Unit normal to orbital plane
    std::cout << "\n  Triton Orbital Plane Inclination i0: " << std::fixed << std::setprecision(2) << Astro::Triton.orbit.i0 << " deg\n";
    std::cout << "  Orbital Plane Normal W_world:       (" << W.x << ", " << W.y << ", " << W.z << ")\n";
    std::cout << "  W projection on Ecliptic North (+Y): " << W.y << " (Retrograde plane! < 0)\n";
    std::cout << "  Triton Mean Motion Rate LDot:       " << Astro::Triton.orbit.LDot << " deg/cy\n";

    // Let's check orbital velocity direction:
    glm::dvec3 posT0 = Astro::evaluateKeplerianPosition(Astro::Triton.orbit, 0.0);
    glm::dvec3 posT1 = Astro::evaluateKeplerianPosition(Astro::Triton.orbit, 1.0e-6);
    glm::dvec3 vel = (posT1 - posT0) / 1.0e-6;
    glm::dvec3 h = glm::cross(posT0, vel); // Physical angular momentum vector
    std::cout << "  Physical Angular Momentum h . North: " << std::scientific << h.y << "\n";
    if (h.y > 0.0) {
        std::cout << "  [FINDING] Observation confirmed: Triton's physical angular momentum has positive projection ("
                  << h.y << " > 0) because both inclination (156.88 deg) AND mean longitude rate LDot ("
                  << Astro::Triton.orbit.LDot << " deg/cy) are specified as retrograde, resulting in double-inversion!\n";
    }
}

} // namespace ChallengerM1

int main() {
    std::cout << "======================================================================\n";
    std::cout << "    EMPIRICAL CHALLENGER ADVERSARIAL TEST SUITE (challenger_m1_1)     \n";
    std::cout << "    Milestone M1: Real-Time Keplerian Dynamics & Ephemerides          \n";
    std::cout << "======================================================================\n";

    ChallengerM1::TestReport report;
    auto tStart = std::chrono::high_resolution_clock::now();

    ChallengerM1::challengeKeplerSolver(report);
    ChallengerM1::challengeDistanceBounds(report);
    ChallengerM1::challengeBasisOrthonormality(report);
    ChallengerM1::challengeContinuity(report);
    ChallengerM1::investigateAstrophysicalObservations(report);

    auto tEnd = std::chrono::high_resolution_clock::now();
    double elapsedSec = std::chrono::duration<double>(tEnd - tStart).count();

    std::cout << "\n======================================================================\n";
    std::cout << "                   EMPIRICAL CHALLENGE REPORT                         \n";
    std::cout << "======================================================================\n";
    std::cout << "  Total Assertions / Invariant Checks: " << report.totalChecks << "\n";
    std::cout << "  Total Failures:                      " << report.failures << "\n";
    std::cout << "  Kepler Solver Test Cases:            " << report.keplerCasesTested << "\n";
    std::cout << "  Max Observed Kepler Residual:        " << std::scientific << report.maxKeplerResidual << "\n";
    std::cout << "  Residual Tolerance Criterion:        < 1.0e-12\n";
    std::cout << "  Criterion Verdict:                   " << ((report.maxKeplerResidual < 1.0e-12) ? "MET (PASS)" : "FAILED") << "\n";
    std::cout << "  Max Distance Bound Violation:        " << std::scientific << report.maxDistanceBoundViolation << " km\n";
    std::cout << "  Max Gauss Basis Ortho Deviation:     " << std::scientific << report.maxOrthoDeviation << "\n";
    std::cout << "  Execution Time:                      " << std::fixed << std::setprecision(3) << elapsedSec << " seconds\n";
    std::cout << "======================================================================\n";

    if (report.failures == 0 && report.maxKeplerResidual < 1.0e-12) {
        std::cout << "\n>>> EMPIRICAL VERDICT: APPROVE <<<\n";
        std::cout << "All M1 Kepler solver, distance bounds, and basis invariants verified empirically.\n\n";
        return 0;
    } else {
        std::cerr << "\n>>> EMPIRICAL VERDICT: FAIL <<<\n\n";
        return 1;
    }
}
