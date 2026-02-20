"use strict";

class NetworkService {
    constructor({ rpcClient, cache, cacheTtlMs }) {
        this.rpcClient = rpcClient;
        this.cache = cache;
        this.cacheTtlMs = cacheTtlMs;
    }

    async getSummary() {
        const cacheKey = "network:summary";
        const cached = this.cache.get(cacheKey);

        if (cached) {
            return cached;
        }

        const info = await this.rpcClient.getInfo();

        const summary = {
            height: info.height,
            network_height: info.network_height,
            synced: info.synced,
            difficulty: info.difficulty,
            hashrate: info.hashrate,
            tx_count: info.tx_count,
            tx_pool_size: info.tx_pool_size,
            alt_blocks_count: info.alt_blocks_count,
            incoming_connections_count: info.incoming_connections_count,
            outgoing_connections_count: info.outgoing_connections_count,
            white_peerlist_size: info.white_peerlist_size,
            grey_peerlist_size: info.grey_peerlist_size,
            sync_active_peers: info.sync_active_peers,
            sync_demoted_peers: info.sync_demoted_peers,
            major_version: info.major_version,
            minor_version: info.minor_version,
            start_time: info.start_time,
            status: info.status
        };

        this.cache.set(cacheKey, summary, this.cacheTtlMs);

        return summary;
    }
}

module.exports = NetworkService;

