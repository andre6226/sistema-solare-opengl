#pragma once
#include "../glad/gl.h"
#include <SFML/Window.hpp>
#include <iostream>
#include <cstdlib>


class Setup {
public:
    sf::Window* window;
    Setup() {
        sf::ContextSettings settings;
        settings.depthBits = 32;
        settings.stencilBits = 8;
        settings.antiAliasingLevel = 4;
        settings.majorVersion = 4;
        settings.minorVersion = 1;
        settings.attributeFlags = sf::ContextSettings::Attribute::Core;

        window = new sf::Window(sf::VideoMode({800, 600}), "Tappa 05", sf::Style::Default, sf::State::Windowed, settings);
        window->setVerticalSyncEnabled(true);
        window->setActive(true);
        gladLoadGL(sf::Context::getFunction);
        glEnable(GL_DEPTH_TEST);
    }
    ~Setup() { delete window; }
};
