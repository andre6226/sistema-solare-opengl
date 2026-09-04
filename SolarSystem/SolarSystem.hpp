#pragma once
#include <algorithm>
#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include "CelestialBody.hpp"
#include "Sphere.hpp"
#include "Quad.hpp"
#include "ShapeModel.hpp"
#include "AstronomicalData.hpp"

class SolarSystem {
private:
    // DECLARATION ORDER IS SIGNIFICANT: members are destroyed in reverse order,
    // so the geometries and the textures must come BEFORE the bodies pointing at
    // them, otherwise the Texture*/Geometry* held by each CelestialBody would
    // already be dangling while the bodies are being destroyed.
    Sphere sphereGeometry;
    Quad ringGeometry;
    // One mesh per irregular body: unlike the shared sphere, each measured shape
    // is unique. Declared before `bodies` so it outlives the nodes pointing at it.
    std::vector<std::unique_ptr<ShapeModel>> shapeModels;
    Astro::ScaleConverter converter;
    std::vector<std::unique_ptr<Texture>> textures;

    std::vector<std::unique_ptr<CelestialBody>> bodies;
    std::unique_ptr<CelestialBody> skySphere;

    CelestialBody* sun;
    CelestialBody* currentTarget;

    // Non-owning list of the alpha-blended bodies, gathered once after the tree
    // is built and re-sorted per frame for the transparent pass.
    std::vector<CelestialBody*> transparentBodies;

    // Outer radius of Saturn's rings, in units of Saturn's own radius. Only the
    // C++ side needs it now: the shader receives the parent radius directly.
    static constexpr float ringOuterRadius = 2.3f;

public:
    // 50 parallels, 100 meridians. The arguments used to be the other way round,
    // which spent 100 subdivisions on latitude (1.8 degrees apart) and only 50
    // on longitude (7.2 degrees apart): the silhouette was visibly faceted at
    // the equator while vertices piled up at the poles, where they all converge
    // on the same point anyway.
    SolarSystem() : sphereGeometry(1.0f, 50, 100), sun(nullptr), currentTarget(nullptr) {
        buildSystem();
        currentTarget = sun;

        if (sun) sun->collectTransparent(transparentBodies);
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

    // Real measurements behind the current target, for the HUD. Null only if
    // there is no target at all.
    const Astro::BodyData* getTargetData() const {
        return currentTarget ? currentTarget->getData() : nullptr;
    }

    // How many selectable satellites the target has (the rings do not count).
    int getTargetSatelliteCount() const {
        if (!currentTarget) return 0;

        int count = 0;
        for (const CelestialBody* child : currentTarget->getSatellites()) {
            if (child->isNavigable()) ++count;
        }
        return count;
    }

    // Breadcrumb from the Sun down to the current target, e.g. "Sun > Saturn".
    std::string getTargetPath() const {
        std::vector<std::string> chain;
        for (const CelestialBody* node = currentTarget; node != nullptr; node = node->getParent()) {
            chain.push_back(node->getName());
        }

        std::string path;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            if (!path.empty()) path += "  >  ";
            path += *it;
        }
        return path;
    }

    // Bounding radius of the target: the camera uses it to work out how close it
    // may get without ending up inside the body.
    float getTargetRadius() const {
        if (currentTarget) return currentTarget->getBoundingRadius();
        return 1.0f;
    }

    void update(float deltaTime) {
        if (sun) sun->update(deltaTime, glm::mat4(1.0f));
    }

    // The star sphere is re-centred on the camera every frame so the stars stay
    // effectively at infinity. Anchored at the origin it showed parallax: travel
    // out to Neptune and the constellations visibly slid across the sky.
    void updateSkyPosition(const glm::vec3& cameraPosition) {
        if (skySphere) {
            skySphere->update(0.0f, glm::translate(glm::mat4(1.0f), cameraPosition));
        }
    }

    void draw(const UniformLocations& locs, const glm::vec3& cameraPosition) {
        // First pass: everything opaque, so the depth buffer is complete before
        // anything is blended into the frame.
        if (sun) sun->drawOpaque(locs);
        if (skySphere) skySphere->drawSelf(locs);

        if (transparentBodies.empty()) return;

        // Second pass: alpha-blended bodies, farthest first and without writing
        // depth. Writing depth here would let a semi-transparent surface reject
        // whatever lies behind it — Uranus and Neptune are drawn after Saturn,
        // so they could vanish behind its rings.
        const auto distanceSquared = [&cameraPosition](const CelestialBody* body) {
            const glm::vec3 offset = body->getWorldPosition() - cameraPosition;
            return glm::dot(offset, offset);
        };

        std::sort(transparentBodies.begin(), transparentBodies.end(),
                  [&distanceSquared](const CelestialBody* a, const CelestialBody* b) {
                      return distanceSquared(a) > distanceSquared(b);
                  });

        glDepthMask(GL_FALSE);
        for (const CelestialBody* body : transparentBodies) {
            body->drawSelf(locs);
        }
        glDepthMask(GL_TRUE);
    }

private:
    Texture* loadTexture(const std::string& path) {
        auto texture = std::make_unique<Texture>(path);
        Texture* raw = texture.get();
        textures.push_back(std::move(texture));
        return raw;
    }

    // Loads a measured shape model, falling back to the shared sphere when the
    // file is absent so the program still runs without the optional data.
    Geometry* loadShape(const std::string& path, float textureLongitudeOffsetDegrees = 180.0f) {
        try {
            auto model = std::make_unique<ShapeModel>(path, textureLongitudeOffsetDegrees);
            std::cout << "[shape] " << path << ": " << model->getVertexCount()
                      << " vertices, " << model->getFaceCount() << " facets" << std::endl;
            Geometry* raw = model.get();
            shapeModels.push_back(std::move(model));
            return raw;
        } catch (const std::exception& e) {
            std::cerr << "[shape] " << e.what() << " - falling back to a sphere" << std::endl;
            return &sphereGeometry;
        }
    }

    CelestialBody* createBody(const Astro::BodyData& data, CelestialBody* parent,
                              Geometry* geometry = nullptr) {
        const float orbitRadius   = converter.getOrbitRadius(data);
        const float scale         = converter.getScale(data);
        const float orbitalSpeed  = converter.getOrbitalSpeed(data);
        const float rotationSpeed = converter.getRotationSpeed(data);
        const float axialTilt     = converter.getAxialTilt(data);

        Texture* texture = loadTexture(std::string("resources/") + data.name + ".jpg");

        auto body = std::make_unique<CelestialBody>(data.name, texture,
                                                    geometry ? geometry : &sphereGeometry,
                                                    orbitRadius, orbitalSpeed, rotationSpeed,
                                                    axialTilt, scale);
        CelestialBody* raw = body.get();

        if (parent != nullptr) {
            parent->addSatellite(raw);
        } else if (sun == nullptr) {
            sun = raw;
        }

        raw->setData(&data);
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
        // The two Martian moons are far from spherical, so they use measured
        // plate models rather than the shared UV sphere.
        createBody(Astro::Deimos, mars, loadShape("resources/models/Deimos.tab"));
        createBody(Astro::Phobos, mars, loadShape("resources/models/Phobos.tab"));

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
