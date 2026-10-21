#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>
#include <ctime>
#include <functional>
#include <algorithm>
#include <fstream>
#include <regex>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace E2E {

// ============================================================================
// ANSI Color Codes
// ============================================================================
namespace Color {
    inline const char* Reset   = "\033[0m";
    inline const char* Bold    = "\033[1m";
    inline const char* Red     = "\033[31m";
    inline const char* Green   = "\033[32m";
    inline const char* Yellow  = "\033[33m";
    inline const char* Blue    = "\033[34m";
    inline const char* Cyan    = "\033[36m";
    inline const char* White   = "\033[37m";
}

inline std::string resolveProjectPath(const std::string& relPath) {
    std::vector<std::string> prefixes = {"", "../", "../../", "../../../"};
    for (const auto& prefix : prefixes) {
        std::string candidate = prefix + relPath;
        std::ifstream f(candidate);
        if (f.good()) return candidate;
    }
    return relPath;
}

// ============================================================================
// Test Registry & Metrics Collector
// ============================================================================
struct TestCaseResult {
    std::string tierName;
    std::string featureId;
    std::string testName;
    bool passed;
    std::string message;
    std::string file;
    int line;
    double durationMs;
};

class TestRegistry {
private:
    std::vector<TestCaseResult> results;
    std::map<std::string, int> tierPassed;
    std::map<std::string, int> tierFailed;
    std::map<std::string, int> featurePassed;
    std::map<std::string, int> featureFailed;

    TestRegistry() = default;

public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void recordResult(const std::string& tier, const std::string& feature,
                      const std::string& testName, bool passed,
                      const std::string& msg = "", const std::string& file = "", int line = 0,
                      double durationMs = 0.0) {
        results.push_back({tier, feature, testName, passed, msg, file, line, durationMs});
        if (passed) {
            tierPassed[tier]++;
            featurePassed[feature]++;
        } else {
            tierFailed[tier]++;
            featureFailed[feature]++;
        }
    }

    const std::vector<TestCaseResult>& getResults() const { return results; }
    int totalTests() const { return static_cast<int>(results.size()); }
    int totalPassed() const {
        int sum = 0;
        for (const auto& kv : tierPassed) sum += kv.second;
        return sum;
    }
    int totalFailed() const {
        int sum = 0;
        for (const auto& kv : tierFailed) sum += kv.second;
        return sum;
    }

    int getTierPassed(const std::string& tier) const {
        auto it = tierPassed.find(tier);
        return it != tierPassed.end() ? it->second : 0;
    }

    int getTierFailed(const std::string& tier) const {
        auto it = tierFailed.find(tier);
        return it != tierFailed.end() ? it->second : 0;
    }

    int getFeaturePassed(const std::string& feat) const {
        auto it = featurePassed.find(feat);
        return it != featurePassed.end() ? it->second : 0;
    }

    int getFeatureFailed(const std::string& feat) const {
        auto it = featureFailed.find(feat);
        return it != featureFailed.end() ? it->second : 0;
    }

