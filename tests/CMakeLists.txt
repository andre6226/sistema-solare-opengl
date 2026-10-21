cmake_minimum_required(VERSION 3.5...4.0)
project(SolarSystem_E2E_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# Locate GLM headers
find_path(GLM_INCLUDE_DIR "glm/glm.hpp"
    PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/glm-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/_deps/glm-src"
)

add_executable(e2e_test_runner
    test_tier1_units.cpp
    test_tier2_edges.cpp
    test_tier3_pairwise.cpp
    test_tier4_scenarios.cpp
    test_main.cpp
)

target_include_directories(e2e_test_runner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${GLM_INCLUDE_DIR}"
)

target_compile_options(e2e_test_runner PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -O2>
)

add_test(NAME E2E_Acceptance_Suite COMMAND e2e_test_runner)
