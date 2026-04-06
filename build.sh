#!/bin/bash
# macOS / Linux build script

set -e

BUILD_TYPE="${1:-debug}"
cd "$(dirname "${BASH_SOURCE[0]}")"

case "$(uname -s)" in
    Darwin)
        PRESET="macos-$BUILD_TYPE"
        ;;
    Linux)
        PRESET="linux-$BUILD_TYPE"
        ;;
    *)
        echo "Use build.bat on Windows"
        exit 1
        ;;
esac

echo "=== Build ($PRESET) ==="

cmake --preset "$PRESET"
cmake --build build --parallel

echo "=== Build complete ==="