    void printSummary() const {
        std::cout << "\n" << Color::Bold << Color::Cyan
                  << "================================================================================" << Color::Reset << "\n";
        std::cout << Color::Bold << "                     E2E ACCEPTANCE TEST SUITE SUMMARY                          " << Color::Reset << "\n";
        std::cout << Color::Bold << Color::Cyan
                  << "================================================================================" << Color::Reset << "\n";

        // Tier Breakdown
        std::cout << "\n" << Color::Bold << "--- Tier Breakdown ---" << Color::Reset << "\n";
        std::vector<std::string> tiers = {"Tier 1", "Tier 2", "Tier 3", "Tier 4"};
        for (const auto& t : tiers) {
            int p = getTierPassed(t);
            int f = getTierFailed(t);
            int tot = p + f;
            double pct = tot > 0 ? (100.0 * p / tot) : 0.0;
            std::cout << "  " << std::left << std::setw(10) << t
                      << ": Passed: " << Color::Green << std::setw(4) << p << Color::Reset
                      << " | Failed: " << (f > 0 ? Color::Red : Color::Green) << std::setw(3) << f << Color::Reset
                      << " | Total: " << std::setw(4) << tot
                      << " | Pass Rate: " << std::fixed << std::setprecision(1) << pct << "%\n";
        }

        // Feature Breakdown (F01 to F21)
        std::cout << "\n" << Color::Bold << "--- Feature Inventory Coverage (F01 - F21) ---" << Color::Reset << "\n";
        for (int i = 1; i <= 21; ++i) {
            std::ostringstream ss;
            ss << "F" << std::setfill('0') << std::setw(2) << i;
            std::string fid = ss.str();
            int p = getFeaturePassed(fid);
            int f = getFeatureFailed(fid);
            int tot = p + f;
            std::cout << "  Feature " << fid << ": "
                      << Color::Green << p << " passed" << Color::Reset << ", "
                      << (f > 0 ? Color::Red : Color::Green) << f << " failed" << Color::Reset
                      << " (Total: " << tot << " assertions)\n";
        }

        // Overall Result
        std::cout << "\n" << Color::Bold << Color::Cyan
                  << "--------------------------------------------------------------------------------" << Color::Reset << "\n";
        int totalP = totalPassed();
        int totalF = totalFailed();
        int totalAll = totalP + totalF;
        double overallPct = totalAll > 0 ? (100.0 * totalP / totalAll) : 0.0;

        std::cout << Color::Bold << "Total Assertions: " << totalAll
                  << " | " << Color::Green << "Passed: " << totalP << Color::Reset
                  << " | " << (totalF > 0 ? Color::Red : Color::Green) << "Failed: " << totalF << Color::Reset
                  << " | Pass Rate: " << Color::Bold << (totalF == 0 ? Color::Green : Color::Red)
                  << std::fixed << std::setprecision(2) << overallPct << "%" << Color::Reset << "\n";

        if (totalF == 0) {
            std::cout << Color::Bold << Color::Green
                      << "\n>>> ALL END-TO-END ACCEPTANCE TESTS PASSED SUCCESSFULLY! <<<\n"
                      << Color::Reset;
        } else {
            std::cout << Color::Bold << Color::Red
                      << "\n>>> TEST FAILURES DETECTED! SEE DETAILS ABOVE. <<<\n"
                      << Color::Reset;
        }
        std::cout << Color::Bold << Color::Cyan
                  << "================================================================================" << Color::Reset << "\n\n";
    }
};

// ============================================================================
// Assertion Macros & Verification Engine
// ============================================================================
#define E2E_ASSERT(tier, feat, name, condition, msg) \
    do { \
        bool __cond = static_cast<bool>(condition); \
        if (!__cond) { \
            std::ostringstream __oss; \
            __oss << msg; \
            std::cout << Color::Red << "  [FAIL] " << Color::Reset << tier << " / " << feat << " - " << name \
                      << " (" << __FILE__ << ":" << __LINE__ << "): " << __oss.str() << "\n"; \
            E2E::TestRegistry::instance().recordResult(tier, feat, name, false, __oss.str(), __FILE__, __LINE__); \
        } else { \
            E2E::TestRegistry::instance().recordResult(tier, feat, name, true, "", __FILE__, __LINE__); \
        } \
    } while(0)

#define E2E_ASSERT_TRUE(tier, feat, name, cond) \
    E2E_ASSERT(tier, feat, name, (cond), "Expected true but was false")

#define E2E_ASSERT_FALSE(tier, feat, name, cond) \
    E2E_ASSERT(tier, feat, name, !(cond), "Expected false but was true")

#define E2E_ASSERT_EQ(tier, feat, name, actual, expected) \
    do { \
        auto __act = (actual); \
        auto __exp = (expected); \
        E2E_ASSERT(tier, feat, name, (__act == __exp), \
            "Expected: " << __exp << ", Actual: " << __act); \
    } while(0)

#define E2E_ASSERT_NEAR(tier, feat, name, actual, expected, tol) \
    do { \
        double __act = static_cast<double>(actual); \
        double __exp = static_cast<double>(expected); \
        double __diff = std::abs(__act - __exp); \
        double __t = static_cast<double>(tol); \
        E2E_ASSERT(tier, feat, name, (__diff <= __t), \
            "Expected: " << __exp << ", Actual: " << __act << ", Diff: " << __diff << ", Max Tol: " << __t); \
    } while(0)

#define E2E_ASSERT_VEC3_NEAR(tier, feat, name, actual, expected, tol) \
    do { \
        glm::dvec3 __act(actual); \
        glm::dvec3 __exp(expected); \
        double __dist = glm::length(__act - __exp); \
        double __t = static_cast<double>(tol); \
        E2E_ASSERT(tier, feat, name, (__dist <= __t), \
            "Expected: (" << __exp.x << ", " << __exp.y << ", " << __exp.z << "), " \
            << "Actual: (" << __act.x << ", " << __act.y << ", " << __act.z << "), " \
            << "Dist: " << __dist << ", Max Tol: " << __t); \
    } while(0)

