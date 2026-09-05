# Building from Source

Builds the daemon and the CLI binaries: `Wrkzd`, `wrkz-wallet`,
`wrkz-wallet-api`, `wrkz-service`, `wrkz-txpow-server`, `miner`, `cryptotest`
and `wallet-upgrader`. For the graphical wallets see
[Building the Wallet Apps](building-wallet-apps.md).

The in-repo long form is `COMPILE.md`, which this page tracks.

## Quick start (Linux / macOS)

```bash
git clone https://github.com/wrkzcoin/wrkzcoin
cd wrkzcoin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binaries land in `build/src`.

## Windows (Visual Studio)

From an **x64 Native Tools Command Prompt for VS 2022** (VS 2019 16.11 or later
also works, with `-G "Visual Studio 16 2019"`):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /m
```

Binaries land in `build/src/Release`.

## Requirements

| | |
| --- | --- |
| CMake | 3.16 or later |
| Compiler | **GCC 10+**, **Clang 10+** (15+ on Linux), or **MSVC 19.29+** (VS 2019 16.11 / VS 2022) |
| Also | Git, and Make or Ninja |

The tree is C++20 and CMake refuses older toolchains outright.

!!! warning "clang 14 on Linux will not build this"
    `clang-14` against GCC 13's `libstdc++` headers fails inside `<chrono>`.
    Use GCC, `clang >= 15`, clang with `libc++`, or point clang at an older GCC
    toolchain (GCC 11 headers and libraries work).

### Optional dependencies

Both are detected at configure time and skipped with a status line when absent —
but you almost certainly want them on a node.

| Package | What you lose without it |
| --- | --- |
| OpenSSL (`libssl-dev`) | HTTPS in cpp-httplib: reaching `https://` daemons, HTTPS notify-hook URLs, and TLS to a remote [Tx PoW server](txpow-server.md) |
| zlib (`zlib1g-dev`) | Compressed HTTP bodies. **Every syncing wallet then pulls several times as many bytes.** Also needed to link a static OpenSSL |

Check a running daemon with `curl -s $DAEMON_RPC_URL/info | grep compression` —
`"none"` means the build found no zlib.

Everything else is bundled under `external/` and built with the tree: RocksDB and
zstd (daemon only), libzmq, miniupnpc, argon2, and the header-only cpp-httplib,
cxxopts, nlohmann-json and linenoise. `-DWRKZ_SYSTEM_ROCKSDB=ON` links a system
RocksDB and zstd instead.

### Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential git cmake pkg-config libssl-dev zlib1g-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS (Homebrew)

```bash
brew install cmake openssl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If OpenSSL is not found, add `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl)`.

## Build modes

### Default — use this for a node

`ARCH=default` (portable, no `-march=native`) and `CONSENSUS_SAFE_BUILD=ON`,
which uses safer and reproducible release flags (`-O2` rather than `-Ofast`).

### Performance — local benchmarking only

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCONSENSUS_SAFE_BUILD=OFF \
  -DARCH=native
```

Host-specific optimization. Do not ship the result.

### Static — portable binaries

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPORTABLE_BINARY=ON \
  -DENABLE_X86_AESNI=OFF \
  -DFULLY_STATIC=ON
