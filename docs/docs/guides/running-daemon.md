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
- `--daemon-mode` (`standard` or `explorer`)

See definitions in `src/daemon/DaemonConfiguration.cpp`.

## Mining

The daemon can serve stratum directly, so a stock miner needs no pool:

- `--stratum-bind-port` (`0`, off)
- `--stratum-bind-ip` (`127.0.0.1`)
- `--stratum-share-difficulty` (`0`, meaning the network difficulty)
- `--stratum-max-connections` (`32`)

This is a separate TCP listener with no authentication of its own, so it stays
on loopback unless you change it. Full notes in `MINING.md` in the repository
root.

## Recommended baseline

- Bind to localhost unless you intentionally expose RPC.
- Set `--rpc-access-token` for any non-local use.
- Keep request and rate limits enabled.

## Example command

```bash
./Wrkzd \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 17856 \
  --rpc-access-token "strong-token" \
  --rpc-max-body-bytes 2097152 \
  --rpc-max-rpm 240 \
  --rpc-max-global-index-range 5000 \
  --rpc-max-block-count 100
```

## Smoke tests

```bash
curl -s -H "X-API-Key: strong-token" http://127.0.0.1:17856/info
curl -s -H "X-API-Key: strong-token" -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"getblockcount","params":{}}' \
  http://127.0.0.1:17856/json_rpc
```
