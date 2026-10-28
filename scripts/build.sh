#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
CONFIG="${CONFIG:-Release}"

"$ROOT/scripts/setup-linux.sh"

if [ -d "$ROOT/deps/sysroot/usr" ]; then
    export PKG_CONFIG_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ROOT/deps/sysroot/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}"
    export PKG_CONFIG_SYSROOT_DIR="$ROOT/deps/sysroot"
    CMAKE_EXTRA_ARGS=(
        "-DCMAKE_INCLUDE_PATH=$ROOT/deps/sysroot/usr/include"
        "-DCMAKE_LIBRARY_PATH=$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu"
    )
else
    CMAKE_EXTRA_ARGS=()
fi

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DGEVERB_BUILD_TESTS=ON \
    -DGEVERB_BUILD_SCREENSHOT=ON \
    -DGEVERB_BUILD_HOST_TEST=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

if [ -z "${JOBS:-}" ]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

VST3="$BUILD_DIR/Geverb_artefacts/$CONFIG/VST3/Geverb.vst3"
STANDALONE="$BUILD_DIR/Geverb_artefacts/$CONFIG/Standalone/Geverb"

mkdir -p "$ROOT/dist"

if [ -d "$VST3" ]; then
    rm -rf "$ROOT/dist/Geverb.vst3"
    cp -a "$VST3" "$ROOT/dist/"
fi

if [ -x "$STANDALONE" ]; then
    cp -a "$STANDALONE" "$ROOT/dist/Geverb"
fi

echo
echo "Built artefacts:"
[ -d "$ROOT/dist/Geverb.vst3" ] && echo "  VST3        $ROOT/dist/Geverb.vst3"
[ -x "$ROOT/dist/Geverb" ] && echo "  Standalone  $ROOT/dist/Geverb"
echo "  Tests       $BUILD_DIR/GeverbTests_artefacts/$CONFIG/GeverbTests"

if [ -x "$BUILD_DIR/GeverbHostSmokeTest_artefacts/$CONFIG/GeverbHostSmokeTest" ]; then
    echo "  Host test   $BUILD_DIR/GeverbHostSmokeTest_artefacts/$CONFIG/GeverbHostSmokeTest"
fi

if [ -x "$BUILD_DIR/GeverbScreenshot_artefacts/$CONFIG/Geverb Screenshot" ]; then
    echo "  Screenshot  $BUILD_DIR/GeverbScreenshot_artefacts/$CONFIG/Geverb Screenshot"
fi
