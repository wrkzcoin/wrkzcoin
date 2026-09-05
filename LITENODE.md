# Lite Node

> A shorter, cross-linked version of this page is published at
> <https://docs.wrkz.work/guides/lite-node/>. This file stays the long form.

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

## Who should run a lite node

| | |
|---|---|
| **A user**, own node for own wallet | **Yes.** This is what lite mode is for. |
| **An operator**, public node others connect to | **No — run a full node**, with `--prune` if disk is tight. |
| **An operator**, private or service node, wallets you control | **Yes**, good fit. |

### If you are a user: choosing H is the whole decision

**Set `H` at or below your wallet's creation height.**

If `H` is above it, the wallet's scan is clamped up to `H` and it will never see
transactions between its creation height and `H`. The balance simply comes out too
low. The wallet logs a warning and `/info` reports `lite_start_height`, but neither
gets read when the number looks plausible.

```
H = min(earliest wallet scan height, current network top - 20160)
```

then round **down** by another 10-20k for margin. Err low: too low costs some disk,
too high costs transactions you cannot see.

Two things to accept before you start:

- **`H` cannot be changed without deleting the data directory.** The daemon refuses
  to start on a mismatch rather than wiping anything, but the choice is one way.
- **A lite node cannot serve a wallet older than its `H`.** Restoring an older seed
  later needs a full node, or a fresh lite node at a lower `H`.

### If you are an operator: usually do not

A public lite node is a worse network citizen and a support liability:

- It cannot serve blocks below `H`, so it cannot help new nodes bootstrap. Peers
  below `H` skip it as a sync source automatically. If everyone ran lite, nothing
  could sync.
- You do not know how old your users' wallets are. Someone restoring a two year old
  seed against your node is clamped to `H` and sees a wrong balance. That reads as
  lost funds, and it reaches you as a support ticket rather than a bug report.

For a public node run a full node and use `--prune` if disk is the problem: pruning
drops old block bodies but keeps everything a wallet rescan needs.

Lite mode suits an operator when **you control which wallets connect**:

- A service node behind a recently created hot wallet - exchange, faucet, payouts
- A **mining node**: mining reads only `alreadyGeneratedCoins` and the last 100 block
  sizes, so history is irrelevant to it
- A validation and relay node adding network security on cheap disk

Never make a lite node a seed node.

### Lite mode and explorer mode are mutually exclusive

`--daemon-mode explorer --lite` is refused at startup. Every explorer endpoint below
`H` - `getBlocksByHeight`, `getBlockDetailsByHash`, `getTransactionDetailsByHash`,
`queryblocksdetailed` - reads the transaction records lite mode drops, so it would
return nothing for those heights rather than report an error. A node that looks like
it works while quietly serving an incomplete chain is worse than one that refuses to
start.

Related, for anything you have built on the RPC: `/get_global_indexes_for_range`
**refuses** below `H` rather than narrowing the range, so historical range queries
break rather than quietly returning a wrong answer.

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
| `"b"` | `amount` → output count | **yes** | Ring index bounds, global index assignment |
| `"h"` | the list of distinct amounts | **yes** | Enumerating amounts |
| `"6"` | `CachedBlockInfo` | **yes** | Difficulty, emission, timestamps, block sizes |
| `"5"` | block hash → index | **yes** | Chain queries |
| `"4"` | raw block body | no | Where ring signatures live — the bulk of the chain |
| `"a"` | `ExtendedTransactionInfo` | no | Only needed for rescan and explorer |
| `"1"` | block index → tx hashes | written empty | `insertCachedBlock` writes this key for every block; below `H` the list is empty |
| `"0"` | block index → key images | no | Rewind index only; a lite node never rewinds that far |
| `"f"` | payment ID → tx hash | no | Explorer only |
| `"e"`, `"g"` | timestamp indexes | no | Answer "which height was this date" for scans that cannot start below `H` anyway |

`"b"` used to hold a second kind of record beside the count: a `PackedOutIndex`
per output, keyed by `(amount, globalIndex)` exactly as `"j"` is. The two were 1:1
— 78.7M records against 78.5M, the difference being one count record per distinct
amount — and only two callers ever read the `PackedOutIndex`, both wanting a
single field from it. That field, the block an output was created in, now lives in
`KeyOutputInfo`, and the per-output half of `"b"` is gone.

