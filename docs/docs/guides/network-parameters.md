# Network Parameters

Consensus constants and the heights they change at. All of these live in
`src/config/CryptoNoteConfig.h`; this page is a readable copy of that file, not
a second source of truth. When one disagrees with the other, the header wins.

## Chain basics

| | |
| --- | --- |
| Name | `WRKZCoin` |
| Ticker | `WRKZ` |
| Decimal places | `2` |
| Atomic unit | `0.01 WRKZ` |
| Target block time | `60` seconds |
| Blocks per day | `1440` |
| Total supply | `500,000,000,000.00 WRKZ` (`MONEY_SUPPLY` = 50,000,000,000,000 atomic) |
| Emission speed factor | `22` |
| Genesis block reward | 3% of the money supply |
| Coinbase unlock window | `40` blocks |
| Address base58 prefix | `999730` |
| Initial max block size | `100,000` bytes |
| P2P port / RPC port / ZMQ port | `17855` / `17856` / `17857` |

## Block versions and proof of work

The hashing algorithm is selected by the block's major version
(`HASHING_ALGORITHMS_BY_BLOCK_VERSION`).

| Block version | Activates at height | Algorithm |
| --- | --- | --- |
| 1 | `0` | `cn_slow_hash_v0` |
| 2 | `1` | `cn_slow_hash_v0` |
| 3 | `2` | `cn_slow_hash_v0` |
| 4 | `3` | `cn_lite_slow_hash_v1` |
| 5 | `302,400` | `cn_turtle_lite_slow_hash_v2` |
| 6 | `600,000` | `chukwa_slow_hash` |
| 7 | `1,000,000` | `cn_upx` — **current** |

`cn_upx` is xmrig's `cn/upx2`. See [Solo Mining](solo-mining.md).

## Ring size (mixin)

"Mixin" is the number of decoys; the **ring size is mixin + 1**.

Resolved by `getMixinAllowableRange()` in `src/utilities/Mixins.cpp`. Each row
applies from its height until the next one.

| From height | Minimum mixin | Maximum mixin | Default mixin | Default ring size |
| --- | --- | --- | --- | --- |
| `0` | `0` | unlimited | `3` | 4 |
| `10,000` | `0` | `30` | `3` | 4 |
| `302,400` | `3` | `7` | `3` | 4 |
| `430,000` | `0` | `7` | `3` | 4 |
| `658,500` | `1` | `3` | `3` | 4 |
| `1,000,000` | `1` | `1` | `1` | 2 (fixed) |
| `4,300,000` | `1` | `7` | `7` | **8** |

The limit in force is the one that was correct **when the block was formed**, not
the one current now — otherwise the chain could not be resynced past its own
history.

!!! warning "The V6 rule is transitional, and low mixin is a fingerprint"
    From height 4,300,000 the default ring size is 8, but the **old minimum is
    still accepted**, so outputs whose denomination does not yet have enough
    decoys on chain stay spendable at a lower mixin. The next fork is intended
    to raise the minimum to match the maximum, giving a fixed ring size.

    Until then, a low-mixin spend deanonymises the decoys other transactions
    rely on, and the ring size itself is a distinguishing feature of the
    transaction. Send at the default unless you have no choice.

Since 0.4.8 the wallet retries at the best ring size actually achievable rather
than refusing to send, and reports the ring it used alongside the network's. See
error codes [`21`, `22` and `28`](error-codes.md).

## Fees

| From height | Minimum fee |
| --- | --- |
| `0` | `5` atomic (`0.05 WRKZ`) |
| `678,500` | `50,000` atomic (`500.00 WRKZ`) |

| From height | Minimum fee per byte |
| --- | --- |
| `832,000` | `500.00` per 256-byte chunk |
| `1,500,000` | `10.00` per 128-byte chunk |

## Transaction proof of work

Every transaction carries a small proof of work over its unsigned prefix, hashed
with `cn_upx`.

| From height | Rule |
| --- | --- |
| `1,123,000` | Transaction PoW required. Fixed difficulty `20,000`; fusion transactions `60,000` |
| `1,200,000` | Dynamic difficulty: `40,000 + (inputs + 4 × outputs) × 1,000` |
| `1,500,000` | A transaction may **skip** the proof of work by paying at least `10,000` atomic units (`100.00 WRKZ`) in fees |

A transaction with two inputs and six outputs therefore needs difficulty
`40,000 + (2 + 24) × 1,000 = 66,000`.

The search takes seconds on a desktop, longer on a phone, and is impractical in
the single-threaded browser wallet — which is why the web wallet pays the bypass
fee unless a [transaction PoW server](txpow-server.md) is configured.

## Other transaction limits

| | |
| --- | --- |
| Max outputs in a normal transaction | `90`, from height `777,777` |
| Max single output amount (node) | `125,000,000,000.00 WRKZ`, from height `800,000` |
| Max single output amount (client) | `5,000,000,000.00 WRKZ` |
| Dust threshold | `10` atomic before height `302,400`, `0` after |
| Max transaction size | `1,000,000,000` bytes |

## P2P protocol

| | |
| --- | --- |
| Current version | `19` |
| Minimum accepted version | `16` |
| IPv6 capability gate | `19` |
| Lite block propagation from | `4` |
| Default peer connections | `15` in, `15` out |
| Peers per handshake | `250` |

See [Networking](networking.md).

## Lite node and snapshots

| | |
| --- | --- |
| Minimum depth below the tip for a lite height | `20,160` blocks (14 days) |
| First blessed snapshot height | `4,000,000` |

See [Lite Nodes](lite-node.md) and [Lite Node Snapshots](lite-snapshots.md).
