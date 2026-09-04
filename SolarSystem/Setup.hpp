#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp>
#include <stdexcept>

// Owns the window and the OpenGL context. The window is a by-value member:
// there is no manual new/delete pair to keep balanced, and the class is
// non-copyable because sf::Window is.
class Setup {
public:
    sf::Window window;

    Setup()
        : window(sf::VideoMode({800, 600}), "Solar System",
                 sf::Style::Default, sf::State::Windowed, contextSettings()) {

        window.setVerticalSyncEnabled(true);

        // Without this, holding SHIFT produces a burst of KeyPressed events and
        // the time scale grows exponentially.
        window.setKeyRepeatEnabled(false);

        if (!window.setActive(true)) {
            throw std::runtime_error("could not activate the OpenGL context on the window");
        }

        // gladLoadGL returns 0 on failure. Without this check every function
        // pointer stays null and the first OpenGL call crashes the program with
        // no diagnostic at all.
        if (gladLoadGL(sf::Context::getFunction) == 0) {
            throw std::runtime_error("failed to load the OpenGL function pointers (gladLoadGL)");
        }

        glEnable(GL_DEPTH_TEST);
    }

    Setup(const Setup&)            = delete;
    Setup& operator=(const Setup&) = delete;
    Setup(Setup&&)                 = delete;
    Setup& operator=(Setup&&)      = delete;

private:
    static sf::ContextSettings contextSettings() {
        sf::ContextSettings settings;
        // 24 bits is what every desktop driver actually hands out; asking for 32
        // only produced a mismatch warning at startup.
        settings.depthBits         = 24;
        settings.stencilBits       = 8;
        settings.antiAliasingLevel = 4;
        // Nothing in this project needs OpenGL beyond 3.3, and asking for the
        // lowest version that suffices keeps it portable to older drivers.
        settings.majorVersion      = 3;
        settings.minorVersion      = 3;
        settings.attributeFlags    = sf::ContextSettings::Attribute::Core;
        return settings;
    }
};
