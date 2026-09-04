// glad e' un header single-file: qui, e solo qui, ne viene emessa anche
// l'implementazione. L'#undef NON e' superfluo: il blocco
// "#ifdef GLAD_GL_IMPLEMENTATION" sta FUORI dall'include guard di glad, quindi
// senza toglierlo ogni header successivo che includa gl.h ne ridefinirebbe
// tutti i simboli globali.
#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#undef GLAD_GL_IMPLEMENTATION

#include "Setup.hpp"
#include "Shader.hpp"
#include "Telecamera.hpp"
#include "SistemaSolare.hpp"
#include "GestoreInput.hpp"

#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

// Oltre questa soglia il delta viene troncato: se la finestra resta bloccata
// (trascinamento, sospensione, breakpoint del debugger) il primo frame dopo lo
// sblocco avrebbe un dt enorme e farebbe saltare tutte le orbite in avanti.
static constexpr float deltaTempoMassimo = 0.1f;

int main() {
    try {
        Setup setup;
        sf::Window& window = setup.window;

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
        locs.cameraPos       = glGetUniformLocation(shaderProgram.ID, "cameraPos");
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);


        StatoInterazione stato;
        bool running = true;
        sf::Clock orologioDiSistema;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        std::string bersaglioPrecedente = "";
        float velocitaPrecedente = -1.0f;


        while (running) {
            GestoreInput::gestisciInput(window, telecamera, sistemaPlanetario, running, stato);


            const std::string bersaglioAttuale = sistemaPlanetario.getNomeBersaglio();

            if (bersaglioAttuale != bersaglioPrecedente) {
                // I limiti di zoom dipendono dalle dimensioni del nuovo bersaglio.
                telecamera.adattaAlBersaglio(sistemaPlanetario.getRaggioBersaglio());
            }

            if (bersaglioAttuale != bersaglioPrecedente || stato.moltiplicatoreTempo != velocitaPrecedente) {
                std::cout << "\n[SISTEMA SOLARE] Bersaglio: " << bersaglioAttuale
                          << " | Velocita: " << stato.moltiplicatoreTempo << "x" << std::endl;
                bersaglioPrecedente = bersaglioAttuale;
                velocitaPrecedente = stato.moltiplicatoreTempo;
            }



            const float deltaTempoReale = std::min(orologioDiSistema.restart().asSeconds(), deltaTempoMassimo);
            const float deltaTempo = deltaTempoReale * stato.moltiplicatoreTempo;
            sistemaPlanetario.aggiorna(deltaTempo);

            // Con la finestra ridotta a icona la dimensione puo' essere nulla:
            // l'aspect ratio diventerebbe NaN e propagherebbe NaN in tutta la
            // matrice di proiezione.
            const sf::Vector2u dimensioni = window.getSize();
            if (dimensioni.x == 0 || dimensioni.y == 0) {
                sf::sleep(sf::milliseconds(16));
                continue;
            }

            glm::vec3 posizioneBersaglio = sistemaPlanetario.getPosizioneBersaglio();
            float aspectRatio = static_cast<float>(dimensioni.x) / static_cast<float>(dimensioni.y);
            glm::mat4 matriceVista = telecamera.getMatriceVista(posizioneBersaglio);
            glm::mat4 matriceProiezione = telecamera.getMatriceProiezione(aspectRatio);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            shaderProgram.use();
            glUniformMatrix4fv(locs.vista, 1, GL_FALSE, &matriceVista[0][0]);
            glUniformMatrix4fv(locs.proiezione, 1, GL_FALSE, &matriceProiezione[0][0]);
            glm::vec3 camPos = telecamera.getPosizione(posizioneBersaglio);
            glUniform3fv(locs.cameraPos, 1, &camPos[0]);
            sistemaPlanetario.disegna(locs);

            glUseProgram(0);

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


            window.display();
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[ERRORE FATALE] " << e.what() << std::endl;
        return 1;
    }
}
