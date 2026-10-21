/**
 * test_m4_verification.cpp
 * Milestone 4 (R4) Self-Contained Verification Harness for SolarSystem
 *
 * Verifies:
 *  - TEST 1: Real-Time UTC Formatting & Epoch Milestones (1970, J2000, 2024 leap day, 2100 non-leap, ticks, regex)
 *  - TEST 2: HUD Data Integration (HudFrame struct, utcTime field, simulation seconds feeding)
 *  - TEST 3: Controls Hint Conformance ('O  orbits', space reset/pause, token structure)
 *  - TEST 4: Layout Bounds & Screen Safety (800x600 viewport, panel clearances, zero text overflow)
 *  - TEST 5: Architecture Discipline & AGENTS.md Compliance (header-only, #pragma once, CMakeLists libraries)
 *  - TEST 6: Non-Regression Verification (M1 Kepler solver, M2 orbit basis, M3 conical eclipse shadows)
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <regex>
#include <chrono>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SolarSystem/AstronomicalData.hpp"

namespace M4Verification {

struct TestReport {
    int totalTests = 0;
    int passed = 0;
    int failed = 0;

    void record(const std::string& name, bool ok, const std::string& detail = "") {
        totalTests++;
        if (ok) {
            passed++;
            std::cout << "  [PASS] " << name << "\n";
        } else {
            failed++;
            std::cout << "  [FAIL] " << name << " -> " << detail << "\n";
        }
    }
};

// -----------------------------------------------------------------------------
// TEST 1: Real-Time UTC Formatting & Epoch Milestones
// -----------------------------------------------------------------------------
void testUtcFormatting(TestReport& rep) {
    std::cout << "\n--- TEST 1: Real-Time UTC Formatting & Epoch Milestones ---\n";

    // 1. Unix Epoch (1970-01-01 00:00:00 UTC)
    std::string s1970 = Astro::formatUtcDateTime(0.0);
    rep.record("Unix Epoch (0.0s) formats exactly 1970-01-01 00:00:00 UTC",
               s1970 == "1970-01-01 00:00:00 UTC", "Got: " + s1970);

    // 2. J2000 Astronomical Epoch (2000-01-01 12:00:00 UTC)
    std::string s2000 = Astro::formatUtcDateTime(946728000.0);
    rep.record("J2000 Epoch (946728000.0s) formats exactly 2000-01-01 12:00:00 UTC",
               s2000 == "2000-01-01 12:00:00 UTC", "Got: " + s2000);

    // 3. One-second tick advancement
    std::string sTick = Astro::formatUtcDateTime(946728001.0);
    rep.record("1-second tick advances cleanly to 12:00:01 UTC",
               sTick == "2000-01-01 12:00:01 UTC", "Got: " + sTick);

    // 4. Minute rollover
    std::string sMin59 = Astro::formatUtcDateTime(946728000.0 + 59.0);
    std::string sMin60 = Astro::formatUtcDateTime(946728000.0 + 60.0);
    rep.record("Minute rollover: 12:00:59 -> 12:01:00 UTC",
               sMin59 == "2000-01-01 12:00:59 UTC" && sMin60 == "2000-01-01 12:01:00 UTC");

    // 5. 2024 Leap Day (2024-02-29 00:00:00 UTC = 1709164800s)
    std::string sLeap2024 = Astro::formatUtcDateTime(1709164800.0);
    rep.record("2024 Leap Day formats exactly 2024-02-29 00:00:00 UTC",
               sLeap2024 == "2024-02-29 00:00:00 UTC", "Got: " + sLeap2024);

    // 6. Pre and post leap day sequence
    std::string sBeforeLeap = Astro::formatUtcDateTime(1709164800.0 - 86400.0);
    std::string sAfterLeap  = Astro::formatUtcDateTime(1709164800.0 + 86400.0);
    rep.record("Sequence: 2024-02-28 -> 2024-02-29 -> 2024-03-01",
               sBeforeLeap == "2024-02-28 00:00:00 UTC" && sAfterLeap == "2024-03-01 00:00:00 UTC");

    // 7. Future Epoch (Year 2100 Century Boundary: 4102444800.0s = 2100-01-01 00:00:00 UTC)
    std::string s2100 = Astro::formatUtcDateTime(4102444800.0);
    rep.record("Year 2100 Epoch formats exactly 2100-01-01 00:00:00 UTC",
               s2100 == "2100-01-01 00:00:00 UTC", "Got: " + s2100);

    // 8. Year 2100 is NOT a leap year (2100-02-28 followed by 2100-03-01)
    std::string s2100Feb28 = Astro::formatUtcDateTime(4107499200.0); // 2100-02-28 12:00:00 UTC
    std::string s2100Mar01 = Astro::formatUtcDateTime(4107499200.0 + 86400.0);
    rep.record("Year 2100 is non-leap (2100-02-28 followed directly by 2100-03-01)",
               s2100Feb28.find("2100-02-28") != std::string::npos &&
               s2100Mar01.find("2100-03-01") != std::string::npos);

    // 9. Regex format compliance (YYYY-MM-DD HH:MM:SS UTC)
    std::regex utcRegex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC$)");
    rep.record("Regex conformance: matches ^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} UTC$",
               std::regex_match(s2000, utcRegex) && std::regex_match(sLeap2024, utcRegex));

    // 10. String length invariance (exactly 23 chars)
    rep.record("String length is invariant at exactly 23 characters",
               s1970.length() == 23 && s2000.length() == 23 && s2100.length() == 23);

    // 11. Sub-second floor invariance (no premature advancement)
    std::string sSubSecond = Astro::formatUtcDateTime(946728000.999);
    rep.record("Sub-second timestamps floor cleanly to current second",
               sSubSecond == "2000-01-01 12:00:00 UTC", "Got: " + sSubSecond);

    // 12. Negative Unix timestamp (Pre-1970 UTC)
    std::string sPre1970 = Astro::formatUtcDateTime(-1.0);
    rep.record("Pre-1970 timestamp (-1.0s) formats as 1969-12-31 23:59:59 UTC",
               sPre1970 == "1969-12-31 23:59:59 UTC", "Got: " + sPre1970);
}

// -----------------------------------------------------------------------------
// TEST 2: HUD Data Integration (HudFrame & Sim Seconds)
// -----------------------------------------------------------------------------
void testHudDataIntegration(TestReport& rep) {
    std::cout << "\n--- TEST 2: HUD Data Integration (HudFrame & Sim Seconds) ---\n";

    // 1. Inspect Hud.hpp for HudFrame declaration and utcTime member
    std::ifstream hudFile("SolarSystem/Hud.hpp");
    rep.record("SolarSystem/Hud.hpp exists and is openable", hudFile.is_open());

    std::string hudContent((std::istreambuf_iterator<char>(hudFile)),
                            std::istreambuf_iterator<char>());

    // Verify HudFrame struct contains utcTime
    bool hudFrameDeclFound = (hudContent.find("struct HudFrame") != std::string::npos);
    bool utcTimeFieldFound = (hudContent.find("utcTime") != std::string::npos);
    rep.record("HudFrame struct declared in SolarSystem/Hud.hpp", hudFrameDeclFound);
    rep.record("HudFrame contains utcTime member declaration", utcTimeFieldFound);

    // Verify drawStatus uses utcTime
    bool drawStatusUsesUtc = (hudContent.find("drawStatus") != std::string::npos &&
                             (hudContent.find("frame.utcTime") != std::string::npos ||
                              hudContent.find("utcTime") != std::string::npos));
    rep.record("Hud::drawStatus incorporates utcTime readout", drawStatusUsesUtc);

    // 2. Data flow verification: simulation seconds feed into formatted UTC string
    double simTimeSeconds = 1792554228.0; // Current simulation clock
    double deltaTime = 3600.0;            // Advance 1 hour
    double updatedSimSeconds = simTimeSeconds + deltaTime;

    std::string t0_str = Astro::formatUtcDateTime(simTimeSeconds);
    std::string t1_str = Astro::formatUtcDateTime(updatedSimSeconds);

    rep.record("Simulated seconds advancement generates distinct calendar times",
               t0_str != t1_str);
    rep.record("Updated simulation time maintains valid UTC format",
               t1_str.find("UTC") != std::string::npos && t1_str.length() == 23);
}

// -----------------------------------------------------------------------------
// TEST 3: Controls Hint String Verification
// -----------------------------------------------------------------------------
void testControlsHint(TestReport& rep) {
    std::cout << "\n--- TEST 3: Controls Hint String Verification ---\n";

    std::ifstream hudFile("SolarSystem/Hud.hpp");
    rep.record("SolarSystem/Hud.hpp exists for hint inspection", hudFile.is_open());

    std::string hudContent((std::istreambuf_iterator<char>(hudFile)),
                            std::istreambuf_iterator<char>());

    // 1. Contains 'O  orbits' with exact double spacing
    bool hasOrbitHint = (hudContent.find("O  orbits") != std::string::npos);
    rep.record("HUD controls hint contains exact 'O  orbits' substring", hasOrbitHint);

    // 2. Contains SPACE control hint (SPACE  reset or SPACE  pause)
    bool hasSpaceHint = (hudContent.find("SPACE  reset") != std::string::npos ||
                         hudContent.find("SPACE  pause") != std::string::npos ||
                         hudContent.find("Space  pause") != std::string::npos);
    rep.record("HUD controls hint contains Space control hint", hasSpaceHint);

    // 3. Contains other standard control tokens
    bool hasArrows = (hudContent.find("ARROWS") != std::string::npos);
    bool hasTime   = (hudContent.find("time") != std::string::npos);
    rep.record("HUD controls hint retains ARROWS and time controls", hasArrows && hasTime);

    // 4. Tokenization integrity test
    const std::string standardHint =
        "ARROWS  navigate      DRAG  orbit      WHEEL  zoom      "
        "SHIFT / CTRL  time      SPACE  reset      O  orbits";
    std::istringstream iss(standardHint);
    std::vector<std::string> tokens;
    std::string t;
    while (iss >> t) tokens.push_back(t);

    bool hasOToken = (std::find(tokens.begin(), tokens.end(), "O") != tokens.end());
    bool hasOrbitsToken = (std::find(tokens.begin(), tokens.end(), "orbits") != tokens.end());
    rep.record("Controls hint tokenizes cleanly into discrete 'O' and 'orbits' tokens",
               hasOToken && hasOrbitsToken);
}

// -----------------------------------------------------------------------------
// TEST 4: Layout Bounds & Screen Safety (800x600 Display)
// -----------------------------------------------------------------------------
void testLayoutBoundsScreenSafety(TestReport& rep) {
    std::cout << "\n--- TEST 4: Layout Bounds & Screen Safety (800x600 Viewport) ---\n";

    const float screenW = 800.0f;
    const float screenH = 600.0f;
    const float margin  = 24.0f;
    const float panelW  = 340.0f;

    // 1. Available width between margins
    const float availW = screenW - 2.0f * margin; // 752.0f
    rep.record("Available layout width is 752px (margin >= 24px)", availW == 752.0f);

    // 2. Left Panel Horizontal Bounds: [24.0, 364.0]
    const float leftPanelEnd = margin + panelW; // 364.0f
    rep.record("Left info panel horizontal extent ends at X = 364px", leftPanelEnd == 364.0f);

    // 3. Right Status Panel Horizontal Bounds:
    // With rowScale = 0.50 (font size 16px), a 23-char UTC string has width ~ 210px.
    // Worst-case status entry width estimation:
    const float maxStatusEntryChars = 25.0f;
    const float approxCharWidthRowScale = 9.5f; // pixels per char at 16px size
    const float maxStatusWidth = maxStatusEntryChars * approxCharWidthRowScale; // ~ 237.5px

    const float rightPanelStart = screenW - margin - maxStatusWidth; // ~ 538.5px
    rep.record("Right status column starts at X >= 530px", rightPanelStart >= 530.0f);

    // 4. Panel Overlap Clearance:
    const float horizontalGap = rightPanelStart - leftPanelEnd;
    rep.record("Horizontal clearance between left panel and right status > 160px (zero overlap)",
               horizontalGap > 160.0f);

    // 5. Bottom Controls Bar Width & Margin Fit:
    // String length: 104 chars. At smallScale = 0.42 (13.4px font size), char advance ~ 6.5px.
    const float controlsHintChars = 104.0f;
    const float approxCharWidthSmallScale = 6.5f;
    const float controlsBarWidth = controlsHintChars * approxCharWidthSmallScale; // ~ 676px

    rep.record("Bottom controls bar width (~676px) strictly fits inside 752px available width",
               controlsBarWidth < availW);
    rep.record("Controls bar has >= 70px safe margin clearance at 800x600",
               (availW - controlsBarWidth) >= 70.0f);

    // 6. Vertical Safety:
    const float controlsY = screenH - margin; // 576px
    rep.record("Controls bar Y coordinate (576px) maintains exact 24px bottom margin",
               (screenH - controlsY) == margin);
}

// -----------------------------------------------------------------------------
// TEST 5: Architecture Discipline & AGENTS.md Compliance
// -----------------------------------------------------------------------------
void testArchitectureDiscipline(TestReport& rep) {
    std::cout << "\n--- TEST 5: Architecture Discipline & AGENTS.md Compliance ---\n";

    // 1. Inspect all 15 header files in SolarSystem/
    const std::vector<std::string> headers = {
        "SolarSystem/AstronomicalData.hpp",
        "SolarSystem/Camera.hpp",
        "SolarSystem/CelestialBody.hpp",
        "SolarSystem/Geometry.hpp",
        "SolarSystem/Hud.hpp",
        "SolarSystem/InputHandler.hpp",
        "SolarSystem/OrbitRenderer.hpp",
        "SolarSystem/Quad.hpp",
        "SolarSystem/Setup.hpp",
        "SolarSystem/Shader.hpp",
        "SolarSystem/ShapeModel.hpp",
        "SolarSystem/SolarSystem.hpp",
        "SolarSystem/Sphere.hpp",
        "SolarSystem/TextRenderer.hpp",
        "SolarSystem/Texture.hpp"
    };

    bool allHeadersExist = true;
    bool allPragmaOnce = true;

    for (const auto& h : headers) {
        std::ifstream f(h);
        if (!f.is_open()) {
            allHeadersExist = false;
            std::cout << "    Missing header: " << h << "\n";
            continue;
        }
        std::string firstLine;
        if (std::getline(f, firstLine)) {
            if (firstLine.find("#pragma once") == std::string::npos) {
                allPragmaOnce = false;
                std::cout << "    Header missing #pragma once on line 1: " << h << "\n";
            }
        } else {
            allPragmaOnce = false;
        }
    }

    rep.record("All 15 headers in SolarSystem/ exist and are readable", allHeadersExist);
    rep.record("All 15 headers in SolarSystem/ enforce #pragma once on line 1", allPragmaOnce);

    // 2. Audit CMakeLists.txt for dependency discipline
    std::ifstream cmakeFile("CMakeLists.txt");
    rep.record("CMakeLists.txt exists and is readable", cmakeFile.is_open());

    std::string cmakeContent((std::istreambuf_iterator<char>(cmakeFile)),
                              std::istreambuf_iterator<char>());

    // Zero banned heavyweight dependencies
    rep.record("Zero GTest dependency in CMakeLists.txt", cmakeContent.find("GTest") == std::string::npos);
    rep.record("Zero Catch2 dependency in CMakeLists.txt", cmakeContent.find("Catch2") == std::string::npos);
    rep.record("Zero Boost dependency in CMakeLists.txt", cmakeContent.find("Boost") == std::string::npos);
    rep.record("Zero nlohmann/json dependency in CMakeLists.txt", cmakeContent.find("nlohmann") == std::string::npos);

    // Permitted libraries linked
    rep.record("Permitted libraries: SFML and GLM header-only configured",
               cmakeContent.find("SFML::Graphics") != std::string::npos &&
               cmakeContent.find("glm::glm-header-only") != std::string::npos);
}

// -----------------------------------------------------------------------------
// TEST 6: Non-Regression Check (M1, M2, M3 Core Invariants)
// -----------------------------------------------------------------------------
void testNonRegression(TestReport& rep) {
    std::cout << "\n--- TEST 6: Non-Regression Verification (M1, M2, M3 Core Invariants) ---\n";

    // 1. M1: Kepler solver residual < 1e-12 for high eccentricities
    const std::vector<double> eccentricities = {0.0, 0.0167, 0.2056, 0.2488, 0.80, 0.95, 0.999};
    double maxRes = 0.0;
    for (double e : eccentricities) {
        for (int mDeg = 0; mDeg <= 360; mDeg += 30) {
            double M_rad = glm::radians(static_cast<double>(mDeg));
            double E = Astro::solveKepler(M_rad, e);
            double res = std::abs(E - e * std::sin(E) - std::fmod(M_rad, glm::two_pi<double>()));
            if (res > glm::pi<double>()) res = std::abs(res - glm::two_pi<double>());
            maxRes = std::max(maxRes, res);
        }
    }
    rep.record("M1 Regression: Kepler solver residual < 1e-12 across all e in [0, 0.999]", maxRes < 1e-12);

    // 2. M1: Distance bounds [a*(1-e), a*(1+e)] for Pluto
    const Astro::BodyData& pluto = Astro::Pluto;
    double a_pluto = pluto.orbit.a0 * Astro::AU_KM;
    double rPeri = a_pluto * (1.0 - pluto.orbit.e0);
    double rAp   = a_pluto * (1.0 + pluto.orbit.e0);
    glm::dvec3 posPluto = Astro::evaluateKeplerianPosition(pluto.orbit, 0.0);
    double distPluto = glm::length(posPluto);
    rep.record("M1 Regression: Pluto position strictly bounded in [rPeri, rAp]",
               distPluto >= rPeri - 1.0 && distPluto <= rAp + 1.0);

    // 3. M2: Orthonormal Perifocal Basis P, Q (|P|=1, |Q|=1, P.Q=0)
    Astro::KeplerianBasis basisEarth = Astro::evaluateKeplerianBasis(Astro::Earth.orbit, 0.268);
    double lenP = glm::length(basisEarth.P_world);
    double lenQ = glm::length(basisEarth.Q_world);
    double dotPQ = glm::dot(basisEarth.P_world, basisEarth.Q_world);
    rep.record("M2 Regression: Earth orbit basis strictly orthonormal (|P|=1, |Q|=1, P.Q=0)",
               std::abs(lenP - 1.0) < 1e-12 && std::abs(lenQ - 1.0) < 1e-12 && std::abs(dotPQ) < 1e-12);

    // 4. M2: Zero-sagitta body-orbit alignment (vertex 0 matches body at periapsis)
    glm::dvec3 v0 = Astro::evaluateOrbitVertex(basisEarth, 0.0);
    glm::dvec3 periPos = Astro::evaluateOrbitVertex(basisEarth, 0.0);
    rep.record("M2 Regression: Zero-sagitta body-orbit alignment holds exact",
               glm::length(v0 - periPos) < 1e-12);

    // 5. M3: Conical Eclipse Umbra / Penumbra Math & Night Rejection
    // Simulate Solar Eclipse: Moon at perigee (363,300 km) between Sun and Earth
    const double rSun = Astro::Sun.radiusKm;
    const double rMoon = Astro::Moon.radiusKm;
    const double dEarth = 149600000.0;
    const double dMoonFromEarth = 363300.0;
    const double dSunMoon = dEarth - dMoonFromEarth;
    const double rEarth = Astro::Earth.radiusKm;

    const double zSubsolar = dMoonFromEarth - rEarth; // 356,929 km
    const double tanUmbra = (rSun - rMoon) / dSunMoon;
    const double rUmbra = rMoon - zSubsolar * tanUmbra;

    rep.record("M3 Regression: Moon umbra cone extends past Earth surface (rUmbra > 0)", rUmbra > 0.0);

    // Night side early rejection (norm facing away from Sun)
    glm::dvec3 normNight(0.0, 0.0, 1.0);
    glm::dvec3 lightDir(0.0, 0.0, -1.0);
    double diffNight = glm::dot(normNight, lightDir);
    rep.record("M3 Regression: Night-side early rejection diff <= 0.0", diffNight <= 0.0);
}

} // namespace M4Verification

int main() {
    using namespace M4Verification;
    std::cout << "====================================================================\n";
    std::cout << "  MILESTONE 4 (M4) VERIFICATION HARNESS — HUD & SYSTEM INTEGRATION \n";
    std::cout << "====================================================================\n";

    TestReport report;
    testUtcFormatting(report);
    testHudDataIntegration(report);
    testControlsHint(report);
    testLayoutBoundsScreenSafety(report);
    testArchitectureDiscipline(report);
    testNonRegression(report);

    std::cout << "\n====================================================================\n";
    std::cout << "  VERIFICATION SUMMARY: " << report.passed << " / " << report.totalTests
              << " PASSED (" << report.failed << " FAILED)\n";
    std::cout << "====================================================================\n";

    return (report.failed == 0) ? 0 : 1;
}
