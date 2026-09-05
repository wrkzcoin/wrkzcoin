# Wallet API Changelog

Changes to `wrkz-wallet-api` and the wallet backend it shares with
`wrkz-wallet`, `wrkz-service` and the desktop, mobile and web wallets. Daemon
and RPC changes are in the [Daemon changelog](daemon.md).

Version numbers follow the daemon's — all binaries are built from one tree and
report the same `<major>.<minor>.<revision>.<build>` string.

---

## 0.4.8 (build 280) — 2026-09-05

### Endpoints

No routes were added or removed in this release. The route table in
[Endpoints](../wallet-api/endpoints.md) is current as of build 280.

### New options

- `--rpc-ipc-path`, `--rpc-ipc-mode`, `--rpc-ipc-group` — serve the API over a
  local AF_UNIX socket alongside the TCP listener. POSIX only. Unlike the
  daemon's IPC socket, wallet-api still requires `--rpc-password` on IPC: these
  endpoints move money. See [Local IPC and Console](../guides/ipc-and-console.md).
- `--tx-notify`, `--notify-during-sync` — fire a command or webhook for each
  transaction confirmed in a block for the open wallet. wallet-api does not scan
  the pool for incoming transfers, so unlike `wrkz-service` it has no
  unconfirmed event. See [Notification Hooks](../guides/notify-hooks.md).

### Behaviour

- **Ring size fallback.** With the network minimum rising to 8 at height
  4,300,000, a send whose denomination lacks decoys retries at the best ring
  size actually achievable rather than failing outright, and reports the ring
  size it used alongside the network's. `wrkz-service` does the same.
- **Encrypted payment IDs.** Short payment IDs are encrypted to the receiver.
  Integrated addresses and the `paymentID` field are unchanged for callers; the
  encoding on the wire moved to extra sub-tag `0x03`. See
  [Encrypted Payment IDs](../guides/encrypted-payment-ids.md).
- **Lite node support.** The backend reads `lite_start_height` from `/info`. A
  fresh wallet's scan height is floored to it with a warning; a wallet already
  scanned *below* it stops syncing rather than silently skipping blocks it can
  never see. Applies to every binary built on the wallet backend.
- **Faster sync against a 0.4.8 daemon.** The backend negotiates the daemon's
  `sync_features` and uses empty-block skipping, base64 encoding and parallel
  height windows where offered. Older daemons are unaffected.
- **Optional transaction PoW server.** The backend can ask a
  `wrkz-txpow-server` for a transaction's proof of work and falls back to the
  local CPU if the server is unreachable, refuses, times out or returns a nonce
  that does not verify. `wrkz-wallet-api`, `wrkz-wallet` and `wrkz-service`
  always compute locally; the setting is exposed only in the desktop, mobile and
  web wallets. See [Transaction PoW Server](../guides/txpow-server.md).
- A `ipc://`-prefixed daemon address keeps its prefix when parsed, so
  `daemonHost` can name a local socket.

### Fixes

- A scan height is never derived by taking `min()` against a zero.
- A stalled sync reported the wrong heights and never cleared.
- One block at the chain tip could halt sync for good.
- Deliberate gaps in a sync response — the ones the wallet itself asked for with
  `skipEmptyBlocks` — no longer halt sync.
- Wallets are saved through a temporary file, so an interrupted save cannot
  truncate the container.

---

## 0.4.7 (build 270) — 2026-08-21

### Breaking

- **Fusion / optimize transactions were removed** from the wallet backend,
  wallet API and C API. `POST /transactions/send/fusion/basic` and
  `POST /transactions/send/fusion/advanced` no longer exist and return `404`.
  The `optimize` command was removed from `wrkz-wallet` at the same time. Use
  the sweep endpoints below to consolidate a wallet's inputs.

### Added

- **`POST /transactions/send/sweep`** — send an amount to one destination across
  as many transactions as it takes. Body: `destination`, optional `amount`
  (omitted or `0` sweeps the whole balance), optional `paymentID`.
- **`POST /transactions/send/sweep/all`** — sweep the entire balance to one
  destination. Body: `destination`, optional `paymentID`.

  Both answer `200` with a `transactions` array holding one entry per
  transaction attempted, each either `{"success": true, "transactionHash": …}`
  or `{"success": false, "errorCode": …, "errorMessage": …}`. A partial failure
  is reported per entry, not as an HTTP error.

- `sweep` and `sweep_all` commands in `wrkz-wallet`, which show the estimated
  transaction count and total fee before confirming.

### Fixes

- **Deadlock on wallet open, create, import and close** caused by redundant
  mutex locks in the dispatcher.
- The shared-mutex write-operation classification was tightened for the `/node`,
  `/sync/refresh` and `/export/json` routes, which are the ones that must not
  run concurrently with reads.
- Sweeping a specific amount returned change to self instead of sending every
  input to the recipient.
- The sweep fee estimate now iterates output decomposition and accounts for the
  PoW nonce overhead.
- `sweepToAddress` accepts integrated addresses.
- Sync threads default to `hardware_concurrency`, with larger block batches and
  buffers.

---

## 0.4.6 (build 265) — 2026-03-08 and earlier

See the [release list on GitHub](https://github.com/wrkzcoin/wrkzcoin/releases)
and the [commit history](https://github.com/wrkzcoin/wrkzcoin/commits/development).
