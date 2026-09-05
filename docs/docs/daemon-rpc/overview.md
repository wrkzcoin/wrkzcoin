# Daemon RPC Overview

What the daemon serves, on which listeners, and which options control them.

Implementation: `src/rpc/RpcServer.cpp`, `src/rpc/RpcServer.h`

The daemon exposes:

- JSON-RPC at `/json_rpc` (GET and POST)
- HTTP-style endpoints (`/info`, `/height`, and others)
- The same surface over a local AF_UNIX socket when `--rpc-ipc-path` is given,
  plus `/console`, which is served **only** there. See
  [Local IPC and Console](../guides/ipc-and-console.md)
- A stratum server for miners, off unless `--stratum-bind-port` is given. It is
  a separate TCP listener, not part of the RPC surface. See
  [Solo Mining](../guides/solo-mining.md)

RPC availability depends on the daemon RPC mode, chosen with
`--daemon-mode <standard|explorer>` (`RpcMode` in `src/rpc/RpcServer.h`):

- `Standard` — the default. Everything except the block explorer methods.
- `Explorer` — additionally serves the explorer routes, which walk database
  indexes and are not something a plain node should answer for anyone who asks.

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
- `rpc-sync-cache-size`
- `rpc-trust-proxy`
- `rpc-ipc-path`
- `daemon-mode`

The full option list, with every default, is in the
[Configuration Reference](../guides/daemon-configuration.md). See the other
daemon RPC pages for auth, limits, and method lists.

## On a lite node

A [lite node](../guides/lite-node.md) answers `/info` with `lite` and
`lite_start_height`, clamps `/getwalletsyncdata` and `/getrawblocks` requests up
to that height, and **refuses** `/get_global_indexes_for_range` below it. Lite
mode and explorer mode cannot be combined.

## Quick Examples

Set shared environment variables:

```bash
export DAEMON_RPC_URL="http://127.0.0.1:17856"
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
