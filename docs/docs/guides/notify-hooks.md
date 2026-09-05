# Notification Hooks

Monero-style push hooks, so a service learns about a block or a transaction
without polling. Each flag takes **either** an `http://` / `https://` URL — the
event is POSTed as a JSON object — **or** a command template, run directly
*without a shell*. Empty means disabled.

In a command template, quotes group arguments, `%`-placeholders are substituted
per argument, and `%%` is a literal percent.

## Daemon (`Wrkzd`)

| Option | Fires on | Placeholders | JSON body |
| --- | --- | --- | --- |
| `--block-notify <cmd\|url>` | Every new main-chain block, and each replacement block after a reorg | `%s` block hash, `%h` height | `{"event":"block","height":N,"hash":"..."}` |
| `--reorg-notify <cmd\|url>` | Every chain reorganisation | `%s` split height, `%h` new height, `%n` new blocks, `%d` discarded blocks | `{"event":"reorg","split_height":N,"new_height":N,"new_blocks":N,"discarded_blocks":N}` |
| `--tx-notify <cmd\|url>` | Every transaction entering the mempool | `%s` tx hash | `{"event":"tx","hash":"..."}` |
| `--notify-during-sync` | — | — | Also fire while the node is still synchronizing. Suppressed until synced by default |

All four are also accepted in the daemon config file under the same names.

## Wallet service (`wrkz-service`)

| Option | Fires on | Placeholders |
| --- | --- | --- |
| `--tx-notify <cmd\|url>` | Every new wallet transaction when first seen: incoming (pool or block) and outgoing once sent | `%s` hash, `%h` height (`0` = unconfirmed), `%a` amount (atomic, signed), `%f` fee, `%p` payment id, `%c` confirmed `0`/`1` |
| `--tx-confirmed-notify <cmd\|url>` | Once per transaction, when it gets into a block | Same as above |
| `--notify-during-sync` | — | Also fire for transactions more than 1440 blocks behind the daemon — i.e. during a rescan. Suppressed by default |

JSON body:

```json
{
  "event": "tx",
  "hash": "...",
  "height": 0,
  "amount": -1000000,
  "fee": 1000,
  "paymentId": "",
  "confirmed": false,
  "timestamp": 0,
  "unlockTime": 0
}
```

`"event"` is `"tx"` or `"tx_confirmed"`.

## Wallet API (`wrkz-wallet-api`)

| Option | Fires on | Placeholders |
| --- | --- | --- |
| `--tx-notify <cmd\|url>` | Every transaction **confirmed in a block** for the open wallet | `%s` hash, `%h` height, `%a` amount, `%f` fee, `%p` payment id |
| `--notify-during-sync` | — | Same rescan suppression rule as the wallet service |

wallet-api does not scan the pool for incoming transfers, so it has no
unconfirmed event. The JSON body is the wallet service's plus `"isCoinbase"`,
and `"confirmed"` is always `true`.

## Examples

```bash
# Built-in webhook, no external tools needed
Wrkzd --block-notify https://example.com/hooks/wrkz-block

wrkz-service ... --tx-notify http://127.0.0.1:9000/tx \
                 --tx-confirmed-notify http://127.0.0.1:9000/tx-confirmed

# Monero-compatible command form
Wrkzd --block-notify "/usr/local/bin/on-block.sh %s"
Wrkzd --block-notify "curl -s -X POST https://example.com/hook -d hash=%s -d height=%h"
```

## Behaviour and guarantees

- **Never blocks the node or wallet.** Events are queued in memory and delivered
  from a dedicated worker thread; the daemon's dispatcher and the wallet's sync
  loop only enqueue.
- **The queue is bounded** at 1024 entries. When a receiver is too slow, excess
  notifications are dropped and counted — `dropped=` is logged on shutdown.
- **Timeouts.** Each delivery has a 10-second budget: hung commands are killed,
  slow HTTP calls aborted. Transport failures on webhooks get one retry; HTTP
  4xx and 5xx responses are final.
- **Fire and forget.** There is no persistent retry queue. Treat notifications
  as hints and reconcile through RPC — the same model as Monero and ZMQ.
- **HTTPS webhooks need a build with OpenSSL.** Without it the hook is disabled
  with a warning at startup.
- Commands run with the privileges of the daemon or wallet process, and inherit
  its stdout and stderr.
