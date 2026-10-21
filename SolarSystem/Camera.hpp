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

    // Zoom bounds and frustum planes adapted to body radius in 1:1 scale.
    float minDistance = 1.0f;
    float maxDistance = 3.0e10f;
    float nearPlane   = 0.1f;
    float farPlane    = 3.0e10f;
    float fieldOfView = 45.0f;

    static constexpr float sensitivity = 0.4f;
    // Multiplicative zoom: one wheel notch is always 10% of the current
    // distance, which keeps it usable across astronomical scales.
    static constexpr float zoomFactor = 0.9f;

public:
    Camera(float initialDistance = 4.0f, float initialAzimuth = 0.0f, float initialElevation = 20.0f)
        : distance(initialDistance), azimuth(initialAzimuth), elevation(initialElevation) {}

    void rotate(float deltaX, float deltaY) {
        azimuth   -= deltaX * sensitivity;
        elevation += deltaY * sensitivity;

        if (elevation > 89.0f)  elevation = 89.0f;
        if (elevation < -89.0f) elevation = -89.0f;
    }

    // Call on every target change: recomputes zoom bounds and near/far frustum
    // from the target bounding radius and frames the body at 4 radii away.
    void focusOn(float targetRadius) {
        minDistance = 2.0f * targetRadius;
        maxDistance = 50.0f * targetRadius;
        nearPlane   = targetRadius * 0.5f;
        farPlane    = 3.0e10f;
        distance    = std::clamp(4.0f * targetRadius, minDistance, maxDistance);
    }

    void zoom(float amount) {
        distance *= std::pow(zoomFactor, amount);
        distance  = std::clamp(distance, minDistance, maxDistance);
    }

    float getDistance() const { return distance; }
    float getMinDistance() const { return minDistance; }
    float getMaxDistance() const { return maxDistance; }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }

    glm::mat4 getViewMatrix(glm::vec3 target) const {
        return glm::lookAt(getPosition(target), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
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
