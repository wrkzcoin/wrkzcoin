"use strict";

const { badRequest, notFound } = require("../lib/httpErrors");
const { isHash64, normalizePaymentId, toLowerHex } = require("../utils/parsers");

class TxService {
    constructor({ rpcClient, cache, cacheTtlMs, store }) {
        this.rpcClient = rpcClient;
        this.cache = cache;
        this.cacheTtlMs = cacheTtlMs;
        this.store = store;
    }

    async getTxByHash(hash) {
        if (!isHash64(hash)) {
            throw badRequest("Transaction hash must be 64-char hex");
        }

        const normalizedHash = toLowerHex(hash);
        const key = `tx:${normalizedHash}`;
        const cached = this.cache.get(key);

        if (cached) {
            return cached;
        }

        const result = await this.rpcClient.callJsonRpc("f_transaction_json", { hash: normalizedHash });

        if (!result.tx || !result.txDetails) {
            throw notFound("Transaction not found");
        }

        const payload = {
            block: {
                hash: result.block.hash,
                height: result.block.height,
                timestamp: result.block.timestamp,
                difficulty: result.block.difficulty,
                cumul_size: result.block.cumul_size,
                tx_count: result.block.tx_count
            },
            tx: {
                hash: result.txDetails.hash,
                version: result.tx.version,
                unlock_time: result.tx.unlock_time,
                extra: result.tx.extra,
                amount_out: result.txDetails.amount_out,
                fee: result.txDetails.fee,
                mixin: result.txDetails.mixin,
                payment_id: result.txDetails.paymentId,
                size: result.txDetails.size
            },
            vin: (result.tx.vin || []).map((item) => ({
                type: item.type,
                amount: item.value && item.value.amount,
                k_image: item.value && item.value.k_image,
                key_offsets: item.value && item.value.key_offsets ? item.value.key_offsets : [],
                height: item.value && item.value.height
            })),
            vout: (result.tx.vout || []).map((item) => ({
                amount: item.amount,
                key: item.target && item.target.data ? item.target.data.key : null
            }))
        };

        this.cache.set(key, payload, this.cacheTtlMs);
        return payload;
    }

    async txExists(hash) {
        try {
            await this.getTxByHash(hash);
            return true;
        } catch {
            return false;
        }
    }

    getTxByPaymentId(paymentId, limit = 100) {
        const normalized = normalizePaymentId(paymentId);
        if (!normalized) {
            throw badRequest("Payment ID must be 16 or 64-char hex");
        }

        const lookupKey = normalized.short || normalized.long;
        const rows = this.store.findTxByPaymentId(lookupKey, Math.max(1, Math.min(Number(limit) || 100, 500)));

        return {
            payment_id_query: lookupKey,
            count: rows.length,
            transactions: rows.map((row) => ({
                tx_hash: row.tx_hash,
                payment_id: row.payment_id,
                payment_id_short: row.payment_id_short,
                block_height: row.block_height,
                block_hash: row.block_hash,
                timestamp: row.timestamp
            }))
        };
    }

    hasPaymentId(paymentId) {
        const result = this.getTxByPaymentId(paymentId, 1);
        return result.count > 0;
    }
}

module.exports = TxService;
