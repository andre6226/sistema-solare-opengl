#pragma once
// Questo header includeva soltanto <SFML/System/Clock.hpp> e compilava per puro
// ordine fortunato degli include in main.cc. Ora dichiara le proprie dipendenze
// ed e' includibile da solo.
#include "../glad/gl.h"
#include <SFML/Window.hpp>
#include <algorithm>
#include <optional>
#include "Telecamera.hpp"
#include "SistemaSolare.hpp"

// Stato dell'interazione utente che deve sopravvivere tra un frame e l'altro.
struct StatoInterazione {
    float mousePrecX = 0.0f;
    float mousePrecY = 0.0f;
    bool  trascinamento = false;
    float moltiplicatoreTempo = 1.0f;
};

class GestoreInput {
private:
    static constexpr float moltiplicatoreMin = 0.001f;
    static constexpr float moltiplicatoreMax = 1000.0f;

public:
    static void gestisciInput(sf::Window& window, Telecamera& telecamera, SistemaSolare& sistema,
                              bool& running, StatoInterazione& stato) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                running = false;
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            // Il trascinamento comincia con la pressione DENTRO la finestra.
            // Prima si leggeva sf::Mouse::isButtonPressed, che interroga lo stato
            // globale dell'OS: bastava premere il tasto sopra un'altra
            // applicazione, o uscire e rientrare dalla finestra tenendolo
            // premuto, per far saltare la telecamera.
            else if (const auto* premuto = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (premuto->button == sf::Mouse::Button::Left) {
                    stato.trascinamento = true;
                    stato.mousePrecX = static_cast<float>(premuto->position.x);
                    stato.mousePrecY = static_cast<float>(premuto->position.y);
                }
            }
            else if (const auto* rilasciato = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (rilasciato->button == sf::Mouse::Button::Left) {
                    stato.trascinamento = false;
                }
            }
            else if (event->is<sf::Event::FocusLost>() || event->is<sf::Event::MouseLeft>()) {
                stato.trascinamento = false;
            }
            else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
                const float x = static_cast<float>(mouse->position.x);
                const float y = static_cast<float>(mouse->position.y);

                if (stato.trascinamento) {
                    telecamera.ruota(x - stato.mousePrecX, y - stato.mousePrecY);
                }

                stato.mousePrecX = x;
                stato.mousePrecY = y;
            }
            else if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (scroll->wheel == sf::Mouse::Wheel::Vertical) telecamera.zoom(scroll->delta);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                gestisciTasto(key->code, sistema, stato);
            }
        }
    }

private:
    static void gestisciTasto(sf::Keyboard::Key codice, SistemaSolare& sistema, StatoInterazione& stato) {
        switch (codice) {
            case sf::Keyboard::Key::Up:    sistema.selezionaPadre();             break;
            case sf::Keyboard::Key::Down:  sistema.selezionaPrimoFiglio();       break;
            case sf::Keyboard::Key::Right: sistema.selezionaProssimoFratello();  break;
            case sf::Keyboard::Key::Left:  sistema.selezionaFratelloPrecedente(); break;

            case sf::Keyboard::Key::LShift:
            case sf::Keyboard::Key::RShift:   stato.moltiplicatoreTempo *= 1.5f; break;
            case sf::Keyboard::Key::LControl:
            case sf::Keyboard::Key::RControl: stato.moltiplicatoreTempo /= 1.5f; break;
            case sf::Keyboard::Key::Space:    stato.moltiplicatoreTempo  = 1.0f; break;

            default: return;
        }

        // Senza questo limite il moltiplicatore diverge e le orbite avanzano a
        // salti cosi' grandi da perdere ogni coerenza.
        stato.moltiplicatoreTempo = std::clamp(stato.moltiplicatoreTempo,
                                               moltiplicatoreMin, moltiplicatoreMax);
    }
};
