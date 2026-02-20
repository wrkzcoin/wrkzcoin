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

echo "Resolving libucontext static library targets ..."
TARGETS_JSON="$WORK_DIR/targets-$ABI.json"
if ! meson introspect --targets "$BUILD_DIR" > "$TARGETS_JSON"; then
  : > "$TARGETS_JSON"
fi

LIB_TARGETS="$(python3 - "$TARGETS_JSON" <<'PY'
import json, sys, pathlib
p = pathlib.Path(sys.argv[1])
data = p.read_text(encoding="utf-8").strip() if p.exists() else ""
if not data:
    print("")
    raise SystemExit(0)
try:
    targets = json.loads(data)
except Exception:
    print("")
    raise SystemExit(0)
names = []
for t in targets:
    ttype = str(t.get("type", "")).lower()
    if "static library" not in ttype:
        continue
    name = str(t.get("name", ""))
    fnv = t.get("filename", [])
    if isinstance(fnv, list):
        fn = " ".join(str(x) for x in fnv)
    else:
        fn = str(fnv)
    if "ucontext" in name.lower() or "ucontext" in fn.lower():
        names.append(name)
print("\n".join(dict.fromkeys(names)))
PY
)"

if [ -z "$LIB_TARGETS" ]; then
  echo "No explicit static ucontext targets found via meson introspect."
  echo "Trying full build (may run tests depending on upstream meson rules)..."
  meson compile -C "$BUILD_DIR" -j "$JOBS" || true
else
  echo "Building targets:"
  echo "$LIB_TARGETS"
  while IFS= read -r target; do
    [ -z "$target" ] && continue
    meson compile -C "$BUILD_DIR" -j "$JOBS" "$target"
  done <<< "$LIB_TARGETS"
fi

mkdir -p "$PREFIX/lib" "$PREFIX/include"

FOUND=""
for name in libucontext.a libucontext_posix.a; do
  CANDIDATE="$(find "$BUILD_DIR" -name "$name" | head -n 1 || true)"
  if [ -n "$CANDIDATE" ]; then
    FOUND="$CANDIDATE"
    break
  fi
done

if [ -n "$FOUND" ]; then
  cp -f "$FOUND" "$PREFIX/lib/libucontext.a"
fi

if [ -d "$SRC_DIR/include" ]; then
  cp -a "$SRC_DIR/include/." "$PREFIX/include/"
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
