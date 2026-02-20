# Cross Build (Ubuntu Host)

This folder documents Linux-hosted cross-build flows.

ARM64 scope in this repo:

- `aarch64` means Linux ARM64 cross-builds (GNU/Linux target).
- `macOS arm64` means Apple Silicon macOS targets via osxcross.

## Windows x86_64 from Ubuntu

From repo root:

```bash
# 1) One-step build (runs prep automatically unless SKIP_PREP=1)
bash scripts/cross-build-windows-x86_64.sh

# 2) Package .exe artifacts
bash scripts/package-windows-x86_64.sh
```

Optional manual prep:

```bash
source scripts/prep-windows-x86_64.sh
SKIP_PREP=1 bash scripts/cross-build-windows-x86_64.sh
```

Build output directory:

```text
build-windows-x86_64/src
```

Package output directory:

```text
builds/wrkzcoin-windows-x86_64-<timestamp>.zip
```

Build log:

```text
build-logs/cross-build-windows-x86_64.log
```

Parallel jobs:

- `JOBS` is capped at a maximum of `8` in the build script.
- Example: `JOBS=4 bash scripts/cross-build-windows-x86_64.sh`
- Cross scripts also set `CMAKE_BUILD_PARALLEL_LEVEL=$JOBS` and
  `-DROCKSDB_BUILD_PARALLEL=$JOBS` so RocksDB nested build uses the same
  parallelism.

Runtime note:

- Windows builds using MinGW POSIX may require runtime DLLs (for example
  `libwinpthread-1.dll`).
- `scripts/package-windows-x86_64.sh` now auto-includes common MinGW runtime
  DLLs in the zip when found on the Ubuntu host.

## Linux aarch64 from Ubuntu

This repo includes a legacy Linux ARM64 cross-build flow used by CI.

From repo root:

```bash
# 1) Prepare aarch64 cross toolchain + Boost/OpenSSL
source scripts/prep-aarch64.sh

# 2) Configure and build
rm -rf build-aarch64
mkdir -p build-aarch64 && cd build-aarch64
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../scripts/cross-aarch64.cmake \
  -DARCH=default \
  -DCMAKE_BUILD_TYPE=Release \
  -DPORTABLE_BINARY=ON \
  -DENABLE_X86_AESNI=OFF \
  -DFULLY_STATIC=ON \
  -DROCKSDB_BUILD_PARALLEL=4 \
  -DSTATIC=true
export CMAKE_BUILD_PARALLEL_LEVEL=4
cmake --build . --parallel 4
```

Optional explicit toolchain file:

```bash
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=scripts/cross-aarch64.cmake \
  -DARCH=default \
  -DCMAKE_BUILD_TYPE=Release \
  -DPORTABLE_BINARY=ON \
  -DENABLE_X86_AESNI=OFF \
  -DFULLY_STATIC=ON \
  -DROCKSDB_BUILD_PARALLEL=4 \
  -DSTATIC=true
export CMAKE_BUILD_PARALLEL_LEVEL=4
cmake --build build-aarch64 --parallel 4
```

Build output directory:

```text
build-aarch64/src
```

Note:

- If you switch between different targets (for example `build-windows-x86_64` and `build-aarch64`), do not reuse the same build directory.
- Prefer deleting and recreating the target build directory before reconfiguring to avoid stale architecture flags.

- `scripts/prep-aarch64.sh` installs Ubuntu cross packages and builds static
  target deps (`OpenSSL` and `Boost.DateTime`) into:
  - `$HOME/toolchain/aarch64-linux-gnu/prefix` (default)
- You can override locations/versions with:
  - `TOOLCHAIN_DIR`
  - `CROSS_PREFIX`
  - `TARGET_TRIPLE`
  - `OPENSSL_VERSION`
  - `BOOST_VERSION`

## macOS from Ubuntu (osxcross)

### Prerequisites

1. You must provide an Apple macOS SDK tarball yourself (license requirement).
2. Supported SDK tarball names:
   - `MacOSX*.sdk.tar.xz`
   - `MacOSX*.sdk.tar.gz`
3. Place SDK tarball in:
   - `$HOME/toolchain/macos/sdk`
   - or provide explicit path with `OSXCROSS_SDK_TAR=/path/to/MacOSX*.sdk.tar.xz`
