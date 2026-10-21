#pragma once
// This header used to include only <SFML/System/Clock.hpp> and compiled purely
// because of the lucky include order in main.cc. It now declares its own
// dependencies and can be included on its own.
#include "../glad/gl.h"
#include <SFML/Window.hpp>
#include <algorithm>
#include <optional>
#include "Camera.hpp"
#include "SolarSystem.hpp"

// User interaction state that has to survive from one frame to the next.
struct InputState {
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    bool  dragging   = false;
    float timeScale  = 1.0f;

    // Interactive Date & Time Picker
    bool timePickerOpen = false;
    int pickerField = 0; // 0=Year, 1=Month, 2=Day, 3=Hour, 4=Min, 5=Sec
    int pickerYear = 2026;
    int pickerMonth = 10;
    int pickerDay = 21;
    int pickerHour = 0;
    int pickerMin = 0;
    int pickerSec = 0;
    std::string digitBuffer;
};

class InputHandler {
private:
    static constexpr float minTimeScale = 0.001f;
    static constexpr float maxTimeScale = 100000.0f;

public:
    static void openTimePicker(SolarSystem& system, InputState& state) {
        state.timePickerOpen = true;
        state.pickerField = 0;
        state.digitBuffer.clear();
        state.dragging = false;

        Astro::unixToUtcDateTimeParts(system.getSimUnixSeconds(),
                                      state.pickerYear, state.pickerMonth, state.pickerDay,
                                      state.pickerHour, state.pickerMin, state.pickerSec);
    }

    static void closeTimePicker(InputState& state) {
        state.timePickerOpen = false;
        state.digitBuffer.clear();
    }

    static void applyTimePicker(SolarSystem& system, InputState& state) {
        Astro::clampPickerValues(state.pickerYear, state.pickerMonth, state.pickerDay,
                                 state.pickerHour, state.pickerMin, state.pickerSec);
        const double newUnix = Astro::utcDateTimeToUnix(state.pickerYear, state.pickerMonth, state.pickerDay,
                                                        state.pickerHour, state.pickerMin, state.pickerSec);
        system.setSimUnixSeconds(newUnix);
        closeTimePicker(state);
    }

    static void resetToCurrentUtc(InputState& state) {
        const auto now = std::chrono::system_clock::now();
        const double unixNow = std::chrono::duration<double>(now.time_since_epoch()).count();
        Astro::unixToUtcDateTimeParts(unixNow, state.pickerYear, state.pickerMonth, state.pickerDay,
                                      state.pickerHour, state.pickerMin, state.pickerSec);
        state.digitBuffer.clear();
    }

    static void adjustPickerField(InputState& state, int field, int delta) {
        state.digitBuffer.clear();
        switch (field) {
            case 0: state.pickerYear += delta; break;
            case 1: {
                state.pickerMonth += delta;
                while (state.pickerMonth < 1)  state.pickerMonth += 12;
                while (state.pickerMonth > 12) state.pickerMonth -= 12;
                break;
            }
            case 2: {
                int maxD = Astro::daysInMonth(state.pickerYear, state.pickerMonth);
                state.pickerDay += delta;
                while (state.pickerDay < 1)    state.pickerDay += maxD;
                while (state.pickerDay > maxD) state.pickerDay -= maxD;
                break;
            }
            case 3: {
                state.pickerHour = (state.pickerHour + delta) % 24;
                if (state.pickerHour < 0) state.pickerHour += 24;
                break;
            }
            case 4: {
                state.pickerMin = (state.pickerMin + delta) % 60;
                if (state.pickerMin < 0) state.pickerMin += 60;
                break;
            }
            case 5: {
                state.pickerSec = (state.pickerSec + delta) % 60;
                if (state.pickerSec < 0) state.pickerSec += 60;
                break;
            }
            default: break;
        }
        Astro::clampPickerValues(state.pickerYear, state.pickerMonth, state.pickerDay,
                                 state.pickerHour, state.pickerMin, state.pickerSec);
    }

