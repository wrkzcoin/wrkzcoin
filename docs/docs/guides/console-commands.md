# Daemon Console Commands

Commands available at the daemon's interactive console, and — identically — in a
session opened with [`Wrkzd attach`](ipc-and-console.md#attaching-a-console-to-a-running-daemon)
against a node already running.

Registered in `src/daemon/DaemonCommandsHandler.cpp`. `help` or `?` prints the
same list from the binary itself.

!!! note "Console commands are never served over TCP"
    They change log levels, ban peers, start compactions and stop the node, so
    they live on the local IPC socket alone (`POST /console`), where the mode on
    the socket file decides who may connect. There is no way to enable them on
    an RPC port, with or without a token.

## Session

| Command | What it does |
| --- | --- |
| `help`, `?` | Show the command list |
| `exit` | Local console: shut the daemon down. **Attached console: leave the session**, daemon carries on |
| `quit` | Leave an attached session |
| `stop` | Shut the daemon down. The same handler as the local `exit`, under a second name so that leaving an attached session is never confused with stopping the node |

## Status

| Command | What it does |
| --- | --- |
| `status` | Daemon status: height, network height, peers, difficulty, hash rate, uptime. On a lite node it also shows `Lite Node`, `Full Block Data From`, and `Lite Sync Stage: Index only` while the tip is still below the lite height |
| `sync_info` | Compact synchronization information |
| `sync_tune` | Current sync tuning and adaptive sync statistics |
| `sync_peers` | Per-peer sync diagnostics |
| `prune_status` | Prune mode and prune capability status |
| `db_status` | On-disk database status for the active engine |

## Inspecting the chain

| Command | What it does |
| --- | --- |
| `print_pl` | Print the peer list |
| `print_cn` | Print current connections. Shows IPv6 peers as `[2001:db8::1]:17855`, and which peers are pruned |
| `print_block <hash\|height>` | Print a block |
| `print_tx <hash>` | Print a transaction |
| `print_pool` | Print the transaction pool, long format |
| `print_pool_sh` | Print the transaction pool, short format |

On a [lite node](lite-node.md), `print_block` and `print_tx` cannot answer for
heights below the lite height: the block bodies and transaction records were
never written.

## Maintenance

| Command | What it does |
| --- | --- |
| `save` | Force-save blockchain state to disk |
| `set_log <level>` | Change the current log level. `0` FATAL, `1` ERROR, `2` WARNING, `3` INFO, `4` DEBUG, `5` TRACE |
| `compact_db [start\|status\|wait\|force]` | Manage database compaction. `wait` holds the connection until it finishes |
| `ban list` | List every banned address, IPv4 and IPv6 |
| `ban add <ip> [seconds]` | Ban an address. Default 900 seconds |
| `ban delete <ip>` | Lift a ban |

`compact_db` and `snapshot_export` both run in the background and report through
their `status` subcommand, so the console stays usable while they work. Commands
run one at a time across all consoles, local and attached; one that blocks holds
its connection until it finishes.

## Snapshots

| Command | What it does |
| --- | --- |
| `snapshot_export [start [height] [path]]` | Start a [lite node snapshot](lite-snapshots.md) export. With no arguments, `start` is assumed |
| `snapshot_export status` | While running: elapsed time, output path, current table, records kept of records scanned. When idle: the last result, including the payload digest |
| `snapshot_export cancel` | Cancel a running export. The partial file removes itself |

Shutting the daemon down during an export cancels it rather than waiting tens of
minutes for it to finish.

## Calling one from a script

```bash
curl -s --unix-socket /run/wrkz/wrkzd.sock \
  -H "Content-Type: application/json" \
  -d '{"command":"status"}' \
  http://localhost/console
```

Or pipe into `attach`, which exits when its input ends:

```bash
printf 'status\nprint_cn\n' | Wrkzd attach /run/wrkz/wrkzd.sock
```
