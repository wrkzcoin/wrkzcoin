# Solo mining

`Wrkzd` can hand work straight to a mining program, so solo mining needs no
pool and no bridge. Start the node with a stratum port and point a miner at it.

```
Wrkzd --stratum-bind-port 17858
```

```
xmrig -o 127.0.0.1:17858 -a cn/upx2 -u <your Wrkz address>
```

The address you give the miner is the address the block reward is paid to. The
node validates it at login and refuses anything it cannot pay.

## Why not `xmrig --daemon`

xmrig's own solo mode reads the block template itself, and it expects Monero's
layout: major version, minor version, timestamp, previous hash, nonce. Ours is
Forknote lineage, where the outer header is major, minor and previous hash, and
the real timestamp and nonce live inside a merge-mining parent block after it.
xmrig misreads the first bytes of the previous hash as a timestamp and gives up
with `Invalid block template received from daemon.` No daemon setting changes
that; the blob shape is consensus.

Stratum sidesteps it entirely. What a miner hashes is the parent block's
hashing serialization, which is an ordinary 76-byte CryptoNote blob with a
four-byte nonce at offset 39 - exactly where every stratum miner already writes
one. The node does the block assembly, so the miner never sees a block at all.

## Options

| Option | Default | Meaning |
| --- | --- | --- |
| `--stratum-bind-port` | `0` | Port to listen on. `0` leaves the server off. |
| `--stratum-bind-ip` | `127.0.0.1` | Interface to listen on. Set `0.0.0.0` to accept rigs from the network. |
| `--stratum-share-difficulty` | `0` | Difficulty given to miners. `0` uses the network difficulty. |
| `--stratum-max-connections` | `32` | Miners allowed on at once. |

There is no password and no account: anyone who can reach the port can mine to
their own address using this node. That is why it listens on loopback unless
you say otherwise. Opening it to the internet lets strangers spend your node's
CPU building templates, so put it behind a firewall or a VPN if it has to leave
the machine.

### Share difficulty

By default a miner is given the network difficulty, so it reports only when it
has actually found a block. That is what solo mining means, but it also means a
slow machine can run for days showing nothing, with no way to tell working from
broken.

Setting `--stratum-share-difficulty` to something lower makes the miner report
shares it can actually find. Those are counted and logged; a block still needs
the real network difficulty, which the node checks itself before submitting. Use
it to confirm a rig is hashing correctly, then take it back off.

## What the node logs

```
Stratum server listening on 127.0.0.1:17858
Stratum miner connected from 127.0.0.1 (XMRig/6.26.0) mining to Wrkz...
Stratum miner 127.0.0.1 found block 7e6a9e92... at height 1402731
```

A miner running the wrong algorithm is told so rather than left to look
unlucky: the node compares the hash the miner reports against its own and
answers `Invalid result`, naming the algorithm the current block version wants.

While the node is still synchronizing it refuses to hand out work, saying how
far behind it is. Mining on a chain the network has moved past would only
produce orphans.

## Writing your own pool

If you are driving `getblocktemplate` yourself rather than using the stratum
server, note that the template is not ready to hash as returned: the node leaves
the parent block's merge-mining tag as a placeholder and expects the miner to
seal it. Skip that and every block you submit is rejected as "Proof of work is
too weak", however good the work was. The daemon RPC docs cover it under
[JSON-RPC methods](docs/docs/daemon-rpc/json-rpc.md#before-you-mine-the-template),
and `adjustMergeMiningTag()` in `src/miner/MinerManager.cpp` is the reference.

## The bundled miner

`miner` still works and needs no stratum port, but it is a plain reference
implementation with no assembly path:

```
miner --daemon-address 127.0.0.1:17856 --address <your Wrkz address> --threads 8
```
