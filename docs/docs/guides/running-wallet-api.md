# Running Wallet API

## Required

- `--rpc-password` must be set (`src/walletapi/ParseArguments.cpp`).

## Common startup flags

- `--rpc-bind-ip`
- `--port`
- `--rpc-password`
- `--enable-cors`
- `--threads`

## Request authentication

Send `X-API-KEY: <rpc-password>` on every request.

## Notes

- Wallet API route behavior and wallet state checks are implemented in `src/walletapi/ApiDispatcher.cpp`.
- Prefer binding to `127.0.0.1` unless reverse-proxying securely.

## Example command

```bash
./wrkz-wallet-api \
  --rpc-bind-ip 127.0.0.1 \
  --port 8070 \
  --rpc-password "strong-password" \
  --enable-cors "*"
```

## Smoke tests

```bash
curl -s -H "X-API-KEY: strong-password" http://127.0.0.1:8070/status

curl -s -X POST http://127.0.0.1:8070/wallet/open \
  -H "X-API-KEY: strong-password" \
  -H "Content-Type: application/json" \
  -d '{"filename":"wallet.wallet","password":"wallet-pass","daemonHost":"127.0.0.1","daemonPort":17856,"daemonSSL":false}'
```
