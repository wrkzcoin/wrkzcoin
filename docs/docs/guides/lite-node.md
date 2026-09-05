# Lite Nodes

A lite node stores **full block data only from a chosen height upward**. Below
that height it keeps just the indexes later blocks actually need: key output
info, spent key images, per-amount output counts and block headers. The block
bodies, transaction records, payment ID index and timestamp indexes for those
heights are never written.

This is a different feature from `--prune`, and the two cannot be combined.

| | `--prune` | `--lite` |
| --- | --- | --- |
| What it drops | Raw block bodies older than `--prune-depth` | Bodies, transaction records, payment IDs and timestamp indexes below `--lite-height` |
| When | A pass at every startup, after a full sync | At write time, from the very first block |
| Reversible | Yes, by resyncing | **No** — permanent for the database |
| Wallet rescan below the cut | Works | Does not work |

The in-repo long form, with storage measurements and design notes, is
`LITENODE.md`.

## Usage

```
--lite                  Enable lite mode. Permanent for this database.
--lite-height <H>       Height at and above which full block data is kept. Required.
```

A user who created a wallet at height 4,000,000 and wants a node just for it:

```bash
Wrkzd --lite --lite-height 4000000
```

That node syncs, mines, relays and validates exactly like a full node, and the
wallet syncs against it normally.

## Who should run one

| | |
| --- | --- |
| **A user**, own node for own wallet | **Yes.** This is what lite mode is for |
| **An operator**, public node others connect to | **No — run a full node**, with `--prune` if disk is tight |
| **An operator**, private or service node, wallets you control | **Yes**, good fit |

Never make a lite node a seed node.

### Choosing H is the whole decision

**Set `H` at or below your wallet's creation height.**

If `H` is above it, the wallet's scan is clamped up to `H` and it will never see
transactions between its creation height and `H`. The balance simply comes out
too low. The wallet logs a warning and `/info` reports `lite_start_height`, but
neither gets read when the number looks plausible.

```
H = min(earliest wallet scan height, current network top - 20160)
```

then round **down** by another 10-20k for margin. Err low: too low costs some
disk, too high costs transactions you cannot see.

Two things to accept before you start:

- **`H` cannot be changed without deleting the data directory.** The daemon
  refuses to start on a mismatch rather than wiping anything, but the choice is
  one way.
- **A lite node cannot serve a wallet older than its `H`.** Restoring an older
  seed later needs a full node, or a fresh lite node at a lower `H`.

### Why an operator usually should not

A public lite node is a worse network citizen and a support liability:

- It cannot serve blocks below `H`, so it cannot help new nodes bootstrap. Peers
  below `H` skip it as a sync source automatically. If everyone ran lite,
  nothing could sync.
- You do not know how old your users' wallets are. Someone restoring a two-year
  old seed against your node is clamped to `H` and sees a wrong balance. That
  reads as lost funds, and it reaches you as a support ticket rather than a bug
  report.

Lite mode suits an operator when **you control which wallets connect**: a
service node behind a recently created hot wallet, a mining node (mining reads
only `alreadyGeneratedCoins` and the last 100 block sizes, so history is
irrelevant to it), or a validation and relay node on cheap disk.

### Lite mode and explorer mode are mutually exclusive

`--daemon-mode explorer --lite` is refused at startup. Every explorer endpoint
below `H` — `f_blocks_list_json`, `f_block_json`, `f_transaction_json`,
`/queryblocksdetailed` — reads the transaction records lite mode drops, so it
would return nothing for those heights rather than report an error.

## It saves disk, not bandwidth

The node still downloads every block from 0 to `H` over P2P: the key output and
key image tables have to be built from real block data. What changes is that
most of what it downloads below `H` is used and discarded rather than written.

That pass is cheaper than a normal sync — below the last compiled-in checkpoint
the daemon already skips proof-of-work and ring-signature verification, and lite
mode also skips the majority of the database writes — but it is still a full
download measured in hours. Roughly 4,000 blocks/min on a modest box at low
heights, slower through denser later blocks.

