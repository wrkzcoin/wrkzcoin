"use strict";

class IndexerService {
    constructor({
        store,
        localRpcClient,
        remoteRpcClient,
        enabled,
        indexPaymentIds,
        batchSize,
        pollIntervalMs,
        logger
    }) {
        this.store = store;
        this.localRpcClient = localRpcClient;
        this.remoteRpcClient = remoteRpcClient;
        this.enabled = enabled;
        this.indexPaymentIds = indexPaymentIds;
        this.batchSize = Math.max(1, batchSize);
        this.pollIntervalMs = Math.max(2000, pollIntervalMs);
        this.logger = logger;
        this.timer = null;
        this.syncing = false;
        this.lastSyncError = null;
        this.lastSyncAt = null;
        this.lastKnownLocalTipHeight = null;
    }

    async start() {
        if (!this.enabled) {
            return;
        }

        await this.syncOnce();

        this.timer = setInterval(async () => {
            try {
                await this.syncOnce();
            } catch (err) {
                this.logger.error({ err }, "indexer sync failed");
            }
        }, this.pollIntervalMs);
    }

    stop() {
        if (this.timer) {
            clearInterval(this.timer);
            this.timer = null;
        }
    }

    async syncOnce() {
        if (!this.enabled || this.syncing) {
            return;
        }

        this.syncing = true;
        this.lastSyncError = null;

        try {
            const tipHeader = await this.localRpcClient.getLastBlockHeader();
            const tipHeight = tipHeader.height;
            this.lastKnownLocalTipHeight = tipHeight;
            const lastIndexed = this.store.getTipBlock();
            let nextHeight = lastIndexed ? (lastIndexed.height + 1) : 0;

            while (nextHeight <= tipHeight) {
                const end = Math.min(nextHeight + this.batchSize - 1, tipHeight);
                const batch = [];

                for (let height = nextHeight; height <= end; height += 1) {
                    const header = await this.localRpcClient.getBlockHeaderByHeight(height);
                    batch.push({
                        height: header.height,
                        hash: header.hash,
                        timestamp: header.timestamp,
                        difficulty: header.difficulty,
                        tx_count: header.num_txes,
                        block_size: header.block_size,
                        reward: header.reward,
                        orphan_status: header.orphan_status ? 1 : 0
                    });
                }

                this.store.upsertBlocks(batch);
                if (this.indexPaymentIds) {
                    await this.indexBatchPaymentIds(batch);
                }
                this.store.setMeta("last_indexed_height", String(end));
                this.store.setMeta("last_indexed_hash", batch[batch.length - 1].hash);
                nextHeight = end + 1;
            }

            this.lastSyncAt = Date.now();
        } catch (err) {
            this.lastSyncError = err.message;
            throw err;
        } finally {
            this.syncing = false;
        }
    }

    async checkConsistency({ repair = false } = {}) {
        const status = {
            consistent_with_local: true,
            local_issue: null,
            repaired: false,
            repair_from_height: null,
            compared_with_remote: false,
            consistent_with_remote: null,
            remote_issue: null
        };

        const tip = this.store.getTipBlock();
        if (!tip) {
            return status;
        }

        try {
            const localHeader = await this.localRpcClient.getBlockHeaderByHeight(tip.height);
            if (localHeader.hash !== tip.hash) {
                status.consistent_with_local = false;
                status.local_issue = `Hash mismatch at height ${tip.height}`;

                if (repair) {
                    const commonHeight = await this.findCommonHeight();
                    if (commonHeight < 0) {
                        this.store.clearAllBlocks();
                        this.store.setMeta("last_indexed_height", "-1");
                        this.store.setMeta("last_indexed_hash", "");
                        status.repair_from_height = 0;
                    } else {
                        this.store.deleteBlocksAbove(commonHeight);
                        const row = this.store.getBlockByHeight(commonHeight);
                        this.store.setMeta("last_indexed_height", String(commonHeight));
                        this.store.setMeta("last_indexed_hash", row ? row.hash : "");
                        status.repair_from_height = commonHeight + 1;
                    }

                    await this.syncOnce();
                    status.repaired = true;
                    status.consistent_with_local = true;
                    status.local_issue = null;
                }
            }
        } catch (err) {
            status.consistent_with_local = false;
            status.local_issue = err.message;
        }

        if (this.remoteRpcClient) {
            status.compared_with_remote = true;
            try {
                const localTip = await this.localRpcClient.getLastBlockHeader();
                const remoteTip = await this.remoteRpcClient.getLastBlockHeader();
                const compareHeight = Math.min(localTip.height, remoteTip.height);
                const localAt = await this.localRpcClient.getBlockHeaderByHeight(compareHeight);
                const remoteAt = await this.remoteRpcClient.getBlockHeaderByHeight(compareHeight);
                status.consistent_with_remote = localAt.hash === remoteAt.hash;
                if (!status.consistent_with_remote) {
                    status.remote_issue = `Divergence at or before height ${compareHeight}`;
                }
            } catch (err) {
                status.consistent_with_remote = false;
                status.remote_issue = err.message;
            }
        }

        return status;
    }

