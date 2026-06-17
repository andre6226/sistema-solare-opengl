#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#include <optional>
#undef GLAD_GL_IMPLEMENTATION

#include "Setup.hpp"
#include "Shader.hpp"
#include "Telecamera.hpp"
#include "SistemaSolare.hpp"
#include <SFML/System/Clock.hpp>

void gestisciInput(sf::Window& window, Telecamera& telecamera, bool& running, float& mousePrecX, float& mousePrecY) {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) running = false;
        else if (const auto* resized = event->getIf<sf::Event::Resized>()) glViewport(0, 0, resized->size.x, resized->size.y);
        else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            float deltaX = mouse->position.x - mousePrecX;
            float deltaY = mouse->position.y - mousePrecY;
            mousePrecX = mouse->position.x;
            mousePrecY = mouse->position.y;
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                telecamera.ruota(deltaX, deltaY);
            } 
        }
        else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (scroll->wheel == sf::Mouse::Wheel::Vertical) telecamera.zoom(scroll->delta); 
        }
    }
}

int main() {
    Setup setup;
    sf::Window& window = *setup.window;

    Shader shaderProgram("Tappa04/base.vert", "Tappa04/base.frag");
    
    SistemaSolare sistemaPlanetario;
    
    Telecamera telecamera(30.0f, 0.0f, 20.0f);

    GLint vistaLoc = glGetUniformLocation(shaderProgram.ID, "vista");
    GLint proiezioneLoc = glGetUniformLocation(shaderProgram.ID, "proiezione");
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);

    float mousePrecX = 0.0f, mousePrecY = 0.0f;
    bool running = true;
    sf::Clock orologioDiSistema;

    while (running) {
        gestisciInput(window, telecamera, running, mousePrecX, mousePrecY);
        float deltaTempo = orologioDiSistema.restart().asSeconds();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderProgram.use();

        float aspectRatio = (float)window.getSize().x / (float)window.getSize().y;
        glm::mat4 matriceVista = telecamera.getMatriceVista();
        glm::mat4 matriceProiezione = telecamera.getMatriceProiezione(aspectRatio);

        glUniformMatrix4fv(vistaLoc, 1, GL_FALSE, &matriceVista[0][0]);
        glUniformMatrix4fv(proiezioneLoc, 1, GL_FALSE, &matriceProiezione[0][0]);

        sistemaPlanetario.aggiorna(deltaTempo);
        sistemaPlanetario.disegna(shaderProgram);

        window.display();
    }

    return 0;
}