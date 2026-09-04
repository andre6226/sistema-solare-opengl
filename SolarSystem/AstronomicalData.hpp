#pragma once
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>

namespace Astro {

    // Real measurements. Nothing here is expressed in render units: the
    // conversion to a viewable scale is the ScaleConverter's job.
    struct BodyData {
        const char* name;
        double radiusKm;
        double distanceFromParentKm;
        double orbitalPeriodDays;
        double rotationPeriodDays;   // negative = retrograde rotation
        double axialTiltDegrees;
    };


    inline constexpr BodyData Sun       = {"Sun", 696340.0, 0.0, 0.0, 25.38, 7.25};
    inline constexpr BodyData Mercury   = {"Mercury", 2439.7, 57900000.0, 88.0, 58.6, 0.03};
    inline constexpr BodyData Venus     = {"Venus", 6051.8, 108200000.0, 224.7, -243.0, 177.36};
    inline constexpr BodyData Earth     = {"Earth", 6371.0, 149600000.0, 365.2, 1.0, 23.44};
    inline constexpr BodyData Mars      = {"Mars", 3389.5, 227900000.0, 687.0, 1.03, 25.19};
    inline constexpr BodyData Moon      = {"Moon", 1737.4, 384400.0, 27.3, 27.3, 6.68};
    inline constexpr BodyData Phobos    = {"Phobos", 11.2, 9376.0, 0.31, 0.31, 0.0};
    inline constexpr BodyData Deimos    = {"Deimos", 6.2, 23460.0, 1.26, 1.26, 0.0};
    inline constexpr BodyData Jupiter   = {"Jupiter", 69911.0, 778500000.0, 4331.0, 0.41, 3.13};
    inline constexpr BodyData Saturn    = {"Saturn", 58232.0, 1434000000.0, 10747.0, 0.45, 26.73};
    inline constexpr BodyData Uranus    = {"Uranus", 25362.0, 2871000000.0, 30589.0, -0.72, 97.77};
    inline constexpr BodyData Neptune   = {"Neptune", 24622.0, 4495000000.0, 59800.0, 0.67, 28.32};
    inline constexpr BodyData Io        = {"Io", 1821.6, 421700.0, 1.76, 1.76, 0.0};
    inline constexpr BodyData Europa    = {"Europa", 1560.8, 671100.0, 3.55, 3.55, 0.0};
    inline constexpr BodyData Ganymede  = {"Ganymede", 2634.1, 1070400.0, 7.15, 7.15, 0.0};
    inline constexpr BodyData Callisto  = {"Callisto", 2410.3, 1882700.0, 16.68, 16.68, 0.0};
    inline constexpr BodyData Titan     = {"Titan", 2574.7, 1221800.0, 15.94, 15.94, 0.0};
    inline constexpr BodyData Enceladus = {"Enceladus", 252.1, 237900.0, 1.37, 1.37, 0.0};
    inline constexpr BodyData Titania   = {"Titania", 788.4, 436300.0, 8.7, 8.7, 0.0};
    inline constexpr BodyData Triton    = {"Triton", 1353.4, 354800.0, -5.87, -5.87, 0.0};

    // Distance beyond which a body is treated as orbiting the Sun rather than a
    // planet. Moons and planets get different compression curves.
    inline constexpr double moonDistanceLimitKm = 3000000.0;

    // --- Sky orientation ---
    //
    // The star map is an all-sky panorama in GALACTIC coordinates: the Milky Way
    // runs dead flat along its centre line, which is only true in that frame.
    // Pasted straight onto the sky sphere it laid the galactic plane on top of
    // the planets' orbital plane, when the two are inclined about 60 degrees to
    // each other, so every constellation sat in the wrong place.
    //
    // IAU 1958 definition, J2000 equatorial positions.
    inline constexpr double northGalacticPoleRaDeg  = 192.85948;
    inline constexpr double northGalacticPoleDecDeg =  27.12825;
    inline constexpr double galacticCentreRaDeg     = 266.405;
    inline constexpr double galacticCentreDecDeg    = -28.936;
    inline constexpr double eclipticObliquityDeg    =  23.4392911;

    inline glm::vec3 equatorialDirection(double raDeg, double decDeg) {
        const double ra = glm::radians(raDeg), dec = glm::radians(decDeg);
        return glm::vec3(std::cos(dec) * std::cos(ra),
                         std::cos(dec) * std::sin(ra),
                         std::sin(dec));
    }

