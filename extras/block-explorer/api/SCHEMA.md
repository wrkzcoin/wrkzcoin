# SQLite Schema

Location is configured by `SQLITE_DB_PATH` (default `./data/explorer-index.db`).

## Pragmas

- `journal_mode = WAL`
- `synchronous = NORMAL`

## Tables

### `metadata`

Key/value storage for indexer state.

| Column | Type | Constraints | Description |
| --- | --- | --- | --- |
| `key` | `TEXT` | `PRIMARY KEY` | Metadata key name |
| `value` | `TEXT` | `NOT NULL` | Metadata value as string |

Common keys used:

- `last_indexed_height`
- `last_indexed_hash`

### `indexed_blocks`

Lightweight block index used for fast block/search status.

| Column | Type | Constraints | Description |
| --- | --- | --- | --- |
| `height` | `INTEGER` | `PRIMARY KEY` | Block height |
| `hash` | `TEXT` | `NOT NULL`, `UNIQUE` | Block hash (64 hex) |
| `timestamp` | `INTEGER` | `NOT NULL` | Block timestamp (unix seconds) |
| `difficulty` | `INTEGER` | `NOT NULL` | Block difficulty |
| `tx_count` | `INTEGER` | `NOT NULL` | Number of tx in block |
| `block_size` | `INTEGER` | `NOT NULL` | Serialized block size |
| `reward` | `INTEGER` | `NOT NULL` | Block reward (atomic units) |
| `orphan_status` | `INTEGER` | `NOT NULL` | `0` main chain, `1` orphan/alt |
| `updated_at` | `INTEGER` | `NOT NULL` | Last update time (unix ms) |

### `tx_payment_ids`

Payment ID lookup map for tx search by short/long payment ID.

| Column | Type | Constraints | Description |
| --- | --- | --- | --- |
| `tx_hash` | `TEXT` | `PRIMARY KEY` | Transaction hash (64 hex) |
| `payment_id` | `TEXT` | `NOT NULL` | Full payment ID as stored |
| `payment_id_short` | `TEXT` | nullable | Short payment ID (16 hex), when available |
| `block_height` | `INTEGER` | `NOT NULL` | Block height containing tx |
| `block_hash` | `TEXT` | `NOT NULL` | Block hash containing tx |
| `timestamp` | `INTEGER` | `NOT NULL` | Block timestamp (unix seconds) |

## Indexes

### `indexed_blocks`

- `idx_indexed_blocks_hash` on (`hash`)

### `tx_payment_ids`

- `idx_tx_payment_ids_payment_id` on (`payment_id`)
- `idx_tx_payment_ids_payment_id_short` on (`payment_id_short`)
- `idx_tx_payment_ids_block_height` on (`block_height`)

## Query patterns

- Block by height:
  - `SELECT * FROM indexed_blocks WHERE height = ?`
- Block by hash:
  - `SELECT * FROM indexed_blocks WHERE hash = ?`
- Current indexed tip:
  - `SELECT * FROM indexed_blocks ORDER BY height DESC LIMIT 1`
- Find txs by payment ID:
  - `SELECT ... FROM tx_payment_ids WHERE payment_id = ? OR payment_id_short = ? ORDER BY block_height DESC LIMIT ?`

## Repair behavior

- If local inconsistency is detected at tip:
  - delete rows above last common height from `indexed_blocks`
  - delete rows above same height from `tx_payment_ids`
  - continue reindex from next height

