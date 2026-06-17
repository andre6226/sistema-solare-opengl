#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#include <optional>
#undef GLAD_GL_IMPLEMENTATION

#include "Setup.hpp"
#include "Shader.hpp"
#include "Sphere.hpp"
#include "Telecamera.hpp"


void gestisciInput(sf::Window& window, Telecamera& telecamera, bool& running, float& mousePrecX, float& mousePrecY) {
    
    while (const std::optional event = window.pollEvent()) {
        
        if (event->is<sf::Event::Closed>()) {
            running = false;
        } 
        else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            glViewport(0, 0, resized->size.x, resized->size.y);
        }

        else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            float mouseAttualeX = mouse->position.x;
            float mouseAttualeY = mouse->position.y;

            float deltaX = mouseAttualeX - mousePrecX;
            float deltaY = mouseAttualeY - mousePrecY;

            mousePrecX = mouseAttualeX;
            mousePrecY = mouseAttualeY;

            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                telecamera.ruota(deltaX, deltaY);
            } 
        }
        else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (scroll->wheel == sf::Mouse::Wheel::Vertical) {
                telecamera.zoom(scroll->delta); 
            }
        }
    }
}

int main() {
    Setup setup;
    sf::Window& window = *setup.window;

    Shader shaderProgram("Tappa03/base.vert", "Tappa03/base.frag");
    Sphere sole(0.7f, 100, 50);
    Telecamera telecamera(4.0f, 0.0f, 20.0f);

    GLint vistaLoc = glGetUniformLocation(shaderProgram.ID, "vista");
    GLint proiezioneLoc = glGetUniformLocation(shaderProgram.ID, "proiezione");
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);

    float mousePrecX = 0.0f, mousePrecY = 0.0f;
    bool running = true;

    while (running) {
        
        gestisciInput(window, telecamera, running, mousePrecX, mousePrecY);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderProgram.use();

        float aspectRatio = (float)window.getSize().x / (float)window.getSize().y;
        glm::mat4 matriceVista = telecamera.getMatriceVista();
        glm::mat4 matriceProiezione = telecamera.getMatriceProiezione(aspectRatio);

        glUniformMatrix4fv(vistaLoc, 1, GL_FALSE, &matriceVista[0][0]);
        glUniformMatrix4fv(proiezioneLoc, 1, GL_FALSE, &matriceProiezione[0][0]);

        sole.draw();
        window.display();
    }

    return 0;
}