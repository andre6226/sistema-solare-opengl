#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp>
#include <stdexcept>

// Possiede la finestra e il contesto OpenGL. La finestra e' un membro per
// valore: non c'e' piu' una new/delete manuale da bilanciare, e la classe non
// e' copiabile perche' sf::Window non lo e'.
class Setup {
public:
    sf::Window window;

    Setup()
        : window(sf::VideoMode({800, 600}), "Sistema Solare",
                 sf::Style::Default, sf::State::Windowed, impostazioniContesto()) {

        window.setVerticalSyncEnabled(true);

        // Senza questo, tenere premuto SHIFT genera una raffica di KeyPressed e
        // il moltiplicatore di tempo cresce in modo esponenziale.
        window.setKeyRepeatEnabled(false);

        if (!window.setActive(true)) {
            throw std::runtime_error("impossibile attivare il contesto OpenGL sulla finestra");
        }

        // gladLoadGL restituisce 0 in caso di fallimento: senza questo controllo
        // tutti i puntatori a funzione restano nulli e il primo comando OpenGL
        // fa crashare il programma senza alcun messaggio.
        if (gladLoadGL(sf::Context::getFunction) == 0) {
            throw std::runtime_error("caricamento delle funzioni OpenGL fallito (gladLoadGL). "
                                     "Il driver espone un contesto 4.1 core?");
        }

        glEnable(GL_DEPTH_TEST);
    }

    Setup(const Setup&)            = delete;
    Setup& operator=(const Setup&) = delete;
    Setup(Setup&&)                 = delete;
    Setup& operator=(Setup&&)      = delete;

private:
    static sf::ContextSettings impostazioniContesto() {
        sf::ContextSettings settings;
        settings.depthBits         = 32;
        settings.stencilBits       = 8;
        settings.antiAliasingLevel = 4;
        settings.majorVersion      = 4;
        settings.minorVersion      = 1;
        settings.attributeFlags    = sf::ContextSettings::Attribute::Core;
        return settings;
    }
};
