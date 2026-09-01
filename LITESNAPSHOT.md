# Lite Node Snapshots

A lite node saves disk but not time: it still downloads every block from genesis and
builds the key output and key image indexes from them. On a modest machine that is a
day; on a laptop it has been measured at two to three. A snapshot replaces that work
with a file transfer.

This document is the design. See [LITENODE.md](LITENODE.md) for lite mode itself.

## What a snapshot is

**A base snapshot carries the index-only region `[0, H)` and nothing else.** That is
exactly the region a lite node keeps as bare indexes, and exactly the region that costs
the days. Above `H` a lite node stores full blocks, and those it fetches over P2P as
normal.

The boundary is `H`, always, and never a height relative to the chain tip. Three
consequences follow, and all three are the point:

- **A base snapshot is a pure function of the chain and `H`.** Two people exporting from
  two different nodes at two different tips produce the same snapshot. That is what lets
  a digest be published, compiled in, and checked by someone who did not build the file.

  Precisely: the **digest is over the uncompressed payload**, not over the file. Two
  builds carrying different zstd versions can emit different compressed bytes for the same
  records, and that must not invalidate a published digest. So the guarantee is that the
  digests match; the files are byte-identical too whenever the zstd versions agree.
  Reproducibility tests compare digests.
- **It never goes stale in content**, only in how much tail is left to sync.
- **`H` keeps meaning one thing.** It is what the operator typed, what `lite_node_profile`
  records, what `/info` reports as `lite_start_height`, what every wallet floors its scan
  to, and what error 62 refuses below. A snapshot that ended at `tip - N` would make that
  number depend on when the file was cut, and two operators who typed the same flag would
  get different wallets.

### Why not include the blocks above `H` too

Because the two regions are not the same kind of thing.

| | Below `H` | Above `H` |
|---|---|---|
| Content | derived index tables | raw blocks |
| Cost to rebuild | days | minutes, growing |
| Can the recipient verify it? | **No** - needs the block bodies it does not have | **Yes** - replay and check against checkpoints |
| Changes over time | never, for a given `H` | every block |

Mixing them would put a time-varying part inside the artifact whose whole value is that it
does not vary. If the tail ever becomes painful - a base at `H` = 4,000,000 leaves 200k
blocks today and will leave far more in a year - the answer is a **separate** block tail
file, which needs no digest and no trust because the recipient replays and validates it.
That is deferred; P2P covers 200k blocks in under an hour.

The real fix for an aging base is to publish a new blessed `H`, not to make one file
chase the tip.

## Trust

This is the part that is genuinely new, and it must not be softened.

A lite node built by syncing introduces no new trust assumption - [LITENODE.md](LITENODE.md)
can say it validates exactly as strongly as a full node. **An imported snapshot ends that.**
Two tables are 98% of the bytes and cannot be checked without the block bodies the
recipient does not have:

- **Key output info.** Outputs that never existed let whoever made the file spend coins
  that were never mined, into a wallet that will accept them when no other node does.
- **Spent key images.** Omissions let double spends through.

So a snapshot is only importable when its digest matches one compiled into the binary,
alongside the checkpoints. There is no force flag and no override. An unrecognised file
cannot be imported, and the fix is never to add one - the same rule the rest of lite mode
follows, applied where the downside is stolen money rather than a lost sync.

Two cheap checks run anyway, because they cost seconds and catch a mangled or careless
file before 5 GB is written:

- **Block info is self-auditable.** Cumulative difficulty must be monotonic and consistent
  with the difficulty algorithm over its own window; timestamps must sit inside the
  permitted window; `alreadyGeneratedCoins` deltas must match the reward formula given the
  recorded block sizes. A fabricated emission or difficulty schedule fails here.
- **Block hashes are checkpoint-checkable.** `CachedBlockInfo` carries no `prevHash`, so
  the list cannot be chain-linked, but it can be checked against every compiled-in
  checkpoint at or below `H`.

