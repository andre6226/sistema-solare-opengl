#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// JPL DE440 Chebyshev ephemeris, cut to 1900-2200 by tools/de440_extract.py.
// Positions in km and velocities in km/day, ICRF equatorial axes, time in TDB Julian days.
class Ephemeris {
public:
    // Item order of the DE files.
    enum Item { Mercury, Venus, EarthMoonBarycentre, Mars, Jupiter, Saturn, Uranus, Neptune, Pluto,
                MoonGeocentric, Sun, Nutation, Librations };

    struct State { glm::dvec3 position{0.0}; glm::dvec3 velocity{0.0}; };

    explicit Ephemeris(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open " + path);
        char magic[8];
        in.read(magic, 8);
        if (std::memcmp(magic, "DE440BIN", 8) != 0) throw std::runtime_error(path + ": not a DE440BIN file");

        double interval = 0.0;
        int32_t count = 0, width = 0, layout[39];
        double constants[12];
        in.read(reinterpret_cast<char*>(&jdStart), 8);
        in.read(reinterpret_cast<char*>(&jdEnd), 8);
        in.read(reinterpret_cast<char*>(&interval), 8);
        in.read(reinterpret_cast<char*>(&count), 4);
        in.read(reinterpret_cast<char*>(&width), 4);
        in.read(reinterpret_cast<char*>(layout), sizeof(layout));
        in.read(reinterpret_cast<char*>(constants), sizeof(constants));
        if (!in || interval != recordDays || count <= 0) throw std::runtime_error(path + ": bad header");

        recordWidth = width;
        for (int i = 0; i < 13; ++i) {
            items[i] = {layout[3 * i] - 3, layout[3 * i + 1], layout[3 * i + 2]};  // 1-based, after 2 date words
        }
        earthMoonMassRatio = constants[0];
        for (int i = 0; i < 10; ++i) gm[i] = constants[2 + i];

        coefficients.resize(static_cast<size_t>(count) * width);
        in.read(reinterpret_cast<char*>(coefficients.data()), coefficients.size() * sizeof(double));
        if (!in) throw std::runtime_error(path + ": truncated");
    }

    double firstJd() const { return jdStart; }
    double lastJd()  const { return jdEnd; }

    // GM in km^3/s^2: index 0 = Sun, 1..9 = Mercury..Pluto with 3 = Earth+Moon.
    double gravitationalParameter(int index) const { return gm[index]; }
    double earthMoonRatio() const { return earthMoonMassRatio; }

    // Raw item: planets and Sun from the solar-system barycentre, Moon from Earth,
    // Nutation = (dpsi, deps), Librations = Euler angles (phi, theta, psi), radians.
    State evaluate(Item item, double jdTdb) const {
        jdTdb = std::fmin(std::fmax(jdTdb, jdStart), jdEnd);
        const size_t record = std::min(static_cast<size_t>((jdTdb - jdStart) / recordDays),
                                       coefficients.size() / recordWidth - 1);
        const double recordStart = jdStart + record * recordDays;

        const Layout& l = items[item];
        const double span = recordDays / l.subintervals;
        const int sub = std::min(static_cast<int>((jdTdb - recordStart) / span), l.subintervals - 1);
        const double t = 2.0 * (jdTdb - recordStart - sub * span) / span - 1.0;
        const int components = (item == Nutation) ? 2 : 3;
        const double* c = &coefficients[record * recordWidth + l.offset + sub * components * l.count];

        State s;
        for (int k = 0; k < components; ++k, c += l.count) {
            // Chebyshev T_n(t) and its derivative by recurrence.
            double tPrev = 1.0, tCur = t, dPrev = 0.0, dCur = 1.0;
            double p = c[0] + c[1] * t, v = c[1];
            for (int n = 2; n < l.count; ++n) {
                const double tNext = 2.0 * t * tCur - tPrev;
                const double dNext = 2.0 * tCur + 2.0 * t * dCur - dPrev;
                p += c[n] * tNext;
                v += c[n] * dNext;
                tPrev = tCur; tCur = tNext; dPrev = dCur; dCur = dNext;
            }
            s.position[k] = p;
            s.velocity[k] = v * 2.0 / span;
        }
        return s;
    }

