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
cmake -S . -B build -G "Visual Studio 17 2022" -DWRKZ_BUILD_WALLET_CAPI=ON
cmake --build build --target wallet_capi --config Release
```
Output: `build\src\Release\wallet_capi.dll`


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
- **Local Lite Node** — run `Wrkzd` on this machine and sync against it (see below)

---

## Window, tray and quitting

`AppLifecycle` owns the tray icon, the window close interception and the
shutdown. It sits above the router in `main.dart` on purpose: these used to
live in `MainShell`, which a `ShellRoute` builds, so locking the wallet — a
route outside that shell — disposed the state while the tray icon stayed on
screen with an Exit item bound to it. Riverpod throws on `ref` after dispose,
so tray Exit silently did nothing until the wallet was unlocked again.

| | |
|---|---|
| Close / minimise | Hides to the tray, but only if the icon actually installed. A shell that refuses it leaves close quitting normally, so the window can never vanish with nothing to bring it back |
| First hide | A desktop notification says the app is still in the tray. On Windows a new icon goes into the overflow flyout, and a window that disappears with no icon in sight reads as a crash |
| Exit | Closes the wallet, which is what saves it, then quits. A second Exit joins the first rather than starting a competing close |
| A save that stalls | An overlay covers the window while the wallet is written out, and after 20 seconds offers to quit anyway — with the warning that it can damage the wallet file, because the file is rewritten in place rather than through a temporary |

---

## Local lite node

Settings has a **Local Lite Node** card that supervises a `Wrkzd` child process
on this machine, so the wallet can sync against a node the user owns rather
than a public server.

### Shipping the daemon

The node is a separate binary, looked for in this order:

1. `$WRKZ_DAEMON_PATH`
2. next to the wallet executable
3. a `sidecar/` folder beside it
4. `Contents/Resources/` and `Contents/Resources/sidecar/` (macOS bundle)
5. the working directory, and `sidecar/` inside it (`flutter run`)

Build it with the normal daemon target and drop it next to the app:

```bash
cmake --build build --target Wrkzd --config Release
copy build\src\Release\Wrkzd.exe build\windows\x64\runner\Release\
```

It adds roughly 9 MB to a release. If it is absent the card says so and nothing
else in the app changes.

`WRKZ_DAEMON_EXTRA_ARGS` appends flags to the daemon's command line, space
separated. The app never sets it; it exists for daemons built without something
the standard argument list assumes. The usual case is a RocksDB linked without
ZSTD, which exits the moment it creates a database unless it is given
`--db-enable-compression=false` — and note that running a lite node uncompressed
costs roughly 28 GB instead of 6 GB, so that is a debugging flag, not a setting.

### Testing it

`test/local_node_e2e_test.dart` drives the supervisor against a real daemon on
the real network: start, `/info`, the `lite_start_height` round trip, peers,
stop, port release, delete, and adoption of a running node by a second app
instance. It is skipped unless `WRKZ_LOCAL_NODE_E2E=1` is set, so an ordinary
`flutter test` never touches the network.

```bash
WRKZ_LOCAL_NODE_E2E=1 WRKZ_DAEMON_PATH=/path/to/Wrkzd \
  flutter test test/local_node_e2e_test.dart
```

### What the app does with it

| | |
|---|---|
| Data directory | `<app support>/node`, with its own `pluton-node.json`, pid file and `wrkzd.log` |
| Ports | An ephemeral RPC and P2P port picked once and stored, so the node never collides with a daemon the user runs themselves |
| RPC binding | `127.0.0.1` only |
| Console | `--no-console`; the process is stopped by signal |
| Restarts | A node that was running comes back up on the next launch. One stopped on purpose stays stopped |
| Quitting the app | **The node keeps running.** Nothing stops or signals it — a first sync takes hours and is not worth throwing away every time the window is closed. Stop it from Settings if you want it gone |
| Crash recovery | A node left behind by a force-quit app is found by its port and adopted rather than started twice on the same database |

**The RPC is unauthenticated.** `--rpc-access-token` cannot be used: the wallet
backend builds its own request headers and has no way to send one. On POSIX the
daemon can also expose an AF_UNIX socket with 0600 permissions, which would be
tighter, but Windows compiles that out — loopback is what both ends can always
agree on. Anything else running as any user on the machine can reach this node.

**Stopping is a signal, not a clean shutdown, on Windows.** `dart:io` maps
`SIGINT` onto `TerminateProcess` there. That is safe: writes already in
RocksDB's WAL are in the OS's hands and get replayed on the next open. It costs
recovery time at startup, not data.

### The start height is the whole decision

The setup wizard defaults to the wallet's own start height, read from
`walletSyncStartHeight`. Choosing higher is allowed but needs an explicit
acknowledgement, because a node that starts above the wallet can never show
that wallet's older transactions — the balance simply reads low. The height
cannot be changed afterwards: the daemon refuses to start against a database
built at a different one, so changing it means deleting the node and syncing
again.

### Switching between nodes

The node's life is independent of the wallet's connection. It keeps syncing
whether or not the wallet points at it, so the usual flow is to stay on a
remote node for the hours the first sync takes and switch over when it is
ready. **Use this node** is disabled until the node reports itself synced —
switching earlier would leave the wallet parked with nothing on screen to
explain the stall. Switching back is the **Remote node** chip and Apply, and
stopping or deleting the node moves the wallet back automatically rather than
leaving it addressing a dead port.

See `LITENODE.md` for what a lite node does and does not hold.

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
