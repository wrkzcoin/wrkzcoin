# Wallet API Endpoints

Implementation mapping: route registrations in `src/walletapi/ApiDispatcher.cpp`.

Route placeholders:

- `{address}` -> `ApiConstants::addressRegex`
- `{hash}` -> `ApiConstants::hashRegex`
- `{paymentId}` -> `ApiConstants::paymentIDRegex`

## POST

- `/wallet/open`
- `/wallet/import/key`
- `/wallet/import/seed`
- `/wallet/import/view`
- `/wallet/create`
- `/addresses/create`
- `/addresses/import`
- `/addresses/import/deterministic`
- `/addresses/import/view`
- `/addresses/validate`
- `/transactions/send/prepared`
- `/transactions/prepare/basic`
- `/transactions/send/basic`
- `/transactions/prepare/advanced`
- `/transactions/send/advanced`
- `/transactions/send/fusion/basic`
- `/transactions/send/fusion/advanced`
- `/export/json`

## DELETE

- `/wallet`
- `/addresses/{address}`
- `/transactions/prepared/{hash}`

## PUT

- `/save`
- `/reset`
- `/node`
- `/sync/refresh`

## GET

- `/node`
- `/keys`
- `/keys/{address}`
- `/keys/mnemonic/{address}`
- `/status`
- `/addresses`
- `/addresses/primary`
- `/addresses/{address}/{paymentId}`
- `/transactions`
- `/transactions/unconfirmed`
- `/transactions/unconfirmed/{address}`
- `/transactions/{startHeight}`
- `/transactions/{startHeight}/{endHeight}`
- `/transactions/address/{address}/{startHeight}`
- `/transactions/address/{address}/{startHeight}/{endHeight}`
- `/transactions/privatekey/{hash}`
- `/transactions/hash/{hash}`
- `/transactions/paymentid/{hash}`
- `/transactions/paymentid`
- `/balance`
- `/balance/{address}`
- `/balances`

## OPTIONS

- `.*` wildcard handler for CORS preflight.

## Notes

- `/transactions/paymentid/{hash}` currently uses `hashRegex` in route registration.
- Some endpoints require a full wallet (not view-only); enforced in middleware.

## Example Requests

All examples include:

```bash
-H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json"
```

### Wallet lifecycle

Create wallet:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/create" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"filename":"new.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Open wallet:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/open" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"filename":"new.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Import from keys:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/import/key" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"filename":"import.wallet","password":"wallet-pass","privateViewKey":"<view-secret-key>","privateSpendKey":"<spend-secret-key>","scanHeight":0,"daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Import from seed:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/import/seed" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"filename":"seed.wallet","password":"wallet-pass","mnemonicSeed":"<25-word-seed>","scanHeight":0,"daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Import view wallet:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/import/view" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"filename":"view.wallet","password":"wallet-pass","address":"<address>","privateViewKey":"<view-secret-key>","scanHeight":0,"daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Save, reset, close:

```bash
curl -s -X PUT "$WALLET_API_URL/save" -H "X-API-KEY: $WALLET_API_KEY"
curl -s -X PUT "$WALLET_API_URL/reset" -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" -d '{"scanHeight":0}'
curl -s -X DELETE "$WALLET_API_URL/wallet" -H "X-API-KEY: $WALLET_API_KEY"
```

### Address operations

Create random address:

```bash
curl -s -X POST "$WALLET_API_URL/addresses/create" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{}'
```

Import spend-key address:

```bash
curl -s -X POST "$WALLET_API_URL/addresses/import" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"privateSpendKey":"<spend-secret-key>","scanHeight":0}'
```

Import deterministic address:

```bash
curl -s -X POST "$WALLET_API_URL/addresses/import/deterministic" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"walletIndex":1,"scanHeight":0}'
```

Import view address:

```bash
curl -s -X POST "$WALLET_API_URL/addresses/import/view" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"publicSpendKey":"<public-spend-key>","scanHeight":0}'
```

Validate address:

```bash
curl -s -X POST "$WALLET_API_URL/addresses/validate" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"address":"<address>"}'
```

List/get/delete addresses:

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/addresses"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/addresses/primary"
curl -s -X DELETE -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/addresses/<address>"
```

Create integrated address:

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" \
  "$WALLET_API_URL/addresses/<address>/<payment-id-16-or-64-hex>"
```

### Transaction send/prepare

Prepare basic:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/prepare/basic" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destination":"<address>","amount":1000000,"paymentID":"<optional-payment-id>"}'
```

Send prepared:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/send/prepared" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"transactionHash":"<prepared-transaction-hash>"}'
```

Delete prepared:

```bash
curl -s -X DELETE -H "X-API-KEY: $WALLET_API_KEY" \
  "$WALLET_API_URL/transactions/prepared/<prepared-transaction-hash>"
```

Send basic:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/send/basic" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destination":"<address>","amount":1000000}'
```

Prepare/send advanced:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/prepare/advanced" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destinations":[{"address":"<address>","amount":1000000}],"mixin":7,"fee":1000,"sourceAddresses":["<address>"],"paymentID":"<optional-payment-id>","changeAddress":"<optional-change-address>","unlockTime":0,"extra":"<optional-extra-hex>"}'

curl -s -X POST "$WALLET_API_URL/transactions/send/advanced" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destinations":[{"address":"<address>","amount":1000000}],"mixin":7,"feePerByte":1.25,"sourceAddresses":["<address>"]}'
```

Fusion send:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/send/fusion/basic" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destination":"<address>"}'

curl -s -X POST "$WALLET_API_URL/transactions/send/fusion/advanced" \
  -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" \
  -d '{"destination":"<address>","mixin":7,"sourceAddresses":["<address>"],"extra":"<optional-extra-hex>","optimizeTarget":100000}'
```

### Transaction queries

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/unconfirmed"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/unconfirmed/<address>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/100000"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/100000/101000"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/address/<address>/100000"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/address/<address>/100000/101000"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/hash/<tx-hash-64-hex>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/paymentid/<payment-id-or-hash>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/paymentid"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/privatekey/<tx-hash-64-hex>"
```

### Key, node, and balances

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/keys"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/keys/<address>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/keys/mnemonic/<address>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/node"
curl -s -X PUT "$WALLET_API_URL/node" -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" -d '{"daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
curl -s -X PUT "$WALLET_API_URL/sync/refresh" -H "X-API-KEY: $WALLET_API_KEY"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/balance"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/balance/<address>"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/balances"
curl -s -X POST "$WALLET_API_URL/export/json" -H "X-API-KEY: $WALLET_API_KEY" -H "Content-Type: application/json" -d '{"filename":"wallet-export.json"}'
```