    static void handleDigit(InputState& state, int digit) {
        state.digitBuffer += static_cast<char>('0' + digit);
        const int val = std::stoi(state.digitBuffer);

        switch (state.pickerField) {
            case 0: { // Year
                state.pickerYear = val;
                if (state.digitBuffer.size() >= 4) {
                    state.pickerField = 1;
                    state.digitBuffer.clear();
                }
                break;
            }
            case 1: { // Month
                if (state.digitBuffer.size() == 1) {
                    if (val > 1) {
                        state.pickerMonth = val;
                        state.pickerField = 2;
                        state.digitBuffer.clear();
                    } else {
                        state.pickerMonth = (val == 0 ? 1 : val);
                    }
                } else {
                    state.pickerMonth = std::clamp(val, 1, 12);
                    state.pickerField = 2;
                    state.digitBuffer.clear();
                }
                break;
            }
            case 2: { // Day
                const int maxD = Astro::daysInMonth(state.pickerYear, state.pickerMonth);
                if (state.digitBuffer.size() == 1) {
                    if (val > 3) {
                        state.pickerDay = std::min(val, maxD);
                        state.pickerField = 3;
                        state.digitBuffer.clear();
                    } else {
                        state.pickerDay = (val == 0 ? 1 : val);
                    }
                } else {
                    state.pickerDay = std::clamp(val, 1, maxD);
                    state.pickerField = 3;
                    state.digitBuffer.clear();
                }
                break;
            }
            case 3: { // Hour
                if (state.digitBuffer.size() == 1) {
                    if (val > 2) {
                        state.pickerHour = val;
                        state.pickerField = 4;
                        state.digitBuffer.clear();
                    } else {
                        state.pickerHour = val;
                    }
                } else {
                    state.pickerHour = std::clamp(val, 0, 23);
                    state.pickerField = 4;
                    state.digitBuffer.clear();
                }
                break;
            }
            case 4: { // Minute
                if (state.digitBuffer.size() == 1) {
                    if (val > 5) {
                        state.pickerMin = val;
                        state.pickerField = 5;
                        state.digitBuffer.clear();
                    } else {
                        state.pickerMin = val;
                    }
                } else {
                    state.pickerMin = std::clamp(val, 0, 59);
                    state.pickerField = 5;
                    state.digitBuffer.clear();
                }
                break;
            }
            case 5: { // Second
                if (state.digitBuffer.size() == 1) {
                    state.pickerSec = val;
                } else {
                    state.pickerSec = std::clamp(val, 0, 59);
                    state.digitBuffer.clear();
                }
                break;
            }
            default: break;
        }
        Astro::clampPickerValues(state.pickerYear, state.pickerMonth, state.pickerDay,
                                 state.pickerHour, state.pickerMin, state.pickerSec);
    }

    static void handleEvents(sf::Window& window, Camera& camera, SolarSystem& system,
                             bool& running, InputState& state) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                running = false;
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (pressed->button == sf::Mouse::Button::Left) {
                    const float mx = static_cast<float>(pressed->position.x);
                    const float my = static_cast<float>(pressed->position.y);
                    const auto winSize = window.getSize();

                    if (state.timePickerOpen) {
                        const TimePickerLayout L = TimePickerLayout::compute(winSize.x, winSize.y);
                        if (TimePickerLayout::contains(mx, my, L.dlgX, L.dlgY, L.dlgW, L.dlgH)) {
                            if (TimePickerLayout::contains(mx, my, L.btnCloseX, L.btnCloseY, L.btnCloseW, L.btnCloseH)) {
                                closeTimePicker(state);
                                continue;
                            }
                            if (TimePickerLayout::contains(mx, my, L.btnApplyX, L.btnApplyY, L.btnApplyW, L.btnApplyH)) {
                                applyTimePicker(system, state);
                                continue;
                            }
                            if (TimePickerLayout::contains(mx, my, L.btnCancelX, L.btnCancelY, L.btnCancelW, L.btnCancelH)) {
                                closeTimePicker(state);
                                continue;
                            }
                            if (TimePickerLayout::contains(mx, my, L.btnNowX, L.btnNowY, L.btnNowW, L.btnNowH)) {
                                resetToCurrentUtc(state);
                                continue;
                            }
                            for (int i = 0; i < 6; ++i) {
                                if (TimePickerLayout::contains(mx, my, L.fields[i].plusX, L.fields[i].plusY, L.fields[i].plusW, L.fields[i].plusH)) {
                                    state.pickerField = i;
                                    adjustPickerField(state, i, +1);
                                    break;
                                }
                                if (TimePickerLayout::contains(mx, my, L.fields[i].minusX, L.fields[i].minusY, L.fields[i].minusW, L.fields[i].minusH)) {
                                    state.pickerField = i;
                                    adjustPickerField(state, i, -1);
                                    break;
                                }
                                if (TimePickerLayout::contains(mx, my, L.fields[i].x, L.fields[i].y, L.fields[i].w, L.fields[i].h)) {
                                    state.pickerField = i;
                                    state.digitBuffer.clear();
                                    break;
                                }
                            }
                        } else {
                            closeTimePicker(state);
                        }
                        continue;
                    } else {
                        const float winW = static_cast<float>(winSize.x);
                        if (mx >= winW - 250.0f && mx <= winW - 20.0f && my >= 48.0f && my <= 78.0f) {
                            openTimePicker(system, state);
                            continue;
                        }
                        state.dragging = true;
                        state.lastMouseX = mx;
                        state.lastMouseY = my;
                    }
                }
            }
            else if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (released->button == sf::Mouse::Button::Left) {
                    state.dragging = false;
                }
            }
            else if (event->is<sf::Event::FocusLost>() || event->is<sf::Event::MouseLeft>()) {
                state.dragging = false;
            }
            else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
                const float x = static_cast<float>(mouse->position.x);
                const float y = static_cast<float>(mouse->position.y);

                if (state.dragging && !state.timePickerOpen) {
                    camera.rotate(x - state.lastMouseX, y - state.lastMouseY);
                }

                state.lastMouseX = x;
                state.lastMouseY = y;
            }
            else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (scroll->wheel == sf::Mouse::Wheel::Vertical) {
                    if (state.timePickerOpen) {
                        const int delta = (scroll->delta > 0.0f) ? 1 : -1;
                        adjustPickerField(state, state.pickerField, delta);
                    } else {
                        camera.zoom(scroll->delta);
                    }
                }
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                handleKey(key->code, system, state);
            }
        }
    }

