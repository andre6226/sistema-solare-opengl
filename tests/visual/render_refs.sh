#!/bin/bash
# Renders the reference scenes to PNG: tests/visual/render_refs.sh [OUT_DIR]
# Uses xvfb-run when installed (no window appears, same software renderer as the
# stored references), else the current display. Needs a built build/SolarSystem.bin.
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="$(realpath -m "${1:-$ROOT_DIR/build/visual}")"
mkdir -p "$OUT_DIR"

# One X server for all scenes rather than one xvfb-run per scene.
if [ -z "$RENDER_REFS_IN_XVFB" ] && command -v xvfb-run > /dev/null; then
    RENDER_REFS_IN_XVFB=1 exec xvfb-run -a -s "-screen 0 1024x768x24" "$0" "$OUT_DIR"
elif [ -z "$RENDER_REFS_IN_XVFB" ] && [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "no display and no xvfb-run: cannot render" >&2
    exit 1
fi

# name | UTC date | body | extra options (camera offsets from the Sun's direction)
SCENES=(
    "solar_eclipse_2026|2026-08-12T17:46:00|Earth|--azimuth 20 --elevation 65"   # greatest eclipse, shadow off Iceland, heading for Spain
    "lunar_partial|2026-08-28T04:13:00|Moon|"        # umbral magnitude 0.93: the sliver is in deep penumbra, so dim
    "lunar_total|2025-09-07T18:11:00|Moon|"
    "saturn_rings|2026-10-21T00:00:00|Saturn|--elevation 30"   # rings only ~8 deg open to the Sun in 2026
    "jupiter_io_shadow|2026-02-05T05:05:00|Jupiter|"  # mid shadow transit
)

cd "$ROOT_DIR/build"   # the binary loads resources/ and its shaders relative to here
for scene in "${SCENES[@]}"; do
    IFS='|' read -r name date body extra <<< "$scene"
    # shellcheck disable=SC2086  # $extra is a list of options
    ./SolarSystem.bin --date "$date" --focus "$body" $extra --screenshot "$OUT_DIR/$name.png" > /dev/null 2>&1 \
        || { echo "render failed: $name" >&2; exit 1; }
    echo "$OUT_DIR/$name.png"
done