4. osxcross-compatible target libraries (Boost/OpenSSL) are still required for successful full link.
   - Export as needed before build:
   - `BOOST_ROOT=<path>`
   - `OPENSSL_ROOT_DIR=<path>`
   - `CMAKE_PREFIX_PATH=<path>`
5. Compiler wrappers expected from osxcross:
   - x86_64: `o64-clang`, `o64-clang++`
   - arm64: `oa64-clang`, `oa64-clang++`
   - triplet compilers are used as fallback.

### Build macOS x86_64

```bash
source scripts/prep-macos-osxcross.sh
bash scripts/cross-build-macos.sh x86_64
bash scripts/package-macos.sh build-macos-x86_64 builds "$(date +%Y%m%d-%H%M)" x86_64
```

### Build macOS arm64

```bash
source scripts/prep-macos-osxcross.sh
bash scripts/cross-build-macos.sh arm64
bash scripts/package-macos.sh build-macos-arm64 builds "$(date +%Y%m%d-%H%M)" arm64
```

Build output directories:

```text
build-macos-x86_64/src
build-macos-arm64/src
```

Package output directories:

```text
builds/wrkzcoin-macos-x86_64-<timestamp>.tar.gz
builds/wrkzcoin-macos-arm64-<timestamp>.tar.gz
```

## Android wallet library from Ubuntu/Linux

This repo provides a dedicated Android cross-build helper for the generic
wallet C API shared library:

- Script: `scripts/cross-build-android-wallet.sh`
- CMake target: `wallet_capi`
- Artifact: `libwallet_capi.so`

Prerequisites:

1. Android NDK installed (r26+ recommended).
2. `ANDROID_NDK` exported to your local NDK path.
3. Boost headers available on host (`libboost-dev`) or a custom `BOOST_ROOT`.
4. Android `libucontext` built for the target ABI.
5. `cmake` and `ninja`/build tools available on host.

Build default Android ABI (`arm64-v8a`):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
sudo apt-get install -y libboost-dev
bash scripts/cross-build-android-wallet.sh
```

Output:

```text
build-android-arm64/src/libwallet_capi.so
```

Build another ABI (example `x86_64`):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ANDROID_ABI=x86_64 BUILD_DIR=build-android-x86_64 \
  bash scripts/cross-build-android-wallet.sh
```

Common environment knobs:

- `ANDROID_ABI` (default: `arm64-v8a`)
- `ANDROID_PLATFORM` (default: `android-24`)
- `BUILD_TYPE` (default: `Release`)
- `BUILD_DIR` (default: `build-android-arm64`)
- `JOBS` (default: `nproc`)
- `BOOST_ROOT` (optional custom isolated Boost prefix containing `include/boost/version.hpp`)
- `LIBUCONTEXT_ROOT` (default: `.android-libucontext/<ABI>`, must contain `lib/libucontext.a`)

Notes:

- Script enables wallet-lib profile flags:
  - `WRKZ_ANDROID_PROFILE=ON`
  - `WRKZ_BUILD_EXECUTABLES=OFF`
  - `WRKZ_BUILD_WALLET_CAPI=ON`
  - `ENABLE_ZMQ=OFF`
- If `BOOST_ROOT` is not provided, the script stages `/usr/include/boost` into
  `.android-boost/include/boost` and uses that isolated path for cross build.
- `LIBUCONTEXT_ROOT` must point to a prebuilt Android libucontext prefix for
  the selected ABI. The build stops early if `lib/libucontext.a` is missing.
- Use separate `BUILD_DIR` per ABI to avoid stale CMake cache.

## Notes

- Windows prep builds target OpenSSL and Boost under:
  - `$HOME/toolchain/windows-x86_64/prefix`
- Override common locations with:
  - `TOOLCHAIN_DIR`, `CROSS_PREFIX`, `MINGW_PREFIX`
- Skip prep when environment is already ready:
  - `SKIP_PREP=1 bash scripts/cross-build-windows-x86_64.sh`
  - `SKIP_PREP=1 bash scripts/cross-build-macos.sh x86_64`
- macOS toolchain file inputs:
  - `OSXCROSS_ROOT`
  - `OSXCROSS_TARGET` (default: `x86_64-apple-darwin22.4`)
  - `OSXCROSS_DEPLOYMENT_TARGET` (default: `10.15` in prep)
