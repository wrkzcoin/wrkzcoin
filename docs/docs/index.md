# zcoin-dev Wiki

This docs tree is intended for publishing as project documentation/wiki later.

Scope in this first draft:

- Daemon RPC (`src/rpc/RpcServer.cpp`)
- Wallet API RPC (`src/walletapi/ApiDispatcher.cpp`)
- Wallet Service JSON-RPC (`src/walletservice/PaymentServiceJsonRpcServer.cpp`)
- Wallet CLI (`src/zedwallet++`)
- Basic operator guides

Source of truth is always the current code. Keep these pages updated when route registration or method handlers change.

## Build locally

From repository root:

```bash
mkdocs serve -f docs/mkdocs.yml
```

Build static site:

```bash
mkdocs build -f docs/mkdocs.yml
```
