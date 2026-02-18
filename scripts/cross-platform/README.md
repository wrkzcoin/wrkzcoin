# Cross Build (Ubuntu Host)

This folder documents Linux-hosted cross-build flows.

## Windows x86_64 from Ubuntu

From repo root:

```bash
# 1) Prepare toolchain and target dependencies (MinGW + OpenSSL + Boost)
source scripts/prep-windows-x86_64.sh

# 2) Configure and build all targets for Windows
scripts/cross-build-windows-x86_64.sh

# 3) Package .exe artifacts
scripts/package-windows-x86_64.sh
```

Build output directory:

```text
build-windows-x86_64/src
```

Package output directory:

```text
builds/wrkzcoin-windows-x86_64-<timestamp>.zip
```

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
scripts/cross-build-macos.sh x86_64
scripts/package-macos.sh build-macos-x86_64 builds "$(date +%Y%m%d-%H%M)" x86_64
```

### Build macOS arm64

```bash
source scripts/prep-macos-osxcross.sh
scripts/cross-build-macos.sh arm64
scripts/package-macos.sh build-macos-arm64 builds "$(date +%Y%m%d-%H%M)" arm64
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

## Notes

- Windows prep builds target OpenSSL and Boost under:
  - `$HOME/toolchain/windows-x86_64/prefix`
- Override common locations with:
  - `TOOLCHAIN_DIR`, `CROSS_PREFIX`, `MINGW_PREFIX`
- Skip prep when environment is already ready:
  - `SKIP_PREP=1 scripts/cross-build-windows-x86_64.sh`
  - `SKIP_PREP=1 scripts/cross-build-macos.sh x86_64`
- macOS toolchain file inputs:
  - `OSXCROSS_ROOT`
  - `OSXCROSS_TARGET` (default: `x86_64-apple-darwin22.4`)
  - `OSXCROSS_DEPLOYMENT_TARGET` (default: `10.15` in prep)
