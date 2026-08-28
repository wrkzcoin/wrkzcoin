# Networking: IPv4 & IPv6 Binding

## P2P Network

### IPv4 (always on)

| Flag | Default | Description |
|------|---------|-------------|
| `--p2p-bind-ip` | `0.0.0.0` | IPv4 interface for the P2P listener |
| `--p2p-bind-port` | `17855` | TCP port for the P2P listener |
| `--p2p-external-port` | `0` | External port (NAT/UPnP) |

The daemon always binds an IPv4 P2P listener. All existing peer-to-peer communication uses IPv4.

### IPv6 (opt-in)

| Flag | Default | Description |
|------|---------|-------------|
| `--p2p-bind-ipv6-address` | *(empty, disabled)* | IPv6 bind address. Examples: `::` (all interfaces), `::1` (loopback). Empty = IPv6 disabled. |
| `--p2p-bind-port-ipv6` | `0` | Port for the IPv6 P2P listener. `0` means use the same port as `--p2p-bind-port`. |

When `--p2p-bind-ipv6-address` is set, the daemon binds a **separate** IPv6 listener in addition to the existing IPv4 listener. The IPv6 listener uses `IPV6_V6ONLY=0` (dual-stack mode), so it also accepts IPv4-mapped connections (e.g. `::ffff:1.2.3.4`) transparently.

**Example — listen on all IPv6 interfaces, same port as IPv4:**
```
Wrkzd --p2p-bind-ipv6-address ::
```

**Example — listen on a specific IPv6 address with a different port:**
```
Wrkzd --p2p-bind-ipv6-address 2001:db8::1 --p2p-bind-port-ipv6 17856
```

---

## RPC Server

| Flag | Default | Description |
|------|---------|-------------|
| `--rpc-bind-ip` | `127.0.0.1` | IPv4 interface for the RPC server |
| `--rpc-bind-port` | `17856` | TCP port for the RPC server |
| `--rpc-bind-ipv6-address` | *(empty)* | IPv6 address for the RPC server. Examples: `::` (all interfaces), `::1` (loopback). Empty = IPv6 RPC disabled. |
| `--rpc-use-ipv6` | `false` | Enable IPv6 for the RPC server. Must also set `--rpc-bind-ipv6-address`. |

When `--rpc-use-ipv6` is set and `--rpc-bind-ipv6-address` is non-empty, the daemon starts a **second** RPC server instance bound to the IPv6 address on the same port as `--rpc-bind-port`. An IPv6 bind failure is non-fatal — a warning is logged but the daemon continues running with IPv4 RPC only.

**Example — expose RPC on all IPv6 interfaces:**
```
Wrkzd --rpc-bind-ipv6-address :: --rpc-use-ipv6
```

**Example — loopback only:**
```
Wrkzd --rpc-bind-ipv6-address ::1 --rpc-use-ipv6
```

---

## RPC Security & Rate Limiting

| Flag | Default | Description |
|------|---------|-------------|
| `--daemon-mode` | `standard` | RPC mode: `standard` or `explorer`. Explorer mode enables additional block/tx query endpoints. |
| `--rpc-access-token` | *(empty)* | Require this token in `X-API-Key` or `Authorization: Bearer <token>` header. Empty = no auth required. |
| `--enable-cors` | *(empty)* | Add `Access-Control-Allow-Origin` response header. Use `*` to allow all origins. |
| `--rpc-trust-proxy` | `false` | Trust `X-Forwarded-For` header for the client IP. **Enable only behind a trusted reverse proxy.** |
| `--rpc-read-timeout` | `15` | RPC read timeout in seconds |
| `--rpc-write-timeout` | `30` | RPC write timeout in seconds |
| `--rpc-max-body-bytes` | `2097152` | Maximum RPC request body size in bytes (default 2 MB) |
| `--rpc-max-rpm` | `240` | Max RPC requests per minute per client IP. `0` disables rate limiting. |
| `--rpc-max-global-index-range` | `5000` | Max block range for `get_global_indexes_for_range` |
| `--rpc-max-block-count` | `100` | Max `blockCount` for wallet/raw-block sync RPC methods |