// ============================================================================
// Authoritative Reference Program / Astronomical Oracle
// ============================================================================
namespace Oracle {

    constexpr double kmPerAu = 149597870.7;
    constexpr double J2000_JD = 2451545.0;
    constexpr double UnixEpoch_JD = 2440587.5;
    constexpr double SecondsPerJulianCentury = 3155760000.0; // 36525 * 86400
    constexpr double SunRadiusKm = 696340.0;

    struct KeplerianElements {
        std::string name;
        double a0, aDot;       // AU or km, AU/cy or km/cy
        double e0, eDot;       // eccentricity, 1/cy
        double i0, iDot;       // degrees, deg/cy
        double L0, LDot;       // degrees, deg/cy
        double wBar0, wBarDot; // degrees, deg/cy
        double node0, nodeDot; // degrees, deg/cy
        bool isDistanceKm = false;
        double physicalRadiusKm = 0.0;
        std::string parentName;
    };

    // Time conversions
    inline double unixToJulianDate(double unixSeconds) {
        return UnixEpoch_JD + (unixSeconds / 86400.0);
    }

    inline double unixToJulianCenturies(double unixSeconds) {
        return (unixSeconds - 946728000.0) / SecondsPerJulianCentury;
    }

    inline double julianDateToCenturies(double jd) {
        return (jd - J2000_JD) / 36525.0;
    }

