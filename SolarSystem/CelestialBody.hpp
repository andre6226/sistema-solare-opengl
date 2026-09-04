#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include "AstronomicalData.hpp"
#include "Geometry.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

// How a body is shaded. This used to be three string comparisons per body per
// frame (name == "Sun", name == "SaturnRings", ...), which tied the rendering
// path to display names. The values must stay in sync with the constants at the
// top of base.frag.
enum class BodyType : int {
    Planet = 0,  // lit by the Sun, textured
    Sun    = 1,  // emissive, unlit
    Ring   = 2,  // annulus carved out of a quad, receives the parent's shadow
    Sky    = 3   // background star sphere, unlit
};

class CelestialBody {
private:
    std::string name;
    Texture* texture;
    Geometry* geometry;
    float orbitRadius;
    float orbitalSpeed;
    float rotationSpeed;
    float axialTilt;
    float scale;
    float orbitAngle;
    float rotationAngle;
    BodyType type = BodyType::Planet;
    bool tiltChildren = true;
    bool navigable    = true;

    // The real measurements this node was built from, kept so the HUD can show
    // them. Null for the rings and the star sphere, which are not real bodies.
    const Astro::BodyData* data = nullptr;

    // --- Derived state ---
    // Computed once per frame by update() and only read by draw(). These used to
    // be rebuilt in both methods: double the cost and, worse, two copies of the
    // same formula to keep in sync by hand.
    glm::vec3 worldPosition{0.0f};
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 childMatrix{1.0f};
    glm::mat3 normalMatrix{1.0f};

    CelestialBody* parent;
    std::vector<CelestialBody*> satellites;

    // Wraps the angle back into [0, 2pi). The previous version subtracted 2pi
    // once only: with a high time scale the angle grew without ever returning to
    // the interval, losing float precision.
    static float wrapAngle(float angle) {
        angle = std::fmod(angle, glm::two_pi<float>());
        if (angle < 0.0f) angle += glm::two_pi<float>();
        return angle;
    }

    // Back-face culling is on globally. Two bodies need a local exception: the
    // star sphere is viewed from the inside, so it is its outward-facing
    // triangles that must go, and the ring quad is a flat surface meant to be
    // seen from above and below alike.
    void beginFaceCulling() const {
        if (type == BodyType::Sky)       glCullFace(GL_FRONT);
        else if (type == BodyType::Ring) glDisable(GL_CULL_FACE);
    }

    void endFaceCulling() const {
        if (type == BodyType::Sky)       glCullFace(GL_BACK);
        else if (type == BodyType::Ring) glEnable(GL_CULL_FACE);
    }

public:
    CelestialBody(std::string bodyName, Texture* bodyTexture, Geometry* bodyGeometry,
                  float radius, float orbitSpeed, float spinSpeed, float tilt, float size)
        : name(std::move(bodyName)), texture(bodyTexture), geometry(bodyGeometry),
          orbitRadius(radius), orbitalSpeed(orbitSpeed), rotationSpeed(spinSpeed),
          axialTilt(tilt), scale(size), orbitAngle(0.0f), rotationAngle(0.0f),
          parent(nullptr) {}

    const std::string& getName() const { return name; }

    CelestialBody* getParent() const { return parent; }
    const std::vector<CelestialBody*>& getSatellites() const { return satellites; }

    void addSatellite(CelestialBody* satellite) {
        satellite->parent = this;
        satellites.push_back(satellite);
    }

