# Docker release builds

One command builds the portable WrkzCoin CLI set for every supported platform
and packs each one for release, with the root `LICENSE` inside:

| Target    | Package                                        | How it is built                                   |
|-----------|------------------------------------------------|---------------------------------------------------|
| `linux`   | `wrkzcoin-cli-linux-x86_64-<version>.tar.gz`   | native GCC, fully static glibc binaries           |
| `windows` | `wrkzcoin-cli-windows-x86_64-<version>.zip`    | MinGW-w64 (posix threads) + static OpenSSL        |
| `android` | `wrkzcoin-cli-android-<abi>-<version>.tar.gz`  | Android NDK, one package per ABI, API 24+         |
| `macos`   | `wrkzcoin-cli-macos-x86_64-<version>.tar.gz`   | osxcross, needs an Apple SDK you supply — see [macOS](#macos) |

`linux`, `windows` and `android` are what `all` builds. `macos` is opt-in
because it needs an SDK the image cannot ship, and it is x86_64 only (Apple
Silicon runs it under Rosetta 2); [macOS](#macos) explains both.

The same image also builds and packs the wallet applications under `extras/`,
using toolchain stages that are off by default (see
[Wallet applications](#wallet-applications)):

| Target    | Package                                        | How it is built                                   |
|-----------|------------------------------------------------|---------------------------------------------------|
| `web`     | `pluton-web-<appversion>.tar.gz`               | Emscripten WASM module + Flutter web bundle       |
| `desktop` | `pluton-desktop-linux-x86_64-<appversion>.tar.gz` | Flutter Linux (GTK) bundle + `libwallet_capi.so` |
| `mobile`  | `pluton-mobile-android-<appversion>[-debug].apk` / `.aab` | Flutter Android + `libwallet_capi.so` per ABI, release and debug |
| `apps`    | all three                                       |                                                   |

The *wallet applications* for Windows and macOS, and iOS, are **not** possible
from this image: `flutter build windows` needs MSVC on Windows and the Apple
targets need Xcode on macOS. (The `macos` CLI target above is unaffected — it
cross-builds command-line executables, not a Flutter bundle.) Only the Windows
`wallet_capi.dll` cross-builds, with
`scripts/cross-build-windows-wallet-lib.sh`.

`<version>` is `MAJOR.MINOR.REV.BUILD` from `src/config/version.h.in`
(for example `0.4.8.280`); override it with `VERSION=`. The applications carry
their own `<appversion>` from their `pubspec.yaml` (PLUTON 2.0.0 desktop and
mobile, 1.0.0 web), because they release on their own schedule; every package
produced by a run is still listed in that run's `SHA256SUMS-<version>.txt`.

Every package contains, flat inside a directory of the same name:

```
Wrkzd  wrkz-wallet  wrkz-service  wrkz-wallet-api  wallet-upgrader
miner  cryptotest  wrkz-txpow-server  wrkz-netmon  LICENSE
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
  when you ask for a target that needs them. The `macos` target adds ~1 GB of
  packages to the image and ~4 GB to the build tree, where its toolchain lives.
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

# macOS, once an Apple SDK tarball is in .macos-sdk/ (see "macOS" below)
bash scripts/docker/build.sh macos

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
  wrkzcoin-cli-macos-x86_64-0.4.8.280.tar.gz
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
| `MACOS_SDK`        | first in `.macos-sdk/`        | Apple SDK tarball for `macos`; also searched in `~/toolchain/macos/sdk/`. Set it explicitly if you keep more than one |
| `MACOS_DEPLOYMENT_TARGET` | `10.15`                | oldest macOS the `macos` package runs on                                 |
| `MACOS_ZMQ`        | `1`                           | build the daemon's ZMQ publisher into the `macos` package                |
| `OSXCROSS_REF`     | `master`                      | osxcross commit or branch; pin it for a reproducible toolchain           |
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

**macOS** uses `scripts/cross-macos-x86_64.cmake` against an osxcross toolchain
and a macOS-target OpenSSL, both built on the first run from the SDK you supply
(see [macOS](#macos)), strips with the osxcross `strip` and checks that every
executable is a 64-bit x86_64 Mach-O. Unlike the Linux target it cannot run a
`--version` smoke test, because nothing in the container executes Mach-O. The
package carries an `INSTALL.txt` with the `xattr -dr com.apple.quarantine` step
that unsigned downloads need.

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
| `WITH_OSXCROSS`                 | `0`                                     | clang/llvm/libxml2 etc. that osxcross needs, for `macos` |

`WITH_OSXCROSS` adds only the *packages* osxcross builds against. The toolchain
itself is not in the image — see [macOS](#macos).

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

`build-docker/` also holds the macOS toolchain, so removing it costs another
20-40 minutes on the next `macos` build. `rm -rf build-docker/toolchain/macos`
alone forces just that to be rebuilt.

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
- **Stale configuration after switching branches**: `CLEAN=1`. Note this does
  *not* rebuild the macOS toolchain, which is keyed on the SDK rather than on
  the build tree; delete `build-docker/toolchain/macos` to force that.
- **`no Apple SDK tarball found`**: see [macOS](#macos). `build.sh` looks in
  `.macos-sdk/`, in `~/toolchain/macos/sdk/`, and at `MACOS_SDK`.
- **The macOS build fails in `libzmq`**: the bundled ZeroMQ is the least
  exercised part of the macOS cross-build. `MACOS_ZMQ=0 bash
  scripts/docker/build.sh macos` drops the daemon's ZMQ publisher, as the
  Android packages already do.
- **`osxcross produced no o64-clang wrapper`**: the osxcross build failed
  earlier in `build-docker/logs/macos.log`. Most often the SDK tarball is not
  one osxcross recognises (it wants a `MacOSX<version>.sdk` directory at the
  root of the archive), or the image lacks the osxcross packages because it was
  built with `NO_IMAGE_BUILD=1`.
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

`bash scripts/docker/build.sh macos` produces
`wrkzcoin-cli-macos-x86_64-<version>.tar.gz` like any other target, with one
prerequisite you supply once per machine: an Apple SDK tarball.

### Why the SDK is not in the image

Apple's licence does not let us redistribute the macOS SDK, and it restricts
using it to Apple-branded hardware. So the image carries only the *packages*
osxcross needs (`--build-arg WITH_OSXCROSS=1`, which `build.sh` passes for you);
the toolchain itself — osxcross, then a static OpenSSL for the macOS target —
is built on the first `macos` run into `build-docker/toolchain/macos/`.

That build takes 20-40 minutes and needs network access (osxcross fetches
cctools/ld64 itself). It is then reused by every later run: a stamp file records
the SDK's SHA-256, the osxcross ref, the deployment target and the OpenSSL
version, and the toolchain is rebuilt only when one of those changes. Deleting
`build-docker/` throws it away along with everything else.

### Supplying the SDK

Get `MacOSX<version>.sdk.tar.xz` and drop it in `.macos-sdk/` at the repository
root (git-ignored), or point `MACOS_SDK` at it. Two ways to produce one:

- **On a Mac with Xcode**: osxcross's `tools/gen_sdk_package.sh` packs the SDK
  out of an installed Xcode.
- **On Linux**: download the *Command Line Tools for Xcode* disk image from
  <https://developer.apple.com/download/all/> (an Apple ID and accepting the
  licence are required, which is the one step no script should do for you),
  then run osxcross's `tools/gen_sdk_package_tools*.sh` against it to extract
  the SDK without needing a Mac.

Any SDK from roughly 10.15 onwards works; the toolchain triple
(`x86_64-apple-darwin24`, …) follows the SDK version and is detected, never
hard-coded.

```bash
mkdir -p .macos-sdk
cp /path/to/MacOSX15.2.sdk.tar.xz .macos-sdk/
bash scripts/docker/build.sh macos
```

If no SDK is found, `build.sh` says so and exits before building anything.

### x86_64 only

There is no arm64 package, and the blocker is the source tree rather than this
image: [`src/platform/osx/system/asm.s`](../../src/platform/osx/system/asm.s)
and `Context.h` next to it implement the fibre context switch the dispatcher
needs in x86-64 assembly, with no AArch64 version and no architecture guard.
An arm64 build fails to assemble — the same way a native build on an Apple
Silicon Mac does, so this is not something a different build host would fix.
`scripts/cross-macos-arm64.cmake` and `scripts/cross-build-macos.sh arm64`
are dead paths until someone writes that AArch64 context switch.

Apple Silicon machines run the x86_64 package under Rosetta 2 in the meantime
(`softwareupdate --install-rosetta`).

### Signing

The executables are cross-built, so they are neither signed nor notarised.
macOS quarantines anything downloaded and will refuse to run them until the
flag is cleared; the package's `INSTALL.txt` tells the user to run
`xattr -dr com.apple.quarantine .` once in the unpacked directory. Signing
properly needs an Apple Developer ID and `codesign`/`notarytool` on a Mac, so
it is a step for whoever publishes the release, not for this image.

### Relation to the standalone scripts

`scripts/prep-macos-osxcross.sh` and `scripts/cross-build-macos.sh` still
document the manual flow for a developer's own Ubuntu box (see
[scripts/cross-platform/README.md](../cross-platform/README.md)); the container
does not use them, because the prep script installs packages with `sudo` and
the image has neither `sudo` nor a root build user.
