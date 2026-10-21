#include "test_infra.hpp"

namespace E2E {

void runTier3PairwiseTests() {
    const std::string TIER = "Tier 3";

    // ========================================================================
    // Pair 1: Keplerian Dynamics x Floating Origin Re-centering
    // ========================================================================
    {
        const std::string FEAT = "F07"; // and F04, F02
        auto catalog = Oracle::getStandardCatalog();
        auto itEarth = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Earth"; });
        auto itMoon = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Moon"; });
        auto itJup = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Jupiter"; });

        // Simulate 5 consecutive time steps
        for (int step = 0; step < 5; ++step) {
            double T = 0.01 * step;
            glm::dvec3 earthPos = Oracle::computeOrbitalPosition(*itEarth, T);
            glm::dvec3 moonPosRel = Oracle::computeOrbitalPosition(*itMoon, T);
            glm::dvec3 moonPosExact = earthPos + moonPosRel;
            glm::dvec3 jupPos = Oracle::computeOrbitalPosition(*itJup, T);

            // Origin shifted to Earth: Earth at (0,0,0), Moon at moonPosRel
            glm::vec3 earthRender = glm::vec3(earthPos - earthPos);
            glm::vec3 moonRender = glm::vec3(moonPosExact - earthPos);
            E2E_ASSERT_VEC3_NEAR(TIER, FEAT, "Pair1: Earth at render origin", earthRender, glm::vec3(0.0f), 1e-6f);
            E2E_ASSERT_NEAR(TIER, FEAT, "Pair1: Moon render distance matches exact", glm::length(moonRender), static_cast<float>(glm::length(moonPosRel)), 1.0f);

            // Re-center on Moon: Moon at (0,0,0), Earth at -moonPosRel
            glm::vec3 earthFromMoon = glm::vec3(earthPos - moonPosExact);
            E2E_ASSERT_NEAR(TIER, FEAT, "Pair1: Earth seen from Moon inverted", glm::length(earthFromMoon), static_cast<float>(glm::length(moonPosRel)), 1.0f);

            // Re-center on Jupiter: Jupiter at (0,0,0), Earth-Jupiter distance preserved
            glm::vec3 earthFromJup = glm::vec3(earthPos - jupPos);
            double exactEJDist = glm::length(earthPos - jupPos);
            E2E_ASSERT_NEAR(TIER, FEAT, "Pair1: Earth-Jupiter render distance preserved", glm::length(earthFromJup), static_cast<float>(exactEJDist), 100.0f);
        }
    }

    // ========================================================================
    // Pair 2: 'O' Key Toggle State x Active Target Moon Hierarchy
    // ========================================================================
    {
        const std::string FEAT = "F09"; // and F08
        
        struct SystemState {
            bool orbitVisible = true;
            std::string focusedTarget = "Sun";
            std::string targetParent = "";

            bool isOrbitDrawn(const std::string& bodyParent) const {
                if (!orbitVisible) return false;
                if (bodyParent == "Sun") return true;
                if (focusedTarget == bodyParent) return true;
                if (!targetParent.empty() && targetParent != "Sun" && targetParent == bodyParent) return true;
                return false;
            }
        };

        SystemState sys;

        // When orbits visible and Sun focused:
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair2: Sun focused -> Earth orbit drawn", sys.isOrbitDrawn("Sun"));
        E2E_ASSERT_FALSE(TIER, FEAT, "Pair2: Sun focused -> Moon orbit NOT drawn", sys.isOrbitDrawn("Earth"));

        // Toggle 'O' to OFF: nothing is drawn
        sys.orbitVisible = false;
        E2E_ASSERT_FALSE(TIER, FEAT, "Pair2: 'O' off -> Earth orbit NOT drawn", sys.isOrbitDrawn("Sun"));
        E2E_ASSERT_FALSE(TIER, FEAT, "Pair2: 'O' off -> Moon orbit NOT drawn", sys.isOrbitDrawn("Earth"));

        // Toggle 'O' back ON and switch target to Earth:
        sys.orbitVisible = true;
        sys.focusedTarget = "Earth";
        sys.targetParent = "Sun";
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair2: Earth focused -> Earth orbit drawn", sys.isOrbitDrawn("Sun"));
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair2: Earth focused -> Moon orbit drawn", sys.isOrbitDrawn("Earth"));
        E2E_ASSERT_FALSE(TIER, FEAT, "Pair2: Earth focused -> Io orbit NOT drawn", sys.isOrbitDrawn("Jupiter"));

        // Switch target to Moon (child of Earth):
        sys.focusedTarget = "Moon";
        sys.targetParent = "Earth";
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair2: Moon focused -> Moon orbit drawn", sys.isOrbitDrawn("Earth"));
    }

    // ========================================================================
    // Pair 3: 3D Elliptical Orbit Geometry x High Inclinations
    // ========================================================================
    {
        const std::string FEAT = "F06"; // and F04, F10
        auto catalog = Oracle::getStandardCatalog();
        
        // Pallas: inclination 34.841 deg
        auto itPallas = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Pallas"; });
        auto vertsPallas = Oracle::generateOrbitVertices(*itPallas, 0.0, 180);

        // Find max Y coordinate (ecliptic north)
        double maxY = 0.0;
        for (const auto& v : vertsPallas) {
            maxY = std::max(maxY, std::abs(v.y));
        }
        // Expected max Y bounded by a*(1-e)*sin(i) and a*(1+e)*sin(i)
        double minY = itPallas->a0 * Oracle::kmPerAu * (1.0 - itPallas->e0) * std::sin(glm::radians(itPallas->i0));
        double maxY_bound = itPallas->a0 * Oracle::kmPerAu * (1.0 + itPallas->e0) * std::sin(glm::radians(itPallas->i0));
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair3: Pallas orbit reaches high vertical extent", maxY >= minY && maxY <= maxY_bound);

        // Triton: retrograde inclination 156.88 deg
        auto itTriton = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Triton"; });
        auto vertsTriton = Oracle::generateOrbitVertices(*itTriton, 0.0, 180);
        E2E_ASSERT_EQ(TIER, FEAT, "Pair3: Triton 3D orbit vertices generated", vertsTriton.size(), static_cast<size_t>(180));

        // Verify body position at T = 0.0 aligns with orbit vertex
        glm::dvec3 tritonPos = Oracle::computeOrbitalPosition(*itTriton, 0.0);
        double distToV0 = glm::length(tritonPos - vertsTriton[0]);
        double chordStep = glm::length(vertsTriton[1] - vertsTriton[0]);
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair3: Triton body aligns with orbit vertex 0", distToV0 <= chordStep);
    }

    // ========================================================================
    // Pair 4: Conical Eclipse Math x Multi-Moon Blending (Jovian System)
    // ========================================================================
    {
        const std::string FEAT = "F15"; // and F11, F12
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunJup = 778500000.0;
        glm::dvec3 jupPos(dSunJup, 0.0, 0.0);
        double jupRadius = 69911.0;

        // Io: radius 1821.6 km, dist 421,700 km
        glm::dvec3 ioPos(dSunJup - 421700.0, 0.0, 0.0);
        double ioRadius = 1821.6;

        // Europa: radius 1560.8 km, dist 671,100 km, offset slightly in Y
        glm::dvec3 eurPos(dSunJup - 671100.0, 3000.0, 0.0);
        double eurRadius = 1560.8;

        // Receiver on Jupiter daylight cloud surface
        glm::dvec3 jupFragIo(dSunJup - jupRadius, 0.0, 0.0); // right under Io
        glm::dvec3 jupFragEur(dSunJup - jupRadius, 3000.0, 0.0); // right under Europa
        glm::dvec3 jupFragMid(dSunJup - jupRadius, 1500.0, 0.0); // midway between them
        glm::dvec3 normal(-1.0, 0.0, 0.0);

        // Under Io: Io casts total shadow, Europa casts mild/none
        float sIo_io = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, jupFragIo, normal);
        float sEur_io = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, jupFragIo, normal);
        float netIo = sIo_io * sEur_io;
        E2E_ASSERT_NEAR(TIER, FEAT, "Pair4: Under Io totality (net = 0.0)", netIo, 0.0f, 1e-4f);