To skip that download entirely, import a snapshot instead: see
[Lite Node Snapshots](lite-snapshots.md).

## Running one

**Startup refuses rather than repairs.** Every mismatch between the flags and
what the database was built as exits without touching it. Deleting the data
directory is always left to you. A daemon that will not start is telling you
something, and the fix is never to add a force flag.

| Combination | Result |
| --- | --- |
| `--lite` without `--lite-height` | Refused — there is no safe default |
| `--lite --prune` | Refused — mutually exclusive |
| `--lite --daemon-mode explorer` | Refused — explorer needs records lite drops |
| `--lite` on a database synced in full | Refused — delete the data directory yourself |
| No `--lite` on a lite database | Refused — it has no block bodies to serve |
| `--lite-height H'` on a database built at `H` | Refused — `H` cannot be changed |
| `--rewind-to-height` below `H` | Refused — those blocks cannot be undone |
| A database from an older storage format | Refused — restart with `--resync`, the only thing that deletes a chain |

**The 14-day rule is checked at handshake time**, not at startup, because the
network's height is not knowable before then. If `H` is too close to the tip the
daemon exits and names the highest value the network currently allows. The check
is deliberately hard to trigger: once the node's own chain reaches
`H + 20160` the question is settled from local data and no peer is consulted at
all, and before that what is weighed is the *tallest* chain claimed across
several peers, so no single peer can produce the verdict.

### Monitoring

- `/info` reports `lite` and `lite_start_height`.
- `status` shows `Lite Node`, and on a lite node `Full Block Data From`. While
  the node's own tip is still below `H` it also shows
  `Lite Sync Stage: Index only`, which is why `Block Version` reads `v0` for
  that part of the sync.
- `print_cn` shows which peers are pruned.

## Changing the lite height

`H` cannot be changed in place, but a database can be rebuilt at a different
one:

```bash
Wrkzd --resync --lite --lite-height <new H>
```

`--resync` deletes the chain and the peer state, and the rebuild starts at the
new height. This is the supported way to move `H`, and the reason changing
`--lite-height` on its own is refused: the destructive step has to be asked for
rather than inferred.

Raising `H` on an existing database — turning blocks between the old and new
height from full into index-only — is a deletion pass rather than a download,
and is not implemented.

## RPC behaviour

- `/info` gains `lite` (bool) and `lite_start_height`.
- `/info` reports `major_version` and `minor_version` as `0` while the node's
  own top block is still below `H`, which is most of a first sync. Reading them
  needs the block body, and there is none down there. Everything else in the
  response works normally.
- `/getwalletsyncdata` and `/getrawblocks` **clamp** a start height below
  `lite_start_height` up to it, and drop any start timestamp. A wallet that asks
  from 0 is served from the lite height rather than fed an empty response
  forever.
- `/get_global_indexes_for_range` **refuses** below `lite_start_height`. There
  is nothing sensible to clamp to — a silently narrowed range would read as
  "these heights hold no transactions", which is a different and wrong answer.
- Peers asking for a block below the lite height get it reported in
  `missed_ids`, so they ask elsewhere instead of losing the connection.

## Wallet behaviour

Every wallet built on `walletbackend` — `wrkz-wallet`, `wrkz-wallet-api`, the C
API the desktop and mobile wallets use, and the WASM web wallet — reads
`lite_start_height` from `/info` and acts on it.

| The wallet | What happens |
| --- | --- |
| Has scanned nothing yet | Its scan height is floored to `lite_start_height`. A warning is logged; `status` names the floor |
| Has scanned to a height at or above `lite_start_height` | Nothing changes. It carries on |
| Has scanned to a height **below** `lite_start_height` | **Sync stops** |

The third case is the one worth understanding. The daemon would answer from its
lite height whatever the wallet asked for, so continuing would move the wallet's
recorded position over blocks nobody ever looked at: the wallet would report
itself synced while every transaction in the skipped stretch was missing.
Stopping is the honest answer. Point that wallet at a full node, or rebuild the
lite node at a lower `H`.

Rescanning a wallet below `H` against a lite node does not work, for the same
reason.