**This does not affect global indexes.** An output's global index is the *key* of
these records, not their value, and `"j"` keeps it. The numbering still comes from
the per-amount counter that remains under `"b"`. What a wallet receives as
`globalIndex` — inline in `/getwalletsyncdata`, or from
`/get_global_indexes_for_range` — is read from `ExtendedTransactionInfo` in the
`"a"` table, and never came from `"b"` at all.

### `KeyOutputInfo.transactionHash` is zeroed below `H`

`"j"` is by far the largest table, and a third of each record is a
`transactionHash` that a lite node can never read. Its only consumer is
`extractKeyOtputReferences`, reached only from `Core::getTransactionDetails` to
say which transaction a ring member came from — an explorer answer, and explorer
mode is refused on a lite node. Ring verification and decoy serving read
`publicKey` and `unlockTime` and nothing else.

Below `H` the field is therefore written as zeroes. That is 32 high-entropy bytes
per key output — on a 4.2M-block chain, ~2.3 GiB of a 9.4 GiB database, and the
part compression cannot touch. Zeroing rather than removing keeps the record
layout and the schema version unchanged, so nothing else in the tree needs to
know, and 78 million identical zero hashes compress to almost nothing.

Above `H` the real hash is written as normal: the region a lite node calls full
really is full.

**Consequence:** if a lite node ever did serve `getTransactionDetails` for a
transaction whose ring members predate `H`, it would report a null hash for those
members rather than fail. It cannot today — explorer mode and lite mode are
mutually exclusive — but any future work that relaxes that has to deal with this
first.

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

**The byte totals are logical, not on-disk.** They are the key and value bytes as
the records deserialise; RocksDB compresses them on the way to disk. The report
prints the directory size and the ratio alongside them, because every decision
about snapshot size was previously being made against a number about three times
too large.

A real mainnet measurement, lite node at `H` = 4,000,000, top block 4,201,153:

| Table | Records | Logical |
|---|---|---|
| key output info `"j"` | 78,534,957 | 15,428.7 MB |
| key output indexes `"b"` | 78,675,036 | 5,999.2 MB |
| spent key images `"7"` | 67,187,899 | 5,190.1 MB |
| block info `"6"` | 4,201,154 | 953.6 MB |
| **snapshot payload** | 228,599,046 | **27,571.6 MB** |
| *whole database, logical* | | *28,492.9 MB* |
| **whole database, on disk** | | **9.4 GiB — 2.96x** |

### Squeezing the database further

Two RocksDB knobs are exposed, both defaulting to RocksDB's own behaviour so
nothing changes unless you ask:

```
--db-compression-dict-bytes <n>   per-SST ZSTD dictionary, 0 = off (default)
--db-block-size <kb>              SST data block size, default 4
```

A dictionary is worth trying on this data — hundreds of millions of small records
that agree on everything but a few high-entropy bytes, where plain block
compression relearns the framing every 4 KiB. Larger blocks give the compressor
more context and cost more decompression per point-lookup miss.

Both apply only to **newly written** SST files, so after changing either, run
`compact_db force` in the console. A plain `compact_db` will not do it: RocksDB
skips the bottommost level unless forced, and on a compacted database that level
is nearly all of it. `force` rewrites everything, so it wants free space of about
the database's size and takes a while.

### What that means for a snapshot format

The compressor has already taken the framing. `KeyOutputInfo` is 74 bytes of
payload in ~206 bytes of stored record because the KV binary format writes field
names into every one — and repeated field names are exactly what ZSTD removes.
So a packed canonical dump does **not** save 3x on top of what you see on disk;
it saves what is left after compression, which is not much.

The floor is the high-entropy payload, which no format and no compressor touches:

| | records | random bytes each | irreducible |
|---|---|---|---|
| key output info — `publicKey` + `transactionHash` | 78.53M | 64 | 4.68 GiB |
| spent key images — the image itself | 67.19M | 32 | 2.00 GiB |
| block hashes — block info + hash index | 4.20M | 32 x2 | 0.25 GiB |
| everything else | | | ~0.3 GiB |
| **floor** | | | **~7.2 GiB** |

Against 9.4 GiB actually on disk. RocksDB with ZSTD is already within about 25%
of the entropy floor, so a bespoke snapshot format would buy roughly 20% over a
tarball of a compacted `DB` directory — in exchange for a serialiser, a verifier
and a new trust surface. **The lever worth pulling is dropping fields, not
framing**: see the `transactionHash` note under "What is retained" below.

Do not extrapolate a measurement at one height to another: early blocks are dense
in denominated outputs and are not representative. Measure on a synced node.

## Running one

