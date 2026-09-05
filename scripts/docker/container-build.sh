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

# Flutter application targets (extras/*-wallet). These are packaged under their
# own prefix and their own pubspec version, which moves independently of the
# daemon's: PLUTON 2.0.0 ships against Wrkzd 0.4.8.x.
APP_PKG_PREFIX="${APP_PKG_PREFIX:-pluton}"
# The WASM module is built with pthreads, which is what the deployed web wallet
# uses. It makes the page require COOP/COEP headers - see SERVING.txt in the
# package. Set WEB_PTHREADS=0 for a single-threaded (much slower) build.
WEB_PTHREADS="${WEB_PTHREADS:-1}"
# Which Android artefacts the mobile target produces: apk, aab, or both.
MOBILE_FORMATS="${MOBILE_FORMATS:-apk aab}"
# Packages written by this run, recorded through a file because run_target
# executes each target in a pipeline (and therefore a subshell).
PACKAGE_LIST="$BUILD_ROOT/packages.list"

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

# configure_and_build_target <build-dir> <cmake target> [configure args...]
# Same as configure_and_build for the wallet libraries, which are one target in
# a tree that would otherwise build the whole daemon as well.
configure_and_build_target() {
  local bd="$1" target="$2"
  shift 2
  if [ "$CLEAN" = "1" ] && [ -d "$bd" ]; then
    log "CLEAN=1: removing $bd"
    rm -rf "$bd"
  fi
  mkdir -p "$bd"
  log "Configuring $bd (target $target)"
  cmake -S "$REPO_ROOT" -B "$bd" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DROCKSDB_BUILD_PARALLEL="$JOBS" \
    "$@"
  log "Building $target in $bd with $JOBS jobs"
  cmake --build "$bd" --target "$target" --parallel "$JOBS"
}

# ---------------------------------------------------------------------------
# Flutter application helpers
# ---------------------------------------------------------------------------

# read_app_version <app dir>: the pubspec version without its +build suffix.
read_app_version() {
  local v
  v="$(awk '$1 == "version:" { print $2; exit }' "$1/pubspec.yaml" | tr -d '\r')"
  v="${v%%+*}"
  [ -n "$v" ] || v="$VERSION"
  printf '%s' "$v"
}

# app_workdir <extras subdirectory>: a fresh copy of the application under
# $BUILD_ROOT/apps, path on stdout. Flutter writes its outputs into the project
# directory; building in a copy keeps the bind-mounted checkout clean and stops
# a container build from fighting with the developer's own build/ tree.
app_workdir() {
  local name="$1"
  local src="$REPO_ROOT/extras/$name"
  local dest="$BUILD_ROOT/apps/$name"
  [ -d "$src" ] || die "no such application: $src"
  rm -rf "$dest"
  mkdir -p "$dest"
  (cd "$src" && tar -cf - \
      --exclude=build \
      --exclude=.dart_tool \
      --exclude=.flutter-plugins \
      --exclude=.flutter-plugins-dependencies \
      .) | (cd "$dest" && tar -xf -)
  printf '%s' "$dest"
}

# flutter_env: check the SDK is in the image and point pub at a writable cache.
flutter_env() {
  command -v flutter >/dev/null 2>&1 || die \
    "no Flutter SDK in this image. Rebuild it with
    IMAGE_BUILD_ARGS=\"--build-arg WITH_FLUTTER=1\" bash scripts/docker/build.sh --image-only
  (build.sh adds that automatically when you ask for an application target)."
  export PUB_CACHE="${PUB_CACHE:-$BUILD_ROOT/.pub-cache}"
  export FLUTTER_SUPPRESS_ANALYTICS=true
  mkdir -p "$PUB_CACHE"
  flutter --version
}

