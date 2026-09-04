#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// Orbital camera: it always looks at a target point from a position given in
// spherical coordinates (distance, azimuth, elevation) around it.
class Camera {
private:
    float distance;
    float azimuth;    // degrees, rotation around the vertical axis
    float elevation;  // degrees, clamped to avoid the lookAt degeneracy at the poles

    // Zoom bounds. The lower one is not a constant but depends on the body in
    // view, otherwise the camera ends up inside the Sun (radius 3.3) or inside
    // Saturn's rings (radius 2.0).
    float minDistance = 1.0f;
    float maxDistance = 800.0f;

    static constexpr float sensitivity = 0.4f;
    // Multiplicative zoom: one wheel notch is always 10% of the current
    // distance, which keeps it usable both at 2 and at 800 units.
    static constexpr float zoomFactor = 0.9f;
    // How many target radii away the camera is allowed to get.
    static constexpr float targetMargin = 2.5f;

public:
    Camera(float initialDistance = 4.0f, float initialAzimuth = 0.0f, float initialElevation = 20.0f)
        : distance(initialDistance), azimuth(initialAzimuth), elevation(initialElevation) {}

    void rotate(float deltaX, float deltaY) {
        azimuth   -= deltaX * sensitivity;
        elevation += deltaY * sensitivity;

        if (elevation > 89.0f)  elevation = 89.0f;
        if (elevation < -89.0f) elevation = -89.0f;
    }

    // Call on every target change: recomputes the zoom bounds from the new
    // body's bounding radius and pulls the current distance back into range.
    void focusOn(float targetRadius) {
        minDistance = std::max(targetRadius * targetMargin, 0.01f);
        maxDistance = std::max(minDistance * 4.0f, 800.0f);
        distance    = std::clamp(distance, minDistance, maxDistance);
    }

    void zoom(float amount) {
        distance *= std::pow(zoomFactor, amount);
        distance  = std::clamp(distance, minDistance, maxDistance);
    }

    glm::mat4 getViewMatrix(glm::vec3 target) const {
        return glm::lookAt(getPosition(target), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const {
        const float fov = glm::radians(45.0f);

        // The near plane tracks the camera distance. With the old fixed value of
        // 0.1 the small moons (Phobos has a radius of 0.011) were clipped away
        // exactly when you moved in to look at them. The 0.1 ceiling keeps depth
        // precision at normal distances identical to what it was before.
        const float nearPlane = std::clamp(distance * 0.005f, 0.002f, 0.1f);
        const float farPlane  = 4000.0f;

        return glm::perspective(fov, aspectRatio, nearPlane, farPlane);
    }

    glm::vec3 getPosition(glm::vec3 target) const {
        const float azimuthRad   = glm::radians(azimuth);
        const float elevationRad = glm::radians(elevation);

        const float x = distance * glm::cos(elevationRad) * glm::sin(azimuthRad);
        const float y = distance * glm::sin(elevationRad);
        const float z = distance * glm::cos(elevationRad) * glm::cos(azimuthRad);

        return target + glm::vec3(x, y, z);
    }
};