private:
    static void handleKey(sf::Keyboard::Key code, SolarSystem& system, InputState& state) {
        if (state.timePickerOpen) {
            if (code == sf::Keyboard::Key::Escape || code == sf::Keyboard::Key::T) {
                closeTimePicker(state);
                return;
            }
            if (code == sf::Keyboard::Key::Enter) {
                applyTimePicker(system, state);
                return;
            }
            if (code == sf::Keyboard::Key::Left) {
                state.digitBuffer.clear();
                state.pickerField = (state.pickerField + 5) % 6;
                return;
            }
            if (code == sf::Keyboard::Key::Right || code == sf::Keyboard::Key::Tab) {
                state.digitBuffer.clear();
                state.pickerField = (state.pickerField + 1) % 6;
                return;
            }
            if (code == sf::Keyboard::Key::Up || code == sf::Keyboard::Key::Add || code == sf::Keyboard::Key::Equal) {
                adjustPickerField(state, state.pickerField, +1);
                return;
            }
            if (code == sf::Keyboard::Key::Down || code == sf::Keyboard::Key::Subtract || code == sf::Keyboard::Key::Hyphen) {
                adjustPickerField(state, state.pickerField, -1);
                return;
            }
            if (code == sf::Keyboard::Key::Backspace) {
                if (!state.digitBuffer.empty()) {
                    state.digitBuffer.pop_back();
                } else if (state.pickerField > 0) {
                    state.pickerField--;
                }
                return;
            }
            if (code == sf::Keyboard::Key::N) {
                resetToCurrentUtc(state);
                return;
            }
            if (code >= sf::Keyboard::Key::Num0 && code <= sf::Keyboard::Key::Num9) {
                handleDigit(state, static_cast<int>(code) - static_cast<int>(sf::Keyboard::Key::Num0));
                return;
            }
            if (code >= sf::Keyboard::Key::Numpad0 && code <= sf::Keyboard::Key::Numpad9) {
                handleDigit(state, static_cast<int>(code) - static_cast<int>(sf::Keyboard::Key::Numpad0));
                return;
            }
            return;
        }

        switch (code) {
            case sf::Keyboard::Key::Up:    system.selectParent();          break;
            case sf::Keyboard::Key::Down:  system.selectFirstChild();      break;
            case sf::Keyboard::Key::Right: system.selectNextSibling();     break;
            case sf::Keyboard::Key::Left:  system.selectPreviousSibling(); break;

            case sf::Keyboard::Key::LShift:
            case sf::Keyboard::Key::RShift:   state.timeScale *= 1.5f; break;
            case sf::Keyboard::Key::LControl:
            case sf::Keyboard::Key::RControl: state.timeScale /= 1.5f; break;
            case sf::Keyboard::Key::Space:    state.timeScale  = 1.0f; break;

            case sf::Keyboard::Key::O:        system.toggleOrbitLines();      break;
            case sf::Keyboard::Key::T:        openTimePicker(system, state);  break;

            default: return;
        }

        // Without this bound the time scale diverges and the orbits advance in
        // steps large enough to lose all coherence.
        state.timeScale = std::clamp(state.timeScale, minTimeScale, maxTimeScale);
    }
};
