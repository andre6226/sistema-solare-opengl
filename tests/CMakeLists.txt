cmake_minimum_required(VERSION 3.5...4.0)
project(SolarSystem_E2E_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# Locate GLM and SFML headers
find_path(GLM_INCLUDE_DIR "glm/glm.hpp"
    PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/glm-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/_deps/glm-src"
    NO_DEFAULT_PATH
)
find_path(SFML_INCLUDE_DIR "SFML/Window.hpp"
    PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/sfml-src/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/_deps/sfml-src/include"
    NO_DEFAULT_PATH
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
    $<$<CXX_COMPILER_ID:MSVC>: /W3 /O2 /utf-8 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX>
)

add_test(NAME E2E_Acceptance_Suite COMMAND e2e_test_runner WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(test_time_picker test_time_picker.cpp)
target_include_directories(test_time_picker PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${GLM_INCLUDE_DIR}"
    "${SFML_INCLUDE_DIR}"
)
target_compile_definitions(test_time_picker PRIVATE SFML_STATIC)
target_compile_options(test_time_picker PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -O2>
    $<$<CXX_COMPILER_ID:MSVC>: /W3 /O2 /utf-8 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX>
)
add_test(NAME Time_Picker_And_Ephemeris_Suite COMMAND test_time_picker WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