---

## Local IPC Sockets (`--rpc-ipc-path`, `--bind-ipc-path`)

Every listener that speaks HTTP can also be served over an **AF_UNIX socket** instead of,
or alongside, a TCP port. Nothing is bound unless a path is given: IPC is off by default
in all four binaries.

The point is not to avoid loopback but to change who decides. A TCP listener is reachable
by every process on the machine and defends itself with a shared secret; a socket file is
reachable only by whoever the mode on that file admits, and the kernel enforces it.

**Not available on Windows.** Windows has had AF_UNIX since Windows 10 1803, but the socket
file carries no permissions the OS will enforce and there is no `SO_PEERCRED`, so the access
control this rests on would not exist. The flags are accepted and ignored with a warning
rather than opening an unrestricted endpoint.

### Serving

| Binary | Flag | Default | Description |
|--------|------|---------|-------------|
| `Wrkzd` | `--rpc-ipc-path` | *(empty)* | Also serve RPC on this socket, alongside the TCP listeners rather than instead of them |
| `Wrkzd` | `--rpc-ipc-mode` | `0600` | Octal permissions for the socket file |
| `Wrkzd` | `--rpc-ipc-group` | *(empty)* | Group to own the socket file, for a `0660` shared setup |
| `Wrkzd` | `--rpc-ipc-require-token` | `false` | Also demand `--rpc-access-token` from IPC callers |
| `Wrkz-service` | `--bind-ipc-path` | *(empty)* | Serve JSON-RPC on this socket **instead of** the TCP port |
| `Wrkz-service` | `--bind-ipc-mode` | `0600` | Octal permissions for the socket file |
| `Wrkz-service` | `--bind-ipc-group` | *(empty)* | Group to own the socket file |
| `wallet-api` | `--rpc-ipc-path` | *(empty)* | Also serve the API on this socket |
| `wallet-api` | `--rpc-ipc-mode` | `0600` | Octal permissions for the socket file |
| `wallet-api` | `--rpc-ipc-group` | *(empty)* | Group to own the socket file |

All three also accept these keys in a config file and in `--dump-config` output.

A path must be absolute. Prefixing it with `@` uses the **Linux abstract namespace**, which
has no filesystem entry and therefore no permissions at all — every process in the network
namespace can connect, and the daemon warns when you ask for one.

### Connecting

Anywhere a daemon address is accepted, an absolute path, an `@name` or an `ipc://path`
means the daemon's local socket rather than a host. One spelling works across all of them:

```
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock

miner --daemon-address /run/wrkz/wrkzd.sock --address WRKZ...
zedwallet++ --remote-daemon /run/wrkz/wrkzd.sock
Wrkz-service --daemon-address /run/wrkz/wrkzd.sock --container-file w --container-password p
```

The daemon's own console uses the IPC socket automatically whenever one is bound, so
`status`, `print_cn` and friends stop making loopback TCP connections to their own process.

### Authentication

On the daemon's IPC socket `--rpc-access-token` is **not** required. The mode on the socket
file already decided who may connect and the kernel enforced it, so the token adds nothing
except an obligation to hand the secret to every local integration. Set
`--rpc-ipc-require-token` to demand both. Rate limiting is skipped on IPC for the same
reason it is skipped on loopback — the caller is neither anonymous nor remote.

`wallet-api` and `Wrkz-service` still require their password on IPC. Those endpoints move
money, and silently dropping an authentication step that was previously unconditional is
not a change worth making by default.

### Examples

Owner-only socket, the default:

```
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock
```

Shared with a group, so an unprivileged service account can reach the node:

```
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock --rpc-ipc-mode 0660 --rpc-ipc-group wrkz
```

Wallet service with no TCP port at all, talking to the daemon over IPC as well:

```
Wrkz-service --bind-ipc-path /run/wrkz/service.sock \
             --daemon-address /run/wrkz/wrkzd.sock \
             --container-file wallet --container-password hunter2
```

