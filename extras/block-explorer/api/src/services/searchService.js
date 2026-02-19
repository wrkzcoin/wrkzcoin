"use strict";

const { badRequest, notFound } = require("../lib/httpErrors");
const { isNumeric, isHash64, isPaymentIdShort, toLowerHex } = require("../utils/parsers");

class SearchService {
    constructor({ blockService, txService, indexerService }) {
        this.blockService = blockService;
        this.txService = txService;
        this.indexerService = indexerService;
    }

    async search(query) {
        const q = (query || "").trim();

        if (!q) {
            throw badRequest("Query parameter 'q' is required");
        }
        if (q.length > 128) {
            throw badRequest("Query parameter 'q' is too long");
        }

        if (isNumeric(q)) {
            const height = Number.parseInt(q, 10);
            const indexed = this.indexerService ? this.indexerService.getIndexedBlockByHeight(height) : null;
            const exists = indexed ? true : await this.blockService.blockExistsByHeight(height);

            if (!exists) {
                throw notFound("Block height not found");
            }

            return {
                query: q,
                type: "block",
                target: `/api/v1/block/${q}`
            };
        }

        if (isHash64(q)) {
            const hash = toLowerHex(q);
            const indexed = this.indexerService ? this.indexerService.getIndexedBlockByHash(hash) : null;
            const isBlock = indexed ? true : await this.blockService.blockExistsByHash(hash);

            if (isBlock) {
                return {
                    query: hash,
                    type: "block",
                    target: `/api/v1/block/${hash}`
                };
            }

            const isTx = await this.txService.txExists(hash);

            if (isTx) {
                return {
                    query: hash,
                    type: "tx",
                    target: `/api/v1/tx/${hash}`
                };
            }

            const hasPaymentId = this.txService.hasPaymentId(hash);
            if (hasPaymentId) {
                return {
                    query: hash,
                    type: "payment_id",
                    target: `/api/v1/tx/by-payment-id/${hash}`
                };
            }

            throw notFound("No block or transaction found for this hash");
        }

        if (isPaymentIdShort(q)) {
            const pid = toLowerHex(q);
            const hasPaymentId = this.txService.hasPaymentId(pid);

            if (!hasPaymentId) {
                throw notFound("Payment ID not found in index");
            }

            return {
                query: pid,
                type: "payment_id",
                target: `/api/v1/tx/by-payment-id/${pid}`
            };
        }

        throw badRequest("Query must be a block height, block/tx hash, or short/long payment ID");
    }
}

module.exports = SearchService;