    void update(float deltaTime, const glm::mat4& parentMatrix = glm::mat4(1.0f)) {
        orbitAngle    = wrapAngle(orbitAngle    + orbitalSpeed  * deltaTime);
        rotationAngle = wrapAngle(rotationAngle + rotationSpeed * deltaTime);

        glm::mat4 orbitMatrix = glm::mat4(1.0f);
        orbitMatrix = glm::rotate(orbitMatrix, orbitAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        orbitMatrix = glm::translate(orbitMatrix, glm::vec3(orbitRadius, 0.0f, 0.0f));
        // Undo the orbital rotation: the body translates along its orbit but
        // keeps a fixed orientation with respect to space (Galilean frame).
        orbitMatrix = glm::rotate(orbitMatrix, -orbitAngle, glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat4 positionMatrix = parentMatrix * orbitMatrix;
        worldPosition = glm::vec3(positionMatrix[3]);

        const glm::mat4 tiltedMatrix =
            glm::rotate(positionMatrix, axialTilt, glm::vec3(0.0f, 0.0f, 1.0f));

        modelMatrix = glm::rotate(tiltedMatrix, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));

        normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

        // Children inherit the axial tilt only if they orbit the parent's
        // equatorial plane (the moons of Mars and Uranus, Saturn's rings).
        // The Moon instead follows the ecliptic, not Earth's equator.
        childMatrix = tiltChildren ? tiltedMatrix : positionMatrix;

        for (CelestialBody* satellite : satellites) {
            satellite->update(deltaTime, childMatrix);
        }
    }

    // Alpha-blended bodies are held back for a second pass, so that the opaque
    // ones behind them are already in the depth buffer when they are drawn.
    bool isTransparent() const { return type == BodyType::Ring; }

    // Renders this node alone, without recursing into its satellites.
    void drawSelf(const UniformLocations& locs) const {
        glUniformMatrix4fv(locs.model, 1, GL_FALSE, &modelMatrix[0][0]);
        glUniformMatrix3fv(locs.normalMatrix, 1, GL_FALSE, &normalMatrix[0][0]);
        glUniform1i(locs.bodyType, static_cast<int>(type));

        // The ring shader needs the geometry of the body casting the shadow onto
        // it. It used to reverse-engineer that radius out of the model matrix by
        // dividing by the literal 2.3, a constant duplicated in the C++ code
        // that built the rings: changing one silently broke the other.
        if (type == BodyType::Ring && parent != nullptr) {
            glUniform3fv(locs.parentCenter, 1, &parent->worldPosition[0]);
            glUniform1f(locs.parentRadius, parent->scale);
        }

        if (texture != nullptr) {
            texture->bind(0);
        }

        beginFaceCulling();
        geometry->draw();
        endFaceCulling();
    }

    // Recurses through the subtree drawing the opaque nodes only.
    void drawOpaque(const UniformLocations& locs) const {
        if (!isTransparent()) {
            drawSelf(locs);
        }

        for (const CelestialBody* satellite : satellites) {
            satellite->drawOpaque(locs);
        }
    }

    // Collects the alpha-blended nodes of this subtree for the second pass.
    void collectTransparent(std::vector<CelestialBody*>& out) {
        if (isTransparent()) out.push_back(this);

        for (CelestialBody* satellite : satellites) {
            satellite->collectTransparent(out);
        }
    }

    void setType(BodyType bodyType) { type = bodyType; }
    void setData(const Astro::BodyData* bodyData) { data = bodyData; }
    const Astro::BodyData* getData() const { return data; }
    void setTiltChildren(bool tilt) { tiltChildren = tilt; }

    // Non-navigable bodies (the rings) stay in the render tree but are skipped
    // by the arrow-key selection.
    void setNavigable(bool value) { navigable = value; }
    bool isNavigable() const { return navigable; }

    // Radius of the sphere enclosing the body plus anything rigidly attached to
    // it, that is, children with a zero orbit radius (Saturn's rings). The
    // camera uses it to avoid moving inside the body it is looking at.
    float getBoundingRadius() const {
        float radius = scale;
        for (const CelestialBody* child : satellites) {
            if (child->orbitRadius == 0.0f) {
                radius = std::max(radius, child->scale);
            }
        }
        return radius;
    }

    glm::vec3 getWorldPosition() const {
        return worldPosition;
    }
};
