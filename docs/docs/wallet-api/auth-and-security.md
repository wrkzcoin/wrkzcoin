# Wallet API Auth and Security

Implementation: `src/walletapi/ApiDispatcher.cpp`, `src/walletapi/ParseArguments.cpp`

## Authentication

Wallet API requires `--rpc-password` at startup.

Clients must send:

- `X-API-KEY: <rpc-password>`

The server derives and compares a PBKDF2-HMAC-SHA256 hash of the supplied value.

Missing or invalid `X-API-KEY` returns `401`.

## CORS

When `--enable-cors` is set, response includes:

- `Access-Control-Allow-Origin: <configured value>`
- `Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept, X-API-KEY`

## Wallet state and permission checks

Middleware enforces:

- Wallet must be open for most operational endpoints
- Wallet must be closed for open/create/import operations
- Some endpoints are blocked for view-only wallets

Common status codes:

- `401` unauthorized
- `403` wrong wallet state
- `400` invalid request or restricted view-wallet operation

## Auth Examples

Missing API key (`401`):

```bash
curl -i -s "$WALLET_API_URL/status"
```

Valid API key:

```bash
curl -s \
  -H "X-API-KEY: $WALLET_API_KEY" \
  "$WALLET_API_URL/status"
```

Invalid API key (`401`):

```bash
curl -i -s \
  -H "X-API-KEY: wrong-password" \
  "$WALLET_API_URL/status"
```

CORS preflight:

```bash
curl -i -s -X OPTIONS \
  -H "Origin: https://example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: X-API-KEY, Content-Type" \
  "$WALLET_API_URL/wallet/open"
```