Neither touches the key outputs or the key images. The digest is what does.

## What the file carries

Three tables are serialised, three are derived on import, and the rest a lite node does
not have below `H` anyway.

| Prefix | Table | In the file | Why |
|---|---|---|---|
| `"6"` | `CachedBlockInfo` | **yes** | Difficulty, emission, timestamps, block sizes |
| `"7"` | key image to block index | **yes** | Double-spend detection |
| `"j"` | `KeyOutputInfo` | **yes** | Ring member resolution and decoy serving. The bulk |
| `"b"` | amount to output count | derived | Counting the `"j"` records of each amount |
| `"h"` | list of distinct amounts | derived | Falls out of the same walk |
| `"5"` | block hash to index | derived | `CachedBlockInfo` already holds the hash |

Written in that prefix order, which is also RocksDB's key order: every key of these tables
shares an identical serialised preamble ending in the one-byte prefix, so `"6" < "7" <
"j"` holds over the stored keys too. **The payload is therefore globally sorted**, which
is what lets the importer write SST files directly instead of going through the write
path. The three derived tables are built during the ingest, in their own sorted passes.

### Everything is filtered to `[0, H)`, not copied

This is the part that is easy to get wrong. An exporting node is at some tip well above
`H`, and its tables describe *that* tip. A snapshot has to describe the chain as it stood
at a top block of `H - 1`, so every table is filtered on the way out:

| Table | Filter |
|---|---|
| `"6"` | block index `< H` |
| `"7"` | value - the block the image was spent in - `< H` |
| `"j"` | `KeyOutputInfo.blockIndex < H` |

Global indexes survive that filter intact. They are assigned in block order, so for any
amount the outputs created below `H` hold exactly indexes `0 .. n-1` and the ones above
`H` follow. Dropping the tail leaves a contiguous prefix, and the per-amount count as of
`H - 1` is just how many survived - which is why `"b"` is derived rather than carried.
Copying `"b"` across would import counts as of the *exporter's* tip and corrupt the global
index of every output the importing node ever writes.

Two counters come out of the same filter:

- `KEY_OUTPUT_AMOUNTS_COUNT_KEY` is the number of distinct amounts that survived
- `TRANSACTIONS_COUNT_KEY` is `CachedBlockInfo[H-1].alreadyGeneratedTransactions` - the
  running counter is maintained for skipped transactions precisely so it stays a
  chain-wide total on a lite node

Both are also written into the header, so the importer can cross-check what it derived
against what the exporter believed. A mismatch refuses the file.

### Size

Measured on mainnet at `H` = 4,000,000, after the `"b"` fold-in of 2877c757:

| Table | Records | Logical |
|---|---|---|
| key output info `"j"` | 78.53M | 15,428.7 MB |
| spent key images `"7"` | 67.19M | 5,190.1 MB |
| block info `"6"` | 4.20M | 953.6 MB |
| **payload** | | **~21.6 GB logical** |

(`"b"` is ~140k records and ~7 MB after 2877c757 folded its per-output half into `"j"`;
it is derived on import rather than carried, so it is not in that total. The pre-fold
capture in `.howto/` still shows it at 78.7M records and 6 GB - do not size against that
file.)

The irreducible part - bytes no format and no compressor can shrink - is lower than the
figure in [LITENODE.md](LITENODE.md)'s floor table, because that table charges 64 bytes per
key output for `publicKey` plus `transactionHash` and **below `H` the hash is zeroed**:

| | records | random bytes each | irreducible |
|---|---|---|---|
| key output `publicKey` | 78.53M | 32 | 2.34 GiB |
| spent key images | 67.19M | 32 | 2.00 GiB |
| block hashes | 4.20M | 32 | 0.13 GiB |
| **floor** | | | **~4.5 GiB** |

So expect a **~5 to 5.5 GB** file. Re-measure with `--snapshot-stats` before trusting any
of this on another chain or another height; early blocks are dense in denominated outputs
and do not extrapolate.