# emsdk_env: put emcc/emcmake on PATH with a writable cache.
emsdk_env() {
  local emsdk="${EMSDK:-/opt/emsdk}"
  [ -f "$emsdk/emsdk_env.sh" ] || die \
    "no Emscripten SDK in this image. Rebuild it with
    IMAGE_BUILD_ARGS=\"--build-arg WITH_EMSDK=1\" bash scripts/docker/build.sh --image-only
  (build.sh adds that automatically for the web target)."
  export EM_CACHE="${EM_CACHE:-$BUILD_ROOT/.emscripten-cache}"
  mkdir -p "$EM_CACHE"
  # shellcheck disable=SC1091  # sourced from the image, not the repository
  . "$emsdk/emsdk_env.sh" >/dev/null 2>&1
  command -v emcmake >/dev/null 2>&1 || die "emcmake not on PATH after sourcing $emsdk/emsdk_env.sh"
  emcc --version | head -1
}

# android_sdk_env: check the SDK + JDK the Gradle build needs.
android_sdk_env() {
  local sdk="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
  [ -n "$sdk" ] && [ -d "$sdk/platform-tools" ] || die \
    "no Android SDK in this image. Rebuild it with
    IMAGE_BUILD_ARGS=\"--build-arg WITH_FLUTTER=1 --build-arg WITH_ANDROID_SDK=1\" bash scripts/docker/build.sh --image-only
  (build.sh adds that automatically for the mobile target)."
  command -v java >/dev/null 2>&1 || die "no JDK in this image; the Android SDK stage installs one"
  export ANDROID_SDK_ROOT="$sdk" ANDROID_HOME="$sdk"
  export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$BUILD_ROOT/.gradle}"
  # A Gradle daemon would outlive the container run for nothing.
  export GRADLE_OPTS="${GRADLE_OPTS:-} -Dorg.gradle.daemon=false"
  mkdir -p "$GRADLE_USER_HOME"
}

# flutter_target_platform <abi...>: the --target-platform value for an ABI list.
flutter_target_platform() {
  local abi out=""
  for abi in "$@"; do
    case "$abi" in
      arm64-v8a)   out="$out,android-arm64" ;;
      armeabi-v7a) out="$out,android-arm" ;;
      x86_64)      out="$out,android-x64" ;;
      x86)         out="$out,android-x86" ;;
      *)           die "no Flutter target platform for ABI '$abi'" ;;
    esac
  done
  printf '%s' "${out#,}"
}

# build_wallet_capi_android <abi> <destination directory>
build_wallet_capi_android() {
  local abi="$1" dest="$2"
  local bd="$BUILD_ROOT/wallet-capi-android-$abi"
  local ndk="${ANDROID_NDK:-${ANDROID_NDK_HOME:-}}"
  [ -n "$ndk" ] && [ -f "$ndk/build/cmake/android.toolchain.cmake" ] \
    || die "ANDROID_NDK is unset or has no build/cmake/android.toolchain.cmake"
  local ucontext="$ANDROID_LIBUCONTEXT_BASE/$abi"
  [ -f "$ucontext/lib/libucontext.a" ] \
    || die "libucontext for $abi not found at $ucontext/lib/libucontext.a (the image only builds the ABIs in its ANDROID_ABIS build argument)"

  # Same configuration as scripts/cross-build-android-wallet.sh.
  configure_and_build_target "$bd" wallet_capi \
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DWRKZ_ANDROID_PROFILE=ON \
    -DWRKZ_BUILD_EXECUTABLES=OFF \
    -DWRKZ_BUILD_WALLET_CAPI=ON \
    -DWRKZ_ANDROID_DISABLE_OPENSSL=ON \
    -DENABLE_ZMQ=OFF \
    -DLIBUCONTEXT_ROOT="$ucontext"

  [ -f "$bd/src/libwallet_capi.so" ] || die "missing $bd/src/libwallet_capi.so"
  mkdir -p "$dest"
  cp "$bd/src/libwallet_capi.so" "$dest/libwallet_capi.so"
  local strip="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
  if [ -x "$strip" ]; then
    "$strip" "$dest/libwallet_capi.so"
  fi
  file "$dest/libwallet_capi.so"
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

# record_package <path>: remember a package this run produced, so the summary
# can checksum it even though each target runs in its own subshell.
record_package() {
  printf '%s\n' "$1" >> "$PACKAGE_LIST"
}

# make_tarball <package-name>
make_tarball() {
  local name="$1" out="$OUT_DIR/$1.tar.gz"
  rm -f "$out"
  tar -C "$BUILD_ROOT/pkg" --owner=0 --group=0 --numeric-owner -czf "$out" "$name"
  record_package "$out"
  log "Package: $out"
}

# make_zip <package-name>
make_zip() {
  local name="$1" out="$OUT_DIR/$1.zip"
  rm -f "$out"
  (cd "$BUILD_ROOT/pkg" && zip -q -r -X "$out" "$name")
  record_package "$out"
  log "Package: $out"
}

# require_zip_entries <archive> <what> <entry...>: fail unless the archive
# contains every named entry. The listing is read in full before anything
# inspects it. "unzip -l ... | grep -q" looks equivalent but is not: grep exits
# at the first match, unzip takes SIGPIPE on its next write, and pipefail then
# reports a check that actually passed as a failure.
require_zip_entries() {
  local archive="$1" what="$2" listing entry
  shift 2
  [ -f "$archive" ] || die "expected artefact not found: $archive"
  listing="$(unzip -Z1 "$archive")" || die "cannot list $archive"
  for entry in "$@"; do
    case $'\n'"$listing"$'\n' in
      *$'\n'"$entry"$'\n'*) ;;
      *) die "$what carries no $entry" ;;
    esac
  done
  echo "$what carries: $*"
}

