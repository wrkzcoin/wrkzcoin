"use strict";

const fs = require("node:fs");
const path = require("node:path");
const Database = require("better-sqlite3");

class SqliteStore {
    constructor(dbPath) {
        const resolvedPath = path.resolve(process.cwd(), dbPath);
        const dir = path.dirname(resolvedPath);
        fs.mkdirSync(dir, { recursive: true });

        this.db = new Database(resolvedPath);
        this.db.pragma("journal_mode = WAL");
        this.db.pragma("synchronous = NORMAL");

        this.initSchema();
        this.prepareStatements();
    }

    initSchema() {
        this.db.exec(`
            CREATE TABLE IF NOT EXISTS metadata (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS indexed_blocks (
                height INTEGER PRIMARY KEY,
                hash TEXT NOT NULL UNIQUE,
                timestamp INTEGER NOT NULL,
                difficulty INTEGER NOT NULL,
                tx_count INTEGER NOT NULL,
                block_size INTEGER NOT NULL,
                reward INTEGER NOT NULL,
                orphan_status INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_indexed_blocks_hash ON indexed_blocks(hash);

            CREATE TABLE IF NOT EXISTS tx_payment_ids (
                tx_hash TEXT PRIMARY KEY,
                payment_id TEXT NOT NULL,
                payment_id_short TEXT,
                block_height INTEGER NOT NULL,
                block_hash TEXT NOT NULL,
                timestamp INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_tx_payment_ids_payment_id ON tx_payment_ids(payment_id);
            CREATE INDEX IF NOT EXISTS idx_tx_payment_ids_payment_id_short ON tx_payment_ids(payment_id_short);
            CREATE INDEX IF NOT EXISTS idx_tx_payment_ids_block_height ON tx_payment_ids(block_height);
        `);
    }

    prepareStatements() {
        this.stmtGetMeta = this.db.prepare("SELECT value FROM metadata WHERE key = ?");
        this.stmtSetMeta = this.db.prepare(`
            INSERT INTO metadata(key, value)
            VALUES(?, ?)
            ON CONFLICT(key) DO UPDATE SET value = excluded.value
        `);
        this.stmtGetBlockByHeight = this.db.prepare("SELECT * FROM indexed_blocks WHERE height = ?");
        this.stmtGetBlockByHash = this.db.prepare("SELECT * FROM indexed_blocks WHERE hash = ?");
        this.stmtGetTip = this.db.prepare("SELECT * FROM indexed_blocks ORDER BY height DESC LIMIT 1");
        this.stmtDeleteAbove = this.db.prepare("DELETE FROM indexed_blocks WHERE height > ?");
        this.stmtDeletePaymentIdsAbove = this.db.prepare("DELETE FROM tx_payment_ids WHERE block_height > ?");
        this.stmtCountBlocks = this.db.prepare("SELECT COUNT(*) AS c FROM indexed_blocks");
        this.stmtCountPaymentIds = this.db.prepare("SELECT COUNT(*) AS c FROM tx_payment_ids");
        this.stmtUpsertBlock = this.db.prepare(`
            INSERT INTO indexed_blocks(
                height, hash, timestamp, difficulty, tx_count, block_size, reward, orphan_status, updated_at
            )
            VALUES(@height, @hash, @timestamp, @difficulty, @tx_count, @block_size, @reward, @orphan_status, @updated_at)
            ON CONFLICT(height) DO UPDATE SET
                hash = excluded.hash,
                timestamp = excluded.timestamp,
                difficulty = excluded.difficulty,
                tx_count = excluded.tx_count,
                block_size = excluded.block_size,
                reward = excluded.reward,
                orphan_status = excluded.orphan_status,
                updated_at = excluded.updated_at
        `);
        this.stmtUpsertPaymentId = this.db.prepare(`
            INSERT INTO tx_payment_ids(
                tx_hash, payment_id, payment_id_short, block_height, block_hash, timestamp
            )
            VALUES(@tx_hash, @payment_id, @payment_id_short, @block_height, @block_hash, @timestamp)
            ON CONFLICT(tx_hash) DO UPDATE SET
                payment_id = excluded.payment_id,
                payment_id_short = excluded.payment_id_short,
                block_height = excluded.block_height,
                block_hash = excluded.block_hash,
                timestamp = excluded.timestamp
        `);
        this.stmtFindByPaymentId = this.db.prepare(`
            SELECT tx_hash, payment_id, payment_id_short, block_height, block_hash, timestamp
            FROM tx_payment_ids
            WHERE payment_id = @pid OR payment_id_short = @pid
            ORDER BY block_height DESC
            LIMIT @limit
        `);
    }

    getMeta(key) {
        const row = this.stmtGetMeta.get(key);
        return row ? row.value : null;
    }

    setMeta(key, value) {
        this.stmtSetMeta.run(key, String(value));
    }

    getBlockByHeight(height) {
        return this.stmtGetBlockByHeight.get(height) || null;
    }

    getBlockByHash(hash) {
        return this.stmtGetBlockByHash.get(hash) || null;
    }

    getTipBlock() {
        return this.stmtGetTip.get() || null;
    }

    getBlockCount() {
        return this.stmtCountBlocks.get().c;
    }

    getPaymentIdCount() {
        return this.stmtCountPaymentIds.get().c;
    }

    upsertBlock(block) {
        this.stmtUpsertBlock.run({
            ...block,
            updated_at: Date.now()
        });
    }

    upsertBlocks(blocks) {
        const tx = this.db.transaction((items) => {
            for (const item of items) {
                this.upsertBlock(item);
            }
        });
        tx(blocks);
    }

    upsertPaymentIdRows(rows) {
        if (!rows || rows.length === 0) {
            return;
        }
        const tx = this.db.transaction((items) => {
            for (const item of items) {
                this.stmtUpsertPaymentId.run(item);
            }
        });
        tx(rows);
    }

    findTxByPaymentId(paymentId, limit) {
        return this.stmtFindByPaymentId.all({
            pid: paymentId,
            limit: Number(limit)
        });
    }

    deleteBlocksAbove(height) {
        this.stmtDeleteAbove.run(height);
        this.stmtDeletePaymentIdsAbove.run(height);
    }

    clearAllBlocks() {
        this.db.exec("DELETE FROM indexed_blocks");
        this.db.exec("DELETE FROM tx_payment_ids");
    }
}

module.exports = SqliteStore;
