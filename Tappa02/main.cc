#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#undef GLAD_GL_IMPLEMENTATION
#include <SFML/Window.hpp>
#include <iostream>

#include "Setup.hpp"
#include "Shader.hpp"
#include "Sphere.hpp"


int main() {
    Setup setup;
    sf::Window& window = *setup.window;

    
    Shader shaderProgram("Tappa02/base.vert", "Tappa02/base.frag");
    
    Sphere sole(0.7f, 36, 18);

    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);

    bool running = true;
    while (running) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) running = false;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shaderProgram.use();
        sole.draw();

        window.display();
    }

    return 0;
}