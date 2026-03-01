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
| `--rpc-bind-ipv6-address` | *(empty)* | IPv6 address for the RPC server (future) |
| `--rpc-use-ipv6` | `false` | Enable IPv6 for the RPC server (future) |

> **Note:** `--rpc-bind-ipv6-address` and `--rpc-use-ipv6` are accepted by the CLI but the RPC server does not yet use them. IPv6 RPC support requires updating the HttpServer and is planned for a future release.

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

## ZMQ Publisher

| Flag | Default | Description |
|------|---------|-------------|
| `--zmq-pub` | `tcp://127.0.0.1:17857` | ZMQ PUB endpoint. Empty string disables the publisher. |
| `--no-zmq` | `false` | Disable the ZMQ publisher even if `--zmq-pub` is set. |

The ZMQ publisher broadcasts new block and transaction events. Useful for downstream services (e.g. explorers, wallets) that need real-time chain updates without polling.

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

### Not Yet Working / Known Limitations
- **`print_cn` IPv6 display** — connections from pure IPv6 peers (inbound or outbound) show remote IP as `0.0.0.0` because `CryptoNoteConnectionContext::m_remote_ip` is `uint32_t`. A future refactor is needed to store the full `IpAddress`.
- **RPC IPv6** — `--rpc-bind-ipv6-address` / `--rpc-use-ipv6` are parsed but not yet applied. The HTTP server only binds IPv4.
- **Pure IPv6 inbound peer tracking** — peers that connect *inbound* over pure IPv6 still have `m_remote_ip=0` so they are not promoted to the IPv4 white-list. Outbound IPv6 connections are properly tracked in the IPv6 white-list.
- **IPv6 duplicate-connection detection** — `is_peer_used6()` can only deduplicate by peer ID (not by IP), because `m_remote_ip` does not store IPv6. Duplicate attempts gracefully fail at handshake level.
- **`--add-peer` / `--add-priority-node` / `--add-exclusive-node`** — these CLI flags only accept IPv4 addresses. IPv6 literal support requires updating `parsePeerFromString()` in `NetNodeConfig.cpp`.
- **wallet-api** — no IPv6 CLI flags added yet.