```

A static glibc link prints warnings of the form *"Using `getaddrinfo` in
statically linked applications requires at runtime the shared libraries from the
glibc version used for linking"* (also for `dlopen`, `gethostbyname`,
`getgrnam`). **These are expected and harmless.** Name and group lookups go
through NSS; with glibc 2.34 or newer the `files` and `dns` backends are inside
libc, so DNS seed resolution and `--rpc-ipc-group` work as long as
`/etc/nsswitch.conf` uses only those two. Other backends (`sss`, `ldap`,
`mdns`, `resolve`) need a runtime glibc matching the build machine.

!!! note "Switching target architecture"
    Going from x86_64 to aarch64 or back needs a **fresh build directory**, or
    at least a cleared CMake cache — otherwise stale `-march` and ISA flags
    survive.

## Build speed

| Option | Effect |
| --- | --- |
| `-G Ninja` | Faster incremental builds. `ccache` is auto-enabled when installed |
| `-DWRKZ_ENABLE_PCH=ON` | Precompiled headers for the larger targets. **Measure first** — on MSVC it made a clean build *slower* (per-target PCH files run 150-280 MB); GCC and Clang load them much more cheaply. With ccache set `CCACHE_SLOPPINESS=pch_defines,time_macros` |
| `-DWRKZ_LINKER=mold` | Or `lld` / `gold`, when the toolchain supports `-fuse-ld=`. Falls back to the default linker |
| `-DWRKZ_BUILD_EXECUTABLES=OFF` | Wallet-library-only builds skip zstd and RocksDB entirely |

RocksDB is a nested CMake sub-build. To keep the job count aligned across both:

```bash
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
cmake --build build -j
```

Or set RocksDB's alone at configure time with `-DROCKSDB_BUILD_PARALLEL=4`. The
sub-build is configured once per build directory, so re-running `cmake` is
cheap; its auto-detected job count is capped by `ROCKSDB_BUILD_PARALLEL_CAP`
(default 8).

## ZeroMQ

On by default (`ENABLE_ZMQ=ON`). It builds bundled `external/libzmq` as a static
library with CURVE off, so no system libzmq or libsodium is needed. Pass
`-DENABLE_ZMQ=OFF` to leave it out — the daemon then ignores `--zmq-pub`. See
[Networking](networking.md#zmq-publisher).

## Release packages with Docker

One command builds and packs the portable CLI set for Linux (static), Windows
(MinGW) and Android inside a single image, needing nothing on the host but
Docker:

```bash
bash scripts/docker/build.sh
```

| Target | Package |
| --- | --- |
| `linux` | `wrkzcoin-cli-linux-x86_64-<version>.tar.gz` — native GCC, fully static glibc |
| `windows` | `wrkzcoin-cli-windows-x86_64-<version>.zip` — MinGW-w64 with static OpenSSL |
| `android` | `wrkzcoin-cli-android-<abi>-<version>.tar.gz` — NDK, one per ABI, API 24+ |
| `macos` | Not yet |

The same image also builds the wallet apps (`web`, `desktop`, `mobile`, or
`apps` for all three) from toolchain stages that are off by default. Windows and
macOS *desktop* builds and iOS are **not** possible from it — `flutter build
windows` needs MSVC on Windows and the Apple targets need Xcode on macOS. Only
the Windows `wallet_capi.dll` cross-builds, with
`scripts/cross-build-windows-wallet-lib.sh`.

`<version>` is `MAJOR.MINOR.REV.BUILD` from `src/config/version.h.in`; the apps
carry their own version from their `pubspec.yaml`. Every package in a run is
listed in that run's `SHA256SUMS-<version>.txt`.

Details in `scripts/docker/README.md`.

## Cross-building on an Ubuntu host

`scripts/cross-platform/README.md` documents Linux-hosted cross-build flows for
aarch64, Windows, Android and macOS (via osxcross), split into two tracks:
wallet library only (`wallet_capi`), and full node/CLI binaries. Use the Docker
image above unless you specifically need a prepared host.

## Known build traps

- **MSVC and ZSTD.** RocksDB's CMake keeps its compression block in the
  `else()` arm of `if(MSVC)`, so `ZSTD` was never defined and RocksDB compiled
  the support out — every MSVC-built `Wrkzd` from this tree failed to create a
  database. Fixed in `external/CMakeLists.txt` as of 0.4.8. If you hit it on an
  older checkout, rebuild rather than working around it with
  `--db-enable-compression=false`.
- **A stale `build/` tree** silently references sources deleted on another
  branch and fails with `C1083`. Re-run `cmake -S . -B build` after switching
  branches.
- **A running `Wrkzd` or `wrkz-txpow-server` locks its own executable** on
  Windows, so the next link fails with `LNK1104`. Stop them first.
