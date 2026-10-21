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
#include "Travel.hpp"

#include <cstdio>
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
              << "       [--azimuth DEG] [--elevation DEG] [--size WxH] [--orbits on|off]\n"
              << "       [--screenshot OUT.png]\n"
              << "With --focus, the camera looks from the Sun's side of the body; azimuth and\n"
              << "elevation turn it from there. --screenshot freezes time, saves, and exits.\n";
    return 2;
}

int main(int argc, char** argv) {
    std::string dateArg, focusArg, screenshotPath;
    float distanceKm = 0.0f, azimuthOffset = 0.0f, elevationOffset = 0.0f;
    sf::Vector2u windowSize{800, 600};
    bool orbitLines = true;
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
            else if (opt == "--orbits" && (std::string(value) == "on" || std::string(value) == "off")) orbitLines = std::string(value) == "on";
            else if (opt == "--size") {
                unsigned w = 0, h = 0;
                if (std::sscanf(value, "%ux%u", &w, &h) != 2 || w < 64 || h < 64 || w > 8192 || h > 8192) return usage(argv[0]);
                windowSize = {w, h};
            }
            else return usage(argv[0]);
        } catch (const std::exception&) {
            return usage(argv[0]);
        }
    }

    try {
        Setup setup(windowSize);
        sf::Window& window = setup.window;

        Shader shader("SolarSystem/base.vert", "SolarSystem/base.frag");
        SolarSystem solarSystem;
        if (!orbitLines) solarSystem.toggleOrbitLines();
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
        locs.parentPolarRadius = shader.uniformLocation("parentPolarRadius");
        locs.ringCenter      = shader.uniformLocation("ringCenter");
        locs.ringNormal      = shader.uniformLocation("ringNormal");
        locs.ringOuterRadius = shader.uniformLocation("ringOuterRadius");
        locs.ringInner       = shader.uniformLocation("ringInner");
        locs.ringDustBelow   = shader.uniformLocation("ringDustBelow");
        locs.sunRadius          = shader.uniformLocation("sunRadius");
        locs.shadowCasterCount   = shader.uniformLocation("shadowCasterCount");
        locs.shadowCasterPos     = shader.uniformLocation("shadowCasterPos");
        locs.shadowCasterRadius  = shader.uniformLocation("shadowCasterRadius");
        locs.shadowCasterSoft    = shader.uniformLocation("shadowCasterSoft");
        locs.casterHasAtmosphere = shader.uniformLocation("casterHasAtmosphere");
        locs.airless             = shader.uniformLocation("airless");
        locs.cometTail           = shader.uniformLocation("cometTail");
        locs.hasClouds          = shader.uniformLocation("hasClouds");
        locs.hasNight           = shader.uniformLocation("hasNight");
        locs.bodyPole           = shader.uniformLocation("bodyPole");
        locs.atmoColor          = shader.uniformLocation("atmoColor");

        shader.use();
        // Sampler uniforms default to 0, so binding textures to GL_TEXTURE0
        // happened to work already. Stating it makes the coupling explicit.
        glUniform1i(shader.uniformLocation("bodyTexture"), 0);
        glUniform1i(shader.uniformLocation("ringTexture"), 1);
        glUniform1i(shader.uniformLocation("cloudTexture"), 2);
        glUniform1i(shader.uniformLocation("nightTexture"), 3);
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
            // From the Sun itself there is no Sun direction: keep the default one.
            const glm::vec3 toSun = solarSystem.getSunPosition() - solarSystem.getTargetPosition();
            camera.setDirection(glm::length(toSun) > 0.0f ? toSun : glm::vec3(0.0f, 0.0f, 1.0f),
                                azimuthOffset, elevationOffset);
        }
        camera.focusOn(solarSystem.getTargetRadius());
        previousTarget = solarSystem.getTargetName();   // the loop must not re-focus over --distance
        if (distanceKm > 0.0f) camera.setDistance(distanceKm);
        const bool screenshotMode = !screenshotPath.empty();
        int framesRendered = 0;
        if (screenshotMode) input.timeScale = 0.0f;   // frozen until a SHIFT/CTRL press
        float smoothedFps = 60.0f;

        // Clicking a body's dot or label flies there; the click is hit-tested against
        // the markers of the last frame, where the user saw them.
        Travel travel;
        std::vector<BodyMarker> lastMarkers;
        glm::vec3 lastCameraPosition(0.0f), lastForward(0.0f, 0.0f, -1.0f);

        while (running) {
            InputHandler::handleEvents(window, camera, solarSystem, running, input);

            if (input.clickPending) {
                input.clickPending = false;
                const CelestialBody* destination = solarSystem.findBody(hud.markerAt(lastMarkers, input.clickX, input.clickY));
                if (destination != nullptr && destination != solarSystem.getTarget()) {
                    travel.start(solarSystem.getTarget(), destination, lastCameraPosition, lastForward,
                                 solarSystem.getSunPosition());
                }
            }
            if (!input.flyRequest.empty()) {
                const CelestialBody* destination = solarSystem.findBody(input.flyRequest);
                input.flyRequest.clear();
                if (destination != nullptr && destination != solarSystem.getTarget()) {
                    travel.start(solarSystem.getTarget(), destination, lastCameraPosition, lastForward,
                                 solarSystem.getSunPosition());
                }
            }
            // A target picked from the keyboard mid-flight ends the flight.
            if (travel.active() && solarSystem.getTarget() != travel.origin() &&
                solarSystem.getTarget() != travel.destination()) {
                travel.stop();
            }

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
            glm::mat4 viewMatrix = camera.getViewMatrix(targetPosition);
            glm::mat4 projectionMatrix = camera.getProjectionMatrix(aspectRatio);
            glm::vec3 cameraPosition = camera.getPosition(targetPosition);

            if (travel.active()) {
                Travel::Pose pose = travel.step(realDelta);
                if (travel.pastHalfway() && solarSystem.getTarget() != travel.destination()) {
                    solarSystem.selectByName(travel.destination()->getName());   // recentres the world
                    pose = travel.pose();
                }
                const glm::vec3 up = std::abs(pose.forward.y) > 0.999f ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                                        : glm::vec3(0.0f, 1.0f, 0.0f);
                cameraPosition = pose.position;
                // Rotation from the direction alone, then the translation: lookAt(eye, eye + forward)
                // collapses to eye == centre in float once eye is ~1e7 km out, and returns NaN.
                viewMatrix = glm::lookAt(glm::vec3(0.0f), pose.forward, up) * glm::translate(glm::mat4(1.0f), -pose.position);
                projectionMatrix = glm::perspective(glm::radians(pose.fov), aspectRatio,
                                                    camera.getNearPlane(), camera.getFarPlane());
                if (travel.arrived()) {
                    // Hand over to the orbit camera exactly where the flight ends.
                    camera.focusOn(travel.destination()->getScale());
                    camera.setDirection(travel.arrivalDirection());
                    previousTarget = solarSystem.getTargetName();
                    travel.stop();
                }
            }
            lastCameraPosition = cameraPosition;
            lastForward = -glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);

            // Known only once the camera has been placed, hence after update().
            solarSystem.updateSkyPosition(cameraPosition);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();
            glUniformMatrix4fv(locs.view, 1, GL_FALSE, &viewMatrix[0][0]);
            glUniformMatrix4fv(locs.projection, 1, GL_FALSE, &projectionMatrix[0][0]);
            glUniform3fv(locs.cameraPos, 1, &cameraPosition[0]);

            solarSystem.draw(locs, cameraPosition);
            solarSystem.drawBelt(viewMatrix, projectionMatrix, cameraPosition);
            solarSystem.drawOrbits(viewMatrix, projectionMatrix);

            HudFrame hudFrame;
            hudFrame.targetName     = currentTarget;
            hudFrame.targetPath     = solarSystem.getTargetPath();
            hudFrame.data           = solarSystem.getTargetData();
            hudFrame.orbit          = solarSystem.getTargetOrbit();
            hudFrame.satelliteCount = solarSystem.getTargetSatelliteCount();
            hudFrame.timeScale      = input.timeScale;
            hudFrame.cameraDistance = glm::length(cameraPosition - solarSystem.getTargetPosition());   // true mid-flight too
            hudFrame.fps            = smoothedFps;
            hudFrame.utcTime        = Astro::formatUtcDateTime(solarSystem.getSimUnixSeconds());
            hudFrame.timePickerOpen = input.timePickerOpen;
            hudFrame.searchOpen     = input.searchOpen;
            hudFrame.searchText     = input.searchText;
            const CelestialBody* match = solarSystem.findByPrefix(input.searchText);
            hudFrame.searchMatch    = match ? match->getName() : "";
            hudFrame.pickerField    = input.pickerField;
            hudFrame.pickerYear     = input.pickerYear;
            hudFrame.pickerMonth    = input.pickerMonth;
            hudFrame.pickerDay      = input.pickerDay;
            hudFrame.pickerHour     = input.pickerHour;
            hudFrame.pickerMin      = input.pickerMin;
            hudFrame.pickerSec      = input.pickerSec;
            solarSystem.collectMarkers(projectionMatrix * viewMatrix, cameraPosition,
                                       size.x, size.y, hudFrame.markers);
            lastMarkers = hudFrame.markers;
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
