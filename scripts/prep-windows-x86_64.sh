#!/usr/bin/env bash
set -euo pipefail

# Prepare an Ubuntu host for x86_64 Windows cross-builds.

BASEDIR="$(pwd)"
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$HOME/toolchain/windows-x86_64}"
PREFIX_DIR="${CROSS_PREFIX:-$TOOLCHAIN_DIR/prefix}"
MINGW_PREFIX="${MINGW_PREFIX:-x86_64-w64-mingw32}"
JOBS="${JOBS:-$(nproc)}"

OPENSSL_VERSION="${OPENSSL_VERSION:-1.1.1w}"
BOOST_VERSION="${BOOST_VERSION:-1.84.0}"
BOOST_VERSION_U="${BOOST_VERSION//./_}"

mkdir -p "$TOOLCHAIN_DIR"
cd "$TOOLCHAIN_DIR"

echo "Installing Ubuntu packages for MinGW cross-build..."
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  ccache \
  wget \
  curl \
  unzip \
  xz-utils \
  perl \
  make \
  mingw-w64 \
  gcc-mingw-w64-x86-64 \
  g++-mingw-w64-x86-64 \
  binutils-mingw-w64-x86-64

echo "Using toolchain prefix: $PREFIX_DIR"
mkdir -p "$PREFIX_DIR"

echo -n "Checking for OpenSSL ${OPENSSL_VERSION} (Windows target)... "
if [ ! -f "$PREFIX_DIR/lib/libcrypto.a" ]; then
  echo "Not found. Building..."
  OPENSSL_TAR="openssl-${OPENSSL_VERSION}.tar.gz"
  OPENSSL_SRC="openssl-${OPENSSL_VERSION}"
  if [ ! -f "$OPENSSL_TAR" ]; then
    wget "https://www.openssl.org/source/${OPENSSL_TAR}"
  fi
  rm -rf "$OPENSSL_SRC"
  tar zxf "$OPENSSL_TAR"
  cd "$OPENSSL_SRC"
  ./Configure mingw64 \
    no-shared \
    no-tests \
    --cross-compile-prefix="${MINGW_PREFIX}-" \
    --prefix="$PREFIX_DIR" \
    --openssldir="$PREFIX_DIR/ssl"
  make -j"$JOBS"
  make install_sw
  cd "$TOOLCHAIN_DIR"
  echo "Done."
else
  echo "Found."
fi

echo -n "Checking for Boost ${BOOST_VERSION} date_time (Windows target)... "
if [ ! -f "$PREFIX_DIR/lib/libboost_date_time.a" ]; then
  echo "Not found. Building..."
  BOOST_TAR="boost_${BOOST_VERSION_U}.tar.gz"
  BOOST_SRC="boost_${BOOST_VERSION_U}"
  if [ ! -f "$BOOST_TAR" ]; then
    wget "https://archives.boost.io/release/${BOOST_VERSION}/source/${BOOST_TAR}"
  fi
  rm -rf "$BOOST_SRC"
  tar zxf "$BOOST_TAR"
  cd "$BOOST_SRC"
  ./bootstrap.sh
  cat > tools/build/src/user-config.jam <<EOF
using gcc : mingw : ${MINGW_PREFIX}-g++ ;
EOF
  ./b2 -j"$JOBS" \
    --user-config=tools/build/src/user-config.jam \
    toolset=gcc-mingw \
    target-os=windows \
    architecture=x86 \
    address-model=64 \
    threading=multi \
    runtime-link=static \
    link=static \
    variant=release \
    --with-date_time \
    install \
    --prefix="$PREFIX_DIR"
  cd "$TOOLCHAIN_DIR"
  echo "Done."
else
  echo "Found."
fi

export CC="/usr/bin/${MINGW_PREFIX}-gcc"
export CXX="/usr/bin/${MINGW_PREFIX}-g++"
export AR="/usr/bin/${MINGW_PREFIX}-ar"
export RANLIB="/usr/bin/${MINGW_PREFIX}-ranlib"
export STRIP="/usr/bin/${MINGW_PREFIX}-strip"
export RC="/usr/bin/${MINGW_PREFIX}-windres"

export CROSS_PREFIX="$PREFIX_DIR"
export BOOST_ROOT="$PREFIX_DIR"
export OPENSSL_ROOT_DIR="$PREFIX_DIR"
export CMAKE_PREFIX_PATH="$PREFIX_DIR"
export CUSTOM_TOOLCHAIN_FILE="../scripts/cross-windows-x86_64.cmake"

echo
echo "Environment prepared for Windows x86_64 cross-build."
echo "CC=$CC"
echo "CXX=$CXX"
echo "BOOST_ROOT=$BOOST_ROOT"
echo "OPENSSL_ROOT_DIR=$OPENSSL_ROOT_DIR"
echo "CUSTOM_TOOLCHAIN_FILE=$CUSTOM_TOOLCHAIN_FILE"

cd "$BASEDIR"

