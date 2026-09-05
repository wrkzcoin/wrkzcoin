# Wallet Service JSON-RPC Overview

The legacy payment-service RPC served by `wrkz-service`: its envelope, its
options, and how it differs from the wallet API.

Implementation: `src/walletservice/PaymentServiceJsonRpcServer.cpp`

This is a JSON-RPC server used by the legacy wallet service binary (`wrkz-service`), separate from the HTTP wallet-api routes.

Request format is JSON-RPC-like with:

- `method`
- optional `params`
- optional `password` when legacy security is disabled

Password checking behavior is in `processJsonRpcRequest`.

Use this section when maintaining integrations built on payment-service RPC method names.

## Notable options

| Option | Default | Meaning |
| --- | --- | --- |
| `--rpc-password` | none | Value clients must send as the request's `password` field |
| `--rpc-legacy-security` | `false` | Drop the password check. Do not use |
| `--bind-address` | `127.0.0.1` | Interface to listen on. Accepts an IPv6 address |
| `--bind-port` | `7856` | TCP port. **Shares its default with `wrkz-wallet-api`** — change one if you run both |
| `--daemon-address` | `127.0.0.1` | Daemon host, an IPv6 literal, or a local IPC socket path |
| `--bind-ipc-path` | empty | Serve JSON-RPC on this local socket **instead of** the TCP port. POSIX only |
| `--bind-ipc-mode` | `0600` | Permissions for the socket file |
| `--bind-ipc-group` | empty | Group to own the socket file |
| `--tx-notify` | empty | Fire on every new wallet transaction, incoming or outgoing |
| `--tx-confirmed-notify` | empty | Fire once per transaction, when it gets into a block |
| `--notify-during-sync` | `false` | Also fire during a rescan |

Unlike the daemon, `wrkz-service` still requires its password on the IPC
socket — these endpoints move money. See
[Local IPC and Console](../guides/ipc-and-console.md) and
[Notification Hooks](../guides/notify-hooks.md).

## Endpoint and Envelope

HTTP endpoint:

- `POST /json_rpc`

Example envelope:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getStatus",
  "password": "rpc-password",
  "params": {}
}
```

Curl template:

```bash
export WALLET_SERVICE_URL="http://127.0.0.1:7856/json_rpc"
export WALLET_SERVICE_PASSWORD="replace-me"

curl -s -H "Content-Type: application/json" \
  -d "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"getStatus\",\"password\":\"$WALLET_SERVICE_PASSWORD\",\"params\":{}}" \
  "$WALLET_SERVICE_URL"
```
