# Daemon RPC Overview

Implementation: `src/rpc/RpcServer.cpp`, `src/rpc/RpcServer.h`

The daemon exposes:

- JSON-RPC at `/json_rpc` (GET and POST)
- HTTP-style endpoints (`/info`, `/height`, and others)

RPC availability depends on daemon RPC mode:

- `Default`
- `MiningEnabled`
- `BlockExplorerEnabled`
- `AllMethodsEnabled`

Permissions are enforced in middleware before handlers run.

## Main runtime options

Defined in `src/daemon/DaemonConfiguration.h` and parsed in `src/daemon/DaemonConfiguration.cpp`:

- `rpc-bind-ip`
- `rpc-bind-port`
- `rpc-access-token`
- `rpc-read-timeout`
- `rpc-write-timeout`
- `rpc-max-body-bytes`
- `rpc-max-rpm`
- `rpc-max-global-index-range`
- `rpc-max-block-count`
- `rpc-trust-proxy`

See the other daemon RPC pages for auth, limits, and method lists.

## Quick Examples

Set shared environment variables:

```bash
export DAEMON_RPC_URL="http://127.0.0.1:11898"
export DAEMON_RPC_TOKEN="replace-me"
```

Get daemon info:

```bash
curl -s "$DAEMON_RPC_URL/info"
```

Get peers with token auth:

```bash
curl -s \
  -H "X-API-Key: $DAEMON_RPC_TOKEN" \
  "$DAEMON_RPC_URL/peers"
```

Call JSON-RPC (`getblockcount`):

```bash
curl -s \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```
