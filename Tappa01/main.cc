#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h" 
#include <SFML/Window.hpp>
#include <iostream>

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

        window = new sf::Window(
            sf::VideoMode({800, 600}), 
            "Tappa 01 - Setup Sistema Solare", 
            sf::Style::Default, 
            sf::State::Windowed, 
            settings
        );
        window->setVerticalSyncEnabled(true);

        if (!window->setActive(true)) {
            std::cerr << "Errore SFML OpenGL." << std::endl;
            exit(1);
        }

        if (!gladLoadGL(sf::Context::getFunction)) {
            std::cerr << "Errore GLAD." << std::endl;
            exit(1);
        }

        glEnable(GL_DEPTH_TEST);
    }
    ~Setup() { delete window; }
};

int main() {
    Setup setup;
    sf::Window& window = *setup.window;

    // colore di sfondo blu notte
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);

    bool running = true;
    while (running) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) running = false;
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
        window.display();
    }
    return 0;
}
