#include "test_infra.hpp"

namespace E2E {

void runTier4ScenarioTests() {
    const std::string TIER = "Tier 4";

    // ========================================================================
    // Scenario 1: Great American Solar Eclipse (August 21, 2017 18:26 UTC)
    // ========================================================================
    {
        const std::string FEAT = "F11"; // and F12, F14, F02
        
        // 1. Time conversion check
        // 2017-08-21 18:26:40 UTC = 1503340000 unix seconds
        double unixSec2017 = 1503340000.0;
        std::string dateStr = Oracle::formatUtcDateTime(unixSec2017);
        E2E_ASSERT_EQ(TIER, FEAT, "Scenario 1: UTC calendar string match", dateStr, "2017-08-21 18:26:40 UTC");

        // 2. Astronomical Geometry on August 21, 2017
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;

        // Earth at 1.011 AU (near aphelion in August): ~151,200,000 km
        double dSunEarth = 151200000.0;
        // Moon at 371,700 km from Earth on eclipse day
        double dEarthMoon = 371700.0;
        double dSunMoon = dSunEarth - dEarthMoon;

        glm::dvec3 moonPos(dSunMoon, 0.0, 0.0);
        double moonRadius = 1737.4;

        glm::dvec3 earthCenter(dSunEarth, 0.0, 0.0);
        double earthRadius = 6371.0;

        // Receiver point on Earth's daylight hemisphere directly under the Moon's shadow:
        glm::dvec3 centerPathFrag(dSunEarth - earthRadius, 0.0, 0.0);
        glm::dvec3 normalTowardSun(-1.0, 0.0, 0.0);

        // Umbra length calculation:
        double tanUmbra = (sunRadius - moonRadius) / dSunMoon;
        double Lu = moonRadius / tanUmbra; // ~377,200 km
        E2E_ASSERT_TRUE(TIER, FEAT, "Scenario 1: Umbra cone reaches Earth (Lu > dEarthMoon)", Lu > dEarthMoon);

        // Umbra radius on Earth surface:
        double zEarth = dEarthMoon - earthRadius; // ~365,329 km
        double ruSurface = moonRadius - zEarth * tanUmbra;
        // Spot radius ~ 55 km -> path width ~ 110 km
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 1: Totality path radius ~55 km", ruSurface, 55.0, 15.0);

        // Shadow factor at path center is 0.0 (Totality)
        float sCenter = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, centerPathFrag, normalTowardSun);
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 1: Totality shadow factor is 0.0", sCenter, 0.0f, 1e-4f);

        // Penumbra radius on Earth surface:
        double tanPenumbra = (sunRadius + moonRadius) / dSunMoon;
        double rpSurface = moonRadius + zEarth * tanPenumbra; // ~3400 km
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 1: Penumbra radius ~3400 km", rpSurface, 3400.0, 200.0);

        // Partial eclipse at 1000 km from totality line
        glm::dvec3 partialFrag(dSunEarth - earthRadius, 1000.0, 0.0);
        float sPartial = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, partialFrag, normalTowardSun);
        E2E_ASSERT_TRUE(TIER, FEAT, "Scenario 1: Partial eclipse 1000 km away (0.1 < s < 0.6)", sPartial > 0.1f && sPartial < 0.6f);
    }

    // ========================================================================
    // Scenario 2: Total Lunar Eclipse / Super Blood Moon (May 16, 2022 04:12 UTC)
    // ========================================================================
    {
        const std::string FEAT = "F13"; // and F11, F12, F02
        
        // 2022-05-16 04:12:00 UTC = 1652674320 unix seconds
        double unixSec2022 = 1652674320.0;
        std::string dateStr = Oracle::formatUtcDateTime(unixSec2022);
        E2E_ASSERT_EQ(TIER, FEAT, "Scenario 2: UTC calendar string match", dateStr, "2022-05-16 04:12:00 UTC");

        // Alignment: Sun -> Earth -> Moon
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunEarth = 151000000.0;
        double dEarthMoon = 362127.0; // Perigee Moon on May 16, 2022

        glm::dvec3 earthPos(dSunEarth, 0.0, 0.0);
        double earthRadius = 6371.0;

        glm::dvec3 moonPos(dSunEarth + dEarthMoon, 0.0, 0.0);
        double moonRadius = 1737.4;

        // Umbra radius at Moon distance:
        double tanUmbra = (sunRadius - earthRadius) / dSunEarth;
        double ruAtMoon = earthRadius - dEarthMoon * tanUmbra; // ~4717 km
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 2: Earth umbra radius at Moon ~4700 km", ruAtMoon, 4717.0, 100.0);

        // Entire Moon disc fits well inside umbra (1737.4 km < 4717 km)
        E2E_ASSERT_TRUE(TIER, FEAT, "Scenario 2: Entire Moon disc immersed in umbra", ruAtMoon > moonRadius * 2.0);

        // Point on Moon sub-Earth surface:
        glm::dvec3 moonFrag(dSunEarth + dEarthMoon - moonRadius, 0.0, 0.0);
        glm::dvec3 normalTowardEarth(-1.0, 0.0, 0.0);
        float sLunar = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, moonFrag, normalTowardEarth);
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 2: Total lunar eclipse shadow factor is 0.0", sLunar, 0.0f, 1e-4f);

        // Point at Moon limb:
        glm::dvec3 moonLimbFrag(dSunEarth + dEarthMoon, moonRadius, 0.0);
        glm::dvec3 normalTowardSun = glm::normalize(glm::dvec3(-1.0, 0.5, 0.0));
        float sLimb = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, moonLimbFrag, normalTowardSun);
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 2: Moon limb also in total eclipse", sLimb, 0.0f, 1e-4f);
    }

    // ========================================================================
    // Scenario 3: Jupiter Multi-Moon Triple Shadow Transit
    // ========================================================================
    {
        const std::string FEAT = "F15"; // and F16, F11, F12
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunJup = 778500000.0;
        glm::dvec3 jupPos(dSunJup, 0.0, 0.0);
        double jupRadius = 69911.0;

        // Three transiting Galilean moons at different longitudes/latitudes
        // Io: transiting at latitude +10 deg (~12,000 km north)
        glm::dvec3 ioPos(dSunJup - 421700.0, 12000.0, 0.0);
        double ioRadius = 1821.6;

        // Europa: transiting at equator (latitude 0 deg)
        glm::dvec3 eurPos(dSunJup - 671100.0, 0.0, 0.0);
        double eurRadius = 1560.8;

        // Ganymede: transiting at latitude -15 deg (~18,000 km south)
        glm::dvec3 ganPos(dSunJup - 1070400.0, -18000.0, 0.0);
        double ganRadius = 2634.1;

        glm::dvec3 normal(-1.0, 0.0, 0.0);

        // 1. Under Io shadow spot
        glm::dvec3 fragIo(dSunJup - jupRadius, 12000.0, 0.0);
        float sIo = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, fragIo, normal);
        float sEur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, fragIo, normal);
        float sGan = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ganPos, ganRadius, fragIo, normal);
        float netIo = sIo * sEur * sGan;
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 3: Io spot has totality", netIo, 0.0f, 1e-4f);

        // 2. Under Europa shadow spot
        glm::dvec3 fragEur(dSunJup - jupRadius, 0.0, 0.0);
        sIo = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, fragEur, normal);
        sEur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, fragEur, normal);
        sGan = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ganPos, ganRadius, fragEur, normal);
        float netEur = sIo * sEur * sGan;
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 3: Europa spot has totality", netEur, 0.0f, 1e-4f);

        // 3. Under Ganymede shadow spot
        glm::dvec3 fragGan(dSunJup - jupRadius, -18000.0, 0.0);
        sIo = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, fragGan, normal);
        sEur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, fragGan, normal);
        sGan = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ganPos, ganRadius, fragGan, normal);
        float netGan = sIo * sEur * sGan;
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 3: Ganymede spot has totality", netGan, 0.0f, 1e-4f);

        // 4. Outside all three shadow discs (e.g. at latitude +40,000 km)
        glm::dvec3 fragClear(dSunJup - jupRadius, 40000.0, 0.0);
        sIo = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, fragClear, normal);
        sEur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, fragClear, normal);
        sGan = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ganPos, ganRadius, fragClear, normal);
        float netClear = sIo * sEur * sGan;
        E2E_ASSERT_EQ(TIER, FEAT, "Scenario 3: Clear daylight band has s = 1.0", netClear, 1.0f);
    }

    // ========================================================================
    // Scenario 4: Mercury & Pluto Keplerian Perihelion / Aphelion Variation
    // ========================================================================
    {
        const std::string FEAT = "F05"; // and F03, F04
        auto catalog = Oracle::getStandardCatalog();
        
        // Mercury orbit test across 12 eccentric anomaly points
        auto itMerc = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Mercury"; });
        double a_km = itMerc->a0 * Oracle::kmPerAu;
        double e = itMerc->e0;

        for (int step = 0; step < 12; ++step) {
            double E = (step * glm::two_pi<double>()) / 12.0;
            double M = E - e * std::sin(E); // Kepler equation inverse
            double E_solved = Oracle::solveKepler(M, e);
            double eDiff = std::abs(E_solved - E);
            if (eDiff > glm::pi<double>()) eDiff = std::abs(eDiff - glm::two_pi<double>());
            E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 4: Mercury solved E matches exact", eDiff, 0.0, 1e-11);

            double r_expected = a_km * (1.0 - e * std::cos(E));
            double xPrime = a_km * (std::cos(E) - e);
            double yPrime = a_km * std::sqrt(1.0 - e * e) * std::sin(E);
            double r_cartesian = std::sqrt(xPrime * xPrime + yPrime * yPrime);
            E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 4: Orbital radius matches Keplerian formula", r_cartesian, r_expected, 1e-4);
        }

        // Pluto orbit test
        auto itPluto = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Pluto"; });
        double a_p = itPluto->a0 * Oracle::kmPerAu;
        double e_p = itPluto->e0;
        double r_peri_p = a_p * (1.0 - e_p);
        double r_ap_p = a_p * (1.0 + e_p);

        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 4: Pluto perihelion AU", r_peri_p / Oracle::kmPerAu, 29.658, 0.05);
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 4: Pluto aphelion AU", r_ap_p / Oracle::kmPerAu, 49.305, 0.05);
    }

    // ========================================================================
    // Scenario 5: Full 38-Body Trajectory Alignment & Hierarchy Tracking
    // ========================================================================
    {
        const std::string FEAT = "F10"; // and F06, F08
        auto catalog = Oracle::getStandardCatalog();
        
        int bodiesTested = 0;
        for (const auto& body : catalog) {
            double T = 0.0;
            double E_body = 0.0;
            glm::dvec3 pos = Oracle::computeOrbitalPosition(body, T, &E_body);
            // Sample orbit starting at body eccentric anomaly E_body
            auto verts = Oracle::generateOrbitVertices(body, T, 180, E_body);

            // Vertex 0 of the loop is positioned exactly at the body center (zero sagitta alignment)
            double distToV0 = glm::length(pos - verts[0]);
            E2E_ASSERT_NEAR(TIER, FEAT, body.name + " aligns with orbit line vertex 0", distToV0, 0.0, 1e-3);
            bodiesTested++;
        }
        E2E_ASSERT_EQ(TIER, FEAT, "Scenario 5: All 38 bodies verified on orbit lines", bodiesTested, 38);
    }

    // ========================================================================
    // Scenario 6: 100-Year Ephemeris & Time Drift Audit
    // ========================================================================
    {
        const std::string FEAT = "F02"; // and F17, F21
        
        // Advance 100 Julian years in 1-day steps (36,525 steps)
        double simTime = 946728000.0; // J2000.0
        double oneDay = 86400.0;
        int totalDays = 36525; // exactly 1 Julian century

        for (int day = 0; day < totalDays; ++day) {
            simTime += oneDay;
        }

        // 1. Exact Julian century elapsed: T = 1.0
        double T_end = Oracle::unixToJulianCenturies(simTime);
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 6: 100 years step yields T = 1.0", T_end, 1.0, 1e-12);

        // 2. Accumulated seconds matches exact 3,155,760,000 s
        double totalSecElapsed = simTime - 946728000.0;
        E2E_ASSERT_NEAR(TIER, FEAT, "Scenario 6: Total seconds matches Julian century", totalSecElapsed, 3155760000.0, 1e-6);

        // 3. Formatted string is exactly 2100-01-01 12:00:00 UTC
        std::string s2100 = Oracle::formatUtcDateTime(simTime);
        E2E_ASSERT_EQ(TIER, FEAT, "Scenario 6: Year 2100 calendar format match", s2100, "2100-01-01 12:00:00 UTC");

        // 4. Earth position after 100 revolutions
        auto catalog = Oracle::getStandardCatalog();
        auto itEarth = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Earth"; });
        glm::dvec3 posJ2000 = Oracle::computeOrbitalPosition(*itEarth, 0.0);
        glm::dvec3 posJ2100 = Oracle::computeOrbitalPosition(*itEarth, 1.0);
        
        // Distance difference between J2000 and J2100 is small (same orbital phase + small century precession)
        double diffDist = glm::length(posJ2100 - posJ2000);
        E2E_ASSERT_TRUE(TIER, FEAT, "Scenario 6: Earth completes 100 orbits to same quadrant", diffDist < 5.0e7);
    }
}

} // namespace E2E
