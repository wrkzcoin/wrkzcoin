# Web Wallet WASM

This workspace contains a browser-first wallet stack:

- `wasm/`: WebAssembly bridge for selected `wallet_capi` operations
- `web/`: React + Vite + TypeScript UI and wallet worker client
- `scripts/`: Build and dev helper scripts
- `GUIDE.md`: Build, run, and security guide

## Goals

- Keep private keys in-browser
- Support remote node(s) only
- Include multi-node management from day one

## Non-Custodial Rules

- Seed phrase, private spend key, private view key, and wallet password are handled only inside browser memory/WASM.
- No secret material is sent to remote nodes.
- Network requests are limited to daemon RPC flows, not wallet secret-management flows.

## WASM Build

- Build and copy artifacts with:
  - `bash extras/web-wallet-wasm/scripts/build-wasm.sh`
  - `powershell -ExecutionPolicy Bypass -File extras/web-wallet-wasm/scripts/build-wasm.ps1`

See `extras/web-wallet-wasm/GUIDE.md` for full steps.
