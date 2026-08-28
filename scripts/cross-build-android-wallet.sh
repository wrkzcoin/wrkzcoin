#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-android-$ANDROID_ABI}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_ABIS="${ANDROID_ABIS:-}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-24}"
JOBS="${JOBS:-$(nproc)}"
LIBUCONTEXT_ROOT="${LIBUCONTEXT_ROOT:-$REPO_ROOT/.android-libucontext/$ANDROID_ABI}"
LIBUCONTEXT_ROOT_BASE="${LIBUCONTEXT_ROOT_BASE:-$REPO_ROOT/.android-libucontext}"

if [ -z "${ANDROID_NDK:-}" ]; then
  echo "ANDROID_NDK is not set."
  echo "Example:"
  echo "  export ANDROID_NDK=\$HOME/Android/Sdk/ndk/26.3.11579264"
  exit 1
fi

if [ ! -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]; then
  echo "Missing Android toolchain file:"
  echo "  $ANDROID_NDK/build/cmake/android.toolchain.cmake"
  exit 1
fi


build_one_abi() {
  local abi="$1"
  local build_dir="$2"
  local libucontext_root="$3"

  if [ ! -f "$libucontext_root/lib/libucontext.a" ]; then
    echo "Warning: libucontext not found at:"
    echo "  $libucontext_root/lib/libucontext.a"
    echo "Continuing without libucontext for ABI '$abi'. Link may fail on getcontext/swapcontext/makecontext."
  fi

  echo "Configuring Android wallet C API build for ABI '$abi' in $build_dir ..."
  cmake -S "$REPO_ROOT" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DWRKZ_ANDROID_PROFILE=ON \
    -DWRKZ_BUILD_EXECUTABLES=OFF \
    -DWRKZ_BUILD_WALLET_CAPI=ON \
    -DENABLE_ZMQ=OFF \
    -DLIBUCONTEXT_ROOT="$libucontext_root"

  echo "Building wallet C API target for ABI '$abi' ..."
  cmake --build "$build_dir" --target wallet_capi --parallel "$JOBS"

  echo "Done. Artifact for $abi:"
  echo "  $build_dir/src/libwallet_capi.so"
}

if [ -n "$ANDROID_ABIS" ]; then
  # Accept space/comma/newline-separated ABI list and strip CRs.
  ABIS_NORMALIZED="$(printf '%s' "$ANDROID_ABIS" | tr ',;\r\n\t' '     ')"
  # shellcheck disable=SC2206
  ABI_LIST=($ABIS_NORMALIZED)

  if [ "${#ABI_LIST[@]}" -eq 0 ]; then
    echo "ANDROID_ABIS is set but no ABI values were parsed."
    echo "Provided value: '$ANDROID_ABIS'"
    exit 1
  fi

  echo "Multi-ABI mode enabled. ABIs: ${ABI_LIST[*]}"

  for abi in "${ABI_LIST[@]}"; do
    build_one_abi \
      "$abi" \
      "$REPO_ROOT/build-android-$abi" \
      "$LIBUCONTEXT_ROOT_BASE/$abi"
  done
else
  build_one_abi "$ANDROID_ABI" "$BUILD_DIR" "$LIBUCONTEXT_ROOT"
fi
