# Daemon HTTP Endpoints

Implementation mapping: route registrations in `src/rpc/RpcServer.cpp`.

## GET

| Path | Handler | Permission Mode |
| --- | --- | --- |
| `/json_rpc` | JSON-RPC dispatcher | Depends on method |
| `/info` | `info` | `Default` |
| `/fee` | `fee` | `Default` |
| `/height` | `height` | `Default` |
| `/peers` | `peers` | `Default` |

## POST

| Path | Handler | Permission Mode |
| --- | --- | --- |
| `/json_rpc` | JSON-RPC dispatcher | Depends on method |
| `/sendrawtransaction` | `sendTransaction` | `Default` |
| `/getrandom_outs` | `getRandomOuts` | `Default` |
| `/getwalletsyncdata` | `getWalletSyncData` | `Default` |
| `/get_global_indexes_for_range` | `getGlobalIndexes` | `Default` |
| `/queryblockslite` | `queryBlocksLite` | `Default` |
| `/get_transactions_status` | `getTransactionsStatus` | `Default` |
| `/get_pool_changes_lite` | `getPoolChanges` | `Default` |
| `/queryblocksdetailed` | `queryBlocksDetailed` | `AllMethodsEnabled` |
| `/get_o_indexes` | `getGlobalIndexesDeprecated` | `Default` |
| `/getrawblocks` | `getRawBlocks` | `Default` |

## OPTIONS

- `.*` handled for CORS preflight support.

## GET Examples

```bash
curl -s "$DAEMON_RPC_URL/info"
curl -s "$DAEMON_RPC_URL/fee"
curl -s "$DAEMON_RPC_URL/height"
curl -s "$DAEMON_RPC_URL/peers"
```

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
