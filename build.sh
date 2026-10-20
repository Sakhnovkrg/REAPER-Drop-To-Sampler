#!/bin/sh
# macOS build (universal arm64 + x86_64).
# Usage:
#   ./build.sh           configure + build
#   ./build.sh deploy    build, copy the dylib to REAPER's UserPlugins
# Requires Xcode command line tools and CMake. Version: DTS_VERSION=x.y.z ./build.sh
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build/macos"
DYLIB="reaper_drop_to_sampler.dylib"
PLUGIN_DIR="${PLUGIN_DIR:-$HOME/Library/Application Support/REAPER/UserPlugins}"

if [ ! -f "$ROOT/vendor/WDL/WDL/swell/swell.h" ]; then
    echo "=== Fetching WDL submodule ==="
    git -C "$ROOT" submodule update --init
fi

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE:-Release}"
if [ -n "$DTS_VERSION" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DDTS_VERSION=$DTS_VERSION"
fi

echo "=== Configure ==="
cmake -S "$ROOT" -B "$BUILD_DIR" $CMAKE_ARGS

echo "=== Build ==="
cmake --build "$BUILD_DIR" --parallel
echo "Built: $BUILD_DIR/$DYLIB"

if [ "$1" = "deploy" ]; then
    echo "=== Copy to UserPlugins ==="
    mkdir -p "$PLUGIN_DIR"
    cp "$BUILD_DIR/$DYLIB" "$PLUGIN_DIR/$DYLIB"
    echo "Copied to: $PLUGIN_DIR/$DYLIB (restart REAPER)"
fi
