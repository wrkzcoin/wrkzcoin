# Daemon RPC Auth and Security

Implementation: `src/rpc/RpcServer.cpp`, `src/daemon/DaemonConfiguration.cpp`

## Authentication

If `rpc-access-token` is set, every request must provide:

- `X-API-Key: <token>` or
- `Authorization: Bearer <token>`

Otherwise daemon returns `401 Unauthorized`.

If `rpc-access-token` is empty, token auth is disabled.

## Request limits and protection

Current protections in middleware:

- Max request body size: `rpc-max-body-bytes` (returns `413` on overflow)
- Rate limit per remote IP: `rpc-max-rpm` (returns `429` when exceeded)
- Optional proxy trust behavior: `rpc-trust-proxy`
- Method permission checks by RPC mode (returns `403` when blocked)

Domain-specific bounds:

- `rpc-max-block-count` for block sync/raw block queries
- `rpc-max-global-index-range` for global index range queries

## CORS

When CORS is configured, daemon sets `Access-Control-Allow-Origin`.

## Auth Header Examples

Using `X-API-Key`:

```bash
curl -s \
  -H "X-API-Key: $DAEMON_RPC_TOKEN" \
  "$DAEMON_RPC_URL/info"
```

Using Bearer token:

```bash
curl -s \
  -H "Authorization: Bearer $DAEMON_RPC_TOKEN" \
  "$DAEMON_RPC_URL/info"
```

Unauthorized request example (expected `401`):

```bash
curl -i -s "$DAEMON_RPC_URL/info"
```

Body too large example (expected `413`):

```bash
python - <<'PY'
import requests
url = "http://127.0.0.1:17856/sendrawtransaction"
big = {"tx_as_hex": "aa" * 10_000_000}
resp = requests.post(url, json=big)
print(resp.status_code)
print(resp.text[:200])
PY
```

Rate-limit behavior example (expected some `429` when exceeded):

```bash
for i in $(seq 1 400); do
  curl -s -o /dev/null -w "%{http_code}\n" "$DAEMON_RPC_URL/info"
done | sort | uniq -c
```
