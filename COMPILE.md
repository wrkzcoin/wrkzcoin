### Compiling WrkzCoin

This document reflects the current build system and recommended defaults.

## Quick Start (Linux/macOS)

```bash
git clone https://github.com/wrkzcoin/wrkzcoin
cd wrkzcoin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binaries are produced in `build/src`.

### Build Parallelism (Top-level + RocksDB sub-build)

RocksDB is built as a nested CMake sub-build. To keep parallel job count aligned across both top-level build and RocksDB, set:

```bash
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
cmake --build build -j
```

You can also override only RocksDB jobs at configure time:

```bash
cmake -S . -B build -DROCKSDB_BUILD_PARALLEL=4
```

## Build Speed

- `-DWRKZ_ENABLE_PCH=ON` enables precompiled headers for the larger internal
  targets. Measure before adopting it: on MSVC it made a clean build slower
  (the per-target PCH files are 150-280 MB); GCC/Clang load PCH much more
  cheaply. With ccache, set `CCACHE_SLOPPINESS=pch_defines,time_macros`.
- `-DWRKZ_LINKER=mold` (or `lld`, `gold`) links with an alternative linker when
  the toolchain supports `-fuse-ld=`; otherwise the default linker is kept.
- The RocksDB sub-build is only configured once per build directory (or when
  its arguments change), so re-running `cmake` is fast. Its auto-detected job
  count is capped by `ROCKSDB_BUILD_PARALLEL_CAP` (default 8) unless
  `ROCKSDB_BUILD_PARALLEL`, `CMAKE_BUILD_PARALLEL_LEVEL` or `make -jN` is used.
- Wallet-only builds (`-DWRKZ_BUILD_EXECUTABLES=OFF`, e.g. the Android /
  wallet_capi flows) skip zstd and RocksDB entirely.

## Build Modes

### Default (recommended for nodes / reproducible)

The project now defaults to:

- `ARCH=default` (portable target, no `-march=native`)
- `CONSENSUS_SAFE_BUILD=ON`

This uses safer/reproducible release flags (for example `-O2` instead of `-Ofast`).

### Performance build (local benchmark/testing only)

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCONSENSUS_SAFE_BUILD=OFF \
  -DARCH=native
cmake --build build -j
```

Use performance mode only when you explicitly want host-specific optimization.

### Static build (portable)

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPORTABLE_BINARY=ON \
  -DENABLE_X86_AESNI=OFF \
  -DFULLY_STATIC=ON
cmake --build build -j
```

When changing target architecture/toolchain (for example x86_64 -> aarch64 or vice versa), use a fresh build directory or clear CMake cache first to avoid stale `-march`/ISA flags.

## Dependencies

### Linux

Minimum tooling:

- CMake >= 3.16
- C++ compiler:
  - GCC >= 7, or
  - Clang >= 6
- C++20 `<chrono>` compatibility note (Linux):
  - Confirmed: `clang-15` compiles successfully.
  - `clang-14` with GCC 13 `libstdc++` headers is known to fail in `<chrono>`.
  - Use GCC, `clang >= 15`, clang with `libc++`, or point clang at an older GCC toolchain (for example GCC 11 headers/libs).
- OpenSSL development package (`libssl-dev`)
- zlib development package (`zlib1g-dev`) — required for static OpenSSL linking
- Git
- Make or Ninja

Database/compression libs used by build:

- RocksDB
- Zstd

### Optional: ZeroMQ (daemon publisher)

ZMQ support is enabled by default at configure time (`ENABLE_ZMQ=ON`), but it is auto-disabled with a CMake warning if `libzmq` is not found.

- Runtime feature: daemon `--zmq-pub` and `--no-zmq`
- Default endpoint: `tcp://127.0.0.1:17857`

For Linux builds:

- Install headers/libs: `libzmq3-dev` and `libsodium-dev`
- For portable/static builds, make sure static archives exist:
  - `/usr/lib/x86_64-linux-gnu/libzmq.a`
  - `/usr/lib/x86_64-linux-gnu/libsodium.a`

If you changed dependencies, use a fresh build dir (or clear cache) so CMake does not reuse stale `ZMQ_LIBRARY` values.

## Ubuntu example

```bash
sudo apt update
sudo apt install -y \
  build-essential git cmake pkg-config \
  libzstd-dev libzmq3-dev libsodium-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## macOS example (Homebrew)

```bash
brew install cmake openssl zstd
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If OpenSSL is not auto-detected, pass:

```bash
-DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
```

## Windows (Visual Studio)

Open **x64 Native Tools Command Prompt for VS 2019/2022**:

```bat
cd <your_wrkzcoin_directory>
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- /m
```

Binaries are produced in `build/src/Release`.

## Optional: Faster incremental builds

Use Ninja and ccache when available:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`ccache` is auto-enabled by CMake when installed.
