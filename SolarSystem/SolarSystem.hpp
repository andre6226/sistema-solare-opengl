#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include "CelestialBody.hpp"
#include "Sphere.hpp"
#include "Quad.hpp"
#include "ShapeModel.hpp"
#include "AstronomicalData.hpp"
#include "Hud.hpp"
#include "OrbitRenderer.hpp"
#include "Ephemeris.hpp"
#include "IauRotation.hpp"

class SolarSystem {
private:
    double simTimeSeconds = 0.0;
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

    OrbitRenderer orbitRenderer;

    Ephemeris ephemeris{"resources/de440_1900_2200.bin"};

    // How each body is driven: a DE440 item and/or an IAU rotation model.
    struct BodyLink {
        CelestialBody* body;
        int deItem;                    // -1 = keeps its Kepler elements
        const Astro::IauBody* iau;     // nullptr = keeps tilt + spin
        glm::dmat3 meshToBody;
        const Astro::SatelliteOrbit* moon = nullptr;   // fitted planetary moon
        std::shared_ptr<MoonTable> moonTable;           // its JPL correction
    };
    std::vector<BodyLink> links;
    CelestialBody* saturnRings = nullptr;

    // Outer edge of the ring texture, km: it runs from 70,750 km (inside the C ring)
    // to the F ring, measured on the Cassini division and the Encke gap. base.frag
    // keeps the inner 50.2% of the quad empty.
    static constexpr float ringOuterRadiusKm = 140920.0f;

