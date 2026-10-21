#pragma once
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <cassert>
#include <ctime>
#include <string>
#include "SatelliteOrbits.hpp"

namespace Astro {

    // Fundamental astronomical and temporal constants
    inline constexpr double AU_KM                      = 149597870.7;    // 1 AU in km (1:1 scale)
    inline constexpr double J2000_EPOCH_JD             = 2451545.0;      // 2000-01-01 12:00:00 TT
    inline constexpr double UNIX_EPOCH_JD              = 2440587.5;      // 1970-01-01 00:00:00 UTC
    inline constexpr double SECONDS_PER_DAY            = 86400.0;
    inline constexpr double DAYS_PER_JULIAN_CENTURY    = 36525.0;
    inline constexpr double SECONDS_PER_JULIAN_CENTURY = 3155760000.0;   // 36525.0 * 86400.0
    inline constexpr double UNIX_TO_J2000_SECONDS      = 946728000.0;    // (2451545.0 - 2440587.5) * 86400.0

    // Keplerian osculating orbital elements (NASA/JPL Standish & Horizons)
    struct KeplerianElements {
        double a0 = 0.0, aDot = 0.0;       // Semi-major axis: AU (heliocentric) or km (moons); rate per century
        double e0 = 0.0, eDot = 0.0;       // Eccentricity; rate per century
        double i0 = 0.0, iDot = 0.0;       // Inclination (degrees); rate per century
        double L0 = 0.0, LDot = 0.0;       // Mean longitude L = M + wBar (degrees); rate per century
        double wBar0 = 0.0, wBarDot = 0.0; // Longitude of perihelion wBar = w + Omega (degrees); rate per century
        double node0 = 0.0, nodeDot = 0.0; // Longitude of ascending node Omega (degrees); rate per century
        bool isDistanceKm = false;         // true for moons (km), false for heliocentric (AU)
    };

    // Real measurements. In true 1:1 scale, 1 render unit = 1 km.
    struct BodyData {
        const char* name;
        double radiusKm;
        double distanceFromParentKm;
        double orbitalPeriodDays;
        double rotationPeriodDays;   // negative = retrograde rotation
        double axialTiltDegrees;
        KeplerianElements orbit{};
    };

    // Converts UNIX timestamp (seconds since 1970-01-01 00:00:00 UTC) to Julian centuries from J2000.0
    inline double unixToJulianCenturies(double unixSeconds) {
        return (unixSeconds - UNIX_TO_J2000_SECONDS) / SECONDS_PER_JULIAN_CENTURY;
    }

    // Converts UNIX timestamp to Julian Date (JD)
    inline double unixToJulianDate(double unixSeconds) {
        return UNIX_EPOCH_JD + (unixSeconds / SECONDS_PER_DAY);
    }

    // Solves Kepler's equation: M = E - e*sin(E)
    // Quadratic Newton-Raphson convergence with tolerance |f(E)| < 1e-12
    inline double solveKepler(double M_rad, double e) {
        M_rad = std::fmod(M_rad, glm::two_pi<double>());
        if (M_rad > glm::pi<double>())  M_rad -= glm::two_pi<double>();
        if (M_rad < -glm::pi<double>()) M_rad += glm::two_pi<double>();

        double E;
        if (e < 0.8) {
            E = M_rad + e * std::sin(M_rad);
        } else {
            const double sinM = std::sin(M_rad);
            const double sgn = (sinM >= 0.0) ? 1.0 : -1.0;
            E = M_rad + sgn * 0.85 * e;
        }

        for (int iter = 0; iter < 15; ++iter) {
            const double f = E - e * std::sin(E) - M_rad;
            const double fPrime = 1.0 - e * std::cos(E);
            const double delta = f / fPrime;
            E -= delta;
            if (std::abs(f) < 1.0e-12) break;
        }

        assert(std::abs(E - e * std::sin(E) - M_rad) < 1.0e-11);
        return E;
    }

    // Orthonormal orbital basis in engine world coordinates
    struct KeplerianBasis {
        glm::dvec3 P_world{0.0}; // Unit vector pointing to periapsis in world frame
        glm::dvec3 Q_world{0.0}; // Unit vector at 90 degrees in orbital plane in world frame
        double a_km = 0.0;       // Semi-major axis in km
        double e = 0.0;          // Current eccentricity
        double M_rad = 0.0;      // Mean anomaly in radians
        double E_rad = 0.0;      // Eccentric anomaly in radians
        glm::dvec3 focus_world{0.0}; // Where the ellipse's focus sits relative to the parent
    };