# ship_file <source file> <package file name>: copy a single-file artefact
# (an .apk, an .aab) into OUT_DIR under the release name.
ship_file() {
  local src="$1" out="$OUT_DIR/$2"
  [ -f "$src" ] || die "expected artefact not found: $src"
  rm -f "$out"
  cp "$src" "$out"
  record_package "$out"
  log "Package: $out"
}

# print_dir_summary <stage-dir>: size and file count of a staged bundle.
print_dir_summary() {
  local d="$1"
  echo "Staged $(find "$d" -type f | wc -l) files, $(du -sh "$d" | cut -f1) in $d"
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

# PLUTON Web: the Emscripten wallet module plus the Flutter web bundle.
build_web() {
  local app name stage appver
  flutter_env
  emsdk_env

  local bd="$BUILD_ROOT/wasm"
  if [ "$CLEAN" = "1" ] && [ -d "$bd" ]; then
    log "CLEAN=1: removing $bd"
    rm -rf "$bd"
  fi
  mkdir -p "$bd"
  local pthreads=OFF
  if [ "$WEB_PTHREADS" = "1" ]; then
    pthreads=ON
  fi
  log "Configuring the WASM wallet module in $bd (pthreads=$pthreads)"
  emcmake cmake -S "$REPO_ROOT" -B "$bd" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DWRKZ_BUILD_WALLET_CAPI=ON \
    -DWRKZ_BUILD_WALLET_WASM=ON \
    -DWRKZ_WASM_PTHREADS="$pthreads"
  log "Building wallet_wasm with $JOBS jobs"
  cmake --build "$bd" --target wallet_wasm --parallel "$JOBS"

  app="$(app_workdir web-wallet)"
  appver="$(read_app_version "$app")"
  name="$APP_PKG_PREFIX-web-$appver"

  # The three bridge scripts have a canonical copy under web-wallet-wasm and a
  # working copy in web/; they have drifted before, so re-copy every build.
  local js
  for js in wallet_bridge.js wallet_storage.js wallet_worker.js; do
    if [ -f "$REPO_ROOT/extras/web-wallet-wasm/wasm/js/$js" ]; then
      cp "$REPO_ROOT/extras/web-wallet-wasm/wasm/js/$js" "$app/web/$js"
    fi
  done
  # ... and the module itself, which is the step that fails silently when it is
  # skipped: the app just waits for walletBridgeReady forever.
  cp "$bd/wasm/wallet_wasm.js" "$app/web/wallet_wasm.js"
  cp "$bd/wasm/wallet_wasm.wasm" "$app/web/wallet_wasm.wasm"
  if [ "$pthreads" = "ON" ]; then
    if [ -f "$bd/wasm/wallet_wasm.worker.js" ]; then
      cp "$bd/wasm/wallet_wasm.worker.js" "$app/web/wallet_wasm.worker.js"
    else
      echo "note: this Emscripten version emits no wallet_wasm.worker.js"
    fi
  fi

  log "Building the Flutter web bundle"
  (cd "$app" && flutter pub get && flutter build web --release --no-web-resources-cdn)

  stage="$(stage_dir "$name")"
  cp -a "$app/build/web/." "$stage/"
  cp "$REPO_ROOT/LICENSE" "$stage/LICENSE"
  [ -f "$stage/index.html" ] || die "no index.html in the web bundle"
  [ -f "$stage/wallet_wasm.wasm" ] || die "the web bundle carries no wallet_wasm.wasm"
  if [ "$pthreads" = "ON" ]; then
    cat > "$stage/SERVING.txt" <<'EOF'
This build uses a pthread-enabled WebAssembly module, so the browser only
allocates its shared memory when the page is served with both of these
response headers:

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

nginx:

    add_header Cross-Origin-Opener-Policy   same-origin   always;
    add_header Cross-Origin-Embedder-Policy require-corp  always;

Without them the wallet loads and then waits forever for the WASM bridge.
Serve the directory as static files; no server-side code is needed.
EOF
  fi
  print_dir_summary "$stage"
  make_tarball "$name"
}

# PLUTON desktop wallet, Linux x86_64: the GTK bundle plus libwallet_capi.so.
build_desktop() {
  local app name stage appver bundle
  flutter_env

  local bd="$BUILD_ROOT/wallet-capi-linux"
  configure_and_build_target "$bd" wallet_capi \
    -DWRKZ_BUILD_EXECUTABLES=OFF \
    -DWRKZ_BUILD_WALLET_CAPI=ON \
    -DENABLE_ZMQ=OFF \
    -DARCH=default \
    -DPORTABLE_BINARY=ON \
    -DENABLE_X86_AESNI=OFF
  [ -f "$bd/src/libwallet_capi.so" ] || die "missing $bd/src/libwallet_capi.so"

  app="$(app_workdir desktop-wallet)"
  appver="$(read_app_version "$app")"
  name="$APP_PKG_PREFIX-desktop-linux-x86_64-$appver"

  log "Building the Flutter Linux bundle"
  (cd "$app" && flutter pub get && flutter build linux --release)

  bundle="$app/build/linux/x64/release/bundle"
  [ -x "$bundle/wrkz_wallet" ] || die "no wrkz_wallet executable in $bundle"

  stage="$(stage_dir "$name")"
  cp -a "$bundle/." "$stage/"
  # The runner's RUNPATH is $ORIGIN/lib; the symlink also covers a plain
  # dlopen("libwallet_capi.so") that resolves against the executable directory.
  mkdir -p "$stage/lib"
  cp "$bd/src/libwallet_capi.so" "$stage/lib/libwallet_capi.so"
  strip "$stage/lib/libwallet_capi.so"
  ln -sf lib/libwallet_capi.so "$stage/libwallet_capi.so"
  cp "$REPO_ROOT/LICENSE" "$stage/LICENSE"

  log "Verifying the desktop bundle"
  file "$stage/wrkz_wallet"
  file "$stage/wrkz_wallet" | grep -q 'ELF 64-bit.*x86-64' \
    || die "wrkz_wallet is not an x86-64 ELF executable"
  print_dir_summary "$stage"
  make_tarball "$name"
}

# PLUTON mobile wallet: an Android APK and/or AAB with libwallet_capi.so for
# each ABI in ANDROID_ABIS.
build_mobile() {
  local app name appver abi abis platforms fmt
  flutter_env
  android_sdk_env

  # shellcheck disable=SC2206  # deliberate word splitting of the ABI list
  abis=($ANDROID_ABIS)
  [ "${#abis[@]}" -gt 0 ] || die "ANDROID_ABIS is empty"
  platforms="$(flutter_target_platform "${abis[@]}")"

  app="$(app_workdir mobile-wallet)"
  appver="$(read_app_version "$app")"
  name="$APP_PKG_PREFIX-mobile-android-$appver"

  # The checkout carries prebuilt .so files in jniLibs; the package must ship
  # the ones this run built, and only for the ABIs the APK targets.
  rm -rf "$app/android/app/src/main/jniLibs"
  for abi in "${abis[@]}"; do
    log "Building libwallet_capi.so for $abi"
    build_wallet_capi_android "$abi" "$app/android/app/src/main/jniLibs/$abi"
  done

  log "Building the Android application ($MOBILE_FORMATS) for $platforms"
  (cd "$app" && flutter pub get)
  for fmt in $MOBILE_FORMATS; do
    case "$fmt" in
      apk)
        (cd "$app" && flutter build apk --release --target-platform "$platforms")
        local apk="$app/build/app/outputs/flutter-apk/app-release.apk"
        # An APK for an ABI whose libwallet_capi.so is missing installs and
        # then dies on the first FFI call, so check before shipping it.
        local want=()
        for abi in "${abis[@]}"; do want+=("lib/$abi/libwallet_capi.so"); done
        require_zip_entries "$apk" "the APK" "${want[@]}"
        ship_file "$apk" "$name.apk"
        ;;
      aab)
        (cd "$app" && flutter build appbundle --release --target-platform "$platforms")
        local aab="$app/build/app/outputs/bundle/release/app-release.aab"
        # Play serves one split per ABI, so a split without the library is
        # an install that dies on the first FFI call. A bundle keeps its
        # native libraries under the base module.
        local want=()
        for abi in "${abis[@]}"; do want+=("base/lib/$abi/libwallet_capi.so"); done
        require_zip_entries "$aab" "the AAB" "${want[@]}"
        ship_file "$aab" "$name.aab"
        ;;
      *)
        die "unknown MOBILE_FORMATS entry '$fmt' (use: apk, aab)"
        ;;
    esac
  done

  # android/app/build.gradle signs release builds with the debug key, so these
  # artefacts install from a download but are not Play Store uploads.
  echo "note: signed with the Android debug key (android/app/build.gradle);"
  echo "      re-sign with your own keystore before publishing to a store."
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
    web)       fn=build_web ;;
    desktop)   fn=build_desktop ;;
    mobile)    fn=build_mobile ;;
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
      apps)
        targets+=(web desktop mobile)
        ;;
      linux|windows|macos|web|desktop|mobile)
        targets+=("$t")
        ;;
      *)
        die "unknown target '$t' (use: linux, windows, android, macos, all, web, desktop, mobile, apps)"
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
  : > "$PACKAGE_LIST"

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
  local pkg extra sums="$OUT_DIR/SHA256SUMS-$VERSION.txt"
  shopt -s nullglob
  local pkgs=("$OUT_DIR/$PKG_PREFIX-"*"-$VERSION.tar.gz" "$OUT_DIR/$PKG_PREFIX-"*"-$VERSION.zip")
  # Application packages carry their own pubspec version, so they cannot be
  # globbed by $VERSION - match them on the prefix instead. Globbing them,
  # rather than trusting $PACKAGE_LIST alone, is what keeps a package an
  # earlier run produced in the checksum file: the list is truncated every
  # run, so a run that built only "mobile" used to drop the "web" line it
  # found there.
  pkgs+=("$OUT_DIR/$APP_PKG_PREFIX-"*.tar.gz "$OUT_DIR/$APP_PKG_PREFIX-"*.apk "$OUT_DIR/$APP_PKG_PREFIX-"*.aab)
  shopt -u nullglob
  # ... plus anything else this run recorded that those globs did not catch.
  if [ -s "$PACKAGE_LIST" ]; then
    while read -r extra; do
      [ -f "$extra" ] || continue
      for pkg in ${pkgs[@]+"${pkgs[@]}"}; do
        [ "$pkg" = "$extra" ] && continue 2
      done
      pkgs+=("$extra")
    done < "$PACKAGE_LIST"
  fi
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
