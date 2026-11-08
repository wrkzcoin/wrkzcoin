# WebSocket events

With `--enable-websocket`, `Wrkzd` serves `GET /ws` on its RPC port: what
happens to the chain and the pool, as a WebSocket stream. It carries the same
topics and the same JSON as the ZMQ socket (`--zmq-pub`), on a port a browser,
a reverse proxy and TLS can all reach. The wallets in this repository follow
it, so a synced wallet hears of a block the moment its node has it instead of
asking every few seconds.

It is off by default, and needs no ZMQ support in the build.

```sh
Wrkzd --enable-websocket
# ws://127.0.0.1:17856/ws
```

| Option | Default | What it does |
| --- | --- | --- |
| `--enable-websocket` | off | serve `GET /ws` on the RPC port. Off, `/ws` is the 404 of any unrouted path |
| `--ws-max-clients N` | 128 | subscribers at once; past it an upgrade is a `503`. Must be at least 1 |
| `--ws-max-clients-per-ip N` | 4 | from one address, loopback exempt; past it a `429`. `0` is no per-address cap |

All three are configuration-file keys too, `enable-websocket`,
`ws-max-clients` and `ws-max-clients-per-ip`, and `--dump-config` writes them.

The stream is served on the IPv4 and IPv6 RPC listeners, not on the local IPC
socket (`--rpc-ipc-path`).

## Messages

Every message is a text frame holding one JSON object,
`{"topic":"…","data":{…}}`. `data` is, byte for byte, the body the ZMQ socket
publishes under the same topic (`src/rpc/ChainEvents.cpp` builds both);
`height` is a block's index.

| Topic | `data` | Sent when |
| --- | --- | --- |
| `hello` | `{"height":N,"hash":"…","topics":[…]}` | first, once: the tip, and the topics this stream carries |
| `hashblock` | `{"height":N,"hash":"…"}` | a block joins the main chain |
| `chain_main` | `{"height":N,"hash":"…","transaction_hashes":[…]}` | straight after it; the coinbase first. Not sent below a lite node's lite height |
| `hashblock_alt` | `{"height":N,"hash":"…"}` | a block is kept on an alternative chain |
| `chainswitch` | `{"common_root_height":R,"hashes":[…]}` | a reorganisation; the common root first |
| `txpool_add` | `{"hashes":["…"]}` | a transaction enters the pool |
| `txpool_del` | `{"hashes":[…],"reason":"InBlock"}`, or `Outdated`, `NotActual` | transactions leave it |
| `heartbeat` | `{}` | after 30 seconds with nothing else to send |

`?topics=` picks topics by prefix, comma-separated, as a ZMQ subscription
does: `/ws?topics=hashblock,chainswitch` brings `hashblock`, `hashblock_alt`
and `chainswitch`. A prefix that matches no topic is a `400`, so a misspelling
is not a silent stream; at most 16 prefixes. Without `?topics=` every topic is
sent; `hello` and `heartbeat` always are.

Treat the stream as a notification, not a ledger. It never replays what was
missed: after connecting, read `hello` and catch up over the ordinary RPC.

## Limits and liveness

- The upgrade goes through the same checks as every route, in the same
  order: the access token (`X-API-Key` or `Authorization: Bearer`), then the
  rate limit, where it counts as one request. A refusal is an ordinary HTTP
  answer, before any `101`.
- A `GET /ws` that does not ask to upgrade, or asks in a WebSocket version
  other than 13, is a `426` naming version 13.
- Each subscriber holds one RPC worker thread while it is open, plus the
  thread that pings it, and a queue of 1000 messages. The RPC pool grows to
  make room, up to eight times its base size.
- A subscriber that falls 1000 messages behind is **disconnected** (close
  1008), not skipped, so a stream that looks complete is complete.
- The node pings every 30 seconds, and sends a `heartbeat` whenever 30
  seconds pass without a message, for browsers, whose scripts never see pings.
- The node never reads what a subscriber sends. The connection's one reader
  is the thread that closes it, and a subscriber has nothing to say. A
  subscriber that has gone away is found when a write to it fails, which the
  pings and heartbeats make happen within a minute or so.
- When the node stops, every subscriber gets a close with code 1001.

## Browsers

A page cannot set headers on a WebSocket, so it cannot present an access
token: a node with `--rpc-access-token` streams only to programs. And a page
may subscribe only where it may already call the RPC: a request that carries
an `Origin` is refused with a `403` unless `--enable-cors` allows that origin
(or `*`).

## Wallets

`wrkz-wallet`, `wrkz-wallet-api`, `wrkz-service`, the desktop and mobile GUI
wallets and the web wallet follow their node's stream by themselves. There is
nothing to configure:

- on `hello`, a block, a reorganisation or a pool change, the wallet syncs at
  once, and asks `/info` again (at most once a second) so the heights move
  with the block;
- while the stream is live - a frame within the last 75 seconds - a synced
  wallet polls every 30 seconds instead of every 5, only in case a message was
  lost (the web wallet: instead of every 2);
- a node that does not serve `/ws` - an older node, or one without
  `--enable-websocket` - answers the upgrade with something other than `101`.
  After three such answers from a node that has never streamed, the wallet
  leaves it alone for ten minutes and polls exactly as before. Nothing is
  logged above debug;
- a dropped stream is retried after one second, backing off to a minute;
- `https://` nodes are followed over `wss://` when the build has TLS, except
  on Android, whose trust store the WebSocket client cannot read; a node over
  a local IPC socket is not followed.

The stream only ever wakes a wallet. Balances and history still come from the
ordinary sync, so a lost or late message costs at most the time until the next
poll. The wallet side lives in `src/nigel/TipWatch.cpp` (and, for the web
wallet, `wallet_worker.js`).

## Watching it

Any WebSocket client works. With [websocat](https://github.com/vi/websocat):

```sh
websocat 'ws://127.0.0.1:17856/ws?topics=hashblock'
```
