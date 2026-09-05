# Wallet API RPC Overview

What `wrkz-wallet-api` exposes and how a client is expected to drive it.

Implementation: `src/walletapi/ApiDispatcher.cpp`, `src/walletapi/ApiDispatcher.h`

Wallet API is an HTTP API (not JSON-RPC) with routes for:

- Wallet lifecycle (`/wallet/*`, `/save`, `/reset`)
- Address management (`/addresses/*`)
- Transaction creation, sending, sweeping and query
- Key operations (`/keys/*`)
- Balance and node status (`/balance`, `/balances`, `/node`, `/status`)

Fusion / optimize endpoints were **removed in 0.4.7**; consolidate inputs with
`/transactions/send/sweep` and `/transactions/send/sweep/all` instead. See the
[changelog](../changelog/wallet-api.md).

Request handling goes through middleware with:

- API key authentication
- Wallet state checks (must be open/closed depending on endpoint)
- View-wallet restrictions for sensitive operations

Regex constants used in routes:

- Address regex: `ApiConstants::addressRegex`
- Hash regex: `ApiConstants::hashRegex`
- Payment ID regex: `ApiConstants::paymentIDRegex`

Defined in `src/walletapi/Constants.h`.

## Quick Flow Examples

Set environment variables:

```bash
export WALLET_API_URL="http://127.0.0.1:7856"
export WALLET_API_KEY="replace-me"
```

Open wallet:

```bash
curl -s -X POST "$WALLET_API_URL/wallet/open" \
  -H "X-API-KEY: $WALLET_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"filename":"wallet.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```

Check status:

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/status"
```

Get total balance:

```bash
curl -s -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/balance"
```

Close wallet:

```bash
curl -s -X DELETE -H "X-API-KEY: $WALLET_API_KEY" "$WALLET_API_URL/wallet"
```
