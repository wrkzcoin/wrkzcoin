# Cross Build Guide (Ubuntu Host)

This folder documents Linux-hosted cross-build flows.

ARM64 scope in this repo:

- `aarch64` means Linux ARM64 cross-builds (GNU/Linux target).
- `macOS arm64` means Apple Silicon macOS targets via osxcross.

## Build Tracks

Use one of these tracks depending on output type:

1. Wallet library only (`wallet_capi` / `wallet_capi_c`)
2. Full node/CLI binaries (daemon/service/miner/etc.)

## Wallet Library Builds

### Linux Native (Static by Default)

From repo root:

```bash
bash scripts/build-linux-wallet-lib.sh
```

Default artifact:

```text
build-linux-wallet-capi/src/libwallet_capi_c.a
```

Optional output kinds:

```bash
# Shared only
WALLET_LIB_KIND=shared bash scripts/build-linux-wallet-lib.sh

# Static + shared
WALLET_LIB_KIND=both bash scripts/build-linux-wallet-lib.sh
```

### Windows x86_64 (Cross from Ubuntu, Static by Default)

From repo root:

```bash
# One-step prep + cross-build of wallet C API only
bash scripts/cross-build-windows-wallet-lib.sh
```

Skip prep if already prepared:

```bash
SKIP_PREP=1 bash scripts/cross-build-windows-wallet-lib.sh
```

Output directory:

```text
build-windows-wallet-x86_64/src
```

Default artifact:

```text
build-windows-wallet-x86_64/src/libwallet_capi_c.a
```

Build log:

```text
build-logs/cross-build-windows-wallet-lib.log
```

Optional output kinds:

```bash
# Shared only
WALLET_LIB_KIND=shared bash scripts/cross-build-windows-wallet-lib.sh

# Static + shared
WALLET_LIB_KIND=both bash scripts/cross-build-windows-wallet-lib.sh
```

### Android (Shared Library)

Android flow currently targets shared `wallet_capi` (`libwallet_capi.so`).

Prerequisites:

1. Android NDK installed (r26+ recommended).
2. `ANDROID_NDK` exported to your local NDK path.
4. Android `libucontext` built for the target ABI.
5. `cmake` and `ninja`/build tools available on host.

Build default Android ABI (`arm64-v8a`):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
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

Build both `arm64-v8a` and `x86_64` in one command:

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ANDROID_ABIS=arm64-v8a,x86_64 LIBUCONTEXT_ROOT_BASE="$PWD/.android-libucontext" \
bash scripts/cross-build-android-wallet.sh
```

Build `libucontext` per ABI (recommended before wallet build):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ABI=arm64-v8a scripts/build-libucontext-android.sh
ABI=x86_64 scripts/build-libucontext-android.sh
```

`scripts/build-libucontext-android.sh` auto-applies an Android bionic
x86_64 compatibility patch to libucontext register macros (`REG_*`) to avoid
`sys/ucontext.h` enum name collisions.

Common Android env knobs:

- `ANDROID_ABI` (default: `arm64-v8a`)
- `ANDROID_ABIS` (optional list; supports space/comma/newline separators, e.g. `"arm64-v8a x86_64"` or `"arm64-v8a,x86_64"`)
- `ANDROID_PLATFORM` (default: `android-24`)
- `BUILD_TYPE` (default: `Release`)
- `BUILD_DIR` (default: `build-android-arm64`)
- `JOBS` (default: `nproc`)
- `LIBUCONTEXT_ROOT` (default: `.android-libucontext/<ABI>`, must contain `lib/libucontext.a`)
- `LIBUCONTEXT_ROOT_BASE` (default: `.android-libucontext`, used when `ANDROID_ABIS` is set)

Android notes:

- Script enables wallet-lib profile flags:
  - `WRKZ_ANDROID_PROFILE=ON`
  - `WRKZ_BUILD_EXECUTABLES=OFF`
  - `WRKZ_BUILD_WALLET_CAPI=ON`
  - `ENABLE_ZMQ=OFF`
- If `libucontext` is missing for an ABI, the script warns and continues.
  Final link may still fail on `getcontext/swapcontext/makecontext`.
- Use separate `BUILD_DIR` per ABI to avoid stale CMake cache.

## Full Node/CLI Builds

### Android (CLI Binaries)

Android CLI flow builds daemon and CLI executables for Android ABIs.

Prerequisites:

1. Android NDK installed (r26+ recommended).
2. `ANDROID_NDK` exported to your local NDK path.
4. Android `libucontext` built for the target ABI (recommended).
5. `cmake` and `ninja`/build tools available on host.

Build default Android ABI (`arm64-v8a`):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
bash scripts/cross-build-android-cli.sh
```

Output directory:

```text
build-android-cli-arm64-v8a/src
```

Build another ABI (example `x86_64`):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ANDROID_ABI=x86_64 BUILD_DIR=build-android-cli-x86_64 \
  bash scripts/cross-build-android-cli.sh
```

