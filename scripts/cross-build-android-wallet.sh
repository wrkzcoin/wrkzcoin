#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-android-arm64}"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-24}"
JOBS="${JOBS:-$(nproc)}"

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

if [ -z "${BOOST_ROOT:-}" ] && [ ! -f "/usr/include/boost/version.hpp" ]; then
  echo "Boost headers not found."
  echo "Install headers on host (Ubuntu):"
  echo "  sudo apt-get install -y libboost-dev"
  echo "or set BOOST_ROOT to a Boost prefix containing include/boost/version.hpp"
  exit 1
fi

echo "Configuring Android wallet C API build in $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ANDROID_ABI" \
  -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
  -DWRKZ_ANDROID_PROFILE=ON \
  -DWRKZ_BUILD_EXECUTABLES=OFF \
  -DWRKZ_BUILD_WALLET_CAPI=ON \
  -DENABLE_ZMQ=OFF \
  ${BOOST_ROOT:+-DBOOST_ROOT="$BOOST_ROOT"}

echo "Building wallet C API target ..."
cmake --build "$BUILD_DIR" --target wallet_capi --parallel "$JOBS"

echo "Done. Artifact:"
echo "  $BUILD_DIR/src/libwallet_capi.so"
