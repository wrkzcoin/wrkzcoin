# Daemon HTTP Endpoints

Every non-JSON-RPC route the daemon serves, including the wallet sync endpoints
and their optional fields.

Implementation mapping: route registrations in `src/rpc/RpcServer.cpp`.

## GET

| Path | Handler | Permission Mode |
| --- | --- | --- |
| `/json_rpc` | JSON-RPC dispatcher | Depends on method |
| `/info` | `info` | `Standard` |
| `/height` | `height` | `Standard` |
| `/peers` | `peers` | `Standard` |
| `/getinfo` | `info` | `Standard` |
| `/getheight` | `height` | `Standard` |

`/getinfo` and `/getheight` are aliases of `/info` and `/height`, serving the
same handlers under the spelling Monero-lineage tooling expects. A solo miner
polls `/getheight` first and falls back to `/getinfo` when the answer carries no
`hash` member, which is how it tells a CryptoNote daemon from a Monero one — so
`/getheight` deliberately answers without one.

## POST

| Path | Handler | Permission Mode |
| --- | --- | --- |
| `/json_rpc` | JSON-RPC dispatcher | Depends on method |
| `/sendrawtransaction` | `sendTransaction` | `Standard` |
| `/getrandom_outs` | `getRandomOuts` | `Standard` |
| `/getwalletsyncdata` | `getWalletSyncData` | `Standard` |
| `/get_global_indexes_for_range` | `getGlobalIndexes` | `Standard` |
| `/queryblockslite` | `queryBlocksLite` | `Standard` |
| `/get_transactions_status` | `getTransactionsStatus` | `Standard` |
| `/get_pool_changes_lite` | `getPoolChanges` | `Standard` |
| `/queryblocksdetailed` | `queryBlocksDetailed` | `Explorer` |
| `/get_o_indexes` | `getGlobalIndexesDeprecated` | `Standard` |
| `/getrawblocks` | `getRawBlocks` | `Standard` |
| `/console` | `console` | `Standard`, local socket only |

`/console` runs daemon console commands and is registered **only** on the IPC
socket (`--rpc-ipc-path`), never on a TCP listener, token or no token. The mode
on the socket file decides who may connect.

## OPTIONS

- `.*` handled for CORS preflight support.

## GET Examples

```bash
curl -s "$DAEMON_RPC_URL/info"
curl -s "$DAEMON_RPC_URL/height"
curl -s "$DAEMON_RPC_URL/peers"
```

`/info` reports the node's own state: `height`, `top_block_hash`,
`network_height`, `difficulty`, `hashrate`, peer counts, `synced`, the lite node
fields (`lite`, `lite_start_height`), `compression` and `sync_features`.

## POST Examples

`/sendrawtransaction`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"tx_as_hex":"<signed-tx-hex>"}' \
  "$DAEMON_RPC_URL/sendrawtransaction"
```

`/getrandom_outs`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"amounts":[10000,50000],"outs_count":5}' \
  "$DAEMON_RPC_URL/getrandom_outs"
```

`/getwalletsyncdata`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"blockHashCheckpoints":[],"startHeight":0,"startTimestamp":0,"blockCount":100,"skipCoinbaseTransactions":false}' \
  "$DAEMON_RPC_URL/getwalletsyncdata"
```

### Optional sync fields

All of these are opt in, and a request that omits them behaves exactly as it
always has. A client should only send one after seeing the daemon name it in
`sync_features` from `/info` — an older daemon ignores what it does not know,
which for these would mean silently getting a different answer than expected.

| Request field | Feature name | Effect |
| --- | --- | --- |
| `skipEmptyBlocks` | `skipEmptyBlocks` | Leaves out blocks holding nothing but a coinbase. Only takes effect together with `skipCoinbaseTransactions`, since otherwise the coinbase is wanted. One response can then carry a wallet across far more heights than its `blockCount`. |
| `encoding` | `base64` | `"hex"` (default) or `"base64"`. Hashes and keys are 44 characters as base64 against 64 as hex. |
| `endHeight` | `heightRange` | Exclusive upper bound on the heights the response may cover. Lets a client name several windows before it has seen any of the answers, and fetch them at once. `0` or absent means unbounded. |

| Response field | Meaning |
| --- | --- |
| `scannedToHeight` | The highest height the daemon actually looked at, which is not the same as the highest block it sent — the ones in between held nothing worth sending. A client may resume from `scannedToHeight + 1`. Absent when the daemon cannot vouch for a whole window, in which case only advance as far as the last block received. |

Because blocks are left out rather than the range being cut short, the heights
in a response are not contiguous when `skipEmptyBlocks` is on. A client that
treats a gap as a chain fork must not ask for it.

`/get_global_indexes_for_range`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"startHeight":1000,"endHeight":1100}' \
  "$DAEMON_RPC_URL/get_global_indexes_for_range"
```

`/queryblockslite`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"blockIds":["<known-block-hash>"],"timestamp":0}' \
  "$DAEMON_RPC_URL/queryblockslite"
```

`/get_transactions_status`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"transactionHashes":["<tx-hash-1>","<tx-hash-2>"]}' \
  "$DAEMON_RPC_URL/get_transactions_status"
```

`/get_pool_changes_lite`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"tailBlockId":"<tail-block-hash>","knownTxsIds":[]}' \
  "$DAEMON_RPC_URL/get_pool_changes_lite"
```

`/queryblocksdetailed`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"blockIds":["<known-block-hash>"],"timestamp":0,"blockCount":20}' \
  "$DAEMON_RPC_URL/queryblocksdetailed"
```

`/get_o_indexes`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"txid":"<tx-hash-64-hex>"}' \
  "$DAEMON_RPC_URL/get_o_indexes"
```

`/getrawblocks`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"startHeight":0,"startTimestamp":0,"blockHashCheckpoints":[],"blockCount":20,"skipCoinbaseTransactions":false}' \
  "$DAEMON_RPC_URL/getrawblocks"
```
