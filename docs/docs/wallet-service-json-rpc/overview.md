# Wallet Service JSON-RPC Overview

Implementation: `src/walletservice/PaymentServiceJsonRpcServer.cpp`

This is a JSON-RPC server used by wallet service (`walletservice`), separate from the HTTP wallet-api routes.

Request format is JSON-RPC-like with:

- `method`
- optional `params`
- optional `password` when legacy security is disabled

Password checking behavior is in `processJsonRpcRequest`.

Use this section when maintaining integrations built on payment-service RPC method names.

## Endpoint and Envelope

HTTP endpoint:

- `POST /json_rpc`

Example envelope:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getStatus",
  "password": "rpc-password",
  "params": {}
}
```

Curl template:

```bash
export WALLET_SERVICE_URL="http://127.0.0.1:8070/json_rpc"
export WALLET_SERVICE_PASSWORD="replace-me"

curl -s -H "Content-Type: application/json" \
  -d "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"getStatus\",\"password\":\"$WALLET_SERVICE_PASSWORD\",\"params\":{}}" \
  "$WALLET_SERVICE_URL"
```
