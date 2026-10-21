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

// How a body is shaded. The values must stay in sync with the constants at the
// top of base.vert and base.frag.
enum class BodyType : int {
    Planet  = 0,  // lit by the Sun, textured
    Sun     = 1,  // emissive photosphere with overexposed glare
    Ring    = 2,  // annulus carved out of a quad, receives the parent's shadow
    Sky     = 3,  // background star sphere, unlit at infinity
    SunGlow = 4   // radial solar corona and glare halo
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
    double orbitAngle    = 0.0;
    double rotationAngle = 0.0;

    // Set by SolarSystem from the ephemeris and the IAU models; when present they
    // replace the built-in Kepler orbit and tilt + spin.
    bool hasEphemerisOffset = false;
    glm::dvec3 ephemerisOffset{0.0};   // from the parent, world axes, km
    bool hasPole = false;
    glm::dmat3 bodyFrame{1.0};         // mesh axes to world, with the spin
    glm::dmat3 equatorFrame{1.0};      // same without the spin: the frame rings and tilted moons use
    glm::vec3 shape{1.0f};             // per-axis radius over `scale`, mesh axes
    Astro::KeplerianBasis orbitBasis{};
    BodyType type = BodyType::Planet;
    bool tiltChildren = true;
    bool navigable    = true;

    // The real measurements this node was built from, kept so the HUD can show
    // them. Null for the rings and the star sphere, which are not real bodies.
    const Astro::BodyData* data = nullptr;

    // Exact heliocentric state in double precision so 10-billion-km orbits never
    // quantise small bodies or camera motion in float32.
    glm::dvec3 exactPosition{0.0};
    glm::mat3  exactOrientation{1.0f};

    // Render-space state (shifted so the focused target sits at the origin).
    glm::vec3 worldPosition{0.0f};
    glm::mat4 modelMatrix{1.0f};
    glm::mat3 normalMatrix{1.0f};

    CelestialBody* parent;
    std::vector<CelestialBody*> satellites;

    static double wrapAngle(double angle) {
        angle = std::fmod(angle, glm::two_pi<double>());
        if (angle < 0.0) angle += glm::two_pi<double>();
        return angle;
    }

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
          axialTilt(tilt), scale(size), parent(nullptr) {}

    const std::string& getName() const { return name; }
    float getOrbitRadius() const { return orbitRadius; }
    float getScale() const { return scale; }
    Texture* getTexture() const { return texture; }

    CelestialBody* getParent() const { return parent; }
    const std::vector<CelestialBody*>& getSatellites() const { return satellites; }

    void addSatellite(CelestialBody* satellite) {
        satellite->parent = this;
        satellites.push_back(satellite);
    }

    void update(double julianCenturies, float deltaTime, const glm::dmat4& parentMatrix = glm::dmat4(1.0)) {
        rotationAngle = wrapAngle(rotationAngle + static_cast<double>(rotationSpeed) * deltaTime);

        glm::dvec3 relPosition(0.0);
        if (hasEphemerisOffset) {
            relPosition = ephemerisOffset;
        } else if (data != nullptr && data->orbit.a0 > 0.0) {
            orbitBasis = Astro::evaluateKeplerianBasis(data->orbit, julianCenturies);
            orbitBasis.P_world = glm::dmat3(parentMatrix) * orbitBasis.P_world;
            orbitBasis.Q_world = glm::dmat3(parentMatrix) * orbitBasis.Q_world;
            relPosition = glm::dmat3(parentMatrix) * Astro::evaluateOrbitVertex(orbitBasis, orbitBasis.E_rad);
        } else if (orbitRadius > 0.0f) {
            orbitAngle = wrapAngle(orbitAngle + static_cast<double>(orbitalSpeed) * deltaTime);
            glm::dmat4 fallbackOrbit(1.0);
            fallbackOrbit = glm::rotate(fallbackOrbit, orbitAngle, glm::dvec3(0.0, 1.0, 0.0));
            fallbackOrbit = glm::translate(fallbackOrbit, glm::dvec3(static_cast<double>(orbitRadius), 0.0, 0.0));
            relPosition = glm::dmat3(parentMatrix) * glm::dvec3(fallbackOrbit[3]);
        }

        // Offsets are in world axes, so only the parent's position is inherited, never its tilt.
        exactPosition = glm::dvec3(parentMatrix[3]) + relPosition;
        const glm::dmat4 positionMatrix = glm::translate(glm::dmat4(1.0), exactPosition);

        // Bodies without an IAU model: tilting about -X leans the pole toward world -Z
        // (ecliptic longitude 90 deg), where Earth's north pole points.
        const glm::dmat4 tiltedMatrix = hasPole
            ? positionMatrix * glm::dmat4(equatorFrame)
            : glm::rotate(positionMatrix, static_cast<double>(axialTilt), glm::dvec3(-1.0, 0.0, 0.0));

        const glm::dmat4 spinMatrix = hasPole
            ? positionMatrix * glm::dmat4(bodyFrame)
            : glm::rotate(tiltedMatrix, rotationAngle, glm::dvec3(0.0, 1.0, 0.0));

        exactOrientation = glm::mat3(spinMatrix);
        normalMatrix     = glm::transpose(glm::inverse(exactOrientation * glm::mat3(glm::scale(glm::mat4(1.0f), shape))));
        applyOrigin(glm::dvec3(0.0), false);

        const glm::dmat4 childMatrix = tiltChildren ? tiltedMatrix : positionMatrix;

        for (CelestialBody* satellite : satellites) {
            satellite->update(julianCenturies, deltaTime, childMatrix);
        }
    }

