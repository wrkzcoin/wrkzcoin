"use strict";

const { notFound } = require("../lib/httpErrors");
const { isNumeric, isHash64, toLowerHex } = require("../utils/parsers");

class BlockService {
    constructor({ rpcClient, cache, cacheTtlMs }) {
        this.rpcClient = rpcClient;
        this.cache = cache;
        this.cacheTtlMs = cacheTtlMs;
    }

    async getBlockById(id) {
        const key = `block:${id}`;
        const cached = this.cache.get(key);

        if (cached) {
            return cached;
        }

        const hash = await this.resolveHash(id);
        const result = await this.rpcClient.callJsonRpc("f_block_json", { hash });
        const block = result.block;

        if (!block) {
            throw notFound("Block not found");
        }

        const normalized = {
            block: {
                hash: block.hash,
                height: block.height,
                timestamp: block.timestamp,
                major_version: block.major_version,
                minor_version: block.minor_version,
                prev_hash: block.prev_hash,
                nonce: block.nonce,
                orphan_status: block.orphan_status,
                depth: block.depth,
                difficulty: block.difficulty,
                reward: block.reward,
                block_size: block.blockSize,
                transactions_cumulative_size: block.transactionsCumulativeSize,
                already_generated_coins: block.alreadyGeneratedCoins,
                already_generated_transactions: block.alreadyGeneratedTransactions,
                size_median: block.sizeMedian,
                effective_size_median: block.effectiveSizeMedian,
                base_reward: block.baseReward,
                penalty: block.penalty,
                total_fee_amount: block.totalFeeAmount
            },
            transactions: (block.transactions || []).map((tx, index) => ({
                hash: tx.hash,
                fee: tx.fee,
                amount_out: tx.amount_out,
                size: tx.size,
                is_coinbase: index === 0
            }))
        };

        this.cache.set(key, normalized, this.cacheTtlMs);
        return normalized;
    }

    async blockExistsByHash(hash) {
        try {
            await this.rpcClient.callJsonRpc("getblockheaderbyhash", { hash });
            return true;
        } catch {
            return false;
        }
    }

    async blockExistsByHeight(height) {
        try {
            await this.rpcClient.callJsonRpc("getblockheaderbyheight", { height });
            return true;
        } catch {
            return false;
        }
    }

    async resolveHash(id) {
        if (isNumeric(id)) {
            const height = Number.parseInt(id, 10);
            const header = await this.rpcClient.callJsonRpc("getblockheaderbyheight", { height });
            const hash = header.block_header && header.block_header.hash;

            if (!hash) {
                throw notFound("Block not found");
            }

            return hash;
        }

        if (isHash64(id)) {
            return toLowerHex(id);
        }

        throw notFound("Block identifier must be a height or 64-char hash");
    }
}

module.exports = BlockService;