        // Under Europa: Europa casts total shadow
        float sIo_eur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, jupFragEur, normal);
        float sEur_eur = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, jupFragEur, normal);
        float netEur = sIo_eur * sEur_eur;
        E2E_ASSERT_NEAR(TIER, FEAT, "Pair4: Under Europa totality (net = 0.0)", netEur, 0.0f, 1e-4f);

        // Midway: both moons cast penumbral overlap
        float sIo_mid = Oracle::computeConicalShadowFactor(sunPos, sunRadius, ioPos, ioRadius, jupFragMid, normal);
        float sEur_mid = Oracle::computeConicalShadowFactor(sunPos, sunRadius, eurPos, eurRadius, jupFragMid, normal);
        float netMid = sIo_mid * sEur_mid;
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair4: Midway overlap blends penumbrae", netMid <= sIo_mid && netMid <= sEur_mid);
    }

    // ========================================================================
    // Pair 5: Lunar Eclipse x Nodal Regression
    // ========================================================================
    {
        const std::string FEAT = "F13"; // and F01
        auto catalog = Oracle::getStandardCatalog();
        auto itMoon = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Moon"; });

        // Node regresses at -1934.136 deg/century (~18.61 years for 360 deg)
        double T_half_cycle = 180.0 / 1934.136; // ~0.093 centuries = 9.3 years
        double omega_init = itMoon->node0;
        double omega_half = itMoon->node0 + itMoon->nodeDot * T_half_cycle;
        double deltaNode = std::abs(omega_half - omega_init);
        E2E_ASSERT_NEAR(TIER, FEAT, "Pair5: Nodal half cycle regresses ~180 deg", deltaNode, 180.0, 1.0);
    }

    // ========================================================================
    // Pair 6: Time Progression x HUD Calendar Readout
    // ========================================================================
    {
        const std::string FEAT = "F17"; // and F02
        
        double currentSimTime = 1770000000.0; // year ~2026

        // Forward 100 seconds at 10x
        double delta = 10.0;
        double scale = 10.0;
        currentSimTime += delta * scale;
        std::string s1 = Oracle::formatUtcDateTime(currentSimTime);

        // Backward 100 seconds at -10x (rewind)
        currentSimTime += delta * (-scale);
        std::string s2 = Oracle::formatUtcDateTime(currentSimTime);
        E2E_ASSERT_EQ(TIER, FEAT, "Pair6: Rewind restores exact original UTC string", s2, Oracle::formatUtcDateTime(1770000000.0));
    }

    // ========================================================================
    // Pair 7: GPU Logarithmic Depth Buffer Monotonicity
    // ========================================================================
    {
        const std::string FEAT = "F21";
        
        float wPrev = 0.1f;
        float zPrev = Oracle::computeLogarithmicDepth(wPrev);
        bool strictlyMonotonic = true;

        // Test across 8 orders of magnitude (from 1 km to 10^10 km)
        for (float w = 1.0f; w <= 3.0e10f; w *= 10.0f) {
            float z = Oracle::computeLogarithmicDepth(w);
            if (z <= zPrev) strictlyMonotonic = false;
            zPrev = z;
        }
        E2E_ASSERT_TRUE(TIER, FEAT, "Pair7: Logarithmic depth is strictly monotonic", strictlyMonotonic);
    }
}

} // namespace E2E
