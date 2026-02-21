# Web Wallet Guide

## Architecture

- Wallet core runs in browser worker via WASM.
- UI talks to worker over message commands.
- Worker calls `wallet_wasm_request` exported from WASM.
- Remote node connectivity uses ordered endpoints with manual switching from Settings.

## Non-Custodial Rules

- Seed phrase, private spend key, private view key, and wallet password stay in browser memory/WASM.
- Secrets at rest are encrypted in IndexedDB (PBKDF2 + AES-GCM).
- Only daemon RPC calls needed for sync and broadcast go over network.

## Build WASM

Prerequisites:

- Emscripten SDK (`emcmake` available)
- CMake toolchain

From repo root:

- Linux/macOS: `bash extras/web-wallet-wasm/scripts/build-wasm.sh`
- Windows PowerShell: `powershell -ExecutionPolicy Bypass -File extras/web-wallet-wasm/scripts/build-wasm.ps1`

Expected artifacts:

- `extras/web-wallet-wasm/web/wasm/generated/wallet_wasm.js`
- `extras/web-wallet-wasm/web/wasm/generated/wallet_wasm.wasm`
- `extras/web-wallet-wasm/web/wasm/generated/wallet_wasm.worker.js` (when pthread build is enabled)

## Multithread (pthreads) Runtime Requirements

The WASM build is configured with pthreads. Browser runtime requires cross-origin isolation:

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`
- `Cross-Origin-Resource-Policy: same-origin` (recommended for local assets)

If these headers are missing, build may succeed but multithread execution will fail at runtime.

Example nginx headers (wallet web domain):

```nginx
add_header Cross-Origin-Opener-Policy "same-origin" always;
add_header Cross-Origin-Embedder-Policy "require-corp" always;
add_header Cross-Origin-Resource-Policy "same-origin" always;

types {
    application/wasm wasm;
}
```

## Run Web UI

From `extras/web-wallet-wasm`:

- `yarn install`
- `yarn dev:web`

## Vault Smoke Test

1. Open web UI.
2. Enter vault password and click `Init/Unlock Vault`.
3. Enter demo secret and click `Store Demo Secret`.
4. Click `Load Demo Secret` to confirm decrypt.
5. Click `Lock Vault`, then verify load returns `vault_locked`.

## Remote Node Management

1. Keep ordered endpoints: primary, secondary, tertiary.
2. Set one endpoint as default for create/import actions.
3. During an active wallet session, use `Use For Wallet` to call `swapNode`.
4. Add/remove custom endpoints as needed.
