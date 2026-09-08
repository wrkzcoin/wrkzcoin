# WrkzCoin

[![Discord](https://img.shields.io/discord/460755304863498250?label=WrkzCoin%20Discord)](https://chat.wrkz.work) [![GitHub All Releases](https://img.shields.io/github/downloads/wrkzcoin/wrkzcoin/total.svg?include_prereleases)](https://latest.wrkz.work/) [![GitHub contributors](https://img.shields.io/github/contributors-anon/wrkzcoin/wrkzcoin?label=Contributors)](https://github.com/wrkzcoin/wrkzcoin/graphs/contributors) [![GitHub issues](https://img.shields.io/github/issues/wrkzcoin/wrkzcoin?label=Issues)](https://github.com/wrkzcoin/wrkzcoin/issues) ![GitHub stars](https://img.shields.io/github/stars/wrkzcoin/wrkzcoin?label=Github%20Stars)
![Version](https://img.shields.io/github/v/release/wrkzcoin/wrkzcoin?include_prereleases)

<!-- Table of Contents -->

<summary><h2 style="display: inline-block">Table of Contents</h2></summary>
<ul>
    <li><a href="#documentation">Documentation</a></li>
    <li><a href="#installing-wrkzcoin">Installing WrkzCoin</a></li>
    <li><a href="#node-system-requirements">Node System Requirements</a></li>
    <li><a href="#build-from-source">Build from Source</a></li>
    <li><a href="#getting-started-fast">Getting Started Fast</a></li>
    <li><a href="#lite-node--snapshots">Lite Node &amp; Snapshots</a></li>
    <li><a href="#solo-mining">Solo Mining</a></li>
    <li><a href="#transaction-pow-server">Transaction PoW Server</a></li>
    <li><a href="#network-monitor">Network Monitor</a></li>
    <li><a href="#daemon-db-compaction">Daemon DB Compaction</a></li>
    <li><a href="#daemon-zmq-quick-test">Daemon ZMQ (Quick Test)</a></li>
    <li><a href="#notification-hooks-monero-style---block-notify----tx-notify">Notification Hooks</a></li>
    <li><a href="#a-note-for-contributing-developers">A note for contributing developers</a></li>
    <li><a href="#contributing-projects">Contributing Projects</a></li>
    <li><a href="#community">Community</a></li>
</ul>

### Documentation

Full documentation - daemon RPC, wallet API, wallet guides and operator guides - is
published at **<https://docs.wrkz.work/>**. It tracks the `development` branch, and the
sources live in [docs/](docs/).

| I want to&nbsp;... | Page |
|---|---|
| Run a node | [Running a Node](https://docs.wrkz.work/guides/running-daemon/) |
| Look up a daemon flag | [Configuration Reference](https://docs.wrkz.work/guides/daemon-configuration/) |
| Run a node on a small disk | [Lite Nodes](https://docs.wrkz.work/guides/lite-node/) and [Lite Node Snapshots](https://docs.wrkz.work/guides/lite-snapshots/) |
| Open a node to the world safely | [Networking](https://docs.wrkz.work/guides/networking/) and [Security Hardening](https://docs.wrkz.work/guides/security-hardening/) |
| Mine | [Solo Mining](https://docs.wrkz.work/guides/solo-mining/) |
| Build on the daemon RPC | [Daemon RPC](https://docs.wrkz.work/daemon-rpc/overview/) |
| Take payments | [Wallet API](https://docs.wrkz.work/wallet-api/overview/) |
| Use a wallet | [Wallet CLI](https://docs.wrkz.work/guides/wallet-cli/), or the [desktop, mobile and web wallets](https://docs.wrkz.work/guides/wallet-apps/) |
| Check a fork height or ring size | [Network Parameters](https://docs.wrkz.work/guides/network-parameters/) |
| Look up a wallet error code | [Wallet Error Codes](https://docs.wrkz.work/guides/error-codes/) |
| See what changed | [Changelog](https://docs.wrkz.work/changelog/daemon/) |

Every page is published as plain markdown as well as HTML, so an agent or a script can
read the docs without parsing the rendered site:
[`/llms.txt`](https://docs.wrkz.work/llms.txt) indexes every page,
[`/llms-full.txt`](https://docs.wrkz.work/llms-full.txt) is the whole set concatenated,
and appending `.md` to any page URL returns that page's source.

To build the docs locally:

```
pip install -r docs/requirements.txt
mkdocs serve -f docs/mkdocs.yml
```

When a route, a method handler or a command-line flag changes, update the page that names
it in the same change.

### Installing WrkzCoin

To use WrkzCoin, you'll need a way to connect to the network, and a wallet to hold your funds. This software includes those things for you, you can compile it yourself, or you can download the ones that we have compiled for you.

**Click here to download: https://latest.wrkz.work**

### Node System Requirements

Minimum requirements to run a **Wrkzd** daemon node:

| Requirement | Lite Node | Pruned Node | Full Node |
|---|---|---|---|
| **Internet** | Stable broadband | Stable broadband | Stable broadband |
| **CPU** | 1 core, 1.0 GHz | 1 core, 1.0 GHz | 1 core, 1.5 GHz |
| **RAM** | 2 GB | 2 GB | 2 GB |
| **Free Disk Space** | 15 GB SSD/NVMe | 30 GB SSD/NVMe | 50 GB SSD/NVMe |

- A **lite node** (`--lite --lite-height H`) keeps raw block data only from height `H` upward, and below `H` only the indexes later blocks actually need. It validates, mines and relays exactly like a full node, but it cannot serve a wallet older than `H`, and it cannot be combined with `--prune`. See [Lite Node & Snapshots](#lite-node--snapshots).
- A **pruned node** (`--prune`) discards raw block and transaction binary data for blocks older than the configured depth (default: 7 days ≈ 10,080 blocks). Block metadata, key images, and transaction indexes are retained for the full chain, so consensus and wallet sync remain functional.
- A **full node** retains raw block and transaction data for the entire blockchain history. Run one if other people connect to your node.
- **SSD or NVMe storage is required.** HDDs are too slow for RocksDB random I/O and will cause severe sync delays or daemon instability.
- Disk usage grows over time as the chain advances — provision extra headroom.
- RAM shown covers the daemon alone; allow more if also running `wrkz-service` or a miner on the same machine.

#### What a Pruned Node Removes vs. Retains

| Data | Pruned Node | Full Node |
|---|---|---|
| Raw block binary data (old blocks) | Removed | Kept |
| Raw transaction binary data (old blocks) | Removed | Kept |
| Raw block/transaction data (recent blocks, within prune depth) | Kept | Kept |
| Block metadata (hash, height, difficulty, timestamp) | Kept for all | Kept for all |
| Transaction indexes (global output indexes) | Kept for all | Kept for all |
| Key images (spent output tracking) | Kept for all | Kept for all |
| Payment ID indexes | Kept for all | Kept for all |

The prune depth controls how many recent blocks keep their raw data. The default is 7 days worth of blocks (`--prune-depth` can increase this). Use `--prune-depth N` to retain more history.

> **Building from source** requires at least **4 GB RAM** for the compiler (RocksDB and C++20 templates are memory-intensive at compile time).

### Build from Source

Build instructions are maintained in:

- [COMPILE.md](COMPILE.md), also published as [Building from Source](https://docs.wrkz.work/guides/building/)
- [Cross-platform build guide](scripts/cross-platform/README.md)
- [Docker release builds](scripts/docker/README.md) - one command builds and packages the Linux, Windows and Android CLI sets

### Getting Started Fast

Everyone starts somewhere. If you're new or returning, you'll probably want to get in sync with the network so you can use your funds. Syncing from your own node is faster than syncing from a remote node. Here are some handy links to get you there as soon as possible.

-   **Use checkpoints** - Checkpoints help your node sync faster, [WrkzCoin Checkpoints](https://checkpoints.wrkz.work/) or via [Direct link](https://checkpoints.wrkz.work/checkpoints.csv), or generate your own from a synced daemon with [`scripts/checkpoints/gen_checkpoints.sh`](scripts/checkpoints/gen_checkpoints.sh)
-   **Run a lite node** - If the node only ever serves your own wallet, `--lite` cuts the database to a fraction of a full node's, and a snapshot cuts the first sync from days to minutes. See [Lite Node & Snapshots](#lite-node--snapshots)
-   **Backup your keys** - You can generate a wallet right inside the software, or use [this paper wallet generator](https://paperwallet.wrkz.work)

### Lite Node & Snapshots

A **lite node** stores full block data only from a height you choose, `H`, upward. Below
`H` it keeps just the indexes later blocks actually need - key outputs, spent key images,
per-amount output counts and block headers - and never writes the block bodies,
transaction records or payment ID index. Measured on mainnet at `H` = 4,000,000 with the
chain at 4.2M blocks, that is **~ 6 GB on disk against a full node's ~38 GB**, and the
node still syncs, mines, relays and validates exactly like a full one.

```
Wrkzd --lite --lite-height 4000000
```

**Choosing `H` is the whole decision.** Set it at or below your own wallet's creation
height. If `H` is above it, the wallet's scan is clamped up to `H`, the transactions in
between are never seen, and the balance simply comes out too low:

```
H = min(earliest wallet scan height, current network top - 20160)
```

then round down another 10-20k for margin. Two things to accept before starting: `H` is
recorded in the database and **cannot be changed without a resync**, and a lite node
**cannot serve a wallet older than its `H`** - restoring an older seed later needs a full
node, or a fresh lite node at a lower height.

Lite mode is for a node serving wallets you control - your own, a service hot wallet, a
mining node. **Do not run a public or seed node in lite mode**: it cannot help new nodes
bootstrap, and you cannot know how old your users' wallets are. Use `--prune` on a full
node instead when disk is tight. `--lite` is refused together with `--prune` and with
`--daemon-mode explorer`.

#### Snapshots

Lite mode saves disk but not time - the node still downloads every block from genesis to
build those indexes, which is a day or more. A **snapshot** ships that index region as a
file instead, turning the first sync into a file transfer plus the remaining tail:

```
Wrkzd --lite --lite-height 4000000 --import-lite-snapshot wrkz-lite-base-h4000000-v1.litesnap
Wrkzd --lite --lite-height 4000000
```

The first run imports and exits; the second is an ordinary lite node that syncs the rest
from the network. `Wrkzd --snapshot-info <file>` prints what a file carries without
importing it.

A snapshot carries chain state that its recipient cannot verify for itself, so **only a
file whose digest is compiled into the binary can be imported** - there is no force flag.
The first blessed height is **4,000,000**. Running a lite node at any other `H` is still
fine; you just sync it yourself. To produce a snapshot from a synced node (full or lite),
use the `snapshot_export` console command.

Long form in [LITENODE.md](LITENODE.md) and [LITESNAPSHOT.md](LITESNAPSHOT.md); shorter,
cross-linked versions at [Lite Nodes](https://docs.wrkz.work/guides/lite-node/) and
[Lite Node Snapshots](https://docs.wrkz.work/guides/lite-snapshots/).

### Solo Mining

- The daemon serves stratum directly (`--stratum-bind-port`), so a stock miner points straight at the node with no pool and no bridge. [MINING.md](MINING.md) covers the flags, why `xmrig --daemon` cannot mine here, and what the node logs; also published as [Solo Mining](https://docs.wrkz.work/guides/solo-mining/).

### Transaction PoW Server

- Every transaction carries a small proof of work. `wrkz-txpow-server` computes it on behalf of wallets that would rather not spend their own CPU on it, such as phones and browsers; the desktop, mobile and web wallets have a setting for it and fall back to their own CPU when the server does not answer. [TXPOWSERVER.md](TXPOWSERVER.md) covers running it and the protocol; also published as [Transaction PoW Server](https://docs.wrkz.work/guides/txpow-server/).

### Network Monitor

- `wrkz-netmon` walks the P2P network and serves a dashboard of what it finds: which nodes are reachable, what protocol version they run, how far behind the tip they are, whether the network agrees on one chain, and where the nodes sit. It needs no blockchain and no local daemon, and it serves its own dashboard, so one command is enough to try it. [NETMON.md](NETMON.md) covers running it, the DB-IP location files and an nginx layout; also published as [Network Monitor](https://docs.wrkz.work/guides/netmon/).

### Daemon DB Compaction

- The daemon can compact RocksDB in background while syncing.
- Boot-time background compaction is enabled by default.
- Disable boot compaction with `--skip-boot-compaction`.
- Check status from daemon console: `compact_db status`.
- Manual controls: `compact_db start` and `compact_db wait`.
- Every console command is listed in [Console Commands](https://docs.wrkz.work/guides/console-commands/), and every flag in the [Configuration Reference](https://docs.wrkz.work/guides/daemon-configuration/).

### Daemon ZMQ (Quick Test)

- Default daemon ZMQ PUB endpoint: `tcp://127.0.0.1:17857`
- Start daemon with explicit endpoint:
  - `Wrkzd --zmq-pub tcp://127.0.0.1:17857`
- Test subscriber script:
  - `pip install pyzmq`
  - `python scripts/zmq_sub_test.py --endpoint tcp://127.0.0.1:17857 --topics hashblock chain_main`

### Notification Hooks (Monero-style `--block-notify` / `--tx-notify`)

- Daemon: `--block-notify`, `--reorg-notify`, `--tx-notify`; wallets: `--tx-notify` (`wrkz-service` also `--tx-confirmed-notify`).
- Each takes an `http(s)://` URL (JSON POST) or a command template (`%s` hash, `%h` height, ...), e.g.
  - `Wrkzd --block-notify https://example.com/hooks/block`
  - `Wrkzd --block-notify "curl -s -X POST https://example.com/hook -d hash=%s -d height=%h"`
- Delivery is asynchronous (own worker thread, bounded queue, 10 s timeout) and never blocks the node. Details in [NETWORKING.md](NETWORKING.md#notification-hooks---block-notify---tx-notify-), also published as [Notification Hooks](https://docs.wrkz.work/guides/notify-hooks/).

Build requirements and static/portable notes are in [COMPILE.md](COMPILE.md).

### A note for contributing developers

Hello, and thank you for helping us! Our work makes use of many brilliant projects from other communities who contributed their code which helped us get to where we are now. To make sure we're always doing things the right way, we try to make sure we get the proper license header in every file we modify. By the terms of this project's license, any open source project may use our software, but the licenses may only be appended to, not altered. 

See `src/config/CryptoNoteConfig.h` for an example.

```
// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2020, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin Developers
//
// Please see the included LICENSE file for more information.
```

### Contributing Projects

[![cryptonote](https://user-images.githubusercontent.com/34389545/72484723-d84bf700-37ca-11ea-812e-e24cd7bf9fca.png)](https://cryptonote.org/)[![bytecoin](https://user-images.githubusercontent.com/34389545/72484467-ef3e1980-37c9-11ea-903d-3d1266e9c4c2.png)](https://bytecoin.org/)[![monero](https://user-images.githubusercontent.com/34389545/72484448-e0576700-37c9-11ea-934a-15a7d9231709.png)](https://web.getmonero.org/)[![forknote](https://user-images.githubusercontent.com/34389545/72484430-d59cd200-37c9-11ea-8529-e06ae2426dca.png)](http://forknote.net/)[![turtlecoin](https://user-images.githubusercontent.com/34389545/72484404-c0c03e80-37c9-11ea-8754-0b5a8e797965.png)](https://turtlecoin.lol)

### Community

* Discord: <https://discord.com/invite/KN8PjyuvG4>
* X: <https://x.com/wrkzcoin>
* GitHub: <https://github.com/wrkzcoin>
* Documentation: <https://docs.wrkz.work/>

### Stars

[![Star History Chart](https://api.star-history.com/svg?repos=wrkzcoin/wrkzcoin&type=Date)](https://star-history.com/#wrkzcoin/wrkzcoin&Date)
