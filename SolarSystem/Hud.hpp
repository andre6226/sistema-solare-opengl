#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "AstronomicalData.hpp"
#include "TextRenderer.hpp"

// Everything the overlay needs for one frame, gathered by the caller so the HUD
// stays independent of the simulation classes.
struct HudFrame {
    std::string targetName;
    std::string targetPath;
    const Astro::BodyData* data = nullptr;
    int satelliteCount = 0;
    float timeScale = 1.0f;
    float cameraDistance = 0.0f;
    float fps = 0.0f;
};

class Hud {
public:
    Hud() : text("resources/font.ttf", 32) {}

    void draw(const HudFrame& frame, unsigned int width, unsigned int height) {
        text.begin(width, height);

        drawInfoPanel(frame);
        drawStatus(frame, static_cast<float>(width));
        drawControls(static_cast<float>(height));

        text.end();
    }

private:
    // Authored in sRGB and written straight to the framebuffer: TextRenderer
    // turns the automatic encode off for the duration of the overlay.
    static constexpr glm::vec4 accent{1.00f, 0.72f, 0.30f, 1.00f};
    static constexpr glm::vec4 bright{0.94f, 0.95f, 0.97f, 1.00f};
    static constexpr glm::vec4 dim{0.58f, 0.62f, 0.70f, 1.00f};
    static constexpr glm::vec4 panel{0.03f, 0.04f, 0.07f, 0.72f};

    static constexpr float titleScale = 0.92f;
    static constexpr float rowScale   = 0.50f;
    static constexpr float smallScale = 0.42f;

    static constexpr float margin      = 24.0f;
    static constexpr float panelWidth  = 340.0f;
    static constexpr float panelPad    = 18.0f;
    static constexpr float valueColumn = 165.0f;

    TextRenderer text;

    void drawInfoPanel(const HudFrame& frame) {
        const std::vector<std::pair<std::string, std::string>> rows = buildRows(frame);

        const float rowHeight = text.lineHeight(rowScale);
        const float panelHeight = panelPad + 16.0f + text.lineHeight(titleScale)
                                + 14.0f + rowHeight * rows.size() + panelPad;

        text.rect(margin, margin, panelWidth, panelHeight, panel);
        text.rect(margin, margin, 3.0f, panelHeight, accent); // accent spine

        const float x = margin + panelPad;
        float y = margin + panelPad + 10.0f;

        text.text(frame.targetPath, x, y, smallScale, dim);

        y += text.lineHeight(titleScale) * 0.88f;
        text.text(frame.targetName, x, y, titleScale, bright);

        y += 14.0f;
        text.rect(x, y, panelWidth - 2.0f * panelPad, 1.0f,
                  glm::vec4(accent.r, accent.g, accent.b, 0.35f));

        for (const auto& row : rows) {
            y += rowHeight;
            text.text(row.first, x, y, rowScale, dim);
            text.text(row.second, x + valueColumn, y, rowScale, bright);
        }
    }

    void drawStatus(const HudFrame& frame, float width) {
        const float rowHeight = text.lineHeight(rowScale);
        float y = margin + panelPad + 10.0f;

        const std::string entries[] = {
            format("%.0f FPS", frame.fps),
            format("TIME  %s\xC3\x97", trimFloat(frame.timeScale).c_str()),
            format("DIST  %.2f u", frame.cameraDistance),
        };

        bool first = true;
        for (const std::string& entry : entries) {
            const float w = text.measure(entry, rowScale);
            text.text(entry, width - margin - w, y, rowScale, first ? accent : dim);
            y += rowHeight;
            first = false;
        }
    }

    void drawControls(float height) {
        const std::string hint =
            "ARROWS  navigate      DRAG  orbit      WHEEL  zoom      "
            "SHIFT / CTRL  time      SPACE  reset";

        text.text(hint, margin, height - margin, smallScale, dim);
    }

    std::vector<std::pair<std::string, std::string>> buildRows(const HudFrame& frame) const {
        std::vector<std::pair<std::string, std::string>> rows;
        const Astro::BodyData* d = frame.data;
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
            rows.emplace_back("DAY LENGTH",
                              formatDays(std::abs(d->rotationPeriodDays)) +
                              (retrograde ? "  retro" : ""));
        }

        rows.emplace_back("AXIAL TILT", format("%.2f\xC2\xB0", d->axialTiltDegrees));

        if (frame.satelliteCount > 0) {
            rows.emplace_back("SATELLITES", format("%d", frame.satelliteCount));
        }

        return rows;
    }

    // --- formatting helpers ---

    template <typename... Args>
    static std::string format(const char* pattern, Args... args) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), pattern, args...);
        return std::string(buffer);
    }

    static constexpr double kmPerAu = 149597870.7;

    static std::string formatKm(double km) {
        return groupDigits(static_cast<long long>(km + 0.5)) + " km";
    }

    // Planetary orbits are quoted in astronomical units, which is how they are
    // actually talked about; a moon's orbit is small enough that kilometres stay
    // the readable choice. The threshold is the same one the scale converter
    // uses to tell a moon from a planet.
    static std::string formatDistance(double km) {
        if (km >= Astro::moonDistanceLimitKm) return format("%.2f AU", km / kmPerAu);
        return formatKm(km);
    }

    static std::string formatDays(double days) {
        if (days >= 365.25) return format("%.2f yr", days / 365.25);
        if (days >= 1.0)    return format("%.2f d", days);
        return format("%.1f h", days * 24.0);
    }

    // 149600000 -> "149 600 000", easier to read at a glance than a bare run of
    // digits. A thin space would be the typographic choice, but the atlas only
    // carries Latin-1.
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

    // 1 -> "1", 1.5 -> "1.5", 0.004 -> "0.004": keeps the multiplier readable
    // across the whole 0.001x - 1000x range without a wall of zeros.
    static std::string trimFloat(float value) {
        std::string s = (std::abs(value) < 0.1f) ? format("%.3f", value)
                                                 : format("%.2f", value);
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }
};
