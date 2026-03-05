#!/usr/bin/env bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  echo "Do not source this script."
  echo "Run it directly: bash scripts/cross-build-windows-wallet-lib.sh"
  return 0
fi

set -euo pipefail
trap 'echo "[ERROR] line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-windows-wallet-x86_64}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Ninja}"
SKIP_PREP="${SKIP_PREP:-0}"
LOG_DIR="${LOG_DIR:-$REPO_ROOT/build-logs}"
LOG_FILE="${LOG_FILE:-$LOG_DIR/cross-build-windows-wallet-lib.log}"
WALLET_LIB_KIND="${WALLET_LIB_KIND:-static}"
JOBS="${JOBS:-$(nproc)}"
if [ "$JOBS" -gt 8 ]; then
  JOBS=8
fi
if [ "$JOBS" -lt 1 ]; then
  JOBS=1
fi
export CMAKE_BUILD_PARALLEL_LEVEL="$JOBS"

if [ "$SKIP_PREP" != "1" ]; then
  bash "$SCRIPT_DIR/prep-windows-x86_64.sh"
fi

if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
fi

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "Log file: $LOG_FILE"
echo "Configuring Windows wallet C API cross-build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-windows-x86_64.cmake" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DARCH=default \
  -DCONSENSUS_SAFE_BUILD=ON \
  -DROCKSDB_BUILD_PARALLEL="$JOBS" \
  -DSTATIC=true \
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

echo "Cross-build complete."
echo "Expected artifacts (in $BUILD_DIR/src):"
if [ "$WALLET_LIB_KIND" = "static" ] || [ "$WALLET_LIB_KIND" = "both" ]; then
  echo "  libwallet_capi_c.a (or wallet_capi_c.lib)"
fi
if [ "$WALLET_LIB_KIND" = "shared" ] || [ "$WALLET_LIB_KIND" = "both" ]; then
  echo "  wallet_capi.dll (or libwallet_capi.dll)"
  echo "  import library (.dll.a or .lib)"
fi
