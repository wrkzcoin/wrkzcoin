# PLUTON Mobile

WrkzCoin (WRKZ) mobile wallet for Android and iOS, powered by the `wallet_capi` native library via Dart FFI.

## Wallet Storage

All wallet files are stored in the app's internal documents directory:

```
<app_docs>/wallets/
├── wallets.json              # Registry (captions, filenames, metadata)
├── main_wallet.wallet        # Wallet data
├── main_wallet.wallet.keys   # Wallet keys
└── ...
```

Users identify wallets by **caption** (e.g. "Main Wallet", "Savings").
Captions are sanitized to safe filenames internally.
Multiple wallets are supported — switch between them from Settings or the Wallet Picker screen.

## Native Library

The app requires the `wallet_capi` shared library compiled for the target platform:

- **Android**: `libwallet_capi.so` (place in `android/app/src/main/jniLibs/<abi>/`)
- **iOS**: Link `libwallet_capi.a` as a static framework or embed `.dylib`

### Building wallet_capi

```bash
# Android (requires NDK)
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

## Running

```bash
cd extras/mobile-wallet
flutter pub get
flutter run          # Connected Android/iOS device or emulator
```

## Features

- Create, import (seed/keys), and manage multiple wallets
- Real-time sync with daemon node (configurable, with presets)
- Send transactions with QR code scanning and review step
- Receive with QR code display and share
- Integrated address generation (short/long payment IDs)
- Transaction history with search and direction filters
- Biometric unlock (fingerprint / Face ID)
- Auto-lock when app goes to background
- Incoming transaction notifications
- Light / Dark / System theme
- Pull-to-refresh on all data screens
- Haptic feedback on key actions
- Offline/connection status banner
- Lite-node awareness (see below)

## Lite nodes

The app reads `daemonLiteStartHeight` and `isSyncStalledByLiteNode` from
`wallet_get_status_json` and reacts to both:

- **Overview** carries a standing notice naming the lowest block the connected
  node holds. It stays up once the wallet is synced, which is exactly when a
  balance missing its older half looks most trustworthy.
- **Settings → Daemon Node** shows *Serves blocks from* for whatever node is
  connected — a height for a lite node, *Full chain* otherwise.
- **Rescan** is floored at the node's start height. Asking for less is refused
  in the app, and refused again by the wallet backend (error 62), because a
  lite node answers a lower request from its own start anyway — which would
  move the recorded scan position over blocks nobody looked at.

If the node starts above the wallet's own start height the notice turns into a
warning: funds received in between cannot be found through that node and the
balance reads low. Connect a node holding the whole chain to see them.

### Running the node on the phone — not yet

There is no embedded node on mobile, and it is a longer way off than the
desktop version was:

- The Android build profile forces `WRKZ_BUILD_EXECUTABLES=OFF`, and RocksDB is
  only built when executables are on, so the daemon has never been compiled for
  Android in this tree.
- Size does not follow the start height. Key output info is written for every
  block from genesis, so a lite node is ~6 GB whatever height it starts at, and
  it downloads the whole chain (a few GB) to build that index.
- A first sync is hours of continuous work, against Android's doze and
  background execution limits.

Until then, point the wallet at a node you run yourself — a desktop machine on
the same network works well, and the desktop app can host one for you.

## Coin Configuration

| Parameter | Value |
|-----------|-------|
| Ticker | WRKZ |
| Decimal places | 2 |
| Address prefix | `Wrkz` |
| Standard address length | 98 |
| Integrated address lengths | 120, 186 |
| Default daemon | `nodes.wrkz.work:17856` |