ZMQ over IPC — `--zmq-pub` hands its endpoint straight to libzmq, so this already worked:

```
Wrkzd --zmq-pub ipc:///run/wrkz/wrkzd.zmq
```

### Behaviour and guarantees

- **Permissions are in place before the first client can connect.** The socket is created
  under a umask derived from the requested mode rather than chmod'ed after the fact, so
  there is no window in which it is reachable more widely than asked for.
- **A stale socket from a crashed run is cleared automatically.** A path that exists but is
  not a socket is never removed, and a path another process is still listening on is
  refused rather than stolen.
- **The socket file is unlinked on shutdown.**
- **A failed IPC bind is not fatal for the daemon or wallet-api** — it warns and keeps
  serving on the TCP listeners, matching how an IPv6 bind failure is handled. For
  `Wrkz-service`, where IPC replaces the TCP port, a failed bind stops startup.
- Socket paths are limited to 107 bytes by the kernel, not by us.

---

## ZMQ Publisher

| Flag | Default | Description |
|------|---------|-------------|
| `--zmq-pub` | `tcp://127.0.0.1:17857` | ZMQ PUB endpoint. Empty string disables the publisher. |
| `--no-zmq` | `false` | Disable the ZMQ publisher even if `--zmq-pub` is set. |

The ZMQ publisher broadcasts new block and transaction events. Useful for downstream services (e.g. explorers, wallets) that need real-time chain updates without polling.

---

## Notification Hooks (`--block-notify`, `--tx-notify`, ...)

Monero-style push hooks. Each flag takes **either** an `http://` / `https://` URL (the event is POSTed as a JSON object) **or** a command template (run directly, *without a shell*; quotes group arguments, `%`-placeholders are substituted per argument, `%%` is a literal percent). Empty = disabled.

### Daemon (`Wrkzd`)

| Flag | Fires on | Placeholders | JSON body |
|------|----------|--------------|-----------|
| `--block-notify <cmd\|url>` | every new main-chain block (and each replacement block after a reorg) | `%s` block hash, `%h` height | `{"event":"block","height":N,"hash":"..."}` |
| `--reorg-notify <cmd\|url>` | every chain reorganisation | `%s` split height, `%h` new height, `%n` new blocks, `%d` discarded blocks | `{"event":"reorg","split_height":N,"new_height":N,"new_blocks":N,"discarded_blocks":N}` |
| `--tx-notify <cmd\|url>` | every transaction entering the mempool | `%s` tx hash | `{"event":"tx","hash":"..."}` |
| `--notify-during-sync` | — | — | also fire while the node is still synchronizing (default: suppressed until synced) |

All three are also accepted in the daemon config file (`block-notify`, `reorg-notify`, `tx-notify`, `notify-during-sync`).

### Wallet service (`Wrkz-service`)

| Flag | Fires on | Placeholders |
|------|----------|--------------|
| `--tx-notify <cmd\|url>` | every new wallet transaction when first seen: incoming (pool or block) and outgoing once sent | `%s` hash, `%h` height (0 = unconfirmed), `%a` amount (atomic, signed), `%f` fee, `%p` payment id, `%c` confirmed 0/1 |
| `--tx-confirmed-notify <cmd\|url>` | once per transaction, when it gets into a block | same as above |
| `--notify-during-sync` | — | also fire for transactions more than `WALLET_NOTIFY_SYNC_LAG_BLOCKS` (1440) blocks behind the daemon, i.e. during a rescan (default: suppressed) |

JSON body: `{"event":"tx"|"tx_confirmed","hash":"...","height":N,"amount":N,"fee":N,"paymentId":"...","confirmed":bool,"timestamp":N,"unlockTime":N}`.

### wallet-api

| Flag | Fires on | Placeholders |
|------|----------|--------------|
| `--tx-notify <cmd\|url>` | every transaction **confirmed in a block** for the open wallet (wallet-api does not scan the pool for incoming transfers) | `%s` hash, `%h` height, `%a` amount, `%f` fee, `%p` payment id |
| `--notify-during-sync` | — | same rescan suppression rule as the wallet service |

