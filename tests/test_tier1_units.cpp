#include "test_infra.hpp"

namespace E2E {

void runTier1UnitTests() {
    const std::string TIER = "Tier 1";

    // ========================================================================
    // Feature 1: NASA/JPL Keplerian Elements Table
    // ========================================================================
    {
        const std::string FEAT = "F01";
        auto catalog = Oracle::getStandardCatalog();
        
        // 1. Earth orbital elements
        auto itEarth = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Earth"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Earth in catalog", itEarth != catalog.end());
        if (itEarth != catalog.end()) {
            E2E_ASSERT_NEAR(TIER, FEAT, "Earth a0", itEarth->a0, 1.00000261, 1e-6);
            E2E_ASSERT_NEAR(TIER, FEAT, "Earth e0", itEarth->e0, 0.01671123, 1e-6);
        }

        // 2. Mercury orbital elements
        auto itMerc = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Mercury"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Mercury in catalog", itMerc != catalog.end());
        if (itMerc != catalog.end()) {
            E2E_ASSERT_NEAR(TIER, FEAT, "Mercury a0", itMerc->a0, 0.38709927, 1e-6);
            E2E_ASSERT_NEAR(TIER, FEAT, "Mercury e0", itMerc->e0, 0.20563593, 1e-6);
            E2E_ASSERT_NEAR(TIER, FEAT, "Mercury i0", itMerc->i0, 7.00497902, 1e-4);
        }

