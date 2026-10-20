#!/bin/sh
# macOS (universal arm64 + x86_64) and Linux build.
# Usage:
#   ./build.sh           configure + build
#   ./build.sh deploy    build, copy the plugin to REAPER's UserPlugins
# Requires CMake, Perl and a C++17 compiler (Xcode command line tools on macOS).
# Version: DTS_VERSION=x.y.z ./build.sh
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
case "$(uname -s)" in
    Darwin)
        BUILD_DIR="$ROOT/build/macos"
        PLUGIN="reaper_drop_to_sampler.dylib"
        PLUGIN_DIR="${PLUGIN_DIR:-$HOME/Library/Application Support/REAPER/UserPlugins}"
        ;;
    Linux)
        BUILD_DIR="$ROOT/build/linux"
        PLUGIN="reaper_drop_to_sampler-$(uname -m).so"
        PLUGIN_DIR="${PLUGIN_DIR:-${XDG_CONFIG_HOME:-$HOME/.config}/REAPER/UserPlugins}"
        ;;
    *)
        echo "Unsupported OS: $(uname -s) (use build.bat on Windows)" >&2
        exit 1
        ;;
esac

ACTION="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
case "$ACTION" in
    ""|deploy) ;;
    *) echo "Unknown argument: $1 (expected: deploy)" >&2; exit 1 ;;
esac

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
echo "Built: $BUILD_DIR/$PLUGIN"

if [ "$ACTION" = "deploy" ]; then
    echo "=== Copy to UserPlugins ==="
    mkdir -p "$PLUGIN_DIR"
    cp "$BUILD_DIR/$PLUGIN" "$PLUGIN_DIR/$PLUGIN"
    echo "Copied to: $PLUGIN_DIR/$PLUGIN (restart REAPER)"
fi
