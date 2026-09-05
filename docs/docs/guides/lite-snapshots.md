# Lite Node Snapshots

A [lite node](lite-node.md) still downloads every block from 0 to its lite
height `H` to build the indexes it keeps below it — hours of work for data that
is then mostly discarded. A snapshot is that index-only region as a portable
file: export it once, import it into an empty database, and the node starts life
already holding it.

The in-repo long form, with the file format, sizing measurements and the test
record, is `LITESNAPSHOT.md`.

## Trust

A snapshot is a chain state somebody else computed. What stands between an
importer and a fabricated one is a **digest compiled into the binary**: the
header names a height and a digest, the whole payload is streamed and hashed
during import, and it must match a digest in `LITE_SNAPSHOT_DIGESTS` for that
height. A file whose digest is not in the binary is refused, whoever produced it.

That is also why importing is restricted to **blessed heights**. The first is
**4,000,000**. New ones are published roughly annually — otherwise the tail
every importer has to sync afterwards grows without bound.

An operator may still run a lite node at any `H` they like by syncing it
themselves. The restriction is on importing, not on lite mode.

## Producing one

A console command, following `compact_db`'s shape: it starts, reports, and can
be cancelled, and the console stays usable while it runs.

```
snapshot_export [start [height] [path] | status | cancel]
```

With no arguments, `start` is assumed. See
[Console Commands](console-commands.md#snapshots).

The default output is
`<parent of the data directory>/wrkz-lite-base-h<H>-v1.litesnap`. There is
deliberately no timestamp in the name: identical content must produce an
identical name, or nobody can tell two copies of the same snapshot apart from
two different ones.

It refuses to start when:

- the chain tip is below `H + 20160`, the same margin lite mode enforces at
  handshake. The region has to be settled beyond any reorg;
- the output file already exists;
- free space is short — checked before the write, not discovered 4 GB in.

**A full node can produce one too**, and should — otherwise making a snapshot
would first require the days-long lite sync it exists to avoid. The export
zeroes `transactionHash` on every key output below `H` and skips the tables lite
mode drops, so a full node and a lite node at the same `H` emit the same bytes.

!!! note "Export from a node at the tip"
    The walk holds a RocksDB snapshot for its duration, which pins SST files. On
    a node still syncing, the data directory grows for as long as the export
    runs.

## Importing one

Two runs. The first imports and exits; the second is an ordinary lite node.

```bash
Wrkzd --lite --lite-height 4000000 --import-lite-snapshot <file>
Wrkzd --lite --lite-height 4000000
```

Check a copy before spending an import on it — this reads the header only, with
no database, chain or core, and prints JSON:

```bash
Wrkzd --snapshot-info <file>
```

**Why it exits rather than carrying straight on.** The import runs after the
core has loaded, because that load writes the genesis block in full — raw block,
base transaction and all — and a snapshot carries none of it. But by then the
core has cached what it believes the chain's shape to be, and the import has
just changed it underneath. Restarting is cheap and settles the question; it is
also what `--import-blockchain` already does.

### What happens, in order

**Refusals first, before the file is read twice:**

1. The database must hold nothing but genesis.
2. The genesis hash in the header must be this network's.
3. The header's height must equal `--lite-height`.
4. The header's digest must appear in `LITE_SNAPSHOT_DIGESTS`.

**Then a verifying pass that writes nothing:**

5. The payload is streamed and hashed, and must hash to the digest the header
   claims.
6. Every record must belong to a table a snapshot may carry.
7. Block info must be complete — exactly `H` records, none twice, none at or
   above `H` — monotonic in cumulative difficulty, generated coins and
   transaction count, and must match every compiled-in checkpoint below `H`.
8. The record counts and the final transaction count must match the header.

Nothing reaches the database until all of that passes. A digest can only be
confirmed once the whole payload has been read, so writing as it went would mean
a failed check leaves a half-imported chain that looks whole — which is the
failure this design exists to prevent. The second read costs about a minute of
decompression.

**Then a writing pass:**

9. Key images and key output info go through `SstFileWriter` and
   `IngestExternalFile` — one write of about the file's size, rather than the 25
   to 30 GB the normal write path amplifies to. This is the difference between
   half an hour and several hours on a laptop.
10. Block info goes through the ordinary write path, so the result matches a
    synced database rather than merely working. Block 0 is skipped: genesis is
    already there, in full.
11. The per-amount output tables are derived from the key outputs and
    cross-checked against the header's amount count, because an off-by-one there
    silently corrupts the global index of every output written afterwards.
12. The transaction count is taken from the header.

After the restart the node is an ordinary lite node that happens to already hold
its index region. It validates, relays and mines exactly as one that synced the
region itself, and every wallet behaviour in [Lite Nodes](lite-node.md) applies
unchanged.

## Sizing

Key outputs are the overwhelming majority of a snapshot. `--snapshot-stats`
reports per-table storage on a synced node, which is how to size one before
producing it.

## The desktop wallet

The desktop wallet can import a snapshot from its setup dialog when it manages a
local node for you, so a user never types either command. See
[Desktop, Mobile and Web Wallets](wallet-apps.md).
