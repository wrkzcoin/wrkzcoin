#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-android-cli-arm64-v8a}"
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

if [ -n "${BOOST_ROOT:-}" ]; then
  BOOST_INCLUDE_CANDIDATE="$BOOST_ROOT/include"
  if [ -f "$BOOST_ROOT/boost/version.hpp" ]; then
    BOOST_INCLUDE_CANDIDATE="$BOOST_ROOT"
  fi
else
  if [ ! -f "/usr/include/boost/version.hpp" ]; then
    echo "Boost headers not found."
    echo "Install headers on host (Ubuntu):"
    echo "  sudo apt-get install -y libboost-dev"
    echo "or set BOOST_ROOT to a Boost prefix containing include/boost/version.hpp"
    exit 1
  fi

  # Avoid injecting host /usr/include into Android cross compile.
  STAGED_BOOST_ROOT="${REPO_ROOT}/.android-boost"
  STAGED_BOOST_INCLUDE="${STAGED_BOOST_ROOT}/include"
  mkdir -p "${STAGED_BOOST_INCLUDE}"
  rm -rf "${STAGED_BOOST_INCLUDE}/boost"
  cp -a /usr/include/boost "${STAGED_BOOST_INCLUDE}/boost"
  BOOST_ROOT="${STAGED_BOOST_ROOT}"
  BOOST_INCLUDE_CANDIDATE="${STAGED_BOOST_INCLUDE}"
fi

for h in boost/version.hpp boost/uuid/uuid.hpp boost/variant.hpp boost/algorithm/string.hpp; do
  if [ ! -f "$BOOST_INCLUDE_CANDIDATE/$h" ]; then
    echo "Missing required Boost header: $BOOST_INCLUDE_CANDIDATE/$h"
    echo "Install full headers (Ubuntu): sudo apt-get install -y libboost-dev"
    echo "or set BOOST_ROOT to a valid Boost prefix."
    exit 1
  fi
done

build_one_abi() {
  local abi="$1"
  local build_dir="$2"
  local libucontext_root="$3"

  if [ ! -f "$libucontext_root/lib/libucontext.a" ]; then
    echo "Warning: libucontext not found at:"
    echo "  $libucontext_root/lib/libucontext.a"
    echo "Continuing without libucontext for ABI '$abi'. Link may fail on getcontext/swapcontext/makecontext."
  fi

  echo "Configuring Android CLI build for ABI '$abi' in $build_dir ..."
  cmake -S "$REPO_ROOT" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DWRKZ_BUILD_EXECUTABLES=ON \
    -DWRKZ_BUILD_WALLET_CAPI=OFF \
    -DWRKZ_ANDROID_PROFILE=OFF \
    -DWRKZ_ANDROID_HEADER_ONLY_BOOST=ON \
    -DWRKZ_ANDROID_DISABLE_OPENSSL=ON \
    -DENABLE_ZMQ=OFF \
    -DBOOST_ROOT="$BOOST_ROOT" \
    -DLIBUCONTEXT_ROOT="$libucontext_root"

  echo "Building Android CLI binaries for ABI '$abi' ..."
  cmake --build "$build_dir" --parallel "$JOBS"

  echo "Done. Artifacts for $abi:"
  echo "  $build_dir/src/Wrkzd"
  echo "  $build_dir/src/wrkz-wallet"
  echo "  $build_dir/src/wrkz-service"
  echo "  $build_dir/src/wrkz-wallet-api"
  echo "  $build_dir/src/wallet-upgrader"
  echo "  $build_dir/src/miner"
  echo "  $build_dir/src/cryptotest"
}

if [ -n "$ANDROID_ABIS" ]; then
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
      "$REPO_ROOT/build-android-cli-$abi" \
      "$LIBUCONTEXT_ROOT_BASE/$abi"
  done
else
  build_one_abi "$ANDROID_ABI" "$BUILD_DIR" "$LIBUCONTEXT_ROOT"
fi
