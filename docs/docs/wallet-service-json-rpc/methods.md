# Wallet Service JSON-RPC Methods

Every method `wrkz-service` answers, with an example payload for each.

Method registrations are in `src/walletservice/PaymentServiceJsonRpcServer.cpp`.

Current methods:

- `save`
- `export`
- `reset`
- `createAddress`
- `createAddressList`
- `deleteAddress`
- `getSpendKeys`
- `getBalance`
- `getBlockHashes`
- `getTransactionHashes`
- `getTransactions`
- `getUnconfirmedTransactionHashes`
- `getTransaction`
- `sendTransaction`
- `createDelayedTransaction`
- `getDelayedTransactionHashes`
- `deleteDelayedTransaction`
- `sendDelayedTransaction`
- `getViewKey`
- `getMnemonicSeed`
- `getStatus`
- `getAddresses`
- `createIntegratedAddress`
- `getFeeInfo`
- `getNodeFeeInfo`

## Node fees are not implemented

`getFeeInfo` and `getNodeFeeInfo` succeed but always answer with an empty
address and an amount of `0`. The wallet backend does not read a node fee from
the daemon (`NodeRpcProxy::getFeeInfo` clears both fields), and the daemon's
`--fee-address` / `--fee-amount` options are not served over RPC. Do not build a
fee-splitting integration on these two methods.

## Error codes

Failures carry a numeric code from the shared wallet error table; see
[Wallet Error Codes](../guides/error-codes.md).

## Authentication behavior

When `legacySecurity` is disabled, requests must include a `password` field in request JSON. Invalid or missing password is rejected.

## Example Payloads

Base command pattern:

```bash
curl -s -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"<method>","password":"<rpc-password>","params":{}}' \
  "$WALLET_SERVICE_URL"
```

`save`, `reset`, `export`:

```json
{"jsonrpc":"2.0","id":"1","method":"save","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"2","method":"reset","password":"<rpc-password>","params":{"scanHeight":0}}
{"jsonrpc":"2.0","id":"3","method":"export","password":"<rpc-password>","params":{"fileName":"wallet-export.json"}}
```

`createAddress`, `createAddressList`, `deleteAddress`:

```json
{"jsonrpc":"2.0","id":"4","method":"createAddress","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"5","method":"createAddress","password":"<rpc-password>","params":{"spendSecretKey":"<secret-key>","scanHeight":0}}
{"jsonrpc":"2.0","id":"6","method":"createAddressList","password":"<rpc-password>","params":{"spendSecretKeys":["<secret-key-1>","<secret-key-2>"],"scanHeight":0}}
{"jsonrpc":"2.0","id":"7","method":"deleteAddress","password":"<rpc-password>","params":{"address":"<address>"}}
```

`getSpendKeys`, `getBalance`, `getAddresses`, `getViewKey`, `getMnemonicSeed`:

```json
{"jsonrpc":"2.0","id":"8","method":"getSpendKeys","password":"<rpc-password>","params":{"address":"<address>"}}
{"jsonrpc":"2.0","id":"9","method":"getBalance","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"10","method":"getBalance","password":"<rpc-password>","params":{"address":"<address>"}}
{"jsonrpc":"2.0","id":"11","method":"getAddresses","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"12","method":"getViewKey","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"13","method":"getMnemonicSeed","password":"<rpc-password>","params":{"address":"<address>"}}
```

`getStatus`, `getFeeInfo`, `getNodeFeeInfo`:

```json
{"jsonrpc":"2.0","id":"14","method":"getStatus","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"15","method":"getFeeInfo","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"16","method":"getNodeFeeInfo","password":"<rpc-password>","params":{}}
```

`getBlockHashes`:

```json
{"jsonrpc":"2.0","id":"17","method":"getBlockHashes","password":"<rpc-password>","params":{"firstBlockIndex":0,"blockCount":100}}
```

`getTransactionHashes`, `getTransactions`, `getTransaction`, `getUnconfirmedTransactionHashes`:

```json
{"jsonrpc":"2.0","id":"18","method":"getTransactionHashes","password":"<rpc-password>","params":{"firstBlockIndex":0,"blockCount":100}}
{"jsonrpc":"2.0","id":"19","method":"getTransactionHashes","password":"<rpc-password>","params":{"blockHash":"<block-hash>","blockCount":100,"paymentId":"<optional-payment-id>","addresses":["<address>"]}}
{"jsonrpc":"2.0","id":"20","method":"getTransactions","password":"<rpc-password>","params":{"firstBlockIndex":0,"blockCount":100}}
{"jsonrpc":"2.0","id":"21","method":"getTransaction","password":"<rpc-password>","params":{"transactionHash":"<tx-hash>"}}
{"jsonrpc":"2.0","id":"22","method":"getUnconfirmedTransactionHashes","password":"<rpc-password>","params":{"addresses":["<address>"]}}
```

`sendTransaction`:

```json
{"jsonrpc":"2.0","id":"23","method":"sendTransaction","password":"<rpc-password>","params":{"addresses":["<source-address>"],"transfers":[{"address":"<destination>","amount":1000000}],"fee":1000,"anonymity":7,"unlockTime":0}}
```

`createDelayedTransaction`, `getDelayedTransactionHashes`, `sendDelayedTransaction`, `deleteDelayedTransaction`:

```json
{"jsonrpc":"2.0","id":"24","method":"createDelayedTransaction","password":"<rpc-password>","params":{"addresses":["<source-address>"],"transfers":[{"address":"<destination>","amount":1000000}],"feePerByte":1.25,"anonymity":7}}
{"jsonrpc":"2.0","id":"25","method":"getDelayedTransactionHashes","password":"<rpc-password>","params":{}}
{"jsonrpc":"2.0","id":"26","method":"sendDelayedTransaction","password":"<rpc-password>","params":{"transactionHash":"<tx-hash>"}}
{"jsonrpc":"2.0","id":"27","method":"deleteDelayedTransaction","password":"<rpc-password>","params":{"transactionHash":"<tx-hash>"}}
```

`createIntegratedAddress`:

```json
{"jsonrpc":"2.0","id":"28","method":"createIntegratedAddress","password":"<rpc-password>","params":{"address":"<address>","paymentId":"<payment-id-hex>"}}
```