    inline KeplerianBasis evaluateKeplerianBasis(const KeplerianElements& el, double T) {
        KeplerianBasis b{};
        if (el.a0 <= 0.0) return b;

        const double a = el.a0 + el.aDot * T;
        b.a_km = el.isDistanceKm ? a : (a * AU_KM);
        b.e = std::clamp(el.e0 + el.eDot * T, 0.0, 0.9999);

        const double i_rad    = glm::radians(el.i0 + el.iDot * T);
        const double L_rad    = glm::radians(el.L0 + el.LDot * T);
        const double wBar_rad = glm::radians(el.wBar0 + el.wBarDot * T);
        const double node_rad = glm::radians(el.node0 + el.nodeDot * T);

        const double w_rad = wBar_rad - node_rad; // Argument of perihelion
        b.M_rad = L_rad - wBar_rad;               // Mean anomaly
        b.E_rad = solveKepler(b.M_rad, b.e);      // Eccentric anomaly

        const double cosO = std::cos(node_rad), sinO = std::sin(node_rad);
        const double cosW = std::cos(w_rad),    sinW = std::sin(w_rad);
        const double cosi = std::cos(i_rad),    sini = std::sin(i_rad);

        // Gauss P and Q unit vectors in standard astronomical ecliptic frame
        const glm::dvec3 P_ecl(
            cosO * cosW - sinO * sinW * cosi,
            sinO * cosW + cosO * sinW * cosi,
            sinW * sini
        );
        const glm::dvec3 Q_ecl(
            -cosO * sinW - sinO * cosW * cosi,
            -sinO * sinW + cosO * cosW * cosi,
            cosW * sini
        );

        // Ecliptic to engine world frame mapping (+X -> +X, +Z -> +Y ecliptic north, +Y -> -Z)
        b.P_world = glm::dvec3(P_ecl.x, P_ecl.z, -P_ecl.y);
        b.Q_world = glm::dvec3(Q_ecl.x, Q_ecl.z, -Q_ecl.y);
        return b;
    }

    inline glm::dvec3 evaluateOrbitVertex(const KeplerianBasis& b, double E_rad) {
        const double cosE = std::cos(E_rad);
        const double sinE = std::sin(E_rad);
        const double xPrime = b.a_km * (cosE - b.e);
        const double yPrime = b.a_km * std::sqrt(std::max(0.0, 1.0 - b.e * b.e)) * sinE;
        return xPrime * b.P_world + yPrime * b.Q_world;
    }

    inline glm::dvec3 evaluateKeplerianPosition(const KeplerianElements& el, double T) {
        if (el.a0 <= 0.0) return glm::dvec3(0.0);
        const KeplerianBasis b = evaluateKeplerianBasis(el, T);
        return evaluateOrbitVertex(b, b.E_rad);
    }

    // TT - UT1 in seconds. Measured values (USNO, 1 January of each year) through 2026, then the
    // tidal-braking parabola (32 s/century^2, Morrison & Stephenson) started from
    // today's value and slope. The future part is a forecast: by 2100 it may be
    // off by a minute or more, by 2200 by several, and no model can do better.
    inline double deltaTSeconds(double unixUtc) {
        static constexpr double measured[] = {   // 1 January of 1900 .. 2026
            -2.70, -1.48, -0.08, 1.26, 2.59, 3.92, 5.20, 6.29, 7.68, 9.13,
            10.38, 11.64, 13.23, 14.69, 16.00, 17.19, 18.19, 19.13, 20.14, 20.86,
            21.41, 22.06, 22.51, 23.01, 23.46, 23.63, 23.95, 24.39, 24.34, 24.10,
            24.02, 23.98, 23.89, 23.93, 23.88, 23.91, 23.76, 23.91, 23.96, 24.04,
            24.35, 24.82, 25.30, 25.77, 26.27, 26.76, 27.27, 27.77, 28.25, 28.70,
            29.15, 29.57, 29.97, 30.36, 30.72, 31.07, 31.35, 31.68, 32.17, 32.67,
            33.15, 33.58, 33.99, 34.47, 35.03, 35.74, 36.55, 37.43, 38.29, 39.20,
            40.18, 41.17, 42.23, 43.37, 44.48, 45.48, 46.46, 47.52, 48.53, 49.59,
            50.54, 51.38, 52.17, 52.96, 53.79, 54.34, 54.87, 55.32, 55.82, 56.30,
            56.86, 57.57, 58.31, 59.12, 59.98, 60.79, 61.63, 62.30, 62.97, 63.47,
            63.83, 64.09, 64.30, 64.47, 64.57, 64.69, 64.85, 65.15, 65.46, 65.78,
            66.07, 66.32, 66.60, 66.91, 67.28, 67.64, 68.10, 68.59, 68.97, 69.22,
            69.36, 69.36, 69.29, 69.20, 69.18, 69.14, 69.11};
        constexpr int last = static_cast<int>(sizeof(measured) / sizeof(measured[0])) - 1;
        const double year = 1970.0 + unixUtc / (365.2425 * SECONDS_PER_DAY);
        if (year <= 1900.0) return measured[0];
        if (year < 1900.0 + last) {
            const int i = static_cast<int>(year - 1900.0);
            return measured[i] + (measured[i + 1] - measured[i]) * (year - 1900.0 - i);
        }
        const double t = (year - 1900.0 - last) / 100.0;
        return measured[last] + 100.0 * (measured[last] - measured[last - 1]) * t + 32.0 * t * t;
    }

