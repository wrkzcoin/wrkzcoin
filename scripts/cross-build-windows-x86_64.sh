#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-windows-x86_64}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
GENERATOR="${GENERATOR:-Ninja}"
SKIP_PREP="${SKIP_PREP:-0}"

if [ "$SKIP_PREP" != "1" ]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/prep-windows-x86_64.sh"
fi

if ! command -v ninja >/dev/null 2>&1 && [ "$GENERATOR" = "Ninja" ]; then
  GENERATOR="Unix Makefiles"
fi

echo "Configuring cross-build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-windows-x86_64.cmake" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DARCH=default \
  -DCONSENSUS_SAFE_BUILD=ON \
  -DSTATIC=true

echo "Building targets ..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Cross-build complete."
echo "Binaries: $BUILD_DIR/src"

