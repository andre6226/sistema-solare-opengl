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
};

class InputHandler {
private:
    static constexpr float minTimeScale = 0.001f;
    static constexpr float maxTimeScale = 1000.0f;

public:
    static void handleEvents(sf::Window& window, Camera& camera, SolarSystem& system,
                             bool& running, InputState& state) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                running = false;
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            // Dragging starts with a press INSIDE the window. This used to read
            // sf::Mouse::isButtonPressed, which queries the global OS state:
            // pressing the button over another application, or leaving and
            // re-entering the window while holding it, made the camera jump.
            else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (pressed->button == sf::Mouse::Button::Left) {
                    state.dragging = true;
                    state.lastMouseX = static_cast<float>(pressed->position.x);
                    state.lastMouseY = static_cast<float>(pressed->position.y);
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

                if (state.dragging) {
                    camera.rotate(x - state.lastMouseX, y - state.lastMouseY);
                }

                state.lastMouseX = x;
                state.lastMouseY = y;
            }
            else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (scroll->wheel == sf::Mouse::Wheel::Vertical) camera.zoom(scroll->delta);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                handleKey(key->code, system, state);
            }
        }
    }

private:
    static void handleKey(sf::Keyboard::Key code, SolarSystem& system, InputState& state) {
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

            default: return;
        }

        // Without this bound the time scale diverges and the orbits advance in
        // steps large enough to lose all coherence.
        state.timeScale = std::clamp(state.timeScale, minTimeScale, maxTimeScale);
    }
};
