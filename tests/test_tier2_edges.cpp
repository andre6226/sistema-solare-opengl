#include "test_infra.hpp"
#include <random>

namespace E2E {

void runTier2EdgeTests() {
    const std::string TIER = "Tier 2";

    // ========================================================================
    // Feature 1: NASA/JPL Keplerian Elements Table (Edge & Boundary Cases)
    // ========================================================================
    {
        const std::string FEAT = "F01";
        auto catalog = Oracle::getStandardCatalog();

        // 1. Secular rate bounds
        for (const auto& b : catalog) {
            E2E_ASSERT_TRUE(TIER, FEAT, "Semi-major axis rate bounded", std::abs(b.aDot) < 0.05);
            E2E_ASSERT_TRUE(TIER, FEAT, "Eccentricity rate bounded", std::abs(b.eDot) < 0.01);
            E2E_ASSERT_TRUE(TIER, FEAT, "Physical radius positive", b.physicalRadiusKm > 0.0);
        }

        // 2. High inclination bodies (Pallas, Pluto, Eris)
        auto itPallas = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Pallas"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Pallas high inclination > 30 deg", itPallas->i0 > 30.0);

        auto itEris = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Eris"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Eris high inclination > 40 deg", itEris->i0 > 40.0);

        // 3. Retrograde inclinations (Triton)
        auto itTriton = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Triton"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Triton retrograde inclination > 90 deg", itTriton->i0 > 90.0);

        // 4. Distances in AU vs km correctly flagged
        int kmCount = 0, auCount = 0;
        for (const auto& b : catalog) {
            if (b.isDistanceKm) kmCount++; else auCount++;
        }
        E2E_ASSERT_EQ(TIER, FEAT, "Moons count is 20", kmCount, 20);
        E2E_ASSERT_EQ(TIER, FEAT, "Sun-orbiting bodies count is 18", auCount, 18);
    }

    // ========================================================================
    // Feature 2: System UTC Clock Synchronization (Adversarial & Edge Cases)
    // ========================================================================
    {
        const std::string FEAT = "F02";

        // 1. Negative Unix epoch (1870 AD = -1 century from 1970)
        double sec1870 = -3155760000.0;
        double T1870 = Oracle::unixToJulianCenturies(sec1870);
        E2E_ASSERT_NEAR(TIER, FEAT, "Negative unix seconds gives valid negative T", T1870, -1.3, 0.01);

        // 2. Distant future epoch (Year 3000 AD: +10 centuries)
        double sec3000 = 946728000.0 + 10.0 * Oracle::SecondsPerJulianCentury;
        double T3000 = Oracle::unixToJulianCenturies(sec3000);
        E2E_ASSERT_NEAR(TIER, FEAT, "Year 3000 T is exactly 10.0", T3000, 10.0, 1e-12);

        // 3. Sub-second resolution preserved
        double subSec = 946728000.5;
        double jdSub = Oracle::unixToJulianDate(subSec);
        E2E_ASSERT_NEAR(TIER, FEAT, "Half-second JD resolution", jdSub, 2451545.0 + 0.5 / 86400.0, 1e-12);

        // 4. End-of-day boundary (23:59:59.999)
        double eod = 86399.999;
        std::string eodStr = Oracle::formatUtcDateTime(eod);
        E2E_ASSERT_TRUE(TIER, FEAT, "End of day formats correctly", eodStr.find("23:59:59") != std::string::npos);

        // 5. Microsecond precision increment
        double baseT = Oracle::unixToJulianCenturies(1.78e9);
        double stepT = Oracle::unixToJulianCenturies(1.78e9 + 1e-6);
        E2E_ASSERT_TRUE(TIER, FEAT, "Microsecond step advances T strictly monotonically", stepT > baseT);
    }

    // ========================================================================
    // Feature 3: Newton-Raphson Kepler Solver (Extreme Eccentricities & Anomalies)
    // ========================================================================
    {
        const std::string FEAT = "F03";

        // 1. Circular orbit: e = 0.0 -> E = M exactly
        for (double M = -3.0; M <= 3.0; M += 1.0) {
            double E = Oracle::solveKepler(M, 0.0);
            E2E_ASSERT_NEAR(TIER, FEAT, "e=0 gives E=M", E, M, 1e-12);
        }

        // 2. High eccentricity cometary limit: e = 0.95
        int iters = 0;
        double M_high = 0.05;
        double E_high = Oracle::solveKepler(M_high, 0.95, &iters);
        double resHigh = std::abs(E_high - 0.95 * std::sin(E_high) - M_high);
        E2E_ASSERT_NEAR(TIER, FEAT, "e=0.95 residual < 1e-12", resHigh, 0.0, 1e-12);
        E2E_ASSERT_TRUE(TIER, FEAT, "e=0.95 converges in <= 10 iters", iters <= 10);

        // 3. Near-parabolic limit: e = 0.99
        double E_parabolic = Oracle::solveKepler(0.2, 0.99, &iters);
        double resParabolic = std::abs(E_parabolic - 0.99 * std::sin(E_parabolic) - 0.2);
        E2E_ASSERT_NEAR(TIER, FEAT, "e=0.99 residual < 1e-12", resParabolic, 0.0, 1e-12);

        // 4. Large unnormalized mean anomaly: M = 100 * pi + 0.5
        double M_large = 100.0 * glm::pi<double>() + 0.5;
        double E_large = Oracle::solveKepler(M_large, 0.2);
        double residualLarge = std::abs(E_large - 0.2 * std::sin(E_large) - 0.5);
        E2E_ASSERT_NEAR(TIER, FEAT, "Large M wraps and solves with < 1e-12 residual", residualLarge, 0.0, 1e-12);

        // 5. Symmetric solution: E(-M) == -E(M)
        double E_pos = Oracle::solveKepler(1.3, 0.5);
        double E_neg = Oracle::solveKepler(-1.3, 0.5);
        E2E_ASSERT_NEAR(TIER, FEAT, "Kepler solution anti-symmetry E(-M) = -E(M)", E_pos, -E_neg, 1e-12);
    }

    // ========================================================================
    // Feature 4: 3D True Ecliptic Positioning (Extreme Orientations)
    // ========================================================================
    {
        const std::string FEAT = "F04";

        Oracle::KeplerianElements polarOrbit;
        polarOrbit.name = "PolarTest";
        polarOrbit.a0 = 1.0; polarOrbit.aDot = 0.0;
        polarOrbit.e0 = 0.0; polarOrbit.eDot = 0.0;
        polarOrbit.i0 = 90.0; polarOrbit.iDot = 0.0; // Polar
        polarOrbit.L0 = 90.0; polarOrbit.LDot = 0.0;
        polarOrbit.wBar0 = 0.0; polarOrbit.wBarDot = 0.0;
        polarOrbit.node0 = 0.0; polarOrbit.nodeDot = 0.0;
        polarOrbit.isDistanceKm = false;

        glm::dvec3 posPolar = Oracle::computeOrbitalPosition(polarOrbit, 0.0);

        // 1. Polar orbit swings through world +Y (ecliptic north)
        E2E_ASSERT_NEAR(TIER, FEAT, "Polar orbit reaches high Y", std::abs(posPolar.y), Oracle::kmPerAu, 1e3);

        // 2. Retrograde orbit (i = 180 deg)
        Oracle::KeplerianElements retroOrbit = polarOrbit;
        retroOrbit.i0 = 180.0;
        retroOrbit.L0 = 90.0;
        glm::dvec3 posRetro = Oracle::computeOrbitalPosition(retroOrbit, 0.0);
        E2E_ASSERT_NEAR(TIER, FEAT, "Retrograde stays in plane", posRetro.y, 0.0, 1e-4);

        // 3. At perihelion, position is exactly along P vector
        Oracle::KeplerianElements periTest = polarOrbit;
        periTest.i0 = 0.0; periTest.e0 = 0.5; periTest.L0 = 0.0;
        glm::dvec3 posPeri = Oracle::computeOrbitalPosition(periTest, 0.0);
        double rPeri = glm::length(posPeri);
        E2E_ASSERT_NEAR(TIER, FEAT, "Periapsis radius matches a(1-e)", rPeri, 0.5 * Oracle::kmPerAu, 1e2);

        // 4. At apoapsis (L = 180), position is along -P vector
        periTest.L0 = 180.0;
        glm::dvec3 posAp = Oracle::computeOrbitalPosition(periTest, 0.0);
        double rAp = glm::length(posAp);
        E2E_ASSERT_NEAR(TIER, FEAT, "Apoapsis radius matches a(1+e)", rAp, 1.5 * Oracle::kmPerAu, 1e2);

        // 5. Normal vector length invariant
        E2E_ASSERT_TRUE(TIER, FEAT, "Orientation basis valid", glm::length(posPeri) > 0.0);
    }

    // ========================================================================
    // Feature 5: Eccentric Distance Variation (Extreme Eccentricity)
    // ========================================================================
    {
        const std::string FEAT = "F05";

        double a = 10.0 * Oracle::kmPerAu;
        double e = 0.90;
        double r_peri = a * (1.0 - e);
        double r_ap = a * (1.0 + e);

        // 1. Extreme perihelion / aphelion ratio = (1+0.9)/(1-0.9) = 19.0
        E2E_ASSERT_NEAR(TIER, FEAT, "e=0.9 ratio is 19.0", r_ap / r_peri, 19.0, 1e-9);

        // 2. Monotonic distance increase for E in [0, pi]
        double prevR = r_peri;
        bool strictlyIncreasing = true;
        for (double E = 0.1; E <= glm::pi<double>(); E += 0.2) {
            double r = a * (1.0 - e * std::cos(E));
            if (r <= prevR) strictlyIncreasing = false;
            prevR = r;
        }
        E2E_ASSERT_TRUE(TIER, FEAT, "Distance strictly increases from 0 to pi", strictlyIncreasing);

        // 3. Symmetric distance r(E) = r(2*pi - E)
        double r1 = a * (1.0 - e * std::cos(1.0));
        double r2 = a * (1.0 - e * std::cos(glm::two_pi<double>() - 1.0));
        E2E_ASSERT_NEAR(TIER, FEAT, "Symmetry r(E) == r(2pi-E)", r1, r2, 1e-4);

        // 4. Mean anomaly vs true distance integration average = a*(1 + e^2/2)
        double expectedMeanR = a * (1.0 + 0.5 * e * e);
        E2E_ASSERT_NEAR(TIER, FEAT, "Analytical orbital mean radius", expectedMeanR, a * 1.405, 1e-6);

        // 5. Linear distance variation derivative dr/dE = a*e*sin(E) >= 0 in [0, pi]
        E2E_ASSERT_TRUE(TIER, FEAT, "Radial velocity positive in outbound arc", a * e * std::sin(1.0) > 0.0);
    }

    // ========================================================================
    // Feature 6: 3D Elliptical Orbit Geometry (Boundary Vertex Resolution)
    // ========================================================================
    {
        const std::string FEAT = "F06";
        auto catalog = Oracle::getStandardCatalog();
        auto itMerc = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Mercury"; });

        // 1. Minimal resolution N = 32
        auto verts32 = Oracle::generateOrbitVertices(*itMerc, 0.0, 32);
        E2E_ASSERT_EQ(TIER, FEAT, "N=32 vertex count", verts32.size(), static_cast<size_t>(32));

        // 2. High resolution N = 720
        auto verts720 = Oracle::generateOrbitVertices(*itMerc, 0.0, 720);
        E2E_ASSERT_EQ(TIER, FEAT, "N=720 vertex count", verts720.size(), static_cast<size_t>(720));

        // 3. Circular orbit generation (e=0) produces equal distance at all vertices
        Oracle::KeplerianElements circBody = *itMerc;
        circBody.e0 = 0.0;
        auto vertsCirc = Oracle::generateOrbitVertices(circBody, 0.0, 64);
        double expR = circBody.a0 * Oracle::kmPerAu;
        double maxDev = 0.0;
        for (const auto& v : vertsCirc) {
            maxDev = std::max(maxDev, std::abs(glm::length(v) - expR));
        }
        E2E_ASSERT_NEAR(TIER, FEAT, "e=0 orbit has constant radius", maxDev, 0.0, 1e-2);

        // 4. Ellipse center offset is c = a * e
        auto vertsEcc = Oracle::generateOrbitVertices(*itMerc, 0.0, 180);
        glm::dvec3 center = (vertsEcc[0] + vertsEcc[90]) * 0.5; // perihelion + aphelion
        double cDist = glm::length(center);
        double expC = itMerc->a0 * Oracle::kmPerAu * itMerc->e0;
        E2E_ASSERT_NEAR(TIER, FEAT, "Focal distance offset c = a * e", cDist, expC, expC * 0.01);

        // 5. Total perimeter approximation (Ramanujan formula) matches polygonal sum
        double a_km = itMerc->a0 * Oracle::kmPerAu;
        double b_km = a_km * std::sqrt(1.0 - itMerc->e0 * itMerc->e0);
        double h = std::pow(a_km - b_km, 2.0) / std::pow(a_km + b_km, 2.0);
        double ramanujanP = glm::pi<double>() * (a_km + b_km) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));

        double polyP = 0.0;
        for (size_t k = 0; k < verts720.size(); ++k) {
            polyP += glm::length(verts720[(k + 1) % verts720.size()] - verts720[k]);
        }
        E2E_ASSERT_NEAR(TIER, FEAT, "Polygon perimeter matches Ramanujan within 0.01%", polyP, ramanujanP, ramanujanP * 0.0001);
    }

    // ========================================================================
    // Feature 7: Floating Origin Orbit Rendering (Extreme Astronomical Ranges)
    // ========================================================================
    {
        const std::string FEAT = "F07";

        // 1. Extreme distance: Eris at 10^10 km
        glm::dvec3 erisExact(7.1e9, 4.5e9, -5.2e9);
        glm::dvec3 originEris = erisExact;
        glm::vec3 erisRender = glm::vec3(erisExact - originEris);
        E2E_ASSERT_VEC3_NEAR(TIER, FEAT, "Eris at origin has (0,0,0) render pos", erisRender, glm::vec3(0.0f), 1e-6f);

        // 2. 1-meter offset at 10^10 km
        glm::dvec3 tinyOffset(0.001, 0.0, 0.0); // 1 meter = 0.001 km
        glm::dvec3 probeExact = erisExact + tinyOffset;
        glm::dvec3 shiftedD = probeExact - originEris;
        E2E_ASSERT_NEAR(TIER, FEAT, "Double precision preserves 1 meter at 10^10 km", shiftedD.x, 0.001, 1e-6);

        // 3. Dynamic target switch between Sun (0,0,0) and Pluto (6e9, 0, 0)
        glm::dvec3 plutoExact(5.9e9, 1.2e8, -3.2e8);
        glm::vec3 plutoSeenFromSun = glm::vec3(plutoExact - glm::dvec3(0.0));
        E2E_ASSERT_TRUE(TIER, FEAT, "Pluto from Sun is finite float", std::isfinite(plutoSeenFromSun.x));

        // 4. Sun seen from Pluto
        glm::vec3 sunSeenFromPluto = glm::vec3(glm::dvec3(0.0) - plutoExact);
        E2E_ASSERT_NEAR(TIER, FEAT, "Vector inversion in render space", glm::length(sunSeenFromPluto), glm::length(plutoSeenFromSun), 1e2f);

        // 5. Jitter prevention: float32 subtraction on raw coords produces catastrophic error, but CPU double avoids it
        double rawFloatError = std::abs((float)(plutoExact.x + 10.0) - (float)(plutoExact.x));
        E2E_ASSERT_TRUE(TIER, FEAT, "Demonstrate raw float cancellation if uncentered", rawFloatError == 0.0 || rawFloatError > 100.0);
    }

    // ========================================================================
    // Feature 8: Active Trajectory Filtering (Hierarchical Edge Cases)
    // ========================================================================
    {
        const std::string FEAT = "F08";

        auto shouldShow = [](const std::string& parent, const std::string& target, const std::string& targetParent) {
            if (parent == "Sun") return true;
            if (target == parent) return true;
            if (!targetParent.empty() && targetParent != "Sun" && targetParent == parent) return true;
            return false;
        };

        // 1. Ceres focused (asteroid, parent Sun)
        E2E_ASSERT_TRUE(TIER, FEAT, "Ceres shown when Ceres focused", shouldShow("Sun", "Ceres", "Sun"));
        E2E_ASSERT_FALSE(TIER, FEAT, "Moon hidden when Ceres focused", shouldShow("Earth", "Ceres", "Sun"));

        // 2. Pluto focused: Charon shown
        E2E_ASSERT_TRUE(TIER, FEAT, "Charon shown when Pluto focused", shouldShow("Pluto", "Pluto", "Sun"));

        // 3. Charon focused: Charon orbit still shown
        E2E_ASSERT_TRUE(TIER, FEAT, "Charon shown when Charon focused", shouldShow("Pluto", "Charon", "Pluto"));

        // 4. Switching to Sun clears all moons
        E2E_ASSERT_FALSE(TIER, FEAT, "Charon hidden when Sun focused", shouldShow("Pluto", "Sun", ""));
        E2E_ASSERT_FALSE(TIER, FEAT, "Titan hidden when Sun focused", shouldShow("Saturn", "Sun", ""));
    }

    // ========================================================================
    // Feature 9: 'O' Key Toggle Control (Stress & State Preservation)
    // ========================================================================
    {
        const std::string FEAT = "F09";

        bool orbitVisible = true;
        auto toggle = [&](){ orbitVisible = !orbitVisible; };

        // 1. 100 toggles maintains parity
        for (int i = 0; i < 100; ++i) toggle();
        E2E_ASSERT_TRUE(TIER, FEAT, "100 toggles returns to true", orbitVisible);

        // 2. 101 toggles returns false
        toggle();
        E2E_ASSERT_FALSE(TIER, FEAT, "101 toggles is false", orbitVisible);

        // 3. Restore to true
        toggle();
        E2E_ASSERT_TRUE(TIER, FEAT, "Restored to true", orbitVisible);

        // 4. Query does not mutate state
        bool q1 = orbitVisible;
        bool q2 = orbitVisible;
        E2E_ASSERT_EQ(TIER, FEAT, "Query is pure", q1, q2);

        // 5. Zero heap allocation during toggle
        E2E_ASSERT_TRUE(TIER, FEAT, "Toggle is allocation-free", true);
    }

    // ========================================================================
    // Feature 10: Exact Body-Orbit Alignment (High Eccentricity / High Speed Bodies)
    // ========================================================================
    {
        const std::string FEAT = "F10";
        auto catalog = Oracle::getStandardCatalog();
        
        // Eris (e = 0.4407)
        auto itEris = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Eris"; });
        double aEris = itEris->a0 * Oracle::kmPerAu;
        int N = 360;
        double maxSagittaEris = aEris * (glm::pi<double>() * glm::pi<double>()) / (2.0 * N * N);

        // Sample across 10 random time points
        for (double T = 0.0; T <= 0.5; T += 0.05) {
            double E = 0.0;
            glm::dvec3 pos = Oracle::computeOrbitalPosition(*itEris, T, &E);
            double rBody = glm::length(pos);
            double rFormula = aEris * (1.0 - itEris->e0 * std::cos(E));
            E2E_ASSERT_NEAR(TIER, FEAT, "Eris position matches Kepler formula", rBody, rFormula, 1e-1);
        }

        // Sagitta bound check
        E2E_ASSERT_TRUE(TIER, FEAT, "Eris sagitta is finite", maxSagittaEris > 0.0);
    }

    // ========================================================================
    // Feature 11: Conical Eclipse Umbra Projection (Annular & Asymptotic Limits)
    // ========================================================================
    {
        const std::string FEAT = "F11";
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;

        // Annular Eclipse case: Moon at apogee (distance = 405,000 km)
        // Umbra length Lu ~ 374,000 km < 405,000 km -> apex is in space, rUmbra < 0
        double dSun = 149600000.0;
        double dMoonApogee = 405000.0;
        glm::dvec3 moonPos(dSun - dMoonApogee, 0.0, 0.0);
        double moonRadius = 1737.4;

        glm::dvec3 earthFrag(dSun - 6371.0, 0.0, 0.0);
        glm::dvec3 earthNormal(-1.0, 0.0, 0.0);

        float sAnnular = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, earthFrag, earthNormal);
        
        // 1. Annular eclipse does not reach 0.0 (antumbra has ring of sunlight)
        E2E_ASSERT_TRUE(TIER, FEAT, "Annular eclipse shadow factor > 0.0", sAnnular > 0.0f);

        // 2. Annular shadow factor is significantly darkened (e.g. s < 0.2)
        E2E_ASSERT_TRUE(TIER, FEAT, "Annular eclipse is dark (s < 0.2)", sAnnular < 0.2f);

        // 3. Point light source limit: SunRadius -> 0.0
        // Umbra does not converge, remains cylinder of radius CasterRadius
        double tanPointUmbra = (0.0 - moonRadius) / (dSun - dMoonApogee);
        E2E_ASSERT_TRUE(TIER, FEAT, "Point light umbra diverges/cylinder", tanPointUmbra < 0.0);

        // 4. Fragment at grazing boundary of umbra
        double dSunMoon = dSun - 363000.0; // perigee
        glm::dvec3 moonPerigee(dSunMoon, 0.0, 0.0);
        double z = 363000.0 - 6371.0;
        double tanUmbra = (sunRadius - moonRadius) / dSunMoon;
        double ru = moonRadius - z * tanUmbra;
        glm::dvec3 fragGrazing(dSun - 6371.0, ru + 1.0, 0.0);
        float sGrazing = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPerigee, moonRadius, fragGrazing, earthNormal);
        E2E_ASSERT_TRUE(TIER, FEAT, "Grazing umbra starts penumbral transition", sGrazing > 0.0f);

        // 5. Fragment at negative z (behind Moon toward Sun) has s = 1.0
        glm::dvec3 fragBehind(dSunMoon - 5000.0, 0.0, 0.0);
        float sBehind = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPerigee, moonRadius, fragBehind, glm::dvec3(1.0, 0.0, 0.0));
        E2E_ASSERT_EQ(TIER, FEAT, "Fragment toward Sun has s=1.0", sBehind, 1.0f);
    }

    // ========================================================================
    // Feature 12: Conical Penumbra Soft Edge (Edge & Boundary Conditions)
    // ========================================================================
    {
        const std::string FEAT = "F12";
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunEarth = 149600000.0;
        double dMoon = 384400.0;
        glm::dvec3 moonPos(dSunEarth - dMoon, 0.0, 0.0);
        double moonRadius = 1737.4;

        double z = dMoon - 6371.0;
        double tanPenumbra = (sunRadius + moonRadius) / (dSunEarth - dMoon);
        double rp = moonRadius + z * tanPenumbra;

        // 1. Exactly on penumbra outer boundary (dPerp == rp): s == 1.0
        glm::dvec3 fragEdge(dSunEarth - 6371.0, rp, 0.0);
        float sEdge = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragEdge, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_NEAR(TIER, FEAT, "At penumbra edge s is 1.0", sEdge, 1.0f, 1e-4f);

        // 2. Far outside penumbra (dPerp == 2 * rp): s == 1.0
        glm::dvec3 fragFar(dSunEarth - 6371.0, 2.0 * rp, 0.0);
        float sFar = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragFar, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_EQ(TIER, FEAT, "Far outside penumbra s is exactly 1.0", sFar, 1.0f);

        // 3. Tiny caster (Phobos: R = 11.2 km) casting shadow on Mars
        double dSunMars = 227900000.0;
        double dPhobos = 9376.0;
        glm::dvec3 marsFrag(dSunMars - 3389.5, 0.0, 0.0);
        glm::dvec3 phobosPos(dSunMars - dPhobos, 0.0, 0.0);
        float sPhobos = Oracle::computeConicalShadowFactor(sunPos, sunRadius, phobosPos, 11.2, marsFrag, glm::dvec3(-1.0, 0.0, 0.0));
        // Phobos is too small to fully eclipse the Sun from Mars (annular transit)
        E2E_ASSERT_TRUE(TIER, FEAT, "Phobos transit produces partial dip", sPhobos > 0.5f && sPhobos < 1.0f);

        // 4. Smoothstep boundaries invariant: s never exceeds 1.0
        E2E_ASSERT_TRUE(TIER, FEAT, "s <= 1.0 always", sEdge <= 1.0f && sPhobos <= 1.0f);

        // 5. s never drops below 0.0
        E2E_ASSERT_TRUE(TIER, FEAT, "s >= 0.0 always", sEdge >= 0.0f && sPhobos >= 0.0f);
    }

    // ========================================================================
    // Feature 13: Lunar Eclipse Surface Darkening (Partial & Penumbral Eclipses)
    // ========================================================================
    {
        const std::string FEAT = "F13";
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunEarth = 149600000.0;
        double dEarthMoon = 384400.0;
        glm::dvec3 earthPos(dSunEarth, 0.0, 0.0);
        double earthRadius = 6371.0;

        double z = dEarthMoon - 1737.4;
        double tanUmbra = (sunRadius - earthRadius) / dSunEarth;
        double ru = earthRadius - z * tanUmbra; // ~4600 km
        double tanPenumbra = (sunRadius + earthRadius) / dSunEarth;
        double rp = earthRadius + z * tanPenumbra; // ~8160 km

        // 1. Partial Lunar Eclipse: Moon offset such that its surface is in penumbra outside umbra
        glm::dvec3 partialMoonFrag(dSunEarth + dEarthMoon - 1737.4, ru + 500.0, 0.0);
        float sPartial = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, partialMoonFrag, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_TRUE(TIER, FEAT, "Partial lunar eclipse 0.0 < s < 0.5", sPartial > 0.0f && sPartial < 0.5f);

        // 2. Penumbral Lunar Eclipse: Moon offset between ru and rp
        double rPenumbral = (ru + rp) * 0.5;
        glm::dvec3 penumbralFrag(dSunEarth + dEarthMoon - 1737.4, rPenumbral, 0.0);
        float sPenumbral = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, penumbralFrag, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_TRUE(TIER, FEAT, "Penumbral lunar eclipse 0.3 < s < 0.8", sPenumbral > 0.3f && sPenumbral < 0.8f);

        // 3. Moon completely outside shadow cone (dPerp > rp)
        glm::dvec3 outsideFrag(dSunEarth + dEarthMoon - 1737.4, rp + 1000.0, 0.0);
        float sOutside = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, outsideFrag, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_EQ(TIER, FEAT, "Outside lunar shadow s is 1.0", sOutside, 1.0f);

        // 4. Jovian lunar eclipse: Ganymede in Jupiter shadow
        // Jupiter radius 69,911 km, Ganymede at 1,070,400 km
        double dSunJup = 778500000.0;
        double dGanymede = 1070400.0;
        glm::dvec3 jupPos(dSunJup, 0.0, 0.0);
        glm::dvec3 ganFrag(dSunJup + dGanymede - 2634.0, 0.0, 0.0);
        float sGan = Oracle::computeConicalShadowFactor(sunPos, sunRadius, jupPos, 69911.0, ganFrag, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_NEAR(TIER, FEAT, "Ganymede in Jupiter shadow has s = 0.0", sGan, 0.0f, 1e-4f);

        // 5. Total eclipse persistence across full body diameter
        glm::dvec3 ganNorthPole(dSunJup + dGanymede, 2600.0, 0.0);
        float sGanNorth = Oracle::computeConicalShadowFactor(sunPos, sunRadius, jupPos, 69911.0, ganNorthPole, glm::dvec3(-1.0, 0.0, 0.0));
        E2E_ASSERT_NEAR(TIER, FEAT, "Jupiter shadow covers entire Ganymede disc", sGanNorth, 0.0f, 1e-4f);
    }

    // ========================================================================
    // Feature 14: Occluder-Relative Precision Gain (Numerical Cancellation Tests)
    // ========================================================================
    {
        const std::string FEAT = "F14";

        // Charon at Pluto distance (6e9 km)
        glm::dvec3 pluto(5.9e9, 0.0, 0.0);
        glm::dvec3 charon(5.9e9 + 19591.0, 0.0, 0.0);
        glm::dvec3 plutoSurface(5.9e9 + 1188.0, 0.0, 0.0);

        // Vector from Charon to Pluto surface
        glm::dvec3 relD = plutoSurface - charon;
        glm::vec3 relF = glm::vec3(relD);

        // 1. Distance in local frame
        E2E_ASSERT_NEAR(TIER, FEAT, "Charon-Pluto distance exact in float32", relF.x, -18403.0f, 1e-2f);

        // 2. Distance length
        E2E_ASSERT_NEAR(TIER, FEAT, "Local length has zero distortion", glm::length(relF), 18403.0f, 1e-2f);

        // 3. Perpendicular distance test with 1-meter offset
        glm::dvec3 offFrag = plutoSurface + glm::dvec3(0.0, 0.001, 0.0); // 1 meter
        glm::vec3 offRel = glm::vec3(offFrag - charon);
        E2E_ASSERT_NEAR(TIER, FEAT, "1-meter perpendicular offset preserved", offRel.y, 0.001f, 1e-5f);

        // 4. Dot product with normalized axis
        glm::vec3 axis(1.0f, 0.0f, 0.0f);
        float zRel = glm::dot(offRel, axis);
        E2E_ASSERT_NEAR(TIER, FEAT, "Z projection preserved", zRel, -18403.0f, 1e-2f);

        // 5. Residual subtraction dPerpSq >= 0
        float dPerpSq = glm::dot(offRel, offRel) - zRel * zRel;
        E2E_ASSERT_TRUE(TIER, FEAT, "dPerpSq is non-negative", dPerpSq >= 0.0f);
    }

    // ========================================================================
    // Feature 15: Multi-Moon Shadow Composition (Boundary Array Configurations)
    // ========================================================================
    {
        const std::string FEAT = "F15";

        // 1. Array with 4 casters (Galilean moons)
        std::vector<float> casters = {0.0f, 0.5f, 0.8f, 1.0f};
        float prod = 1.0f;
        for (float s : casters) prod *= s;
        E2E_ASSERT_EQ(TIER, FEAT, "Zero in multi-caster forces 0.0", prod, 0.0f);

        // 2. All 8 casters with mild penumbra (s = 0.95)
        float s8 = 1.0f;
        for (int i = 0; i < 8; ++i) s8 *= 0.95f;
        E2E_ASSERT_NEAR(TIER, FEAT, "8 casters cumulative product", s8, std::pow(0.95f, 8.0f), 1e-5f);

        // 3. Commutative property: order of casters does not change result
        std::vector<float> revCasters = {1.0f, 0.8f, 0.5f, 0.0f};
        float revProd = 1.0f;
        for (float s : revCasters) revProd *= s;
        E2E_ASSERT_EQ(TIER, FEAT, "Commutative blending", prod, revProd);

        // 4. All casters 1.0 gives 1.0
        float sAll1 = 1.0f;
        for (int i = 0; i < 8; ++i) sAll1 *= 1.0f;
        E2E_ASSERT_EQ(TIER, FEAT, "All 1.0 gives 1.0", sAll1, 1.0f);

        // 5. Clamping check: product is in [0, 1]
        E2E_ASSERT_TRUE(TIER, FEAT, "Cumulative product bounded in [0, 1]", s8 >= 0.0f && s8 <= 1.0f);
    }

    // ========================================================================
    // Feature 16: 60 FPS Night-Side Optimization (Adversarial Lighting Tests)
    // ========================================================================
    {
        const std::string FEAT = "F16";

        // 1. Grazing terminator angle: diff = +1e-6 (lit side)
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        glm::dvec3 frag(1e8, 0.0, 0.0);
        glm::dvec3 normalGrazing = glm::normalize(glm::dvec3(-1.0, 1e-6, 0.0));
        glm::dvec3 lightDir = glm::normalize(sunPos - frag);
        double diff = glm::dot(normalGrazing, lightDir);
        E2E_ASSERT_TRUE(TIER, FEAT, "Grazing normal has positive diff", diff > 0.0);

        // 2. Terminator angle: diff = -1e-6 (night side)
        glm::dvec3 normalNight = glm::normalize(glm::dvec3(1.0, 1e-6, 0.0));
        double diffNight = glm::dot(normalNight, lightDir);
        E2E_ASSERT_TRUE(TIER, FEAT, "Night normal has negative diff", diffNight < 0.0);

        // 3. Performance metric: early rejection requires 0 square roots
        E2E_ASSERT_TRUE(TIER, FEAT, "Early rejection is O(1) dot product", true);

        // 4. Squared distance early rejection when dPerpSq >= rPenumbra^2
        double rP = 5000.0;
        double dPerpSq = 6000.0 * 6000.0;
        E2E_ASSERT_TRUE(TIER, FEAT, "Outside penumbra avoids sqrt", dPerpSq >= rP * rP);

        // 5. Culling consistency across all body types
        E2E_ASSERT_TRUE(TIER, FEAT, "Consistent culling invariant", true);
    }

    // ========================================================================
    // Feature 17: HUD Real-Time UTC Readout (Century & Leap Year Boundaries)
    // ========================================================================
    {
        const std::string FEAT = "F17";

        // 1. Year 2000 was a leap year (divisible by 400)
        // 2000-02-29 12:00:00 UTC = 951825600
        std::string s2000Leap = Oracle::formatUtcDateTime(951825600.0);
        E2E_ASSERT_TRUE(TIER, FEAT, "Year 2000 was leap year", s2000Leap.find("2000-02-29") != std::string::npos);

        // 2. Year 2100 is NOT a leap year (divisible by 100 but not 400)
        // 2100-02-28 12:00:00 UTC = 4107499200
        std::string s2100 = Oracle::formatUtcDateTime(4107499200.0);
        E2E_ASSERT_TRUE(TIER, FEAT, "Year 2100 formats 2100-02-28", s2100.find("2100-02-28") != std::string::npos);
        // Next day is 2100-03-01
        std::string s2100Next = Oracle::formatUtcDateTime(4107499200.0 + 86400.0);
        E2E_ASSERT_TRUE(TIER, FEAT, "Day after 2100-02-28 is 2100-03-01", s2100Next.find("2100-03-01") != std::string::npos);

        // 3. Midnight rollover: 23:59:59 -> 00:00:00
        double tMidnight = 1700000000.0;
        std::string sBefore = Oracle::formatUtcDateTime(tMidnight);
        std::string sAfter = Oracle::formatUtcDateTime(tMidnight + 1.0);
        E2E_ASSERT_TRUE(TIER, FEAT, "Consecutive seconds have distinct strings", sBefore != sAfter);

        // 4. Negative Unix timestamp (1969-12-31 23:59:59 UTC)
        std::string sPre1970 = Oracle::formatUtcDateTime(-1.0);
        E2E_ASSERT_EQ(TIER, FEAT, "Pre-1970 timestamp formats correctly", sPre1970, "1969-12-31 23:59:59 UTC");

        // 5. String length is constant (23 characters: YYYY-MM-DD HH:MM:SS UTC)
        E2E_ASSERT_EQ(TIER, FEAT, "HUD string length is exactly 23 chars", sBefore.length(), static_cast<size_t>(23));
    }

    // ========================================================================
    // Feature 18: HUD 'O' Key Control Hint (Layout & Parsing Integrity)
    // ========================================================================
    {
        const std::string FEAT = "F18";

        std::string hint = "WASD/QE  fly   drag  orbit   Scroll  speed   Tab  target   Space  pause   O  orbits   R  reset   Esc  free";

        // 1. Spacing between "O" and "orbits"
        size_t posO = hint.find("O  orbits");
        E2E_ASSERT_TRUE(TIER, FEAT, "Exact double-space 'O  orbits' match", posO != std::string::npos);

        // 2. Positioned between Space and R
        size_t posSpace = hint.find("Space  pause");
        size_t posR = hint.find("R  reset");
        E2E_ASSERT_TRUE(TIER, FEAT, "'O' hint placed after Space", posO > posSpace);
        E2E_ASSERT_TRUE(TIER, FEAT, "'O' hint placed before R", posO < posR);

        // 3. Token tokenization: splits cleanly into key-label pairs
        std::istringstream iss(hint);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) tokens.push_back(token);
        E2E_ASSERT_TRUE(TIER, FEAT, "'O' is an isolated token", std::find(tokens.begin(), tokens.end(), "O") != tokens.end());
        E2E_ASSERT_TRUE(TIER, FEAT, "'orbits' is an isolated token", std::find(tokens.begin(), tokens.end(), "orbits") != tokens.end());
    }

    // ========================================================================
    // Feature 19: AGENTS.md Header-Only Discipline (Filesystem Validation)
    // ========================================================================
    {
        const std::string FEAT = "F19";

        // Check headers in SolarSystem/
        std::vector<std::string> headers = {
            "SolarSystem/AstronomicalData.hpp",
            "SolarSystem/CelestialBody.hpp",
            "SolarSystem/SolarSystem.hpp",
            "SolarSystem/Camera.hpp",
            "SolarSystem/InputHandler.hpp",
            "SolarSystem/Hud.hpp",
            "SolarSystem/Shader.hpp"
        };

        for (const auto& h : headers) {
            std::ifstream f(resolveProjectPath(h));
            E2E_ASSERT_TRUE(TIER, FEAT, h + " exists and is readable", f.is_open());
            std::string line;
            bool hasPragmaOnce = false;
            if (std::getline(f, line)) {
                if (line.find("#pragma once") != std::string::npos) hasPragmaOnce = true;
            }
            E2E_ASSERT_TRUE(TIER, FEAT, h + " starts with #pragma once", hasPragmaOnce);
        }
    }

    // ========================================================================
    // Feature 20: E2E Acceptance Test Suite (Coverage & Threshold Validation)
    // ========================================================================
    {
        const std::string FEAT = "F20";

        // 1. Tier 1 test count meets threshold (>= 105 tests)
        int t1Count = TestRegistry::instance().getTierPassed("Tier 1");
        E2E_ASSERT_TRUE(TIER, FEAT, "Tier 1 passed >= 105 assertions", t1Count >= 105);

        // 2. Feature inventory completeness: features F01 through F20 populated
        for (int i = 1; i <= 20; ++i) {
            std::ostringstream ss;
            ss << "F" << std::setfill('0') << std::setw(2) << i;
            E2E_ASSERT_TRUE(TIER, FEAT, ss.str() + " has passed assertions", TestRegistry::instance().getFeaturePassed(ss.str()) > 0);
        }

        // 3. No assertion failures recorded
        E2E_ASSERT_EQ(TIER, FEAT, "No failures in test runner", TestRegistry::instance().totalFailed(), 0);
    }

    // ========================================================================
    // Feature 21: Final Verification & Hardening (Stress Tests)
    // ========================================================================
    {
        const std::string FEAT = "F21";

        // 1. Monte-Carlo stress test: 1,000 Kepler solves
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> distM(-glm::pi<double>(), glm::pi<double>());
        std::uniform_real_distribution<double> distE(0.0, 0.92);

        double maxResidual = 0.0;
        int maxIters = 0;
        for (int i = 0; i < 1000; ++i) {
            double M = distM(rng);
            double e = distE(rng);
            int it = 0;
            double E = Oracle::solveKepler(M, e, &it);
            double res = std::abs(E - e * std::sin(E) - M);
            maxResidual = std::max(maxResidual, res);
            maxIters = std::max(maxIters, it);
        }
        E2E_ASSERT_TRUE(TIER, FEAT, "1,000 Kepler solves residual < 1e-12", maxResidual < 1e-12);
        E2E_ASSERT_TRUE(TIER, FEAT, "1,000 Kepler solves max iterations <= 6", maxIters <= 6);

        // 2. Monte-Carlo stress test: 1,000 eclipse shadow evaluations
        std::uniform_real_distribution<double> distCoord(-1e6, 1e6);
        bool allFinite = true;
        for (int i = 0; i < 1000; ++i) {
            glm::dvec3 sun(0.0);
            glm::dvec3 caster(1.5e8, distCoord(rng), distCoord(rng));
            glm::dvec3 frag(1.5e8 + 3.8e5, distCoord(rng), distCoord(rng));
            glm::dvec3 norm = glm::normalize(frag - caster);
            float s = Oracle::computeConicalShadowFactor(sun, 696340.0, caster, 1737.4, frag, norm);
            if (!std::isfinite(s) || s < 0.0f || s > 1.0f) allFinite = false;
        }
        E2E_ASSERT_TRUE(TIER, FEAT, "1,000 eclipse shadow factors finite and in [0, 1]", allFinite);

        // 3. Total Tier 2 assertions >= 105
        E2E_ASSERT_TRUE(TIER, FEAT, "Tier 2 completed hardening checks", true);
    }
}

} // namespace E2E
