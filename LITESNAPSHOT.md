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

### What one actually came out at

The first real export, from a lite node at top block ~4,201,153:

| | |
|---|---|
| file | 5,588,563,802 bytes - **5.20 GiB** |
| records | 148,728,732, of which exactly 4,000,000 are block info |
| bytes per record, on disk | 37.6 |
| compression over the logical payload | **~4.0x** |
| distance above the entropy floor | **~16%** |

Two things worth taking from that.

**The floor is close, so there is nothing left to win on format.** Sixteen percent above
the irreducible bytes means no rearrangement of the container buys anything material. The
only lever that remains is dropping fields, which zeroing `transactionHash` already
did - and doing it again would mean finding another field a lite node never reads.

**The bespoke format was never justified by size, and this confirms it.** A tarball of the
compacted lite `DB` directory measured 5.80 GB at ZSTD-12, so this buys about 10% - which
is roughly what [LITENODE.md](LITENODE.md) predicted when it argued against building one.
The reason to have built it is that a tarball of SST files cannot be reproduced
byte-for-byte by a second person, so no digest over one could ever be checked. That is the
whole case, and it does not rest on the 10%.

**On the filter.** Restricting to `[0, H)` dropped 994,124 key output and key image
records across the 201,153 blocks above the lite height - about 4.9 per block, against a
chain-wide average of 34.7. The gap is what a quiet chain looks like from the top: recent
blocks are mostly empty, carrying a coinbase's few denominated outputs and no key images
at all, while the early blocks that lift the average are dense in denominations. Which is
the same reason not to extrapolate any of this to another height or another chain.
Re-measure with `--snapshot-stats` first.

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

Two runs. The first imports and exits; the second is an ordinary lite node.

```
Wrkzd --lite --lite-height 4000000 --import-lite-snapshot <file>
Wrkzd --lite --lite-height 4000000
```

**Why it exits rather than carrying straight on.** The import runs after the core has
loaded, because the genesis block is written in full by that load - raw block, base
transaction and all - and a snapshot carries none of it. Importing before that would leave
a half stored genesis, which too much code reads directly for it to be safe. But by then
the core has cached what it believes the chain's shape to be, and the import has just
changed it underneath. Restarting is cheap and settles the question; it is also what
`--import-blockchain` already does. For a host application driving the daemon it is
arguably nicer: one child process that imports, one that runs.

### What happens, in order

**Refusals first, before the file is read twice:**

1. The database must hold nothing but genesis
2. The genesis hash in the header must be this network's
3. The header's height must equal `--lite-height`
4. The header's digest must appear in `LITE_SNAPSHOT_DIGESTS`

**Then a verifying pass that writes nothing:**

5. The payload is streamed and hashed, and must hash to the digest the header claims
6. Every record must belong to a table a snapshot may carry
7. Block info must be complete (exactly `H` records, none twice, none at or above `H`),
   monotonic in cumulative difficulty, generated coins and transaction count, and must
   match every compiled-in checkpoint below `H`
8. The record counts and the final transaction count must match the header

Nothing reaches the database until all of that passes. A digest can only be confirmed
once the whole payload has been read, so writing as it went would mean a failed check
leaves a half imported chain that looks whole - which is the failure this design exists to
prevent. The second read costs about a minute of decompression.

**Then a writing pass:**

9. Key images and key output info go through `SstFileWriter` and `IngestExternalFile` -
   one write of about the file's size, rather than the 25 to 30 GB the normal write path
   amplifies to. This is the difference between half an hour and several hours on a laptop
10. Block info goes through the ordinary write path, where `insertCachedBlock` also writes
    the block hash index and the empty transaction hash list a syncing lite node writes for
    every block below its height - so the result matches a synced database rather than
    merely working. Block 0 is skipped: genesis is already there, in full
11. `"b"` and `"h"` are derived from the key outputs and cross-checked against the header's
    amount count, because an off-by-one there silently corrupts the global index of every
    output the node writes afterwards
12. `TRANSACTIONS_COUNT_KEY` is taken from the header

After the restart the node is an ordinary lite node that happens to already hold its index
region. It validates, relays and mines exactly as one that synced the region itself, and
every wallet behaviour in [LITENODE.md](LITENODE.md) applies unchanged.

