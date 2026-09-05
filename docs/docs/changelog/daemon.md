# Daemon Changelog

Changes to `Wrkzd` — consensus, P2P, storage, the RPC surface and the console —
that an operator or an RPC integrator has to know about. Wallet-side changes are
in the [Wallet API changelog](wallet-api.md).

The version string a binary reports is
`<major>.<minor>.<revision>.<build>`; `Wrkzd --version` prints it, and it is set
in `src/config/version.h.in`. Full commit history is on
[GitHub](https://github.com/wrkzcoin/wrkzcoin/commits/development).

---

## 0.4.8 (build 280) — 2026-09-05

The largest release since 0.4.4. Lite nodes, snapshots, a stratum server, an
optional transaction PoW service, and P2P version 19.

### Breaking and operator-visible

- **Nodes advertise P2P version 19.** The minimum accepted version stays 16, so
  older peers still connect. IPv6 peer exchange is now gated on version 19
  rather than being advertised unconditionally.
- **A database written by an older storage scheme is refused at startup**
  instead of being silently deleted. Resync, or import a lite snapshot. See
  [Lite Node Snapshots](../guides/lite-snapshots.md).
- **Lite mode cannot be combined with explorer mode.** `--lite --daemon-mode
  explorer` is refused at startup, because every explorer route below the lite
  height reads records lite mode does not store.
- **Windows builds could not create a database before this release.** RocksDB
  was compiled without ZSTD under MSVC, so every MSVC-built `Wrkzd` from this
  tree failed at database creation. Fixed in the build system; a rebuild is the
  fix, not `--db-enable-compression=false`.

### Lite nodes and snapshots

- `--lite` with `--lite-height <H>` keeps full block data only from `H` upward
  and stores just the indexes later blocks need below it. Permanent for the
  database. See [Lite Nodes](../guides/lite-node.md).
- `/info` gains `lite` and `lite_start_height`, and keeps answering while the
  node's own tip is still below `H`. The `status` console command shows
  `Lite Node`, `Full Block Data From` and the index-only sync stage.
- `/getwalletsyncdata` and `/getrawblocks` clamp a start height below the lite
  height up to it; `/get_global_indexes_for_range` refuses below it rather than
  narrowing the range.
- Peers asking for a block below the lite height get it in `missed_ids` rather
  than losing the connection, and can no longer shut a lite node down.
- **Snapshots**: `snapshot_export` writes the index-only region as a portable
  file; `--import-lite-snapshot <file>` loads one into an empty database.
  `--snapshot-info <file>` prints what a file carries. `--snapshot-stats`
  measures per-table storage. See [Lite Node Snapshots](../guides/lite-snapshots.md).

### Mining

- **Stratum server in the daemon**, off unless `--stratum-bind-port` is given
  and bound to loopback when it is. A stock miner can now mine solo with no pool
  and no bridge. See [Solo Mining](../guides/solo-mining.md).
- The merge-mining tag is sealed before a stratum job goes out, and blocks found
  over stratum are announced to peers immediately — they previously reached the
  network only through the next timed sync, which produced visible alt blocks.
- Stratum shares are hashed off the dispatcher thread.
- The daemon RPC speaks xmrig's solo-mining dialect: `/getinfo` and `/getheight`
  are served as aliases of `/info` and `/height`, and `/getheight` deliberately
  answers without a `hash` member, which is how those miners identify a
  CryptoNote daemon.
- `/json_rpc` echoes the request `id` back, as JSON-RPC 2.0 requires.
- `getblocktemplate` accepts `extra_nonce` as well as `reserve_size`.
- CryptoNight keeps its scratchpad for the life of the thread, roughly doubling
  hash rate. The bundled `miner`'s `--limit`, Ctrl+C handling and hash-rate
  report are fixed.

### Consensus

- **Default ring size rises to 8 at height 4,300,000.** The minimum accepted
  ring is judged on the smallest ring in a transaction, and a decoy shortage is
  reportable so wallets can fall back to a smaller ring instead of refusing to
  send.
- Short payment IDs are encrypted to the receiver, under extra sub-tag `0x03`.
  See [Encrypted Payment IDs](../guides/encrypted-payment-ids.md).

### Networking

- **Local IPC sockets**: `--rpc-ipc-path`, `--rpc-ipc-mode`, `--rpc-ipc-group`
  and `--rpc-ipc-require-token` serve daemon RPC over an AF_UNIX socket whose
  file mode decides who may connect. POSIX only. See
  [Local IPC and Console](../guides/ipc-and-console.md).
- **`Wrkzd attach <socket>`** opens an interactive console against a running
  daemon over its IPC socket, for a node under a process manager. The `/console`
  route is registered on the IPC listener only, never on TCP. Inside an attached
  session, `exit` leaves and `stop` shuts the node down.
- The daemon's own console routes its RPC calls over the IPC socket when one is
  bound, instead of making loopback TCP connections to itself.
- A daemon whose stdin is at end of file runs headless with one notice instead
  of spinning on empty reads.
- Seeds are retried when the node is stuck, DNS seed hostnames are re-resolved
  in the background, recently failed peers are skipped for 10 minutes, and one
  gray-list peer a minute is verified. `/info` reports `seed_nodes_count` and
  `last_seed_bootstrap`.

### Notification hooks

- `--block-notify`, `--reorg-notify` and `--tx-notify` take either a command
  template or an `http(s)://` webhook URL, suppressed until the node is synced
  unless `--notify-during-sync` is given. See
  [Notification Hooks](../guides/notify-hooks.md).

### Storage and performance

- New database tuning flags: `--db-compression-level`, `--db-row-cache-percent`,
  `--db-bottom-filters`, `--db-compression-dict-bytes`, `--db-block-size`.
- `--rpc-sync-cache-size` (default 64 MB) keeps finished `/getwalletsyncdata`
  bodies, so wallets syncing past the same height are served from one build.
- Optional sync fields on `/getwalletsyncdata`, advertised in `/info` as
  `sync_features`: `skipEmptyBlocks`, `encoding: "base64"` and `endHeight`.
- The packed output index table is folded into key output info.
- The daemon no longer compacts the whole database every minute while syncing,
  and the maintenance loop no longer makes the console look stuck.
- Boost is no longer a dependency of the tree.
- Bundled miniupnpc updated 2.1 → 2.3.3.

---

## 0.4.7 (build 270) — 2026-08-21

A stability and sync-performance release.

- **Reorg safety.** A shared mutex guards chain reads during a reorg; RPC
  endpoints and the sparse chain builder retry safely instead of racing it; alt
  block reorg gained null guards, retry logic and exception safety.
- **Alt chain management.** Stale alt chains deeper than 180 blocks behind the
  main chain are pruned, the alt chain count is capped and the weakest evicted
  by cumulative difficulty, total alt blocks are capped at 100, and pruning runs
  on every block add rather than only on main-chain blocks. A use-after-free in
  alt chain pruning was fixed by excluding the active cache.
- **Sync recovery.** Network-consensus block trust and a dynamic checkpoint
  recover a node stalled on local ring-signature validation.
- Transaction proof of work is skipped inside the checkpoint zone during sync.
- Block batches are requested before the current one finishes validating; block
  and wallet-sync serving reads are batched; bloom filters were added with
  index and filter memory bounded to the block cache; an fsync per block is
  avoided while catching up.
- RPC gains HTTP keep-alive, `TCP_NODELAY` and a wider thread pool.
- `validateTransaction` argument order was corrected for pool admission, and the
  pool's priority comparator made a strict weak ordering.
- rapidjson was replaced with nlohmann-json across the tree.

---

## 0.4.6 (build 265) — 2026-03-08 and earlier

See the [release list on GitHub](https://github.com/wrkzcoin/wrkzcoin/releases)
and the [commit history](https://github.com/wrkzcoin/wrkzcoin/commits/development).
