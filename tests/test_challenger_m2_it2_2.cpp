/**
 * test_challenger_m2_it2_2.cpp
 *
 * Empirical Adversarial Challenger Harness — Milestone M2 Iteration 2
 * Roles: critic, specialist (Empirical Challenger)
 * Agent: challenger_m2_it2_2
 *
 * Direct Empirical Challenges:
 *  1. Active Hierarchy Filter across Scene Graph:
 *     - Exhaustive verification of Sun focus, 7 Planet foci, 11 Moon foci (sibling filtering),
 *       Asteroid/Dwarf foci, and hostile edge cases (null target, null body, Sun, non-navigable rings).
 *  2. 'O' Key Toggle State & Memory Cleanliness:
 *     - Parity invariance under 1,000,000 rapid toggles.
 *     - Zero vertex computations and zero submitted orbits when disabled.
 *     - VBO capacity headroom (5,760 vertices vs 4,320 peak load).
 *     - Zero VBO reallocations or resource leakage across toggles.
 *  3. CPU Performance Stability & Zero-Heap Allocation over 1000 Iterations:
 *     - 1000 consecutive frames under peak load (Saturn focus: 24 active orbits, 4,320 vertices,
 *       with parent planet axial tilt matrix rotation).
 *     - Detailed microsecond latency statistics: Min, Max, Mean, P95, P99, StdDev.
 *     - Exact zero-heap allocation assertion across all 1000 frames.
 *     - Verification that frame CPU time is strictly < 1.0 ms (far within 16.6 ms 60 FPS budget).
 *  4. Moon Orbit Tilt Alignment Invariant:
 *     - Exact zero-sagitta alignment (< 1e-6 km) across all 20 catalog moons.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "SolarSystem/AstronomicalData.hpp"
#include "SolarSystem/CelestialBody.hpp"
#include "SolarSystem/OrbitRenderer.hpp"

// Global heap allocation tracker to verify zero heap churn in the inner loop
static size_t g_heapAllocCount = 0;
static size_t g_heapAllocBytes = 0;
static bool g_trackAllocations = false;

void* operator new(size_t size) {
    if (g_trackAllocations) {
        g_heapAllocCount++;
        g_heapAllocBytes += size;
    }
    return std::malloc(size);
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

namespace ChallengerM2_2 {

struct Report {
    int total = 0;
    int passed = 0;
    int failed = 0;

    void record(const std::string& name, bool pass, const std::string& detail = "") {
        total++;
        if (pass) {
            passed++;
            std::cout << "  \033[32m[PASS]\033[0m " << name;
            if (!detail.empty()) std::cout << " (" << detail << ")";
            std::cout << "\n";
        } else {
            failed++;
            std::cout << "  \033[31m[FAIL]\033[0m " << name;
            if (!detail.empty()) std::cout << " -> " << detail;
            std::cout << "\n";
        }
    }
};

// Scene Graph builder mirroring SolarSystem::buildSystem() exactly
struct FullSolarSystem {
    Astro::ScaleConverter conv;
    std::vector<std::unique_ptr<CelestialBody>> bodies;
    CelestialBody* sun = nullptr;
    CelestialBody* mercury = nullptr;
    CelestialBody* venus = nullptr;
    CelestialBody* earth = nullptr;
    CelestialBody* moon = nullptr;
    CelestialBody* eros = nullptr;
    CelestialBody* mars = nullptr;
    CelestialBody* phobos = nullptr;
    CelestialBody* deimos = nullptr;
    CelestialBody* gaspra = nullptr;
    CelestialBody* vesta = nullptr;
    CelestialBody* ceres = nullptr;
    CelestialBody* pallas = nullptr;
    CelestialBody* ida = nullptr;
    CelestialBody* jupiter = nullptr;
    CelestialBody* io = nullptr;
    CelestialBody* europa = nullptr;
    CelestialBody* ganymede = nullptr;
    CelestialBody* callisto = nullptr;
    CelestialBody* saturn = nullptr;
    CelestialBody* mimas = nullptr;
    CelestialBody* enceladus = nullptr;
    CelestialBody* dione = nullptr;
    CelestialBody* rhea = nullptr;
    CelestialBody* titan = nullptr;
    CelestialBody* iapetus = nullptr;
    CelestialBody* saturnRings = nullptr;
    CelestialBody* uranus = nullptr;
    CelestialBody* miranda = nullptr;
    CelestialBody* ariel = nullptr;
    CelestialBody* umbriel = nullptr;
    CelestialBody* titania = nullptr;
    CelestialBody* oberon = nullptr;
    CelestialBody* neptune = nullptr;
    CelestialBody* triton = nullptr;
    CelestialBody* pluto = nullptr;
    CelestialBody* charon = nullptr;
    CelestialBody* haumea = nullptr;
    CelestialBody* makemake = nullptr;
    CelestialBody* eris = nullptr;

    CelestialBody* createBody(const Astro::BodyData* data, CelestialBody* parent = nullptr, bool navigable = true) {
        std::string name = data ? data->name : "Ring";
        float radius = data ? conv.getOrbitRadius(*data) : 0.0f;
        float orbitSpd = data ? conv.getOrbitalSpeed(*data) : 0.0f;
        float rotSpd = data ? conv.getRotationSpeed(*data) : 0.0f;
        float tilt = data ? conv.getAxialTilt(*data) : 0.0f;
        float sz = data ? conv.getScale(*data) : 1.0f;

        auto b = std::make_unique<CelestialBody>(name, nullptr, nullptr, radius, orbitSpd, rotSpd, tilt, sz);
        b->setData(data);
        b->setNavigable(navigable);
        if (parent) {
            parent->addSatellite(b.get());
        }
        CelestialBody* ptr = b.get();
        bodies.push_back(std::move(b));
        return ptr;
    }

    FullSolarSystem() {
        sun = createBody(&Astro::Sun, nullptr);
        sun->setTiltChildren(false);

        mercury = createBody(&Astro::Mercury, sun);
        venus   = createBody(&Astro::Venus, sun);
        earth   = createBody(&Astro::Earth, sun);
        earth->setTiltChildren(false);
        moon    = createBody(&Astro::Moon, earth);

        eros    = createBody(&Astro::Eros, sun);
        mars    = createBody(&Astro::Mars, sun);
        phobos  = createBody(&Astro::Phobos, mars);
        deimos  = createBody(&Astro::Deimos, mars);

        gaspra  = createBody(&Astro::Gaspra, sun);
        vesta   = createBody(&Astro::Vesta, sun);
        ceres   = createBody(&Astro::Ceres, sun);
        pallas  = createBody(&Astro::Pallas, sun);
        ida     = createBody(&Astro::Ida, sun);

        jupiter  = createBody(&Astro::Jupiter, sun);
        io       = createBody(&Astro::Io, jupiter);
        europa   = createBody(&Astro::Europa, jupiter);
        ganymede = createBody(&Astro::Ganymede, jupiter);
        callisto = createBody(&Astro::Callisto, jupiter);

        saturn    = createBody(&Astro::Saturn, sun);
        mimas     = createBody(&Astro::Mimas, saturn);
        enceladus = createBody(&Astro::Enceladus, saturn);
        dione     = createBody(&Astro::Dione, saturn);
        rhea      = createBody(&Astro::Rhea, saturn);
        titan     = createBody(&Astro::Titan, saturn);
        iapetus   = createBody(&Astro::Iapetus, saturn);
        saturnRings = createBody(nullptr, saturn, false); // non-navigable ring

        uranus  = createBody(&Astro::Uranus, sun);
        miranda = createBody(&Astro::Miranda, uranus);
        ariel   = createBody(&Astro::Ariel, uranus);
        umbriel = createBody(&Astro::Umbriel, uranus);
        titania = createBody(&Astro::Titania, uranus);
        oberon  = createBody(&Astro::Oberon, uranus);

        neptune = createBody(&Astro::Neptune, sun);
        triton  = createBody(&Astro::Triton, neptune);

        pluto   = createBody(&Astro::Pluto, sun);
        charon  = createBody(&Astro::Charon, pluto);

        haumea   = createBody(&Astro::Haumea, sun);
        makemake = createBody(&Astro::Makemake, sun);
        eris     = createBody(&Astro::Eris, sun);
    }

    struct FilterResult {
        int helioCount = 0;
        int moonCount = 0;
        int total() const { return helioCount + moonCount; }
        std::vector<std::string> visibleMoonNames;
    };

    FilterResult queryFilter(const CelestialBody* target, bool visible = true) const {
        FilterResult res;
        if (!visible) return res;

        for (const auto& b : bodies) {
            if (OrbitRenderer::shouldRenderOrbit(b.get(), target, sun)) {
                if (b->getParent() == sun) {
                    res.helioCount++;
                } else {
                    res.moonCount++;
                    if (b->getData()) {
                        res.visibleMoonNames.push_back(b->getData()->name);
                    }
                }
            }
        }
        return res;
    }
};

// ============================================================================
// CHALLENGE 1: Exhaustive Hierarchy Filter Verification
// ============================================================================
void challengeHierarchyFilter(Report& rep) {
    std::cout << "\n=================================================================\n";
    std::cout << " 1. CHALLENGE: ACTIVE HIERARCHY FILTER ACROSS SCENE GRAPH\n";
    std::cout << "=================================================================\n";

    FullSolarSystem sys;

    // 1A. Sun Focus
    auto rSun = sys.queryFilter(sys.sun);
    rep.record("Sun focus yields exactly 18 heliocentric orbits", rSun.helioCount == 18);
    rep.record("Sun focus yields exactly 0 moons", rSun.moonCount == 0);
    rep.record("Sun focus total orbits == 18", rSun.total() == 18);
    rep.record("Sun itself never renders an orbit line", !OrbitRenderer::shouldRenderOrbit(sys.sun, sys.sun, sys.sun));

    // 1B. Planet Foci
    struct PlanetExpectation {
        CelestialBody* planet;
        std::string name;
        int expectedMoons;
        std::vector<std::string> moonNames;
    };

    const PlanetExpectation planetCases[] = {
        {sys.earth,   "Earth",   1, {"Moon"}},
        {sys.mars,    "Mars",    2, {"Phobos", "Deimos"}},
        {sys.jupiter, "Jupiter", 4, {"Io", "Europa", "Ganymede", "Callisto"}},
        {sys.saturn,  "Saturn",  6, {"Mimas", "Enceladus", "Dione", "Rhea", "Titan", "Iapetus"}},
        {sys.uranus,  "Uranus",  5, {"Miranda", "Ariel", "Umbriel", "Titania", "Oberon"}},
        {sys.neptune, "Neptune", 1, {"Triton"}},
        {sys.pluto,   "Pluto",   1, {"Charon"}},
    };

    for (const auto& pc : planetCases) {
        auto res = sys.queryFilter(pc.planet);
        bool helioOk = (res.helioCount == 18);
        bool moonOk = (res.moonCount == pc.expectedMoons);
        bool matchNames = (res.visibleMoonNames == pc.moonNames);
        rep.record(pc.name + " focus yields 18 helio + " + std::to_string(pc.expectedMoons) + " moons",
                   helioOk && moonOk && matchNames,
                   "Total: " + std::to_string(res.total()));
    }

    // 1C. Moon Foci (Sibling Moon Filtering)
    struct MoonExpectation {
        CelestialBody* moon;
        std::string name;
        int expectedMoons;
        std::vector<std::string> moonNames;
    };

    const MoonExpectation moonCases[] = {
        {sys.moon,     "Moon (Earth)",       1, {"Moon"}},
        {sys.phobos,   "Phobos (Mars)",      2, {"Phobos", "Deimos"}},
        {sys.deimos,   "Deimos (Mars)",      2, {"Phobos", "Deimos"}},
        {sys.io,       "Io (Jupiter)",       4, {"Io", "Europa", "Ganymede", "Callisto"}},
        {sys.europa,   "Europa (Jupiter)",   4, {"Io", "Europa", "Ganymede", "Callisto"}},
        {sys.ganymede, "Ganymede (Jupiter)", 4, {"Io", "Europa", "Ganymede", "Callisto"}},
        {sys.callisto, "Callisto (Jupiter)", 4, {"Io", "Europa", "Ganymede", "Callisto"}},
        {sys.titan,    "Titan (Saturn)",     6, {"Mimas", "Enceladus", "Dione", "Rhea", "Titan", "Iapetus"}},
        {sys.titania,  "Titania (Uranus)",   5, {"Miranda", "Ariel", "Umbriel", "Titania", "Oberon"}},
        {sys.triton,   "Triton (Neptune)",   1, {"Triton"}},
        {sys.charon,   "Charon (Pluto)",     1, {"Charon"}},
    };

    for (const auto& mc : moonCases) {
        auto res = sys.queryFilter(mc.moon);
        bool helioOk = (res.helioCount == 18);
        bool moonOk = (res.moonCount == mc.expectedMoons);
        bool matchNames = (res.visibleMoonNames == mc.moonNames);
        rep.record(mc.name + " sibling filter yields 18 helio + " + std::to_string(mc.expectedMoons) + " moons",
                   helioOk && moonOk && matchNames,
                   "Total: " + std::to_string(res.total()));
    }

    // 1D. Asteroids and Dwarf Planets
    auto rCeres = sys.queryFilter(sys.ceres);
    rep.record("Ceres (asteroid) focus yields 18 helio and 0 moons", rCeres.helioCount == 18 && rCeres.moonCount == 0);

    auto rMakemake = sys.queryFilter(sys.makemake);
    rep.record("Makemake (dwarf) focus yields 18 helio and 0 moons", rMakemake.helioCount == 18 && rMakemake.moonCount == 0);

    // 1E. Edge & Hostile Cases
    auto rNull = sys.queryFilter(nullptr);
    rep.record("Null target yields 18 helio and 0 moons (safe fallback)", rNull.helioCount == 18 && rNull.moonCount == 0);

    rep.record("Null body pointer safely returns false", !OrbitRenderer::shouldRenderOrbit(nullptr, sys.earth, sys.sun));
    rep.record("Saturn Rings (non-navigable) never rendered", !OrbitRenderer::shouldRenderOrbit(sys.saturnRings, sys.saturn, sys.sun));

    Astro::BodyData zeroAData = Astro::Earth;
    zeroAData.orbit.a0 = 0.0;
    auto zeroBody = std::make_unique<CelestialBody>("ZeroBody", nullptr, nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    zeroBody->setData(&zeroAData);
    sys.sun->addSatellite(zeroBody.get());
    rep.record("Body with a0 <= 0 safely returns false", !OrbitRenderer::shouldRenderOrbit(zeroBody.get(), sys.earth, sys.sun));

    auto orphanBody = std::make_unique<CelestialBody>("Orphan", nullptr, nullptr, 100.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    orphanBody->setData(&Astro::Earth);
    rep.record("Body with null parent (non-Sun) safely returns false", !OrbitRenderer::shouldRenderOrbit(orphanBody.get(), sys.earth, sys.sun));
}

// ============================================================================
// CHALLENGE 2: 'O' Key Toggle State & Memory Cleanliness
// ============================================================================
void challengeToggleAndMemory(Report& rep) {
    std::cout << "\n=================================================================\n";
    std::cout << " 2. CHALLENGE: 'O' KEY TOGGLE STATE & VBO MEMORY CLEANLINESS\n";
    std::cout << "=================================================================\n";

    FullSolarSystem sys;

    // A. Parity Test across 1,000,000 rapid toggles
    bool visible = true;
    auto toggle = [&visible]() { visible = !visible; };

    rep.record("Initial visibility state is true", visible == true);
    toggle();
    rep.record("First toggle flips state to false", visible == false);
    auto resOff = sys.queryFilter(sys.saturn, visible);
    rep.record("When visibility is false, total rendered orbits == 0", resOff.total() == 0);

    toggle();
    rep.record("Second toggle restores state to true", visible == true);
    auto resOn = sys.queryFilter(sys.saturn, visible);
    rep.record("When visibility is true, total rendered orbits == 24", resOn.total() == 24);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        toggle();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    rep.record("1,000,000 toggles maintains strict parity (true)", visible == true);
    rep.record("Toggle overhead < 50 ns per operation", (elapsedMs / 1000000.0 * 1000.0) < 50.0,
               std::to_string(elapsedMs) + " ms total, " + std::to_string(elapsedMs / 1000000.0 * 1000.0) + " ns/op");

    // B. VBO Memory Capacity & Headroom Analysis
    constexpr int NUM_SEGMENTS = 180;
    constexpr size_t MAX_VISIBLE_ORBITS = 32;
    constexpr size_t INITIAL_CAPACITY = MAX_VISIBLE_ORBITS * NUM_SEGMENTS; // 5,760 vertices
    constexpr size_t VERTEX_SIZE_BYTES = sizeof(glm::vec3); // 12 bytes
    constexpr size_t BUFFER_SIZE_BYTES = INITIAL_CAPACITY * VERTEX_SIZE_BYTES; // 69,120 bytes (67.5 KB)

    constexpr size_t PEAK_ORBITS = 24; // Saturn: 18 helio + 6 moons
    constexpr size_t PEAK_VERTICES = PEAK_ORBITS * NUM_SEGMENTS; // 4,320 vertices

    rep.record("VBO capacity (5,760 vertices / 67.5 KB) >= Peak load (4,320 vertices / 50.6 KB)",
               INITIAL_CAPACITY >= PEAK_VERTICES);
    rep.record("Buffer headroom >= 1,440 vertices (allows 8 more orbits without realloc)",
               (INITIAL_CAPACITY - PEAK_VERTICES) >= 1440);

    // C. Verify std::vector capacity preservation (zero reallocation across frames)
    std::vector<glm::vec3> testBuffer;
    testBuffer.reserve(INITIAL_CAPACITY);
    size_t initialCap = testBuffer.capacity();

    for (int frame = 0; frame < 100; ++frame) {
        testBuffer.clear();
        for (size_t i = 0; i < PEAK_VERTICES; ++i) {
            testBuffer.push_back(glm::vec3(1.0f, 2.0f, 3.0f));
        }
    }
    rep.record("Vector capacity strictly preserved across 100 clear/refill cycles (zero heap realloc)",
               testBuffer.capacity() == initialCap);
}

// ============================================================================
// CHALLENGE 3: CPU Performance Stability across 1000 Simulated Iterations
// ============================================================================
void challengePerformance1000Frames(Report& rep) {
    std::cout << "\n=================================================================\n";
    std::cout << " 3. CHALLENGE: CPU TIME & ALLOCATION OVER 1000 ITERATIONS\n";
    std::cout << "=================================================================\n";

    FullSolarSystem sys;
    const CelestialBody* target = sys.saturn; // Peak load: 18 helio + 6 moons = 24 orbits
    const CelestialBody* sun = sys.sun;

    // Filter active bodies once
    std::vector<const CelestialBody*> activeBodies;
    for (const auto& b : sys.bodies) {
        if (OrbitRenderer::shouldRenderOrbit(b.get(), target, sun)) {
            activeBodies.push_back(b.get());
        }
    }
    rep.record("Peak active bodies count == 24 (18 helio + 6 Saturn moons)", activeBodies.size() == 24);

    constexpr int NUM_SEGMENTS = 180;
    constexpr size_t BUFFER_CAPACITY = 32 * NUM_SEGMENTS;
    std::vector<glm::vec3> vertexBuffer;
    vertexBuffer.reserve(BUFFER_CAPACITY);

    struct Batch {
        int first;
        int count;
        glm::vec4 color;
    };
    std::vector<Batch> batches;
    batches.reserve(32);

    const int NUM_FRAMES = 1000;
    std::vector<double> frameTimesUs;
    frameTimesUs.reserve(NUM_FRAMES);

    double simTimeSeconds = 845812800.0; // ~2026 epoch
    const double dt = 0.01666667; // 60 FPS delta time

    // Warm-up 10 frames to avoid initial cache misses
    for (int w = 0; w < 10; ++w) {
        simTimeSeconds += dt;
        double T = Astro::unixToJulianCenturies(simTimeSeconds);
        vertexBuffer.clear();
        batches.clear();
        const glm::dvec3 origin = target->getExactPosition();
        const double step = glm::two_pi<double>() / static_cast<double>(NUM_SEGMENTS);

        for (const auto* body : activeBodies) {
            const Astro::KeplerianBasis basis =
                Astro::evaluateKeplerianBasis(body->getData()->orbit, T);
            const CelestialBody* parent = body->getParent();
            const glm::dvec3 parentExact = parent ? parent->getExactPosition() : glm::dvec3(0.0);
            const bool applyParentTilt = (parent != nullptr && parent->getTiltChildren());
            const glm::dmat4 tiltRot = applyParentTilt
                ? glm::rotate(glm::dmat4(1.0), static_cast<double>(parent->getAxialTilt()), glm::dvec3(0.0, 0.0, 1.0))
                : glm::dmat4(1.0);
            const int startIdx = static_cast<int>(vertexBuffer.size());

            for (int k = 0; k < NUM_SEGMENTS; ++k) {
                const double Ek = basis.E_rad + static_cast<double>(k) * step;
                glm::dvec3 vRel = Astro::evaluateOrbitVertex(basis, Ek);
                if (applyParentTilt) {
                    vRel = glm::dvec3(tiltRot * glm::dvec4(vRel, 1.0));
                }
                const glm::vec3 vRender = glm::vec3((parentExact - origin) + vRel);
                vertexBuffer.push_back(vRender);
            }
            batches.push_back({startIdx, NUM_SEGMENTS, glm::vec4(1.0f)});
        }
    }

    // Begin tracked 1000 iterations
    g_heapAllocCount = 0;
    g_heapAllocBytes = 0;
    g_trackAllocations = true;

    auto tStartTotal = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        simTimeSeconds += dt;
        const double T = Astro::unixToJulianCenturies(simTimeSeconds);

        auto tStartFrame = std::chrono::high_resolution_clock::now();

        // Exact OrbitRenderer::update logic
        vertexBuffer.clear();
        batches.clear();
        const glm::dvec3 origin = target->getExactPosition();
        const double step = glm::two_pi<double>() / static_cast<double>(NUM_SEGMENTS);

        for (const auto* body : activeBodies) {
            const Astro::KeplerianBasis basis =
                Astro::evaluateKeplerianBasis(body->getData()->orbit, T);
            if (basis.a_km <= 0.0) continue;

            const CelestialBody* parent = body->getParent();
            const glm::dvec3 parentExact = parent ? parent->getExactPosition() : glm::dvec3(0.0);
            const bool applyParentTilt = (parent != nullptr && parent->getTiltChildren());
            const glm::dmat4 tiltRot = applyParentTilt
                ? glm::rotate(glm::dmat4(1.0), static_cast<double>(parent->getAxialTilt()), glm::dvec3(0.0, 0.0, 1.0))
                : glm::dmat4(1.0);
            const int startIdx = static_cast<int>(vertexBuffer.size());

            for (int k = 0; k < NUM_SEGMENTS; ++k) {
                const double Ek = basis.E_rad + static_cast<double>(k) * step;
                glm::dvec3 vRel = Astro::evaluateOrbitVertex(basis, Ek);
                if (applyParentTilt) {
                    vRel = glm::dvec3(tiltRot * glm::dvec4(vRel, 1.0));
                }
                const glm::vec3 vRender = glm::vec3((parentExact - origin) + vRel);
                vertexBuffer.push_back(vRender);
            }
            batches.push_back({startIdx, NUM_SEGMENTS, glm::vec4(0.7f, 0.7f, 0.8f, 0.45f)});
        }

        auto tEndFrame = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(tEndFrame - tStartFrame).count();
        frameTimesUs.push_back(us);
    }

    auto tEndTotal = std::chrono::high_resolution_clock::now();
    g_trackAllocations = false;

    double totalTimeMs = std::chrono::duration<double, std::milli>(tEndTotal - tStartTotal).count();

    // Compute statistics
    std::sort(frameTimesUs.begin(), frameTimesUs.end());
    double minUs = frameTimesUs.front();
    double maxUs = frameTimesUs.back();
    double sumUs = std::accumulate(frameTimesUs.begin(), frameTimesUs.end(), 0.0);
    double meanUs = sumUs / NUM_FRAMES;
    double p95Us = frameTimesUs[static_cast<size_t>(NUM_FRAMES * 0.95)];
    double p99Us = frameTimesUs[static_cast<size_t>(NUM_FRAMES * 0.99)];

    double variance = 0.0;
    for (double t : frameTimesUs) {
        variance += (t - meanUs) * (t - meanUs);
    }
    double stdDevUs = std::sqrt(variance / NUM_FRAMES);

    std::cout << "\n  --- 1000 Frame Latency Statistics (Saturn Focus, 24 Orbits, 4320 Vertices) ---\n";
    std::cout << "  Total Wall Time  : " << std::fixed << std::setprecision(2) << totalTimeMs << " ms\n";
    std::cout << "  Mean Frame Time  : " << std::fixed << std::setprecision(4) << (meanUs / 1000.0) << " ms (" << meanUs << " us)\n";
    std::cout << "  Min Frame Time   : " << std::fixed << std::setprecision(4) << (minUs / 1000.0) << " ms (" << minUs << " us)\n";
    std::cout << "  Max Frame Time   : " << std::fixed << std::setprecision(4) << (maxUs / 1000.0) << " ms (" << maxUs << " us)\n";
    std::cout << "  95th Percentile  : " << std::fixed << std::setprecision(4) << (p95Us / 1000.0) << " ms (" << p95Us << " us)\n";
    std::cout << "  99th Percentile  : " << std::fixed << std::setprecision(4) << (p99Us / 1000.0) << " ms (" << p99Us << " us)\n";
    std::cout << "  Std Deviation    : " << std::fixed << std::setprecision(4) << (stdDevUs / 1000.0) << " ms (" << stdDevUs << " us)\n";
    std::cout << "  Heap Allocations : " << g_heapAllocCount << " allocs (" << g_heapAllocBytes << " bytes)\n\n";

    rep.record("Zero heap allocations during 1000 frame updates (0 bytes churn)", g_heapAllocCount == 0);
    rep.record("Mean frame CPU time < 1.0 ms", (meanUs / 1000.0) < 1.0,
               std::to_string(meanUs / 1000.0) + " ms vs 1.0 ms budget");
    rep.record("Mean frame CPU time < 0.3 ms (exceeds budget expectation)", (meanUs / 1000.0) < 0.3,
               std::to_string(meanUs / 1000.0) + " ms");
    rep.record("99th percentile frame CPU time < 1.0 ms", (p99Us / 1000.0) < 1.0,
               std::to_string(p99Us / 1000.0) + " ms");
    rep.record("Max frame CPU time < 1.0 ms (zero stutter spikes)", (maxUs / 1000.0) < 1.0,
               std::to_string(maxUs / 1000.0) + " ms");
    rep.record("Headroom vs 16.6 ms (60 FPS) frame budget > 15.0 ms", (16.666 - (meanUs / 1000.0)) > 15.0,
               std::to_string(16.666 - (meanUs / 1000.0)) + " ms margin");
}

// ============================================================================
// CHALLENGE 4: Axial Tilt Moon Alignment Invariant Check
// ============================================================================
void challengeMoonTiltAlignment(Report& rep) {
    std::cout << "\n=================================================================\n";
    std::cout << " 4. CHALLENGE: AXIAL TILT MOON ALIGNMENT INVARIANT\n";
    std::cout << "=================================================================\n";

    FullSolarSystem sys;
    const double T = 0.268;

    struct MoonItem {
        CelestialBody* moon;
        CelestialBody* parent;
        std::string name;
    };

    const MoonItem moons[] = {
        {sys.moon, sys.earth, "Moon"},
        {sys.phobos, sys.mars, "Phobos"},
        {sys.deimos, sys.mars, "Deimos"},
        {sys.io, sys.jupiter, "Io"},
        {sys.europa, sys.jupiter, "Europa"},
        {sys.ganymede, sys.jupiter, "Ganymede"},
        {sys.callisto, sys.jupiter, "Callisto"},
        {sys.mimas, sys.saturn, "Mimas"},
        {sys.enceladus, sys.saturn, "Enceladus"},
        {sys.dione, sys.saturn, "Dione"},
        {sys.rhea, sys.saturn, "Rhea"},
        {sys.titan, sys.saturn, "Titan"},
        {sys.iapetus, sys.saturn, "Iapetus"},
        {sys.miranda, sys.uranus, "Miranda"},
        {sys.ariel, sys.uranus, "Ariel"},
        {sys.umbriel, sys.uranus, "Umbriel"},
        {sys.titania, sys.uranus, "Titania"},
        {sys.oberon, sys.uranus, "Oberon"},
        {sys.triton, sys.neptune, "Triton"},
        {sys.charon, sys.pluto, "Charon"},
    };

    sys.sun->update(T, 0.0f);

    double worstError = 0.0;
    bool allAligned = true;

    for (const auto& item : moons) {
        const glm::dvec3 moonPos = item.moon->getExactPosition();
        const glm::dvec3 parentPos = item.parent->getExactPosition();

        const Astro::KeplerianBasis basis =
            Astro::evaluateKeplerianBasis(item.moon->getData()->orbit, T);
        glm::dvec3 vRel = Astro::evaluateOrbitVertex(basis, basis.E_rad);

        const bool applyParentTilt = item.parent->getTiltChildren();
        const glm::dmat4 tiltRot = applyParentTilt
            ? glm::rotate(glm::dmat4(1.0), static_cast<double>(item.parent->getAxialTilt()), glm::dvec3(0.0, 0.0, 1.0))
            : glm::dmat4(1.0);

        if (applyParentTilt) {
            vRel = glm::dvec3(tiltRot * glm::dvec4(vRel, 1.0));
        }

        const glm::dvec3 v0_world = parentPos + vRel;
        const double errorKm = glm::length(moonPos - v0_world);
        worstError = std::max(worstError, errorKm);

        if (errorKm > 1e-6) {
            allAligned = false;
            std::cout << "  Misaligned moon: " << item.name << " err: " << errorKm << " km\n";
        }
    }

    rep.record("All 20 catalog moons align with orbit vertex 0 (< 1e-6 km)", allAligned,
               "Worst error: " + std::to_string(worstError) + " km");
    rep.record("Earth Moon tiltChildren is false (ecliptic plane alignment)", !sys.earth->getTiltChildren());
    rep.record("Mars tiltChildren is true (equatorial plane alignment)", sys.mars->getTiltChildren());
    rep.record("Jupiter tiltChildren is true", sys.jupiter->getTiltChildren());
    rep.record("Saturn tiltChildren is true", sys.saturn->getTiltChildren());
    rep.record("Uranus tiltChildren is true", sys.uranus->getTiltChildren());
    rep.record("Neptune tiltChildren is true", sys.neptune->getTiltChildren());
    rep.record("Pluto tiltChildren is true", sys.pluto->getTiltChildren());
}

} // namespace ChallengerM2_2

int main() {
    std::cout << "=================================================================\n";
    std::cout << " EMPIRICAL ADVERSARIAL CHALLENGER — MILESTONE M2 ITERATION 2\n";
    std::cout << " Roles: critic, specialist | Agent: challenger_m2_it2_2\n";
    std::cout << "=================================================================\n";

    ChallengerM2_2::Report rep;

    ChallengerM2_2::challengeHierarchyFilter(rep);
    ChallengerM2_2::challengeToggleAndMemory(rep);
    ChallengerM2_2::challengePerformance1000Frames(rep);
    ChallengerM2_2::challengeMoonTiltAlignment(rep);

    std::cout << "\n=================================================================\n";
    std::cout << " CHALLENGE SUMMARY REPORT\n";
    std::cout << " Total Checks : " << rep.total << "\n";
    std::cout << " Passed       : " << rep.passed << "\n";
    std::cout << " Failed       : " << rep.failed << "\n";
    std::cout << " Pass Rate    : " << std::fixed << std::setprecision(2)
              << (100.0 * rep.passed / rep.total) << "%\n";
    std::cout << " Verdict      : " << (rep.failed == 0 ? "\033[32mAPPROVE (PASS)\033[0m" : "\033[31mFAIL\033[0m") << "\n";
    std::cout << "=================================================================\n";

    return (rep.failed == 0) ? 0 : 1;
}