JSON body as for the wallet service plus `"isCoinbase":bool`; `confirmed` is always `true`.

### Examples

```
# built-in webhook (no external tools needed)
Wrkzd --block-notify https://example.com/hooks/wrkz-block
Wrkz-service ... --tx-notify http://127.0.0.1:9000/tx --tx-confirmed-notify http://127.0.0.1:9000/tx-confirmed

# Monero-compatible command form
Wrkzd --block-notify "/usr/local/bin/on-block.sh %s"
Wrkzd --block-notify "curl -s -X POST https://example.com/hook -d hash=%s -d height=%h"
```

### Behaviour and guarantees

- **Never blocks the node/wallet.** Events are queued in memory and delivered from a dedicated worker thread; the daemon's dispatcher and the wallet's sync loop only enqueue. The queue is bounded (1024 entries); when a receiver is too slow, excess notifications are dropped and counted (`dropped=` is logged on shutdown).
- **Timeouts.** Each delivery has a 10 s budget: hung commands are killed, slow HTTP calls are aborted. Transport failures on webhooks get one retry; HTTP 4xx/5xx responses are final.
- **Fire-and-forget.** There is no persistent retry queue; receivers should treat notifications as hints and reconcile through RPC (same model as Monero / ZMQ).
- **HTTPS** webhooks require a build with OpenSSL (`CPPHTTPLIB_OPENSSL_SUPPORT`); otherwise the hook is disabled with a warning at startup.
- Commands run with the privileges of the daemon/wallet process. Child stdout/stderr are inherited.

---

## Ban System (`ban` command)

The in-memory ban list supports **both IPv4 and IPv6** addresses:

```
ban list                    # List all banned IPs (IPv4 and IPv6)
ban add <ip> [seconds]      # Ban an IPv4 or IPv6 address (default: 900s)
ban delete <ip>             # Remove a ban by IPv4 or IPv6 address
```

IPv6 addresses should be in standard notation (brackets optional for the CLI):

```
ban add 2001:db8::1 3600
ban add ::ffff:1.2.3.4 900
ban delete 2001:db8::1
```

IPv4 and IPv6 bans are stored in separate lists internally. IPv4 bans can prevent connections from both the IPv4 listener and IPv4-mapped clients on the IPv6 listener.

---

## Peer Connection Settings

| Flag | Default | Description |
|------|---------|-------------|
| `--out-peers` | `15` | Maximum number of outgoing P2P connections |
| `--in-peers` | `15` | Maximum number of incoming P2P connections |
| `--allow-local-ip` | `false` | Allow the local machine's IP to be added to the peer list |
| `--hide-my-port` | `false` | Do not announce this node as a peer list candidate |
| `--p2p-reset-peerstate` | `false` | Generate a new peer ID and discard saved peer state (`p2pstate.wrkz.bin`) |
| `--add-peer` | *(none)* | Add a peer to the local peer list: `--add-peer 1.2.3.4:17855` |
| `--add-priority-node` | *(none)* | Add a peer and actively maintain a connection to it |
| `--add-exclusive-node` | *(none)* | Connect **only** to this node (bypasses all other peer discovery) |
| `--seed-node` | *(none)* | Bootstrap from this node (connect, fetch peers, then disconnect) |

Multiple `--add-peer`, `--add-priority-node`, `--add-exclusive-node`, and `--seed-node` flags may be specified.

---

## Seed Nodes & DNS Seeds

Static seed nodes are compiled in (`CryptoNoteConfig.h → SEED_NODES`). DNS-based seeds (`DNS_SEED_NODES`) are resolved at startup using `IpResolver::resolveAll()` which queries **both A and AAAA records**. IPv4 results are added to the IPv4 seed list; IPv6 results are added to a separate IPv6 seed list. The daemon bootstraps from either or both, whichever respond first.

