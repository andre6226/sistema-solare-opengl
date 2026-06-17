#include <SFML/System/Clock.hpp>
#include <SFML/Window/Keyboard.hpp> 

class GestoreInput {
public:
    static void gestisciInput(sf::Window& window, Telecamera& telecamera, SistemaSolare& sistema, bool& running, float& mousePrecX, float& mousePrecY, float& moltiplicatoreTempo) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) running = false;
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
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
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Up) sistema.selezionaPadre();
                else if (key->code == sf::Keyboard::Key::Down) sistema.selezionaPrimoFiglio();
                else if (key->code == sf::Keyboard::Key::Right) sistema.selezionaProssimoFratello();
                else if (key->code == sf::Keyboard::Key::Left) sistema.selezionaFratelloPrecedente();
                
                else if (key->code == sf::Keyboard::Key::LShift || key->code == sf::Keyboard::Key::RShift) moltiplicatoreTempo *= 1.5f;
                else if (key->code == sf::Keyboard::Key::LControl || key->code == sf::Keyboard::Key::RControl) moltiplicatoreTempo /= 1.5f;
                else if (key->code == sf::Keyboard::Key::Space) moltiplicatoreTempo = 1.0f; // Reset veloce
            }
        }
    }
};