#!/usr/bin/env bash

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  echo "Do not source this script."
  echo "Run it directly: bash scripts/cross-build-windows-x86_64.sh"
  return 0
fi

set -euo pipefail
trap 'echo "[ERROR] line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-windows-x86_64}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
DEFAULT_JOBS="$(nproc)"
JOBS="${JOBS:-$DEFAULT_JOBS}"
if [ "$JOBS" -gt 8 ]; then
  JOBS=8
fi
if [ "$JOBS" -lt 1 ]; then
  JOBS=1
fi
export CMAKE_BUILD_PARALLEL_LEVEL="$JOBS"
GENERATOR="${GENERATOR:-Ninja}"
SKIP_PREP="${SKIP_PREP:-0}"
LOG_DIR="${LOG_DIR:-$REPO_ROOT/build-logs}"
LOG_FILE="${LOG_FILE:-$LOG_DIR/cross-build-windows-x86_64.log}"

if [ "$SKIP_PREP" != "1" ]; then
  bash "$SCRIPT_DIR/prep-windows-x86_64.sh"
fi

if ! command -v ninja >/dev/null 2>&1 && [ "$GENERATOR" = "Ninja" ]; then
  GENERATOR="Unix Makefiles"
fi

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "Log file: $LOG_FILE"
echo "Configuring cross-build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-windows-x86_64.cmake" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DARCH=default \
  -DCONSENSUS_SAFE_BUILD=ON \
  -DROCKSDB_BUILD_PARALLEL="$JOBS" \
  -DSTATIC=true

echo "Building targets ..."
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Cross-build complete."
echo "Binaries: $BUILD_DIR/src"