    // Maps a direction on the sky sphere's own surface to the galactic direction
    // the star map paints there.
    //
    // The panorama's layout was measured rather than assumed, by hunting for the
    // Large Magellanic Cloud - the brightest extended source well off the
    // galactic plane. Of the four possible layouts it is the only one that puts
    // a source at the LMC's catalogue position, 7x above the local background
    // and 10x better than any alternative: galactic longitude DECREASES to the
    // right, and latitude runs upside down, with the south galactic pole at the
    // top of the image.
    //
    // Composing that with the sphere's own parameterisation happens to give a
    // proper rotation, determinant +1, so it can live in the model matrix
    // without inverting the winding and needs no special case in the shader.
    inline glm::mat3 skyTextureToGalactic() {
        return glm::mat3(-1.0f,  0.0f,  0.0f,   // columns
                          0.0f,  0.0f, -1.0f,
                          0.0f, -1.0f,  0.0f);
    }

    // Takes a direction given in the galactic frame into this program's world
    // frame, where the planets orbit the XZ plane and +Y is the ecliptic north.
    inline glm::mat3 galacticToWorld() {
        // Build the galactic axes as seen from the equatorial frame. Using two
        // measured directions rather than Euler angles avoids every sign trap;
        // the published pair is not exactly orthogonal, so it is re-orthonormalised.
        const glm::vec3 zGal = glm::normalize(equatorialDirection(northGalacticPoleRaDeg,
                                                                  northGalacticPoleDecDeg));
        glm::vec3 xGal = glm::normalize(equatorialDirection(galacticCentreRaDeg,
                                                            galacticCentreDecDeg));
        xGal = glm::normalize(xGal - zGal * glm::dot(zGal, xGal));
        const glm::vec3 yGal = glm::cross(zGal, xGal);

        // Columns carry galactic components into the equatorial frame.
        const glm::mat3 galacticToEquatorial(xGal, yGal, zGal);

        // Equatorial to ecliptic: a rotation about the vernal equinox by the
        // obliquity.
        const float e = glm::radians(static_cast<float>(eclipticObliquityDeg));
        const glm::mat3 equatorialToEcliptic(1.0f,  0.0f,          0.0f,
                                             0.0f,  std::cos(e), -std::sin(e),
                                             0.0f,  std::sin(e),  std::cos(e));

        // Ecliptic to world: the ecliptic pole becomes +Y, the orbital plane XZ.
        const glm::mat3 eclipticToWorld(1.0f, 0.0f,  0.0f,
                                        0.0f, 0.0f, -1.0f,
                                        0.0f, 1.0f,  0.0f);

        return eclipticToWorld * equatorialToEcliptic * galacticToEquatorial;
    }

// Turns real measurements into render units. The whole point is compression:
// at true scale the planets would be invisible dots separated by empty space,
// so distances and radii go through fractional powers that preserve the
// ordering while pulling the extremes together.
class ScaleConverter {
private:
    float earthDistanceUnit = 45.0f;
    float earthRadiusUnit   = 0.4f;
    float baseSpeed         = 1.1f;

    static bool isMoon(const BodyData& body) {
        return body.distanceFromParentKm > 0.0 &&
               body.distanceFromParentKm < moonDistanceLimitKm;
    }

public:
    float getOrbitRadius(const BodyData& body) const {
        if (body.distanceFromParentKm <= 0.0) return 0.0f;

        if (!isMoon(body)) {
            const double normalized = body.distanceFromParentKm / Earth.distanceFromParentKm;
            return (float)(std::pow(normalized, 0.42) * earthDistanceUnit);
        }

        const double normalized = body.distanceFromParentKm / Moon.distanceFromParentKm;

        // - The 1.2 offset guarantees no moon ends up inside its parent planet.
        // - The 0.6 exponent and the 2.0 multiplier fan Jupiter's moons apart.
        return 1.2f + (float)std::pow(normalized, 0.6) * 2.0f;
    }

    float getScale(const BodyData& body) const {
        const double normalized = body.radiusKm / Earth.radiusKm;
        float scale = (float)(std::pow(normalized, 0.38) * earthRadiusUnit);

        if (body.radiusKm > 600000.0) {
            scale *= 1.4f; // the Sun, given extra presence
        } else if (isMoon(body)) {
            scale *= 0.3f; // moons, shrunk so they read as satellites
        }

        return scale;
    }

    float getOrbitalSpeed(const BodyData& body) const {
        if (body.orbitalPeriodDays == 0.0) return 0.0f;
        float speed = (float)(baseSpeed / body.orbitalPeriodDays);

        if (isMoon(body)) speed *= 0.3f;

        return std::clamp(speed, -10.0f, 10.0f);
    }

    float getRotationSpeed(const BodyData& body) const {
        if (body.rotationPeriodDays == 0.0) return 0.0f;
        float speed = (float)(baseSpeed / body.rotationPeriodDays);

        if (isMoon(body)) speed *= 0.3f;

        return std::clamp(speed, -10.0f, 10.0f);
    }

    float getAxialTilt(const BodyData& body) const {
        return glm::radians((float)body.axialTiltDegrees);
    }
};
}
