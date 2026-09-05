# Docker release builds

One command builds the portable WrkzCoin CLI set for every supported platform
and packs each one for release, with the root `LICENSE` inside:

| Target    | Package                                        | How it is built                                   |
|-----------|------------------------------------------------|---------------------------------------------------|
| `linux`   | `wrkzcoin-cli-linux-x86_64-<version>.tar.gz`   | native GCC, fully static glibc binaries           |
| `windows` | `wrkzcoin-cli-windows-x86_64-<version>.zip`    | MinGW-w64 (posix threads) + static OpenSSL        |
| `android` | `wrkzcoin-cli-android-<abi>-<version>.tar.gz`  | Android NDK, one package per ABI, API 24+         |
| `macos`   | not yet, see [macOS](#macos) below             |                                                   |

The same image also builds and packs the wallet applications under `extras/`,
using toolchain stages that are off by default (see
[Wallet applications](#wallet-applications)):

| Target    | Package                                        | How it is built                                   |
|-----------|------------------------------------------------|---------------------------------------------------|
| `web`     | `pluton-web-<appversion>.tar.gz`               | Emscripten WASM module + Flutter web bundle       |
| `desktop` | `pluton-desktop-linux-x86_64-<appversion>.tar.gz` | Flutter Linux (GTK) bundle + `libwallet_capi.so` |
| `mobile`  | `pluton-mobile-android-<appversion>[-debug].apk` / `.aab` | Flutter Android + `libwallet_capi.so` per ABI, release and debug |
| `apps`    | all three                                       |                                                   |

Windows and macOS desktop builds and iOS are **not** possible from this image:
`flutter build windows` needs MSVC on Windows and the Apple targets need Xcode
on macOS. Only the Windows `wallet_capi.dll` cross-builds, with
`scripts/cross-build-windows-wallet-lib.sh`.

`<version>` is `MAJOR.MINOR.REV.BUILD` from `src/config/version.h.in`
(for example `0.4.8.280`); override it with `VERSION=`. The applications carry
their own `<appversion>` from their `pubspec.yaml` (PLUTON 2.0.0 desktop and
mobile, 1.0.0 web), because they release on their own schedule; every package
produced by a run is still listed in that run's `SHA256SUMS-<version>.txt`.

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
  another 6-8 GB for the build trees. The application stages add roughly
  2 GB (Flutter), 1.5 GB (Emscripten) and 3 GB (Android SDK) on top, and only
  when you ask for a target that needs them.
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

# The wallet applications (each adds its toolchain to the image on first use)
bash scripts/docker/build.sh web
bash scripts/docker/build.sh desktop mobile
bash scripts/docker/build.sh apps

# APK only, for one ABI
MOBILE_FORMATS=apk ANDROID_ABIS=arm64-v8a bash scripts/docker/build.sh mobile

# release artefacts only, skipping the debug ones
MOBILE_MODES=release bash scripts/docker/build.sh mobile
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
  pluton-web-1.0.0.tar.gz
  pluton-desktop-linux-x86_64-2.0.0.tar.gz
  pluton-mobile-android-2.0.0.apk
  pluton-mobile-android-2.0.0.aab
  pluton-mobile-android-2.0.0-debug.apk
  pluton-mobile-android-2.0.0-debug.aab
  SHA256SUMS-0.4.8.280.txt
```

Per-target logs are in `build-docker/logs/`.

### Options

All options are environment variables. Targets are positional arguments.

| Variable           | Default                       | Meaning                                                                  |
|--------------------|-------------------------------|--------------------------------------------------------------------------|
| `JOBS`             | all CPUs in the container     | parallel compile jobs (also used for the nested RocksDB build)           |
| `VERSION`          | from `src/config/version.h.in`| version string in the package names                                      |
| `ANDROID_ABIS`     | `arm64-v8a`                   | ABIs to build for `android`; space or comma separated. Also the ABIs the `mobile` package carries and targets |
| `MOBILE_FORMATS`   | `apk aab`                     | Android artefacts the `mobile` target produces                            |
| `MOBILE_MODES`     | `release debug`               | Android build modes; debug artefacts get a `-debug` name suffix           |
| `WEB_PTHREADS`     | `1`                           | build the WASM module with pthreads (`0` is single-threaded and slower)   |
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

## Wallet applications

The three Flutter applications in `extras/` are built from the same image, but
each needs a toolchain the CLI targets do not: Flutter for all of them,
Emscripten for the web module, an Android SDK and JDK for the APK. Those stages
sit at the end of the Dockerfile and are off by default; `build.sh` switches on
the ones your targets need (`--build-arg WITH_FLUTTER=1` and friends) and
rebuilds the image, so the first `build.sh web` takes a while and later ones do
not. Switching between CLI-only and application builds does not thrash the
cache: only the trailing stages differ, and Docker keeps both variants.

Every application is built in a **copy** of its directory under
`build-docker/apps/`, never in the bind-mounted checkout, so a container build
does not fight with your own `flutter run` tree or leave outputs behind in
`extras/`.

**web** builds the WASM wallet module with `emcmake` (`WRKZ_BUILD_WALLET_WASM=ON`,
pthreads unless `WEB_PTHREADS=0`), copies `wallet_wasm.{js,wasm,worker.js}` and
the three bridge scripts from `extras/web-wallet-wasm/wasm/js/` into the app's
`web/` directory - both copies are re-made every build, because that directory
holds duplicates that have silently drifted before - then runs
`flutter build web --release --no-web-resources-cdn` (CanvasKit is self-hosted,
so the bundle needs no CDN at run time). The package is the contents of
`build/web`, plus `LICENSE`. With pthreads it also carries a `SERVING.txt`: the
page only works when the server sends `Cross-Origin-Opener-Policy: same-origin`
and `Cross-Origin-Embedder-Policy: require-corp`.

**desktop** builds `libwallet_capi.so` (`WRKZ_BUILD_EXECUTABLES=OFF`,
`WRKZ_BUILD_WALLET_CAPI=ON`, `PORTABLE_BINARY=ON`) and the Flutter Linux
bundle, then stages the bundle with the stripped library in `lib/` - the
runner's RUNPATH is `$ORIGIN/lib` - and a `libwallet_capi.so` symlink beside
the executable for a plain `dlopen`. Unlike the CLI, a GTK application cannot
be statically linked: the bundle needs the glibc and GTK 3 of the image's base
(Ubuntu 24.04, glibc 2.39) or newer on the user's machine. Build the image
`--build-arg UBUNTU_VERSION=22.04` for a lower floor, at the cost of an older
GCC for every other target.

**mobile** builds `libwallet_capi.so` for each ABI in `ANDROID_ABIS` against
the NDK and the image's libucontext, drops them into `jniLibs/` (replacing the
prebuilt ones the checkout carries), and runs `flutter build apk` and
`flutter build appbundle` limited to those ABIs, so no device gets an APK
without a matching native library. Gradle downloads its own distribution and
the app's dependencies on the first run into `build-docker/.gradle`, so that
build needs network access even when the image is already built.
`android/app/build.gradle` signs release builds with the **debug** key: the
`.apk` installs from a download, but re-sign the `.aab` with your own keystore
before uploading it to a store.

By default the target builds each artefact twice, once per entry in
`MOBILE_MODES`. The release artefact keeps the plain name; the debug one is
suffixed `-debug`. A debug build runs the Dart VM in JIT mode with the debug
banner drawn and the observatory port open, which makes it several times
larger and much slower - give it to testers who need logs, not to users, and
never upload it to a store. Both modes package the same `Release`-built
`libwallet_capi.so`, which is compiled once per ABI and reused, so the second
mode costs only its Gradle and Dart work. Set `MOBILE_MODES=release` to get
the previous behaviour.

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

The application stages, off unless the target needs them (`build.sh` passes
these itself; set them by hand only for `--image-only`):

| Build argument                  | Default                                 | Used for                                          |
|---------------------------------|-----------------------------------------|---------------------------------------------------|
| `WITH_FLUTTER`                  | `0`                                     | Flutter SDK + clang/GTK 3: every application target |
| `FLUTTER_VERSION`               | `3.38.7`                                | the apps ask for Flutter 3.38+ / Dart 3.10+       |
| `WITH_ANDROID_SDK`              | `0`                                     | Android SDK + JDK 17 for `mobile`                  |
| `ANDROID_CMDLINE_TOOLS_VERSION` | `13114758`                              | commandlinetools zip to fetch                      |
| `ANDROID_SDK_PACKAGES`          | `platform-tools platforms;android-36 build-tools;36.0.0` | matches the app's `compileSdk` |
| `ANDROID_SDK_NDK_VERSION`       | empty (detected from the Flutter SDK)   | the NDK Gradle wants for `flutter.ndkVersion`; separate from `/opt/android-ndk` |
| `WITH_EMSDK`                    | `0`                                     | Emscripten for the `web` WASM module               |
| `EMSDK_VERSION`                 | `3.1.50`                                | the version the first green web build used         |

```bash
# Prepare an image with every application toolchain, without building anything
bash scripts/docker/build.sh --image-only apps
```

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
- **`no Flutter SDK in this image` / `no Emscripten SDK` / `no Android SDK`**:
  the image predates the application stages, or was built with
  `NO_IMAGE_BUILD=1`. Run `bash scripts/docker/build.sh --image-only apps`.
- **Gradle asks for an NDK version that is not installed**: the app follows
  `flutter.ndkVersion`, which the image detects from the Flutter SDK. Pin it:
  `IMAGE_BUILD_ARGS="--build-arg WITH_FLUTTER=1 --build-arg WITH_ANDROID_SDK=1 --build-arg ANDROID_SDK_NDK_VERSION=<x.y.z>"`.
- **`flutter pub get` or Gradle fails to download**: application builds need
  network access every run (pub.dev, Gradle distributions, Maven). The caches
  under `build-docker/.pub-cache` and `build-docker/.gradle` make later runs
  cheap but not offline.
- **The web wallet loads and then waits forever**: the server is not sending
  the two COOP/COEP headers the pthread build needs (`SERVING.txt` in the
  package), or `wallet_wasm.wasm` did not reach the document root.
- **`GLIBC_2.39 not found` from the desktop bundle**: the target machine is
  older than the image's base. Rebuild with
  `IMAGE_BUILD_ARGS="--build-arg UBUNTU_VERSION=22.04 --build-arg WITH_FLUTTER=1"`.
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
