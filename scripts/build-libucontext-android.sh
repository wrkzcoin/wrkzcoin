#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ABI="${ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-24}"
JOBS="${JOBS:-$(nproc)}"
LIBUCONTEXT_GIT_URL="${LIBUCONTEXT_GIT_URL:-https://github.com/kaniini/libucontext.git}"
LIBUCONTEXT_REF="${LIBUCONTEXT_REF:-master}"
WORK_DIR="${WORK_DIR:-$REPO_ROOT/.tmp-libucontext}"
SRC_DIR="${SRC_DIR:-$WORK_DIR/src}"
BUILD_DIR="${BUILD_DIR:-$WORK_DIR/build-$ABI}"
PREFIX="${PREFIX:-$REPO_ROOT/.android-libucontext/$ABI}"

if [ -z "${ANDROID_NDK:-}" ]; then
  echo "ANDROID_NDK is not set."
  echo "Example:"
  echo "  export ANDROID_NDK=\$HOME/Android/Sdk/ndk/27.2.12479018"
  exit 1
fi

if [ ! -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]; then
  echo "Missing Android NDK toolchain:"
  echo "  $ANDROID_NDK/build/cmake/android.toolchain.cmake"
  exit 1
fi

for tool in git meson ninja; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing required tool: $tool"
    exit 1
  fi
done

TOOLCHAIN_BIN="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"

case "$ABI" in
  arm64-v8a)
    TARGET_TRIPLE="aarch64-linux-android"
    CPU_FAMILY="aarch64"
    CPU_NAME="aarch64"
    ;;
  armeabi-v7a)
    TARGET_TRIPLE="armv7a-linux-androideabi"
    CPU_FAMILY="arm"
    CPU_NAME="armv7"
    ;;
  x86_64)
    TARGET_TRIPLE="x86_64-linux-android"
    CPU_FAMILY="x86_64"
    CPU_NAME="x86_64"
    ;;
  x86)
    TARGET_TRIPLE="i686-linux-android"
    CPU_FAMILY="x86"
    CPU_NAME="i686"
    ;;
  *)
    echo "Unsupported ABI: $ABI"
    echo "Supported: arm64-v8a, armeabi-v7a, x86_64, x86"
    exit 1
    ;;
esac

CC="$TOOLCHAIN_BIN/${TARGET_TRIPLE}${ANDROID_PLATFORM}-clang"
AR="$TOOLCHAIN_BIN/llvm-ar"
STRIP="$TOOLCHAIN_BIN/llvm-strip"

if [ ! -x "$CC" ]; then
  echo "Android clang not found:"
  echo "  $CC"
  exit 1
fi

mkdir -p "$WORK_DIR"

if [ ! -d "$SRC_DIR/.git" ]; then
  echo "Cloning libucontext into $SRC_DIR ..."
  git clone --depth 1 --branch "$LIBUCONTEXT_REF" "$LIBUCONTEXT_GIT_URL" "$SRC_DIR"
else
  echo "Refreshing existing libucontext repo in $SRC_DIR ..."
  git -C "$SRC_DIR" fetch --depth 1 origin "$LIBUCONTEXT_REF"
  git -C "$SRC_DIR" checkout -f FETCH_HEAD
fi

mkdir -p "$(dirname "$BUILD_DIR")"
mkdir -p "$PREFIX"

CROSS_FILE="$WORK_DIR/cross-$ABI.ini"
cat > "$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
ar = '$AR'
strip = '$STRIP'
pkgconfig = 'false'

[host_machine]
system = 'android'
cpu_family = '$CPU_FAMILY'
cpu = '$CPU_NAME'
endian = 'little'
EOF

echo "Configuring libucontext for $ABI ..."
if [ -f "$BUILD_DIR/meson-private/coredata.dat" ]; then
  meson setup "$BUILD_DIR" "$SRC_DIR" \
    --cross-file "$CROSS_FILE" \
    --default-library=static \
    --buildtype=release \
    --prefix "$PREFIX" \
    --wipe
else
  rm -rf "$BUILD_DIR"
  meson setup "$BUILD_DIR" "$SRC_DIR" \
    --cross-file "$CROSS_FILE" \
    --default-library=static \
    --buildtype=release \
    --prefix "$PREFIX"
fi

echo "Building libucontext ..."
meson compile -C "$BUILD_DIR" -j "$JOBS"

echo "Installing libucontext into $PREFIX ..."
meson install -C "$BUILD_DIR"

if [ ! -f "$PREFIX/lib/libucontext.a" ]; then
  FOUND="$(find "$BUILD_DIR" -name libucontext.a | head -n 1 || true)"
  if [ -n "$FOUND" ]; then
    mkdir -p "$PREFIX/lib"
    cp -f "$FOUND" "$PREFIX/lib/libucontext.a"
  fi
fi

if [ ! -f "$PREFIX/lib/libucontext.a" ]; then
  echo "Failed to produce $PREFIX/lib/libucontext.a"
  exit 1
fi

echo "Done."
echo "libucontext root: $PREFIX"
echo "Static library:  $PREFIX/lib/libucontext.a"
echo "Use with wallet build:"
echo "  LIBUCONTEXT_ROOT=\"$PREFIX\" ANDROID_ABI=\"$ABI\" bash scripts/cross-build-android-wallet.sh"
