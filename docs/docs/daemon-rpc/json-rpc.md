# Daemon JSON-RPC Methods

Implementation mapping: `src/rpc/RpcServer.cpp` method switch in `/json_rpc` dispatcher.

`/json_rpc` accepts JSON with a `method` field. Current methods:

| Method | Handler | Permission Mode |
| --- | --- | --- |
| `getblocktemplate` | `getBlockTemplate` | `MiningEnabled` |
| `submitblock` | `submitBlock` | `MiningEnabled` |
| `getblockcount` | `getBlockCount` | `Default` |
| `getlastblockheader` | `getLastBlockHeader` | `Default` |
| `getblockheaderbyhash` | `getBlockHeaderByHash` | `Default` |
| `getblockheaderbyheight` | `getBlockHeaderByHeight` | `Default` |
| `f_blocks_list_json` | `getBlocksByHeight` | `BlockExplorerEnabled` |
| `f_block_json` | `getBlockDetailsByHash` | `BlockExplorerEnabled` |
| `f_transaction_json` | `getTransactionDetailsByHash` | `BlockExplorerEnabled` |
| `f_on_transactions_pool_json` | `getTransactionsInPool` | `BlockExplorerEnabled` |

Unknown methods return `404`.

## Request shape

Example:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getblockcount",
  "params": {}
}
```

Use `X-API-Key` or `Authorization: Bearer` header when `rpc-access-token` is enabled.

## Method Examples

All examples call `POST /json_rpc`.

`getblockcount`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`getlastblockheader`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"2","method":"getlastblockheader","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`getblockheaderbyhash`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"3","method":"getblockheaderbyhash","params":{"hash":"<block-hash-64-hex>"}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`getblockheaderbyheight`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"4","method":"getblockheaderbyheight","params":{"height":12345}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`getblocktemplate`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"5","method":"getblocktemplate","params":{"reserve_size":8,"wallet_address":"<miner-address>"}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`submitblock`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"6","method":"submitblock","params":["<block-blob-hex>"]}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`f_blocks_list_json`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"7","method":"f_blocks_list_json","params":{"height":100000}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`f_block_json`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"8","method":"f_block_json","params":{"hash":"<block-hash-64-hex>"}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`f_transaction_json`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"9","method":"f_transaction_json","params":{"hash":"<tx-hash-64-hex>"}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

`f_on_transactions_pool_json`:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"10","method":"f_on_transactions_pool_json","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```

Unknown method example (`404`):

```bash
curl -i -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"404","method":"does_not_exist","params":{}}' \
  "$DAEMON_RPC_URL/json_rpc"
```
