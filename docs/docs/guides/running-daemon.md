# Running Daemon RPC

## Typical startup flags

Use the daemon binary (`Wrkzd`) with RPC-related flags such as:

- `--rpc-bind-ip`
- `--rpc-bind-port`
- `--rpc-access-token`
- `--rpc-read-timeout`
- `--rpc-write-timeout`
- `--rpc-max-body-bytes`
- `--rpc-max-rpm`
- `--rpc-max-global-index-range`
- `--rpc-max-block-count`
- `--rpc-trust-proxy`

See definitions in `src/daemon/DaemonConfiguration.cpp`.

## Recommended baseline

- Bind to localhost unless you intentionally expose RPC.
- Set `--rpc-access-token` for any non-local use.
- Keep request and rate limits enabled.

## Example command

```bash
./Wrkzd \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 11898 \
  --rpc-access-token "strong-token" \
  --rpc-max-body-bytes 2097152 \
  --rpc-max-rpm 240 \
  --rpc-max-global-index-range 5000 \
  --rpc-max-block-count 100
```

## Smoke tests

```bash
curl -s -H "X-API-Key: strong-token" http://127.0.0.1:11898/info
curl -s -H "X-API-Key: strong-token" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  http://127.0.0.1:11898/json_rpc
```
