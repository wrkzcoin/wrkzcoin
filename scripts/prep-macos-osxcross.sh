#!/usr/bin/env bash
set -euo pipefail

# Prepare an Ubuntu host for macOS cross-builds with osxcross.
# Note: Apple SDK must be provided by the user due license restrictions.

BASEDIR="$(pwd)"
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$HOME/toolchain/macos}"
OSXCROSS_ROOT="${OSXCROSS_ROOT:-$TOOLCHAIN_DIR/osxcross}"
SDK_SEARCH_DIR="${SDK_SEARCH_DIR:-$TOOLCHAIN_DIR/sdk}"
OSXCROSS_TARGET="${OSXCROSS_TARGET:-x86_64-apple-darwin22.4}"
OSXCROSS_DEPLOYMENT_TARGET="${OSXCROSS_DEPLOYMENT_TARGET:-10.15}"
JOBS="${JOBS:-$(nproc)}"

mkdir -p "$TOOLCHAIN_DIR" "$SDK_SEARCH_DIR"
cd "$TOOLCHAIN_DIR"

echo "Installing Ubuntu packages for osxcross..."
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  ccache \
  wget \
  curl \
  git \
  xz-utils \
  bzip2 \
  patch \
  clang \
  lld \
  libxml2-dev \
  zlib1g-dev \
  libssl-dev

if [ ! -d "$OSXCROSS_ROOT/.git" ]; then
  echo "Cloning osxcross..."
  git clone https://github.com/tpoechtrager/osxcross.git "$OSXCROSS_ROOT"
fi

mkdir -p "$OSXCROSS_ROOT/tarballs"

SDK_TARBALL=""
if [ -n "${OSXCROSS_SDK_TAR:-}" ] && [ -f "${OSXCROSS_SDK_TAR}" ]; then
  SDK_TARBALL="${OSXCROSS_SDK_TAR}"
else
  SDK_TARBALL="$(find "$SDK_SEARCH_DIR" -maxdepth 1 -type f \( -name "MacOSX*.sdk.tar.xz" -o -name "MacOSX*.sdk.tar.gz" \) | head -n 1 || true)"
fi

if [ -z "$SDK_TARBALL" ]; then
  echo "No macOS SDK tarball found."
  echo "Place one of these files in $SDK_SEARCH_DIR (or set OSXCROSS_SDK_TAR):"
  echo "  MacOSX*.sdk.tar.xz or MacOSX*.sdk.tar.gz"
  exit 1
fi

echo "Using SDK tarball: $SDK_TARBALL"
cp -f "$SDK_TARBALL" "$OSXCROSS_ROOT/tarballs/"

if [ ! -x "$OSXCROSS_ROOT/target/bin/${OSXCROSS_TARGET}-clang" ]; then
  echo "Building osxcross toolchain..."
  (
    cd "$OSXCROSS_ROOT"
    UNATTENDED=1 ./build.sh
  )
fi

if [ ! -x "$OSXCROSS_ROOT/target/bin/${OSXCROSS_TARGET}-clang" ]; then
  echo "osxcross build did not produce compiler: ${OSXCROSS_TARGET}-clang"
  exit 1
fi

SDK_PATH="$(find "$OSXCROSS_ROOT/target/SDK" -maxdepth 1 -type d -name "MacOSX*.sdk" | head -n 1 || true)"
if [ -z "$SDK_PATH" ]; then
  echo "Unable to locate extracted SDK path in $OSXCROSS_ROOT/target/SDK"
  exit 1
fi

export OSXCROSS_ROOT
export OSXCROSS_TARGET
export OSXCROSS_SDK="$SDK_PATH"
export OSXCROSS_DEPLOYMENT_TARGET

echo
echo "Environment prepared for macOS cross-build."
echo "OSXCROSS_ROOT=$OSXCROSS_ROOT"
echo "OSXCROSS_TARGET=$OSXCROSS_TARGET"
echo "OSXCROSS_SDK=$OSXCROSS_SDK"
echo "OSXCROSS_DEPLOYMENT_TARGET=$OSXCROSS_DEPLOYMENT_TARGET"
echo
echo "Dependency note:"
echo "Provide target Boost/OpenSSL for macOS and export:"
echo "  BOOST_ROOT=<path>"
echo "  OPENSSL_ROOT_DIR=<path>"
echo "  CMAKE_PREFIX_PATH=<path>"

cd "$BASEDIR"

