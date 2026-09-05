# Daemon Configuration Reference

Every `Wrkzd` command-line option, with the default the binary compiles in.
Defined in `src/daemon/DaemonConfiguration.h` and parsed in
`src/daemon/DaemonConfiguration.cpp`; `Wrkzd --help` prints the same list.

Every option here is also accepted as a key in a config file (INI or JSON) via
`--config-file`, and appears in `--dump-config` output.

## Configuration files

| Option | Default | Meaning |
| --- | --- | --- |
| `--config-file <path>` | none | Load settings from an INI or JSON file. Command-line options win over file values |
| `--save-config <path>` | none | Write the resolved configuration to a file and exit |
| `--dump-config` | `false` | Print the resolved configuration to stdout |
| `--data-dir <path>` | platform default | Blockchain data directory |

## Logging and console

| Option | Default | Meaning |
| --- | --- | --- |
| `--log-file <path>` | `WRKZCoind.log` | Log file path |
| `--log-level <0-5>` | `2` (WARNING) | `0` FATAL, `1` ERROR, `2` WARNING, `3` INFO, `4` DEBUG, `5` TRACE |
| `--no-console` | `false` | Do not start the interactive console. Use this under a process manager |
| `--attach <socket>` | none | Attach an interactive console to a daemon already running, over its IPC socket, instead of starting a node. `Wrkzd attach <socket>` is the same thing. See [Local IPC and Console](ipc-and-console.md) |

A daemon whose stdin is at end of file — what a process manager hands a daemon
started without `--no-console` — prints one notice and runs headless.

## RPC server

| Option | Default | Meaning |
| --- | --- | --- |
| `--rpc-bind-ip <ip>` | `127.0.0.1` | IPv4 interface for the RPC server |
| `--rpc-bind-port <port>` | `17856` | RPC port |
| `--rpc-bind-ipv6-address <addr>` | empty | IPv6 address for a second RPC listener |
| `--rpc-use-ipv6` | `false` | Start the IPv6 listener. Needs `--rpc-bind-ipv6-address` too |
| `--daemon-mode <mode>` | `standard` | `standard` or `explorer`. Explorer additionally serves the block-explorer routes |
| `--rpc-access-token <token>` | empty | Require this token in `X-API-Key` or `Authorization: Bearer`. Empty disables auth |
| `--enable-cors <origin>` | empty | `Access-Control-Allow-Origin` value |
| `--rpc-trust-proxy` | `false` | Take the client IP from `X-Forwarded-For`. **Only behind a trusted reverse proxy** |
| `--rpc-read-timeout <s>` | `15` | RPC read timeout, seconds |
| `--rpc-write-timeout <s>` | `30` | RPC write timeout, seconds |
| `--rpc-max-body-bytes <n>` | `2097152` | Maximum request body, bytes (2 MB). Over it returns `413` |
| `--rpc-max-rpm <n>` | `240` | Requests per minute per client IP. `0` disables. Over it returns `429` |
| `--rpc-max-global-index-range <n>` | `5000` | Cap for `/get_global_indexes_for_range` |
| `--rpc-max-block-count <n>` | `1000` | Cap on `blockCount` for wallet sync and raw-block methods |
| `--rpc-sync-cache-size <MB>` | `64` | Megabytes of finished wallet-sync responses to keep. `0` disables |

An IPv6 bind failure is non-fatal: the daemon warns and keeps serving IPv4.

## Local IPC socket

POSIX only; on Windows these are accepted and ignored with a warning. See
[Local IPC and Console](ipc-and-console.md).

| Option | Default | Meaning |
| --- | --- | --- |
| `--rpc-ipc-path <path>` | empty | Also serve RPC on this AF_UNIX socket, alongside the TCP listeners |
| `--rpc-ipc-mode <octal>` | `0600` | Permissions for the socket file |
| `--rpc-ipc-group <group>` | empty | Group to own the socket file, for a `0660` shared setup |
| `--rpc-ipc-require-token` | `false` | Also demand `--rpc-access-token` from IPC callers |

