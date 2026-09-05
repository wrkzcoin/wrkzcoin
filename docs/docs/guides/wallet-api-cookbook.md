# Wallet API Cookbook

A complete working sequence against the wallet API, from creating a wallet to
sending from it and closing it again.

Set shared vars:

```bash
export WALLET_API_URL="http://127.0.0.1:7856"
export WALLET_API_KEY="strong-password"
```

## 1) Create wallet

```bash
curl -s -X POST "$WALLET_API_URL/wallet/create" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"filename":"demo.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

## 2) Open wallet

```bash
curl -s -X POST "$WALLET_API_URL/wallet/open" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"filename":"demo.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

## 3) Check status and balance

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/status"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/balance"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/addresses"
```

## 4) Send transaction (basic)

```bash
curl -s -X POST "$WALLET_API_URL/transactions/send/basic" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"destination":"<address>","amount":1000000}'
```

## 5) Prepare then send transaction

```bash
curl -s -X POST "$WALLET_API_URL/transactions/prepare/basic" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"destination":"<address>","amount":1000000}'
```

Use returned hash:

```bash
curl -s -X POST "$WALLET_API_URL/transactions/send/prepared" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"transactionHash":"<prepared-transaction-hash>"}'
```

## 6) Query transactions

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/unconfirmed"
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/transactions/hash/<tx-hash>"
```

## 7) Export and save

```bash
curl -s -X POST "$WALLET_API_URL/export/json" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"filename":"wallet-export.json"}'

curl -s -X PUT "$WALLET_API_URL/save" -H "X-API-KEY: $WALLET_API_KEY"
```

## 8) Reset and refresh

```bash
curl -s -X PUT "$WALLET_API_URL/reset" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"scanHeight":0}'

curl -s -X PUT "$WALLET_API_URL/sync/refresh" -H "X-API-KEY: $WALLET_API_KEY"
```

## 9) Close wallet

```bash
curl -s -X DELETE "$WALLET_API_URL/wallet" -H "X-API-KEY: $WALLET_API_KEY"
```