    // Heliocentric state of a planet; Earth itself (not the barycentre) for EarthMoonBarycentre.
    State heliocentric(Item planet, double jdTdb) const {
        State s = evaluate(planet, jdTdb);
        if (planet == EarthMoonBarycentre) {
            const State moon = evaluate(MoonGeocentric, jdTdb);
            s.position -= moon.position / (1.0 + earthMoonMassRatio);
            s.velocity -= moon.velocity / (1.0 + earthMoonMassRatio);
        }
        const State sun = evaluate(Sun, jdTdb);
        return {s.position - sun.position, s.velocity - sun.velocity};
    }

    // Shadow geometry: Earth one solar light time back along its path (20" of
    // aberration, so the Sun sits where Earth sees it) and the Moon one lunar light
    // time back. Without them eclipses come 33 s late against the NASA catalogue.
    State earthForShadows(double jdTdb) const {
        State e = heliocentric(EarthMoonBarycentre, jdTdb);
        e.position -= e.velocity * (glm::length(e.position) / lightKmPerDay);
        return e;
    }
    State moonForShadows(double jdTdb) const {
        const double lightTime = glm::length(evaluate(MoonGeocentric, jdTdb).position) / lightKmPerDay;
        return evaluate(MoonGeocentric, jdTdb - lightTime);
    }

private:
    static constexpr double lightKmPerDay = 299792.458 * 86400.0;
    static constexpr double recordDays = 32.0;
    struct Layout { int offset = 0, count = 0, subintervals = 1; };
    Layout items[13];
    std::vector<double> coefficients;
    double jdStart = 0.0, jdEnd = 0.0, earthMoonMassRatio = 0.0, gm[10]{};
    size_t recordWidth = 0;
};

// A moon's offset from its mean-element orbit (tools/satellite_tables.py), km, ICRF,
// so that orbit + offset matches the JPL satellite ephemerides to under 1 km.
class MoonTable {
public:
    explicit MoonTable(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open " + path);
        char magic[8];
        int32_t count = 0;
        in.read(magic, 8);
        in.read(reinterpret_cast<char*>(&jdStart), 8);
        in.read(reinterpret_cast<char*>(&segmentDays), 8);
        in.read(reinterpret_cast<char*>(&count), 4);
        in.read(reinterpret_cast<char*>(&order), 4);
        if (!in || std::memcmp(magic, "MOONCHEB", 8) != 0 || count <= 0 || order < 2 || segmentDays <= 0.0) {
            throw std::runtime_error(path + ": not a MOONCHEB table");
        }
        coefficients.resize(static_cast<size_t>(count) * 3 * order);
        in.read(reinterpret_cast<char*>(coefficients.data()), coefficients.size() * sizeof(float));
        if (!in) throw std::runtime_error(path + ": truncated");
    }

    glm::dvec3 offset(double jdTdb) const {
        const size_t count = coefficients.size() / (3 * order);
        const double u = std::fmin(std::fmax((jdTdb - jdStart) / segmentDays, 0.0), static_cast<double>(count) - 1e-9);
        const size_t segment = std::min(static_cast<size_t>(u), count - 1);
        const double t = 2.0 * (u - segment) - 1.0;
        const float* c = &coefficients[segment * 3 * order];
        glm::dvec3 result(0.0);
        for (int axis = 0; axis < 3; ++axis, c += order) {
            double tPrev = 1.0, tCur = t, sum = c[0] + c[1] * t;
            for (int n = 2; n < order; ++n) {
                const double tNext = 2.0 * t * tCur - tPrev;
                sum += c[n] * tNext;
                tPrev = tCur; tCur = tNext;
            }
            result[axis] = sum;
        }
        return result;
    }

private:
    std::vector<float> coefficients;
    double jdStart = 0.0, segmentDays = 0.0;
    int32_t order = 0;
};
