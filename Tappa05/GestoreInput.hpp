#include <SFML/System/Clock.hpp>
    
class GestoreInput {
    public:
    static void gestisciInput(sf::Window& window, Telecamera& telecamera, bool& running, float& mousePrecX, float& mousePrecY) {
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
};