## P2P network

| Option | Default | Meaning |
| --- | --- | --- |
| `--p2p-bind-ip <ip>` | `0.0.0.0` | IPv4 interface for the P2P listener |
| `--p2p-bind-port <port>` | `17855` | P2P port |
| `--p2p-external-port <port>` | `0` | External port to announce, for NAT/UPnP |
| `--p2p-bind-ipv6-address <addr>` | empty | IPv6 bind address. `::` for all interfaces. Empty disables IPv6 |
| `--p2p-bind-port-ipv6 <port>` | `0` | IPv6 P2P port. `0` reuses `--p2p-bind-port` |
| `--out-peers <n>` | `15` | Maximum outgoing connections |
| `--in-peers <n>` | `15` | Maximum incoming connections |
| `--allow-local-ip` | `false` | Allow this machine's own IP into the peer list |
| `--hide-my-port` | `false` | Do not announce this node as a peer candidate |
| `--p2p-reset-peerstate` | `false` | New peer ID, discard saved peer state |
| `--add-peer <ip:port>` | none | Add a peer to the local list. Repeatable |
| `--add-priority-node <ip:port>` | none | Add a peer and keep a connection to it. Repeatable |
| `--add-exclusive-node <ip:port>` | none | Connect **only** to these, bypassing discovery. Repeatable |
| `--seed-node <host:port>` | none | Bootstrap from this node. Repeatable |

`--add-peer`, `--add-priority-node` and `--add-exclusive-node` accept IPv4
addresses only. `--seed-node` hostnames resolve to both A and AAAA records.

## Syncing

| Option | Default | Meaning |
| --- | --- | --- |
| `--transaction-validation-threads <n>` | hardware concurrency | Threads for parallel input validation |
| `--sync-max-peers <n>` | `3` | Peers to sync blocks from at once |
| `--sync-peer-failure-threshold <n>` | `2` | Failures before a sync peer is demoted |
| `--sync-batch-min <n>` | `120` | Minimum adaptive batch size |
| `--sync-batch-max <n>` | `600` | Maximum adaptive batch size |
| `--block-sync-size <n>` | `600` | Maximum blocks per sync chunk |
| `--block-sync-bytes <n>` | `16777216` | Approximate maximum bytes per sync chunk (16 MB) |

## Database

| Option | Default | Meaning |
| --- | --- | --- |
| `--db-enable-compression` | `true` | RocksDB block compression. This is a cxxopts implicit-value bool: write `--db-enable-compression=false`, not `--db-enable-compression false` |
| `--db-max-open-files <n>` | `4096` | Files RocksDB may hold open |
| `--db-read-buffer-size <MB>` | `256` | Read cache size |
| `--db-write-buffer-size <MB>` | `64` | Write buffer size |
| `--db-threads <n>` | `8` | Background compaction and flush threads |
| `--db-compression-level <n>` | `0` | ZSTD level for the bottommost level. `0` uses RocksDB's default of 3 |
| `--db-compression-dict-bytes <n>` | `0` | ZSTD dictionary size. `0` disables the dictionary |
| `--db-block-size <KB>` | `4` | SST block size |
| `--db-row-cache-percent <n>` | `0` | Share of the read buffer given to the row cache. `0` uses the built-in eighth |
| `--db-bottom-filters` | `false` | Keep bloom filters on the bottommost level. Costs space; makes spent key-image checks a lookup rather than a block read |
| `--skip-boot-compaction` | `false` | Skip the startup compaction check |

## Auto-prune and auto-compaction

