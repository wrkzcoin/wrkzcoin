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

1. Qt 6.5+ (`Quick`, `Qml`, `QuickControls2`, `Concurrent`)
2. Built wallet C API library:
   - Linux shared: `libwallet_capi.so`
   - Linux static: `libwallet_capi_c.a`
   - Windows import/static library
3. Wallet C API headers at repo `include/walletcapi/wallet_capi.h`

## Build (Linux example)

```bash
# From repo root, first build wallet_capi if needed
WALLET_LIB_KIND=both bash scripts/build-linux-wallet-lib.sh

# Configure desktop-wallet
cmake -S extras/desktop-wallet -B build-desktop-wallet \
  -DWALLET_CAPI_INCLUDE_DIR="$PWD/include" \
  -DWALLET_CAPI_LIBRARY="$PWD/build-linux-wallet-capi/src/libwallet_capi.so"

# Build
cmake --build build-desktop-wallet -j
```

Run:

```bash
./build-desktop-wallet/gui_wallet
```

If runtime linker cannot find `libwallet_capi.so`, set:

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

## Notes

- Current app uses direct C API calls on UI thread for simplicity.
- For production UX, move long operations to worker threads and keep a serialized backend command queue.
- `wallet_capi` remains generic and reusable by other frontends.
