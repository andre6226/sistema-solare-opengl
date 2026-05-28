#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp> 
#include <iostream>
#include <cstdlib>

class Setup {
public:
    sf::RenderWindow* window; 
    Setup() {
        sf::ContextSettings settings;
        settings.depthBits = 32;
        settings.stencilBits = 8;
        settings.antiAliasingLevel = 4;
        settings.majorVersion = 4;
        settings.minorVersion = 1;
        settings.attributeFlags = sf::ContextSettings::Attribute::Default;     
             
        window = new sf::RenderWindow(sf::VideoMode({800, 600}), "Sistema Solare", sf::Style::Default, sf::State::Windowed, settings);
        
        window->setVerticalSyncEnabled(true);
        if (!window->setActive(true)) {
            std::cerr << "Failed to activate OpenGL context" << std::endl;
        }        
        gladLoadGL(sf::Context::getFunction);
        glEnable(GL_DEPTH_TEST);
    }
    ~Setup() { delete window; }
};