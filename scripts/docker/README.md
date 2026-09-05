# Docker release builds

One command builds the portable WrkzCoin CLI set for every supported platform
and packs each one for release, with the root `LICENSE` inside:

| Target    | Package                                        | How it is built                                   |
|-----------|------------------------------------------------|---------------------------------------------------|
| `linux`   | `wrkzcoin-cli-linux-x86_64-<version>.tar.gz`   | native GCC, fully static glibc binaries           |
| `windows` | `wrkzcoin-cli-windows-x86_64-<version>.zip`    | MinGW-w64 (posix threads) + static OpenSSL        |
| `android` | `wrkzcoin-cli-android-<abi>-<version>.tar.gz`  | Android NDK, one package per ABI, API 24+         |
| `macos`   | not yet, see [macOS](#macos) below             |                                                   |

`<version>` is `MAJOR.MINOR.REV.BUILD` from `src/config/version.h.in`
(for example `0.4.8.280`); override it with `VERSION=`.

Every package contains, flat inside a directory of the same name:

```
Wrkzd  wrkz-wallet  wrkz-service  wrkz-wallet-api  wallet-upgrader
miner  cryptotest  wrkz-txpow-server  LICENSE
```

The executable list is taken from the checked-out `src/CMakeLists.txt`, so a
branch that does not build `wrkz-txpow-server` yet gets a package without it
(the build log notes the omission). The Windows zip also carries whichever MinGW runtime DLLs the executables
import (typically `libwinpthread-1.dll`), detected from the binaries rather
than guessed, so the zip runs on a machine with nothing installed. A
`SHA256SUMS-<version>.txt` is written next to the packages.

## Requirements

- Docker 20.10 or newer (or Podman: `DOCKER=podman`). Any Linux host works;
  Docker Desktop on macOS/Windows works too, the image is always `linux/amd64`.
- About 12 GB of free disk for the image (the Android NDK is most of it) and
  another 6-8 GB for the build trees.
- RAM: the RocksDB and C++20 sources need roughly 1.5 GB per compile job.
  A 4 GB machine should use `JOBS=2`.
- Network access the first time, to fetch the base image, the NDK, OpenSSL
  and libucontext. Later runs are offline.

## Usage

From the repository root:

```bash
# Everything: Linux, Windows and Android (arm64-v8a)
bash scripts/docker/build.sh

# A subset
bash scripts/docker/build.sh linux
bash scripts/docker/build.sh windows android

# Both Android ABIs the image carries
ANDROID_ABIS="arm64-v8a x86_64" bash scripts/docker/build.sh android

# Fewer compile jobs on a small machine
JOBS=2 bash scripts/docker/build.sh
```

The first run builds the toolchain image (10-20 minutes, mostly downloads and
the OpenSSL / libucontext compiles). After that the image is cached and each
run goes straight to the build. The build trees under `build-docker/` and the
ccache inside it survive between runs, so a rebuild after a small source
change is quick; `CLEAN=1` starts the affected targets from scratch.

Packages land in `builds/`:

```
builds/
  wrkzcoin-cli-linux-x86_64-0.4.8.280.tar.gz
  wrkzcoin-cli-windows-x86_64-0.4.8.280.zip
  wrkzcoin-cli-android-arm64-v8a-0.4.8.280.tar.gz
  SHA256SUMS-0.4.8.280.txt
```

Per-target logs are in `build-docker/logs/`.

### Options

All options are environment variables. Targets are positional arguments.

| Variable           | Default                       | Meaning                                                                  |
|--------------------|-------------------------------|--------------------------------------------------------------------------|
| `JOBS`             | all CPUs in the container     | parallel compile jobs (also used for the nested RocksDB build)           |
| `VERSION`          | from `src/config/version.h.in`| version string in the package names                                      |
| `ANDROID_ABIS`     | `arm64-v8a`                   | ABIs to build for `android`; space or comma separated                    |
| `OUT_DIR`          | `builds/`                     | where packages and checksums go                                          |
| `BUILD_ROOT`       | `build-docker/`               | build trees, staging directories, ccache and logs                        |
| `CLEAN`            | `0`                           | `1` wipes each requested target's build tree before configuring          |
| `KEEP_GOING`       | `0`                           | `1` keeps building the remaining targets after one fails                 |
| `IMAGE`            | `wrkzcoin-cli-builder:latest` | image name                                                               |
| `NO_IMAGE_BUILD`   | `0`                           | `1` skips the `docker build` step (use an image you already have)        |
| `IMAGE_BUILD_ARGS` | empty                         | extra `docker build` arguments, see [Toolchain versions](#toolchain-versions) |
| `DOCKER`           | `docker`                      | container CLI                                                            |
| `DOCKER_PLATFORM`  | `linux/amd64`                 | image platform; keep amd64, the NDK has no other Linux build            |

Flags:

- `--shell` opens an interactive shell in the image with the repository at
  `/work`, the build trees at `/build` and the package directory at `/out`.
  Inside it, `bash /work/scripts/docker/container-build.sh linux` does exactly
  what `build.sh linux` does.
- `--image-only` builds or refreshes the image and stops.

On Linux the container runs as your user (under `sudo`, as the user who ran
`sudo`), so nothing in `build-docker/` or `builds/` ends up root-owned.

## What each target does

The flags mirror the manual flows in [COMPILE.md](../../COMPILE.md) and
[scripts/cross-platform/README.md](../cross-platform/README.md); the Docker
image just supplies the toolchains those documents ask you to install.

**Linux** configures with `-DFULLY_STATIC=ON -DPORTABLE_BINARY=ON
-DENABLE_X86_AESNI=OFF -DARCH=default -DCONSENSUS_SAFE_BUILD=ON`, links against
the base image's static OpenSSL and zlib, and checks every executable with
`file` for `statically linked` before running `Wrkzd --version` and
`wrkz-wallet --version`. The glibc NSS caveat from COMPILE.md applies:
name lookups use the `files` and `dns` backends built into glibc 2.34+, which
is what every mainstream distribution ships.

**Windows** uses `scripts/cross-windows-x86_64.cmake` with the OpenSSL that the
image built for the target (`no-shared`), strips the executables with the
MinGW `strip`, then reads the import tables with `objdump` and copies every
`lib*.dll` they reference into the package.

**Android** uses the NDK's CMake toolchain with the same switches as
`scripts/cross-build-android-cli.sh` (`WRKZ_BUILD_EXECUTABLES=ON`,
`WRKZ_ANDROID_DISABLE_OPENSSL=ON`, `ENABLE_ZMQ=OFF`, `android-24`), links the
static libucontext the image built for the ABI, strips with `llvm-strip` and
checks the ELF machine type. The binaries link the static libc++, so they
only depend on bionic. They run under Termux or any shell with a writable
directory.

## Toolchain versions

Everything the image downloads is pinned at the top of the
[Dockerfile](Dockerfile) and can be changed with `--build-arg`:

| Build argument        | Default                                      | Used for                                   |
|-----------------------|----------------------------------------------|--------------------------------------------|
| `UBUNTU_VERSION`      | `24.04`                                      | base image: GCC 13, MinGW-w64 GCC 13, CMake 3.28 |
| `OPENSSL_VERSION`     | `3.5.8`                                      | Windows-target OpenSSL (the Linux target uses `libssl-dev`) |
| `ANDROID_NDK_VERSION` | `r27d`                                       | Android NDK (r27 is the LTS line)          |
| `ANDROID_NDK_SHA1`    | checksum of r27d                             | verified after download; set empty to skip |
| `LIBUCONTEXT_REF`     | `libucontext-1.5.2`                          | libucontext tag built per Android ABI      |
| `ANDROID_ABIS`        | `arm64-v8a x86_64`                           | ABIs that get a libucontext in the image   |
| `ANDROID_PLATFORM`    | `24`                                         | libucontext target API level               |

Example, moving to a newer NDK:

```bash
IMAGE_BUILD_ARGS="--build-arg ANDROID_NDK_VERSION=r29 --build-arg ANDROID_NDK_SHA1=<sha1 from developer.android.com>" \
  bash scripts/docker/build.sh --image-only
```

The image is built from the `scripts/` directory only; the repository is
bind-mounted at run time, never copied in. Editing anything outside the
Dockerfile and `scripts/build-libucontext-android.sh` does not invalidate it.

## Cleaning up

```bash
rm -rf build-docker builds          # build trees, ccache, logs, packages
docker rmi wrkzcoin-cli-builder     # the image
```

## Troubleshooting

- **Compiler killed / `c++: fatal error: Killed signal`**: out of memory.
  Lower `JOBS`, or give Docker Desktop more memory.
- **`libucontext for <abi> not found`**: the image only carries the ABIs in
  its `ANDROID_ABIS` build argument (`arm64-v8a x86_64` by default). Rebuild
  the image with the ABI added.
- **`... is imported by the Windows executables but was not found`**: a
  MinGW runtime DLL the toolchain links against is missing from the image.
  Report the DLL name; the lookup lives in `find_mingw_dll` in
  `container-build.sh`.
- **Stale configuration after switching branches**: `CLEAN=1`.
- **Windows host, Git Bash**: `build.sh` converts paths for Docker Desktop
  itself. The repository should be checked out with LF line endings for the
  scripts under `scripts/docker/` (the `.gitattributes` rule takes care of a
  fresh clone).

## macOS

Not in the image yet. Two things are needed that the Linux, Windows and
Android targets do not have:

1. An Apple macOS SDK tarball (`MacOSX*.sdk.tar.xz`), which Apple's license
   does not allow us to redistribute or bake into a public image. It has to be
   supplied by whoever builds the image.
2. The osxcross toolchain built against that SDK, plus an OpenSSL built for
   the macOS target.

The manual flow (`scripts/prep-macos-osxcross.sh`, `scripts/cross-build-macos.sh`,
`scripts/package-macos.sh`) already exists and is documented in
[scripts/cross-platform/README.md](../cross-platform/README.md). Adding it here
means an optional image stage that takes the SDK tarball as a build secret or
a mounted file, runs the osxcross build, cross-compiles OpenSSL, and a
`build_macos` in `container-build.sh` that packages `x86_64` and `arm64` as
`wrkzcoin-cli-macos-<arch>-<version>.tar.gz`. Until then `build.sh macos`
prints this explanation and exits non-zero.
