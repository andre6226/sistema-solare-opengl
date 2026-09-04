// glad is a single-file header: this, and only this, translation unit emits its
// implementation. The #undef is NOT redundant: glad's "#ifdef
// GLAD_GL_IMPLEMENTATION" block sits OUTSIDE its include guard, so leaving the
// macro defined would make every later header that includes gl.h redefine all
// of its global symbols.
#define GLAD_GL_IMPLEMENTATION
#include "../glad/gl.h"
#undef GLAD_GL_IMPLEMENTATION

#include "Setup.hpp"
#include "Shader.hpp"
#include "Camera.hpp"
#include "SolarSystem.hpp"
#include "InputHandler.hpp"

#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

// Frame deltas are truncated at this value: if the window stalls (dragging,
// suspend, a debugger breakpoint) the first frame after the stall would carry a
// huge dt and jump every orbit forward.
static constexpr float maxFrameDelta = 0.1f;

// The Sun sits at the origin of world space and is the only light source.
static const glm::vec3 lightPosition(0.0f, 0.0f, 0.0f);

int main() {
    try {
        Setup setup;
        sf::Window& window = setup.window;

        Shader shader("SolarSystem/base.vert", "SolarSystem/base.frag");
        SolarSystem solarSystem;
        Camera camera(30.0f, 0.0f, 20.0f);

        UniformLocations locs;
        locs.view         = shader.uniformLocation("view");
        locs.projection   = shader.uniformLocation("projection");
        locs.cameraPos    = shader.uniformLocation("cameraPos");
        locs.lightPos     = shader.uniformLocation("lightPos");
        locs.model        = shader.uniformLocation("model");
        locs.normalMatrix = shader.uniformLocation("normalMatrix");
        locs.bodyType     = shader.uniformLocation("bodyType");
        locs.parentCenter = shader.uniformLocation("parentCenter");
        locs.parentRadius = shader.uniformLocation("parentRadius");

        shader.use();
        // Sampler uniforms default to 0, so binding textures to GL_TEXTURE0
        // happened to work already. Stating it makes the coupling explicit.
        glUniform1i(shader.uniformLocation("bodyTexture"), 0);
        // The light never moves, so it is uploaded once rather than per frame.
        glUniform3fv(locs.lightPos, 1, &lightPosition[0]);

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        InputState input;
        bool running = true;
        sf::Clock clock;

        std::string previousTarget = "";
        float previousTimeScale = -1.0f;

        while (running) {
            InputHandler::handleEvents(window, camera, solarSystem, running, input);

            const std::string currentTarget = solarSystem.getTargetName();

            if (currentTarget != previousTarget) {
                // The zoom bounds depend on the size of the new target.
                camera.focusOn(solarSystem.getTargetRadius());
            }

            if (currentTarget != previousTarget || input.timeScale != previousTimeScale) {
                std::cout << "\n[SOLAR SYSTEM] Target: " << currentTarget
                          << " | Speed: " << input.timeScale << "x" << std::endl;
                previousTarget = currentTarget;
                previousTimeScale = input.timeScale;
            }

            const float realDelta = std::min(clock.restart().asSeconds(), maxFrameDelta);
            solarSystem.update(realDelta * input.timeScale);

            // A minimised window can report a size of zero: the aspect ratio
            // would become NaN and poison the whole projection matrix.
            const sf::Vector2u size = window.getSize();
            if (size.x == 0 || size.y == 0) {
                sf::sleep(sf::milliseconds(16));
                continue;
            }

            const float aspectRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
            const glm::vec3 targetPosition = solarSystem.getTargetPosition();
            const glm::mat4 viewMatrix = camera.getViewMatrix(targetPosition);
            const glm::mat4 projectionMatrix = camera.getProjectionMatrix(aspectRatio);
            const glm::vec3 cameraPosition = camera.getPosition(targetPosition);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            glUniformMatrix4fv(locs.view, 1, GL_FALSE, &viewMatrix[0][0]);
            glUniformMatrix4fv(locs.projection, 1, GL_FALSE, &projectionMatrix[0][0]);
            glUniform3fv(locs.cameraPos, 1, &cameraPosition[0]);

            solarSystem.draw(locs);

            glUseProgram(0);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            window.display();
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    }
}