    // Julian date in TDB (= TT to 2 ms), the time argument of the ephemeris and the IAU models.
    inline double unixToJulianDateTdb(double unixUtc) {
        return unixToJulianDate(unixUtc) + deltaTSeconds(unixUtc) / SECONDS_PER_DAY;
    }

    // Active right-handed rotations, radians.
    inline glm::dmat3 rotationX(double a) { const double c = std::cos(a), s = std::sin(a); return glm::dmat3(1, 0, 0, 0, c, s, 0, -s, c); }
    inline glm::dmat3 rotationY(double a) { const double c = std::cos(a), s = std::sin(a); return glm::dmat3(c, 0, -s, 0, 1, 0, s, 0, c); }
    inline glm::dmat3 rotationZ(double a) { const double c = std::cos(a), s = std::sin(a); return glm::dmat3(c, s, 0, -s, c, 0, 0, 0, 1); }

    inline constexpr double ARCSEC = 3.14159265358979323846 / (180.0 * 3600.0);
    inline constexpr double J2000_OBLIQUITY_ARCSEC = 84381.406;   // IAU 2006

    // ICRF (equatorial J2000) to the engine world frame: J2000 ecliptic with
    // ecliptic north on +Y and the ecliptic y axis on -Z.
    inline glm::dmat3 icrfToWorld() {
        const glm::dmat3 eclipticToWorld(1, 0, 0, 0, 0, -1, 0, 1, 0);
        return eclipticToWorld * rotationX(-J2000_OBLIQUITY_ARCSEC * ARCSEC);
    }

    // Mesh axes to body-fixed axes (x at longitude 0, z north). Sphere.hpp puts
    // longitude 0 on -X and the pole on +Y; ShapeModel puts longitude 0 on +X.
    inline glm::dmat3 sphereMeshToBody()     { return glm::dmat3(-1, 0, 0, 0, 0, 1, 0, 1, 0); }
    inline glm::dmat3 shapeModelMeshToBody() { return glm::dmat3( 1, 0, 0, 0, 0, 1, 0, -1, 0); }

    // Body-fixed to ICRF for a pole at (ra, dec) and prime meridian angle w, radians (IAU convention).
    inline glm::dmat3 poleToIcrf(double ra, double dec, double w) {
        return rotationZ(glm::half_pi<double>() + ra) * rotationX(glm::half_pi<double>() - dec) * rotationZ(w);
    }

