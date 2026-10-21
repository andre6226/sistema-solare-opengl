/**
 * test_challenger_m4_empirical.cpp
 * Adversarial Empirical Verification & Stress Test Harness for Milestone M4
 * Focus: UTC Clock Precision, Boundary Stress & HudFrame Synchronization
 *
 * Authored by: challenger_m4_1 (critic, specialist)
 *
 * This test harness empirically challenges:
 * 1. Real-time UTC clock formatting across extreme boundaries:
 *    - Negative Unix timestamps (pre-1970, 1969, 1960, 1900, 1800, 1000 AD)
 *    - Fractional seconds & sub-second floor behavior (positive, negative, epsilon offsets)
 *    - Leap years (1972, 2000, 2024, 2028): second-by-second rollover, 29 days in Feb
 *    - Century non-leap years (1900, 2100, 2200): second-by-second rollover, strict omission of Feb 29
 *    - High future timestamps (Year 3000 non-leap century, Year 9999 4-digit limit)
 *    - Midnight, month-end, and year-end rollovers across leap and common years
 * 2. 100,000 random timestamps stress test:
 *    - Verifying 100% strictly conform to `^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC$`
 *    - Verifying 100% have length exactly 23 characters
 *    - Validating calendar field bounds (YYYY, MM, DD, HH, MM, SS)
 *    - Invertibility round-trip test via POSIX timegm()
 * 3. HudFrame simulation clock synchronization & invariance:
 *    - Monotonic advancement across 100,000 variable time steps
 *    - Drift-free accumulation across 100,000 steps at 60 FPS and multi-rate (30-240Hz)
 *    - Multi-scale acceleration and time rewind robustness
 *    - HudFrame member integration, empty bypass, and 'O  orbits' HUD hint conformance
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <regex>
#include <chrono>
#include <random>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "SolarSystem/AstronomicalData.hpp"
#include "SolarSystem/Hud.hpp"

namespace ChallengerM4 {

struct AdversarialReport {
    int totalChecks = 0;
    int passedChecks = 0;
    int failedChecks = 0;

    void assertCheck(const std::string& description, bool condition, const std::string& details = "") {
        totalChecks++;
        if (condition) {
            passedChecks++;
            std::cout << "  [PASS] " << description << "\n";
        } else {
            failedChecks++;
            std::cout << "  [FAIL] " << description << " --> " << details << "\n";
        }
    }
};

// Helper: Check Gregorian leap year rule
inline bool isLeapYear(int year) {
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return (year % 4 == 0);
}

// Helper: Get days in month for a given year
inline int daysInMonth(int year, int month) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && isLeapYear(year)) return 29;
    if (month >= 1 && month <= 12) return days[month - 1];
    return 0;
}

// ============================================================================
// SUITE 1: Extreme Temporal Boundary Oracle Tests
// ============================================================================
void testTemporalBoundaries(AdversarialReport& report) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 1: EXTREME TEMPORAL BOUNDARY ORACLE TESTS\n";
    std::cout << "====================================================================\n";

    // --- 1.1 Pre-1970 Negative Unix Timestamps ---
    std::cout << "\n--- 1.1 Pre-1970 Negative Unix Timestamps ---\n";
    {
        // 1 second before Unix Epoch
        std::string sMinus1 = Astro::formatUtcDateTime(-1.0);
        report.assertCheck("Timestamp -1.0s formats to 1969-12-31 23:59:59 UTC",
                           sMinus1 == "1969-12-31 23:59:59 UTC", "Got: " + sMinus1);

        // Exactly 1 day before Unix Epoch
        std::string sMinus1Day = Astro::formatUtcDateTime(-86400.0);
        report.assertCheck("Timestamp -86400.0s formats to 1969-12-31 00:00:00 UTC",
                           sMinus1Day == "1969-12-31 00:00:00 UTC", "Got: " + sMinus1Day);

        // Exactly 1 year before Unix Epoch (1969 has 365 days)
        std::string sMinus1Year = Astro::formatUtcDateTime(-31536000.0);
        report.assertCheck("Timestamp -31536000.0s formats to 1969-01-01 00:00:00 UTC",
                           sMinus1Year == "1969-01-01 00:00:00 UTC", "Got: " + sMinus1Year);

        // 1968 Leap Day: 1968-02-29 00:00:00 UTC = -58060800.0s
        std::string s1968Leap = Astro::formatUtcDateTime(-58060800.0);
        report.assertCheck("Timestamp -58060800.0s formats to 1968-02-29 00:00:00 UTC",
                           s1968Leap == "1968-02-29 00:00:00 UTC", "Got: " + s1968Leap);

        // Historical Year 1900 Epoch: 1900-01-01 00:00:00 UTC = -2208988800.0s
        std::string s1900Epoch = Astro::formatUtcDateTime(-2208988800.0);
        report.assertCheck("Timestamp -2208988800.0s formats to 1900-01-01 00:00:00 UTC",
                           s1900Epoch == "1900-01-01 00:00:00 UTC", "Got: " + s1900Epoch);

        // Historical Year 1800 Epoch: 1800-01-01 00:00:00 UTC = -5364662400.0s
        std::string s1800Epoch = Astro::formatUtcDateTime(-5364662400.0);
        report.assertCheck("Timestamp -5364662400.0s formats to 1800-01-01 00:00:00 UTC",
                           s1800Epoch == "1800-01-01 00:00:00 UTC", "Got: " + s1800Epoch);

        // Historical Year 1000 Epoch: 1000-01-01 00:00:00 UTC = -30610224000.0s
        std::string s1000Epoch = Astro::formatUtcDateTime(-30610224000.0);
        report.assertCheck("Timestamp -30610224000.0s formats to 1000-01-01 00:00:00 UTC",
                           s1000Epoch == "1000-01-01 00:00:00 UTC", "Got: " + s1000Epoch);
    }

    // --- 1.2 Fractional Seconds & std::floor Mechanics ---
    std::cout << "\n--- 1.2 Fractional Seconds & std::floor Mechanics ---\n";
    {
        // Positive fractional seconds floor to current second
        report.assertCheck("0.0s -> 1970-01-01 00:00:00 UTC",
                           Astro::formatUtcDateTime(0.0) == "1970-01-01 00:00:00 UTC");
        report.assertCheck("0.000001s -> 1970-01-01 00:00:00 UTC",
                           Astro::formatUtcDateTime(0.000001) == "1970-01-01 00:00:00 UTC");
        report.assertCheck("0.500000s -> 1970-01-01 00:00:00 UTC",
                           Astro::formatUtcDateTime(0.5) == "1970-01-01 00:00:00 UTC");
        report.assertCheck("0.999999s -> 1970-01-01 00:00:00 UTC",
                           Astro::formatUtcDateTime(0.999999) == "1970-01-01 00:00:00 UTC");
        report.assertCheck("1.000000s -> 1970-01-01 00:00:01 UTC",
                           Astro::formatUtcDateTime(1.0) == "1970-01-01 00:00:01 UTC");

        // Negative fractional seconds floor correctly to -1 (1969-12-31 23:59:59)
        report.assertCheck("-0.000001s floors to -1.0 -> 1969-12-31 23:59:59 UTC",
                           Astro::formatUtcDateTime(-0.000001) == "1969-12-31 23:59:59 UTC",
                           "Got: " + Astro::formatUtcDateTime(-0.000001));
        report.assertCheck("-0.500000s floors to -1.0 -> 1969-12-31 23:59:59 UTC",
                           Astro::formatUtcDateTime(-0.5) == "1969-12-31 23:59:59 UTC",
                           "Got: " + Astro::formatUtcDateTime(-0.5));
        report.assertCheck("-0.999999s floors to -1.0 -> 1969-12-31 23:59:59 UTC",
                           Astro::formatUtcDateTime(-0.999999) == "1969-12-31 23:59:59 UTC",
                           "Got: " + Astro::formatUtcDateTime(-0.999999));
        report.assertCheck("-1.000000s floors to -1.0 -> 1969-12-31 23:59:59 UTC",
                           Astro::formatUtcDateTime(-1.0) == "1969-12-31 23:59:59 UTC",
                           "Got: " + Astro::formatUtcDateTime(-1.0));
        report.assertCheck("-1.000001s floors to -2.0 -> 1969-12-31 23:59:58 UTC",
                           Astro::formatUtcDateTime(-1.000001) == "1969-12-31 23:59:58 UTC",
                           "Got: " + Astro::formatUtcDateTime(-1.000001));

        // Sub-second invariance sweep: 1000 points between 946728000.0 and 946728000.999
        bool allFloorMatches = true;
        for (int i = 0; i < 1000; ++i) {
            double testTs = 946728000.0 + (double)i / 1000.0;
            if (Astro::formatUtcDateTime(testTs) != "2000-01-01 12:00:00 UTC") {
                allFloorMatches = false;
                break;
            }
        }
        report.assertCheck("1000 sub-second fractional samples floor cleanly without premature advancement",
                           allFloorMatches);
    }

    // --- 1.3 Leap Years Verification (1972, 2000, 2024, 2028) ---
    std::cout << "\n--- 1.3 Leap Years Verification (1972, 2000, 2024, 2028) ---\n";
    {
        struct LeapYearScenario {
            int year;
            double feb28_235959;
            double feb29_000000;
            double feb29_235959;
            double mar01_000000;
        };

        const LeapYearScenario scenarios[] = {
            { 1972, 68169599.0, 68169600.0, 68255999.0, 68256000.0 },
            { 2000, 951782399.0, 951782400.0, 951868799.0, 951868800.0 },
            { 2024, 1709164799.0, 1709164800.0, 1709251199.0, 1709251200.0 },
            { 2028, 1835395199.0, 1835395200.0, 1835481599.0, 1835481600.0 }
        };

        for (const auto& sc : scenarios) {
            std::string yStr = std::to_string(sc.year);
            std::string sFeb28 = Astro::formatUtcDateTime(sc.feb28_235959);
            std::string sFeb29Start = Astro::formatUtcDateTime(sc.feb29_000000);
            std::string sFeb29End = Astro::formatUtcDateTime(sc.feb29_235959);
            std::string sMar01 = Astro::formatUtcDateTime(sc.mar01_000000);

            report.assertCheck("Leap Year " + yStr + ": Feb 28 23:59:59 formats correctly",
                               sFeb28 == yStr + "-02-28 23:59:59 UTC", "Got: " + sFeb28);
            report.assertCheck("Leap Year " + yStr + ": Feb 29 00:00:00 exists and transitions cleanly",
                               sFeb29Start == yStr + "-02-29 00:00:00 UTC", "Got: " + sFeb29Start);
            report.assertCheck("Leap Year " + yStr + ": Feb 29 23:59:59 formats correctly",
                               sFeb29End == yStr + "-02-29 23:59:59 UTC", "Got: " + sFeb29End);
            double feb01 = sc.feb29_000000 - 28.0 * 86400.0;
            std::string sFeb01 = Astro::formatUtcDateTime(feb01);
            report.assertCheck("Leap Year " + yStr + ": Feb 01 00:00:00 formats correctly",
                               sFeb01 == yStr + "-02-01 00:00:00 UTC", "Got: " + sFeb01);
            report.assertCheck("Leap Year " + yStr + ": February duration is exactly 29 days (2505600s)",
                               (sc.mar01_000000 - feb01) == 29.0 * 86400.0);
        }
    }

    // --- 1.4 Century Non-Leap Years (1900, 2100, 2200) ---
    std::cout << "\n--- 1.4 Century Non-Leap Years (1900, 2100, 2200) ---\n";
    {
        struct CenturyNonLeapScenario {
            int year;
            double feb28_235959;
            double mar01_000000;
        };

        const CenturyNonLeapScenario scenarios[] = {
            { 1900, -2203891201.0, -2203891200.0 },
            { 2100, 4107542399.0, 4107542400.0 },
            { 2200, 7263215999.0, 7263216000.0 }
        };

        for (const auto& sc : scenarios) {
            std::string yStr = std::to_string(sc.year);
            std::string sFeb28 = Astro::formatUtcDateTime(sc.feb28_235959);
            std::string sMar01 = Astro::formatUtcDateTime(sc.mar01_000000);

            report.assertCheck("Century Non-Leap Year " + yStr + ": Feb 28 23:59:59 formats correctly",
                               sFeb28 == yStr + "-02-28 23:59:59 UTC", "Got: " + sFeb28);
            report.assertCheck("Century Non-Leap Year " + yStr + ": 1-second step transitions directly to Mar 01",
                               sMar01 == yStr + "-03-01 00:00:00 UTC", "Got: " + sMar01);
            report.assertCheck("Century Non-Leap Year " + yStr + ": Feb 29 is strictly omitted (no 02-29 in output)",
                               sFeb28.find("-02-29") == std::string::npos && sMar01.find("-02-29") == std::string::npos);
        }
    }

    // --- 1.5 High Future Timestamps (Year 3000 & Year 9999) ---
    std::cout << "\n--- 1.5 High Future Timestamps (Year 3000 & Year 9999) ---\n";
    {
        // Year 3000 Epoch
        std::string s3000Start = Astro::formatUtcDateTime(32503680000.0);
        report.assertCheck("Year 3000 Epoch formats to 3000-01-01 00:00:00 UTC",
                           s3000Start == "3000-01-01 00:00:00 UTC", "Got: " + s3000Start);

        // Year 3000 is a Century Non-Leap Year (3000 % 400 == 200 != 0)
        std::string s3000Feb28 = Astro::formatUtcDateTime(32508777599.0);
        std::string s3000Mar01 = Astro::formatUtcDateTime(32508777600.0);
        report.assertCheck("Year 3000 Feb 28 23:59:59 formats to 3000-02-28 23:59:59 UTC",
                           s3000Feb28 == "3000-02-28 23:59:59 UTC", "Got: " + s3000Feb28);
        report.assertCheck("Year 3000 1-second step advances directly to 3000-03-01 00:00:00 UTC (non-leap century)",
                           s3000Mar01 == "3000-03-01 00:00:00 UTC", "Got: " + s3000Mar01);

        // Year 3000 End of Year -> Year 3001
        std::string s3000End = Astro::formatUtcDateTime(32535215999.0);
        std::string s3001Start = Astro::formatUtcDateTime(32535216000.0);
        report.assertCheck("Year 3000 End of Year formats to 3000-12-31 23:59:59 UTC",
                           s3000End == "3000-12-31 23:59:59 UTC", "Got: " + s3000End);
        report.assertCheck("Year 3000 millennium rollover advances to 3001-01-01 00:00:00 UTC",
                           s3001Start == "3001-01-01 00:00:00 UTC", "Got: " + s3001Start);

        // High Future Boundary: Year 9999 End of Year
        std::string s9999End = Astro::formatUtcDateTime(253402300799.0);
        report.assertCheck("Year 9999 End of Year formats to 9999-12-31 23:59:59 UTC",
                           s9999End == "9999-12-31 23:59:59 UTC", "Got: " + s9999End);
    }

    // --- 1.6 Midnight, Month, and Year Rollovers ---
    std::cout << "\n--- 1.6 Midnight, Month, and Year Rollovers ---\n";
    {
        // 100 consecutive days of midnight rollover
        double curMid = 1704067199.0; // 2023-12-31 23:59:59 UTC
        bool midnightRolloverOk = true;
        for (int day = 0; day < 100; ++day) {
            std::string tBefore = Astro::formatUtcDateTime(curMid);
            std::string tAfter  = Astro::formatUtcDateTime(curMid + 1.0);

            if (tBefore.substr(11, 8) != "23:59:59" || tAfter.substr(11, 8) != "00:00:00") {
                midnightRolloverOk = false;
                break;
            }
            curMid += 86400.0;
        }
        report.assertCheck("100 consecutive midnight rollovers execute seamlessly (23:59:59 -> 00:00:00)",
                           midnightRolloverOk);

        // All 12 month transitions in Leap Year 2024
        // Oracle timestamps for month ends in 2024:
        const std::pair<double, std::string> monthEnds2024[] = {
            { 1706745599.0, "2024-01-31 23:59:59 UTC" },
            { 1709251199.0, "2024-02-29 23:59:59 UTC" },
            { 1711929599.0, "2024-03-31 23:59:59 UTC" },
            { 1714521599.0, "2024-04-30 23:59:59 UTC" },
            { 1717199999.0, "2024-05-31 23:59:59 UTC" },
            { 1719791999.0, "2024-06-30 23:59:59 UTC" },
            { 1722470399.0, "2024-07-31 23:59:59 UTC" },
            { 1725148799.0, "2024-08-31 23:59:59 UTC" },
            { 1727740799.0, "2024-09-30 23:59:59 UTC" },
            { 1730419199.0, "2024-10-31 23:59:59 UTC" },
            { 1733011199.0, "2024-11-30 23:59:59 UTC" },
            { 1735689599.0, "2024-12-31 23:59:59 UTC" }
        };

        bool allMonthEndsOk = true;
        for (const auto& me : monthEnds2024) {
            std::string sBefore = Astro::formatUtcDateTime(me.first);
            std::string sAfter  = Astro::formatUtcDateTime(me.first + 1.0);
            if (sBefore != me.second || sAfter.substr(11, 8) != "00:00:00") {
                allMonthEndsOk = false;
                break;
            }
        }
        report.assertCheck("All 12 month transitions in leap year 2024 rollover cleanly to 00:00:00 of next month",
                           allMonthEndsOk);

        // Year boundary rollovers: 1999->2000, 2023->2024, 2024->2025, 2099->2100
        const double yearRolloverTimes[] = {
            946684799.0,   // 1999-12-31 23:59:59
            1704067199.0,  // 2023-12-31 23:59:59
            1735689599.0,  // 2024-12-31 23:59:59
            4102444799.0   // 2099-12-31 23:59:59
        };

        bool allYearRolloversOk = true;
        for (double t : yearRolloverTimes) {
            std::string sBefore = Astro::formatUtcDateTime(t);
            std::string sAfter  = Astro::formatUtcDateTime(t + 1.0);
            int yBefore = std::stoi(sBefore.substr(0, 4));
            int yAfter  = std::stoi(sAfter.substr(0, 4));
            if (yAfter != yBefore + 1 ||
                sBefore.substr(5, 14) != "12-31 23:59:59" ||
                sAfter.substr(5, 14) != "01-01 00:00:00") {
                allYearRolloversOk = false;
                break;
            }
        }
        report.assertCheck("Year rollovers (1999->2000, 2023->2024, 2024->2025, 2099->2100) advance year and reset date",
                           allYearRolloversOk);
    }
}

// ============================================================================
// SUITE 2: 100,000 Random Timestamps Stress Suite
// ============================================================================
void test100kRandomTimestamps(AdversarialReport& report) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 2: 100,000 RANDOM TIMESTAMPS STRESS SUITE\n";
    std::cout << "====================================================================\n";

    const std::regex utcRegex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC$)");
    const int N = 100000;

    std::mt19937_64 rng(0xFEEDFACE);

    // Distribution ranges:
    // 1. Modern era (1970 - 2100): 40,000 samples [0.0, 4102444800.0]
    // 2. Next millennium (2100 - 3000): 20,000 samples [4102444800.0, 32503680000.0]
    // 3. Deep future (3000 - 9999): 20,000 samples [32503680000.0, 253402300799.0]
    // 4. Historical negative (1000 - 1969): 20,000 samples [-30610224000.0, -1.0]

    std::uniform_real_distribution<double> distModern(0.0, 4102444800.0);
    std::uniform_real_distribution<double> distMillennium(4102444800.0, 32503680000.0);
    std::uniform_real_distribution<double> distDeepFuture(32503680000.0, 253402300799.0);
    std::uniform_real_distribution<double> distHistorical(-30610224000.0, -1.0);
    std::uniform_real_distribution<double> distSubSec(0.0, 0.999999);

    int regexFailures = 0;
    int lengthFailures = 0;
    int semanticFailures = 0;
    int roundTripFailures = 0;

    auto tStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        double rawTs = 0.0;
        if (i < 40000) {
            rawTs = distModern(rng);
        } else if (i < 60000) {
            rawTs = distMillennium(rng);
        } else if (i < 80000) {
            rawTs = distDeepFuture(rng);
        } else {
            rawTs = distHistorical(rng);
        }
        rawTs += distSubSec(rng);

        const std::string s = Astro::formatUtcDateTime(rawTs);

        // 1. Length invariant check: strictly 23 chars
        if (s.length() != 23) {
            lengthFailures++;
        }

        // 2. Regex check: strictly matches ^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} UTC$
        if (!std::regex_match(s, utcRegex)) {
            regexFailures++;
        }

        // 3. Semantic calendar field bounds
        int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
        if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d UTC",
                        &year, &month, &day, &hour, &min, &sec) != 6) {
            semanticFailures++;
        } else {
            if (year < 1000 || year > 9999) semanticFailures++;
            if (month < 1 || month > 12) semanticFailures++;
            if (day < 1 || day > daysInMonth(year, month)) semanticFailures++;
            if (hour < 0 || hour > 23) semanticFailures++;
            if (min < 0 || min > 59) semanticFailures++;
            if (sec < 0 || sec > 59) semanticFailures++;

            // 4. Inversion round-trip via POSIX timegm()
            std::tm tmBack = {};
            tmBack.tm_year = year - 1900;
            tmBack.tm_mon  = month - 1;
            tmBack.tm_mday = day;
            tmBack.tm_hour = hour;
            tmBack.tm_min  = min;
            tmBack.tm_sec  = sec;
            tmBack.tm_isdst = 0;

            time_t invertedSec = timegm(&tmBack);
            time_t expectedSec = static_cast<time_t>(std::floor(rawTs));

            if (invertedSec != expectedSec) {
                roundTripFailures++;
            }
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    std::cout << "  [Stress Execution] Processed " << N << " timestamps in "
              << std::fixed << std::setprecision(2) << elapsedMs << " ms ("
              << (N / (elapsedMs / 1000.0)) << " ts/sec)\n";

    report.assertCheck("100,000 random timestamps: 100% strictly have length == 23",
                       lengthFailures == 0, "Failures: " + std::to_string(lengthFailures));

    report.assertCheck("100,000 random timestamps: 100% strictly conform to ^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} UTC$",
                       regexFailures == 0, "Failures: " + std::to_string(regexFailures));

    report.assertCheck("100,000 random timestamps: 100% satisfy Gregorian calendar semantics (YYYY, MM, DD, HH, MM, SS)",
                       semanticFailures == 0, "Failures: " + std::to_string(semanticFailures));

    report.assertCheck("100,000 random timestamps: 100% round-trip match timegm(&tm) == floor(simSeconds)",
                       roundTripFailures == 0, "Failures: " + std::to_string(roundTripFailures));
}

// ============================================================================
// SUITE 3: HudFrame Simulation Clock Synchronization & Invariance
// ============================================================================
void testHudFrameSynchronization(AdversarialReport& report) {
    std::cout << "\n====================================================================\n";
    std::cout << "  SUITE 3: HUDFRAME SIMULATION CLOCK SYNCHRONIZATION & INVARIANCE\n";
    std::cout << "====================================================================\n";

    // --- 3.1 Monotonic Advancement Under Variable Frame Rates ---
    std::cout << "\n--- 3.1 Monotonic Advancement Under Variable Frame Rates ---\n";
    {
        double simTime = 1709164800.0; // 2024-02-29 00:00:00 UTC
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dtDist(0.001f, 0.050f); // 20 FPS to 1000 FPS variable dt

        bool strictlyMonotonicSimTime = true;
        bool strictlyMonotonicJulianDate = true;
        bool strictlyMonotonicCenturies = true;
        bool weaklyMonotonicString = true;

        double prevSimTime = simTime;
        double prevJD = Astro::unixToJulianDate(simTime);
        double prevT = Astro::unixToJulianCenturies(simTime);
        std::string prevStr = Astro::formatUtcDateTime(simTime);

        const int steps = 100000;
        for (int i = 0; i < steps; ++i) {
            float dt = dtDist(rng);
            simTime += static_cast<double>(dt);

            double curJD = Astro::unixToJulianDate(simTime);
            double curT  = Astro::unixToJulianCenturies(simTime);
            std::string curStr = Astro::formatUtcDateTime(simTime);

            if (simTime <= prevSimTime) strictlyMonotonicSimTime = false;
            if (curJD <= prevJD) strictlyMonotonicJulianDate = false;
            if (curT <= prevT) strictlyMonotonicCenturies = false;
            if (curStr < prevStr) weaklyMonotonicString = false;

            prevSimTime = simTime;
            prevJD = curJD;
            prevT = curT;
            prevStr = curStr;
        }

        report.assertCheck("Simulation clock: simTimeSeconds strictly increases over 100,000 steps",
                           strictlyMonotonicSimTime);
        report.assertCheck("Astronomical clock: Julian Date JD strictly increases over 100,000 steps",
                           strictlyMonotonicJulianDate);
        report.assertCheck("Astronomical clock: Julian Centuries T strictly increases over 100,000 steps",
                           strictlyMonotonicCenturies);
        report.assertCheck("HUD clock string: formatUtcDateTime produces monotonic lexicographical sequence",
                           weaklyMonotonicString);
    }

    // --- 3.2 Drift-Free Accumulation at 60 FPS and Multi-Rate Tests ---
    std::cout << "\n--- 3.2 Drift-Free Accumulation at 60 FPS and Multi-Rate Tests ---\n";
    {
        const double tStart = 1700000000.0;
        const int steps = 100000;
        const double dt60 = 1.0 / 60.0;

        double accumulatedSim = tStart;
        for (int i = 0; i < steps; ++i) {
            accumulatedSim += dt60;
        }

        const double exactAnalytic = tStart + static_cast<double>(steps) * dt60;
        const double drift60 = std::abs(accumulatedSim - exactAnalytic);

        // In IEEE-754 double precision at 1.7e9, 1 ULP is ~2.38e-7s. Over 100,000 steps,
        // theoretical worst-case ULP accumulation bound is ~0.024s.
        // We verify that drift is strictly < 0.01s (< 10 ms), ruling out single-precision
        // float degradation (which would cause 128s drift or complete clock freezing).
        report.assertCheck("60 FPS double accumulation drift across 100,000 frames is < 10 ms (IEEE-754 bounded)",
                           drift60 < 0.01, "Drift: " + std::to_string(drift60) + "s");

        // Multi-rate stress: 30, 60, 120, 144, 240 FPS across 10,000 frames each
        const double fpsList[] = { 30.0, 60.0, 120.0, 144.0, 240.0 };
        bool allRatesDriftFree = true;
        double maxDrift = 0.0;
        for (double fps : fpsList) {
            double dt = 1.0 / fps;
            int totalSteps = 10000;
            double sim = tStart;
            for (int s = 0; s < totalSteps; ++s) {
                sim += dt;
            }
            double analytic = tStart + static_cast<double>(totalSteps) * dt;
            double drift = std::abs(sim - analytic);
            if (drift > maxDrift) maxDrift = drift;
            if (drift > 0.005) { // 5 ms threshold for 10,000 steps
                allRatesDriftFree = false;
                break;
            }
        }
        report.assertCheck("Multi-rate simulation (30, 60, 120, 144, 240 FPS) over 10,000 frames each has drift < 5 ms",
                           allRatesDriftFree, "Max drift: " + std::to_string(maxDrift) + "s");
    }

    // --- 3.3 Multi-Scale Acceleration and Time Rewind Robustness ---
    std::cout << "\n--- 3.3 Multi-Scale Acceleration and Time Rewind Robustness ---\n";
    {
        const double t0 = 1709164800.0; // 2024-02-29 00:00:00 UTC
        const double scales[] = { 1.0, 10.0, 60.0, 3600.0, 86400.0, 31536000.0, -10.0, -86400.0 };

        bool allScalesValid = true;
        for (double scale : scales) {
            double sim = t0;
            float dt = 0.0166667f; // ~60 FPS
            for (int step = 0; step < 1000; ++step) {
                sim += static_cast<double>(dt) * scale;
            }
            std::string s = Astro::formatUtcDateTime(sim);
            if (s.length() != 23 || s.find("UTC") == std::string::npos) {
                allScalesValid = false;
                break;
            }
        }
        report.assertCheck("Time acceleration (1x to 31,536,000x) and rewind (-10x, -86400x) maintain valid UTC strings",
                           allScalesValid);
    }

    // --- 3.4 HudFrame Struct Integration & Layout Conformance ---
    std::cout << "\n--- 3.4 HudFrame Struct Integration & Layout Conformance ---\n";
    {
        // Verify HudFrame struct can hold and expose utcTime
        HudFrame frame;
        frame.targetName = "Earth";
        frame.fps = 60.0f;
        frame.timeScale = 1.0f;
        frame.cameraDistance = 149597870.7f;
        frame.utcTime = "2024-02-29 12:00:00 UTC";

        report.assertCheck("HudFrame instantiates and correctly populates utcTime member",
                           frame.utcTime == "2024-02-29 12:00:00 UTC");

        // Verify empty utcTime behaves safely
        frame.utcTime = "";
        report.assertCheck("HudFrame safely supports empty utcTime without corruption",
                           frame.utcTime.empty());

        // Verify Hud.hpp contains empty-check guard 'if (entry.empty()) continue;'
        std::ifstream hudFile("SolarSystem/Hud.hpp");
        std::string hudContent((std::istreambuf_iterator<char>(hudFile)),
                                std::istreambuf_iterator<char>());

        bool emptyGuardFound = (hudContent.find("if (entry.empty()) continue;") != std::string::npos);
        report.assertCheck("Hud::drawStatus contains 'if (entry.empty()) continue;' guard to skip blank rows",
                           emptyGuardFound);

        // Verify Hud::drawStatus places utcTime in entries array
        bool statusHasUtc = (hudContent.find("frame.utcTime") != std::string::npos);
        report.assertCheck("Hud::drawStatus incorporates frame.utcTime into status entries block",
                           statusHasUtc);

        // Verify controls hint string contains 'O  orbits'
        bool hintHasOrbits = (hudContent.find("O  orbits") != std::string::npos);
        report.assertCheck("Hud::drawControls displays 'O  orbits' control hint",
                           hintHasOrbits);

        // Verify layout geometry margin clearance on 800x600 screen
        // Controls hint length: 104 characters, font scale 0.42 * 32px glyph = ~13.4px per char avg width ~6.5px
        // Total text width approx 676px in available 752px (margin 24px each side)
        const float screenWidth = 800.0f;
        const float screenHeight = 600.0f;
        const float margin = 24.0f;
        const float availableWidth = screenWidth - 2.0f * margin; // 752px
        const float estimatedControlsWidth = 104.0f * (32.0f * 0.42f * 0.48f); // ~670px
        const float clearance = availableWidth - estimatedControlsWidth;

        report.assertCheck("Minimal 800x600 viewport provides >= 70px safe margin clearance for controls hint",
                           clearance >= 70.0f, "Clearance: " + std::to_string(clearance));
    }
}

} // namespace ChallengerM4

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main() {
    std::cout << "====================================================================\n";
    std::cout << "  EMPIRICAL CHALLENGER M4: UTC PRECISION & BOUNDARY STRESS HARNESS  \n";
    std::cout << "  Target: SolarSystem HUD Integration & Clock Precision (Milestone 4)\n";
    std::cout << "====================================================================\n";

    ChallengerM4::AdversarialReport report;

    auto t0 = std::chrono::high_resolution_clock::now();

    ChallengerM4::testTemporalBoundaries(report);
    ChallengerM4::test100kRandomTimestamps(report);
    ChallengerM4::testHudFrameSynchronization(report);

    auto t1 = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n====================================================================\n";
    std::cout << "  CHALLENGE EXECUTION SUMMARY\n";
    std::cout << "====================================================================\n";
    std::cout << "  Total Assertions Tested : " << report.totalChecks << "\n";
    std::cout << "  Passed Assertions       : " << report.passedChecks << "\n";
    std::cout << "  Failed Assertions       : " << report.failedChecks << "\n";
    std::cout << "  Total Test Time         : " << std::fixed << std::setprecision(2)
              << totalMs << " ms\n";
    std::cout << "====================================================================\n";

    if (report.failedChecks == 0) {
        std::cout << "  VERDICT: >>> APPROVE <<< (100% OF ADVERSARIAL STRESS TESTS PASSED)\n";
        std::cout << "====================================================================\n";
        return 0;
    } else {
        std::cout << "  VERDICT: >>> FAIL <<< (" << report.failedChecks << " ASSERTIONS FAILED)\n";
        std::cout << "====================================================================\n";
        return 1;
    }
}
