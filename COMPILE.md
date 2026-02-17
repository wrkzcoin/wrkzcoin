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

## Dependencies

### Linux

Minimum tooling:

- CMake >= 3.8
- C++ compiler:
  - GCC >= 7, or
  - Clang >= 6
- OpenSSL development package
- Git
- Make or Ninja

Database/compression libs used by build:

- RocksDB
- LevelDB
- Snappy
- Zstd

### Boost status (updated)

Boost is no longer required as a broad/full dependency package in build docs.
Current CMake uses Boost `date_time` only.

- Linux packages: prefer minimal Boost dev packages (for example `libboost-date-time-dev`) instead of `libboost-all-dev`.
- Windows: if your toolchain cannot auto-resolve Boost, point CMake to your Boost install with `-DBOOST_ROOT=...`.

## Ubuntu example

```bash
sudo apt update
sudo apt install -y \
  build-essential git cmake pkg-config \
  libssl-dev libboost-date-time-dev \
  libsnappy-dev libzstd-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## macOS example (Homebrew)

```bash
brew install cmake openssl boost snappy zstd
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

If Boost is not discovered automatically:

```bat
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DBOOST_ROOT=C:/local/boost_1_69_0
```

Binaries are produced in `build/src/Release`.

## Optional: Faster incremental builds

Use Ninja and ccache when available:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`ccache` is auto-enabled by CMake when installed.
