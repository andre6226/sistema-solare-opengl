#pragma once
#include <algorithm>
#include <vector>
#include <memory>
#include <string>
#include "CelestialBody.hpp"
#include "Sphere.hpp"
#include "Quad.hpp"
#include "AstronomicalData.hpp"

class SolarSystem {
private:
    // DECLARATION ORDER IS SIGNIFICANT: members are destroyed in reverse order,
    // so the geometries and the textures must come BEFORE the bodies pointing at
    // them, otherwise the Texture*/Geometry* held by each CelestialBody would
    // already be dangling while the bodies are being destroyed.
    Sphere sphereGeometry;
    Quad ringGeometry;
    Astro::ScaleConverter converter;
    std::vector<std::unique_ptr<Texture>> textures;

    std::vector<std::unique_ptr<CelestialBody>> bodies;
    std::unique_ptr<CelestialBody> skySphere;

    CelestialBody* sun;
    CelestialBody* currentTarget;

    // Outer radius of Saturn's rings, in units of Saturn's own radius. Only the
    // C++ side needs it now: the shader receives the parent radius directly.
    static constexpr float ringOuterRadius = 2.3f;

public:
    SolarSystem() : sphereGeometry(1.0f, 100, 50), sun(nullptr), currentTarget(nullptr) {
        buildSystem();
        currentTarget = sun;
    }

    // Holds geometries with OpenGL handles and a web of internal pointers:
    // copying or moving it would make no sense and would break the tree.
    SolarSystem(const SolarSystem&)            = delete;
    SolarSystem& operator=(const SolarSystem&) = delete;
    SolarSystem(SolarSystem&&)                 = delete;
    SolarSystem& operator=(SolarSystem&&)      = delete;

    void selectParent() {
        if (currentTarget && currentTarget->getParent() != nullptr) {
            currentTarget = currentTarget->getParent();
        }
    }

    void selectFirstChild() {
        if (!currentTarget) return;

        for (CelestialBody* child : currentTarget->getSatellites()) {
            if (child->isNavigable()) {
                currentTarget = child;
                return;
            }
        }
    }

    void selectNextSibling()     { moveToSibling(+1); }
    void selectPreviousSibling() { moveToSibling(-1); }

    glm::vec3 getTargetPosition() const {
        if (currentTarget) return currentTarget->getWorldPosition();
        return glm::vec3(0.0f);
    }

    std::string getTargetName() const {
        if (currentTarget) return currentTarget->getName();
        return "Sun";
    }

    // Bounding radius of the target: the camera uses it to work out how close it
    // may get without ending up inside the body.
    float getTargetRadius() const {
        if (currentTarget) return currentTarget->getBoundingRadius();
        return 1.0f;
    }

    void update(float deltaTime) {
        if (sun) sun->update(deltaTime, glm::mat4(1.0f));
        if (skySphere) skySphere->update(deltaTime, glm::mat4(1.0f));
    }

    void draw(const UniformLocations& locs) const {
        if (sun) sun->draw(locs);
        if (skySphere) skySphere->draw(locs);
    }

private:
    Texture* loadTexture(const std::string& path) {
        auto texture = std::make_unique<Texture>(path);
        Texture* raw = texture.get();
        textures.push_back(std::move(texture));
        return raw;
    }

    CelestialBody* createBody(const Astro::BodyData& data, CelestialBody* parent) {
        const float orbitRadius   = converter.getOrbitRadius(data);
        const float scale         = converter.getScale(data);
        const float orbitalSpeed  = converter.getOrbitalSpeed(data);
        const float rotationSpeed = converter.getRotationSpeed(data);
        const float axialTilt     = converter.getAxialTilt(data);

        Texture* texture = loadTexture(std::string("resources/") + data.name + ".jpg");

        auto body = std::make_unique<CelestialBody>(data.name, texture, &sphereGeometry,
                                                    orbitRadius, orbitalSpeed, rotationSpeed,
                                                    axialTilt, scale);
        CelestialBody* raw = body.get();

        if (parent != nullptr) {
            parent->addSatellite(raw);
        } else if (sun == nullptr) {
            sun = raw;
        }

        bodies.push_back(std::move(body));
        return raw;
    }

    void buildSystem() {
        // --- THE CENTRE ---
        sun = createBody(Astro::Sun, nullptr);
        sun->setType(BodyType::Sun);

        // --- BACKGROUND STAR SPHERE ---
        // Not part of the hierarchy: it is drawn separately, unlit, at a radius
        // large enough to enclose the whole system.
        skySphere = std::make_unique<CelestialBody>("Stars", loadTexture("resources/Stars.jpg"),
                                                    &sphereGeometry, 0.0f, 0.0f, 0.0f, 0.0f, 2500.0f);
        skySphere->setType(BodyType::Sky);

        // --- INNER PLANETS AND THEIR MOONS ---
        createBody(Astro::Mercury, sun);
        createBody(Astro::Venus, sun);

        CelestialBody* earth = createBody(Astro::Earth, sun);
        earth->setTiltChildren(false); // the Moon follows the ecliptic, not Earth's equator
        createBody(Astro::Moon, earth);

        CelestialBody* mars = createBody(Astro::Mars, sun);
        createBody(Astro::Deimos, mars);
        createBody(Astro::Phobos, mars);

        // --- GAS GIANTS ---
        CelestialBody* jupiter = createBody(Astro::Jupiter, sun);
        CelestialBody* saturn  = createBody(Astro::Saturn, sun);
        CelestialBody* uranus  = createBody(Astro::Uranus, sun);
        CelestialBody* neptune = createBody(Astro::Neptune, sun);

        // --- GALILEAN MOONS OF JUPITER ---
        createBody(Astro::Io, jupiter);
        createBody(Astro::Europa, jupiter);
        createBody(Astro::Ganymede, jupiter);
        createBody(Astro::Callisto, jupiter);

        // --- MOONS OF SATURN ---
        createBody(Astro::Titan, saturn);
        createBody(Astro::Enceladus, saturn);

        // --- SATURN'S RINGS ---
        // A child of Saturn so that they inherit its axial tilt, but not a
        // selectable target for the arrow keys.
        auto rings = std::make_unique<CelestialBody>(
            "Saturn's Rings", loadTexture("resources/SaturnRings.png"), &ringGeometry,
            0.0f, 0.0f, 0.0f, 0.0f, ringOuterRadius * converter.getScale(Astro::Saturn));
        rings->setType(BodyType::Ring);
        rings->setNavigable(false);
        saturn->addSatellite(rings.get());
        bodies.push_back(std::move(rings));

        // --- OUTER MOONS ---
        createBody(Astro::Titania, uranus);
        createBody(Astro::Triton, neptune);
    }

    // Walks the siblings in the given direction, skipping the non-navigable
    // ones. The previous version broke out of the loop on the name
    // "SaturnRings": any moon added after the rings would have been unreachable.
    void moveToSibling(int direction) {
        if (!currentTarget || !currentTarget->getParent()) return;

        const auto& siblings = currentTarget->getParent()->getSatellites();
        const auto position = std::find(siblings.begin(), siblings.end(), currentTarget);
        if (position == siblings.end()) return;

        const int index = static_cast<int>(std::distance(siblings.begin(), position));
        const int count = static_cast<int>(siblings.size());

        for (int i = index + direction; i >= 0 && i < count; i += direction) {
            if (siblings[i]->isNavigable()) {
                currentTarget = siblings[i];
                return;
            }
        }
    }
};
