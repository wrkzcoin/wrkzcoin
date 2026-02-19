# Explorer API (Fastify)

Lightweight API layer for block explorer features on top of daemon RPC.

## Prerequisites

- Node.js 18.17+
- Daemon RPC enabled with block explorer methods:
  - `--enable-blockexplorer`
  - `--rpc-access-token "<token>"` (recommended)

## Quick start

```bash
cd extras/block-explorer/api
cp .env.example .env
npm install
npm run start
```

Server defaults:

- Host: `127.0.0.1`
- Port: `8080`

## Implemented endpoints

- `GET /api/v1/health`
  - default is shallow (no heavy consistency RPC checks)
  - use `?deep=1` for consistency checks
  - use `?repair=1` to attempt repair (requires `X-Admin-Key`)
- `GET /api/v1/search?q=...`
- `GET /api/v1/block/:id` (`id` = block height or hash)
- `GET /api/v1/tx/:hash`
- `GET /api/v1/tx/by-payment-id/:paymentId`
- `GET /api/v1/network/summary`
- `GET /api/v1/index/status`
  - add `?deep=1` to include live daemon tip check
- `POST /api/v1/index/repair`
  - requires header `X-Admin-Key: <ADMIN_API_KEY>`

## Notes

- This service keeps the daemon token server-side.
- Basic TTL caching is enabled for summary/block/tx responses.
- Search resolves:
  - numeric: block height
  - 64-hex: block hash, then tx hash, then long payment ID
  - 16-hex: short payment ID
- SQLite stores lightweight indexed block metadata only:
  - `height`, `hash`, `timestamp`, `difficulty`, `tx_count`, `block_size`, `reward`, `orphan_status`
- SQLite also stores payment ID mapping for tx lookup:
  - `payment_id` / `payment_id_short` -> `tx_hash`, `block_height`, `timestamp`
- Payment ID indexing is optional and enabled by default (`INDEX_PAYMENT_IDS=true`), and it is significantly heavier.
- Admin operations are protected by `ADMIN_API_KEY`.
- Indexer bootstraps from genesis on first run and resumes from last indexed height on restart.
- Repair path checks local consistency and can rewind/reindex if an inconsistency is detected.