| Option | Default | Meaning |
| --- | --- | --- |
| `--auto-prune-min-gap-blocks <n>` | `120` | Minimum blocks between automatic prune passes. `0` disables |
| `--auto-compaction-min-gap-blocks <n>` | `720` | Minimum blocks between automatic compactions. `0` disables |
| `--auto-prune-min-free-bytes <n>` | `4294967296` | Free space (4 GB) before the regular auto-prune schedule runs. Low-space mode can still force one earlier |
| `--auto-compaction-min-free-bytes <n>` | `8589934592` | Free space (8 GB) required to start an automatic compaction |

## Storage mode

`--prune` and `--lite` are different features and cannot be combined. See
[Lite Nodes](lite-node.md).

| Option | Default | Meaning |
| --- | --- | --- |
| `--prune` | `false` | Pruned node: keep only the most recent `--prune-depth` block bodies. Reversible by resyncing |
| `--prune-depth <n>` | `10080` | Blocks to retain when pruning. Minimum is the same 7 days (`10080` blocks at 60 s) |
| `--lite` | `false` | Lite node: store full block data only from `--lite-height` upward. **Permanent for the database** |
| `--lite-height <H>` | `0` | Height at and above which a lite node stores full blocks. Required with `--lite` |
| `--import-lite-snapshot <file>` | none | Load a lite node snapshot into an empty database, then exit. Needs `--lite` and the matching `--lite-height` |
| `--snapshot-info <file>` | none | Print what a snapshot file contains, as JSON, and exit. Reads the header only |
| `--snapshot-stats` | `false` | Report per-table storage, for sizing a snapshot |

## One-shot blockchain operations

Each of these does its work and exits.

| Option | Meaning |
| --- | --- |
| `--resync` | Delete the chain and peer state, then sync from scratch. The only option that deletes a chain |
| `--rewind-to-height <n>` | Roll the local chain back to this height. Must be above `0`; refused below a lite node's height |
| `--import-blockchain` | Import the database from a dump file |
| `--export-blockchain` | Export the database to a dump file |
| `--max-export-blocks <n>` | Cap the blocks included in an export |
| `--load-checkpoints <path\|default>` | CSV checkpoint file, or `default` for the compiled-in checkpoints |
| `--print-genesis-tx` | Print the genesis transaction and exit |
| `--version`, `--os-version`, `--help` | Print and exit |

## Stratum

Off unless `--stratum-bind-port` is given. See [Solo Mining](solo-mining.md).

| Option | Default | Meaning |
| --- | --- | --- |
| `--stratum-bind-port <port>` | `0` | Port to listen on. `0` leaves the server off |
| `--stratum-bind-ip <ip>` | `127.0.0.1` | Interface to listen on |
| `--stratum-share-difficulty <n>` | `0` | Difficulty given to miners. `0` uses the network difficulty |
| `--stratum-max-connections <n>` | `32` | Miners allowed on at once |

The stratum listener has no account and no password. Anyone who reaches the port
can mine to their own address using this node.

## ZMQ publisher

| Option | Default | Meaning |
| --- | --- | --- |
| `--zmq-pub <endpoint>` | `tcp://127.0.0.1:17857` | ZMQ PUB endpoint for block and transaction events. An empty string disables it. Accepts `ipc://` endpoints |
| `--no-zmq` | `false` | Disable the publisher even if `--zmq-pub` is set |

## Notification hooks

See [Notification Hooks](notify-hooks.md).

| Option | Default | Meaning |
| --- | --- | --- |
| `--block-notify <cmd\|url>` | empty | Fires on every new main-chain block |
| `--reorg-notify <cmd\|url>` | empty | Fires on every chain reorganisation |
| `--tx-notify <cmd\|url>` | empty | Fires on every transaction entering the mempool |
| `--notify-during-sync` | `false` | Also fire while the node is still synchronizing |

## Options that do nothing

| Option | Why |
| --- | --- |
| `--fee-address`, `--fee-amount` | Parsed and stored, but no RPC route serves them and the wallet backend hard-codes the node fee to zero. Node fees are not implemented on this network |
