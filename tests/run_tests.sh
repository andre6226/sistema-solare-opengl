#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$ROOT_DIR"

mkdir -p build

echo "================================================================="
echo " Compiling E2E Acceptance Test Runner..."
echo "================================================================="

# Compile with g++ using C++17 and vendored GLM headers
g++ -std=c++17 -O2 \
    -I"$ROOT_DIR" \
    -I"$ROOT_DIR/build/_deps/glm-src" \
    "$ROOT_DIR/tests/test_tier1_units.cpp" \
    "$ROOT_DIR/tests/test_tier2_edges.cpp" \
    "$ROOT_DIR/tests/test_tier3_pairwise.cpp" \
    "$ROOT_DIR/tests/test_tier4_scenarios.cpp" \
    "$ROOT_DIR/tests/test_main.cpp" \
    -o "$ROOT_DIR/build/e2e_test_runner"

echo "Build successful -> $ROOT_DIR/build/e2e_test_runner"
echo ""
echo "================================================================="
echo " Running E2E Acceptance Test Suite..."
echo "================================================================="
"$ROOT_DIR/build/e2e_test_runner"

echo ""
echo "================================================================="
echo " Compiling & Running Date/Time Picker Tests..."
echo "================================================================="
g++ -std=c++17 -O2 -Wall -Wextra -DSFML_STATIC \
    -I"$ROOT_DIR" \
    -I"$ROOT_DIR/build/_deps/glm-src" \
    -I"$ROOT_DIR/build/_deps/sfml-src/include" \
    "$ROOT_DIR/tests/test_time_picker.cpp" \
    -o "$ROOT_DIR/build/test_time_picker" \
    -L"$ROOT_DIR/build/_deps/sfml-build/lib" -lsfml-graphics -lsfml-window -lsfml-system -lGL
"$ROOT_DIR/build/test_time_picker"

echo ""
echo "================================================================="
echo " Visual Regression (screenshots vs tests/visual/ref)..."
echo "================================================================="
if [ ! -x "$ROOT_DIR/build/SolarSystem.bin" ]; then
    echo "SKIP: build/SolarSystem.bin not built"
elif ! command -v xvfb-run > /dev/null && [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "SKIP: no display and no xvfb-run"
else
    g++ -std=c++17 -O2 -Wall -Wextra -DSFML_STATIC \
        -I"$ROOT_DIR/build/_deps/sfml-src/include" \
        "$ROOT_DIR/tests/visual/compare_png.cpp" \
        -o "$ROOT_DIR/build/compare_png" \
        -L"$ROOT_DIR/build/_deps/sfml-build/lib" -lsfml-graphics-s -lsfml-system-s
    "$ROOT_DIR/tests/visual/render_refs.sh" "$ROOT_DIR/build/visual" > /dev/null
    # References come from xvfb (llvmpipe); a GPU render differs by ~0.5-2, another scene by ~20.
    for ref in "$ROOT_DIR"/tests/visual/ref/*.png; do
        printf '%-24s ' "$(basename "$ref" .png)"
        "$ROOT_DIR/build/compare_png" "$ref" "$ROOT_DIR/build/visual/$(basename "$ref")"
    done
    echo ">>> VISUAL REGRESSION PASSED <<<"
fi
