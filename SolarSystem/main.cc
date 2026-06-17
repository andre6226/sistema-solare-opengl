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
    sf::Window& window = *setup.window; 

    Shader shaderProgram("SolarSystem/base.vert", "SolarSystem/base.frag");
    UniformLocations locs;
    SistemaSolare sistemaPlanetario;
    Telecamera telecamera(30.0f, 0.0f, 20.0f); 

    locs.modello         = glGetUniformLocation(shaderProgram.ID, "modello");
    locs.normaleMatrice  = glGetUniformLocation(shaderProgram.ID, "normaleMatrice");
    locs.coloreOggetto   = glGetUniformLocation(shaderProgram.ID, "coloreOggetto");
    locs.isSole          = glGetUniformLocation(shaderProgram.ID, "isSole");
    locs.isAnello        = glGetUniformLocation(shaderProgram.ID, "isAnello");
    locs.isCielo         = glGetUniformLocation(shaderProgram.ID, "isCielo");
    locs.vista           = glGetUniformLocation(shaderProgram.ID, "vista");
    locs.proiezione      = glGetUniformLocation(shaderProgram.ID, "proiezione");
    locs.cameraPos = glGetUniformLocation(shaderProgram.ID, "cameraPos");
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    
  
    float mousePrecX = 0.0f, mousePrecY = 0.0f;
    float moltiplicatoreTempo = 1.0f;    
    bool running = true;
    sf::Clock orologioDiSistema;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    std::string bersaglioPrecedente = "";
    float velocitaPrecedente = -1.0f;


    while (running) {
        GestoreInput::gestisciInput(window, telecamera, sistemaPlanetario, running, mousePrecX, mousePrecY, moltiplicatoreTempo);
        

        std::string bersaglioAttuale = sistemaPlanetario.getNomeBersaglio();
        if (bersaglioAttuale != bersaglioPrecedente || moltiplicatoreTempo != velocitaPrecedente) {
            std::cout << "\n[SISTEMA SOLARE] Bersaglio: " << bersaglioAttuale 
                      << " | Velocita: " << moltiplicatoreTempo << "x" << std::endl;
            bersaglioPrecedente = bersaglioAttuale;
            velocitaPrecedente = moltiplicatoreTempo;
        }



        float deltaTempoReale = orologioDiSistema.restart().asSeconds();
        float deltaTempo = deltaTempoReale * moltiplicatoreTempo;
        sistemaPlanetario.aggiorna(deltaTempo);

        glm::vec3 posizioneBersaglio = sistemaPlanetario.getPosizioneBersaglio();
        float aspectRatio = (float)window.getSize().x / (float)window.getSize().y;
        glm::mat4 matriceVista = telecamera.getMatriceVista(posizioneBersaglio);
        glm::mat4 matriceProiezione = telecamera.getMatriceProiezione(aspectRatio);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderProgram.use();
        glUniformMatrix4fv(locs.vista, 1, GL_FALSE, &matriceVista[0][0]);
        glUniformMatrix4fv(locs.proiezione, 1, GL_FALSE, &matriceProiezione[0][0]);
        glm::vec3 camPos = telecamera.getPosizione(posizioneBersaglio);
        glUniform3fv(locs.cameraPos, 1, &camPos[0]);
        sistemaPlanetario.disegna(shaderProgram, locs);

        glUseProgram(0);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        

        window.display();
    }

    return 0;
}