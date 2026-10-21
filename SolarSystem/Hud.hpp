#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "AstronomicalData.hpp"
#include "TextRenderer.hpp"

// Projected 2D screen marker for a distant solar-system body.
struct BodyMarker {
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 4.0f;
    float dotAlpha = 0.8f;
    float labelAlpha = 0.8f;
    float prominence = 0.5f;
    glm::vec3 color{1.0f};
};

// Layout and hitboxes for the interactive date and time picker.
struct TimePickerLayout {
    float dlgX = 0.0f, dlgY = 0.0f, dlgW = 460.0f, dlgH = 210.0f;
    struct FieldBox {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        float plusX = 0.0f, plusY = 0.0f, plusW = 0.0f, plusH = 0.0f;
        float minusX = 0.0f, minusY = 0.0f, minusW = 0.0f, minusH = 0.0f;
    } fields[6];
    float btnNowX = 0.0f, btnNowY = 0.0f, btnNowW = 90.0f, btnNowH = 26.0f;
    float btnCancelX = 0.0f, btnCancelY = 0.0f, btnCancelW = 100.0f, btnCancelH = 26.0f;
    float btnApplyX = 0.0f, btnApplyY = 0.0f, btnApplyW = 140.0f, btnApplyH = 26.0f;
    float btnCloseX = 0.0f, btnCloseY = 0.0f, btnCloseW = 24.0f, btnCloseH = 24.0f;

    static TimePickerLayout compute(float screenW, float screenH) {
        TimePickerLayout L;
        L.dlgX = std::max(24.0f, (screenW - L.dlgW) * 0.5f);
        L.dlgY = std::max(24.0f, (screenH - L.dlgH) * 0.42f);

        L.btnCloseX = L.dlgX + L.dlgW - 28.0f;
        L.btnCloseY = L.dlgY + 8.0f;

        const float boxY = L.dlgY + 82.0f;
        const float boxH = 30.0f;
        const float widths[6] = {68.0f, 40.0f, 40.0f, 40.0f, 40.0f, 40.0f};
        const float gaps[5] = {14.0f, 14.0f, 26.0f, 14.0f, 14.0f};

        float curX = L.dlgX + (L.dlgW - 336.0f) * 0.5f;
        for (int i = 0; i < 6; ++i) {
            L.fields[i].x = curX;
            L.fields[i].y = boxY;
            L.fields[i].w = widths[i];
            L.fields[i].h = boxH;

            L.fields[i].plusX = curX;
            L.fields[i].plusY = boxY - 20.0f;
            L.fields[i].plusW = widths[i];
            L.fields[i].plusH = 17.0f;

            L.fields[i].minusX = curX;
            L.fields[i].minusY = boxY + boxH + 3.0f;
            L.fields[i].minusW = widths[i];
            L.fields[i].minusH = 17.0f;

            curX += widths[i];
            if (i < 5) curX += gaps[i];
        }

        const float btnY = L.dlgY + L.dlgH - 38.0f;
        L.btnNowX = L.dlgX + 35.0f;
        L.btnNowY = btnY;
        L.btnCancelX = L.dlgX + 145.0f;
        L.btnCancelY = btnY;
        L.btnApplyX = L.dlgX + 265.0f;
        L.btnApplyY = btnY;

        return L;
    }

    static bool contains(float px, float py, float rx, float ry, float rw, float rh) {
        return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
    }
};

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
    std::string utcTime;
    std::vector<BodyMarker> markers;
    bool timePickerOpen = false;
    int pickerField = 0; // 0=Year, 1=Month, 2=Day, 3=Hour, 4=Min, 5=Sec
    int pickerYear = 2026;
    int pickerMonth = 10;
    int pickerDay = 21;
    int pickerHour = 0;
    int pickerMin = 0;
    int pickerSec = 0;
};

class Hud {
public:
    Hud() : text("resources/font.ttf", 32) {}

    void draw(const HudFrame& frame, unsigned int width, unsigned int height) {
        text.begin(width, height);

        drawMarkers(frame.markers);
        drawInfoPanel(frame);
        drawStatus(frame, static_cast<float>(width));
        drawControls(static_cast<float>(height));
        if (frame.timePickerOpen) {
            drawTimePicker(frame, static_cast<float>(width), static_cast<float>(height));
        }

        text.end();
    }