    async findCommonHeight() {
        const localTip = await this.localRpcClient.getLastBlockHeader();
        const indexedTip = this.store.getTipBlock();
        let height = Math.min(localTip.height, indexedTip.height);

        while (height >= 0) {
            const indexed = this.store.getBlockByHeight(height);
            if (!indexed) {
                height -= 1;
                continue;
            }

            const header = await this.localRpcClient.getBlockHeaderByHeight(height);
            if (header.hash === indexed.hash) {
                return height;
            }
            height -= 1;
        }

        return -1;
    }

    async getStatus({ includeDaemon = false } = {}) {
        const indexedTip = this.store.getTipBlock();
        let localTipHeight = this.lastKnownLocalTipHeight;

        if (includeDaemon) {
            try {
                const localTip = await this.localRpcClient.getLastBlockHeader();
                localTipHeight = localTip.height;
                this.lastKnownLocalTipHeight = localTipHeight;
            } catch (_err) {
                localTipHeight = null;
            }
        }

        return {
            enabled: this.enabled,
            index_payment_ids: this.indexPaymentIds,
            syncing: this.syncing,
            indexed_block_count: this.store.getBlockCount(),
            indexed_payment_id_count: this.store.getPaymentIdCount(),
            indexed_tip_height: indexedTip ? indexedTip.height : null,
            indexed_tip_hash: indexedTip ? indexedTip.hash : null,
            local_tip_height: localTipHeight,
            lag: localTipHeight !== null && indexedTip ? (localTipHeight - indexedTip.height) : null,
            last_sync_at: this.lastSyncAt,
            last_sync_error: this.lastSyncError
        };
    }

    getIndexedBlockByHeight(height) {
        return this.store.getBlockByHeight(height);
    }

    getIndexedBlockByHash(hash) {
        return this.store.getBlockByHash(hash);
    }

    async indexBatchPaymentIds(blocks) {
        for (const block of blocks) {
            const details = await this.localRpcClient.callJsonRpc("f_block_json", { hash: block.hash });
            const txs = details && details.block && details.block.transactions ? details.block.transactions : [];
            const rows = [];

            for (const tx of txs) {
                if (!tx.hash) {
                    continue;
                }
                const txResult = await this.localRpcClient.callJsonRpc("f_transaction_json", { hash: tx.hash });
                const pid = txResult && txResult.txDetails ? txResult.txDetails.paymentId : "";
                if (!pid) {
                    continue;
                }
                rows.push({
                    tx_hash: tx.hash.toLowerCase(),
                    payment_id: pid.toLowerCase(),
                    payment_id_short: pid.length === 16 ? pid.toLowerCase() : null,
                    block_height: block.height,
                    block_hash: block.hash.toLowerCase(),
                    timestamp: block.timestamp
                });
            }

            this.store.upsertPaymentIdRows(rows);
        }
    }
}

module.exports = IndexerService;
