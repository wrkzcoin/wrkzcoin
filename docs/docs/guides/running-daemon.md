# Running a Node

The daemon binary is `Wrkzd`. Run it with no arguments and it syncs the full
chain, serves RPC on `127.0.0.1:17856` and P2P on `0.0.0.0:17855`, with no
authentication and no stratum server.

Every option is listed with its default in the
[Configuration Reference](daemon-configuration.md).

## A reasonable public node

```bash
./Wrkzd \
  --rpc-bind-ip 0.0.0.0 \
  --rpc-bind-port 17856 \
  --rpc-access-token "strong-token" \
  --rpc-max-body-bytes 2097152 \
  --rpc-max-rpm 240 \
  --rpc-max-global-index-range 5000 \
  --rpc-max-block-count 1000 \
  --log-level 3 \
  --no-console
```

- Bind RPC to localhost unless you intend to expose it.
- Set `--rpc-access-token` for any non-local use.
- Keep the request and rate limits on.
- Use `--no-console` under a process manager, and reach the console with
  [`attach`](ipc-and-console.md#attaching-a-console-to-a-running-daemon) when
  you need it.

Fuller advice is in [Security Hardening](security-hardening.md) and
[Rate Limits and Performance](rate-limits-and-performance.md).

## Choosing a storage mode

| | When |
| --- | --- |
| **Full** (default) | Public nodes, seed nodes, explorers, anything other people sync against |
| **Pruned** (`--prune`) | Disk is tight but the node still serves others. Old block bodies are dropped; everything a wallet rescan needs is kept. Reversible |
| **Lite** (`--lite --lite-height H`) | A node for your own wallets. Full block data only from `H` upward. Much smaller, **permanent for the database**, and a poor public node |

See [Lite Nodes](lite-node.md) before choosing lite, and
[Lite Node Snapshots](lite-snapshots.md) to skip the index-only sync.

`--prune` and `--lite` cannot be combined, and `--lite` cannot be combined with
`--daemon-mode explorer`.

## Explorer mode

```bash
./Wrkzd --daemon-mode explorer
```

Explorer mode additionally serves `f_blocks_list_json`, `f_block_json`,
`f_transaction_json`, `f_on_transactions_pool_json`,
`f_transactions_by_payment_id_json` and `/queryblocksdetailed`. These walk
database indexes and are not something a plain node should answer for anyone who
asks — put a token or a proxy in front of it.

## Mining

The daemon can serve stratum directly, so a stock miner needs no pool:

```bash
./Wrkzd --stratum-bind-port 17858
```

It is a separate TCP listener with no authentication of its own, so it stays on
loopback unless you change it. Full notes in [Solo Mining](solo-mining.md).

## Local socket and console

```bash
./Wrkzd --no-console --rpc-ipc-path /run/wrkz/wrkzd.sock
./Wrkzd attach /run/wrkz/wrkzd.sock
```

The socket serves the whole RPC surface, plus `/console`, which is registered
**only** there. See [Local IPC and Console](ipc-and-console.md).

## Push notifications

```bash
./Wrkzd --block-notify https://example.com/hooks/block \
        --tx-notify "/usr/local/bin/on-tx.sh %s"
```

See [Notification Hooks](notify-hooks.md), or use the ZMQ publisher described in
[Networking](networking.md#zmq-publisher).

## Smoke tests

```bash
curl -s -H "X-API-Key: strong-token" http://127.0.0.1:17856/info

curl -s -H "X-API-Key: strong-token" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  http://127.0.0.1:17856/json_rpc
```

`/info` answers `{"status":"BUSY"}` for the whole of a heavy initial sync — the
handler tries to lock the chain and gives up rather than blocking it. That is
expected; poll again once sync settles.

Check that the build has zlib, without which every syncing wallet pulls several
times as many bytes:

```bash
curl -s http://127.0.0.1:17856/info | grep -o '"compression":"[^"]*"'
```

`"none"` means the build found no zlib. See
[Rate Limits and Performance](rate-limits-and-performance.md#response-compression).
