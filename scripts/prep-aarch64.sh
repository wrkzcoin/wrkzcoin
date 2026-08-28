#!/usr/bin/env bash

_PREP_SCRIPT_SOURCED=0
if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  _PREP_SCRIPT_SOURCED=1
fi

_PREV_SHELL_OPTS="$-"
_PREV_PIPEFAIL_STATE="$(set -o | awk '/pipefail/ {print $2}')"
_PREV_ERR_TRAP="$(trap -p ERR || true)"
set -euo pipefail
trap 'echo "[ERROR] prep-aarch64 failed at line ${LINENO}: ${BASH_COMMAND}" >&2' ERR

BASEDIR="$(pwd)"
TARGET_TRIPLE="${TARGET_TRIPLE:-aarch64-linux-gnu}"
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$HOME/toolchain/aarch64-linux-gnu}"
PREFIX_DIR="${CROSS_PREFIX:-$TOOLCHAIN_DIR/prefix}"
JOBS="${JOBS:-$(nproc)}"
OPENSSL_VERSION="${OPENSSL_VERSION:-1.1.1w}"

mkdir -p "$TOOLCHAIN_DIR"
cd "$TOOLCHAIN_DIR"

echo "Installing Ubuntu packages for aarch64 cross-build..."
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  ccache \
  wget \
  curl \
  xz-utils \
  perl \
  make \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu

for bin in "${TARGET_TRIPLE}-gcc" "${TARGET_TRIPLE}-g++" "${TARGET_TRIPLE}-ar" "${TARGET_TRIPLE}-ranlib" "${TARGET_TRIPLE}-ld" "${TARGET_TRIPLE}-strip"; do
  if ! command -v "$bin" >/dev/null 2>&1; then
    echo "Missing required cross tool: $bin"
    exit 1
  fi
done

mkdir -p "$PREFIX_DIR"
echo "Using target prefix: $PREFIX_DIR"

echo -n "Checking for OpenSSL ${OPENSSL_VERSION} (aarch64 target)... "
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
  ./Configure linux-aarch64 \
    no-shared \
    no-tests \
    --cross-compile-prefix="${TARGET_TRIPLE}-" \
    --prefix="$PREFIX_DIR" \
    --openssldir="$PREFIX_DIR/ssl"
  make -j"$JOBS"
  make install_sw
  cd "$TOOLCHAIN_DIR"
  echo "Done."
else
  echo "Found."
fi



export CC="/usr/bin/${TARGET_TRIPLE}-gcc"
export CPP="/usr/bin/${TARGET_TRIPLE}-cpp"
export CXX="/usr/bin/${TARGET_TRIPLE}-g++"
export AR="/usr/bin/${TARGET_TRIPLE}-ar"
export RANLIB="/usr/bin/${TARGET_TRIPLE}-ranlib"
export LD="/usr/bin/${TARGET_TRIPLE}-ld"
export STRIP="/usr/bin/${TARGET_TRIPLE}-strip"

export CROSS_PREFIX="$PREFIX_DIR"
export OPENSSL_ROOT_DIR="$PREFIX_DIR"
export CMAKE_PREFIX_PATH="$PREFIX_DIR"
export CUSTOM_TOOLCHAIN_FILE="../scripts/cross-aarch64.cmake"

echo
echo "Environment prepared for Linux aarch64 cross-build."
echo "CC=$CC"
echo "CXX=$CXX"
echo "OPENSSL_ROOT_DIR=$OPENSSL_ROOT_DIR"
echo "CUSTOM_TOOLCHAIN_FILE=$CUSTOM_TOOLCHAIN_FILE"

cd "$BASEDIR"

if [[ "$_PREP_SCRIPT_SOURCED" -eq 1 ]]; then
  case "${_PREV_SHELL_OPTS}" in
    *e*) set -e ;; *) set +e ;;
  esac
  case "${_PREV_SHELL_OPTS}" in
    *u*) set -u ;; *) set +u ;;
  esac
  case "${_PREV_SHELL_OPTS}" in
    *f*) set -f ;; *) set +f ;;
  esac
  if [[ "${_PREV_SHELL_OPTS}" == *x* ]]; then
    set -x
  else
    set +x
  fi
  if [[ "${_PREV_PIPEFAIL_STATE}" == "on" ]]; then
    set -o pipefail
  else
    set +o pipefail
  fi
  if [[ -n "${_PREV_ERR_TRAP}" ]]; then
    eval "${_PREV_ERR_TRAP}"
  else
    trap - ERR
  fi
fi