    // Earth's body-fixed (ITRS) axes in ICRF: IAU 2006 precession, the DE440 nutation
    // (dpsi, deps), and Greenwich sidereal time from the Earth Rotation Angle.
    // withSpin = false gives the true equator of date, which the oblate mesh needs without the spin.
    inline glm::dmat3 earthToIcrf(double jdUt1, double jdTt, double dpsi, double deps, bool withSpin = true) {
        const double t = (jdTt - J2000_EPOCH_JD) / DAYS_PER_JULIAN_CENTURY;
        const double zeta  = ( 2.650545 + t * (2306.083227 + t * (0.2988499 + t * 0.01801828))) * ARCSEC;
        const double z     = (-2.650545 + t * (2306.077181 + t * (1.0927348 + t * 0.01826837))) * ARCSEC;
        const double theta = (t * (2004.191903 - t * (0.4294934 + t * 0.04182264))) * ARCSEC;
        const double eps   = (J2000_OBLIQUITY_ARCSEC - t * (46.836769 + t * (0.0001831 - t * 0.00200340))) * ARCSEC;

        const double du  = jdUt1 - J2000_EPOCH_JD;
        const double era = glm::two_pi<double>() * std::fmod(0.7790572732640 + 1.00273781191135448 * du, 1.0);
        const double gmst = era + (0.014506 + t * (4612.156534 + t * 1.3915817)) * ARCSEC;
        const double gast = gmst + dpsi * std::cos(eps);

        // ICRF -> true equator and equinox of date. The textbook passive R3(a) is the active rotationZ(-a).
        const glm::dmat3 precession = rotationZ(z) * rotationY(-theta) * rotationZ(zeta);
        const glm::dmat3 nutation = rotationX(eps + deps) * rotationZ(dpsi) * rotationX(-eps);
        const glm::dmat3 toTrue = glm::transpose(nutation * precession);
        return withSpin ? toTrue * rotationZ(gast) : toTrue;
    }

    // DE440 lunar libration Euler angles (phi, theta, psi) to the Moon's principal-axis frame in ICRF.
    // shortcut: uses the principal-axis frame for the mean-Earth texture frame, ~80" (0.7 km) apart.
    inline glm::dmat3 moonToIcrf(const glm::dvec3& euler) {
        return rotationZ(euler.x) * rotationX(euler.y) * rotationZ(euler.z);
    }

    // Osculating ellipse from a state vector (km, km/s) around a centre with gravitational parameter mu (km^3/s^2).
    inline KeplerianBasis osculatingBasis(const glm::dvec3& r, const glm::dvec3& v, double mu) {
        KeplerianBasis b{};
        const double rLen = glm::length(r);
        const glm::dvec3 h = glm::cross(r, v);
        const glm::dvec3 eVec = glm::cross(v, h) / mu - r / rLen;
        b.a_km = 1.0 / (2.0 / rLen - glm::dot(v, v) / mu);
        b.e = std::min(glm::length(eVec), 0.9999);
        b.P_world = (b.e > 1.0e-9) ? eVec / glm::length(eVec) : r / rLen;
        b.Q_world = glm::normalize(glm::cross(h, b.P_world));
        b.E_rad = std::atan2(glm::dot(r, b.Q_world) / std::sqrt(1.0 - b.e * b.e), glm::dot(r, b.P_world) + b.a_km * b.e);
        b.M_rad = b.E_rad - b.e * std::sin(b.E_rad);
        return b;
    }

    // A fitted moon (tools/satellite_fit.py) at `days` from J2000 TDB: ellipse axes in ICRF, planetocentric.
    inline KeplerianBasis satelliteBasis(const SatelliteOrbit& s, double days) {
        double lambda = s.meanLongitude0 + days * (s.meanMotion + days * s.meanMotionDrift);
        for (const Libration& l : s.librations) lambda += l.amplitude * std::sin(l.phase + l.rate * days);
        const double varpi = s.periapsisLongitude0 + s.periapsisRate * days;
        const double node = s.node0 + s.nodeRate * days;
        const double w = varpi - node;

        const glm::dvec3 z(std::cos(s.poleDec) * std::cos(s.poleRa), std::cos(s.poleDec) * std::sin(s.poleRa), std::sin(s.poleDec));
        const glm::dvec3 x = glm::normalize(glm::cross(glm::dvec3(0.0, 0.0, 1.0), z));
        const glm::dmat3 laplace(x, glm::cross(z, x), z);

        const double cO = std::cos(node), sO = std::sin(node), cw = std::cos(w), sw = std::sin(w);
        const double ci = std::cos(s.inclination), si = std::sin(s.inclination);
        KeplerianBasis b{};
        b.P_world = laplace * glm::dvec3(cO * cw - sO * sw * ci, sO * cw + cO * sw * ci, sw * si);
        b.Q_world = laplace * glm::dvec3(-cO * sw - sO * cw * ci, -sO * sw + cO * cw * ci, cw * si);
        // The fit may land on a < 0, the same ellipse turned half a turn: flip it back so
        // every caller (the orbit lines skip a <= 0) sees a positive semi-major axis.
        b.a_km = std::abs(s.a);
        if (s.a < 0.0) { b.P_world = -b.P_world; b.Q_world = -b.Q_world; }
        b.e = s.e;
        b.M_rad = lambda - varpi;
        b.E_rad = solveKepler(b.M_rad, b.e);
        return b;
    }

