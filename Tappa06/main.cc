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

    Shader shaderProgram("Tappa06/base.vert", "Tappa06/base.frag");
    
    SistemaSolare sistemaPlanetario;
    Telecamera telecamera(30.0f, 0.0f, 20.0f); 

    GLint vistaLoc = glGetUniformLocation(shaderProgram.ID, "vista");
    GLint proiezioneLoc = glGetUniformLocation(shaderProgram.ID, "proiezione");
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    
    float mousePrecX = 0.0f, mousePrecY = 0.0f;
    bool running = true;
    sf::Clock orologioDiSistema;

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
        glUniformMatrix4fv(vistaLoc, 1, GL_FALSE, &matriceVista[0][0]);
        glUniformMatrix4fv(proiezioneLoc, 1, GL_FALSE, &matriceProiezione[0][0]);
        
        sistemaPlanetario.disegna(shaderProgram);


        glUseProgram(0);
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        

        window.display();
    }

    return 0;
}