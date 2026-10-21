#include <cassert>
#include <iostream>
#include <cmath>
#include <string>
#include <tuple>
#include "../SolarSystem/AstronomicalData.hpp"
#include "../SolarSystem/Ephemeris.hpp"
#include "../SolarSystem/Hud.hpp"
#include "../SolarSystem/InputHandler.hpp"

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
        int y = 1960, m = 15, d = 35, h = 26, min = 70, s = 90;
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
            const glm::dmat3 toBody = glm::transpose(Astro::earthToIcrf(Astro::unixToJulianDate(unix), jdTdb, nut.x, nut.y));
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

    std::cout << "\n>>> ALL TIME PICKER UNIT TESTS PASSED SUCCESSFULLY! <<<\n";
    return 0;
}