**Expect a full download.** Lite mode saves disk, not bandwidth: every block from 0
is still fetched, the indexes are built from it, and only the bulky parts are left
unwritten. Plan for hours, not minutes. Roughly 4,000 blocks/min on a modest box at
low heights, slower through denser later blocks.

**Startup refuses rather than repairs.** Every mismatch between the flags and what
the database was built as exits without touching it. Deleting the data directory is
always left to you - a chain dropped because of a forgotten flag would be the worst
possible reading of an operator's intent. So a daemon that will not start is telling
you something, and the fix is never to add a force flag.

**The 14 day rule is checked at handshake time**, not at startup, because the
network's height is not knowable before then. If `H` is too close to the tip the
daemon exits and names the highest value the network currently allows.

The check is deliberately hard to trigger, because the height it reads is a number
a peer chose and the failing branch kills the daemon. Two things guard it. Once
this node's own chain has reached `H + MIN_LITE_FULL_BLOCK_DEPTH` the question is
settled from local data and no peer is consulted at all — which is every restart
of a synced node. Before that, what is weighed is the *tallest* chain claimed
across several peers, so a peer reporting a short chain cannot drag the verdict
down and no single peer can produce it.

**Monitoring**

- `/info` reports `lite` and `lite_start_height`
- `status` shows `Lite Node`, and on a lite node `Full Block Data From`. While the
  node's own tip is still below `H` it also shows `Lite Sync Stage: Index only`,
  which is why `Block Version` reads `v0` for that part of the sync
- `print_cn` shows which peers are pruned

**Things that will refuse to run**

| Combination | Result |
|---|---|
| `--lite` without `--lite-height` | refused - there is no safe default |
| `--lite --prune` | refused - mutually exclusive |
| `--lite --daemon-mode explorer` | refused - explorer needs records lite drops |
| `--lite` on a database synced in full | refused - delete the data directory yourself |
| no `--lite` on a lite database | refused - it has no block bodies to serve |
| `--lite-height H'` on a database built at `H` | refused - `H` cannot be changed |
| `--rewind-to-height` below `H` | refused - those blocks cannot be undone |
| a database built by an older daemon, after a storage format change | refused - restart with `--resync`, which is the only thing that deletes a chain |

## Changing the lite height

`H` cannot be changed in place, but a database can be rebuilt at a different one:

```
Wrkzd --resync --lite --lite-height <new H>
```

`--resync` deletes the chain and the peer state, and the rebuild then starts at the
new height. This is the supported way to move `H`, and the reason changing
`--lite-height` on its own is refused: the destructive step has to be asked for
rather than inferred.

For an **embedded daemon**, prefer restarting the process with these flags over
adding a reset call to the RPC. Rewind and resync are deliberately startup-only:
an operation that needs a restart cannot be triggered by a stray request, and the
daemon's RPC has no authentication. A host application already controls the daemon
process, so it does not need the extra surface. Restarting also makes any connected
wallet reconnect and re-read `lite_start_height`, which it only reads when it starts
a scan.

Note this is a full resync. Raising `H` on an existing database - turning blocks
between the old and new height from full into index-only, which is a deletion pass
rather than a download - is not implemented.

## RPC behaviour

- `/info` gains `lite` (bool) and `lite_start_height`.
- `/info` reports `major_version` and `minor_version` as `0` while the node's own
  top block is still below `H`, which is most of a first sync. Reading them needs
  the block body, and there is none down there. Everything else in the response,
  and the `status` console command that reads it, work normally.
- `/getwalletsyncdata` and `/getrawblocks` **clamp** a start height below `lite_start_height`
  up to it, and drop any start timestamp. A wallet that asks from 0 is served from the lite
  height rather than fed an empty response forever.
- `/get_global_indexes_for_range` **refuses** below `lite_start_height`. There is nothing
  sensible to clamp to — a silently narrowed range would read as "these heights hold no
  transactions", which is a different and wrong answer.
- Peers asking for a block below the lite height get it reported in `missed_ids`, so they
  ask elsewhere instead of losing the connection.

## Wallet behaviour

Every wallet built on `walletbackend` — zedwallet++, wallet-api, the C API the desktop
and mobile wallets use, and the wasm web wallet — reads `lite_start_height` from `/info`
and acts on it. Three cases, and only the first is silent:

| The wallet | What happens |
|---|---|
| has scanned nothing yet | Its scan height is floored to `lite_start_height`. A warning is logged; `status` names the floor. |
| has scanned to a height at or above `lite_start_height` | Nothing changes. It carries on. |
| has scanned to a height **below** `lite_start_height` | **Sync stops.** |