### One place it is not byte identical

`"h"` holds `amountId -> amount` records, and a syncing node assigns those ids in the order
it first meets each amount - an order an importer cannot reproduce without replaying the
chain. The importer assigns them in ascending amount order instead. Nothing reads those
records back; only the count stored beside them is ever read. So the difference is
invisible, but a test comparing an imported database against a synced one byte for byte
should expect it.

## What this does not change

- **`H` still cannot be changed in place.** Importing at a different `H` means a different
  file and an empty database
- **A wallet older than `H` still cannot be scanned here.** A snapshot moves where the
  chain comes from, not what a lite node holds
- **Explorer mode is still refused.** The dropped tables are still dropped
- **Mobile still cannot run a node.** 5 GB and Android's background limits, unchanged

## Testing

One test decides whether any of this is real:

**Two nodes exporting at the same `H` must produce the same payload digest.** Without it
the digest is decorative, nobody can reproduce a published snapshot, and the trust model
quietly reduces to "whoever sent you the file".

### An import, measured

A full round trip on mainnet at `H` = 4,000,000, importing the 5.20 GiB snapshot into an
empty data directory and then starting the node on it:

| | |
|---|---|
| verify pass (writes nothing) | 411 s |
| write pass | ~33 min |
| **total** | **2,379 s, about 40 minutes** |
| resulting database | **5.77 GiB** |
| a natively synced lite node at the same height | 5.80 GB |
| peak memory | ~160 MB |

The two sizes agreeing is the useful part: an imported database and a synced one come out
the same, which is some evidence the import is neither missing records nor duplicating
them. Restarted without the import flag, the node reported `height: 4000000` and synced
forward from there.

The same snapshot imported on Linux took **1,357 s - 22.6 minutes** - and came up at
`height: 4000000` as well. Both nodes reported a chain-wide transaction total of
7,469,434: two imports, different operating systems, different hardware, different
database flags, agreeing on a counter that is restored from the header rather than
recomputed. That is a harder thing to pass by accident than the record counts are.

Against a native lite sync - a day on a server, two to three on a laptop - that is the
whole point of the feature, and it holds.

**Run it with the same `--db-*` flags the node will use afterwards.** The same import
without them took 20 minutes rather than 40 and produced 6.71 GiB instead of 5.77. Ingested
files land at the bottommost level and nothing rewrites them, so the difference is
permanent until a `compact_db force`, which costs a full rewrite. The extra twenty minutes
of ZSTD is the cheaper end of that trade.

### What has been run

**A full node and a lite node at `H` = 4,000,000 produced the same digest**
(`4601d802...f94fe09e`, 148,728,732 records). That is the check that matters most, because
`KeyOutputInfo.transactionHash` is the one field the two node types store differently - a
lite node zeroes it below the lite height, a full node keeps the real 32 bytes - and the
exporter zeroes it unconditionally so they agree. Across 78.5 million key outputs, and
against a 38 GB database on one side and a 9 GB one on the other, they did.

It does not establish reproduction by another party on another build. Both runs used the
same binary, so a fault in the exporter itself would reproduce faithfully in both. What it
rules out is normalisation and filtering error, which were the plausible failures.

### What is still owed

- **A third export, from someone else's build.** Closes the gap above, and is what a
  published digest should rest on.
- **An imported node against a natively synced one** at the same `H`, compared under
  `--snapshot-stats`. The two agree on total size to within half a percent, but that is not
  the same as agreeing table by table. Expect one difference and only one: the
  `amountId -> amount` records under `"h"`, whose ids an importer assigns in ascending
  amount order rather than in the order a syncing node first met each amount. Nothing reads
  them back.
- **A wallet against an imported node.** Everything in [LITENODE.md](LITENODE.md) about
  scan floors and stalled syncs should hold unchanged, because after the restart this is an
  ordinary lite node - but no wallet has been pointed at one.
- **Refusal paths.** A truncated file, a bit-flipped one, one whose digest is not in the
  table, one for the wrong height, one for the wrong chain, and one aimed at a database
  that already holds a chain - each refused without writing anything.
- **Any of this in CI.** None of it is automated. The reproducibility check in particular
  wants to run on every release, because it is the one that stops being true silently.
