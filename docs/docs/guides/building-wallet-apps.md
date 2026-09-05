# Building the Wallet Apps

The desktop, mobile and web wallets are Flutter apps that call `wallet_capi` —
the wallet backend compiled as a shared library — through FFI, or through
WebAssembly in the browser. **Building one is always two builds**: the native
library from this C++ tree, then the Flutter app that loads it.

For what the apps do once built, see
[Desktop, Mobile and Web Wallets](wallet-apps.md). For the daemon and CLI
binaries see [Building from Source](building.md).

Each app keeps the authoritative recipe in its own `README.md` under `extras/`.

## Step 1 — build `wallet_capi`

From the repository root. `-DWRKZ_BUILD_WALLET_CAPI=ON` is what adds the target.

=== "Windows"

    ```bash
    cmake -S . -B build -G "Visual Studio 17 2022" -DWRKZ_BUILD_WALLET_CAPI=ON
    cmake --build build --target wallet_capi --config Release
    ```

    Output: `build\src\Release\wallet_capi.dll`

=== "Linux"

    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWRKZ_BUILD_WALLET_CAPI=ON
    cmake --build build --target wallet_capi
    ```

    Output: `build/libwallet_capi.so`

=== "macOS"

    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWRKZ_BUILD_WALLET_CAPI=ON
    cmake --build build --target wallet_capi
    ```

    Output: `build/libwallet_capi.dylib`

A wallet-library-only build can also set `-DWRKZ_BUILD_EXECUTABLES=OFF`, which
skips zstd and RocksDB entirely and is much faster. That is what the Android
flow does.

## Desktop (Windows, Linux, macOS)

Source: `extras/desktop-wallet`. Requires Flutter 3.38.7+ / Dart 3.10.7+, CMake
3.14+, and Visual Studio 2022 with "Desktop development with C++" on Windows or
GCC/Clang 11+ elsewhere.

Enable desktop support once:

```bash
flutter config --enable-windows-desktop
flutter config --enable-linux-desktop
flutter config --enable-macos-desktop
```

### Place the library next to the Flutter executable

| Platform | Library | Destination |
| --- | --- | --- |
| Windows | `wallet_capi.dll` | `build\windows\x64\runner\Debug\` or `Release\` |
| Linux | `libwallet_capi.so` | `build/linux/x64/debug/bundle/` or `release/bundle/` |
| macOS | `libwallet_capi.dylib` | Next to `pluton_wallet.app` |

### Run and package

```bash
cd extras/desktop-wallet
flutter pub get
flutter run -d windows        # or linux / macos
```

```bash
# Windows
flutter build windows --release
copy build\Release\wallet_capi.dll build\windows\x64\runner\Release\
# -> build/windows/x64/runner/Release/pluton_wallet.exe

# Linux
flutter build linux --release
cp build/libwallet_capi.so build/linux/x64/release/bundle/
# -> build/linux/x64/release/bundle/pluton_wallet

# macOS
flutter build macos --release
cp build/libwallet_capi.dylib "build/macos/Build/Products/Release/"
# -> build/macos/Build/Products/Release/pluton_wallet.app
```

### Shipping a daemon with it

The desktop app can supervise a local node, but **`Wrkzd` is not bundled by the
Flutter build**. Build the normal daemon target and drop the binary beside the
app — about 9 MB on a release:

```bash
cmake --build build --target Wrkzd --config Release
copy build\src\Release\Wrkzd.exe build\windows\x64\runner\Release\
```

It is searched for in `$WRKZ_DAEMON_PATH`, next to the wallet executable, in a
`sidecar/` folder beside it, in the macOS bundle's `Contents/Resources`, and in
the working directory. If it is absent the Local Lite Node card says so and
nothing else changes. See
[the desktop node section](wallet-apps.md#desktop-running-a-node-for-you).

## Mobile (Android, iOS)

Source: `extras/mobile-wallet`.

```bash
# Android — needs the NDK
cmake -S ../.. -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DWRKZ_BUILD_WALLET_CAPI=ON
cmake --build build-android --target wallet_capi

# iOS
cmake -S ../.. -B build-ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DWRKZ_BUILD_WALLET_CAPI=ON
cmake --build build-ios --target wallet_capi
```

Then place the result:

- **Android** — `libwallet_capi.so` into `android/app/src/main/jniLibs/<abi>/`.
  Build once per ABI you ship.
- **iOS** — link `libwallet_capi.a` as a static framework, or embed the
  `.dylib`.

```bash
cd extras/mobile-wallet
flutter pub get
flutter run
```

!!! note "The daemon does not build for Android"
    The Android profile forces `WRKZ_BUILD_EXECUTABLES=OFF`, and RocksDB is only
    built when executables are on, so `Wrkzd` has never been compiled for
    Android in this tree. There is no node on the phone — see
    [Mobile: no node on the phone yet](wallet-apps.md#mobile-no-node-on-the-phone-yet).

## Web

Source: `extras/web-wallet`, with the WASM bridge in `extras/web-wallet-wasm`.
Requires Flutter 3.29+, Emscripten 3.1.50+ and CMake 3.16+.

```bash
# Emscripten, if not already active
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh

# From the repo root
mkdir build-wasm && cd build-wasm
emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DWRKZ_BUILD_WALLET_CAPI=ON \
  -DWRKZ_BUILD_WALLET_WASM=ON
  # -DWRKZ_WASM_PTHREADS=ON  for multi-threaded sync
cmake --build . -j$(nproc)
```

Copy the artefacts and the bridge JS into the Flutter web directory:

```bash
cp build-wasm/wasm/wallet_wasm.js        extras/web-wallet/web/
cp build-wasm/wasm/wallet_wasm.wasm      extras/web-wallet/web/
cp build-wasm/wasm/wallet_wasm.worker.js extras/web-wallet/web/   # pthreads only

cp extras/web-wallet-wasm/wasm/js/wallet_bridge.js  extras/web-wallet/web/
cp extras/web-wallet-wasm/wasm/js/wallet_storage.js extras/web-wallet/web/
```

```bash
cd extras/web-wallet
flutter pub get        # also generates l10n files
flutter build web      # -> build/web/
```

!!! danger "The pthreads build will not run without cross-origin isolation"
    With `WRKZ_WASM_PTHREADS=ON` the browser needs `SharedArrayBuffer`, which
    requires the server to send **both**:

    ```
    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp
    ```

    Without them the browser refuses to allocate shared memory and the module
    fails to initialise. `extras/web-wallet/README.md` has working nginx,
    Caddy, Docker and static-host configurations.

Browser floor: Chrome 89+, Firefox 89+, Safari 15.2+, Edge 89+.

## Building them all with Docker

`bash scripts/docker/build.sh web|desktop|mobile|apps` builds and packs the web
bundle, the Linux desktop bundle and the Android APK/AAB from one image, with
every toolchain preinstalled. Windows/macOS desktop and iOS cannot be built
there — they need MSVC and Xcode on their own hosts. See
[Building from Source](building.md#release-packages-with-docker).
