/**
 * test_challenger_m4_adversarial.cpp
 *
 * EMPIRICAL ADVERSARIAL CHALLENGE HARNESS — MILESTONE M4
 * Layout Geometry, Viewport Scaling, & Extreme Value Stress Testing
 *
 * Role: critic, specialist (Empirical Challenger)
 * Agent: challenger_m4_2
 *
 * Challenge Scope:
 *  1. Multi-Resolution Viewport Scaling & Panel Clearance:
 *     - Resolutions: 800x600, 1024x768, 1280x720, 1920x1080, 2560x1440, 3840x2160 (4K), 3440x1440 (Ultra-Wide 21:9).
 *     - Left Info Panel exact bounding box: X in [24.0, 364.0]. Maximum height evaluated across all 38 catalog bodies.
 *     - Right Status Block exact bounding box: right-aligned at width - 24.0.
 *     - Empirical proof that Left Info Panel and Right Status Block NEVER collide or overlap (horizontal gap > 0) at any tested resolution.
 *  2. Bottom Controls Hint Bar Layout & Collision Safety:
 *     - Empirical FreeType glyph measurement of exact string width at scale 0.42.
 *     - Proof of fit without clipping: right margin clearance > 0 across all resolutions >= 800px.
 *     - Vertical clearance: proof of zero collision between Bottom Controls bar and Left Info Panel / Right Status Block.
 *  3. Rapid timeScale Mutation & String Formatting Extremes:
 *     - Sweep across 0.001x to 10000x (including degenerate, boundary, and float representations).
 *     - Verification of trimFloat() behavior, buffer safety, and string formatting invariance.
 *  4. Camera Distance Extremes & Unit Formatting:
 *     - km vs AU threshold crossover at 2,000,000 km (moonDistanceLimitKm).
 *     - Distance extremes: 0 km, near-surface, moon orbital radii, 1 AU, Pluto (39.5 AU), Sedna (1000 AU), 10^11 km.
 *     - Verification of groupDigits() formatting and length bounding.
 *  5. 10,000-Iteration Randomized Monte Carlo Fuzzing:
 *     - Fuzzing random resolutions, random bodies, random timeScales, random camera distances, random epochs.
 *     - Real-time invariant enforcement: zero NaN/Inf, zero negative gaps, zero bounding box inversions.
 *  6. Sub-Specification Stress & Critical Resolution Analysis:
 *     - Determination of exact analytical breakdown points (W_crit for collision, W_crit for clipping).
 *     - Empirical safety margin proof for minimum specified resolution 800x600.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <sstream>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/glm.hpp>
#include "SolarSystem/AstronomicalData.hpp"

namespace Color {
    inline const char* Reset   = "\033[0m";
    inline const char* Bold    = "\033[1m";
    inline const char* Red     = "\033[31m";
    inline const char* Green   = "\033[32m";
    inline const char* Yellow  = "\033[33m";
    inline const char* Blue    = "\033[34m";
    inline const char* Magenta = "\033[35m";
    inline const char* Cyan    = "\033[36m";
}

struct ChallengeStats {
    int totalChecks = 0;
    int passed = 0;
    int failed = 0;

    void record(const std::string& name, bool pass, const std::string& detail = "") {
        totalChecks++;
        if (pass) {
            passed++;
            std::cout << "  " << Color::Green << "[PASS] " << Color::Reset << name;
            if (!detail.empty()) std::cout << " (" << Color::Cyan << detail << Color::Reset << ")";
            std::cout << "\n";
        } else {
            failed++;
            std::cout << "  " << Color::Red << "[FAIL] " << Color::Reset << name;
            if (!detail.empty()) std::cout << " -> " << Color::Yellow << detail << Color::Reset;
            std::cout << "\n";
        }
    }
};

// ============================================================================
// FreeType Font Oracle & Layout Engine (Mirrors Hud.hpp and TextRenderer.hpp)
// ============================================================================
class FreeTypeOracle {
public:
    struct GlyphData {
        float advance = 0.0f;
        float lsbDelta = 0.0f;
        float rsbDelta = 0.0f;
        glm::vec2 size{0.0f};
        glm::vec2 offset{0.0f};
    };

    FreeTypeOracle(const std::string& fontPath, unsigned int pixelHeight)
        : glyphHeight(pixelHeight) {
        if (FT_Init_FreeType(&ftLibrary) != 0) {
            throw std::runtime_error("FreeType initialization failed");
        }
        if (FT_New_Face(ftLibrary, fontPath.c_str(), 0, &ftFace) != 0) {
            throw std::runtime_error("Failed to load font from " + fontPath);
        }
        FT_Set_Pixel_Sizes(ftFace, 0, glyphHeight);
        lineSpacing = static_cast<float>(ftFace->size->metrics.height) / 64.0f;
        ascender    = static_cast<float>(ftFace->size->metrics.ascender) / 64.0f;
        descender   = static_cast<float>(ftFace->size->metrics.descender) / 64.0f;

        preloadGlyphs();
    }

    ~FreeTypeOracle() {
        if (ftFace) FT_Done_Face(ftFace);
        if (ftLibrary) FT_Done_FreeType(ftLibrary);
    }

    float getLineSpacing(float scale) const {
        return lineSpacing * scale;
    }

    float getAscender(float scale) const {
        return ascender * scale;
    }

    float getDescender(float scale) const {
        return descender * scale;
    }

    float measure(const std::string& utf8, float scale) const {
        float width = 0.0f;
        std::uint32_t previous = 0;

        for (std::uint32_t codePoint : decodeUtf8(utf8)) {
            const auto it = glyphs.find(codePoint);
            if (it == glyphs.end()) continue;

            if (previous != 0) {
                width += getKerning(previous, codePoint) * scale;
            }
            width += it->second.advance * scale;
            previous = codePoint;
        }
        return width;
    }

    float getKerning(std::uint32_t first, std::uint32_t second) const {
        if (first == 0 || second == 0) return 0.0f;
        FT_UInt index1 = FT_Get_Char_Index(ftFace, first);
        FT_UInt index2 = FT_Get_Char_Index(ftFace, second);

        const auto it1 = glyphs.find(first);
        const auto it2 = glyphs.find(second);
        float rsb1 = (it1 != glyphs.end()) ? it1->second.rsbDelta : 0.0f;
        float lsb2 = (it2 != glyphs.end()) ? it2->second.lsbDelta : 0.0f;

        FT_Vector kerning{0, 0};
        if (FT_HAS_KERNING(ftFace)) {
            FT_Get_Kerning(ftFace, index1, index2, FT_KERNING_UNFITTED, &kerning);
        }

        return std::floor((lsb2 - rsb1 + static_cast<float>(kerning.x) + 32.0f) / 64.0f);
    }

    static std::vector<std::uint32_t> decodeUtf8(const std::string& input) {
        std::vector<std::uint32_t> out;
        std::size_t i = 0;
        while (i < input.size()) {
            const unsigned char lead = static_cast<unsigned char>(input[i]);
            std::uint32_t codePoint = lead;
            std::size_t extra = 0;
            if (lead >= 0xF0)      { codePoint = lead & 0x07u; extra = 3; }
            else if (lead >= 0xE0) { codePoint = lead & 0x0Fu; extra = 2; }
            else if (lead >= 0xC0) { codePoint = lead & 0x1Fu; extra = 1; }
            if (i + extra >= input.size()) extra = 0;
            for (std::size_t k = 1; k <= extra; ++k) {
                codePoint = (codePoint << 6) | (static_cast<unsigned char>(input[i + k]) & 0x3Fu);
            }
            out.push_back(codePoint);
            i += extra + 1;
        }
        return out;
    }

private:
    FT_Library ftLibrary = nullptr;
    FT_Face ftFace = nullptr;
    unsigned int glyphHeight = 32;
    float lineSpacing = 0.0f;
    float ascender = 0.0f;
    float descender = 0.0f;
    std::unordered_map<std::uint32_t, GlyphData> glyphs;

    void preloadGlyphs() {
        std::vector<std::uint32_t> wanted;
        for (std::uint32_t c = 32; c < 127; ++c) wanted.push_back(c);
        wanted.push_back(0x00B0); // degree sign
        wanted.push_back(0x00D7); // multiplication sign

        for (std::uint32_t cp : wanted) {
            FT_UInt glyphIndex = FT_Get_Char_Index(ftFace, cp);
            FT_Error err = FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT);
            if (err != 0) continue;

            GlyphData g;
            g.advance = static_cast<float>(ftFace->glyph->advance.x >> 6);
            g.lsbDelta = static_cast<float>(ftFace->glyph->lsb_delta);
            g.rsbDelta = static_cast<float>(ftFace->glyph->rsb_delta);
            g.size = glm::vec2(ftFace->glyph->metrics.width >> 6, ftFace->glyph->metrics.height >> 6);
            g.offset = glm::vec2(ftFace->glyph->metrics.horiBearingX >> 6, -(ftFace->glyph->metrics.horiBearingY >> 6));
            glyphs.emplace(cp, g);
        }
    }
};

// ============================================================================
// Mirror of HUD String Formatting Functions (from Hud.hpp)
// ============================================================================
namespace HudMirror {
    template <typename... Args>
    static std::string format(const char* pattern, Args... args) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), pattern, args...);
        return std::string(buffer);
    }

    static std::string groupDigits(long long value) {
        std::string digits = std::to_string(value);
        std::string out;
        int sinceGroup = 0;
        for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
            if (sinceGroup == 3) {
                out.push_back(' ');
                sinceGroup = 0;
            }
            out.push_back(*it);
            ++sinceGroup;
        }
        return std::string(out.rbegin(), out.rend());
    }

    static std::string formatKm(double km) {
        return groupDigits(static_cast<long long>(km + 0.5)) + " km";
    }

    static std::string formatDistance(double km) {
        if (km >= Astro::moonDistanceLimitKm) return format("%.2f AU", km / Astro::AU_KM);
        return formatKm(km);
    }

    static std::string formatDays(double days) {
        if (days >= 365.25) return format("%.2f yr", days / 365.25);
        if (days >= 1.0)    return format("%.2f d", days);
        return format("%.1f h", days * 24.0);
    }

    static std::string trimFloat(float value) {
        std::string s = (std::abs(value) < 0.1f) ? format("%.3f", value)
                                                 : format("%.2f", value);
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }

    static std::vector<std::pair<std::string, std::string>> buildRows(const Astro::BodyData* d, int satelliteCount) {
        std::vector<std::pair<std::string, std::string>> rows;
        if (d == nullptr) return rows;

        rows.emplace_back("RADIUS", formatKm(d->radiusKm));
        if (d->distanceFromParentKm > 0.0) {
            rows.emplace_back("ORBIT RADIUS", formatDistance(d->distanceFromParentKm));
        }
        if (d->orbitalPeriodDays != 0.0) {
            rows.emplace_back("ORBITAL PERIOD", formatDays(d->orbitalPeriodDays));
        }
        if (d->rotationPeriodDays != 0.0) {
            const bool retrograde = d->rotationPeriodDays < 0.0;
            rows.emplace_back("DAY LENGTH", formatDays(std::abs(d->rotationPeriodDays)) + (retrograde ? "  retro" : ""));
        }
        rows.emplace_back("AXIAL TILT", format("%.2f\xC2\xB0", d->axialTiltDegrees));
        if (satelliteCount > 0) {
            rows.emplace_back("SATELLITES", format("%d", satelliteCount));
        }
        return rows;
    }
}

// ============================================================================
// Layout Geometry Specifications
// ============================================================================
struct LayoutConstants {
    static constexpr float margin      = 24.0f;
    static constexpr float panelWidth  = 340.0f;
    static constexpr float panelPad    = 18.0f;
    static constexpr float valueColumn = 165.0f;

    static constexpr float titleScale = 0.92f;
    static constexpr float rowScale   = 0.50f;
    static constexpr float smallScale = 0.42f;
};

struct BoundingBox {
    float xMin = 0.0f;
    float xMax = 0.0f;
    float yMin = 0.0f;
    float yMax = 0.0f;

    bool isValid() const {
        return xMax >= xMin && yMax >= yMin &&
               !std::isnan(xMin) && !std::isnan(xMax) &&
               !std::isnan(yMin) && !std::isnan(yMax);
    }

    bool overlaps(const BoundingBox& other) const {
        if (xMax <= other.xMin || xMin >= other.xMax) return false;
        if (yMax <= other.yMin || yMin >= other.yMax) return false;
        return true;
    }
};

// ============================================================================
// CHALLENGE 1: Multi-Resolution Viewport Scaling & Panel Clearance
// ============================================================================
void challengeMultiResolutionClearance(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 1: Multi-Resolution Viewport Scaling & Non-Overlap ===" << Color::Reset << "\n";

    struct Resolution {
        std::string name;
        unsigned int width;
        unsigned int height;
    };

    const std::vector<Resolution> resolutions = {
        {"800x600 (SVGA / Min Viewport)", 800, 600},
        {"1024x768 (XGA 4:3)",            1024, 768},
        {"1280x720 (HD 16:9)",             1280, 720},
        {"1920x1080 (FHD 1080p)",          1920, 1080},
        {"2560x1440 (QHD 1440p)",          2560, 1440},
        {"3840x2160 (4K UHD)",             3840, 2160},
        {"3440x1440 (21:9 Ultra-Wide)",    3440, 1440}
    };

    // Calculate maximum possible Left Info Panel height across all 38 catalog bodies
    const std::vector<const Astro::BodyData*> allBodies = {
        &Astro::Sun, &Astro::Mercury, &Astro::Venus, &Astro::Earth, &Astro::Moon,
        &Astro::Mars, &Astro::Phobos, &Astro::Deimos, &Astro::Jupiter, &Astro::Io,
        &Astro::Europa, &Astro::Ganymede, &Astro::Callisto, &Astro::Saturn, &Astro::Titan,
        &Astro::Rhea, &Astro::Iapetus, &Astro::Dione, &Astro::Enceladus, &Astro::Mimas,
        &Astro::Uranus, &Astro::Titania, &Astro::Oberon, &Astro::Umbriel, &Astro::Ariel, &Astro::Miranda,
        &Astro::Neptune, &Astro::Triton,
        &Astro::Pluto, &Astro::Charon, &Astro::Ceres, &Astro::Vesta, &Astro::Pallas,
        &Astro::Eris, &Astro::Haumea, &Astro::Makemake, &Astro::Eros, &Astro::Gaspra, &Astro::Ida
    };

    float maxPanelHeight = 0.0f;
    std::string tallestBodyName;

    const float rowHeight   = oracle.getLineSpacing(LayoutConstants::rowScale);
    const float titleHeight = oracle.getLineSpacing(LayoutConstants::titleScale);

    for (const auto* body : allBodies) {
        int satCount = (body == &Astro::Earth) ? 1 :
                       (body == &Astro::Jupiter) ? 4 :
                       (body == &Astro::Saturn) ? 7 :
                       (body == &Astro::Uranus) ? 5 :
                       (body == &Astro::Neptune) ? 3 :
                       (body == &Astro::Pluto) ? 1 : 0;
        auto rows = HudMirror::buildRows(body, satCount);
        float h = LayoutConstants::panelPad + 16.0f + titleHeight + 14.0f + rowHeight * rows.size() + LayoutConstants::panelPad;
        if (h > maxPanelHeight) {
            maxPanelHeight = h;
            tallestBodyName = body->name;
        }
    }

    // Left Panel Box
    BoundingBox leftPanelBox;
    leftPanelBox.xMin = LayoutConstants::margin;
    leftPanelBox.xMax = LayoutConstants::margin + LayoutConstants::panelWidth; // 364.0f
    leftPanelBox.yMin = LayoutConstants::margin;
    leftPanelBox.yMax = LayoutConstants::margin + maxPanelHeight;

    stats.record("Left Info Panel X extent invariant [24.0, 364.0]",
                 leftPanelBox.xMin == 24.0f && leftPanelBox.xMax == 364.0f,
                 "Width = 340.0 px");

    stats.record("Tallest Info Panel identified (" + tallestBodyName + ")",
                 maxPanelHeight > 0.0f,
                 "Panel Height = " + std::to_string(maxPanelHeight) + " px, yMax = " + std::to_string(leftPanelBox.yMax) + " px");

    // Worst-case status entries
    const std::string worstFps  = HudMirror::format("%.0f FPS", 99999.0f);
    const std::string worstUtc  = "2026-10-21 04:00:00 UTC"; // 23 chars
    const std::string worstTime = HudMirror::format("TIME  %s\xC3\x97", HudMirror::trimFloat(10000.0f).c_str());
    const std::string worstDist = "DIST  " + HudMirror::formatDistance(1999999.0); // "DIST  1 999 999 km"

    const std::vector<std::string> worstEntries = {worstFps, worstUtc, worstTime, worstDist};
    float maxEntryWidth = 0.0f;
    std::string widestEntry;
    for (const auto& entry : worstEntries) {
        float w = oracle.measure(entry, LayoutConstants::rowScale);
        if (w > maxEntryWidth) {
            maxEntryWidth = w;
            widestEntry = entry;
        }
    }

    stats.record("Status block widest entry identified ('" + widestEntry + "')",
                 maxEntryWidth > 0.0f,
                 "Measured Width = " + std::to_string(maxEntryWidth) + " px at scale 0.50");

    // Test across all resolutions
    for (const auto& res : resolutions) {
        float screenW = static_cast<float>(res.width);
        float screenH = static_cast<float>(res.height);

        // Right Status Bounding Box
        BoundingBox rightStatusBox;
        rightStatusBox.xMin = screenW - LayoutConstants::margin - maxEntryWidth;
        rightStatusBox.xMax = screenW - LayoutConstants::margin;
        rightStatusBox.yMin = LayoutConstants::margin + LayoutConstants::panelPad + 10.0f;
        rightStatusBox.yMax = rightStatusBox.yMin + 4.0f * rowHeight;

        // Check horizontal clearance
        float horizontalGap = rightStatusBox.xMin - leftPanelBox.xMax;
        bool noOverlap = (horizontalGap > 0.0f);

        stats.record("Resolution " + res.name + ": Non-Overlap Guarantee",
                     noOverlap && !leftPanelBox.overlaps(rightStatusBox),
                     "LeftPanel X=[24, 364], RightStatus X=[" +
                     std::to_string(rightStatusBox.xMin) + ", " + std::to_string(rightStatusBox.xMax) +
                     "], Gap = " + std::to_string(horizontalGap) + " px");

        // Verify bounds are well inside screen
        stats.record("Resolution " + res.name + ": Right status within screen boundaries",
                     rightStatusBox.xMax <= screenW && rightStatusBox.xMin >= 0.0f);
    }
}

// ============================================================================
// CHALLENGE 2: Bottom Controls Hint Bar Layout & Collision Safety
// ============================================================================
void challengeBottomControlsSafety(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 2: Bottom Controls Hint Bar Layout & Non-Clipping ===" << Color::Reset << "\n";

    const std::string hint =
        "ARROWS  navigate      DRAG  orbit      WHEEL  zoom      "
        "SHIFT / CTRL  time      SPACE  reset      O  orbits";

    float hintWidth = oracle.measure(hint, LayoutConstants::smallScale);
    float fontAscent = oracle.getAscender(LayoutConstants::smallScale);
    float fontDescent = std::abs(oracle.getDescender(LayoutConstants::smallScale));

    // The worker claimed 104 characters; empirical measurement shows 107 characters
    stats.record("Controls hint actual length is 107 chars (worker claimed 104)",
                 hint.length() == 107,
                 "Actual = " + std::to_string(hint.length()) + " chars; worker claim was 104");
    stats.record("Controls hint measured width via FreeType",
                 hintWidth > 600.0f && hintWidth < 700.0f,
                 "Width = " + std::to_string(hintWidth) + " px (at smallScale 0.42)");

    const std::vector<std::pair<unsigned int, unsigned int>> resList = {
        {800, 600}, {1024, 768}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {3440, 1440}
    };

    const float rowHeight   = oracle.getLineSpacing(LayoutConstants::rowScale);
    const float titleHeight = oracle.getLineSpacing(LayoutConstants::titleScale);
    const float maxPanelHeight = LayoutConstants::panelPad + 16.0f + titleHeight + 14.0f + rowHeight * 6.0f + LayoutConstants::panelPad;

    for (const auto& res : resList) {
        float screenW = static_cast<float>(res.first);
        float screenH = static_cast<float>(res.second);

        // Horizontal fit test:
        // Text is drawn at x = margin (24.0f)
        float textX_start = LayoutConstants::margin;
        float textX_end   = textX_start + hintWidth;
        float rightMarginLimit = screenW - LayoutConstants::margin;
        float rightClearance = rightMarginLimit - textX_end;

        stats.record("Controls hint fits horizontally at " + std::to_string(res.first) + "x" + std::to_string(res.second),
                     textX_end <= rightMarginLimit,
                     "textX_end = " + std::to_string(textX_end) + " px, limit = " + std::to_string(rightMarginLimit) +
                     " px, clearance = " + std::to_string(rightClearance) + " px");

        // Vertical safety test:
        // Baseline is at screenH - margin (e.g. 576.0f at height 600)
        // Top of controls glyphs: screenH - margin - fontAscent
        // Bottom of info panel: margin + maxPanelHeight
        float controlsTopY = screenH - LayoutConstants::margin - fontAscent;
        float infoPanelBottomY = LayoutConstants::margin + maxPanelHeight;
        float verticalGapInfo = controlsTopY - infoPanelBottomY;

        stats.record("Vertical clearance InfoPanel vs Controls at " + std::to_string(res.first) + "x" + std::to_string(res.second),
                     verticalGapInfo > 0.0f,
                     "controlsTopY = " + std::to_string(controlsTopY) + " px, infoBottomY = " +
                     std::to_string(infoPanelBottomY) + " px, gap = " + std::to_string(verticalGapInfo) + " px");

        // Vertical gap Status block vs Controls:
        float statusBottomY = LayoutConstants::margin + LayoutConstants::panelPad + 10.0f + 4.0f * rowHeight + fontDescent;
        float verticalGapStatus = controlsTopY - statusBottomY;

        stats.record("Vertical clearance Status vs Controls at " + std::to_string(res.first) + "x" + std::to_string(res.second),
                     verticalGapStatus > 0.0f,
                     "statusBottomY = " + std::to_string(statusBottomY) + " px, gap = " + std::to_string(verticalGapStatus) + " px");
    }
}

// ============================================================================
// CHALLENGE 3: Rapid timeScale Mutation & String Formatting Extremes
// ============================================================================
void challengeTimeScaleFormatting(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 3: Rapid timeScale Mutation & String Formatting Extremes ===" << Color::Reset << "\n";

    // Sweep across timeScales from 0.001x to 10000x and degenerate values
    const std::vector<float> timeScales = {
        0.001f, 0.002f, 0.005f, 0.009f, 0.010f, 0.050f, 0.099f, 0.100f, 0.101f,
        0.250f, 0.500f, 0.750f, 1.000f, 1.250f, 2.000f, 5.000f, 10.000f, 25.0f,
        50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f,
        // Degenerate and stress values:
        0.0f, 0.0001f, 12345.67f, 99999.99f
    };

    float maxTimeWidth = 0.0f;
    std::string widestTimeStr;

    for (float ts : timeScales) {
        std::string trimmed = HudMirror::trimFloat(ts);
        std::string formatted = HudMirror::format("TIME  %s\xC3\x97", trimmed.c_str());

        // Basic sanity checks:
        bool notEmpty = !trimmed.empty() && !formatted.empty();
        bool noTrailingDot = (trimmed.back() != '.');
        bool containsMultiplicationSign = (formatted.find("\xC3\x97") != std::string::npos);

        float w = oracle.measure(formatted, LayoutConstants::rowScale);
        if (w > maxTimeWidth) {
            maxTimeWidth = w;
            widestTimeStr = formatted;
        }

        stats.record("timeScale=" + std::to_string(ts) + " -> '" + formatted + "'",
                     notEmpty && noTrailingDot && containsMultiplicationSign && !std::isnan(w) && w > 0.0f,
                     "Width = " + std::to_string(w) + " px");
    }

    stats.record("Widest TIME string bounded under 180 px",
                 maxTimeWidth < 180.0f,
                 "Widest: '" + widestTimeStr + "', Width = " + std::to_string(maxTimeWidth) + " px");
}

// ============================================================================
// CHALLENGE 4: Camera Distance Extremes & Unit Formatting
// ============================================================================
void challengeCameraDistanceFormatting(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 4: Camera Distance Extremes & Unit Formatting ===" << Color::Reset << "\n";

    const std::vector<double> distancesKm = {
        0.0,
        1.0,
        100.0,
        6371.0,          // Earth radius
        384400.0,        // Moon orbit
        1000000.0,       // 1M km
        1999999.0,       // Just below threshold
        2000000.0,       // Exactly threshold (Astro::moonDistanceLimitKm)
        2000001.0,       // Just above threshold
        149597870.7,     // 1.00 AU
        778500000.0,     // Jupiter (~5.2 AU)
        4500000000.0,    // Neptune (~30 AU)
        5906380000.0,    // Pluto (~39.5 AU)
        150000000000.0,  // 1000 AU (~1.5e11 km)
        1.0e12           // Deep space
    };

    float maxDistWidth = 0.0f;
    std::string widestDistStr;

    for (double d : distancesKm) {
        std::string distStr = "DIST  " + HudMirror::formatDistance(d);
        float w = oracle.measure(distStr, LayoutConstants::rowScale);
        if (w > maxDistWidth) {
            maxDistWidth = w;
            widestDistStr = distStr;
        }

        bool hasUnit = (distStr.find("km") != std::string::npos || distStr.find("AU") != std::string::npos);
        bool expectedUnit = (d >= Astro::moonDistanceLimitKm) ? (distStr.find("AU") != std::string::npos)
                                                              : (distStr.find("km") != std::string::npos);

        stats.record("Dist " + std::to_string(d) + " km -> '" + distStr + "'",
                     hasUnit && expectedUnit && !std::isnan(w) && w > 0.0f,
                     "Width = " + std::to_string(w) + " px");
    }

    stats.record("Widest DIST string bounded under 220 px",
                 maxDistWidth < 220.0f,
                 "Widest: '" + widestDistStr + "', Width = " + std::to_string(maxDistWidth) + " px");
}

// ============================================================================
// CHALLENGE 5: 10,000-Iteration Randomized Monte Carlo Fuzzing
// ============================================================================
void challengeMonteCarloFuzzing(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 5: 10,000-Iteration Randomized Monte Carlo Fuzzing ===" << Color::Reset << "\n";

    std::mt19937_64 rng(133742);
    std::uniform_int_distribution<unsigned int> widthDist(800, 3840);
    std::uniform_int_distribution<unsigned int> heightDist(600, 2160);
    std::uniform_real_distribution<float> fpsDist(0.0f, 1000.0f);
    std::uniform_real_distribution<double> unixDist(-1.0e8, 4.2e9); // 1966 to 2103
    std::uniform_real_distribution<float> logTimeDist(-3.0f, 4.0f);  // 10^-3 to 10^4
    std::uniform_real_distribution<double> logDistKm(0.0, 11.0);     // 1 to 10^11 km

    const std::vector<const Astro::BodyData*> catalog = {
        &Astro::Sun, &Astro::Mercury, &Astro::Venus, &Astro::Earth, &Astro::Moon,
        &Astro::Mars, &Astro::Jupiter, &Astro::Saturn, &Astro::Uranus, &Astro::Neptune, &Astro::Pluto
    };
    std::uniform_int_distribution<std::size_t> bodyDist(0, catalog.size() - 1);

    int collisionFailures = 0;
    int clippingFailures = 0;
    int invalidBoxFailures = 0;
    float minObservedGap = 1e9f;

    const std::string hint =
        "ARROWS  navigate      DRAG  orbit      WHEEL  zoom      "
        "SHIFT / CTRL  time      SPACE  reset      O  orbits";
    const float hintWidth = oracle.measure(hint, LayoutConstants::smallScale);
    const float fontAscent = oracle.getAscender(LayoutConstants::smallScale);
    const float rowHeight = oracle.getLineSpacing(LayoutConstants::rowScale);
    const float titleHeight = oracle.getLineSpacing(LayoutConstants::titleScale);

    const int ITERATIONS = 10000;
    for (int i = 0; i < ITERATIONS; ++i) {
        unsigned int w = widthDist(rng);
        unsigned int h = heightDist(rng);
        float fps = fpsDist(rng);
        double simSec = unixDist(rng);
        float timeScale = std::pow(10.0f, logTimeDist(rng));
        double camDistKm = std::pow(10.0, logDistKm(rng));
        const Astro::BodyData* body = catalog[bodyDist(rng)];

        // 1. Build Info Panel Box
        auto rows = HudMirror::buildRows(body, 2);
        float panelHeight = LayoutConstants::panelPad + 16.0f + titleHeight + 14.0f + rowHeight * rows.size() + LayoutConstants::panelPad;
        BoundingBox leftBox{LayoutConstants::margin, LayoutConstants::margin + LayoutConstants::panelWidth,
                           LayoutConstants::margin, LayoutConstants::margin + panelHeight};

        // 2. Build Status Box
        std::string fpsStr = HudMirror::format("%.0f FPS", fps);
        std::string utcStr = Astro::formatUtcDateTime(simSec);
        std::string timeStr = HudMirror::format("TIME  %s\xC3\x97", HudMirror::trimFloat(timeScale).c_str());
        std::string distStr = "DIST  " + HudMirror::formatDistance(camDistKm);

        float maxEntryW = std::max({
            oracle.measure(fpsStr, LayoutConstants::rowScale),
            oracle.measure(utcStr, LayoutConstants::rowScale),
            oracle.measure(timeStr, LayoutConstants::rowScale),
            oracle.measure(distStr, LayoutConstants::rowScale)
        });

        float statusLeftX = static_cast<float>(w) - LayoutConstants::margin - maxEntryW;
        float statusRightX = static_cast<float>(w) - LayoutConstants::margin;
        float statusTopY = LayoutConstants::margin + LayoutConstants::panelPad + 10.0f;
        float statusBottomY = statusTopY + 4.0f * rowHeight;
        BoundingBox statusBox{statusLeftX, statusRightX, statusTopY, statusBottomY};

        // Invariant Checks:
        if (!leftBox.isValid() || !statusBox.isValid()) {
            invalidBoxFailures++;
        }

        float gap = statusBox.xMin - leftBox.xMax;
        if (gap < minObservedGap) {
            minObservedGap = gap;
        }

        if (gap <= 0.0f || leftBox.overlaps(statusBox)) {
            collisionFailures++;
        }

        // Controls bar right edge check
        float controlsEnd = LayoutConstants::margin + hintWidth;
        if (controlsEnd > static_cast<float>(w) - LayoutConstants::margin) {
            clippingFailures++;
        }

        // Vertical collision check with controls bar
        float controlsTopY = static_cast<float>(h) - LayoutConstants::margin - fontAscent;
        if (leftBox.yMax >= controlsTopY || statusBox.yMax >= controlsTopY) {
            collisionFailures++;
        }
    }

    stats.record("10,000 Monte Carlo iterations: Zero invalid bounding boxes",
                 invalidBoxFailures == 0,
                 "Failures = " + std::to_string(invalidBoxFailures));

    stats.record("10,000 Monte Carlo iterations: Zero panel collisions",
                 collisionFailures == 0,
                 "Failures = " + std::to_string(collisionFailures) + ", Min Observed Gap = " + std::to_string(minObservedGap) + " px");

    stats.record("10,000 Monte Carlo iterations: Zero controls bar clipping",
                 clippingFailures == 0,
                 "Failures = " + std::to_string(clippingFailures));
}

// ============================================================================
// CHALLENGE 6: Sub-Specification Stress & Critical Resolution Analysis
// ============================================================================
void challengeCriticalResolutionAnalysis(ChallengeStats& stats, const FreeTypeOracle& oracle) {
    std::cout << "\n" << Color::Bold << "=== CHALLENGE 6: Sub-Specification Stress & Critical Thresholds ===" << Color::Reset << "\n";

    const std::string hint =
        "ARROWS  navigate      DRAG  orbit      WHEEL  zoom      "
        "SHIFT / CTRL  time      SPACE  reset      O  orbits";
    const float hintWidth = oracle.measure(hint, LayoutConstants::smallScale);

    // Critical width where controls hint clips:
    // W_crit_clip = 2 * margin + hintWidth = 48.0 + hintWidth
    float wCritClip = 2.0f * LayoutConstants::margin + hintWidth;

    // Worst-case status entry width
    std::string worstUtc = "2026-10-21 04:00:00 UTC";
    float utcWidth = oracle.measure(worstUtc, LayoutConstants::rowScale);
    float worstStatusWidth = std::max(utcWidth, oracle.measure("DIST  1 999 999 km", LayoutConstants::rowScale));

    // Critical width where status collides with left panel:
    // W_crit_collide = panelWidth + 2 * margin + worstStatusWidth = 340 + 48 + worstStatusWidth = 388 + worstStatusWidth
    float wCritCollide = LayoutConstants::panelWidth + 2.0f * LayoutConstants::margin + worstStatusWidth;

    stats.record("Analytical Critical Clipping Width W_crit_clip < 700 px",
                 wCritClip < 700.0f,
                 "W_crit_clip = " + std::to_string(wCritClip) + " px");

    stats.record("Analytical Critical Collision Width W_crit_collide < 600 px",
                 wCritCollide < 600.0f,
                 "W_crit_collide = " + std::to_string(wCritCollide) + " px");

    // Empirical safety margins at 800px
    float clipMargin800 = 800.0f - wCritClip;
    float collideMargin800 = 800.0f - wCritCollide;

    stats.record("Empirical Clipping Safety Margin at 800px > 100 px",
                 clipMargin800 > 100.0f,
                 "Safety Margin = " + std::to_string(clipMargin800) + " px");

    stats.record("Empirical Collision Safety Margin at 800px > 200 px",
                 collideMargin800 > 200.0f,
                 "Safety Margin = " + std::to_string(collideMargin800) + " px");
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << "  EMPIRICAL ADVERSARIAL CHALLENGE — MILESTONE M4 HUD & GEOMETRY     \n";
    std::cout << "====================================================================\n";

    ChallengeStats stats;
    try {
        FreeTypeOracle oracle("resources/font.ttf", 32);

        challengeMultiResolutionClearance(stats, oracle);
        challengeBottomControlsSafety(stats, oracle);
        challengeTimeScaleFormatting(stats, oracle);
        challengeCameraDistanceFormatting(stats, oracle);
        challengeMonteCarloFuzzing(stats, oracle);
        challengeCriticalResolutionAnalysis(stats, oracle);

    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n====================================================================\n";
    std::cout << "  CHALLENGE SUMMARY: " << stats.passed << " / " << stats.totalChecks
              << " PASSED (" << stats.failed << " FAILED)\n";
    std::cout << "====================================================================\n";

    return (stats.failed == 0) ? 0 : 1;
}
