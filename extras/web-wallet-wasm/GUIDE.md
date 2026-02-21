# Web Wallet Guide

## Architecture

- Wallet core runs in browser worker via WASM.
- UI talks to worker over message commands.
- Worker calls `wallet_wasm_request` exported from WASM.
- Remote node connectivity uses ordered endpoints with failover.

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

## Remote Node Failover

1. Keep ordered endpoints: primary, secondary, tertiary.
2. Probe on startup and periodically.
3. Route traffic to best healthy endpoint.
4. Swap endpoint on timeout/error threshold.
5. Periodically compare chain height with alternate endpoint.
