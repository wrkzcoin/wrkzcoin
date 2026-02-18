# Rate Limits and Performance

## Daemon RPC controls

Primary knobs (daemon config/CLI):

- `rpc-max-rpm`: per-IP request cap (0 disables).
- `rpc-max-body-bytes`: max request body bytes.
- `rpc-max-global-index-range`: cap for `/get_global_indexes_for_range`.
- `rpc-max-block-count`: cap for block sync/raw block methods.
- `rpc-read-timeout` and `rpc-write-timeout`.

## Recommended starting points

- `rpc-max-rpm`: `240` for public-but-controlled access, higher for private LAN.
- `rpc-max-body-bytes`: keep low, raise only for real payload needs.
- `rpc-max-global-index-range`: keep default unless client genuinely needs larger windows.
- `rpc-max-block-count`: keep conservative; paginate client requests instead.

## Client-side performance patterns

- Prefer incremental sync calls over very large batch requests.
- Use checkpoint-based sync (`blockHashCheckpoints`) for wallet sync.
- Implement retry with backoff on `429`.
- Cache static-ish responses where possible (`/fee`, some metadata).

## Example: pagination instead of huge blockCount

```bash
for h in 0 100 200 300; do
  curl -s -H "Content-Type: application/json" \
    -d "{\"startHeight\":$h,\"startTimestamp\":0,\"blockCount\":100,\"blockHashCheckpoints\":[],\"skipCoinbaseTransactions\":false}" \
    "$DAEMON_RPC_URL/getwalletsyncdata" > /dev/null
done
```

## Example: observe throttling quickly

```bash
for i in $(seq 1 500); do
  curl -s -o /dev/null -w "%{http_code}\n" "$DAEMON_RPC_URL/info"
done | sort | uniq -c
```

## Wallet API tuning notes

- `--threads` affects wallet sync worker count.
- Higher thread counts can improve sync but increase CPU and memory pressure.
- Keep daemon and wallet API near each other (low RTT) for best sync behavior.
