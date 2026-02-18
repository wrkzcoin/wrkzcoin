#!/usr/bin/env bash
set -euo pipefail

# Cross-build for macOS from Linux using osxcross.
# Usage: scripts/cross-build-macos.sh [x86_64|arm64]

TARGET_ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
GENERATOR="${GENERATOR:-Ninja}"
SKIP_PREP="${SKIP_PREP:-0}"

case "$TARGET_ARCH" in
  x86_64)
    TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-macos-x86_64.cmake"
    BUILD_DIR_DEFAULT="$REPO_ROOT/build-macos-x86_64"
    CLANG_WRAPPER="o64-clang"
    CLANGXX_WRAPPER="o64-clang++"
    ;;
  arm64)
    TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-macos-arm64.cmake"
    BUILD_DIR_DEFAULT="$REPO_ROOT/build-macos-arm64"
    CLANG_WRAPPER="oa64-clang"
    CLANGXX_WRAPPER="oa64-clang++"
    ;;
  *)
    echo "Unsupported target arch: $TARGET_ARCH"
    echo "Use: x86_64 or arm64"
    exit 1
    ;;
esac

BUILD_DIR="${BUILD_DIR:-$BUILD_DIR_DEFAULT}"

if [ "$SKIP_PREP" != "1" ]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/prep-macos-osxcross.sh"
fi

if [ -z "${OSXCROSS_ROOT:-}" ] || [ -z "${OSXCROSS_TARGET:-}" ]; then
  echo "OSXCROSS_ROOT/OSXCROSS_TARGET are not set."
  echo "Run: source scripts/prep-macos-osxcross.sh"
  exit 1
fi

if [ -x "$OSXCROSS_ROOT/target/bin/$CLANG_WRAPPER" ] && [ -x "$OSXCROSS_ROOT/target/bin/$CLANGXX_WRAPPER" ]; then
  export OSXCROSS_CLANG="$OSXCROSS_ROOT/target/bin/$CLANG_WRAPPER"
  export OSXCROSS_CLANGXX="$OSXCROSS_ROOT/target/bin/$CLANGXX_WRAPPER"
else
  export OSXCROSS_CLANG="$OSXCROSS_ROOT/target/bin/${OSXCROSS_TARGET}-clang"
  export OSXCROSS_CLANGXX="$OSXCROSS_ROOT/target/bin/${OSXCROSS_TARGET}-clang++"
fi

if [ ! -x "$OSXCROSS_CLANG" ] || [ ! -x "$OSXCROSS_CLANGXX" ]; then
  echo "Unable to locate osxcross clang wrappers for $TARGET_ARCH."
  echo "Checked:"
  echo "  $OSXCROSS_CLANG"
  echo "  $OSXCROSS_CLANGXX"
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1 && [ "$GENERATOR" = "Ninja" ]; then
  GENERATOR="Unix Makefiles"
fi

echo "Configuring macOS $TARGET_ARCH cross-build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DARCH=default \
  -DCONSENSUS_SAFE_BUILD=ON \
  -DSTATIC=true

echo "Building targets ..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Cross-build complete."
echo "Binaries: $BUILD_DIR/src"