To publish IPv6 seed nodes, simply add AAAA records to the seed hostnames in your DNS zone — no daemon configuration changes are needed.

---

## Database Settings

| Flag | Default | Description |
|------|---------|-------------|
| `--db-enable-compression` | `true` | Enable RocksDB block compression |
| `--db-max-open-files` | `4096` | Max number of files RocksDB can have open simultaneously |
| `--db-read-buffer-size` | `256` | RocksDB read cache size in MB |
| `--db-write-buffer-size` | `64` | RocksDB write buffer size in MB |
| `--db-threads` | `8` | RocksDB background compaction/flush threads |
| `--skip-boot-compaction` | `false` | Skip the automatic DB compaction check at startup |

---

## Syncing Settings

| Flag | Default | Description |
|------|---------|-------------|
| `--transaction-validation-threads` | *(hardware concurrency)* | Threads for parallel tx input validation during sync |
| `--sync-max-peers` | `3` | Max peers to sync blocks from simultaneously |
| `--sync-peer-failure-threshold` | `2` | Failures allowed for a sync peer before it is demoted |
| `--sync-batch-min` | `120` | Minimum adaptive block request batch size |
| `--sync-batch-max` | `600` | Maximum adaptive block request batch size |
| `--block-sync-size` | `600` | Max blocks requested per sync chunk |
| `--block-sync-bytes` | `16777216` | Max approximate bytes requested per sync chunk (default 16 MB) |

### Auto-Prune & Auto-Compaction

These thresholds control when the daemon automatically prunes old blocks or compacts the database during normal operation. They only apply when `--prune` is enabled (for auto-prune) or at all times (for auto-compaction).

| Flag | Default | Description |
|------|---------|-------------|
| `--auto-prune-min-gap-blocks` | `120` | Minimum blocks between automatic prune passes. `0` disables periodic auto-prune. |
| `--auto-compaction-min-gap-blocks` | `720` | Minimum blocks between automatic DB compactions. `0` disables periodic auto-compaction. |
| `--auto-prune-min-free-bytes` | `4294967296` | Min free disk space (bytes) before regular auto-prune schedule activates (default 4 GB). Low-space mode can still force a prune earlier. |
| `--auto-compaction-min-free-bytes` | `8589934592` | Min free disk space (bytes) required to start automatic compaction (default 8 GB). |

---

## Daemon / Logging Settings

| Flag | Default | Description |
|------|---------|-------------|
| `--data-dir` | *(platform default)* | Path to the blockchain data directory |
| `--log-file` | `WRKZCoind.log` | Path to the log file |
| `--log-level` | `3` (WARNING) | Log verbosity: `0`=FATAL `1`=ERROR `2`=WARNING `3`=INFO `4`=DEBUG `5`=TRACE |
| `--no-console` | `false` | Disable the interactive daemon console |
| `--load-checkpoints` | `default` | CSV checkpoint file path, or `default` for built-in checkpoints |
| `--config-file` | *(none)* | Load settings from a config file (ini or JSON) |
| `--save-config` | *(none)* | Write the current resolved configuration to a file and exit |
| `--dump-config` | `false` | Print the current resolved configuration to stdout |

### Blockchain Operations

| Flag | Description |
|------|-------------|
| `--resync` | Delete existing blockchain data and start a full re-sync from scratch |
| `--rewind-to-height #` | Roll back the local blockchain to the given height (must be > 0; use `--resync` for full reset) |
| `--prune` | Enable pruned-node mode — only `--prune-depth` recent blocks are kept locally |
| `--prune-depth #` | Number of recent blocks to retain in prune mode (minimum: ~7 days ≈ 10080 blocks) |
| `--import-blockchain` | Import blockchain DB from a dump file |
| `--export-blockchain` | Export blockchain DB to a dump file |
| `--max-export-blocks #` | Maximum number of blocks to include in an export |

---

## What Works / What Doesn't

