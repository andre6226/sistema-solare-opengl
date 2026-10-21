#include <cassert>
#include <iostream>
#include <cmath>
#include <string>
#include <tuple>
#include "../SolarSystem/AstronomicalData.hpp"
#include "../SolarSystem/Ephemeris.hpp"
#include "../SolarSystem/Orbits.hpp"
#include "../SolarSystem/Hud.hpp"
#include "../SolarSystem/InputHandler.hpp"
#include "../SolarSystem/Travel.hpp"

int main() {
    std::cout << "=== Test Time Picker Logic & Astronomy Conversions ===\n";

    // 1. Astro::daysInMonth tests
    assert(Astro::daysInMonth(2024, 2) == 29); // Leap year
    assert(Astro::daysInMonth(2023, 2) == 28); // Non-leap
    assert(Astro::daysInMonth(2000, 2) == 29); // Century leap
    assert(Astro::daysInMonth(1900, 2) == 28); // Century non-leap
    assert(Astro::daysInMonth(2026, 4) == 30);
    assert(Astro::daysInMonth(2026, 10) == 31);
    std::cout << "[PASS] daysInMonth calculations\n";

    // 2. Round-trip conversion between unix and UTC parts
    const double testEpochs[] = {
        0.0,            // 1970-01-01 00:00:00
        946728000.0,    // 2000-01-01 12:00:00
        1712600280.0,   // 2024-04-08 18:18:00 (Great North American Eclipse)
        1792554228.0    // ~2026-10-21
    };

    for (double origUnix : testEpochs) {
        int y = 0, m = 0, d = 0, h = 0, min = 0, s = 0;
        Astro::unixToUtcDateTimeParts(origUnix, y, m, d, h, min, s);
        double roundTrip = Astro::utcDateTimeToUnix(y, m, d, h, min, s);
        assert(std::abs(roundTrip - origUnix) < 0.001);
    }
    std::cout << "[PASS] Unix <-> UTC parts round-trip\n";

    // 3. Astro::clampPickerValues tests
    {
        int y = 1850, m = 15, d = 35, h = 26, min = 70, s = 90;
        Astro::clampPickerValues(y, m, d, h, min, s);
        assert(y == 1900);
        assert(m == 12);
        assert(d == 31);
        assert(h == 23);
        assert(min == 59);
        assert(s == 59);

        // Day clamping on February of leap vs non-leap year
        int y2 = 2024, m2 = 2, d2 = 31, h2 = 0, min2 = 0, s2 = 0;
        Astro::clampPickerValues(y2, m2, d2, h2, min2, s2);
        assert(d2 == 29);

        int y3 = 2023, m3 = 2, d3 = 31, h3 = 0, min3 = 0, s3 = 0;
        Astro::clampPickerValues(y3, m3, d3, h3, min3, s3);
        assert(d3 == 28);
    }
    std::cout << "[PASS] clampPickerValues boundary behavior\n";

    // 4. TimePickerLayout geometry tests
    {
        const float screenW = 1280.0f;
        const float screenH = 720.0f;
        const TimePickerLayout L = TimePickerLayout::compute(screenW, screenH);

        assert(L.dlgW > 400.0f);
        assert(L.dlgH > 180.0f);
        assert(L.dlgX >= 24.0f && L.dlgX + L.dlgW <= screenW);
        assert(L.dlgY >= 24.0f && L.dlgY + L.dlgH <= screenH);

        // Fields must be ordered left to right with positive widths and heights
        for (int i = 0; i < 6; ++i) {
            assert(L.fields[i].w > 0.0f && L.fields[i].h > 0.0f);
            assert(L.fields[i].plusH > 0.0f && L.fields[i].minusH > 0.0f);
            if (i > 0) {
                assert(L.fields[i].x > L.fields[i - 1].x + L.fields[i - 1].w);
            }
        }

        // Hit testing
        assert(TimePickerLayout::contains(L.dlgX + 10.0f, L.dlgY + 10.0f, L.dlgX, L.dlgY, L.dlgW, L.dlgH));
        assert(!TimePickerLayout::contains(L.dlgX - 10.0f, L.dlgY, L.dlgX, L.dlgY, L.dlgW, L.dlgH));
    }
    std::cout << "[PASS] TimePickerLayout geometry & hitboxes\n";

    // 5. InputState adjustments & digit typing tests
    {
        InputState st;
        st.pickerYear = 2024;
        st.pickerMonth = 4;
        st.pickerDay = 8;
        st.pickerHour = 18;
        st.pickerMin = 18;
        st.pickerSec = 0;

        // Adjust month forwards and backwards with wrap
        InputHandler::adjustPickerField(st, 1, 1);
        assert(st.pickerMonth == 5);
        InputHandler::adjustPickerField(st, 1, -5); // 5 - 5 -> 0 wraps to 12
        assert(st.pickerMonth == 12);

        // Adjust hour with wrap
        InputHandler::adjustPickerField(st, 3, 6); // 18 + 6 = 24 wraps to 0
        assert(st.pickerHour == 0);
        InputHandler::adjustPickerField(st, 3, -1); // 0 - 1 wraps to 23
        assert(st.pickerHour == 23);

        // Digit typing simulation for Year: type '2', '0', '2', '6'
        st.pickerField = 0;
        st.digitBuffer.clear();
        InputHandler::handleDigit(st, 2);
        assert(st.pickerField == 0);
        InputHandler::handleDigit(st, 0);
        assert(st.pickerField == 0);
        InputHandler::handleDigit(st, 2);
        assert(st.pickerField == 0);
        InputHandler::handleDigit(st, 6);
        assert(st.pickerYear == 2026);
        assert(st.pickerField == 1); // auto-advanced to month!
        assert(st.digitBuffer.empty());
    }
    std::cout << "[PASS] InputHandler adjustment & digit typing\n";

    // 6. Real-time Astronomical Synchronized Rotation Verification
    {
        // Earth 24-hour full rotation
        const double T0 = 0.268; // Epoch
        const double T_24h = T0 + 1.0 / Astro::DAYS_PER_JULIAN_CENTURY; // + 1 solar day (86400s)
        const double D0 = T0 * Astro::DAYS_PER_JULIAN_CENTURY;
        const double D1 = T_24h * Astro::DAYS_PER_JULIAN_CENTURY;

        // Solar eclipses against the NASA catalogue (greatest eclipse in TD, gamma):
        // the shadow axis must pass closest to Earth's centre within 2 s and 0.0005 Earth radii.
        const Ephemeris eph("resources/de440_1900_2200.bin");
        struct Eclipse { int y, mo, d, h, mi, s; double gamma; };
        for (const Eclipse& x : {Eclipse{1901, 5, 18, 5, 33, 48, -0.3626}, Eclipse{1919, 5, 29, 13, 8, 55, -0.2955},
                                 Eclipse{1973, 6, 30, 11, 38, 41, -0.0785}, Eclipse{1999, 8, 11, 11, 4, 9, 0.5062},
                                 Eclipse{2026, 8, 12, 17, 47, 6, 0.8977}, Eclipse{2027, 8, 2, 10, 7, 50, 0.1421},
                                 Eclipse{2099, 9, 14, 16, 57, 53, 0.3942}, Eclipse{2200, 4, 14, 15, 49, 57, 0.5847}}) {
            const double jd0 = Astro::unixToJulianDate(Astro::utcDateTimeToUnix(x.y, x.mo, x.d, x.h, x.mi, x.s));
            double best = 1e9, bestDt = 0.0;
            for (double dt = -120.0; dt <= 120.0; dt += 0.25) {
                const double jd = jd0 + dt / Astro::SECONDS_PER_DAY;
                const glm::dvec3 moon = eph.moonForShadows(jd).position;
                const glm::dvec3 axis = glm::normalize(moon + eph.earthForShadows(jd).position);  // Sun -> Moon
                const double miss = glm::length(glm::cross(moon, axis)) / 6378.137;
                if (miss < best) { best = miss; bestDt = dt; }
            }
            assert(std::abs(bestDt) <= 2.0);
            assert(std::abs(best - std::abs(x.gamma)) < 0.0005);
        }

        // Earth's orientation: the subsolar point at noon UTC sits on Greenwich give or
        // take the equation of time, at the solar declination.
        for (const auto& [month, day, lat, lon] : {std::tuple{3, 20, 0.0, 1.9}, std::tuple{6, 21, 23.44, 0.4},
                                                     std::tuple{12, 21, -23.43, -0.5}}) {
            const double unix = Astro::utcDateTimeToUnix(2026, month, day, 12, 0, 0);
            const double jdTdb = Astro::unixToJulianDateTdb(unix);
            const glm::dvec3 nut = eph.evaluate(Ephemeris::Nutation, jdTdb).position;
            const glm::dmat3 toBody = glm::transpose(Astro::earthToIcrf(Astro::unixToJulianDateUt1(unix), jdTdb, nut.x, nut.y));
            const glm::dvec3 sun = glm::normalize(toBody * -eph.heliocentric(Ephemeris::EarthMoonBarycentre, jdTdb).position);
            assert(std::abs(glm::degrees(std::asin(sun.z)) - lat) < 0.3);
            assert(std::abs(glm::degrees(std::atan2(sun.y, sun.x)) - lon) < 0.3);
        }

        // Delta T: measured values, continuity at the end of the table, a sane forecast.
        assert(std::abs(Astro::deltaTSeconds(Astro::utcDateTimeToUnix(2024, 1, 1, 0, 0, 0)) - 69.18) < 0.01);
        assert(std::abs(Astro::deltaTSeconds(Astro::utcDateTimeToUnix(1950, 1, 1, 0, 0, 0)) - 29.15) < 0.01);
        const double joint = Astro::utcDateTimeToUnix(2026, 1, 1, 0, 0, 0);
        assert(std::abs(Astro::deltaTSeconds(joint + 3600.0) - Astro::deltaTSeconds(joint - 3600.0)) < 0.01);
        assert(Astro::deltaTSeconds(Astro::utcDateTimeToUnix(2200, 1, 1, 0, 0, 0)) > 100.0);
        // TT - UTC: leap seconds from 1972 (37 s TAI - UTC since 2017), none assumed after;
        // UT1 stays within 0.9 s of UTC while leap seconds kept it there.
        assert(std::abs(Astro::ttMinusUtcSeconds(Astro::utcDateTimeToUnix(2016, 12, 31, 23, 0, 0)) - 68.184) < 1e-9);
        assert(std::abs(Astro::ttMinusUtcSeconds(Astro::utcDateTimeToUnix(2017, 1, 1, 0, 0, 0)) - 69.184) < 1e-9);
        assert(std::abs(Astro::ttMinusUtcSeconds(Astro::utcDateTimeToUnix(2150, 1, 1, 0, 0, 0)) - 69.184) < 1e-9);
        assert(std::abs(Astro::ttMinusUtcSeconds(Astro::utcDateTimeToUnix(1972, 1, 1, 0, 0, 0)) - 42.184) < 1e-9);
        for (int year = 1972; year <= 2026; ++year) {
            const double u = Astro::utcDateTimeToUnix(year, 6, 1, 0, 0, 0);
            assert(std::abs(Astro::unixToJulianDateUt1(u) - Astro::unixToJulianDate(u)) * Astro::SECONDS_PER_DAY < 1.0);
        }

        // Planetary moons, mean orbit + correction table, against JPL Horizons
        // (planet-centred ICRF km, instants off the tables' sample grid): within 1 km.
        struct MoonCheck { const char* name; double jd, x, y, z; };
        int moonsChecked = 0;
        for (const MoonCheck& c : {
                                  MoonCheck{"Phobos", 2417848.936849, -3505.880, -8412.562, -2101.356},
                                  MoonCheck{"Deimos", 2423596.498020, -14485.424, 12721.450, 13349.187},
                                  MoonCheck{"Io", 2429344.059191, 412520.033, 78588.131, 44325.211},
                                  MoonCheck{"Europa", 2435091.620362, -475092.891, 421511.872, 197254.258},
                                  MoonCheck{"Ganymede", 2440839.181534, 21708.514, 966749.834, 458042.909},
                                  MoonCheck{"Callisto", 2446586.742705, 1324878.500, 1192514.656, 584149.326},
                                  MoonCheck{"Mimas", 2452334.303876, -150849.842, 113622.996, -584.028},
                                  MoonCheck{"Enceladus", 2458081.865047, -229492.755, -62430.216, 24319.725},
                                  MoonCheck{"Tethys", 2460955.271828, -145374.071, -255036.011, 25653.804},
                                  MoonCheck{"Dione", 2463829.426219, -58409.827, 372972.553, -22443.410},
                                  MoonCheck{"Rhea", 2469576.987390, 440852.868, 282644.369, -56418.955},
                                  MoonCheck{"Titan", 2475324.548561, -506334.671, -1125553.300, 119398.690},
                                  MoonCheck{"Iapetus", 2481072.109732, 1061669.791, -3280544.895, -869657.829},
                                  MoonCheck{"Miranda", 2486819.670903, -51396.163, 41012.752, -111986.356},
                                  MoonCheck{"Ariel", 2492567.232075, -21494.797, 55326.757, -181497.999},
                                  MoonCheck{"Umbriel", 2498314.793246, 158225.555, -89820.097, 194934.759},
                                  MoonCheck{"Titania", 2504062.354417, -179155.364, -67403.999, 393223.505},
                                  MoonCheck{"Oberon", 2509809.915588, 260537.176, 83640.295, -514680.152},
                                  MoonCheck{"Triton", 2515557.476760, 209606.852, 282114.469, -47959.890},
                                  MoonCheck{"Charon", 2521305.037931, 9140.228, 6099.864, -16224.129},
                                  MoonCheck{"Hyperion", 2420000.123, -1201004.484, -585016.214, 130579.231},
                                  MoonCheck{"Proteus", 2470000.371, 60684.883, -54553.200, -84728.111},
                                  MoonCheck{"Nereid", 2500000.6, 4593619.233, 3329112.974, 2039225.231},
                                  MoonCheck{"Nereid", 2433500.25, 2408248.978, 8227790.643, 4408314.831}}) {
            for (const Astro::SatelliteOrbit& s : Astro::satelliteOrbits()) {
                if (std::string(s.name) != c.name) continue;
                const glm::dvec3 p = Orbits::moonPosition(s, MoonTable(std::string("resources/moons/") + c.name + ".bin"), c.jd);
                assert(glm::length(p - glm::dvec3(c.x, c.y, c.z)) < 1.0);
                ++moonsChecked;
            }
        }
        assert(moonsChecked == 24);
        // Hyperion's HUD period (Orbits::measuredTurn): one turn on the real path, 21.28 d
        // on average, a few tenths either way with Titan's tug; its osculating ellipse
        // would say ~22 d. The rest keep their mean values.
        for (const Astro::SatelliteOrbit& s : Astro::satelliteOrbits()) {
            assert(Orbits::offAnyEllipse(s) == (std::string(s.name) == "Hyperion" || std::string(s.name) == "Nereid"));
            if (std::string(s.name) != "Hyperion") continue;
            const MoonTable table("resources/moons/Hyperion.bin");
            for (double jd = 2440000.0; jd < 2480000.0; jd += 1234.5) {
                assert(std::abs(Orbits::measuredTurn(s, table, jd).periodDays - 21.28) < 0.6);
            }
        }

        // Every moon's orbit line is drawn: none may reach the renderer with a <= 0.
        for (const Astro::SatelliteOrbit& s : Astro::satelliteOrbits()) {
            const Astro::KeplerianBasis b = Astro::satelliteBasis(s, 1234.5);
            assert(b.a_km > 0.0);
            assert(glm::length(Astro::evaluateOrbitVertex(b, b.E_rad)) >= 0.999 * b.a_km * (1.0 - b.e));   // never inside periapsis
        }
        // The drawn line (Orbits::moonLine) passes through the moon and stays a closed
        // ellipse, Hyperion included.
        for (const Astro::SatelliteOrbit& s : Astro::satelliteOrbits()) {
            const MoonTable table(std::string("resources/moons/") + s.name + ".bin");
            const double jd = 2461335.5;
            const Astro::KeplerianBasis b = Orbits::moonLine(s, table, jd);
            assert(glm::length(Astro::evaluateOrbitVertex(b, b.E_rad) - Orbits::moonPosition(s, table, jd)) < 1.0);
            assert(b.a_km > 0.5 * std::abs(s.a) && b.a_km < 2.0 * std::abs(s.a) && b.e < 0.9);
        }

        // Pluto's centre: DE440's Pluto-Charon barycentre minus Charon's pull (as in
        // SolarSystem::plutoCentreOffset), against JPL Horizons body 999: within 1 km.
        {
            const double jd = 2459324.825000;
            const glm::dvec3 jpl(2151452480.741, -4217531964.625, -1964164356.060);
            for (const Astro::SatelliteOrbit& s : Astro::satelliteOrbits()) {
                if (std::string(s.name) != "Charon") continue;
                const glm::dvec3 charon = Orbits::moonPosition(s, MoonTable("resources/moons/Charon.bin"), jd);
                const glm::dvec3 pluto = eph.heliocentric(Ephemeris::Pluto, jd).position - Orbits::plutoBarycentreOffset(charon, eph);
                assert(glm::length(pluto - jpl) < 1.0);
            }
        }

        // Asteroids and dwarf planets: barycentric table minus the DE440 Sun, against
        // JPL Horizons heliocentric ICRF km (short requests, off the table grid): within 1 km.
        struct BodyCheck { const Astro::BodyData* body; double jd, x, y, z; };
        for (const BodyCheck& c : {
                                  BodyCheck{&Astro::Ceres, 2416798.270000, 134153810.254, -365280716.817, -197355128.272},
                                  BodyCheck{&Astro::Pallas, 2428818.770000, 273572167.709, -427710791.332, 67110035.424},
                                  BodyCheck{&Astro::Vesta, 2440839.270000, -312552020.034, -116127781.150, -5363698.720},
                                  BodyCheck{&Astro::Ida, 2452859.770000, -282177876.921, -313473716.954, -145548816.380},
                                  BodyCheck{&Astro::Eros, 2464880.270000, 74406003.102, 141417718.741, 93715400.263},
                                  BodyCheck{&Astro::Gaspra, 2476900.770000, -377755497.563, 31385594.706, -15148267.217},
                                  BodyCheck{&Astro::Haumea, 2488921.270000, 3212123512.326, -4141037029.736, -1971658775.260},
                                  BodyCheck{&Astro::Eris, 2500941.770000, 6375868801.281, 6675202882.429, 6302981683.622},
                                  BodyCheck{&Astro::Makemake, 2512962.270000, 4900546273.837, -195445917.756, -3154151491.973},
                                  BodyCheck{&Astro::Halley, 2446470.8, 50111949.728, -72019365.066, -4035751.484},   // perihelion 1986
                                  BodyCheck{&Astro::Halley, 2418000.4, -174233512.009, 1200488793.779, 249558487.450}}) {
            const glm::dvec3 p = MoonTable(std::string("resources/bodies/") + c.body->name + ".bin").offset(c.jd)
                - eph.evaluate(Ephemeris::Sun, c.jd).position;
            assert(glm::length(p - glm::dvec3(c.x, c.y, c.z)) < 1.0);
        }

        // Halley's line (Orbits::smallBodyLine) far from the Sun holds still: perihelion
        // distance steady to 0.1% over 1990-2055, where the heliocentric ellipse swings 5%
        // with Jupiter. Its period, shown in the HUD, ~75.45 years until the 2061 passage.
        {
            const MoonTable halley("resources/bodies/Halley.bin");
            double qMin = 1e30, qMax = 0.0;
            for (double jd = 2448000.0; jd < 2472000.0; jd += 97.0) {
                glm::dvec3 fromSun;
                const Astro::KeplerianBasis b = Orbits::smallBodyLine(eph, halley, jd, &fromSun);
                assert(glm::length(b.focus_world + Astro::evaluateOrbitVertex(b, b.E_rad) - fromSun) < 1.0);   // through the comet
                qMin = std::min(qMin, b.a_km * (1.0 - b.e));
                qMax = std::max(qMax, b.a_km * (1.0 - b.e));
                assert(std::abs(b.periodDays / 365.25 - 75.45) < 0.1);
            }
            assert(qMax / qMin < 1.001);
            assert(std::abs(Orbits::smallBodyLine(eph, halley, 2477000.0).periodDays / 365.25 - 72.6) < 0.2);   // after 2061
        }

        // Find by name: prefix first, then substring, case aside.
        {
            const std::vector<std::string> names = {"Sun", "Saturn", "Dione", "Io", "Halley"};
            assert(SolarSystem::bestNameMatch(names, "io") == 3);
            assert(SolarSystem::bestNameMatch(names, "SAT") == 1);
            assert(SolarSystem::bestNameMatch(names, "ley") == 4);
            assert(SolarSystem::bestNameMatch(names, "") == -1 && SolarSystem::bestNameMatch(names, "xyz") == -1);
        }

        // Mars 1.03 days
        const double T_mars = T0 + 1.03 / Astro::DAYS_PER_JULIAN_CENTURY;
        const double marsRot0 = (D0 / Astro::Mars.rotationPeriodDays) * glm::two_pi<double>();
        const double marsRot1 = ((T_mars * Astro::DAYS_PER_JULIAN_CENTURY) / Astro::Mars.rotationPeriodDays) * glm::two_pi<double>();
        assert(std::abs((marsRot1 - marsRot0) - glm::two_pi<double>()) < 1e-9);

        // Jupiter 0.41 days (9.84 hours)
        const double T_jup = T0 + 0.41 / Astro::DAYS_PER_JULIAN_CENTURY;
        const double jupRot0 = (D0 / Astro::Jupiter.rotationPeriodDays) * glm::two_pi<double>();
        const double jupRot1 = ((T_jup * Astro::DAYS_PER_JULIAN_CENTURY) / Astro::Jupiter.rotationPeriodDays) * glm::two_pi<double>();
        assert(std::abs((jupRot1 - jupRot0) - glm::two_pi<double>()) < 1e-9);

        // Moon 27.3 days
        const double T_moon = T0 + 27.3 / Astro::DAYS_PER_JULIAN_CENTURY;
        const double moonRot0 = (D0 / Astro::Moon.rotationPeriodDays) * glm::two_pi<double>();
        const double moonRot1 = ((T_moon * Astro::DAYS_PER_JULIAN_CENTURY) / Astro::Moon.rotationPeriodDays) * glm::two_pi<double>();
        assert(std::abs((moonRot1 - moonRot0) - glm::two_pi<double>()) < 1e-9);

        // Venus retrograde rotation (negative phase rate)
        const double venusRot0 = (D0 / std::abs(Astro::Venus.rotationPeriodDays)) * glm::two_pi<double>();
        const double venusRot1 = (D1 / std::abs(Astro::Venus.rotationPeriodDays)) * glm::two_pi<double>();
        assert(venusRot1 > venusRot0);
    }
    std::cout << "[PASS] Astronomical synchronized rotation invariants\n";

    // Flight path: starts and ends in place, continuous at halfway, never backtracks.
    {
        const double L = 6.3e8, r0 = 2.5e4, r1 = 2.8e5;   // Earth to Jupiter, both framed at 4 radii
        assert(travelDistance(0.0, L, r0, r1) == 0.0);
        assert(std::abs(travelDistance(1.0, L, r0, r1) - L) < 1e-6 * L);
        assert(std::abs(travelDistance(0.5, L, r0, r1) - 0.5 * L) < 1e-6 * L);
        assert(std::abs(travelDistance(0.5 + 1e-9, L, r0, r1) - 0.5 * L) < 1e-3 * L);
        double prev = 0.0;
        for (int i = 1; i <= 1000; ++i) {
            const double d = travelDistance(i / 1000.0, L, r0, r1);
            assert(d >= prev);
            prev = d;
        }
        assert(std::abs(prev - L) < 1e-6 * L);
        // A tenth of the way in, the camera is still within a few hundred radii of Earth.
        assert(travelDistance(0.1, L, r0, r1) < 300 * r0);
    }
    std::cout << "[PASS] Flight path to a clicked body\n";

    std::cout << "\n>>> ALL TIME PICKER UNIT TESTS PASSED SUCCESSFULLY! <<<\n";
    return 0;
}
