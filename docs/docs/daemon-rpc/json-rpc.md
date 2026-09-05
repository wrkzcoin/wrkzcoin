# Daemon JSON-RPC Methods

Every method the daemon answers on `/json_rpc`, with the RPC mode each needs and
a worked example.

Implementation mapping: `src/rpc/RpcServer.cpp` method switch in `/json_rpc` dispatcher.

`/json_rpc` accepts JSON with a `method` field. Current methods:

| Method | Handler | Permission Mode |
| --- | --- | --- |
| `getblocktemplate` | `getBlockTemplate` | `Standard` |
| `submitblock` | `submitBlock` | `Standard` |
| `getblockcount` | `getBlockCount` | `Standard` |
| `getlastblockheader` | `getLastBlockHeader` | `Standard` |
| `getblockheaderbyhash` | `getBlockHeaderByHash` | `Standard` |
| `getblockheaderbyheight` | `getBlockHeaderByHeight` | `Standard` |
| `f_blocks_list_json` | `getBlocksByHeight` | `Explorer` |
| `f_block_json` | `getBlockDetailsByHash` | `Explorer` |
| `f_transaction_json` | `getTransactionDetailsByHash` | `Explorer` |
| `f_on_transactions_pool_json` | `getTransactionsInPool` | `Explorer` |
| `f_transactions_by_payment_id_json` | `getTransactionHashesByPaymentId` | `Explorer` |

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

## Response shape

The response carries back the `id` of the request it answers, as JSON-RPC 2.0
requires, in whatever type it was sent:

```json
{
  "id": "1",
  "jsonrpc": "2.0",
  "result": { "count": 4207131, "status": "OK" }
}
```

A request that sends no `id` gets an answer without one. Errors carry the same
`id` alongside an `error` object, and still return HTTP `200`:

```json
{
  "id": "1",
  "jsonrpc": "2.0",
  "error": { "code": -4, "message": "..." }
}
```

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

`params` takes the reserved space either way round:

- `reserve_size` — how many bytes to leave blank in the miner transaction's
  extra. The reply's `reserved_offset` says where they are, and the caller fills
  them in. This is what a pool wants.
- `extra_nonce` — hex bytes to embed directly, in which case the node reserves
  exactly that much and writes them for you. This is what Monero-lineage solo
  miners send, and they send no `reserve_size` at all.

Sending neither gets a template with no reserved bytes. Sending both uses
`extra_nonce`. Maximum 255 bytes either way.

The reply carries `blocktemplate_blob`, `difficulty`, `height`,
`reserved_offset` and `status`.

### Before you mine the template

**The template is not ready to hash as returned.** For every block version
above 1 the node appends a *placeholder* merge-mining tag to the parent block's
coinbase and does not fill it in — whoever mines the block is expected to. Set
`mmTag.depth = 0` and `mmTag.merkleRoot` to the block's auxiliary header hash
(`CachedBlock::getAuxiliaryBlockHeaderHash()`, equivalently `getMerkleRoot()` in
`src/miner/BlockUtilities.cpp`), clear the parent coinbase's extra, and write
the tag back. `adjustMergeMiningTag()` in `src/miner/MinerManager.cpp` is the
reference implementation.

Skip this and `submitblock` rejects the finished block with **"Proof of work is
too weak"**, no matter how good the proof of work was. That error covers four
distinct failures in `Currency::checkProofOfWorkV2` — genuinely weak work, a
missing merge-mining tag, an over-long blockchain branch, and this mismatch —
and only the first is about difficulty. The daemon log tells them apart: the
tag failures print `Aux block hash wasn't found in merkle tree` or `merge mining
tag wasn't found ...` just before the rejection, and genuinely weak work prints
nothing extra.

Do this before computing the hashing blob: the tag lives in the parent
coinbase, and the parent's merkle root is part of what gets hashed. Rolling the
nonce afterwards is safe — the nonce is not part of what the tag commits to.

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
