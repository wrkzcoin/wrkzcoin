#!/usr/bin/env bash
set -euo pipefail
trap 'echo "[ERROR] line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-linux-wallet-capi}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Ninja}"
JOBS="${JOBS:-$(nproc)}"
WALLET_LIB_KIND="${WALLET_LIB_KIND:-static}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required"
  exit 1
fi

if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
fi

echo "Configuring Linux wallet C API build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DWRKZ_BUILD_EXECUTABLES=OFF \
  -DWRKZ_BUILD_WALLET_CAPI=ON \
  -DENABLE_ZMQ=OFF

TARGETS=()
case "$WALLET_LIB_KIND" in
  static)
    TARGETS=(wallet_capi_c)
    ;;
  shared)
    TARGETS=(wallet_capi)
    ;;
  both)
    TARGETS=(wallet_capi_c wallet_capi)
    ;;
  *)
    echo "Invalid WALLET_LIB_KIND: $WALLET_LIB_KIND"
    echo "Expected: static | shared | both"
    exit 1
    ;;
esac

echo "Building wallet C API (${WALLET_LIB_KIND}) ..."
cmake --build "$BUILD_DIR" --target "${TARGETS[@]}" --parallel "$JOBS"

echo "Build complete."
if [ "$WALLET_LIB_KIND" = "static" ] || [ "$WALLET_LIB_KIND" = "both" ]; then
  echo "Expected static artifact:"
  echo "  $BUILD_DIR/src/libwallet_capi_c.a"
fi
if [ "$WALLET_LIB_KIND" = "shared" ] || [ "$WALLET_LIB_KIND" = "both" ]; then
  echo "Expected shared artifact:"
  echo "  $BUILD_DIR/src/libwallet_capi.so"
fi
