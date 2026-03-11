# PLUTON v2 — WRKZ Desktop Wallet

A Flutter-based desktop wallet for the WRKZ coin, using `wallet_capi` — a C shared library built directly from the WRKZ daemon source — for all wallet operations via Dart FFI. No subprocess sidecar required.

## Requirements

| Tool | Version |
|------|---------|
| Flutter | 3.38.7+ |
| Dart | 3.10.7+ |
| CMake | 3.14+ |
| Visual Studio 2022 | with "Desktop development with C++" (Windows) |
| GCC / Clang | 11+ (Linux / macOS) |

Enable desktop support once:

```bash
flutter config --enable-windows-desktop
flutter config --enable-linux-desktop
flutter config --enable-macos-desktop
```

---

## Quick Start (Development)

### 1. Build wallet_capi

From the repository root:

**Windows**
```bash
cmake -S . -B build -G "Visual Studio 17 2022" -DWRKZ_BUILD_WALLET_CAPI=ON -DBOOST_ROOT=C:/local/boost_1_86_0
cmake --build build --target wallet_capi --config Release
```
Output: `build\Release\wallet_capi.dll`

> **Boost:** Only headers are required — no compiled Boost libraries.
> Install the [Boost 1.86 Windows package](https://sourceforge.net/projects/boost/files/boost-binaries/) (header-only install is sufficient) and point `-DBOOST_ROOT` at the root folder.

**Linux**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWRKZ_BUILD_WALLET_CAPI=ON
cmake --build build --target wallet_capi
```
Output: `build/libwallet_capi.so`

**macOS**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWRKZ_BUILD_WALLET_CAPI=ON
cmake --build build --target wallet_capi
```
Output: `build/libwallet_capi.dylib`

### 2. Place the library next to the Flutter executable

| Platform | Library file | Destination |
|----------|-------------|-------------|
| Windows | `wallet_capi.dll` | `build\windows\x64\runner\Debug\` (debug) or `Release\` |
| Linux | `libwallet_capi.so` | `build/linux/x64/debug/bundle/` or `release/bundle/` |
| macOS | `libwallet_capi.dylib` | Next to `pluton_wallet.app` |

For `flutter run` (debug), copy the library into the runner output directory Flutter creates on first run, or use the `--dart-define` approach to set the library search path.

### 3. Run

```bash
cd extras/desktop-wallet
flutter pub get
flutter run -d windows   # or linux / macos
```

---

## Building Release Binaries

### Windows

```bash
flutter build windows --release
# Copy wallet_capi.dll into the release output:
copy build\Release\wallet_capi.dll build\windows\x64\runner\Release\
```

Output: `build/windows/x64/runner/Release/pluton_wallet.exe`

### Linux

```bash
flutter build linux --release
cp build/libwallet_capi.so build/linux/x64/release/bundle/
```

Output: `build/linux/x64/release/bundle/pluton_wallet`

### macOS

```bash
flutter build macos --release
cp build/libwallet_capi.dylib "build/macos/Build/Products/Release/"
```

Output: `build/macos/Build/Products/Release/pluton_wallet.app`

---

## Configuration

Default values are defined in [lib/core/config/app_config.dart](lib/core/config/app_config.dart):

| Constant | Default |
|----------|---------|
| `kDefaultDaemonHost` | `nodes.wrkz.work` |
| `kDefaultDaemonPort` | `17856` |
| `kCoinTicker` | `WRKZ` |
| `kCoinDecimalPlaces` | `2` |

All settings (node address, theme, log level) are persisted with `flutter_secure_storage` and can be changed from the **Settings** tab at runtime.

---

## Features

- **Overview** — balance (unlocked / locked), sync status, recent transactions
- **Receive** — primary address + QR code, integrated address generator with payment ID, copy-to-clipboard
- **Transfer** — prepare-then-send flow with fee preview; **Sweep All** mode to consolidate UTXOs
- **History** — filter (All / Received / Sent), full-text search, paginated (25/page), expandable details
- **Address Book** — saved addresses with labels and notes, CRUD dialogs
- **Settings** — daemon node switch, save/export/reset wallet, theme mode (System/Light/Dark), log level (0–5), delete wallet (two-step confirmation)
- **About** — links to GitHub, Discord, Twitter/X, website
- **Lock** — nav-rail lock button saves and closes the wallet, returning to the setup screen

---

## Cross-Platform Notes

| Feature | Windows | Linux | macOS |
|---------|---------|-------|-------|
| Secure storage backend | DPAPI | libsecret / keyring | Keychain |
| File picker | ✓ | ✓ | ✓ |
| Window manager (min size / title) | ✓ | ✓ | ✓ |
| URL launcher | ✓ | ✓ | ✓ |

### Linux: system dependencies

```bash
sudo apt-get install libsecret-1-dev libjsoncpp-dev
```

### macOS: entitlements

`macos/Runner/DebugProfile.entitlements` and `Release.entitlements` must include:

```xml
<key>com.apple.security.network.client</key>
<true/>
```

(Already included in the project defaults.)

---

## Project Structure

```
lib/
├── app/               # App entry, router, MaterialApp
├── core/
│   ├── api/
│   │   └── models/    # Shared data models (Balance, Transaction, WalletStatus)
│   ├── auth/          # Wallet password secure storage
│   ├── config/        # AppConfig constants
│   ├── ffi/           # Dart FFI binding for wallet_capi (wallet_ffi.dart)
│   └── providers/     # Riverpod providers + polling notifiers
├── features/
│   ├── about/
│   ├── addressbook/
│   ├── history/
│   ├── lock/
│   ├── overview/
│   ├── receive/
│   ├── settings/
│   ├── setup/
│   ├── shell/         # Navigation rail shell
│   └── transfer/
├── shared/
│   ├── theme/         # Dark + light ThemeData
│   ├── utils/         # Amount formatter
│   └── widgets/       # Shared widgets (logo, copy button, sync banner)
└── main.dart
```
