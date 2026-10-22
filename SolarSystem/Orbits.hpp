#pragma once
#include "AstronomicalData.hpp"
#include "Ephemeris.hpp"
#include <algorithm>
#include <iterator>
#include <string>

// The orbits the app draws and the HUD reports, from JPL positions. No OpenGL here, so
// tests/test_time_picker.cpp checks these very functions. ICRF axes, km, TDB Julian days.
namespace Orbits {

    // An orbit line's opacity, 0-1: an orbit much wider than the camera's distance from
    // the body in view crosses the screen as a straight line, so it fades out between
    // 30 and 300 times that distance (Earth's orbit from 60,000 km, the Moon's stays).
    inline float orbitFade(double viewKm, double orbitKm) {
        return static_cast<float>(glm::smoothstep(0.003, 0.03, viewKm / orbitKm));
    }

    // Asteroids, dwarf planets, comets, the Kuiper belt: O shows their orbits, labels and
    // the main belt only on its second press. Pluto stays with the planets.
    inline bool isMinorBody(const std::string& name, bool orbitsTheSun) {
        static const char* const major[] = {"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn",
                                            "Uranus", "Neptune", "Pluto"};
        return orbitsTheSun && std::find_if(std::begin(major), std::end(major),
                                            [&](const char* m) { return name == m; }) == std::end(major);
    }

    // A fitted moon's full JPL position: mean orbit (tools/satellite_fit.py) + correction table.
    inline glm::dvec3 moonPosition(const Astro::SatelliteOrbit& s, const MoonTable& table, double jdTdb) {
        const Astro::KeplerianBasis b = Astro::satelliteBasis(s, jdTdb - Astro::J2000_EPOCH_JD);
        return Astro::evaluateOrbitVertex(b, b.E_rad) + table.offset(jdTdb);
    }

    // A moon's orbit line: the osculating ellipse of its full position, so it runs through
    // the moon (the mean ellipse missed Hyperion, pulled about by Titan, by up to a quarter
    // of its orbit). mu = n^2 a^3 of the fit keeps the line at the mean size where the
    // planet's flattening speeds the inner moons up.
    inline Astro::KeplerianBasis moonLine(const Astro::SatelliteOrbit& s, const MoonTable& table, double jdTdb) {
        const double dt = 1.0e-3;   // days, a 300th of Phobos's orbit
        const glm::dvec3 v = (moonPosition(s, table, jdTdb + dt) - moonPosition(s, table, jdTdb - dt))
                           / (2.0 * dt * Astro::SECONDS_PER_DAY);
        const double n = s.meanMotion / Astro::SECONDS_PER_DAY, a = std::abs(s.a);
        return Astro::osculatingBasis(moonPosition(s, table, jdTdb), v, n * n * a * a * a);
    }

    // Hyperion and Nereid, pulled off any ellipse by Titan and the Sun (fit errors ~16 deg
    // against under 6 deg for the rest: Janus and Epimetheus swapping orbits, Prometheus and
    // Pandora's chaotic drift), osculating ones too: Hyperion's says 22.0 d for 21.3. Their
    // HUD values are the time one turn takes on the real path round now, and half its
    // nearest plus farthest distance.
    inline bool offAnyEllipse(const Astro::SatelliteOrbit& s) { return s.fitErrorDeg > 10.0; }

    inline Astro::KeplerianBasis measuredTurn(const Astro::SatelliteOrbit& s, const MoonTable& table, double jdTdb) {
        const double turn = glm::two_pi<double>() / s.meanMotion;
        const int steps = 64;
        double swept = 0.0, nearest = 1e30, farthest = 0.0;
        glm::dvec3 prev = moonPosition(s, table, jdTdb - 0.5 * turn);
        for (int k = 1; k <= steps; ++k) {
            const glm::dvec3 p = moonPosition(s, table, jdTdb + (k / double(steps) - 0.5) * turn);
            swept += std::acos(std::clamp(glm::dot(glm::normalize(prev), glm::normalize(p)), -1.0, 1.0));
            nearest = std::min(nearest, glm::length(p));
            farthest = std::max(farthest, glm::length(p));
            prev = p;
        }
        Astro::KeplerianBasis b{};
        b.a_km = 0.5 * (nearest + farthest);
        b.periodDays = turn * glm::two_pi<double>() / swept;
        return b;
    }

    // An asteroid, dwarf planet or comet (barycentric table): position from the Sun, and
    // its orbit line osculating about the Sun near it and about the barycentre (with the
    // planets' mass) far out, blended between 3 and 10 AU. There the Sun's 12-year wobble
    // under Jupiter made Halley's heliocentric ellipse breathe, perihelion 0.57-0.60 AU.
    // focus_world is the blended centre seen from the Sun.
    inline Astro::KeplerianBasis smallBodyLine(const Ephemeris& eph, const MoonTable& table, double jdTdb, glm::dvec3* fromSun = nullptr) {
        const auto sunAt = [&](double jd) { return eph.evaluate(Ephemeris::Sun, jd).position; };
        const glm::dvec3 p = table.offset(jdTdb) - sunAt(jdTdb);
        if (fromSun) *fromSun = p;
        const double w = glm::smoothstep(3.0, 10.0, glm::length(p) / Astro::AU_KM);
        const auto rel = [&](double jd) { return table.offset(jd) - (1.0 - w) * sunAt(jd); };
        double planets = 0.0;
        for (int i = 1; i < 10; ++i) planets += eph.gravitationalParameter(i);
        Astro::KeplerianBasis b = Astro::osculatingBasis(rel(jdTdb), (rel(jdTdb + 0.5) - rel(jdTdb - 0.5)) / Astro::SECONDS_PER_DAY,
                                                         eph.gravitationalParameter(0) + w * planets);
        b.focus_world = -w * sunAt(jdTdb);
        return b;
    }

    // DE440's Pluto is the Pluto-Charon barycentre, 2,100 km from Pluto: Charon (PLU060 GM
    // 106.10 km^3/s^2) pulls it that way. Pluto's centre is the barycentre minus this.
    inline glm::dvec3 plutoBarycentreOffset(const glm::dvec3& charon, const Ephemeris& eph) {
        return charon * (106.10 / eph.gravitationalParameter(9));
    }

    // Axes of a basis turned from ICRF to the world frame.
    inline Astro::KeplerianBasis rotated(Astro::KeplerianBasis b, const glm::dmat3& toWorld) {
        b.P_world = toWorld * b.P_world;
        b.Q_world = toWorld * b.Q_world;
        b.focus_world = toWorld * b.focus_world;
        return b;
    }
}