## Blessed heights

`H` is not free. A snapshot is only importable at a height whose digest ships in the
binary, because the digest is the only thing standing between a user and a fabricated
chain state.

The first blessed height is **4,000,000**. Publish a new one roughly annually, or the tail
every importer has to sync grows without bound.

An operator may still run a lite node at any `H` they like by syncing it themselves. The
restriction is on importing, not on lite mode.

## Producing one

```
snapshot_export [start [path] | status | cancel]
```

A console command, following `compact_db`'s shape: it starts, reports, and can be
cancelled, and the console stays usable while it runs.

Default output is `<parent of the data directory>/wrkz-lite-base-h<H>-v1.litesnap`. There is no
timestamp in the name, deliberately: identical content must produce an identical name, or
nobody can tell two copies of the same snapshot apart from two different ones.

It refuses to start when:

- the chain tip is below `H + MIN_LITE_FULL_BLOCK_DEPTH` (20,160, the same margin lite
  mode already enforces at handshake). The region has to be settled beyond any reorg
- the output file exists
- free space is short. Checked before the write, not discovered 4 GB in

**A full node can produce one too**, and should - otherwise making a snapshot would first
require the days-long lite sync it exists to avoid. The export zeroes `transactionHash`
on every key output below `H` and skips the tables lite mode drops, so a full node and a
lite node at the same `H` emit the same bytes.

The walk holds a RocksDB snapshot for its duration ([`RocksDBWrapper::iterate`](src/cryptonotecore/RocksDBWrapper.cpp)),
which pins SST files: on a node still syncing, the data directory grows for as long as the
export runs. Export from a node at the tip where churn is low.

## Importing one

```
Wrkzd --lite --lite-height 4000000 --import-lite-snapshot <file>
```

In order:

1. **The database must be empty.** Anything else refuses, without touching it
2. Header `H` must equal `--lite-height`, and the genesis hash must be this network's
3. The payload digest must match the compiled-in entry for this network and `H`
4. The block info audit and the checkpoint comparison described under **Trust**
5. Records go through `SstFileWriter` and `IngestExternalFile` - one write of about the
   file's size, rather than the 25 to 30 GB the normal write path amplifies to. This is
   the difference between half an hour and several hours on a laptop
6. `"5"` and `"h"` are derived; the `"b"` counts carried in the file are cross-checked
   against a count of `"j"`, because an off-by-one here silently corrupts the global index
   of every output the node writes afterwards
7. `LAST_BLOCK_INDEX_KEY` = `H - 1`, plus `TRANSACTIONS_COUNT_KEY`,
   `KEY_OUTPUT_AMOUNTS_COUNT_KEY`, `lite_node_profile` = `lite:H`, and the scheme version
8. Startup continues normally and the node syncs `[H, tip]` over P2P

Step 8 is the whole point: after import the node is an ordinary lite node that happens to
already hold its index region. It validates, relays and mines exactly as one that synced
the region itself, and every wallet behaviour in [LITENODE.md](LITENODE.md) applies
unchanged.

## What this does not change

- **`H` still cannot be changed in place.** Importing at a different `H` means a different
  file and an empty database
- **A wallet older than `H` still cannot be scanned here.** A snapshot moves where the
  chain comes from, not what a lite node holds
- **Explorer mode is still refused.** The dropped tables are still dropped
- **Mobile still cannot run a node.** 5 GB and Android's background limits, unchanged

## Testing

One test decides whether any of this is real:

**Two nodes at different tips, exporting at the same `H`, must produce the same payload
digest.** Without it in CI the digest is decorative, nobody can reproduce a published
snapshot, and the trust model quietly reduces to "whoever sent you the file".

Beyond that: a full node and a lite node at the same `H` must agree on the digest; an
imported node and a natively synced node at the same `H` must have identical databases
under `--snapshot-stats`; and a truncated, a bit-flipped and a digest-mismatched file must
each be refused without writing anything.
