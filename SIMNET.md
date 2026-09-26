# Simnet

A simnet is a private WrkzCoin network: nodes that mine a block in a
millisecond, for trying a wallet, a pool, an exchange integration or a change
to the node without touching mainnet. Its coins are worthless.

It is mainnet with three differences:

- **its own network id** (`"wrkz simnet 0001"`), so a simnet node and a
  mainnet node never finish a handshake;
- **no proof of work**: there is no nonce to search for, and the long hash is
  never even computed. A block template becomes a valid block once its
  merge mining tag is written - the commitment every miner fills in, simnet or
  not;
- **difficulty 1 for every block**, so blocks can come as fast as you like and
  the longest chain wins a reorganisation.

Everything else is mainnet's: the genesis block, the address prefix, the
transaction format, the P2P protocol, the RPC, and the consensus rules of each
height. A simnet chain is a young chain, though. Its blocks are at heights
1, 2, 3 ... and follow the rules of *those* heights, not today's mainnet rules
at 4,200,000: block version 4, and none of the later fee, mixin and
transaction proof of work tiers, nor the 4,300,000 fork.

The network id and the ports are the same as the Rust node's simnet, so the
two implementations can join one simnet.

There are three ways to run one.

## One process: `wrkz-simnet run`

```sh
wrkz-simnet run                      # three nodes in a line, a block every 10 s
wrkz-simnet run --nodes 5 --topology mesh --block-interval 2
```

It prints each node's P2P address, RPC URL and [WebSocket](WEBSOCKET.md) URL
(node 0 on RPC port 27856, node 1 on 27857, and so on; P2P from 27955), mines
60 blocks straight away so the first rewards unlock - a mined reward waits 40
blocks - then one block every `--block-interval` seconds on node 0. Unless
`--mine-to` names an address, it mines to fresh keys and prints them:

```text
Mining to fresh keys:
address:           Wrkz...
private spend key: ...
private view key:  ...
mnemonic seed:     ...
import into a wallet with these keys and scan height 0 (simnet coins only)
```

Import those keys into `wrkz-wallet` or a GUI wallet **with scan height 0**,
point the wallet at `http://127.0.0.1:27856`, and it has coins to send.
`--enable-cors ORIGIN` lets a browser wallet at `ORIGIN` use the nodes.

The chains live in a temporary directory that is removed on exit, unless
`--data-dir` names one to keep. `wrkz-simnet --help` lists every option.

## Separate processes: `Wrkzd --simnet`

`--simnet` turns any `Wrkzd` into a simnet node:

```sh
Wrkzd --simnet --enable-websocket
Wrkzd --simnet --data-dir ./sim2 --p2p-bind-port 27955 --rpc-bind-port 27956 \
    --add-exclusive-node 127.0.0.1:27855 --allow-local-ip
wrkz-simnet mine --daemon http://127.0.0.1:27856 --interval 5
```

With `--simnet` the node uses its own network id, checks no proof of work, has
no checkpoints, no seed nodes and no UPnP, compares heights with its peers
every two seconds rather than every minute, and moves what was left at its
mainnet default:

| | mainnet default | with `--simnet` |
| --- | --- | --- |
| P2P port | 17855 | 27855 |
| RPC port | 17856 | 27856 |
| ZMQ endpoint | `tcp://127.0.0.1:17857` | `tcp://127.0.0.1:27857` |
| data directory | the default one | `simnet` inside it |

On its own, a simnet node counts as synchronized, so its RPC takes
transactions without a peer. `--seed-node` still works, for a simnet spread
over several hosts.

The choice is **permanent for the database**, in both directions: a simnet
data directory refuses to open without `--simnet`, and a mainnet one refuses
to open with it (`src/cryptonotecore/NetworkProfile.cpp`). A mainnet database
carries no record at all, so nothing about an existing node changes. A
`--load-checkpoints` file and `--import-lite-snapshot` are refused with
`--simnet`, since both are mainnet's.

`wrkz-simnet mine` asks for a template, writes its merge mining tag and
submits it with no work behind it. A mainnet node refuses such a block, so
pointing it at the wrong node fails at once. It
retries a node that is not answering yet, every five seconds.

## Docker: `compose.simnet.yml`

`compose.simnet.yml` runs three simnet nodes in a line and a miner, from the
runtime image `scripts/docker/Dockerfile.node` builds out of the Linux
package:

```sh
scripts/docker/build.sh linux
docker build -t wrkzcoin -f scripts/docker/Dockerfile.node \
    --build-arg PACKAGE=wrkzcoin-cli-linux-x86_64-<version>.tar.gz builds
docker compose -f compose.simnet.yml up -d
docker compose -f compose.simnet.yml logs miner     # the keys it mines to
docker compose -f compose.simnet.yml down -v        # stop and wipe
```

The nodes' RPCs, each with `/ws`, are on `127.0.0.1:27856`, `:27866` and
`:27876`. Stopping `node2` cuts `node3` off; starting it again lets `node3`
catch up.

## In a test: `wrkz-simnet test`

`wrkz-simnet test` starts whole nodes inside its own process - the chain, the
pool, the P2P engine, the RPC and its WebSocket stream, assembled as `Wrkzd`
assembles them (`src/simnet/SimnetNode.cpp`) - and checks, end to end:

- relay along a line of nodes;
- the WebSocket stream announcing a block mined two hops away, with
  `?topics=` narrowing it;
- `GET /ws` refusing a misspelt topic, a page from another origin and a plain
  GET;
- a wallet's event-stream watch being woken by a block;
- a node of another network being turned away;
- a partition healing, with the shorter side reorganising onto the longer;
- a simnet database refusing to open as mainnet, and a mainnet one as a
  simnet.

It exits non-zero if any fails. Nodes of a cluster dial only their topology
neighbours, as exclusive nodes, so an address learned from a peer list can
never join two nodes the topology keeps apart.
