# Lite Node

A lite node stores **full block data only from a chosen height upward**. Below that height
it keeps just the indexes that later blocks actually need: key output info, spent key
images, per-amount output counts, and block headers. The block bodies, transaction
records, payment ID index and timestamp indexes for those heights are never written.

This is a different feature from `--prune`, and the two cannot be combined.

| | `--prune` | `--lite` |
|---|---|---|
| What it drops | Raw block bodies older than `--prune-depth` | Bodies, transaction records, payment IDs and timestamp indexes below `--lite-height` |
| When | A pass at every startup, after a full sync | At write time, from the very first block |
| Reversible | Yes, by resyncing | No — permanent for the database |
| Wallet rescan below the cut | Works | Does not work |

## Usage

```
--lite                  Enable lite mode. Permanent for this database.
--lite-height <H>       Height at and above which full block data is kept. Required.
```

Example — a user who created a wallet at height 4,000,000 and wants a node just for it:

```
Wrkzd --lite --lite-height 4000000
```

That node syncs, mines, relays and validates exactly like a full node, and the wallet
syncs against it normally.

## What it does and does not save

**It saves disk, not bandwidth.** The node still downloads every block from 0 to `H` over
P2P — the key output and key image tables have to be built from the real block data, and
there is no trusted snapshot to skip ahead to. What changes is that most of what it
downloads below `H` is used and discarded rather than written.

That pass is cheaper than a normal sync: below the last compiled-in checkpoint the daemon
already skips proof-of-work and ring-signature verification, and lite mode also skips the
majority of the database writes. Expect it to be faster than a full sync, but still a full
download measured in hours.

## Security

**A lite node validates exactly as strongly as a full node**, provided `H` sits below the
last checkpoint. Ring member resolution reads only the key output table, and double-spend
detection reads only the key image table — both of which a lite node keeps complete from
genesis. Nothing consensus depends on is missing.

This is because lite mode is built at *write* time from a normal sync. It is not a
snapshot, introduces no new trust assumption, and needs no fork.

## What is lost

- **A restored wallet cannot rescan below `H` against this node.** A wallet that already
  holds its own outputs can still spend funds received before `H` — the node supplies
  decoys and validates the spend — but a wallet re-scanning from a seed will not find
  them. Wallets should be created at or above `H`.
- **Block explorer and transaction lookups below `H` fail.** No transaction records are
  stored there.
- **The node cannot serve blocks below `H` to peers.** It reports them as missed so the
  peer asks elsewhere.

## Constraints

**`H` must be at least `MIN_LITE_FULL_BLOCK_DEPTH` (14 days, 20,160 blocks) below the
network top.** This keeps a comfortable margin of full block data above `H`, so no reorg
can ever reach into the index-only region — well clear of `MAX_BLOCK_ALLOWED_TO_REWIND`,
which is three days.

**The mode is permanent for the database.** It is recorded under the `lite_node_profile`
key the first time the database is created, and every later start must agree with it:

| Database was built as | Started with | Result |
|---|---|---|
| *(new)* | `--lite --lite-height H` | Recorded, proceeds |
| *(new)* | no lite flags | Full node, proceeds |
| `lite:H` | `--lite --lite-height H` | Proceeds |
| `lite:H` | `--lite --lite-height H′` | Refuses |
| `lite:H` | no lite flags | Refuses |
| full | `--lite …` | Refuses |

Every refusal **exits without touching the database**. Switching modes means deleting the
data directory yourself — the daemon will never do it for you, because dropping a synced
chain over a forgotten flag would be the worst possible reading of an operator's intent.

`--rewind-to-height` below `H` is refused for the same reason.

## What is retained below the lite height

| Prefix | Contents | Kept | Why |
|---|---|---|---|
| `"j"` | `(amount, globalIndex)` → `KeyOutputInfo` | **yes** | Ring member resolution, decoy serving |
| `"7"` | key image → block index | **yes** | Double-spend detection |
| `"b"` | `(amount, idx)` → `PackedOutIndex` | **yes** | Decoy maturity check |
| `"h"` | per-amount output counts | **yes** | Ring index bounds |
| `"6"` | `CachedBlockInfo` | **yes** | Difficulty, emission, timestamps, block sizes |
| `"5"` | block hash → index | **yes** | Chain queries |
| `"4"` | raw block body | no | Where ring signatures live — the bulk of the chain |
| `"a"` | `ExtendedTransactionInfo` | no | Only needed for rescan and explorer |
| `"1"` | block index → tx hashes | no | Only read alongside `"a"` |
| `"0"` | block index → key images | no | Rewind index only; a lite node never rewinds that far |
| `"f"` | payment ID → tx hash | no | Explorer only |
| `"e"`, `"g"` | timestamp indexes | no | Answer "which height was this date" for scans that cannot start below `H` anyway |

The running transaction counter is still updated for skipped transactions, so
`getBlockchainTransactionCount()` and `tx_count` in `/info` stay chain-wide totals.

The genesis block is always stored in full, whatever the lite height. It is written by a
different path than every other block, and too much code reads block zero directly for a
half-stored genesis to be safe. It is one block.

## Measuring storage

`--snapshot-stats` walks every key and reports record counts and byte totals per
table, then exits. It takes minutes on a synced chain. Run it before making any
decision about snapshot size or format:

```
Wrkzd --snapshot-stats --data-dir <dir>
```

Rows tagged `(snapshot)` are what a lite-node snapshot must carry.

A measurement at 27,956 blocks (lite height 3,000) gives the shape of it:

| Table | Records | Stored |
|---|---|---|
| key output info `"j"` | 1,825,692 | 358.7 MB |
| key output indexes `"b"` | 1,825,770 | 139.3 MB |
| spent key images `"7"` | 347,577 | 26.8 MB |
| block info `"6"` | 27,957 | 6.3 MB |
| **snapshot payload** | | **531.2 MB** |

Two things to take from it. **Key outputs are ~94% of a snapshot** — block info is
about 1%, so shrinking the block section is not worth doing. And the stored form
carries heavy per-record framing: `KeyOutputInfo` is 74 bytes of payload in 196
bytes on disk, because the KV binary format writes field names into every record.
A packed canonical dump is roughly 3.2x smaller than these totals, and merging
`"j"` with `"b"` (they are 1:1 by `(amount, globalIndex)`) drops a duplicated key
encoding on top of that.

Do not extrapolate the table above to a full chain: early blocks are dense in
denominated outputs and are not representative. Measure on a synced node.

## RPC behaviour

- `/info` gains `lite` (bool) and `lite_start_height`.
- `/getwalletsyncdata` and `/getrawblocks` **clamp** a start height below `lite_start_height`
  up to it, and drop any start timestamp. A wallet that asks from 0 is served from the lite
  height rather than fed an empty response forever.
- `/get_global_indexes_for_range` **refuses** below `lite_start_height`. There is nothing
  sensible to clamp to — a silently narrowed range would read as "these heights hold no
  transactions", which is a different and wrong answer.
- Peers asking for a block below the lite height get it reported in `missed_ids`, so they
  ask elsewhere instead of losing the connection.
