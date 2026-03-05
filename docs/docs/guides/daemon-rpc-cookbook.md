# Daemon RPC Cookbook

Set shared vars:

```bash
export DAEMON_RPC_URL="http://127.0.0.1:11898"
export DAEMON_RPC_TOKEN="strong-token"
```

Optional auth header:

```bash
AUTH_HEADER=(-H "X-API-Key: $DAEMON_RPC_TOKEN")
```

## 1) Health and network checks

```bash
curl -s "${AUTH_HEADER[@]}" "$DAEMON_RPC_URL/info"
curl -s "${AUTH_HEADER[@]}" "$DAEMON_RPC_URL/height"
curl -s "${AUTH_HEADER[@]}" "$DAEMON_RPC_URL/peers"
curl -s "${AUTH_HEADER[@]}" "$DAEMON_RPC_URL/fee"
```

## 2) Basic JSON-RPC calls

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"

curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"2","method":"getlastblockheader","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

## 3) Wallet sync pipeline (common client flow)

Pull compact sync data:

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"blockHashCheckpoints":[],"startHeight":0,"startTimestamp":0,"blockCount":100,"skipCoinbaseTransactions":false}' \
  "$DAEMON_RPC_URL/getwalletsyncdata"
```

Alternative lightweight chain query:

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"blockIds":["<known-block-hash>"],"timestamp":0}' \
  "$DAEMON_RPC_URL/queryblockslite"
```

Detailed chain query (requires `AllMethodsEnabled`):

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"blockIds":["<known-block-hash>"],"timestamp":0,"blockCount":20}' \
  "$DAEMON_RPC_URL/queryblocksdetailed"
```

## 4) Pool and tx status checks

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"transactionHashes":["<tx-hash-1>","<tx-hash-2>"]}' \
  "$DAEMON_RPC_URL/get_transactions_status"

curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"tailBlockId":"<tail-block-hash>","knownTxsIds":[]}' \
  "$DAEMON_RPC_URL/get_pool_changes_lite"
```

## 5) Broadcast signed transaction

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"tx_as_hex":"<signed-tx-hex>"}' \
  "$DAEMON_RPC_URL/sendrawtransaction"
```

## 6) Mining-oriented methods

```bash
curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"10","method":"getblocktemplate","params":{"reserve_size":8,"wallet_address":"<miner-address>"}}' \
  "$DAEMON_RPC_URL/json_rpc"

curl -s "${AUTH_HEADER[@]}" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"11","method":"submitblock","params":["<block-blob-hex>"]}' \
  "$DAEMON_RPC_URL/json_rpc"
```