### Working
- **IPv4 P2P listening** — always enabled, unchanged
- **IPv6 P2P inbound** — enabled with `--p2p-bind-ipv6-address`; dual-stack socket accepts both IPv4 and native IPv6 peers on one port
- **IPv4-mapped clients on IPv6 listener** — transparently unwrapped; existing ban / peer-list logic applies
- **IPv6 peer-list exchange** — `local_peerlist6` field in handshake and timed-sync responses (v18+ peers only; v17 peers silently ignore it)
- **IPv6 banning** — `ban add <ipv6>` and `ban delete <ipv6>` work; checked in the IPv6 accept loop
- **Backward compatibility** — existing v17 nodes talk to v18 nodes without issues; IPv6 list is simply ignored by old nodes
- **DNS seed resolution (A + AAAA)** — both A records (IPv4) and AAAA records (IPv6) from `DNS_SEED_NODES` and `SEED_NODES` hostnames are resolved at startup. IPv4 seeds go to the IPv4 seed list; IPv6 seeds go to a separate IPv6 seed list.
- **Outbound IPv6 connections** — the daemon dials IPv6 seed nodes and discovered IPv6 peers using `TcpConnector::connect(IpAddress, port)`. IPv6 peers can be synced from alongside IPv4 peers.
- **IPv6 white-list promotion** — after a successful outbound IPv6 handshake the peer is added to the IPv6 white-list (`PeerlistManager::m_peers_white6`) and reused in future connection cycles.
- **`print_cn` IPv6 display** — `print_cn` and the sync status table correctly show the remote IPv6 address (e.g. `[2001:db8::1]:17855`) for both inbound and outbound IPv6 connections. `CryptoNoteConnectionContext` carries an `m_remote_ipv6` string alongside the legacy `m_remote_ip` uint32.
- **Daemon RPC IPv6** — `--rpc-bind-ipv6-address` + `--rpc-use-ipv6` now start a second RPC listener on the IPv6 address. Route registration is shared; IPv6 bind failure is non-fatal.
- **wallet-api IPv6** — `--rpc-bind-ipv6-address` + `--rpc-use-ipv6` start a second API listener on the IPv6 address.
- **HttpClient IPv6** — `NodeRpcProxy` (used by wallet-service and walletd) resolves hostnames using AF_UNSPEC (`IpResolver`), so it can connect to an IPv6 daemon when the address resolves to AAAA.
- **HttpServer IPv6** — wallet-service JSON-RPC can bind to an IPv6 address by passing `::1` or `::` as `--bind-address`.
- **Local IPC sockets** — `--rpc-ipc-path` (daemon, wallet-api) and `--bind-ipc-path` (wallet service) serve HTTP over an AF_UNIX socket whose file mode decides who may connect. Off unless a path is given; POSIX only. Clients reach one by passing the path as the daemon address, which also covers `Wrkz-service` — daemon and the daemon's own console.
- **zedwallet++ `--remote-daemon` IPv6** — bracket notation `[2001:db8::1]:17856` is accepted anywhere a daemon address is parsed (zedwallet++, miner, walletd).

### Known Limitations
- **Pure IPv6 inbound peer tracking** — peers that connect *inbound* over pure IPv6 are tracked in `m_remote_ipv6` for display but are not promoted to the IPv4 white-list (back-ping uses the IPv4 `NetworkAddress` struct). Outbound IPv6 connections are properly tracked in the IPv6 white-list.
- **IPv6 duplicate-connection detection** — `is_peer_used6()` deduplicates by peer ID only, not by IP. Duplicate attempts gracefully fail at handshake level.
- **IPC on Windows** — not supported. AF_UNIX exists there, but the socket file carries no permissions the OS enforces and there is no `SO_PEERCRED`, so the flags are ignored with a warning rather than opening an endpoint nobody can restrict.
- **P2P over IPC** — not offered. A local socket cannot carry a peer-to-peer network.
- **`--add-peer` / `--add-priority-node` / `--add-exclusive-node`** — these CLI flags only accept IPv4 addresses. IPv6 literal support requires updating `parsePeerFromString()` in `NetNodeConfig.cpp`.
