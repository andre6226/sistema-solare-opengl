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
#include "Hud.hpp"

#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Frame deltas are truncated at this value: if the window stalls (dragging,
// suspend, a debugger breakpoint) the first frame after the stall would carry a
// huge dt and jump every orbit forward.
static constexpr float maxFrameDelta = 0.1f;

// The Sun sits at the origin of world space and is the only light source.
static const glm::vec3 lightPosition(0.0f, 0.0f, 0.0f);

// Frames rendered before a --screenshot is read back, so every per-frame
// quantity (sky sphere, markers, FPS smoothing) has settled.
static constexpr int screenshotFrames = 3;

static int usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " [--date YYYY-MM-DDTHH:MM:SS] [--focus BODY] [--distance KM]\n"
              << "       [--azimuth DEG] [--elevation DEG] [--screenshot OUT.png]\n"
              << "With --focus, the camera looks from the Sun's side of the body; azimuth and\n"
              << "elevation turn it from there. --screenshot freezes time, saves, and exits.\n";
    return 2;
}

int main(int argc, char** argv) {
    std::string dateArg, focusArg, screenshotPath;
    float distanceKm = 0.0f, azimuthOffset = 0.0f, elevationOffset = 0.0f;
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (i + 1 >= argc) return usage(argv[0]);
        const char* value = argv[++i];
        try {
            if      (opt == "--date")       dateArg = value;
            else if (opt == "--focus")      focusArg = value;
            else if (opt == "--screenshot") screenshotPath = value;
            else if (opt == "--distance")   distanceKm = std::stof(value);
            else if (opt == "--azimuth")    azimuthOffset = std::stof(value);
            else if (opt == "--elevation")  elevationOffset = std::stof(value);
            else return usage(argv[0]);
        } catch (const std::exception&) {
            return usage(argv[0]);
        }
    }

    try {
        Setup setup;
        sf::Window& window = setup.window;

        Shader shader("SolarSystem/base.vert", "SolarSystem/base.frag");
        SolarSystem solarSystem;
        std::cout << "[time] Initialized UTC: " << Astro::formatUtcDateTime(solarSystem.getSimUnixSeconds())
                  << " (JD " << std::fixed << std::setprecision(2) << solarSystem.getJulianDate()
                  << ", T = " << std::setprecision(6) << solarSystem.getJulianCenturies() << " cy)" << std::endl;
        Camera camera(30.0f, 0.0f, 20.0f);
        Hud hud;

        if (!dateArg.empty()) {
            int y, mo, d, h, mi, sec;
            if (std::sscanf(dateArg.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6) {
                std::cerr << "bad --date '" << dateArg << "', expected YYYY-MM-DDTHH:MM:SS (UTC)" << std::endl;
                return 2;
            }
            // The same path as the time picker's Apply.
            Astro::clampPickerValues(y, mo, d, h, mi, sec);
            solarSystem.setSimUnixSeconds(Astro::utcDateTimeToUnix(y, mo, d, h, mi, sec));
            std::cout << "[time] Set UTC: " << Astro::formatUtcDateTime(solarSystem.getSimUnixSeconds()) << std::endl;
        }

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
        locs.ringCenter      = shader.uniformLocation("ringCenter");
        locs.ringNormal      = shader.uniformLocation("ringNormal");
        locs.ringOuterRadius = shader.uniformLocation("ringOuterRadius");
        locs.sunRadius          = shader.uniformLocation("sunRadius");
        locs.shadowCasterCount   = shader.uniformLocation("shadowCasterCount");
        locs.shadowCasterPos     = shader.uniformLocation("shadowCasterPos");
        locs.shadowCasterRadius  = shader.uniformLocation("shadowCasterRadius");
        locs.casterHasAtmosphere = shader.uniformLocation("casterHasAtmosphere");

        shader.use();
        // Sampler uniforms default to 0, so binding textures to GL_TEXTURE0
        // happened to work already. Stating it makes the coupling explicit.
        glUniform1i(shader.uniformLocation("bodyTexture"), 0);
        glUniform1i(shader.uniformLocation("ringTexture"), 1);
        // The light never moves, so it is uploaded once rather than per frame.
        glUniform3fv(locs.lightPos, 1, &lightPosition[0]);

        // Linear values. With GL_FRAMEBUFFER_SRGB on, the clear colour is
        // encoded to sRGB on write too, so these are the linear equivalents of
        // the (0.02, 0.02, 0.05) that used to be written directly.
        glClearColor(0.0015f, 0.0015f, 0.0039f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        InputState input;
        bool running = true;
        sf::Clock clock;

        std::string previousTarget = "";
        if (!focusArg.empty()) {
            if (!solarSystem.selectByName(focusArg)) {
                std::cerr << "unknown body for --focus: '" << focusArg << "'" << std::endl;
                return 2;
            }
            camera.setDirection(solarSystem.getSunPosition() - solarSystem.getTargetPosition(),
                                azimuthOffset, elevationOffset);
        }
        camera.focusOn(solarSystem.getTargetRadius());
        previousTarget = solarSystem.getTargetName();   // the loop must not re-focus over --distance
        if (distanceKm > 0.0f) camera.setDistance(distanceKm);
        const bool screenshotMode = !screenshotPath.empty();
        int framesRendered = 0;
        if (screenshotMode) input.timeScale = 0.0f;   // frozen until a SHIFT/CTRL press
        float smoothedFps = 60.0f;

        while (running) {
            InputHandler::handleEvents(window, camera, solarSystem, running, input);

            const std::string currentTarget = solarSystem.getTargetName();

            if (currentTarget != previousTarget) {
                // The zoom bounds depend on the size of the new target.
                camera.focusOn(solarSystem.getTargetRadius());
            }

            previousTarget = currentTarget;

            const float realDelta = std::min(clock.restart().asSeconds(), maxFrameDelta);

            // Exponential moving average, otherwise the readout is unreadable.
            if (realDelta > 0.0f) {
                smoothedFps += (1.0f / realDelta - smoothedFps) * 0.1f;
            }

            if (!input.timePickerOpen) {
                solarSystem.update(realDelta * input.timeScale);
            } else {
                solarSystem.update(0.0f);
            }

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

            // Known only once the camera has been placed, hence after update().
            solarSystem.updateSkyPosition(cameraPosition);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            glUniformMatrix4fv(locs.view, 1, GL_FALSE, &viewMatrix[0][0]);
            glUniformMatrix4fv(locs.projection, 1, GL_FALSE, &projectionMatrix[0][0]);
            glUniform3fv(locs.cameraPos, 1, &cameraPosition[0]);

            solarSystem.draw(locs, cameraPosition);
            solarSystem.drawOrbits(viewMatrix, projectionMatrix);

            HudFrame hudFrame;
            hudFrame.targetName     = currentTarget;
            hudFrame.targetPath     = solarSystem.getTargetPath();
            hudFrame.data           = solarSystem.getTargetData();
            hudFrame.satelliteCount = solarSystem.getTargetSatelliteCount();
            hudFrame.timeScale      = input.timeScale;
            hudFrame.cameraDistance = camera.getDistance();
            hudFrame.fps            = smoothedFps;
            hudFrame.utcTime        = Astro::formatUtcDateTime(solarSystem.getSimUnixSeconds());
            hudFrame.timePickerOpen = input.timePickerOpen;
            hudFrame.pickerField    = input.pickerField;
            hudFrame.pickerYear     = input.pickerYear;
            hudFrame.pickerMonth    = input.pickerMonth;
            hudFrame.pickerDay      = input.pickerDay;
            hudFrame.pickerHour     = input.pickerHour;
            hudFrame.pickerMin      = input.pickerMin;
            hudFrame.pickerSec      = input.pickerSec;
            solarSystem.collectMarkers(projectionMatrix * viewMatrix, cameraPosition,
                                       size.x, size.y, hudFrame.markers);
            hud.draw(hudFrame, size.x, size.y);

            glUseProgram(0);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            // Read the back buffer before display(), which leaves it undefined.
            if (screenshotMode && ++framesRendered == screenshotFrames) {
                std::vector<std::uint8_t> pixels(size_t(size.x) * size.y * 4);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, GLsizei(size.x), GLsizei(size.y), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                if (glGetError() != GL_NO_ERROR) throw std::runtime_error("glReadPixels failed");
                sf::Image image(size, pixels.data());
                image.flipVertically();   // OpenGL rows run bottom-up
                if (!image.saveToFile(screenshotPath)) throw std::runtime_error("could not save " + screenshotPath);
                std::cout << "[screenshot] " << screenshotPath << std::endl;
                running = false;
            }

            window.display();
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] " << e.what() << std::endl;
        return 1;
    }
}
