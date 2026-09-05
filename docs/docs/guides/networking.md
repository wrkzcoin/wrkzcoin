# Networking

How `Wrkzd` binds, who it talks to, and how it finds peers. Every flag named
here is listed with its default in the
[Configuration Reference](daemon-configuration.md). The in-repo long form is
`NETWORKING.md`.

## P2P

The daemon always binds an IPv4 P2P listener on `--p2p-bind-ip` (default
`0.0.0.0`) and `--p2p-bind-port` (default `17855`).

IPv6 is opt-in. Setting `--p2p-bind-ipv6-address` binds a **separate** IPv6
listener in addition to the IPv4 one, using `IPV6_V6ONLY=0`, so it also accepts
IPv4-mapped connections (`::ffff:1.2.3.4`) transparently.

```bash
# All IPv6 interfaces, same port as IPv4
Wrkzd --p2p-bind-ipv6-address ::

# A specific address on its own port
Wrkzd --p2p-bind-ipv6-address 2001:db8::1 --p2p-bind-port-ipv6 17856
```

### Protocol version

Nodes advertise **P2P version 19** as of 0.4.8. The minimum accepted version is
16, so older peers still connect; a node below 16 is refused. IPv6 peer exchange
(the `local_peerlist6` field in handshakes and timed syncs) is gated on version
19 — peers below it never see it and ignore it if they do.

### What IPv6 does and does not cover

Working: IPv6 inbound and outbound connections, IPv6 peer-list exchange, IPv6
bans, A+AAAA DNS seed resolution, IPv6 white-list promotion after a successful
outbound handshake, and `print_cn` showing `[2001:db8::1]:17855` for both
directions.

Not covered:

- A peer connecting **inbound** over pure IPv6 is tracked for display but is not
  promoted to the white list — the back-ping uses the IPv4 `NetworkAddress`
  struct.
- IPv6 duplicate-connection detection deduplicates by peer ID, not by address.
  Duplicates fail cleanly at handshake instead.
- `--add-peer`, `--add-priority-node` and `--add-exclusive-node` take IPv4
  literals only.

## RPC binding

`--rpc-bind-ip` (default `127.0.0.1`) and `--rpc-bind-port` (default `17856`).
Setting both `--rpc-use-ipv6` and `--rpc-bind-ipv6-address` starts a **second**
RPC server on the IPv6 address, on the same port. An IPv6 bind failure is
non-fatal: the daemon warns and carries on with IPv4 only.

```bash
Wrkzd --rpc-bind-ipv6-address :: --rpc-use-ipv6
```

RPC can also be served over a local socket — see
[Local IPC and Console](ipc-and-console.md).

## Seed nodes and peer discovery

Static seed nodes and DNS seed hostnames are compiled in
(`SEED_NODES` / `DNS_SEED_NODES` in `src/config/CryptoNoteConfig.h`).
Hostnames resolve to **both A and AAAA records**; IPv4 and IPv6 results go into
separate seed lists and the daemon bootstraps from whichever answers. Publishing
an IPv6 seed needs only an AAAA record on the seed hostname — no daemon change.
`--seed-node` entries join the same list.

A seed is only ever asked for its peer list, and the connection is closed again
afterwards. The daemon asks in two situations:

- **Nothing known yet** — the white lists are empty (first start, or after
  `--p2p-reset-peerstate`).
- **Stuck** — a connection-maker round could not dial anybody new *and* fewer
  than 3 outgoing connections are up. This is what recovers a node whose saved
  peer list has gone stale after a long time offline.

Both share one 5-minute rate limit, so seeds that are down get one walk per
interval rather than one per round. Seed hostnames are re-resolved on a helper
thread every hour, or at the next seed round while the lookup has produced
nothing (DNS not up yet at boot). A failed lookup keeps the addresses already
resolved.

### Keeping the peer lists honest

- An address that refused or timed out is left alone for 10 minutes rather than
  retried every round.
- Once a minute one random gray-list peer is dialled for its peer list: if it
  answers it moves to the white list, if not it is dropped.
- A peer list received in a handshake or timed sync is cut to 250 entries, the
  same number the daemon sends.
- After 2 minutes with no connection at all the daemon logs a WARNING naming the
  known-peer and seed counts. `/info` reports `seed_nodes_count` and
  `last_seed_bootstrap` (unix time, `0` = never).

## Bans

The in-memory ban list holds IPv4 and IPv6 addresses in separate lists.

```
ban list                    # everything currently banned
ban add <ip> [seconds]      # default 900
ban delete <ip>
```

IPv6 addresses go in standard notation; brackets are optional on the console.

```
ban add 2001:db8::1 3600
ban add ::ffff:1.2.3.4 900
```

An IPv4 ban covers both the IPv4 listener and IPv4-mapped clients arriving on
the IPv6 listener.

## ZMQ publisher

`--zmq-pub` (default `tcp://127.0.0.1:17857`) publishes new block and
transaction events for downstream services — explorers, wallets, anything that
would otherwise poll. An empty string, or `--no-zmq`, turns it off. The endpoint
goes straight to libzmq, so `ipc://` endpoints work:

```bash
Wrkzd --zmq-pub ipc:///run/wrkz/wrkzd.zmq
```

Messages are sent as **two ZMQ frames**: the topic, then a JSON payload.
Subscribe to a topic by its exact name, or to an empty prefix for everything.

| Topic | Fires on | Payload |
| --- | --- | --- |
| `hashblock` | Every new main-chain block | `{"height":N,"hash":"..."}` |
| `chain_main` | The same block, with its transaction list, for prefetching | `{"height":N,"hash":"...","transaction_hashes":["..."]}` |
| `hashblock_alt` | A block added to an alternative chain | `{"height":N,"hash":"..."}` |
| `chainswitch` | A reorganisation | `{"common_root_height":N,"hashes":["..."]}` |
| `txpool_add` | Transactions entering the pool | `{"hashes":["..."]}` |
| `txpool_del` | Transactions leaving the pool | `{"hashes":["..."],"reason":"InBlock\|Outdated\|NotActual"}` |

`transaction_hashes` in `chain_main` lists the coinbase first, then the block's
other transactions.

!!! note "Two things to know before you build on it"
    - **A lite node publishes no `chain_main` for blocks below its lite
      height.** The transaction list needs the block body, and there is none
      down there. `hashblock` still fires for every block.
    - **Delivery is lossy by design.** The publisher sends non-blocking; when a
      subscriber cannot keep up, messages are dropped and counted, and the total
      is logged on shutdown (`dropped=`). Treat every message as a hint and
      reconcile through RPC.

`scripts/zmq_sub_test.py` in the repository is a working subscriber to test
against.

The publisher is a build-time option: a daemon compiled without ZeroMQ ignores
`--zmq-pub`. See `COMPILE.md`.

For push notifications to a command or a webhook instead, see
[Notification Hooks](notify-hooks.md).
