# Running Wallet API

`wrkz-wallet-api` serves the HTTP wallet API described in
[Endpoints](../wallet-api/endpoints.md). It holds one wallet open at a time and
talks to a daemon over RPC.

## Required

`--rpc-password` must be set (`src/walletapi/ParseArguments.cpp`). There is no
default and no way to disable authentication.

## Options

| Option | Default | Meaning |
| --- | --- | --- |
| `--rpc-password <pass>` | none, **required** | Value clients must send as `X-API-KEY` |
| `--rpc-bind-ip <ip>` | `127.0.0.1` | IPv4 interface to listen on |
| `-p, --port <port>` | `7856` | TCP port |
| `--rpc-bind-ipv6-address <addr>` | empty | IPv6 address for a second listener |
| `--rpc-use-ipv6` | `false` | Start the IPv6 listener |
| `--enable-cors <origin>` | empty | `Access-Control-Allow-Origin` value |
| `--threads <n>` | hardware concurrency | Wallet sync worker threads |
| `--scan-coinbase-transactions` | `false` | Scan coinbase transactions during sync |
| `--log-level <0-5>` | | Log verbosity |
| `--log-file <path>` | | Log file |
| `--no-console` | `false` | Do not start the interactive console |
| `--rpc-ipc-path <path>` | empty | Also serve the API on this local socket. POSIX only |
| `--rpc-ipc-mode <octal>` | `0600` | Permissions for the socket file |
| `--rpc-ipc-group <group>` | empty | Group to own the socket file |
| `--tx-notify <cmd\|url>` | empty | Fire on each transaction confirmed for the open wallet |
| `--notify-during-sync` | `false` | Also fire during a rescan |

!!! note "Default port collides with `wrkz-service`"
    `wrkz-wallet-api` and `wrkz-service` both default to `7856`. Give one of
    them a different port if you run both on one machine.

## Request authentication

Send `X-API-KEY: <rpc-password>` on every request. The server compares a
PBKDF2-HMAC-SHA256 hash of the supplied value. This applies on the IPC socket
too — unlike the daemon, wallet-api never drops its password, because these
endpoints move money. See [Auth and Security](../wallet-api/auth-and-security.md).

## Example

```bash
./wrkz-wallet-api \
  --rpc-bind-ip 127.0.0.1 \
  --port 7856 \
  --rpc-password "strong-password" \
  --threads 4
```

Prefer binding to `127.0.0.1` unless you are behind a trusted reverse proxy, and
scope `--enable-cors` to a real origin rather than `*`.

## Smoke tests

```bash
curl -s -H "X-API-KEY: strong-password" http://127.0.0.1:7856/status

curl -s -X POST http://127.0.0.1:7856/wallet/open \
  -H "X-API-KEY: strong-password" \
  -H "Content-Type: application/json" \
  -d '{"filename":"wallet.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

`daemonHost` also accepts an IPv6 literal and, on POSIX, a local IPC socket path
or an `ipc://` URL — see [Local IPC and Console](ipc-and-console.md).

## Against a lite node

The wallet backend reads `lite_start_height` from the daemon's `/info`. A wallet
that has already scanned *below* that height **stops syncing** rather than
skipping blocks it can never see. See
[Lite Nodes → Wallet behaviour](lite-node.md#wallet-behaviour).

## Notes

- Route behaviour and wallet state checks live in
  `src/walletapi/ApiDispatcher.cpp`.
- Fusion / optimize endpoints were removed in 0.4.7; use the sweep endpoints
  instead. See the [changelog](../changelog/wallet-api.md).
- `wrkz-wallet-api` always computes transaction proof of work on its own CPU.
  The [Tx PoW server](txpow-server.md) setting exists only in the desktop,
  mobile and web wallets.