    // Format UTC Date Time
    inline std::string formatUtcDateTime(double unixSeconds) {
        std::time_t raw = static_cast<std::time_t>(unixSeconds);
        std::tm tmVal;
#if defined(_WIN32)
        gmtime_s(&tmVal, &raw);
#else
        gmtime_r(&raw, &tmVal);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmVal);
        return std::string(buf);
    }

    // Reference Newton-Raphson Kepler Solver
    inline double solveKepler(double M_rad, double e, int* outIters = nullptr, double tol = 1.0e-12) {
        // Normalize M to [-pi, pi]
        M_rad = std::fmod(M_rad, glm::two_pi<double>());
        if (M_rad > glm::pi<double>())  M_rad -= glm::two_pi<double>();
        if (M_rad < -glm::pi<double>()) M_rad += glm::two_pi<double>();

        // Initial Guess
        double E = (e < 0.8) ? (M_rad + e * std::sin(M_rad))
                             : (M_rad + (M_rad >= 0.0 ? 1.0 : -1.0) * 0.85 * e);

        int iters = 0;
        for (; iters < 25; ++iters) {
            double f = E - e * std::sin(E) - M_rad;
            double fPrime = 1.0 - e * std::cos(E);
            double delta = f / fPrime;
            E -= delta;
            if (std::abs(delta) < tol) {
                iters++;
                break;
            }
        }
        if (outIters) *outIters = iters;
        return E;
    }

    // Compute True 3D Position
    inline glm::dvec3 computeOrbitalPosition(const KeplerianElements& elem, double T, double* outE = nullptr) {
        double a = elem.a0 + elem.aDot * T;
        double e = elem.e0 + elem.eDot * T;
        double i = glm::radians(elem.i0 + elem.iDot * T);
        double L = glm::radians(elem.L0 + elem.LDot * T);
        double wBar = glm::radians(elem.wBar0 + elem.wBarDot * T);
        double Omega = glm::radians(elem.node0 + elem.nodeDot * T);

        double omega = wBar - Omega;
        double M = L - wBar;

        double E = solveKepler(M, e);
        if (outE) *outE = E;

        double distScale = elem.isDistanceKm ? 1.0 : kmPerAu;
        double aKm = a * distScale;

        // In-plane coords
        double xPrime = aKm * (std::cos(E) - e);
        double yPrime = aKm * std::sqrt(std::max(0.0, 1.0 - e * e)) * std::sin(E);

        // Basis vectors P, Q in ecliptic frame
        double cosO = std::cos(Omega), sinO = std::sin(Omega);
        double cosW = std::cos(omega), sinW = std::sin(omega);
        double cosI = std::cos(i),     sinI = std::sin(i);

        glm::dvec3 P(
            cosO * cosW - sinO * sinW * cosI,
            sinO * cosW + cosO * sinW * cosI,
            sinW * sinI
        );

        glm::dvec3 Q(
            -cosO * sinW - sinO * cosW * cosI,
            -sinO * sinW + cosO * cosW * cosI,
            cosW * sinI
        );

        glm::dvec3 rEcl = xPrime * P + yPrime * Q;

        // Convert to engine world frame: Ecliptic +X -> World +X, Ecliptic +Z -> World +Y, Ecliptic +Y -> World -Z
        return glm::dvec3(rEcl.x, rEcl.z, -rEcl.y);
    }

    // Generate 3D Elliptical Orbit Line Loop Vertices
    inline std::vector<glm::dvec3> generateOrbitVertices(const KeplerianElements& elem, double T, int numSegments = 180, double startE = 0.0) {
        std::vector<glm::dvec3> vertices;
        vertices.reserve(numSegments);

        double a = elem.a0 + elem.aDot * T;
        double e = elem.e0 + elem.eDot * T;
        double i = glm::radians(elem.i0 + elem.iDot * T);
        double wBar = glm::radians(elem.wBar0 + elem.wBarDot * T);
        double Omega = glm::radians(elem.node0 + elem.nodeDot * T);
        double omega = wBar - Omega;

        double distScale = elem.isDistanceKm ? 1.0 : kmPerAu;
        double aKm = a * distScale;

        double cosO = std::cos(Omega), sinO = std::sin(Omega);
        double cosW = std::cos(omega), sinW = std::sin(omega);
        double cosI = std::cos(i),     sinI = std::sin(i);

        glm::dvec3 P(
            cosO * cosW - sinO * sinW * cosI,
            sinO * cosW + cosO * sinW * cosI,
            sinW * sinI
        );

        glm::dvec3 Q(
            -cosO * sinW - sinO * cosW * cosI,
            -sinO * sinW + cosO * cosW * cosI,
            cosW * sinI
        );

        double step = glm::two_pi<double>() / static_cast<double>(numSegments);
        for (int k = 0; k < numSegments; ++k) {
            double Ek = startE + k * step;
            double xPrime = aKm * (std::cos(Ek) - e);
            double yPrime = aKm * std::sqrt(std::max(0.0, 1.0 - e * e)) * std::sin(Ek);
            glm::dvec3 rEcl = xPrime * P + yPrime * Q;
            vertices.push_back(glm::dvec3(rEcl.x, rEcl.z, -rEcl.y));
        }
        return vertices;
    }

    // Analytical Dynamic Eclipse Shadow Function (Matching base.frag)
    inline float computeConicalShadowFactor(
        const glm::dvec3& sunPos, double sunRadius,
        const glm::dvec3& casterPos, double casterRadius,
        const glm::dvec3& fragPos, const glm::dvec3& fragNormal)
    {
        // 1. Night-side early rejection
        glm::dvec3 lightDir = glm::normalize(sunPos - fragPos);
        double diff = glm::dot(fragNormal, lightDir);
        if (diff <= 0.0) {
            return 1.0f; // Night side is naturally dark via Lambertian term, skip eclipse attenuation
        }

        // 2. Occluder-relative vector
        glm::dvec3 toCaster = casterPos - sunPos;
        double dSun = glm::length(toCaster);
        if (dSun <= casterRadius) return 1.0f;

        glm::dvec3 shadowAxis = toCaster / dSun;
        glm::dvec3 toFrag = fragPos - casterPos;
        double z = glm::dot(toFrag, shadowAxis);

        if (z <= 0.0) {
            // Fragment is on the illuminated (sun-facing) side of the caster
            return 1.0f;
        }

        double tanPenumbra = (sunRadius + casterRadius) / dSun;
        double tanUmbra    = (sunRadius - casterRadius) / dSun;

        double rPenumbra = casterRadius + z * tanPenumbra;
        double rUmbra    = casterRadius - z * tanUmbra;

        glm::dvec3 perp = toFrag - z * shadowAxis;
        double dPerpSq = glm::dot(perp, perp);

        if (dPerpSq >= rPenumbra * rPenumbra) {
            return 1.0f; // Completely outside penumbra
        }

        double dPerp = std::sqrt(dPerpSq);
        double umbraBound = std::max(0.0, rUmbra);

        // Smoothstep function: smooth transition between umbraBound and rPenumbra
        auto smoothstep = [](double edge0, double edge1, double x) -> double {
            double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };

        float s = static_cast<float>(smoothstep(umbraBound, rPenumbra, dPerp));

        // Annular eclipse handling when rUmbra < 0.0
        if (rUmbra < 0.0) {
            double occultationRatio = std::clamp((casterRadius * dSun) / (sunRadius * z), 0.0, 1.0);
            double sMin = 1.0 - (occultationRatio * occultationRatio);
            s = static_cast<float>(sMin + (1.0 - sMin) * s);
        }

        return s;
    }

    // Logarithmic depth buffer calculation (normalized NDC depth in [-1, 1])
    inline float computeLogarithmicDepth(float w, float maxDist = 3.0e10f) {
        if (w <= 0.0f) return -1.0f;
        float Fcoef = 2.0f / std::log2(maxDist + 1.0f);
        return std::log2(std::max(1.0e-6f, 1.0f + w)) * Fcoef - 1.0f;
    }

    // Complete NASA JPL Standish Celestial Catalog (38 bodies)
    inline std::vector<KeplerianElements> getStandardCatalog() {
        return {
            // Sun-Orbiting Major Planets (Standish 1992 J2000.0)
            {"Mercury", 0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.25032350, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081, false, 2439.7, "Sun"},
            {"Venus",   0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890, 181.97909950, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418, false, 6051.8, "Sun"},
            {"Earth",   1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0.0, 0.0, false, 6371.0, "Sun"},
            {"Mars",    1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343, false, 3389.5, "Sun"},
            {"Jupiter", 5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106, false, 69911.0, "Sun"},
            {"Saturn",  9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794, false, 58232.0, "Sun"},
            {"Uranus",  19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939, 313.23810451, 428.48202785, 170.95427630, 0.40805281, 74.01692503, 0.04240589, false, 25362.0, "Sun"},
            {"Neptune", 30.06992276, 0.00026291, 0.00860610, 0.00005105, 1.77004347, 0.00035372, -55.12002969, 218.45945325, 44.96476224, -0.32241464, 131.78422574, -0.00508664, false, 24622.0, "Sun"},
            {"Pluto",   39.48211675, -0.00031596, 0.24882730, 0.00005170, 17.14001206, 0.00004818, 238.92903833, 145.20780515, 224.06876938, -0.04062942, 110.30393608, -0.01183482, false, 1188.3, "Sun"},

            // Asteroids
            {"Ceres",   2.7675, 0.0, 0.0758, 0.0, 10.593, 0.0, 231.294, 7818.79, 153.924, 0.0, 80.327, 0.0, false, 473.0, "Sun"},
            {"Vesta",   2.3618, 0.0, 0.0887, 0.0, 7.140, 0.0, 275.400, 9923.77, 254.539, 0.0, 103.810, 0.0, false, 262.7, "Sun"},
            {"Pallas",  2.7724, 0.0, 0.2313, 0.0, 34.841, 0.0, 182.780, 7800.12, 123.129, 0.0, 173.080, 0.0, false, 256.0, "Sun"},
            {"Eros",    1.4583, 0.0, 0.2227, 0.0, 10.828, 0.0, 78.400, 20449.46, 123.139, 0.0, 304.322, 0.0, false, 8.4, "Sun"},
            {"Gaspra",  2.2096, 0.0, 0.1739, 0.0, 4.102, 0.0, 165.870, 10966.64, 22.670, 0.0, 253.197, 0.0, false, 6.1, "Sun"},
            {"Ida",     2.8610, 0.0, 0.0461, 0.0, 1.138, 0.0, 158.320, 7441.43, 74.221, 0.0, 324.080, 0.0, false, 15.7, "Sun"},

            // Dwarf Planets
            {"Haumea",   43.132, 0.0, 0.1912, 0.0, 28.190, 0.0, 205.620, 126.71, 2.220, 0.0, 122.020, 0.0, false, 798.0, "Sun"},
            {"Makemake", 45.791, 0.0, 0.1590, 0.0, 28.960, 0.0, 179.950, 116.47, 14.450, 0.0, 79.620, 0.0, false, 715.0, "Sun"},
            {"Eris",     67.781, 0.0, 0.4407, 0.0, 44.040, 0.0, 30.310, 64.51, 187.510, 0.0, 35.870, 0.0, false, 1163.0, "Sun"},

            // Moons (Distances in km)
            {"Moon",      384400.0, 0.0, 0.05490, 0.0, 5.145, 0.0, 218.32, 481267.881, 83.35, 4069.014, 125.08, -1934.136, true, 1737.4, "Earth"},
            {"Phobos",    9376.0, 0.0, 0.0151, 0.0, 1.093, 0.0, 0.0, 4123106.8, 0.0, 0.0, 0.0, 0.0, true, 11.2, "Mars"},
            {"Deimos",    23460.0, 0.0, 0.0002, 0.0, 0.930, 0.0, 0.0, 1041092.6, 0.0, 0.0, 0.0, 0.0, true, 6.2, "Mars"},
            {"Io",        421700.0, 0.0, 0.0041, 0.0, 0.050, 0.0, 106.08, 743259.3, 84.0, 0.0, 0.0, 0.0, true, 1821.6, "Jupiter"},
            {"Europa",    671100.0, 0.0, 0.0090, 0.0, 0.470, 0.0, 175.73, 370269.2, 219.0, 0.0, 0.0, 0.0, true, 1560.8, "Jupiter"},
            {"Ganymede",  1070400.0, 0.0, 0.0013, 0.0, 0.200, 0.0, 87.59, 183783.8, 192.0, 0.0, 0.0, 0.0, true, 2634.1, "Jupiter"},
            {"Callisto",  1882700.0, 0.0, 0.0074, 0.0, 0.280, 0.0, 289.41, 78788.4, 52.0, 0.0, 0.0, 0.0, true, 2410.3, "Jupiter"},
            {"Mimas",     185520.0, 0.0, 0.0202, 0.0, 1.570, 0.0, 0.0, 1395267.0, 0.0, 0.0, 0.0, 0.0, true, 198.2, "Saturn"},
            {"Enceladus", 237900.0, 0.0, 0.0047, 0.0, 0.010, 0.0, 0.0, 959640.0, 0.0, 0.0, 0.0, 0.0, true, 252.1, "Saturn"},
            {"Dione",     377400.0, 0.0, 0.0022, 0.0, 0.020, 0.0, 0.0, 480434.0, 0.0, 0.0, 0.0, 0.0, true, 561.4, "Saturn"},
            {"Rhea",      527040.0, 0.0, 0.00125, 0.0, 0.350, 0.0, 0.0, 291068.0, 0.0, 0.0, 0.0, 0.0, true, 763.8, "Saturn"},
            {"Titan",     1221800.0, 0.0, 0.0288, 0.0, 0.350, 0.0, 260.0, 82464.7, 180.0, 0.0, 0.0, 0.0, true, 2574.7, "Saturn"},
            {"Iapetus",   3561300.0, 0.0, 0.0286, 0.0, 15.470, 0.0, 0.0, 16575.0, 0.0, 0.0, 0.0, 0.0, true, 734.5, "Saturn"},
            {"Miranda",   129390.0, 0.0, 0.0013, 0.0, 4.340, 0.0, 0.0, 930573.0, 0.0, 0.0, 0.0, 0.0, true, 235.8, "Uranus"},
            {"Ariel",     191020.0, 0.0, 0.0012, 0.0, 0.260, 0.0, 0.0, 521785.0, 0.0, 0.0, 0.0, 0.0, true, 578.9, "Uranus"},
            {"Umbriel",   266000.0, 0.0, 0.0039, 0.0, 0.200, 0.0, 0.0, 317302.0, 0.0, 0.0, 0.0, 0.0, true, 584.7, "Uranus"},
            {"Titania",   436300.0, 0.0, 0.0011, 0.0, 0.340, 0.0, 0.0, 151033.0, 0.0, 0.0, 0.0, 0.0, true, 788.4, "Uranus"},
            {"Oberon",    583520.0, 0.0, 0.0014, 0.0, 0.060, 0.0, 0.0, 97667.0, 0.0, 0.0, 0.0, 0.0, true, 761.4, "Uranus"},
            {"Triton",    354800.0, 0.0, 0.000016, 0.0, 156.880, 0.0, 0.0, -223736.0, 0.0, 0.0, 0.0, 0.0, true, 1353.4, "Neptune"},
            {"Charon",    19591.0, 0.0, 0.0002, 0.0, 0.0, 0.0, 0.0, 205840.0, 0.0, 0.0, 0.0, 0.0, true, 606.0, "Pluto"}
        };
    }

} // namespace Oracle

} // namespace E2E