    // The body whose dot or label is under (x, y), or "" for none. Same label
    // placement as drawMarkers; the closest dot wins where several overlap.
    std::string markerAt(const std::vector<BodyMarker>& markers, float x, float y) const {
        std::string best;
        float bestDist = 1e30f;
        for (const BodyMarker& m : markers) {
            if (m.labelAlpha <= 0.05f && m.dotAlpha <= 0.01f) continue;
            const float lx = m.x + m.radius + 5.0f, ly = m.y + 4.0f;
            const bool onLabel = x >= lx - 2.0f && x <= lx + text.measure(m.name, markerScale) + 2.0f &&
                                 y >= ly - 13.0f && y <= ly + 4.0f;
            const float d = std::hypot(x - m.x, y - m.y);
            if ((onLabel || d <= m.radius + 8.0f) && d < bestDist) { best = m.name; bestDist = d; }
        }
        return best;
    }

private:
    static constexpr float markerScale = 0.36f;
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

    void drawMarkers(const std::vector<BodyMarker>& markers) {
        // Draw luminous dots for unresolved bodies (dotAlpha > 0).
        for (const BodyMarker& m : markers) {
            if (m.dotAlpha > 0.01f && m.radius > 0.5f) {
                text.dot(m.x, m.y, m.radius, glm::vec4(m.color, m.dotAlpha));
            }
        }

        // Draw names next to each body, skipping labels that would overlap a
        // brighter body's label already placed nearby.
        std::vector<glm::vec2> placedLabels;
        for (const BodyMarker& m : markers) {
            if (m.labelAlpha <= 0.05f) continue;

            const float lx = m.x + m.radius + 5.0f;
            const float ly = m.y + 4.0f;

            bool overlaps = false;
            for (const glm::vec2& p : placedLabels) {
                if (std::abs(lx - p.x) < 52.0f && std::abs(ly - p.y) < 13.0f) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) continue;

            placedLabels.emplace_back(lx, ly);
            text.text(m.name, lx, ly, markerScale, glm::vec4(bright.r, bright.g, bright.b, m.labelAlpha));
        }
    }

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
            frame.utcTime,
            format("TIME  %s\xC3\x97%s", trimFloat(frame.timeScale).c_str(), simPerSecond(frame.timeScale).c_str()),
            "DIST  " + formatDistance(frame.cameraDistance),
        };

        bool first = true;
        for (const std::string& entry : entries) {
            if (entry.empty()) continue;
            const float w = text.measure(entry, rowScale);
            text.text(entry, width - margin - w, y, rowScale, first ? accent : dim);
            y += rowHeight;
            first = false;
        }
    }

    void drawControls(float height) {
        const std::string hint =
            "CLICK  fly to    ARROWS  navigate    DRAG  orbit    WHEEL  zoom    "
            "SHIFT / CTRL  time    SPACE  reset    O  orbits    T  date";

        text.text(hint, margin, height - margin, smallScale, dim);
    }

    void drawTimePicker(const HudFrame& frame, float screenW, float screenH) {
        const TimePickerLayout L = TimePickerLayout::compute(screenW, screenH);

        // Semi-transparent backdrop overlay to dim the scene slightly
        text.rect(0.0f, 0.0f, screenW, screenH, glm::vec4(0.0f, 0.0f, 0.0f, 0.40f));

        // Dialog background panel with subtle borders
        text.rect(L.dlgX, L.dlgY, L.dlgW, L.dlgH, glm::vec4(0.03f, 0.04f, 0.08f, 0.94f));
        text.rect(L.dlgX, L.dlgY, L.dlgW, 3.0f, accent); // Top spine
        text.rect(L.dlgX, L.dlgY, 1.0f, L.dlgH, glm::vec4(dim.r, dim.g, dim.b, 0.25f));
        text.rect(L.dlgX + L.dlgW - 1.0f, L.dlgY, 1.0f, L.dlgH, glm::vec4(dim.r, dim.g, dim.b, 0.25f));
        text.rect(L.dlgX, L.dlgY + L.dlgH - 1.0f, L.dlgW, 1.0f, glm::vec4(dim.r, dim.g, dim.b, 0.25f));

        // Header and Close button
        text.text("IMPOSTA DATA E ORA UTC", L.dlgX + 20.0f, L.dlgY + 28.0f, 0.58f, bright);
        text.text("X", L.btnCloseX + 6.0f, L.btnCloseY + 16.0f, 0.48f, dim);

        text.text("FRECCE/TAB: scegli   SU/GIU/+/-: cambia   0-9: digita",
                  L.dlgX + 20.0f, L.dlgY + 48.0f, smallScale, dim);

        text.rect(L.dlgX + 20.0f, L.dlgY + 56.0f, L.dlgW - 40.0f, 1.0f,
                  glm::vec4(accent.r, accent.g, accent.b, 0.30f));

        // Values formatting
        const int vals[6] = {frame.pickerYear, frame.pickerMonth, frame.pickerDay,
                             frame.pickerHour, frame.pickerMin, frame.pickerSec};
        const char* const valFmts[6] = {"%04d", "%02d", "%02d", "%02d", "%02d", "%02d"};
        const char* const labels[6] = {"ANNO", "MESE", "GIORNO", "ORA", "MIN", "SEC"};

        for (int i = 0; i < 6; ++i) {
            const auto& f = L.fields[i];
            const bool active = (frame.pickerField == i);

            // Column title
            const float lblW = text.measure(labels[i], 0.30f);
            text.text(labels[i], f.x + (f.w - lblW) * 0.5f, f.y - 24.0f, 0.30f, active ? accent : dim);

            // Plus button
            text.rect(f.plusX, f.plusY, f.plusW, f.plusH, glm::vec4(0.08f, 0.10f, 0.16f, 0.85f));
            text.text("+", f.plusX + f.plusW * 0.5f - 4.0f, f.plusY + 13.0f, 0.42f, active ? accent : dim);

            // Field box
            if (active) {
                text.rect(f.x - 1.0f, f.y - 1.0f, f.w + 2.0f, f.h + 2.0f, accent);
                text.rect(f.x, f.y, f.w, f.h, glm::vec4(0.14f, 0.16f, 0.24f, 0.98f));
            } else {
                text.rect(f.x, f.y, f.w, f.h, glm::vec4(0.06f, 0.08f, 0.13f, 0.85f));
            }

            char valBuf[16];
            std::snprintf(valBuf, sizeof(valBuf), valFmts[i], vals[i]);
            const std::string valStr(valBuf);
            const float valW = text.measure(valStr, 0.52f);
            text.text(valStr, f.x + (f.w - valW) * 0.5f, f.y + 22.0f, 0.52f, active ? accent : bright);

            // Minus button
            text.rect(f.minusX, f.minusY, f.minusW, f.minusH, glm::vec4(0.08f, 0.10f, 0.16f, 0.85f));
            text.text("-", f.minusX + f.minusW * 0.5f - 3.0f, f.minusY + 12.0f, 0.42f, active ? accent : dim);
        }

        // Separators
        text.text("-", L.fields[0].x + L.fields[0].w + 3.0f, L.fields[0].y + 20.0f, 0.50f, dim);
        text.text("-", L.fields[1].x + L.fields[1].w + 3.0f, L.fields[1].y + 20.0f, 0.50f, dim);
        text.text(":", L.fields[3].x + L.fields[3].w + 4.0f, L.fields[3].y + 20.0f, 0.50f, dim);
        text.text(":", L.fields[4].x + L.fields[4].w + 4.0f, L.fields[4].y + 20.0f, 0.50f, dim);

        // Bottom Action buttons
        // [ ADESSO ]
        text.rect(L.btnNowX, L.btnNowY, L.btnNowW, L.btnNowH, glm::vec4(0.08f, 0.10f, 0.16f, 0.85f));
        text.text("ADESSO", L.btnNowX + 14.0f, L.btnNowY + 18.0f, 0.40f, bright);

        // [ ANNULLA ]
        text.rect(L.btnCancelX, L.btnCancelY, L.btnCancelW, L.btnCancelH, glm::vec4(0.08f, 0.10f, 0.16f, 0.85f));
        text.text("ANNULLA", L.btnCancelX + 16.0f, L.btnCancelY + 18.0f, 0.40f, dim);

        // [ APPLICA ]
        text.rect(L.btnApplyX, L.btnApplyY, L.btnApplyW, L.btnApplyH,
                  glm::vec4(accent.r * 0.25f, accent.g * 0.25f, accent.b * 0.25f, 0.90f));
        text.rect(L.btnApplyX, L.btnApplyY, L.btnApplyW, 1.5f, accent);
        text.text("APPLICA (Invio)", L.btnApplyX + 12.0f, L.btnApplyY + 18.0f, 0.40f, accent);
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

    // Past an hour a second the multiplier alone is hard to read: "  2.4 d/s".
    static std::string simPerSecond(float scale) {
        if (scale >= 365.25f * 86400.0f) return format("  %.1f yr/s", scale / (365.25f * 86400.0f));
        if (scale >= 86400.0f)           return format("  %.1f d/s", scale / 86400.0f);
        if (scale >= 3600.0f)            return format("  %.1f h/s", scale / 3600.0f);
        return "";
    }

    // 1 -> "1", 1.5 -> "1.5", 0.004 -> "0.004": keeps the multiplier readable
    // across the whole 0.001x - 1000x range without a wall of zeros, whole numbers above.
    static std::string trimFloat(float value) {
        if (std::abs(value) >= 1000.0f) return format("%.0f", value);
        std::string s = (std::abs(value) < 0.1f) ? format("%.3f", value)
                                                 : format("%.2f", value);
        while (s.size() > 1 && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }
};
