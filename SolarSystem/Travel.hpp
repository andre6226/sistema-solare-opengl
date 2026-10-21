#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include "CelestialBody.hpp"

// Distance covered along a straight leg of length L at fraction s of the flight. It
// grows exponentially away from the start (the camera r0 from the body it leaves)
// and shrinks exponentially onto the end (r1 from the destination): leaving and
// arriving take their time, the empty millions of km in between go by fast.
// What is left of the leg in the second half, computed directly: L minus a nearly
// equal distance would lose the last kilometres to rounding.
inline double travelRemaining(double s, double L, double r1) {
    return std::clamp(r1 * (std::pow((0.5 * L + r1) / r1, 2.0 - 2.0 * s) - 1.0), 0.0, L);
}
inline double travelDistance(double s, double L, double r0, double r1) {
    if (s <= 0.5) return std::min(L, r0 * (std::pow((0.5 * L + r0) / r0, 2.0 * s) - 1.0));
    return L - travelRemaining(s, L, r1);
}

// Cinematic flight to a body whose label was clicked: the camera turns towards it,
// eases out, rushes through the middle with a widening field of view, and eases
// into orbit on its lit side, where the orbit camera takes over. Positions are
// re-read every frame, so both bodies keep moving under it.
class Travel {
public:
    struct Pose { glm::vec3 position, forward; float fov; };

    bool active() const { return from != nullptr; }
    const CelestialBody* origin() const { return from; }
    const CelestialBody* destination() const { return to; }
    glm::vec3 arrivalDirection() const { return endDir; }
    // Past halfway the world should be centred on the destination (float precision).
    bool pastHalfway() const { return eased() >= 0.5f; }
    bool arrived() const { return t >= 1.0f; }
    void stop() { from = to = nullptr; }

    void start(const CelestialBody* a, const CelestialBody* b, glm::vec3 cameraPos,
               glm::vec3 cameraForward, glm::vec3 sunPos) {
        from = a; to = b; t = 0.0f;
        startOffset = cameraPos - a->getWorldPosition();
        lookStart = glm::normalize(cameraForward);
        const glm::vec3 approach = glm::normalize(cameraPos - b->getWorldPosition());
        const glm::vec3 toSun = sunPos - b->getWorldPosition();   // zero when b is the Sun
        const glm::vec3 lit = glm::length(toSun) < 1.0f ? approach : glm::normalize(toSun);
        const glm::vec3 half = approach + lit;   // arrive between where we come from and the Sun
        endDir = glm::length(half) > 1e-3f ? glm::normalize(half) : lit;
        endDist = 4.0f * b->getScale();          // where Camera::focusOn frames a body
        const double L = glm::length(b->getWorldPosition() + endDir * endDist - cameraPos);
        duration = std::clamp(2.0f + 0.4f * static_cast<float>(std::log(std::max(L / endDist, 1.0))), 3.0f, 9.0f);
    }

    Pose step(float realDelta) {
        t = std::min(1.0f, t + realDelta / duration);
        return pose();
    }

    Pose pose() const {
        const float s = eased();
        const glm::vec3 p0 = from->getWorldPosition() + startOffset;
        const glm::vec3 p1 = to->getWorldPosition() + endDir * endDist;
        const glm::vec3 leg = p1 - p0;
        const double L = glm::length(leg);
        // Measured from whichever end is near: the world is centred there, so float
        // positions keep their precision on arrival at a small, distant body.
        glm::vec3 position = p1;
        if (L > 0.0) {
            const glm::vec3 dir = leg / static_cast<float>(L);
            position = s <= 0.5f
                ? p0 + dir * static_cast<float>(travelDistance(s, L, std::max(1.0f, glm::length(startOffset)), endDist))
                : p1 - dir * static_cast<float>(travelRemaining(s, L, endDist));
        }

        // Turn towards the destination during the first third, then keep it centred.
        const glm::vec3 toB = glm::normalize(to->getWorldPosition() - position);
        const float w = glm::smoothstep(0.0f, 0.35f, t);
        const glm::vec3 forward = slerp(lookStart, toB, w);
        const float cruise = std::sin(glm::pi<float>() * s);
        return {position, forward, 45.0f + 20.0f * cruise * cruise};
    }

private:
    const CelestialBody* from = nullptr;
    const CelestialBody* to = nullptr;
    glm::vec3 startOffset{0.0f}, lookStart{0.0f, 0.0f, -1.0f}, endDir{0.0f, 0.0f, 1.0f};
    float endDist = 1.0f, duration = 5.0f, t = 0.0f;

    float eased() const { return t * t * (3.0f - 2.0f * t); }   // zero speed at both ends

    static glm::vec3 slerp(glm::vec3 a, glm::vec3 b, float w) {
        const float d = std::clamp(glm::dot(a, b), -1.0f, 1.0f);
        const float angle = std::acos(d);
        if (angle < 1e-4f) return b;
        glm::vec3 perp = b - a * d;
        if (glm::length(perp) < 1e-4f) perp = glm::cross(a, std::abs(a.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0));
        perp = glm::normalize(perp);
        return std::cos(angle * w) * a + std::sin(angle * w) * perp;
    }
};
