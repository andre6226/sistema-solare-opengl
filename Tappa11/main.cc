#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#include <optional>
#undef GLAD_GL_IMPLEMENTATION

#include "Setup.hpp"
#include "Shader.hpp"
#include "Telecamera.hpp"
#include "SistemaSolare.hpp"
#include "GestoreInput.hpp"
#include <SFML/System/Clock.hpp>
#include <SFML/Graphics.hpp>

int main() {
    Setup setup;
    sf::RenderWindow& window = *setup.window; 

    Shader shaderProgram("Tappa11/base.vert", "Tappa11/base.frag");
    UniformLocations locs;
    SistemaSolare sistemaPlanetario;
    Telecamera telecamera(30.0f, 0.0f, 20.0f); 

    locs.modello         = glGetUniformLocation(shaderProgram.ID, "modello");
    locs.normaleMatrice  = glGetUniformLocation(shaderProgram.ID, "normaleMatrice");
    locs.coloreOggetto   = glGetUniformLocation(shaderProgram.ID, "coloreOggetto");
    locs.isSole          = glGetUniformLocation(shaderProgram.ID, "isSole");
    locs.isAnello        = glGetUniformLocation(shaderProgram.ID, "isAnello");
    locs.vista           = glGetUniformLocation(shaderProgram.ID, "vista");
    locs.proiezione      = glGetUniformLocation(shaderProgram.ID, "proiezione");
    locs.cameraPos = glGetUniformLocation(shaderProgram.ID, "cameraPos");
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    
    sf::Font font;
    
    
    if (!font.openFromFile("Risorse/font.ttf")) {
        std::cerr << "ERRORE: Impossibile caricare Risorse/font.ttf!" << std::endl;
    }
    
    
    sf::Text testoHUD(font);
    testoHUD.setCharacterSize(24);
    testoHUD.setFillColor(sf::Color(255, 255, 255)); 
    testoHUD.setPosition(sf::Vector2f(15.0f, 15.0f)); 

    float mousePrecX = 0.0f, mousePrecY = 0.0f;
    bool running = true;
    sf::Clock orologioDiSistema;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (running) {
        GestoreInput::gestisciInput(window, telecamera, sistemaPlanetario, running, mousePrecX, mousePrecY);
        float deltaTempo = orologioDiSistema.restart().asSeconds();

        sistemaPlanetario.aggiorna(deltaTempo);

        glm::vec3 posizioneBersaglio = sistemaPlanetario.getPosizioneBersaglio();
        float aspectRatio = (float)window.getSize().x / (float)window.getSize().y;
        glm::mat4 matriceVista = telecamera.getMatriceVista(posizioneBersaglio);
        glm::mat4 matriceProiezione = telecamera.getMatriceProiezione(aspectRatio);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderProgram.use();
        glUniformMatrix4fv(locs.vista, 1, GL_FALSE, &matriceVista[0][0]);
        glUniformMatrix4fv(locs.proiezione, 1, GL_FALSE, &matriceProiezione[0][0]);
        glm::vec3 camPos = telecamera.getPosizione();
        glUniform3fv(locs.cameraPos, 1, &camPos[0]);
        sistemaPlanetario.disegna(shaderProgram, locs);

        glUseProgram(0);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
        window.pushGLStates(); 
        
        testoHUD.setString("Target: " + sistemaPlanetario.getNomeBersaglio());
        window.draw(testoHUD);
        
        window.popGLStates();

        window.display();
    }

    return 0;
}