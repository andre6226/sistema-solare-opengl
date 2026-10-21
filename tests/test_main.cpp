#include "test_infra.hpp"

namespace E2E {
    void runTier1UnitTests();
    void runTier2EdgeTests();
    void runTier3PairwiseTests();
    void runTier4ScenarioTests();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << E2E::Color::Bold << E2E::Color::Cyan
              << "================================================================================" << E2E::Color::Reset << "\n";
    std::cout << E2E::Color::Bold
              << "   SOLAR SYSTEM SIMULATOR — FULL ACCEPTANCE & VERIFICATION TEST RUNNER          " << E2E::Color::Reset << "\n";
    std::cout << E2E::Color::Bold << E2E::Color::Cyan
              << "================================================================================" << E2E::Color::Reset << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    // 1. Run Tier 1: Unit & Mathematical Invariants
    std::cout << "\n" << E2E::Color::Bold << "[*] Executing Tier 1: Unit & Mathematical Invariant Tests..." << E2E::Color::Reset << "\n";
    E2E::runTier1UnitTests();

    // 2. Run Tier 2: Boundary, Edge & Adversarial Conditions
    std::cout << E2E::Color::Bold << "[*] Executing Tier 2: Boundary, Edge & Adversarial Tests..." << E2E::Color::Reset << "\n";
    E2E::runTier2EdgeTests();

    // 3. Run Tier 3: Combinatorial & Pairwise Integration Tests
    std::cout << E2E::Color::Bold << "[*] Executing Tier 3: Pairwise Combinatorial Tests..." << E2E::Color::Reset << "\n";
    E2E::runTier3PairwiseTests();

    // 4. Run Tier 4: Real-World Astronomical Scenarios
    std::cout << E2E::Color::Bold << "[*] Executing Tier 4: Real-World Astronomical Scenarios..." << E2E::Color::Reset << "\n";
    E2E::runTier4ScenarioTests();

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalDuration = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Summary Reporting
    E2E::TestRegistry::instance().printSummary();
    std::cout << "Execution completed in: " << std::fixed << std::setprecision(2) << totalDuration << " ms\n\n";

    return (E2E::TestRegistry::instance().totalFailed() == 0) ? 0 : 1;
}
