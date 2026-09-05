#!/usr/bin/env bash
#
# Container side of scripts/docker/build.sh: configure, build, verify and
# package the requested targets, one build tree per target.
#
#   bash scripts/docker/container-build.sh [linux] [windows] [android] [all]
#
# It expects the toolchains the Dockerfile lays out (MinGW OpenSSL prefix,
# Android NDK, libucontext per ABI) but locates all of them through
# environment variables, so it also runs on an Ubuntu host prepared with the
# scripts/prep-*.sh scripts. Everything is driven by environment variables;
# see scripts/docker/README.md for the list.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_ROOT="${BUILD_ROOT:-$REPO_ROOT/build-docker}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/builds}"
JOBS="${JOBS:-}"
VERSION="${VERSION:-}"
ANDROID_ABIS="${ANDROID_ABIS:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-24}"
CLEAN="${CLEAN:-0}"
KEEP_GOING="${KEEP_GOING:-0}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GENERATOR="${GENERATOR:-Ninja}"

# Toolchain locations. The Dockerfile exports the WRKZ_* ones; the fallbacks
# match scripts/prep-windows-x86_64.sh and scripts/build-libucontext-android.sh.
MINGW_TRIPLE="${MINGW_PREFIX:-x86_64-w64-mingw32}"
MINGW_PREFIX_DIR="${WRKZ_MINGW_PREFIX_DIR:-$HOME/toolchain/windows-x86_64/prefix}"
ANDROID_LIBUCONTEXT_BASE="${WRKZ_ANDROID_LIBUCONTEXT_BASE:-$REPO_ROOT/.android-libucontext}"

# Executables a package can carry (plus LICENSE), in this order. Each one is
# packaged only if the checked-out src/CMakeLists.txt declares it, so the same
# script serves branches with and without the newer targets (wrkz-txpow-server
# exists on dev-lite-node but not yet on development). A declared executable
# that is missing after the build is still an error.
BINARY_CANDIDATES=(
  Wrkzd
  wrkz-wallet
  wrkz-service
  wrkz-wallet-api
  wallet-upgrader
  miner
  cryptotest
  wrkz-txpow-server
)
BINARIES=()
PKG_PREFIX="${PKG_PREFIX:-wrkzcoin-cli}"

