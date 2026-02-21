# Desktop Wallet (Qt/QML + wallet_capi)

This is a desktop wallet app scaffold built on top of the generic C wallet API.

Path:

```text
extras/desktop-wallet
```

## Features

- Open/create wallet
- Restore from mnemonic seed
- Save/close/delete wallet file
- Sync status and balances
- Primary address and node info
- Send basic transaction
- Create integrated address
- Swap node
- Reset wallet sync
- Transactions JSON view

## Prerequisites

1. Qt 6.2+ (`Gui`, `Qml`, `Concurrent` for build; `QtQuick`/`QtQuick.Controls` as runtime QML modules)
2. Built wallet C API library:
   - Linux shared: `libwallet_capi.so`
   - Linux static: `libwallet_capi_c.a`
   - Windows import/static library
3. Wallet C API headers at repo `include/walletcapi/wallet_capi.h`

## Build (Linux example)

```bash
# Ubuntu/Debian Qt packages (if missing)
sudo apt-get update
sudo apt-get install -y \
  qt6-base-dev \
  qt6-declarative-dev \
  qt6-tools-dev-tools \
  qml6-module-qtquick \
  qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts \
  qml6-module-qtqml

# From repo root, first build wallet_capi if needed
WALLET_LIB_KIND=both bash scripts/build-linux-wallet-lib.sh

# Configure desktop-wallet
cmake -S extras/desktop-wallet -B build-desktop-wallet \
  -DDESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI=ON \
  -DWALLET_CAPI_INCLUDE_DIR="$PWD/include" \
  -DWALLET_CAPI_LIBRARY="$PWD/build-linux-wallet-capi/src/libwallet_capi_c.a"

# Build
cmake --build build-desktop-wallet -j
```

Static-link note:

- If `WALLET_CAPI_LIBRARY` points to `libwallet_capi_c.a`, this project will
  automatically pull additional static archives from the same wallet build tree
  (for example `build-linux-wallet-capi/src` and `build-linux-wallet-capi/external`)
  so unresolved backend symbols are linked.
- `DESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI` defaults to `ON`. Set it `OFF`
  only if you intentionally want to link against a shared `libwallet_capi.so`.

If CMake still cannot find Qt6, pass `Qt6_DIR` (or `CMAKE_PREFIX_PATH`) explicitly:

```bash
cmake -S extras/desktop-wallet -B build-desktop-wallet \
  -DQt6_DIR=/path/to/Qt/6.x.x/gcc_64/lib/cmake/Qt6 \
  -DDESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI=ON \
  -DWALLET_CAPI_INCLUDE_DIR="$PWD/include" \
  -DWALLET_CAPI_LIBRARY="$PWD/build-linux-wallet-capi/src/libwallet_capi_c.a"
```

Run:

```bash
./build-desktop-wallet/gui_wallet
```

If you choose shared wallet library instead (`WALLET_LIB_KIND=both`), and runtime linker cannot find `libwallet_capi.so`, set:

```bash
export LD_LIBRARY_PATH="$PWD/build-linux-wallet-capi/src:$LD_LIBRARY_PATH"
```

## Build (Windows cross artifact usage)

Configure with your wallet_capi import/static library path:

```bash
cmake -S extras/desktop-wallet -B build-desktop-wallet-win \
  -DWALLET_CAPI_INCLUDE_DIR="%CD%/include" \
  -DWALLET_CAPI_LIBRARY="C:/path/to/wallet_capi.lib"
```

## Build (Windows native, Visual Studio 2022)

Prerequisites:

1. Visual Studio 2022 with C++ workload
2. Qt 6 built for MSVC (for example `msvc2019_64` / `msvc2022_64`)
3. CMake available in terminal

From repo root in `x64 Native Tools Command Prompt for VS 2022` (or PowerShell):

```powershell
# 1) Build static wallet backend library
cmake -S . -B build-windows-wallet-msvc -G "Visual Studio 17 2022" -A x64 `
  -DWRKZ_BUILD_EXECUTABLES=OFF `
  -DWRKZ_BUILD_WALLET_CAPI=ON `
  -DENABLE_ZMQ=OFF

cmake --build build-windows-wallet-msvc --config Release --target wallet_capi_c
```

Expected wallet static library:

```text
build-windows-wallet-msvc/src/Release/wallet_capi_c.lib
```

Then configure and build desktop wallet:

```powershell
cmake -S extras/desktop-wallet -B build-desktop-wallet-win -G "Visual Studio 17 2022" -A x64 `
  -DQt6_DIR="C:/Qt/6.6.3/msvc2019_64/lib/cmake/Qt6" `
  -DDESKTOP_WALLET_REQUIRE_STATIC_WALLET_CAPI=ON `
  -DWALLET_CAPI_INCLUDE_DIR="$PWD/include" `
  -DWALLET_CAPI_LIBRARY="$PWD/build-windows-wallet-msvc/src/Release/wallet_capi_c.lib"

cmake --build build-desktop-wallet-win --config Release --target gui_wallet
```

Run:

```powershell
.\build-desktop-wallet-win\Release\gui_wallet.exe
```

If Qt DLLs are missing at runtime, add Qt bin to `PATH`:

```powershell
$env:Path = "C:\Qt\6.6.3\msvc2019_64\bin;$env:Path"
.\build-desktop-wallet-win\Release\gui_wallet.exe
```

## Notes

- Current app uses direct C API calls on UI thread for simplicity.
- For production UX, move long operations to worker threads and keep a serialized backend command queue.
- `wallet_capi` remains generic and reusable by other frontends.