    void recenterOnTarget() {
        if (sun && currentTarget) {
            sun->applyOrigin(currentTarget->getExactPosition());
        }
    }

public:
    // 50 parallels, 100 meridians.
    SolarSystem() : sphereGeometry(1.0f, 50, 100), sun(nullptr), currentTarget(nullptr) {
        // Initialize simulation clock to real-time UTC
        const auto now = std::chrono::system_clock::now();
        simTimeSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();

        buildSystem();
        currentTarget = sun;

        if (sun) {
            sun->collectTransparent(transparentBodies);
            setSimUnixSeconds(simTimeSeconds);
        }
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
            recenterOnTarget();
        }
    }

    void selectFirstChild() {
        if (!currentTarget) return;

        for (CelestialBody* child : currentTarget->getSatellites()) {
            if (child->isNavigable()) {
                currentTarget = child;
                recenterOnTarget();
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

    const Astro::BodyData* getTargetData() const {
        return currentTarget ? currentTarget->getData() : nullptr;
    }

    int getTargetSatelliteCount() const {
        if (!currentTarget) return 0;

        int count = 0;
        for (const CelestialBody* child : currentTarget->getSatellites()) {
            if (child->isNavigable()) ++count;
        }
        return count;
    }

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

    float getTargetRadius() const {
        if (currentTarget) return currentTarget->getBoundingRadius();
        return 1.0f;
    }

    void syncRotationsToEpoch(double julianCenturies) {
        const double D = julianCenturies * Astro::DAYS_PER_JULIAN_CENTURY;
        for (auto& body : bodies) {
            const Astro::BodyData* d = body->getData();
            if (d != nullptr && d->rotationPeriodDays != 0.0) {
                const double period = (d->axialTiltDegrees > 90.0)
                    ? std::abs(d->rotationPeriodDays)
                    : d->rotationPeriodDays;
                const double angle = (D / period) * glm::two_pi<double>();
                body->setRotationAngle(angle);
            }
        }
    }

    void update(float deltaTime) {
        setSimUnixSeconds(simTimeSeconds + static_cast<double>(deltaTime));
    }

    void updateSkyPosition(const glm::vec3& cameraPosition) {
        if (!skySphere) return;

        static const glm::mat4 skyOrientation =
            glm::mat4(Astro::galacticToWorld() * Astro::skyTextureToGalactic());

        skySphere->update(0.0, 0.0f, glm::dmat4(glm::translate(glm::mat4(1.0f), cameraPosition) * skyOrientation));
    }

    double getSimUnixSeconds() const { return simTimeSeconds; }
    void setSimUnixSeconds(double s) {
        simTimeSeconds = std::clamp(s, firstUnixSeconds, lastUnixSeconds);
        const double T = Astro::unixToJulianCenturies(simTimeSeconds);
        syncRotationsToEpoch(T);
        applyEphemeris();
        if (sun) {
            sun->update(T, 0.0f, glm::dmat4(1.0));
            recenterOnTarget();
        }
    }

    // The bundled ephemeris span, as the clock limits.
    static constexpr double firstUnixSeconds = -2208988800.0;  // 1900-01-01 00:00 UTC
    static constexpr double lastUnixSeconds  = 7289654399.0;   // 2200-12-31 23:59:59 UTC
    double getJulianDate() const { return Astro::unixToJulianDate(simTimeSeconds); }
    double getJulianCenturies() const { return Astro::unixToJulianCenturies(simTimeSeconds); }

    void draw(const UniformLocations& locs, const glm::vec3& cameraPosition) {
        // Keep the light anchored to the Sun in target-centered render space.
        if (sun) {
            const glm::vec3 sunPos = sun->getWorldPosition();
            glUniform3fv(locs.lightPos, 1, &sunPos[0]);
            glUniform1f(locs.sunRadius, sun->getScale());
        }

        // Background star sphere first, at infinity, without writing depth so
        // every solar-system body and beacon renders cleanly in front of it.
        if (skySphere) {
            glDepthMask(GL_FALSE);
            skySphere->drawSelf(locs);
            glDepthMask(GL_TRUE);
        }

        // Opaque 3D meshes.
        if (sun) sun->drawOpaque(locs);

        // True-scale solar corona around the Sun's 3D photosphere.
        drawSunGlow(locs, cameraPosition);

        if (transparentBodies.empty()) return;

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

    void toggleOrbitLines() { orbitRenderer.toggle(); }
    bool areOrbitLinesVisible() const { return orbitRenderer.isVisible(); }

    void drawOrbits(const glm::mat4& view, const glm::mat4& projection) {
        orbitRenderer.render(bodies, sun, currentTarget, view, projection);
    }

    // Projects distant bodies onto 2D screen space so the HUD can draw a
    // luminous dot (sized by radius and distance) and the body's name beside it.
    void collectMarkers(const glm::mat4& viewProj, const glm::vec3& cameraPosition,
                        unsigned int width, unsigned int height,
                        std::vector<BodyMarker>& out) const {
        out.clear();
        const float targetR = currentTarget ? currentTarget->getScale() : 0.0f;

        for (const auto& bodyPtr : bodies) {
            const CelestialBody* b = bodyPtr.get();
            if (!b->isNavigable() || b == currentTarget) continue;

            const glm::vec3 pos = b->getWorldPosition();
            const glm::vec3 toBody = pos - cameraPosition;
            const float dist = glm::length(toBody);
            if (dist <= 0.0f) continue;

            const float angularRadius = b->getScale() / dist;
            // Skip once the 3D body is large on screen.
            if (angularRadius > 0.035f) continue;

            // Show a moon's marker only when inspecting its parent system or
            // when its orbit is wide enough on screen to separate from the planet.
            const CelestialBody* parent = b->getParent();
            if (parent != nullptr && parent != sun) {
                const bool inSameSystem = (parent == currentTarget ||
                                           (currentTarget && currentTarget->getParent() == parent));
                if (!inSameSystem && (b->getOrbitRadius() / dist < 0.012f)) continue;
            }

            // Occlusion check against the currently focused body at (0,0,0).
            if (targetR > 0.0f) {
                const glm::vec3 dir = toBody / dist;
                const float t = glm::dot(-cameraPosition, dir);
                if (t > 0.0f && t < dist) {
                    const float perpSq = glm::dot(cameraPosition, cameraPosition) - t * t;
                    if (perpSq < targetR * targetR) continue;
                }
            }

            const glm::vec4 clip = viewProj * glm::vec4(pos, 1.0f);
            if (clip.w <= 0.0f) continue;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) continue;

            const float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
            const float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);

            // Illuminated fraction facing the camera (1 = full phase, 0 = new/backlit).
            float phase = 1.0f;
            if (b != sun && sun != nullptr) {
                const glm::vec3 toSun = sun->getWorldPosition() - pos;
                const float sunDist = glm::length(toSun);
                if (sunDist > 0.0f) {
                    phase = 0.5f * (1.0f + glm::dot(toSun / sunDist, -toBody / dist));
                }
            }

            // Apparent magnitude from radius / distance and illuminated phase.
            const float logAng = std::log10(std::max(angularRadius, 1.0e-9f));
            float prominence = std::clamp((logAng + 8.5f) / 5.5f, 0.20f, 1.0f);
            if (b == sun) prominence = 1.0f;

            // Fade the synthetic point out as soon as the real 3D sphere is
            // resolved (> ~2.5 px radius, such as the Moon seen from Earth), so
            // the 3D sphere and its true crescent/new phase are never covered.
            const float pointFade = std::clamp((0.0018f - angularRadius) / (0.0018f - 0.0008f), 0.0f, 1.0f);
            const float phaseFactor = (b == sun) ? 1.0f : std::pow( std::clamp(phase, 0.0f, 1.0f), 0.75f);

            BodyMarker m;
            m.name       = b->getName();
            m.x          = sx;
            m.y          = sy;
            m.prominence = prominence * (0.25f + 0.75f * phaseFactor);
            m.dotAlpha   = (0.25f + 0.75f * prominence) * phaseFactor * pointFade;
            m.labelAlpha = (0.35f + 0.55f * prominence) * (0.25f + 0.75f * phaseFactor);
            // Offset label outside the resolved 3D disc when pointFade == 0.
            const float screenDiscRadius = angularRadius * static_cast<float>(height) * 1.2f;
            const float dotRadius = ((b == sun) ? 7.0f : (2.0f + 3.8f * prominence)) * (0.4f + 0.6f * phaseFactor);
            m.radius     = (pointFade > 0.01f) ? dotRadius : std::max(4.0f, screenDiscRadius);
            m.color      = (b == sun) ? glm::vec3(1.0f, 0.92f, 0.65f)
                                      : (b->getTexture() ? b->getTexture()->tint : glm::vec3(0.9f));
            out.push_back(std::move(m));
        }

        std::sort(out.begin(), out.end(), [](const BodyMarker& a, const BodyMarker& b) {
            return a.prominence > b.prominence;
        });
    }

private:
    // True-scale solar corona around the Sun (no artificial angular inflation).
    void drawSunGlow(const UniformLocations& locs, const glm::vec3& cameraPosition) {
        if (!sun) return;

        const glm::vec3 sunPos = sun->getWorldPosition();
        const float dist = glm::length(sunPos - cameraPosition);
        const float sunRadius = sun->getScale();
        if (dist <= sunRadius) return;

        const float glowRadius = sunRadius * 1.45f;
        const glm::mat4 glowModel = glm::translate(glm::mat4(1.0f), sunPos)
                                  * glm::scale(glm::mat4(1.0f), glm::vec3(glowRadius));
        const glm::mat3 identityNormal(1.0f);

        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glUniformMatrix4fv(locs.model, 1, GL_FALSE, &glowModel[0][0]);
        glUniformMatrix3fv(locs.normalMatrix, 1, GL_FALSE, &identityNormal[0][0]);
        glUniform1i(locs.bodyType, static_cast<int>(BodyType::SunGlow));
        glUniform1f(locs.parentRadius, 0.0f);
        sphereGeometry.draw();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }

    void linkBody(CelestialBody* body, const Astro::BodyData& data, bool shapeModel) {
        static const std::pair<const Astro::BodyData*, Ephemeris::Item> deBodies[] = {
            {&Astro::Mercury, Ephemeris::Mercury}, {&Astro::Venus, Ephemeris::Venus},
            {&Astro::Earth, Ephemeris::EarthMoonBarycentre}, {&Astro::Moon, Ephemeris::MoonGeocentric},
            {&Astro::Mars, Ephemeris::Mars}, {&Astro::Jupiter, Ephemeris::Jupiter},
            {&Astro::Saturn, Ephemeris::Saturn}, {&Astro::Uranus, Ephemeris::Uranus},
            {&Astro::Neptune, Ephemeris::Neptune}, {&Astro::Pluto, Ephemeris::Pluto}};

        BodyLink link{body, -1, nullptr, shapeModel ? Astro::shapeModelMeshToBody() : Astro::sphereMeshToBody(), nullptr, nullptr};
        for (const auto& [d, item] : deBodies) {
            if (d == &data) link.deItem = item;
        }
        for (const Astro::IauBody& iau : Astro::iauBodies()) {
            if (std::string(iau.name) == data.name) link.iau = &iau;
        }
        for (const Astro::SatelliteOrbit& orbit : Astro::satelliteOrbits()) {
            if (std::string(orbit.name) == data.name) {
                link.moon = &orbit;
                link.moonTable = std::make_shared<MoonTable>(std::string("resources/moons/") + data.name + ".bin");
            }
        }
        if (link.iau != nullptr && link.iau->radii[0] > 0.0 && !shapeModel) {
            const double* r = link.iau->radii;   // body x, y, z -> mesh x, z, y
            body->setRadii(glm::vec3(glm::dvec3(r[0], r[2], r[1])));
        }
        links.push_back(link);
    }

    // Positions from DE440 and orientations from the IAU models at the current clock.
    void applyEphemeris() {
        const double jdUt  = Astro::unixToJulianDate(simTimeSeconds);   // UTC stands in for UT1 (< 1 s)
        const double jdTdb = Astro::unixToJulianDateTdb(simTimeSeconds);
        const double T = (jdTdb - Astro::J2000_EPOCH_JD) / Astro::DAYS_PER_JULIAN_CENTURY;
        const glm::dmat3 toWorld = Astro::icrfToWorld();

        for (const BodyLink& link : links) {
            if (link.deItem == Ephemeris::MoonGeocentric) {
                const Ephemeris::State m = ephemeris.moonForShadows(jdTdb);
                link.body->setEphemerisOffset(toWorld * m.position,
                    Astro::osculatingBasis(toWorld * m.position, toWorld * m.velocity / Astro::SECONDS_PER_DAY,
                                           ephemeris.gravitationalParameter(3)));
            } else if (link.moon != nullptr) {
                // shortcut: DE440 places Pluto (and each giant) at its system barycentre,
                // 2,100 km from Pluto's centre; the giants' offsets are under 300 km.
                Astro::KeplerianBasis b = Astro::satelliteBasis(*link.moon, jdTdb - Astro::J2000_EPOCH_JD);
                b.P_world = toWorld * b.P_world;
                b.Q_world = toWorld * b.Q_world;
                // The line keeps the mean ellipse; the body gets the full JPL position.
                link.body->setEphemerisOffset(Astro::evaluateOrbitVertex(b, b.E_rad) + toWorld * link.moonTable->offset(jdTdb), b);
            } else if (link.deItem >= 0) {
                const auto item = static_cast<Ephemeris::Item>(link.deItem);
                const Ephemeris::State p = (item == Ephemeris::EarthMoonBarycentre)
                    ? ephemeris.earthForShadows(jdTdb) : ephemeris.heliocentric(item, jdTdb);
                if (item >= Ephemeris::Saturn) {
                    // The Sun swings round the barycentre under Jupiter's pull, so heliocentric
                    // ellipses of the slow outer planets wobble; draw them about the barycentre.
                    const Ephemeris::State bary = ephemeris.evaluate(item, jdTdb);
                    double mu = 0.0;
                    for (int i = 0; i < 10; ++i) mu += ephemeris.gravitationalParameter(i);
                    Astro::KeplerianBasis b = Astro::osculatingBasis(toWorld * bary.position,
                                                                     toWorld * bary.velocity / Astro::SECONDS_PER_DAY, mu);
                    b.focus_world = toWorld * (p.position - bary.position);   // the barycentre, from the Sun
                    link.body->setEphemerisOffset(toWorld * p.position, b);
                } else {
                    const double mu = ephemeris.gravitationalParameter(0) + ephemeris.gravitationalParameter(link.deItem + 1);
                    link.body->setEphemerisOffset(toWorld * p.position,
                        Astro::osculatingBasis(toWorld * p.position, toWorld * p.velocity / Astro::SECONDS_PER_DAY, mu));
                }
            }

            if (link.body->getData() == &Astro::Earth) {
                const glm::dvec3 nut = ephemeris.evaluate(Ephemeris::Nutation, jdTdb).position;
                link.body->setPole(toWorld * Astro::earthToIcrf(jdUt, jdTdb, nut.x, nut.y) * link.meshToBody,
                                   toWorld * Astro::earthToIcrf(jdUt, jdTdb, nut.x, nut.y, false) * link.meshToBody);
            } else if (link.body->getData() == &Astro::Moon) {
                const glm::dmat3 frame = toWorld * Astro::moonToIcrf(ephemeris.evaluate(Ephemeris::Librations, jdTdb).position) * link.meshToBody;
                link.body->setPole(frame, frame);
            } else if (link.iau != nullptr) {
                const Astro::IauBody& b = *link.iau;
                double ra = b.ra[0] + T * (b.ra[1] + T * b.ra[2]);
                double dec = b.dec[0] + T * (b.dec[1] + T * b.dec[2]);
                const double d = jdTdb - Astro::J2000_EPOCH_JD;
                double w = b.pm[0] + d * (b.pm[1] + d * b.pm[2]);
                for (const Astro::IauTerm& term : b.terms) {
                    const double arg = glm::radians(term.a0 + T * (term.a1 + T * term.a2));
                    ra += term.ra * std::sin(arg);
                    dec += term.dec * std::cos(arg);
                    w += term.pm * std::sin(arg);
                }
                ra = glm::radians(ra); dec = glm::radians(dec);
                w = glm::radians(std::fmod(w, 360.0));
                link.body->setPole(toWorld * Astro::poleToIcrf(ra, dec, w) * link.meshToBody,
                                   toWorld * Astro::poleToIcrf(ra, dec, 0.0) * link.meshToBody);
            }
        }
    }

    Texture* loadTexture(const std::string& path) {
        auto texture = std::make_unique<Texture>(path);
        Texture* raw = texture.get();
        textures.push_back(std::move(texture));
        return raw;
    }

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
        raw->setOrbitAngle(static_cast<double>(bodies.size()) * 2.39996322972865332);
        linkBody(raw, data, geometry != nullptr && geometry != &sphereGeometry);
        bodies.push_back(std::move(body));
        return raw;
    }

    void buildSystem() {
        // --- THE CENTRE ---
        sun = createBody(Astro::Sun, nullptr);
        sun->setType(BodyType::Sun);
        sun->setTiltChildren(false); // planetary orbits lie in the ecliptic, not the Sun's equator

        // --- BACKGROUND STAR SPHERE ---
        skySphere = std::make_unique<CelestialBody>("Stars", loadTexture("resources/Stars.jpg"),
                                                    &sphereGeometry, 0.0f, 0.0f, 0.0f, 0.0f, 2.5e10f);
        skySphere->setType(BodyType::Sky);

        // --- INNER PLANETS AND ASTEROID ---
        createBody(Astro::Mercury, sun);
        createBody(Astro::Venus, sun);

        CelestialBody* earth = createBody(Astro::Earth, sun);
        earth->setTiltChildren(false); // the Moon follows the ecliptic, not Earth's equator
        createBody(Astro::Moon, earth);

        createBody(Astro::Eros, sun, loadShape("resources/models/Eros.obj"));

        CelestialBody* mars = createBody(Astro::Mars, sun);
        createBody(Astro::Phobos, mars, loadShape("resources/models/Phobos.tab"));
        createBody(Astro::Deimos, mars, loadShape("resources/models/Deimos.tab"));

        // --- ASTEROIDS & DWARF PLANET CERES ---
        createBody(Astro::Gaspra, sun, loadShape("resources/models/Gaspra.tab"));
        createBody(Astro::Vesta, sun, loadShape("resources/models/Vesta.tab"));
        createBody(Astro::Ceres, sun);
        createBody(Astro::Pallas, sun, loadShape("resources/models/Pallas.obj"));
        createBody(Astro::Ida, sun, loadShape("resources/models/Ida.tab"));

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
        createBody(Astro::Mimas, saturn);
        createBody(Astro::Enceladus, saturn);
        createBody(Astro::Dione, saturn);
        createBody(Astro::Rhea, saturn);
        createBody(Astro::Titan, saturn);
        createBody(Astro::Iapetus, saturn);

        // --- SATURN'S RINGS ---
        auto rings = std::make_unique<CelestialBody>(
            "Saturn's Rings", loadTexture("resources/SaturnRings.png"), &ringGeometry,
            0.0f, 0.0f, 0.0f, 0.0f, ringOuterRadiusKm);
        rings->setType(BodyType::Ring);
        rings->setNavigable(false);
        saturnRings = rings.get();
        saturn->addSatellite(rings.get());
        bodies.push_back(std::move(rings));

        // --- MOONS OF URANUS ---
        createBody(Astro::Miranda, uranus);
        createBody(Astro::Ariel, uranus);
        createBody(Astro::Umbriel, uranus);
        createBody(Astro::Titania, uranus);
        createBody(Astro::Oberon, uranus);

        // --- MOON OF NEPTUNE ---
        createBody(Astro::Triton, neptune);

        // --- OUTER DWARF PLANETS AND MOONS ---
        CelestialBody* pluto = createBody(Astro::Pluto, sun);
        createBody(Astro::Charon, pluto);

        createBody(Astro::Haumea, sun);
        createBody(Astro::Makemake, sun);
        createBody(Astro::Eris, sun);
    }

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
                recenterOnTarget();
                return;
            }
        }
    }
};
