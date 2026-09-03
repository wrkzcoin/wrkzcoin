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

A `-static` link against glibc prints linker warnings of the form
`Using 'getaddrinfo' in statically linked applications requires at runtime the shared libraries from the glibc version used for linking`
(also for `dlopen`, `gethostbyname` and `getgrnam`). They are expected for any static glibc binary and do not affect the build.
Name and group lookups go through NSS; with glibc 2.34 or newer the `files` and `dns` backends are built into libc, so DNS
seed resolution and `--rpc-ipc-group` work as long as `/etc/nsswitch.conf` only uses those two. Other NSS backends
(for example `sss`, `ldap`, `mdns`, `resolve`) need a runtime glibc that matches the build machine.

When changing target architecture/toolchain (for example x86_64 -> aarch64 or vice versa), use a fresh build directory or clear CMake cache first to avoid stale `-march`/ISA flags.

## Dependencies

### Linux

Minimum tooling:

- CMake >= 3.16
- A C++20 compiler (the build sets `CMAKE_CXX_STANDARD 20`, and CMake refuses older toolchains):
  - GCC >= 10, or
  - Clang >= 10 (`clang >= 15` on Linux, see below), or
  - MSVC 19.29 or later (Visual Studio 2019 16.11, or Visual Studio 2022)
- C++20 `<chrono>` compatibility note (Linux):
  - Confirmed: `clang-15` compiles successfully.
  - `clang-14` with GCC 13 `libstdc++` headers is known to fail in `<chrono>`.
  - Use GCC, `clang >= 15`, clang with `libc++`, or point clang at an older GCC toolchain (for example GCC 11 headers/libs).
- Git
- Make or Ninja

Optional, detected at configure time and skipped with a status line when absent:

- OpenSSL development package (`libssl-dev`): HTTPS in cpp-httplib, which the RPC clients use to reach `https://` daemons and notify-hook URLs
- zlib development package (`zlib1g-dev`): compressed HTTP bodies; also needed to link a static OpenSSL

Everything else is bundled under `external/` and built as part of the tree: RocksDB and zstd
(daemon only), libzmq, miniupnpc, argon2, and the header-only cpp-httplib, cxxopts,
nlohmann-json and linenoise. `-DWRKZ_SYSTEM_ROCKSDB=ON` links a system RocksDB and zstd
instead (`librocksdb-dev`, `libzstd-dev`).

### Optional: ZeroMQ (daemon publisher)

ZMQ support is enabled by default at configure time (`ENABLE_ZMQ=ON`). It builds the bundled
`external/libzmq` as a static library with CURVE off, so no system libzmq or libsodium is
needed. Pass `-DENABLE_ZMQ=OFF` to leave it out.

- Runtime feature: daemon `--zmq-pub` and `--no-zmq`
- Default endpoint: `tcp://127.0.0.1:17857`

## Ubuntu example

```bash
sudo apt update
sudo apt install -y \
  build-essential git cmake pkg-config \
  libssl-dev zlib1g-dev
```

The last two packages are optional (HTTPS support); everything else the build needs is bundled.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## macOS example (Homebrew)

```bash
brew install cmake openssl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If OpenSSL is not auto-detected, pass:

```bash
-DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
```

## Windows (Visual Studio)

Open **x64 Native Tools Command Prompt for VS 2022** (Visual Studio 2019 16.11 or later also
works, with `-G "Visual Studio 16 2019"`):

```bat
cd <your_wrkzcoin_directory>
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
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