Build multiple ABIs in one command:

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ANDROID_ABIS=arm64-v8a,x86_64 LIBUCONTEXT_ROOT_BASE="$PWD/.android-libucontext" \
  bash scripts/cross-build-android-cli.sh
```

Build `libucontext` per ABI (recommended before Android CLI build):

```bash
export ANDROID_NDK="$HOME/Android/Sdk/ndk/26.3.11579264"
ABI=arm64-v8a scripts/build-libucontext-android.sh
ABI=x86_64 scripts/build-libucontext-android.sh
```

Expected Android CLI artifacts (per ABI build dir under `src/`):

- `Wrkzd`
- `wrkz-wallet`
- `wrkz-service`
- `wrkz-wallet-api`
- `wallet-upgrader`
- `miner`
- `cryptotest`

Common Android CLI env knobs:

- `ANDROID_ABI` (default: `arm64-v8a`)
- `ANDROID_ABIS` (optional list; supports space/comma/newline separators)
- `ANDROID_PLATFORM` (default: `android-24`)
- `BUILD_TYPE` (default: `Release`)
- `BUILD_DIR` (default: `build-android-cli-arm64-v8a`)
- `JOBS` (default: `nproc`)
- `LIBUCONTEXT_ROOT` (default: `.android-libucontext/<ABI>`, optional but recommended)
- `LIBUCONTEXT_ROOT_BASE` (default: `.android-libucontext`, used when `ANDROID_ABIS` is set)

Android CLI notes:

- Script configures with:
  - `WRKZ_BUILD_EXECUTABLES=ON`
  - `WRKZ_BUILD_WALLET_CAPI=OFF`
  - `WRKZ_ANDROID_PROFILE=OFF`
  - `WRKZ_ANDROID_DISABLE_OPENSSL=ON`
  - `ENABLE_ZMQ=OFF`
- If `libucontext` is missing for an ABI, the script warns and continues.
  Final link may still fail on `getcontext/swapcontext/makecontext`.
- Use separate `BUILD_DIR` per ABI to avoid stale CMake cache.

### Windows x86_64 from Ubuntu

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
- `scripts/package-windows-x86_64.sh` auto-includes common MinGW runtime
  DLLs in the zip when found on the Ubuntu host.

### Linux aarch64 from Ubuntu

This repo includes a legacy Linux ARM64 cross-build flow used by CI.

From repo root:

```bash
# 1) Prepare aarch64 cross toolchain + OpenSSL
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

### macOS from Ubuntu (osxcross)

#### Prerequisites

1. You must provide an Apple macOS SDK tarball yourself (license requirement).
2. Supported SDK tarball names:
   - `MacOSX*.sdk.tar.xz`
   - `MacOSX*.sdk.tar.gz`
3. Place SDK tarball in:
   - `$HOME/toolchain/macos/sdk`
   - or provide explicit path with `OSXCROSS_SDK_TAR=/path/to/MacOSX*.sdk.tar.xz`
4. osxcross-compatible target OpenSSL is still required for successful full link.
   - Export as needed before build:
   - `OPENSSL_ROOT_DIR=<path>`
   - `CMAKE_PREFIX_PATH=<path>`
5. Compiler wrappers expected from osxcross:
   - x86_64: `o64-clang`, `o64-clang++`
   - arm64: `oa64-clang`, `oa64-clang++`
   - triplet compilers are used as fallback.

#### Build macOS x86_64

```bash
source scripts/prep-macos-osxcross.sh
bash scripts/cross-build-macos.sh x86_64
bash scripts/package-macos.sh build-macos-x86_64 builds "$(date +%Y%m%d-%H%M)" x86_64
```

#### Build macOS arm64

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

## Shared Notes

- If you switch between different targets (for example `build-windows-x86_64` and `build-aarch64`), do not reuse the same build directory.
- Prefer deleting and recreating the target build directory before reconfiguring to avoid stale architecture flags.
- Windows prep builds target OpenSSL under:
  - `$HOME/toolchain/windows-x86_64/prefix`
- Linux aarch64 prep builds target OpenSSL under:
  - `$HOME/toolchain/aarch64-linux-gnu/prefix` (default)
- Override common toolchain locations with:
  - `TOOLCHAIN_DIR`, `CROSS_PREFIX`
- Windows prep-specific overrides:
  - `MINGW_PREFIX`
- aarch64 prep-specific overrides:
- Skip prep when environment is already ready:
  - `SKIP_PREP=1 bash scripts/cross-build-windows-x86_64.sh`
  - `SKIP_PREP=1 bash scripts/cross-build-windows-wallet-lib.sh`
  - `SKIP_PREP=1 bash scripts/cross-build-macos.sh x86_64`
- macOS toolchain file inputs:
  - `OSXCROSS_ROOT`
  - `OSXCROSS_TARGET` (default: `x86_64-apple-darwin22.4`)
  - `OSXCROSS_DEPLOYMENT_TARGET` (default: `10.15` in prep)