That third case is the one worth understanding. The daemon would answer from its lite
height whatever the wallet asked for, so continuing would move the wallet's recorded
position over blocks nobody ever looked at: the wallet would report itself synced while
every transaction in the skipped stretch was missing, and the balance would simply be too
low. There is no error a user would see, which is why it stops instead.

It reports that through `status` in zedwallet++, `isSyncStalledByLiteNode` in wallet-api's
`GET /status`, and the same field in `wallet_get_status_json`. Both `/status` responses
also carry `daemonLiteStartHeight`, zero when the daemon holds the whole chain. Recovery
is to connect a daemon that holds the range, or to reset the wallet to the lite height and
accept that transactions below it cannot be found here.

The same check catches a hole from any source, not just a lite node: if the first block of
a sync response sits above where the wallet expects to continue, it is refused rather than
stored. A pruned daemon, a blockchain cache API or a faulty node produces the same silent
gap, and none of them announce it.

### Rescanning against a lite node

A rescan below `lite_start_height` starts at the lite height instead, which drops every
transaction the wallet already holds from underneath it — permanently, as far as that
daemon is concerned. So a wallet that holds any is refused rather than quietly reset:

- **zedwallet++** names the floor and the number of transactions at stake before the
  confirmation prompt.
- **wallet-api** answers `PUT /reset` with `400` and error `62`.
- **`wallet_reset`** returns `62` (`LITE_NODE_CANNOT_RESCAN_THAT_LOW`) without touching
  the wallet.

A wallet holding nothing below the floor loses nothing, so it is not stopped.

## The wallet apps

`extras/desktop-wallet` and `extras/mobile-wallet` both read `daemonLiteStartHeight`
and `isSyncStalledByLiteNode` out of `wallet_get_status_json`, and neither will let a
lite node be mistaken for a full one.

**Both apps** carry a standing notice naming the lowest block the connected node holds.
It does not go away when the wallet finishes syncing — that is precisely when a balance
missing its older half looks most trustworthy. If the node starts above the wallet's own
start height the notice becomes a warning that the balance reads low; if sync has stopped
on a gap, it becomes an error naming both heights. Settings shows *Serves blocks from*
for whatever node is connected, a height for a lite node and *Full chain* otherwise.

**Rescans are floored at the node's start height in the app**, not just refused by the
backend afterwards. The desktop dialog disables its button below the floor, the mobile
screen prints the floor under the height field, and both map error `62` onto a dialog
offering to rescan from the height the node can actually serve.

To make choosing a start height possible at all, `wallet_get_status_json` gained
`walletSyncStartHeight` and `walletSyncStartTimestamp` — the lowest height any sub wallet
was told to scan from. A synced wallet's block count says nothing about how far back its
funds go, so this is the only number a caller can size a lite node against. One of the
two is zero: a wallet is created from a height or from a timestamp, never both, and a
timestamp only resolves to a height once the first sync response arrives.

### The desktop app can run one for you

Settings → **Local Lite Node** supervises a `Wrkzd` child process on the user's own
machine: it picks its own ports, keeps its chain under the app's support directory,
resumes on the next launch, and adopts a node left behind by a force-quit app rather than
starting a second one on the same database. The wizard defaults the start height to the
wallet's own and demands an explicit acknowledgement to go above it.

The node syncs whether or not the wallet is pointed at it, so the intended flow is to
stay on a remote node for the hours a first sync takes and switch over when it is ready.
Switching is gated on the node reporting itself synced; stopping or deleting it moves the
wallet back to a remote node first. See `extras/desktop-wallet/README.md` for how to ship
the binary and what the loopback RPC does and does not protect.

### The mobile app cannot, yet

Three things stand in the way, and only the first is work:

- The Android profile forces `WRKZ_BUILD_EXECUTABLES=OFF` and RocksDB is only built when
  executables are on, so the daemon has never been compiled for Android here.
- **Size does not follow `H`.** Key output info is written for every block from genesis,
  so a lite node is ~6 GB whatever height it starts at, and it downloads the whole chain
  to build that index. Raising `H` to the tip saves a couple of hundred megabytes.
- A first sync is hours of continuous work against Android's doze and background
  execution limits.

So the mobile app gets the warnings and the node picker, and points at a node the user
runs elsewhere. Revisit if snapshot sync ever exists — it is the download, not the
daemon, that rules this out.