    inline std::string formatUtcDateTime(double simUnixSeconds) {
        const std::time_t raw = static_cast<std::time_t>(std::floor(simUnixSeconds));
        std::tm tmVal;
#if defined(_WIN32)
        gmtime_s(&tmVal, &raw);
#else
        gmtime_r(&raw, &tmVal);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmVal);
        return std::string(buf);
    }

    inline int daysInMonth(int year, int month) {
        if (month == 2) {
            bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            return leap ? 29 : 28;
        }
        if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
        return 31;
    }

    inline void unixToUtcDateTimeParts(double simUnixSeconds, int& y, int& m, int& d, int& h, int& min, int& s) {
        const std::time_t raw = static_cast<std::time_t>(std::floor(simUnixSeconds));
        std::tm tmVal;
#if defined(_WIN32)
        gmtime_s(&tmVal, &raw);
#else
        gmtime_r(&raw, &tmVal);
#endif
        y = tmVal.tm_year + 1900;
        m = tmVal.tm_mon + 1;
        d = tmVal.tm_mday;
        h = tmVal.tm_hour;
        min = tmVal.tm_min;
        s = tmVal.tm_sec;
    }

    inline double utcDateTimeToUnix(int year, int month, int day, int hour, int minute, int second) {
        std::tm tmVal{};
        tmVal.tm_year = year - 1900;
        tmVal.tm_mon  = month - 1;
        tmVal.tm_mday = day;
        tmVal.tm_hour = hour;
        tmVal.tm_min  = minute;
        tmVal.tm_sec  = second;
        tmVal.tm_isdst = 0;
#if defined(_WIN32)
        return static_cast<double>(_mkgmtime(&tmVal));
#else
        return static_cast<double>(timegm(&tmVal));
#endif
    }

    inline void clampPickerValues(int& y, int& m, int& d, int& h, int& min, int& s) {
        if (y < 1900) y = 1900;   // the bundled DE440 extract covers 1900-2200
        if (y > 2200) y = 2200;
        if (m < 1) m = 1;
        if (m > 12) m = 12;
        const int maxDays = daysInMonth(y, m);
        if (d < 1) d = 1;
        if (d > maxDays) d = maxDays;
        if (h < 0) h = 0;
        if (h > 23) h = 23;
        if (min < 0) min = 0;
        if (min > 59) min = 59;
        if (s < 0) s = 0;
        if (s > 59) s = 59;
    }

    // =========================================================================
    // THE SUN (Origin, a = 0)
    // =========================================================================
    inline constexpr BodyData Sun = {
        "Sun", 696340.0, 0.0, 0.0, 25.38, 7.25,
        {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false}
    };

    // =========================================================================
    // 9 MAJOR PLANETS (NASA/JPL Standish 1800-2050 AD)
    // =========================================================================
    inline constexpr BodyData Mercury = {
        "Mercury", 2439.7, 57900000.0, 88.0, 58.6, 0.03,
        {0.38709927,  0.00000037, 0.20563593,  0.00001906,  7.00497902, -0.00594749,
         252.25032350, 149472.67411175, 77.45779628,  0.16047689, 48.33076593, -0.12534081, false}
    };

    inline constexpr BodyData Venus = {
        "Venus", 6051.8, 108200000.0, 224.7, -243.0, 177.36,
        {0.72333566,  0.00000390, 0.00677672, -0.00004107,  3.39467605, -0.00078890,
         181.97909950, 58517.81538729, 131.60246718,  0.00268329, 76.67984255, -0.27769418, false}
    };

    inline constexpr BodyData Earth = {
        "Earth", 6371.0, 149600000.0, 365.2, 0.99726968, 23.44,
        {1.00000261,  0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668,
         100.46457166, 35999.37244981, 102.93768193,  0.32327364,  0.00000000,  0.00000000, false}
    };

    inline constexpr BodyData Mars = {
        "Mars", 3389.5, 227900000.0, 687.0, 1.03, 25.19,
        {1.52371034,  0.00001847, 0.09339410,  0.00007882,  1.84969142, -0.00813131,
          -4.55343205, 19140.30268499, -23.94362959,  0.44441088, 49.55953891, -0.29257343, false}
    };

    inline constexpr BodyData Jupiter = {
        "Jupiter", 69911.0, 778500000.0, 4331.0, 0.41, 3.13,
        {5.20288700, -0.00011607, 0.04838624, -0.00013253,  1.30439695, -0.00183714,
          34.39644051, 3034.74612775, 14.72847983,  0.21252668, 100.47390909,  0.20469106, false}
    };

    inline constexpr BodyData Saturn = {
        "Saturn", 58232.0, 1434000000.0, 10747.0, 0.45, 26.73,
        {9.53667594, -0.00125060, 0.05386179, -0.00050991,  2.48599187,  0.00193609,
          49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794, false}
    };

    inline constexpr BodyData Uranus = {
        "Uranus", 25362.0, 2871000000.0, 30589.0, -0.72, 97.77,
        {19.18916464, -0.00196176, 0.04725744, -0.00004397,  0.77263783, -0.00242939,
         313.23810451,  428.48202785, 170.95427630,  0.40805281, 74.01692503,  0.04240589, false}
    };

    inline constexpr BodyData Neptune = {
        "Neptune", 24622.0, 4495000000.0, 59800.0, 0.67, 28.32,
        {30.06992276,  0.00026291, 0.00860610,  0.00005105,  1.77004347,  0.00035372,
         -55.12002969,  218.45945325, 44.96476224, -0.32241464, 131.78422574, -0.00508664, false}
    };

    inline constexpr BodyData Pluto = {
        "Pluto", 1188.3, 5906400000.0, 90560.0, -6.39, 122.53,
        {39.48211675, -0.00031596, 0.24882730,  0.00005170, 17.14001206,  0.00004818,
         238.92903833,  145.20780515, 224.06876938, -0.04062942, 110.30393608, -0.01183482, false}
    };

    // =========================================================================
    // 5 ASTEROIDS (NASA JPL Horizons)
    // =========================================================================
    inline constexpr BodyData Eros = {
        "Eros", 8.4, 217500000.0, 643.0, 0.22, 11.0,
        {1.4583, 0.0, 0.2227, 0.0, 10.828, 0.0, 78.400, 20449.46, 123.139, 0.0, 304.322, 0.0, false}
    };

    inline constexpr BodyData Gaspra = {
        "Gaspra", 6.1, 330600000.0, 1199.0, 0.29, 27.0,
        {2.2096, 0.0, 0.1739, 0.0, 4.102, 0.0, 165.870, 10966.64, 22.670, 0.0, 253.197, 0.0, false}
    };

    inline constexpr BodyData Vesta = {
        "Vesta", 262.7, 353400000.0, 1325.0, 0.22, 29.0,
        {2.3618, 0.0, 0.0887, 0.0, 7.140, 0.0, 275.400, 9923.77, 254.539, 0.0, 103.810, 0.0, false}
    };

    inline constexpr BodyData Pallas = {
        "Pallas", 256.0, 414800000.0, 1686.0, 0.33, 84.0,
        {2.7724, 0.0, 0.2313, 0.0, 34.841, 0.0, 182.780, 7800.12, 123.129, 0.0, 173.080, 0.0, false}
    };

    inline constexpr BodyData Ida = {
        "Ida", 15.7, 428000000.0, 1767.0, 0.19, 89.0,
        {2.8610, 0.0, 0.0461, 0.0, 1.138, 0.0, 158.320, 7441.43, 74.221, 0.0, 324.080, 0.0, false}
    };

    // =========================================================================
    // 4 DWARF PLANETS (NASA JPL Horizons)
    // =========================================================================
    inline constexpr BodyData Ceres = {
        "Ceres", 473.0, 413700000.0, 1682.0, 0.38, 4.0,
        {2.7675, 0.0, 0.0758, 0.0, 10.593, 0.0, 231.294, 7818.79, 153.924, 0.0, 80.327, 0.0, false}
    };

    inline constexpr BodyData Haumea = {
        "Haumea", 798.0, 6432000000.0, 103774.0, 0.16, 126.0,
        {43.132, 0.0, 0.1912, 0.0, 28.190, 0.0, 205.620, 126.71, 2.220, 0.0, 122.020, 0.0, false}
    };

    inline constexpr BodyData Makemake = {
        "Makemake", 715.0, 6850000000.0, 112897.0, 0.95, 29.0,
        {45.791, 0.0, 0.1590, 0.0, 28.960, 0.0, 179.950, 116.47, 14.450, 0.0, 79.620, 0.0, false}
    };

    inline constexpr BodyData Eris = {
        "Eris", 1163.0, 10125000000.0, 203830.0, 1.08, 78.0,
        {67.781, 0.0, 0.4407, 0.0, 44.040, 0.0, 30.310, 64.51, 187.510, 0.0, 35.870, 0.0, false}
    };

    // Comet 1P/Halley: nucleus 15 x 8 x 8 km (Giotto, 1986), perihelion 1986-02-09.
    inline constexpr BodyData Halley = {
        "Halley", 5.5, 2667900000.0, 27510.0, 2.2, 0.0,
        {17.834, 0.0, 0.96714, 0.0, 162.26, 0.0, 236.2, 477.97, 169.75, 0.0, 58.42, 0.0, false}
    };

    // =========================================================================
    // 20 MOONS (isDistanceKm = true, a in km, LDot in deg/century)
    // =========================================================================

    // Earth's Moon (J2000 Ecliptic reference frame: nodal regression & perigee advance)
    inline constexpr BodyData Moon = {
        "Moon", 1737.4, 384400.0, 27.3, 27.3, 6.68,
        {384400.0, 0.0, 0.05490, 0.0, 5.145, 0.0, 218.32, 481267.881, 83.35, 4069.014, 125.08, -1934.136, true}
    };

    // Moons of Mars
    inline constexpr BodyData Phobos = {
        "Phobos", 11.2, 9376.0, 0.31, 0.31, 0.0,
        {9376.0, 0.0, 0.0151, 0.0, 1.093, 0.0, 0.0, 41232361.2, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Deimos = {
        "Deimos", 6.2, 23460.0, 1.26, 1.26, 0.0,
        {23460.0, 0.0, 0.0002, 0.0, 0.930, 0.0, 0.0, 10410926.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Galilean Moons of Jupiter
    inline constexpr BodyData Io = {
        "Io", 1821.6, 421700.0, 1.76, 1.76, 0.0,
        {421700.0, 0.0, 0.0041, 0.0, 0.050, 0.0, 106.08, 7432593.0, 84.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Europa = {
        "Europa", 1560.8, 671100.0, 3.55, 3.55, 0.0,
        {671100.0, 0.0, 0.0090, 0.0, 0.470, 0.0, 175.73, 3702692.0, 219.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Ganymede = {
        "Ganymede", 2634.1, 1070400.0, 7.15, 7.15, 0.0,
        {1070400.0, 0.0, 0.0013, 0.0, 0.200, 0.0, 87.59, 1837838.0, 192.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Callisto = {
        "Callisto", 2410.3, 1882700.0, 16.68, 16.68, 0.0,
        {1882700.0, 0.0, 0.0074, 0.0, 0.280, 0.0, 289.41, 787884.0, 52.0, 0.0, 0.0, 0.0, true}
    };

    // Moons of Saturn
    inline constexpr BodyData Mimas = {
        "Mimas", 198.2, 185520.0, 0.94, 0.94, 0.0,
        {185520.0, 0.0, 0.0202, 0.0, 1.570, 0.0, 0.0, 13952670.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Enceladus = {
        "Enceladus", 252.1, 237900.0, 1.37, 1.37, 0.0,
        {237900.0, 0.0, 0.0047, 0.0, 0.010, 0.0, 0.0, 9596400.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Tethys = {
        "Tethys", 531.1, 294619.0, 1.89, 1.89, 0.0,
        {294619.0, 0.0, 0.0001, 0.0, 1.120, 0.0, 0.0, 6965240.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Dione = {
        "Dione", 561.4, 377400.0, 2.74, 2.74, 0.0,
        {377400.0, 0.0, 0.0022, 0.0, 0.020, 0.0, 0.0, 4804340.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Rhea = {
        "Rhea", 763.8, 527040.0, 4.52, 4.52, 0.0,
        {527040.0, 0.0, 0.00125, 0.0, 0.350, 0.0, 0.0, 2910680.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Titan = {
        "Titan", 2574.7, 1221800.0, 15.94, 15.94, 0.0,
        {1221800.0, 0.0, 0.0288, 0.0, 0.350, 0.0, 260.0, 824647.0, 180.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Iapetus = {
        "Iapetus", 734.5, 3561300.0, 79.33, 79.33, 0.0,
        {3561300.0, 0.0, 0.0286, 0.0, 15.470, 0.0, 0.0, 165750.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Hyperion tumbles chaotically: no rotation model exists, the period is the mean one.
    inline constexpr BodyData Hyperion = {
        "Hyperion", 135.0, 1481010.0, 21.28, 13.0, 0.0,
        {1481010.0, 0.0, 0.1230, 0.0, 0.430, 0.0, 0.0, 617919.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Moons of Uranus
    inline constexpr BodyData Miranda = {
        "Miranda", 235.8, 129390.0, 1.41, 1.41, 0.0,
        {129390.0, 0.0, 0.0013, 0.0, 4.340, 0.0, 0.0, 9305730.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Ariel = {
        "Ariel", 578.9, 191020.0, 2.52, 2.52, 0.0,
        {191020.0, 0.0, 0.0012, 0.0, 0.260, 0.0, 0.0, 5217850.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Umbriel = {
        "Umbriel", 584.7, 266000.0, 4.14, 4.14, 0.0,
        {266000.0, 0.0, 0.0039, 0.0, 0.200, 0.0, 0.0, 3173020.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Titania = {
        "Titania", 788.4, 436300.0, 8.7, 8.7, 0.0,
        {436300.0, 0.0, 0.0011, 0.0, 0.340, 0.0, 0.0, 1510330.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Oberon = {
        "Oberon", 761.4, 583520.0, 13.46, 13.46, 0.0,
        {583520.0, 0.0, 0.0014, 0.0, 0.060, 0.0, 0.0, 976670.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Moon of Neptune (Retrograde orbit: i = 156.88 deg > 90 deg sets retrograde plane; LDot > 0)
    inline constexpr BodyData Triton = {
        "Triton", 1353.4, 354800.0, 5.87, -5.87, 0.0,
        {354800.0, 0.0, 0.000016, 0.0, 156.880, 0.0, 0.0, 2237366.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    inline constexpr BodyData Proteus = {
        "Proteus", 210.0, 117647.0, 1.12, 1.12, 0.0,
        {117647.0, 0.0, 0.0005, 0.0, 0.075, 0.0, 0.0, 11716550.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Nereid: the most eccentric known moon (e 0.75), 1.4 to 9.7 million km from Neptune.
    inline constexpr BodyData Nereid = {
        "Nereid", 170.0, 5513818.0, 360.13, 0.48, 0.0,
        {5513818.0, 0.0, 0.7507, 0.0, 7.090, 0.0, 0.0, 36508.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Moon of Pluto
    inline constexpr BodyData Charon = {
        "Charon", 606.0, 19591.0, 6.39, 6.39, 0.0,
        {19591.0, 0.0, 0.0002, 0.0, 0.000, 0.0, 0.0, 2058400.0, 0.0, 0.0, 0.0, 0.0, true}
    };

    // Distance beyond which HUD formats orbit radius in AU rather than km.
    // Raised to 6M km so the outer moons Iapetus (3.56M km) and Nereid (5.51M km) read in km.
    inline constexpr double moonDistanceLimitKm = 6000000.0;

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

// True 1:1 linear scaling where 1 render unit = 1 km.
// Base angular speed is kept gentle (0.03 rad/day-unit at 1x) so fast rotators
// like asteroids and gas giants can be inspected up close without spinning wildly.
class ScaleConverter {
private:
    float scaleFactor = 1.0f;
    float baseSpeed   = 0.03f;

public:
    float getOrbitRadius(const BodyData& body) const {
        return static_cast<float>(body.distanceFromParentKm * scaleFactor);
    }

    float getScale(const BodyData& body) const {
        return static_cast<float>(body.radiusKm * scaleFactor);
    }

    float getOrbitalSpeed(const BodyData& body) const {
        if (body.orbitalPeriodDays == 0.0) return 0.0f;
        float speed = static_cast<float>(baseSpeed / body.orbitalPeriodDays);
        return std::clamp(speed, -2.0f, 2.0f);
    }

    float getRotationSpeed(const BodyData& body) const {
        if (body.rotationPeriodDays == 0.0) return 0.0f;
        const double period = (body.axialTiltDegrees > 90.0)
            ? std::abs(body.rotationPeriodDays)
            : body.rotationPeriodDays;
        float speed = static_cast<float>(baseSpeed / period);
        return std::clamp(speed, -2.0f, 2.0f);
    }

    float getAxialTilt(const BodyData& body) const {
        return glm::radians(static_cast<float>(body.axialTiltDegrees));
    }
};
}
