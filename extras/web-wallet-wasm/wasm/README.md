# WASM Bridge

This folder contains the WebAssembly bridge source used by the Emscripten build.

## Current scope

- JSON request/response dispatch function: `wallet_wasm_request`
- In-memory wallet handle registry (integer handle IDs)
- Initial command routes:
  - `apiVersion`
  - `version`
  - `open`
  - `create`
  - `close`
  - `status`
  - `balance`
  - `swapNode`
  - `daemonOnline`

## Notes

- Build integration is done through root CMake target `wallet_wasm`.
- `wallet_wasm_exports.cpp` links against `wallet_capi_c` to reuse existing walletbackend logic.
- Worker bindings in `web/workers/wallet.worker.ts` call `wallet_wasm_request`.