    void update(float deltaTime, const glm::dmat4& parentMatrix = glm::dmat4(1.0)) {
        update(0.0, deltaTime, parentMatrix);
    }

    // Shifts render-space coordinates so the focused target sits at (0,0,0),
    // keeping float32 vertex and camera math jitter-free at any orbit radius.
    void applyOrigin(const glm::dvec3& origin, bool recurse = true) {
        worldPosition = glm::vec3(exactPosition - origin);
        modelMatrix   = glm::translate(glm::mat4(1.0f), worldPosition)
                      * glm::mat4(exactOrientation)
                      * glm::scale(glm::mat4(1.0f), scale * shape);

        if (!recurse) return;
        for (CelestialBody* satellite : satellites) {
            satellite->applyOrigin(origin);
        }
    }

    void setEphemerisOffset(const glm::dvec3& offset, const Astro::KeplerianBasis& basis) {
        hasEphemerisOffset = true;
        ephemerisOffset = offset;
        orbitBasis = basis;
    }
    void setPole(const glm::dmat3& body, const glm::dmat3& equator) {
        hasPole = true;
        bodyFrame = body;
        equatorFrame = equator;
    }
    // Triaxial radii along the mesh axes, km; `scale` becomes the largest.
    void setRadii(const glm::vec3& radii) {
        scale = std::max(radii.x, std::max(radii.y, radii.z));
        shape = radii / scale;
    }
    const Astro::KeplerianBasis& getOrbitBasis() const { return orbitBasis; }

    bool isTransparent() const { return type == BodyType::Ring; }

    void drawSelf(const UniformLocations& locs) const {
        glUniformMatrix4fv(locs.model, 1, GL_FALSE, &modelMatrix[0][0]);
        glUniformMatrix3fv(locs.normalMatrix, 1, GL_FALSE, &normalMatrix[0][0]);
        glUniform1i(locs.bodyType, static_cast<int>(type));

        if (type == BodyType::Ring && parent != nullptr) {
            glUniform3fv(locs.parentCenter, 1, &parent->worldPosition[0]);
            glUniform1f(locs.parentRadius, parent->scale);
        }

        // Dynamic Eclipse Shadow Casters
        glm::vec3 casterPos[8];
        float casterRadius[8];
        int casterCount = 0;

        if (type == BodyType::Planet && parent != nullptr) {
            if (parent->type == BodyType::Sun) {
                for (const CelestialBody* sat : satellites) {
                    if (sat->isNavigable() && casterCount < 8) {
                        casterPos[casterCount] = sat->getWorldPosition();
                        casterRadius[casterCount] = sat->getScale();
                        casterCount++;
                    }
                }
            } else {
                casterPos[0] = parent->getWorldPosition();
                casterRadius[0] = parent->getScale();
                casterCount = 1;
            }
        }

        glUniform1i(locs.shadowCasterCount, casterCount);
        if (casterCount > 0) {
            glUniform3fv(locs.shadowCasterPos, casterCount, &casterPos[0][0]);
            glUniform1fv(locs.shadowCasterRadius, casterCount, &casterRadius[0]);
        }

        if (texture != nullptr) {
            texture->bind(0);
        }

        beginFaceCulling();
        geometry->draw();
        endFaceCulling();
    }

    void drawOpaque(const UniformLocations& locs) const {
        if (!isTransparent()) {
            drawSelf(locs);
        }

        for (const CelestialBody* satellite : satellites) {
            satellite->drawOpaque(locs);
        }
    }

    void collectTransparent(std::vector<CelestialBody*>& out) {
        if (isTransparent()) out.push_back(this);

        for (CelestialBody* satellite : satellites) {
            satellite->collectTransparent(out);
        }
    }

    void setType(BodyType bodyType) { type = bodyType; }
    void setOrbitAngle(double angle) { orbitAngle = wrapAngle(angle); }
    void setRotationAngle(double angle) { rotationAngle = wrapAngle(angle); }
    double getRotationAngle() const { return rotationAngle; }
    void setData(const Astro::BodyData* bodyData) { data = bodyData; }
    const Astro::BodyData* getData() const { return data; }
    void setTiltChildren(bool tilt) { tiltChildren = tilt; }
    bool getTiltChildren() const { return tiltChildren; }
    float getAxialTilt() const { return axialTilt; }

    void setNavigable(bool value) { navigable = value; }
    bool isNavigable() const { return navigable; }

    float getBoundingRadius() const {
        float radius = scale;
        for (const CelestialBody* child : satellites) {
            if (child->orbitRadius == 0.0f) {
                radius = std::max(radius, child->scale);
            }
        }
        return radius;
    }

    const glm::dvec3& getExactPosition() const { return exactPosition; }
    glm::vec3 getWorldPosition() const { return worldPosition; }
    const glm::mat4& getModelMatrix() const { return modelMatrix; }
};