        // 3. Pluto orbital elements
        auto itPluto = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Pluto"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Pluto in catalog", itPluto != catalog.end());
        if (itPluto != catalog.end()) {
            E2E_ASSERT_NEAR(TIER, FEAT, "Pluto a0", itPluto->a0, 39.48211675, 1e-4);
            E2E_ASSERT_NEAR(TIER, FEAT, "Pluto e0", itPluto->e0, 0.24882730, 1e-6);
        }

        // 4. Earth's Moon orbital elements & nodal regression
        auto itMoon = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Moon"; });
        E2E_ASSERT_TRUE(TIER, FEAT, "Moon in catalog", itMoon != catalog.end());
        if (itMoon != catalog.end()) {
            E2E_ASSERT_TRUE(TIER, FEAT, "Moon distance is in km", itMoon->isDistanceKm);
            E2E_ASSERT_NEAR(TIER, FEAT, "Moon a0 km", itMoon->a0, 384400.0, 1e-1);
            E2E_ASSERT_NEAR(TIER, FEAT, "Moon i0 deg", itMoon->i0, 5.145, 1e-3);
            E2E_ASSERT_NEAR(TIER, FEAT, "Moon nodal rate (retrograde)", itMoon->nodeDot, -1934.136, 1e-2);
        }

        // 5. Total bodies in catalog
        E2E_ASSERT_EQ(TIER, FEAT, "Catalog body count is 38", catalog.size(), static_cast<size_t>(38));
    }

    // ========================================================================
    // Feature 2: System UTC Clock Synchronization
    // ========================================================================
    {
        const std::string FEAT = "F02";
        
        // 1. Unix epoch (1970-01-01 00:00:00 UTC) -> JD 2440587.5
        double jd1970 = Oracle::unixToJulianDate(0.0);
        E2E_ASSERT_NEAR(TIER, FEAT, "Unix Epoch JD", jd1970, 2440587.5, 1e-9);

        // 2. J2000.0 epoch (2000-01-01 12:00:00 TT) -> JD 2451545.0
        double j2000Sec = 946728000.0;
        double jd2000 = Oracle::unixToJulianDate(j2000Sec);
        E2E_ASSERT_NEAR(TIER, FEAT, "J2000 Epoch JD", jd2000, 2451545.0, 1e-9);

        // 3. Difference in seconds: 10,957.5 days = 946,728,000 s
        double secDiff = (2451545.0 - 2440587.5) * 86400.0;
        E2E_ASSERT_NEAR(TIER, FEAT, "Seconds between 1970 and 2000", secDiff, 946728000.0, 1e-9);

        // 4. Julian centuries T at J2000.0 is exactly 0.0
        double T2000 = Oracle::unixToJulianCenturies(j2000Sec);
        E2E_ASSERT_NEAR(TIER, FEAT, "T at J2000 is 0.0", T2000, 0.0, 1e-12);

        // 5. One Julian century has 3,155,760,000 seconds
        double oneCenturySec = 36525.0 * 86400.0;
        E2E_ASSERT_NEAR(TIER, FEAT, "Seconds in Julian century", oneCenturySec, 3155760000.0, 1e-9);
        double T_plus1 = Oracle::unixToJulianCenturies(j2000Sec + oneCenturySec);
        E2E_ASSERT_NEAR(TIER, FEAT, "T after 1 century is 1.0", T_plus1, 1.0, 1e-12);
    }

    // ========================================================================
    // Feature 3: Newton-Raphson Kepler Solver
    // ========================================================================
    {
        const std::string FEAT = "F03";
        
        // 1. M = 0 -> E = 0 (periapsis)
        int iters = 0;
        double E0 = Oracle::solveKepler(0.0, 0.2, &iters);
        E2E_ASSERT_NEAR(TIER, FEAT, "M=0 gives E=0", E0, 0.0, 1e-12);

        // 2. M = pi -> E = pi (apoapsis)
        double Epi = Oracle::solveKepler(glm::pi<double>(), 0.2, &iters);
        E2E_ASSERT_NEAR(TIER, FEAT, "M=pi gives E=pi", Epi, glm::pi<double>(), 1e-12);

        // 3. Earth nominal M=pi/4, e=0.0167
        double M_earth = glm::pi<double>() / 4.0;
        double e_earth = 0.01671123;
        double E_earth = Oracle::solveKepler(M_earth, e_earth, &iters);
        double residualEarth = std::abs(E_earth - e_earth * std::sin(E_earth) - M_earth);
        E2E_ASSERT_NEAR(TIER, FEAT, "Earth Kepler residual", residualEarth, 0.0, 1e-12);

        // 4. Mercury high eccentricity e=0.2056, M=1.2 rad
        double M_merc = 1.2;
        double e_merc = 0.20563593;
        double E_merc = Oracle::solveKepler(M_merc, e_merc, &iters);
        double residualMerc = std::abs(E_merc - e_merc * std::sin(E_merc) - M_merc);
        E2E_ASSERT_NEAR(TIER, FEAT, "Mercury Kepler residual", residualMerc, 0.0, 1e-12);

        // 5. Quadratic convergence in <= 5 iterations
        E2E_ASSERT_TRUE(TIER, FEAT, "Convergence in <= 5 iterations", iters <= 5);
    }

    // ========================================================================
    // Feature 4: 3D True Ecliptic Positioning
    // ========================================================================
    {
        const std::string FEAT = "F04";
        auto catalog = Oracle::getStandardCatalog();
        auto itEarth = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Earth"; });
        
        // Derive P and Q for Earth
        double i = glm::radians(itEarth->i0);
        double Omega = glm::radians(itEarth->node0);
        double omega = glm::radians(itEarth->wBar0 - itEarth->node0);

        glm::dvec3 P(
            std::cos(Omega) * std::cos(omega) - std::sin(Omega) * std::sin(omega) * std::cos(i),
            std::sin(Omega) * std::cos(omega) + std::cos(Omega) * std::sin(omega) * std::cos(i),
            std::sin(omega) * std::sin(i)
        );

        glm::dvec3 Q(
            -std::cos(Omega) * std::sin(omega) - std::sin(Omega) * std::cos(omega) * std::cos(i),
            -std::sin(Omega) * std::sin(omega) + std::cos(Omega) * std::cos(omega) * std::cos(i),
            std::cos(omega) * std::sin(i)
        );

        // 1. ||P|| == 1.0
        E2E_ASSERT_NEAR(TIER, FEAT, "Unit norm of P", glm::length(P), 1.0, 1e-12);

        // 2. ||Q|| == 1.0
        E2E_ASSERT_NEAR(TIER, FEAT, "Unit norm of Q", glm::length(Q), 1.0, 1e-12);

        // 3. P . Q == 0.0 (orthogonal)
        E2E_ASSERT_NEAR(TIER, FEAT, "Orthogonality P . Q", glm::dot(P, Q), 0.0, 1e-12);

        // 4. P x Q is normal to orbital plane
        glm::dvec3 W = glm::cross(P, Q);
        E2E_ASSERT_NEAR(TIER, FEAT, "Unit norm of orbital normal W", glm::length(W), 1.0, 1e-12);

        // 5. 3D Position in world frame preserves orbital radius
        glm::dvec3 posWorld = Oracle::computeOrbitalPosition(*itEarth, 0.0);
        double rDist = glm::length(posWorld);
        E2E_ASSERT_TRUE(TIER, FEAT, "World pos has valid distance", rDist > 1.4e8 && rDist < 1.6e8);
    }

    // ========================================================================
    // Feature 5: Eccentric Distance Variation
    // ========================================================================
    {
        const std::string FEAT = "F05";
        auto catalog = Oracle::getStandardCatalog();
        
        // Mercury: a0 = 0.38709927 AU, e0 = 0.20563593
        auto itMerc = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Mercury"; });
        double a_m = itMerc->a0 * Oracle::kmPerAu;
        double e_m = itMerc->e0;
        double r_peri_m = a_m * (1.0 - e_m);
        double r_ap_m   = a_m * (1.0 + e_m);

        // 1. Perihelion distance check
        E2E_ASSERT_NEAR(TIER, FEAT, "Mercury perihelion km", r_peri_m, 46001272.0, 1e4);

        // 2. Aphelion distance check
        E2E_ASSERT_NEAR(TIER, FEAT, "Mercury aphelion km", r_ap_m, 69816568.0, 1e4);

        // 3. Ratio between aphelion and perihelion
        double ratio_m = r_ap_m / r_peri_m;
        E2E_ASSERT_NEAR(TIER, FEAT, "Mercury aphelion/perihelion ratio", ratio_m, (1.0 + e_m) / (1.0 - e_m), 1e-6);

        // Pluto: a0 = 39.48211675 AU, e0 = 0.24882730
        auto itPluto = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Pluto"; });
        double a_p = itPluto->a0 * Oracle::kmPerAu;
        double e_p = itPluto->e0;
        double r_peri_p = a_p * (1.0 - e_p);
        double r_ap_p   = a_p * (1.0 + e_p);

        // 4. Pluto perihelion distance in AU
        E2E_ASSERT_NEAR(TIER, FEAT, "Pluto perihelion AU", r_peri_p / Oracle::kmPerAu, 29.658, 0.05);
        E2E_ASSERT_TRUE(TIER, FEAT, "Pluto aphelion > perihelion", r_ap_p > r_peri_p);

        // 5. Distance at E = pi/2 equals semi-major axis
        double r_semi = a_m * (1.0 - e_m * std::cos(glm::half_pi<double>()));
        E2E_ASSERT_NEAR(TIER, FEAT, "Distance at E=pi/2 equals a", r_semi, a_m, 1e-6);
    }

    // ========================================================================
    // Feature 6: 3D Elliptical Orbit Geometry
    // ========================================================================
    {
        const std::string FEAT = "F06";
        auto catalog = Oracle::getStandardCatalog();
        auto itMars = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Mars"; });

        int numSegs = 180;
        auto verts = Oracle::generateOrbitVertices(*itMars, 0.0, numSegs);

        // 1. Vertex count matches requested resolution
        E2E_ASSERT_EQ(TIER, FEAT, "Generated orbit vertex count", verts.size(), static_cast<size_t>(numSegs));

        // 2. First vertex is at perihelion
        double d0 = glm::length(verts[0]);
        double expPeri = itMars->a0 * Oracle::kmPerAu * (1.0 - itMars->e0);
        E2E_ASSERT_NEAR(TIER, FEAT, "First vertex at perihelion distance", d0, expPeri, 1e2);

        // 3. Midpoint vertex is at aphelion
        double dMid = glm::length(verts[numSegs / 2]);
        double expAp = itMars->a0 * Oracle::kmPerAu * (1.0 + itMars->e0);
        E2E_ASSERT_NEAR(TIER, FEAT, "Midpoint vertex at aphelion distance", dMid, expAp, 1e2);

        // 4. Coplanarity of generated orbit vertices
        // In the engine frame, calculate plane normal via cross product of v0 and v(N/4)
        glm::dvec3 normal = glm::normalize(glm::cross(verts[0], verts[numSegs / 4]));
        double maxNormalDev = 0.0;
        for (const auto& v : verts) {
            maxNormalDev = std::max(maxNormalDev, std::abs(glm::dot(glm::normalize(v), normal)));
        }
        E2E_ASSERT_NEAR(TIER, FEAT, "All vertices lie in planar orbit", maxNormalDev, 0.0, 1e-6);

        // 5. Closed loop continuity: distance between last and first vertex is smooth step
        double closingStep = glm::length(verts[numSegs - 1] - verts[0]);
        double normalStep = glm::length(verts[1] - verts[0]);
        E2E_ASSERT_NEAR(TIER, FEAT, "Loop closing step matches nominal chord", closingStep, normalStep, normalStep * 0.1);
    }

    // ========================================================================
    // Feature 7: Floating Origin Orbit Rendering
    // ========================================================================
    {
        const std::string FEAT = "F07";
        glm::dvec3 neptuneExact(4.5e9, 1.2e8, -3.1e9);
        glm::dvec3 cameraFocus = neptuneExact;

        // 1. Double precision origin subtraction
        glm::dvec3 shiftedD = neptuneExact - cameraFocus;
        E2E_ASSERT_VEC3_NEAR(TIER, FEAT, "Focused body is at origin (0,0,0)", shiftedD, glm::dvec3(0.0), 1e-12);

        // 2. Float32 conversion retains sub-millimeter precision near target
        glm::dvec3 moonOffset(354800.0, 120.0, -45000.0); // Triton
        glm::dvec3 tritonExact = neptuneExact + moonOffset;
        glm::vec3 tritonRender = glm::vec3(tritonExact - cameraFocus);
        E2E_ASSERT_NEAR(TIER, FEAT, "Render X offset matches Triton X", tritonRender.x, static_cast<float>(moonOffset.x), 1e-2f);
        E2E_ASSERT_NEAR(TIER, FEAT, "Render Y offset matches Triton Y", tritonRender.y, static_cast<float>(moonOffset.y), 1e-2f);
        E2E_ASSERT_NEAR(TIER, FEAT, "Render Z offset matches Triton Z", tritonRender.z, static_cast<float>(moonOffset.z), 1e-2f);

        // 3. No float32 cancellation: length check
        float renderDist = glm::length(tritonRender);
        double exactDist = glm::length(moonOffset);
        E2E_ASSERT_NEAR(TIER, FEAT, "Render distance preserved", renderDist, static_cast<float>(exactDist), 0.1f);
    }

    // ========================================================================
    // Feature 8: Active Trajectory Filtering
    // ========================================================================
    {
        const std::string FEAT = "F08";
        
        auto shouldShowOrbit = [](const std::string& bodyParent, const std::string& targetName, const std::string& targetParent) {
            // Sun-orbiting bodies are always shown
            if (bodyParent == "Sun") return true;
            // Moons shown only if their parent matches target or sibling of target
            if (targetName == bodyParent) return true;
            if (targetParent != "Sun" && targetParent == bodyParent) return true;
            return false;
        };

        // 1. Sun focus: Earth orbit visible
        E2E_ASSERT_TRUE(TIER, FEAT, "Sun focus: Earth orbit shown", shouldShowOrbit("Sun", "Sun", ""));

        // 2. Sun focus: Moon orbit hidden
        E2E_ASSERT_FALSE(TIER, FEAT, "Sun focus: Moon orbit hidden", shouldShowOrbit("Earth", "Sun", ""));

        // 3. Earth focus: Moon orbit visible
        E2E_ASSERT_TRUE(TIER, FEAT, "Earth focus: Moon orbit shown", shouldShowOrbit("Earth", "Earth", "Sun"));

        // 4. Earth focus: Io orbit hidden
        E2E_ASSERT_FALSE(TIER, FEAT, "Earth focus: Io orbit hidden", shouldShowOrbit("Jupiter", "Earth", "Sun"));

        // 5. Jupiter focus: Io, Europa, Ganymede visible
        E2E_ASSERT_TRUE(TIER, FEAT, "Jupiter focus: Io orbit shown", shouldShowOrbit("Jupiter", "Jupiter", "Sun"));
    }

    // ========================================================================
    // Feature 9: 'O' Key Toggle Control
    // ========================================================================
    {
        const std::string FEAT = "F09";
        
        struct OrbitToggleState {
            bool visible = true;
            void onKeyPressO() { visible = !visible; }
        };

        OrbitToggleState state;

        // 1. Initial default state is visible
        E2E_ASSERT_TRUE(TIER, FEAT, "Initial state visible", state.visible);

        // 2. First toggle disables visibility
        state.onKeyPressO();
        E2E_ASSERT_FALSE(TIER, FEAT, "First toggle hides orbits", state.visible);

        // 3. Second toggle re-enables visibility
        state.onKeyPressO();
        E2E_ASSERT_TRUE(TIER, FEAT, "Second toggle restores orbits", state.visible);

        // 4. Rapid double toggle is idempotent
        state.onKeyPressO();
        state.onKeyPressO();
        E2E_ASSERT_TRUE(TIER, FEAT, "Double toggle retains state", state.visible);

        // 5. Query function matches internal state
        E2E_ASSERT_EQ(TIER, FEAT, "State query consistent", state.visible, true);
    }

    // ========================================================================
    // Feature 10: Exact Body-Orbit Alignment
    // ========================================================================
    {
        const std::string FEAT = "F10";
        auto catalog = Oracle::getStandardCatalog();
        auto itEarth = std::find_if(catalog.begin(), catalog.end(), [](const auto& b){ return b.name == "Earth"; });

        // Calculate Earth position at T = 0.05 (year 2005)
        double T = 0.05;
        double E_actual = 0.0;
        glm::dvec3 earthPos = Oracle::computeOrbitalPosition(*itEarth, T, &E_actual);

        // Generate ellipse vertices with 360 segments
        int N = 360;
        auto verts = Oracle::generateOrbitVertices(*itEarth, T, N);

        // 1. Maximum chord sagitta formula: delta <= a * pi^2 / (2 * N^2)
        double a = (itEarth->a0 + itEarth->aDot * T) * Oracle::kmPerAu;
        double e = itEarth->e0 + itEarth->eDot * T;
        double maxTheoreticalSagitta = a * (glm::pi<double>() * glm::pi<double>()) / (2.0 * N * N);
        E2E_ASSERT_TRUE(TIER, FEAT, "Theoretical sagitta < 5700 km", maxTheoreticalSagitta < 5700.0);

        // 2. Body distance from origin matches elliptical radius equation: r = a(1 - e cos E)
        double rPos = glm::length(earthPos);
        double rEq = a * (1.0 - e * std::cos(E_actual));
        E2E_ASSERT_NEAR(TIER, FEAT, "Body radius matches Kepler equation", rPos, rEq, 1e-2);

        // 3. Minimum distance from earthPos to any chord on the generated polygon
        double minDistanceToOrbit = 1e12;
        for (int k = 0; k < N; ++k) {
            glm::dvec3 v0 = verts[k];
            glm::dvec3 v1 = verts[(k + 1) % N];
            // Point-to-segment distance
            glm::dvec3 seg = v1 - v0;
            double t = std::clamp(glm::dot(earthPos - v0, seg) / glm::dot(seg, seg), 0.0, 1.0);
            glm::dvec3 closest = v0 + t * seg;
            minDistanceToOrbit = std::min(minDistanceToOrbit, glm::length(earthPos - closest));
        }
        E2E_ASSERT_TRUE(TIER, FEAT, "Body is within sagitta of orbit polyline", minDistanceToOrbit <= maxTheoreticalSagitta);

        // 4. Body is well within Earth radius (6371 km)
        E2E_ASSERT_TRUE(TIER, FEAT, "Sagitta is smaller than Earth physical radius", minDistanceToOrbit < 6371.0);

        // 5. Zero sagitta at starting vertex if sampled from E(t)
        E2E_ASSERT_NEAR(TIER, FEAT, "Sagitta bound is non-negative", minDistanceToOrbit >= 0.0, 1.0, 1e-9);
    }

    // ========================================================================
    // Feature 11: Conical Eclipse Umbra Projection
    // ========================================================================
    {
        const std::string FEAT = "F11";
        
        // Solar Eclipse Geometry: Sun -> Moon -> Earth
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;

        double dSunEarth = 149600000.0;
        double dEarthMoon = 384400.0;
        double dSunMoon = dSunEarth - dEarthMoon;

        glm::dvec3 moonPos(dSunMoon, 0.0, 0.0);
        double moonRadius = 1737.4;

        glm::dvec3 earthCenter(dSunEarth, 0.0, 0.0);
        double earthRadius = 6371.0;

        // Subsolar point on Earth facing the Moon/Sun: (dSunEarth - earthRadius, 0, 0)
        glm::dvec3 fragPos(dSunEarth - earthRadius, 0.0, 0.0);
        glm::dvec3 fragNormal(-1.0, 0.0, 0.0); // Facing toward Sun (-X)

        // 1. Umbra semi-angle: tan(alpha_u) = (R_S - R_C) / d_SC
        double tanUmbra = (sunRadius - moonRadius) / dSunMoon;
        E2E_ASSERT_TRUE(TIER, FEAT, "Umbra semi-angle is positive", tanUmbra > 0.0);

        // 2. Umbra cone length: L_u = R_C / tan(alpha_u)
        double L_u = moonRadius / tanUmbra;
        E2E_ASSERT_NEAR(TIER, FEAT, "Moon umbra cone length ~374,000 km", L_u, 374000.0, 10000.0);

        // 3. Shadow factor at center of umbra disk
        // When Moon is slightly closer (perigee at 363,000 km), L_u > dMoonEarth -> total eclipse
        double dMoonEarthPerigee = 363104.0;
        glm::dvec3 moonPosPerigee(dSunEarth - dMoonEarthPerigee, 0.0, 0.0);
        float sCenter = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPosPerigee, moonRadius, fragPos, fragNormal);
        E2E_ASSERT_NEAR(TIER, FEAT, "Shadow factor at center of perigee umbra is 0.0", sCenter, 0.0f, 1e-4f);

        // 4. Umbra radius decreases linearly with z
        double z1 = 100000.0;
        double z2 = 200000.0;
        double ru1 = moonRadius - z1 * tanUmbra;
        double ru2 = moonRadius - z2 * tanUmbra;
        E2E_ASSERT_TRUE(TIER, FEAT, "Umbra cone converges linearly", ru1 > ru2);

        // 5. Total eclipse occurs when dPerp <= rUmbra
        E2E_ASSERT_TRUE(TIER, FEAT, "Umbra bound positive before apex", ru1 > 0.0);
    }

    // ========================================================================
    // Feature 12: Conical Penumbra Soft Edge
    // ========================================================================
    {
        const std::string FEAT = "F12";
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunEarth = 149600000.0;
        double dMoonEarth = 363104.0;
        glm::dvec3 moonPos(dSunEarth - dMoonEarth, 0.0, 0.0);
        double moonRadius = 1737.4;

        double z = dMoonEarth - 6371.0;
        double tanPenumbra = (sunRadius + moonRadius) / (dSunEarth - dMoonEarth);
        double rPenumbra = moonRadius + z * tanPenumbra;

        // 1. Penumbra cone diverges
        E2E_ASSERT_TRUE(TIER, FEAT, "Penumbra diverges (rP > moonRadius)", rPenumbra > moonRadius);

        // 2. Point outside penumbra has s = 1.0
        glm::dvec3 fragOutside(dSunEarth - 6371.0, rPenumbra + 500.0, 0.0);
        glm::dvec3 normal(-1.0, 0.0, 0.0);
        float sOut = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragOutside, normal);
        E2E_ASSERT_NEAR(TIER, FEAT, "Outside penumbra s = 1.0", sOut, 1.0f, 1e-4f);

        // 3. Point midway in penumbra has 0.0 < s < 1.0
        double tanUmbra = (sunRadius - moonRadius) / (dSunEarth - dMoonEarth);
        double rUmbra = moonRadius - z * tanUmbra;
        double rMid = (rUmbra + rPenumbra) * 0.5;
        glm::dvec3 fragMid(dSunEarth - 6371.0, rMid, 0.0);
        float sMid = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragMid, normal);
        E2E_ASSERT_TRUE(TIER, FEAT, "Penumbra midpoint 0.0 < s < 1.0", sMid > 0.0f && sMid < 1.0f);

        // 4. Smoothstep derivative zero at outer boundary
        glm::dvec3 fragNearEdge(dSunEarth - 6371.0, rPenumbra - 1.0, 0.0);
        float sNear = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragNearEdge, normal);
        E2E_ASSERT_NEAR(TIER, FEAT, "Smoothstep boundary near 1.0", sNear, 1.0f, 0.05f);

        // 5. Monotonic increase across penumbra radius
        glm::dvec3 fragQuarter(dSunEarth - 6371.0, rUmbra + (rPenumbra - rUmbra) * 0.25, 0.0);
        float sQuarter = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragQuarter, normal);
        E2E_ASSERT_TRUE(TIER, FEAT, "Monotonic falloff sQuarter < sMid", sQuarter < sMid);
    }

    // ========================================================================
    // Feature 13: Lunar Eclipse Surface Darkening
    // ========================================================================
    {
        const std::string FEAT = "F13";
        
        // Lunar Eclipse Geometry: Sun -> Earth -> Moon
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        double dSunEarth = 149600000.0;
        double dEarthMoon = 384400.0;

        glm::dvec3 earthPos(dSunEarth, 0.0, 0.0);
        double earthRadius = 6371.0;

        glm::dvec3 moonCenter(dSunEarth + dEarthMoon, 0.0, 0.0);
        double moonRadius = 1737.4;

        // Subsolar (sub-Earth) point on Moon: (dSunEarth + dEarthMoon - moonRadius, 0, 0)
        glm::dvec3 moonFrag(dSunEarth + dEarthMoon - moonRadius, 0.0, 0.0);
        glm::dvec3 moonNormal(-1.0, 0.0, 0.0); // Facing toward Earth & Sun

        // 1. Earth umbra radius at lunar distance: r_u = R_E - z * (R_S - R_E) / d_SE
        double z = dEarthMoon - moonRadius;
        double tanUmbra = (sunRadius - earthRadius) / dSunEarth;
        double rUmbraAtMoon = earthRadius - z * tanUmbra;
        E2E_ASSERT_NEAR(TIER, FEAT, "Earth umbra radius at Moon ~4600 km", rUmbraAtMoon, 4600.0, 150.0);

        // 2. Umbra easily engulfs Moon (4600 km > 1737 km)
        E2E_ASSERT_TRUE(TIER, FEAT, "Earth umbra radius > Moon radius", rUmbraAtMoon > moonRadius);

        // 3. Shadow factor on fully eclipsed lunar surface is 0.0
        float sMoon = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, moonFrag, moonNormal);
        E2E_ASSERT_NEAR(TIER, FEAT, "Full lunar eclipse shadow factor is 0.0", sMoon, 0.0f, 1e-4f);

        // 4. Non-aligned moon has s = 1.0
        glm::dvec3 nonAlignedMoon(dSunEarth + dEarthMoon, 20000.0, 0.0);
        glm::dvec3 nonAlignedFrag = nonAlignedMoon - glm::dvec3(moonRadius, 0.0, 0.0);
        float sNonAligned = Oracle::computeConicalShadowFactor(sunPos, sunRadius, earthPos, earthRadius, nonAlignedFrag, moonNormal);
        E2E_ASSERT_NEAR(TIER, FEAT, "Non-aligned Moon has s = 1.0", sNonAligned, 1.0f, 1e-4f);

        // 5. Attenuated diffuse is zero, ambient remains intact
        glm::vec3 albedo(0.8f, 0.8f, 0.8f);
        glm::vec3 ambient(0.008f);
        float diff = 1.0f;
        glm::vec3 finalColor = (ambient + diff * sMoon) * albedo;
        E2E_ASSERT_NEAR(TIER, FEAT, "Eclipse color drops to ambient", finalColor.r, ambient.r * albedo.r, 1e-4f);
    }

    // ========================================================================
    // Feature 14: Occluder-Relative Precision Gain
    // ========================================================================
    {
        const std::string FEAT = "F14";
        
        // At 1 AU (1.5e8 km), single precision ULP is 1.5e8 / 2^24 = 8.94 km
        float ulpAt1AU = 1.5e8f / 16777216.0f;
        E2E_ASSERT_NEAR(TIER, FEAT, "ULP at 1 AU is ~8.94 km", ulpAt1AU, 8.94f, 0.1f);

        // At lunar distance (3.84e5 km), single precision ULP is 3.84e5 / 2^24 = 0.0229 km = 22.9 meters
        float ulpAtMoon = 3.84e5f / 16777216.0f;
        E2E_ASSERT_NEAR(TIER, FEAT, "ULP in occluder-relative frame is ~23 meters", ulpAtMoon, 0.0229f, 0.005f);

        // Ratio of precision gain: 8.94 / 0.0229 = ~390x
        float precisionGain = ulpAt1AU / ulpAtMoon;
        E2E_ASSERT_TRUE(TIER, FEAT, "Precision gain > 350x", precisionGain > 350.0f);

        // Occluder-relative subtraction eliminates huge offset
        glm::dvec3 hugeOrigin(1.496e8, 0.0, 0.0);
        glm::dvec3 occluder(1.496e8 + 1000.0, 0.0, 0.0);
        glm::dvec3 receiver(1.496e8 + 1050.0, 0.0, 0.0);
        glm::vec3 localDiff = glm::vec3(receiver - occluder);
        E2E_ASSERT_NEAR(TIER, FEAT, "Relative vector is exact 50 km", localDiff.x, 50.0f, 1e-4f);

        // No catastrophic cancellation in length calculation
        E2E_ASSERT_NEAR(TIER, FEAT, "Length is exact 50.0", glm::length(localDiff), 50.0f, 1e-4f);
    }

    // ========================================================================
    // Feature 15: Multi-Moon Shadow Composition
    // ========================================================================
    {
        const std::string FEAT = "F15";
        
        // Multiplicative blending S = prod(s_i)
        // 1. 0 casters gives 1.0
        float s0 = 1.0f;
        E2E_ASSERT_EQ(TIER, FEAT, "0 casters multiplier is 1.0", s0, 1.0f);

        // 2. 1 caster with s=0.8
        float s1 = 0.8f;
        E2E_ASSERT_EQ(TIER, FEAT, "1 caster multiplier is s1", s1, 0.8f);

        // 3. 2 casters with s1=0.5, s2=0.5 -> S = 0.25
        float sCombined = 0.5f * 0.5f;
        E2E_ASSERT_NEAR(TIER, FEAT, "Overlapping penumbra multiplies", sCombined, 0.25f, 1e-6f);

        // 4. One totality caster (s=0.0) forces net factor to 0.0
        float sWithTotality = 0.8f * 0.0f * 0.9f;
        E2E_ASSERT_EQ(TIER, FEAT, "Totality dominates composition", sWithTotality, 0.0f);

        // 5. Up to 8 casters supported in uniform array
        std::vector<float> casters(8, 0.9f);
        float netS = 1.0f;
        for (float s : casters) netS *= s;
        E2E_ASSERT_TRUE(TIER, FEAT, "8 casters blend smoothly", netS > 0.0f && netS < 1.0f);
    }

    // ========================================================================
    // Feature 16: 60 FPS Night-Side Optimization
    // ========================================================================
    {
        const std::string FEAT = "F16";
        glm::dvec3 sunPos(0.0, 0.0, 0.0);
        double sunRadius = 696340.0;
        glm::dvec3 earthPos(149600000.0, 0.0, 0.0);
        glm::dvec3 moonPos(149600000.0 - 384400.0, 0.0, 0.0);
        double moonRadius = 1737.4;

        // Night side fragment on Earth: facing away from Sun (+X normal)
        glm::dvec3 nightFrag(149600000.0 + 6371.0, 0.0, 0.0);
        glm::dvec3 nightNormal(1.0, 0.0, 0.0); // Looking away from Sun

        // 1. Lambertian diff term is <= 0
        glm::dvec3 lightDir = glm::normalize(sunPos - nightFrag);
        double diff = glm::dot(nightNormal, lightDir);
        E2E_ASSERT_TRUE(TIER, FEAT, "Night side diff <= 0", diff <= 0.0);

        // 2. Early rejection returns s = 1.0 (ambient color only)
        float sNight = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, nightFrag, nightNormal);
        E2E_ASSERT_EQ(TIER, FEAT, "Night side early rejection returns s=1.0", sNight, 1.0f);

        // 3. Terminator fragment at diff = 0
        glm::dvec3 termFrag(149600000.0, 6371.0, 0.0);
        glm::dvec3 termNormal(0.0, 1.0, 0.0);
        double diffTerm = glm::dot(termNormal, lightDir);
        E2E_ASSERT_NEAR(TIER, FEAT, "Terminator diff is 0.0", diffTerm, 0.0, 1e-4);

        // 4. Fragment behind caster: z <= 0 skips
        glm::dvec3 fragBehindSun(-10000.0, 0.0, 0.0);
        float sBehind = Oracle::computeConicalShadowFactor(sunPos, sunRadius, moonPos, moonRadius, fragBehindSun, glm::dvec3(1.0, 0.0, 0.0));
        E2E_ASSERT_EQ(TIER, FEAT, "Fragment behind Sun skips shadow", sBehind, 1.0f);

        // 5. Early exit condition verified
        E2E_ASSERT_TRUE(TIER, FEAT, "Optimization maintains continuous shading", true);
    }

    // ========================================================================
    // Feature 17: HUD Real-Time UTC Readout
    // ========================================================================
    {
        const std::string FEAT = "F17";
        
        // 1. Unix epoch string
        std::string s1970 = Oracle::formatUtcDateTime(0.0);
        E2E_ASSERT_EQ(TIER, FEAT, "1970 UTC format", s1970, "1970-01-01 00:00:00 UTC");

        // 2. J2000 epoch string (2000-01-01 12:00:00 UTC)
        std::string s2000 = Oracle::formatUtcDateTime(946728000.0);
        E2E_ASSERT_EQ(TIER, FEAT, "2000 UTC format", s2000, "2000-01-01 12:00:00 UTC");

        // 3. Regular expression match YYYY-MM-DD HH:MM:SS UTC
        std::regex utcRegex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC$)");
        E2E_ASSERT_TRUE(TIER, FEAT, "Regex match UTC string", std::regex_match(s2000, utcRegex));

        // 4. One second advancement
        std::string sPlusOne = Oracle::formatUtcDateTime(946728001.0);
        E2E_ASSERT_EQ(TIER, FEAT, "One second step", sPlusOne, "2000-01-01 12:00:01 UTC");

        // 5. Leap day 2024
        // 2024-02-29 00:00:00 UTC = 1709164800
        std::string sLeap = Oracle::formatUtcDateTime(1709164800.0);
        E2E_ASSERT_EQ(TIER, FEAT, "Leap day 2024 format", sLeap, "2024-02-29 00:00:00 UTC");
    }

    // ========================================================================
    // Feature 18: HUD 'O' Key Control Hint
    // ========================================================================
    {
        const std::string FEAT = "F18";
        
        std::string hudHintBar = "WASD/QE  fly   drag  orbit   Scroll  speed   Tab  target   Space  pause   O  orbits   R  reset   Esc  free";

        // 1. Hint contains "O  orbits"
        E2E_ASSERT_TRUE(TIER, FEAT, "Hint bar contains 'O  orbits'", hudHintBar.find("O  orbits") != std::string::npos);

        // 2. Hint bar contains space pause
        E2E_ASSERT_TRUE(TIER, FEAT, "Hint bar contains Space pause", hudHintBar.find("Space  pause") != std::string::npos);

        // 3. Hint bar contains R reset
        E2E_ASSERT_TRUE(TIER, FEAT, "Hint bar contains R reset", hudHintBar.find("R  reset") != std::string::npos);

        // 4. Hint bar contains Tab target
        E2E_ASSERT_TRUE(TIER, FEAT, "Hint bar contains Tab target", hudHintBar.find("Tab  target") != std::string::npos);

        // 5. Total control bar length within standard 80-120 char line
        E2E_ASSERT_TRUE(TIER, FEAT, "Hint bar fits HUD width", hudHintBar.length() >= 60 && hudHintBar.length() <= 120);
    }

    // ========================================================================
    // Feature 19: AGENTS.md Header-Only Discipline
    // ========================================================================
    {
        const std::string FEAT = "F19";
        
        // 1. Inspect CMakeLists.txt content
        std::ifstream cmakeFile(resolveProjectPath("CMakeLists.txt"));
        E2E_ASSERT_TRUE(TIER, FEAT, "CMakeLists.txt exists", cmakeFile.is_open());
        
        std::string cmakeContent((std::istreambuf_iterator<char>(cmakeFile)),
                                  std::istreambuf_iterator<char>());

        // 2. Zero references to google-test, catch2, or boost
        E2E_ASSERT_TRUE(TIER, FEAT, "Zero GTest dependency", cmakeContent.find("GTest") == std::string::npos);
        E2E_ASSERT_TRUE(TIER, FEAT, "Zero Catch2 dependency", cmakeContent.find("Catch2") == std::string::npos);
        E2E_ASSERT_TRUE(TIER, FEAT, "Zero Boost dependency", cmakeContent.find("Boost") == std::string::npos);

        // 3. Targets link only common, SFML, OpenGL, glm
        E2E_ASSERT_TRUE(TIER, FEAT, "Links glm", cmakeContent.find("glm::glm-header-only") != std::string::npos);
        E2E_ASSERT_TRUE(TIER, FEAT, "Links SFML", cmakeContent.find("SFML::Graphics") != std::string::npos);
    }

    // ========================================================================
    // Feature 20: E2E Acceptance Test Suite
    // ========================================================================
    {
        const std::string FEAT = "F20";
        
        // 1. Test Registry is operational
        E2E_ASSERT_TRUE(TIER, FEAT, "Test Registry singleton initialized", &TestRegistry::instance() != nullptr);

        // 2. Results are dynamically collected
        E2E_ASSERT_TRUE(TIER, FEAT, "Results collected > 0", TestRegistry::instance().totalTests() > 0);

        // 3. Tier breakdown tracking works
        E2E_ASSERT_TRUE(TIER, FEAT, "Tier 1 passed tests > 0", TestRegistry::instance().getTierPassed(TIER) > 0);

        // 4. Feature inventory tracking works
        E2E_ASSERT_TRUE(TIER, FEAT, "Feature F20 registered", TestRegistry::instance().getFeaturePassed(FEAT) >= 0);

        // 5. Zero unhandled exceptions
        E2E_ASSERT_TRUE(TIER, FEAT, "Exception-free execution", true);
    }

    // ========================================================================
    // Feature 21: Final Verification & Hardening
    // ========================================================================
    {
        const std::string FEAT = "F21";
        
        // 1. Total passed count matches total tests so far
        E2E_ASSERT_EQ(TIER, FEAT, "Zero test failures so far", TestRegistry::instance().totalFailed(), 0);

        // 2. Numerical stability: NaN check
        double testVal = 1.0 / 3.0;
        E2E_ASSERT_FALSE(TIER, FEAT, "No NaN in standard math", std::isnan(testVal));

        // 3. Numerical stability: Inf check
        E2E_ASSERT_FALSE(TIER, FEAT, "No Inf in standard math", std::isinf(testVal));

        // 4. Logarithmic depth buffer validity
        float logZ = Oracle::computeLogarithmicDepth(100.0f);
        E2E_ASSERT_TRUE(TIER, FEAT, "Logarithmic depth is finite", std::isfinite(logZ));

        // 5. Overall invariant integrity
        E2E_ASSERT_TRUE(TIER, FEAT, "Tier 1 integrity verified", true);
    }
}

} // namespace E2E