log() { printf '\n==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# x.y.z.b from src/config/version.h.in (the release template checklist keeps
# that file as the single source of the version number).
read_version() {
  local header="$REPO_ROOT/src/config/version.h.in"
  local key value parts=()
  for key in APP_VER_MAJOR APP_VER_MINOR APP_VER_REV APP_VER_BUILD; do
    value="$(awk -v k="$key" '$1 == "#define" && $2 == k { print $3; exit }' "$header" | tr -d '\r')"
    [ -n "$value" ] || die "cannot read $key from $header"
    parts+=("$value")
  done
  local IFS=.
  printf '%s' "${parts[*]}"
}

# Fill BINARIES with the candidates this checkout actually builds.
resolve_binaries() {
  local cml="$REPO_ROOT/src/CMakeLists.txt" b
  BINARIES=()
  for b in "${BINARY_CANDIDATES[@]}"; do
    if grep -q "OUTPUT_NAME \"$b\"" "$cml"; then
      BINARIES+=("$b")
    else
      echo "note: $b is not a target in this checkout; not packaged"
    fi
  done
  [ "${#BINARIES[@]}" -gt 0 ] || die "no packageable executables declared in $cml"
}

# configure_and_build <build-dir> [cmake configure args...]
configure_and_build() {
  local bd="$1"
  shift
  if [ "$CLEAN" = "1" ] && [ -d "$bd" ]; then
    log "CLEAN=1: removing $bd"
    rm -rf "$bd"
  fi
  mkdir -p "$bd"
  log "Configuring $bd"
  cmake -S "$REPO_ROOT" -B "$bd" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DROCKSDB_BUILD_PARALLEL="$JOBS" \
    "$@"
  log "Building $bd with $JOBS jobs"
  cmake --build "$bd" --parallel "$JOBS"
}

# stage_dir <package-name>: fresh staging directory, path on stdout.
stage_dir() {
  local d="$BUILD_ROOT/pkg/$1"
  rm -rf "$d"
  mkdir -p "$d"
  printf '%s' "$d"
}

# stage_binaries <bin-dir> <stage-dir> <suffix>: copy the executables and the
# root LICENSE into the staging directory. A missing executable is an error;
# a package with a binary silently left out is worse than no package.
stage_binaries() {
  local bindir="$1" stage="$2" suffix="$3" b
  for b in "${BINARIES[@]}"; do
    [ -f "$bindir/$b$suffix" ] || die "missing binary: $bindir/$b$suffix"
    cp "$bindir/$b$suffix" "$stage/"
    chmod 755 "$stage/$b$suffix"
  done
  cp "$REPO_ROOT/LICENSE" "$stage/LICENSE"
}

# make_tarball <package-name>
make_tarball() {
  local name="$1" out="$OUT_DIR/$1.tar.gz"
  rm -f "$out"
  tar -C "$BUILD_ROOT/pkg" --owner=0 --group=0 --numeric-owner -czf "$out" "$name"
  log "Package: $out"
}

# make_zip <package-name>
make_zip() {
  local name="$1" out="$OUT_DIR/$1.zip"
  rm -f "$out"
  (cd "$BUILD_ROOT/pkg" && zip -q -r -X "$out" "$name")
  log "Package: $out"
}

# print_files <stage-dir> <suffix>: 'file' output for every executable.
print_files() {
  local stage="$1" suffix="$2" b
  for b in "${BINARIES[@]}"; do
    file "$stage/$b$suffix"
  done
}

# find_mingw_dll <name>: path of a MinGW runtime DLL on this host.
find_mingw_dll() {
  local dll="$1" cc p d
  for cc in "$MINGW_TRIPLE-g++-posix" "$MINGW_TRIPLE-g++" "$MINGW_TRIPLE-gcc-posix" "$MINGW_TRIPLE-gcc"; do
    command -v "$cc" >/dev/null 2>&1 || continue
    p="$("$cc" -print-file-name="$dll" 2>/dev/null || true)"
    if [ -n "$p" ] && [ "$p" != "$dll" ] && [ -f "$p" ]; then
      printf '%s' "$p"
      return 0
    fi
  done
  for d in "/usr/$MINGW_TRIPLE/lib" "/usr/$MINGW_TRIPLE/bin" \
           /usr/lib/gcc/"$MINGW_TRIPLE"/*-posix /usr/lib/gcc/"$MINGW_TRIPLE"/*; do
    if [ -f "$d/$dll" ]; then
      printf '%s' "$d/$dll"
      return 0
    fi
  done
  return 1
}

# bundle_mingw_dlls <stage-dir>: copy every MinGW runtime DLL the executables
# actually import (libwinpthread-1.dll, libgcc_s_seh-1.dll, libstdc++-6.dll,
# ...) next to them. Imports that do not start with "lib" are Windows system
# DLLs and are left alone. A required DLL that cannot be found fails the
# build: the zip would not be portable without it.
bundle_mingw_dlls() {
  local stage="$1" exe dll lower path
  local -A wanted=()
  for exe in "$stage"/*.exe; do
    while read -r dll; do
      [ -n "$dll" ] || continue
      lower="$(printf '%s' "$dll" | tr '[:upper:]' '[:lower:]')"
      case "$lower" in
        lib*.dll) wanted["$dll"]=1 ;;
      esac
    done < <("$MINGW_TRIPLE-objdump" -p "$exe" | awk '/DLL Name:/ { print $3 }')
  done
  if [ "${#wanted[@]}" -eq 0 ]; then
    echo "No MinGW runtime DLLs are imported; the executables are self-contained."
    return 0
  fi
  for dll in "${!wanted[@]}"; do
    path="$(find_mingw_dll "$dll")" \
      || die "$dll is imported by the Windows executables but was not found on this host"
    cp "$path" "$stage/"
    echo "Bundled runtime DLL: $dll  ($path)"
  done
}

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

build_linux() {
  local bd="$BUILD_ROOT/linux-x86_64"
  local name="$PKG_PREFIX-linux-x86_64-$VERSION"
  configure_and_build "$bd" \
    -DARCH=default \
    -DCONSENSUS_SAFE_BUILD=ON \
    -DPORTABLE_BINARY=ON \
    -DENABLE_X86_AESNI=OFF \
    -DFULLY_STATIC=ON

  local stage b
  stage="$(stage_dir "$name")"
  stage_binaries "$bd/src" "$stage" ""

  log "Verifying Linux executables"
  print_files "$stage" ""
  for b in "${BINARIES[@]}"; do
    file "$stage/$b" | grep -q 'statically linked' \
      || die "$b is not statically linked; FULLY_STATIC did not take effect"
  done
  # The build host is x86_64 Linux, so the daemon and wallet can prove they
  # start. Run from the staging directory so any log file lands there.
  (cd "$stage" && ./Wrkzd --version && ./wrkz-wallet --version)
  rm -f "$stage"/*.log

  make_tarball "$name"
}

build_windows() {
  local bd="$BUILD_ROOT/windows-x86_64"
  local name="$PKG_PREFIX-windows-x86_64-$VERSION"
  [ -f "$MINGW_PREFIX_DIR/lib/libcrypto.a" ] \
    || die "no Windows-target OpenSSL under $MINGW_PREFIX_DIR (WRKZ_MINGW_PREFIX_DIR)"
  command -v "$MINGW_TRIPLE-g++-posix" >/dev/null 2>&1 || command -v "$MINGW_TRIPLE-g++" >/dev/null 2>&1 \
    || die "MinGW-w64 compiler $MINGW_TRIPLE-g++ not found"

  # scripts/cross-windows-x86_64.cmake reads CROSS_PREFIX; FindOpenSSL reads
  # OPENSSL_ROOT_DIR. Same variables scripts/prep-windows-x86_64.sh exports.
  export CROSS_PREFIX="$MINGW_PREFIX_DIR"
  export OPENSSL_ROOT_DIR="$MINGW_PREFIX_DIR"
  export CMAKE_PREFIX_PATH="$MINGW_PREFIX_DIR"

  configure_and_build "$bd" \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/scripts/cross-windows-x86_64.cmake" \
    -DARCH=default \
    -DCONSENSUS_SAFE_BUILD=ON \
    -DPORTABLE_BINARY=ON \
    -DENABLE_X86_AESNI=OFF

  local stage b
  stage="$(stage_dir "$name")"
  stage_binaries "$bd/src" "$stage" ".exe"

  log "Stripping and verifying Windows executables"
  for b in "${BINARIES[@]}"; do
    "$MINGW_TRIPLE-strip" "$stage/$b.exe"
  done
  print_files "$stage" ".exe"
  for b in "${BINARIES[@]}"; do
    file "$stage/$b.exe" | grep -q 'x86-64' \
      || die "$b.exe is not an x86-64 PE executable"
  done
  bundle_mingw_dlls "$stage"

  make_zip "$name"
}

# build_android <abi>
build_android() {
  local abi="$1"
  local bd="$BUILD_ROOT/android-$abi"
  local name="$PKG_PREFIX-android-$abi-$VERSION"
  local ndk="${ANDROID_NDK:-${ANDROID_NDK_HOME:-}}"
  [ -n "$ndk" ] && [ -f "$ndk/build/cmake/android.toolchain.cmake" ] \
    || die "ANDROID_NDK is unset or has no build/cmake/android.toolchain.cmake"
  local ucontext="$ANDROID_LIBUCONTEXT_BASE/$abi"
  [ -f "$ucontext/lib/libucontext.a" ] \
    || die "libucontext for $abi not found at $ucontext/lib/libucontext.a (the image only builds the ABIs in its ANDROID_ABIS build argument)"

  # Same configuration as scripts/cross-build-android-cli.sh.
  configure_and_build "$bd" \
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DWRKZ_BUILD_EXECUTABLES=ON \
    -DWRKZ_BUILD_WALLET_CAPI=OFF \
    -DWRKZ_ANDROID_PROFILE=OFF \
    -DWRKZ_ANDROID_DISABLE_OPENSSL=ON \
    -DENABLE_ZMQ=OFF \
    -DLIBUCONTEXT_ROOT="$ucontext"

  local stage b
  stage="$(stage_dir "$name")"
  stage_binaries "$bd/src" "$stage" ""

  log "Stripping and verifying Android ($abi) executables"
  local strip="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
  if [ -x "$strip" ]; then
    for b in "${BINARIES[@]}"; do
      "$strip" "$stage/$b"
    done
  else
    echo "warning: $strip not found; shipping unstripped executables"
  fi
  print_files "$stage" ""
  local expect=""
  case "$abi" in
    arm64-v8a)   expect='aarch64' ;;
    armeabi-v7a) expect='ARM,' ;;
    x86_64)      expect='x86-64' ;;
    x86)         expect='Intel 80386' ;;
  esac
  if [ -n "$expect" ]; then
    for b in "${BINARIES[@]}"; do
      file "$stage/$b" | grep -q -- "$expect" \
        || die "$b is not a $abi executable"
    done
  fi

  make_tarball "$name"
}

build_macos() {
  die "macOS is not built by this image yet. It needs an Apple SDK tarball you supply yourself plus an osxcross toolchain; scripts/cross-platform/README.md ('macOS from Ubuntu') has the manual flow and scripts/docker/README.md lists what adding it to the image involves."
}

# run_target <target>: run one build function with its output tee'd to a log.
run_target() {
  local t="$1" fn log="$BUILD_ROOT/logs/$t.log"
  case "$t" in
    linux)     fn=build_linux ;;
    windows)   fn=build_windows ;;
    macos)     fn=build_macos ;;
    android-*) fn="build_android ${t#android-}" ;;
    *)         die "unknown target: $t" ;;
  esac
  log "[$t] started $(date -u '+%Y-%m-%d %H:%M:%S') UTC  (log: $log)"
  # shellcheck disable=SC2086  # fn may carry the ABI argument
  if $fn 2>&1 | tee "$log"; then
    log "[$t] done $(date -u '+%Y-%m-%d %H:%M:%S') UTC"
    return 0
  fi
  log "[$t] FAILED; see $log"
  return 1
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
  if [ -z "$JOBS" ]; then
    JOBS="$(nproc)"
  fi
  case "$JOBS" in
    ''|*[!0-9]*|0) die "JOBS must be a positive integer (got '$JOBS')" ;;
  esac
  export CMAKE_BUILD_PARALLEL_LEVEL="$JOBS"

  if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
    GENERATOR="Unix Makefiles"
  fi

  if [ -z "$VERSION" ]; then
    VERSION="$(read_version)"
  fi
  resolve_binaries

  # Expand the requested targets into one entry per build tree.
  local requested=("$@") targets=() t abi seen
  if [ "${#requested[@]}" -eq 0 ]; then
    requested=(all)
  fi
  ANDROID_ABIS="$(printf '%s' "$ANDROID_ABIS" | tr ',;' '  ')"
  for t in "${requested[@]}"; do
    case "$t" in
      all)
        targets+=(linux windows)
        for abi in $ANDROID_ABIS; do targets+=("android-$abi"); done
        ;;
      android)
        for abi in $ANDROID_ABIS; do targets+=("android-$abi"); done
        ;;
      linux|windows|macos)
        targets+=("$t")
        ;;
      *)
        die "unknown target '$t' (use: linux, windows, android, macos, all)"
        ;;
    esac
  done
  # Drop duplicates while keeping order.
  local unique=()
  for t in "${targets[@]}"; do
    seen=0
    for abi in ${unique[@]+"${unique[@]}"}; do
      [ "$abi" = "$t" ] && seen=1
    done
    [ "$seen" -eq 1 ] || unique+=("$t")
  done
  targets=("${unique[@]}")

  mkdir -p "$OUT_DIR" "$BUILD_ROOT/logs" "$BUILD_ROOT/pkg"

  log "WrkzCoin CLI release build"
  echo "  version:     $VERSION"
  echo "  targets:     ${targets[*]}"
  echo "  executables: ${BINARIES[*]}"
  echo "  jobs:        $JOBS"
  echo "  generator:   $GENERATOR"
  echo "  source:      $REPO_ROOT"
  echo "  build trees: $BUILD_ROOT"
  echo "  packages:    $OUT_DIR"
  if command -v ccache >/dev/null 2>&1; then
    echo "  ccache:      ${CCACHE_DIR:-default location}"
  fi

  local failed=() ok=()
  for t in "${targets[@]}"; do
    if run_target "$t"; then
      ok+=("$t")
    else
      failed+=("$t")
      if [ "$KEEP_GOING" != "1" ]; then
        break
      fi
    fi
  done

  log "Summary"
  local pkg sums="$OUT_DIR/SHA256SUMS-$VERSION.txt"
  shopt -s nullglob
  local pkgs=("$OUT_DIR/$PKG_PREFIX-"*"-$VERSION.tar.gz" "$OUT_DIR/$PKG_PREFIX-"*"-$VERSION.zip")
  shopt -u nullglob
  if [ "${#pkgs[@]}" -gt 0 ]; then
    (cd "$OUT_DIR" && sha256sum "${pkgs[@]##*/}" > "$sums")
    for pkg in "${pkgs[@]}"; do
      printf '  %-60s %s\n' "$(basename "$pkg")" "$(du -h "$pkg" | cut -f1)"
    done
    echo "  checksums: $sums"
  fi
  if [ "${#ok[@]}" -gt 0 ]; then
    echo "  built:  ${ok[*]}"
  fi
  if [ "${#failed[@]}" -gt 0 ]; then
    echo "  FAILED: ${failed[*]}"
    local skipped=$(( ${#targets[@]} - ${#ok[@]} - ${#failed[@]} ))
    if [ "$skipped" -gt 0 ]; then
      echo "  skipped: $skipped target(s) after the failure (KEEP_GOING=1 to continue past failures)"
    fi
    exit 1
  fi
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  main "$@"
fi
