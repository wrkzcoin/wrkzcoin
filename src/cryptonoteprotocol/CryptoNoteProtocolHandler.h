// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "cryptonotecore/ICore.h"
#include "cryptonoteprotocol/CryptoNoteProtocolDefinitions.h"
#include "cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h"
#include "cryptonoteprotocol/ICryptoNoteProtocolObserver.h"
#include "cryptonoteprotocol/ICryptoNoteProtocolQuery.h"
#include "p2p/ConnectionContext.h"
#include "p2p/NetNodeCommon.h"
#include "p2p/P2pProtocolDefinitions.h"

#include <atomic>
#include <chrono>
#include <common/ObserverManager.h>
#include <logging/LoggerRef.h>
#include <unordered_map>
#include <unordered_set>

namespace System
{
    class Dispatcher;
}

namespace CryptoNote
{
    class Currency;

    class CryptoNoteProtocolHandler : public ICryptoNoteProtocolHandler
    {
      public:
        CryptoNoteProtocolHandler(
            const Currency &currency,
            System::Dispatcher &dispatcher,
            ICore &rcore,
            IP2pEndpoint *p_net_layout,
            std::shared_ptr<Logging::ILogger> log);

        virtual ~CryptoNoteProtocolHandler() override {};

        virtual bool addObserver(ICryptoNoteProtocolObserver *observer) override;

        virtual bool removeObserver(ICryptoNoteProtocolObserver *observer) override;

        void set_p2p_endpoint(IP2pEndpoint *p2p);

        // ICore& get_core() { return m_core; }
        virtual bool isSynchronized() const override
        {
            return m_synchronized;
        }

        void log_connections();

        // Interface t_payload_net_handler, where t_payload_net_handler is template argument of nodetool::node_server
        void stop();

        bool start_sync(CryptoNoteConnectionContext &context);

        void onConnectionOpened(CryptoNoteConnectionContext &context);

        void onConnectionClosed(CryptoNoteConnectionContext &context);

        CoreStatistics getStatistics();

        bool get_payload_sync_data(CORE_SYNC_DATA &hshd);

        bool
            process_payload_sync_data(const CORE_SYNC_DATA &hshd, CryptoNoteConnectionContext &context, bool is_inital);

        int handleCommand(
            bool is_notify,
            int command,
            const BinaryArray &in_buff,
            BinaryArray &buff_out,
            CryptoNoteConnectionContext &context,
            bool &handled);

        virtual size_t getPeerCount() const override;

        virtual uint32_t getObservedHeight() const override;

        virtual uint32_t getBlockchainHeight() const override;

        virtual bool isPrunedNode() const override;

        virtual uint32_t getPrunedNodeDepth() const override;

        virtual bool isPruneCapabilityActive() const override;

        virtual uint32_t getSyncActivePeers() const override;

        virtual uint32_t getSyncAvgBatchSize() const override;

        virtual uint32_t getSyncDemotedPeers() const override;

        void setPrunedNodeConfig(bool isPrunedNode, uint32_t prunedNodeDepth);

        /* Zero for a normal node. Above zero this is the height from which full
           block data is stored; see LITENODE.md. */
        void setLiteNodeConfig(uint32_t liteHeight);

        virtual uint32_t getLiteNodeHeight() const override;

        void setSyncTuning(
            uint32_t syncMaxPeers,
            uint32_t syncPeerFailureThreshold,
            uint32_t syncBatchMin,
            uint32_t syncBatchMax,
            uint32_t blockSyncSize,
            uint64_t blockSyncBytes);

        void requestMissingPoolTransactions(const CryptoNoteConnectionContext &context);

      private:
        //----------------- commands handlers ----------------------------------------------
        int handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request &arg, CryptoNoteConnectionContext &context);

        int handle_notify_new_transactions(
            int command,
            NOTIFY_NEW_TRANSACTIONS::request &arg,
            CryptoNoteConnectionContext &context);

        int handle_request_get_objects(
            int command,
            NOTIFY_REQUEST_GET_OBJECTS::request &arg,
            CryptoNoteConnectionContext &context);

        int handle_response_get_objects(
            int command,
            NOTIFY_RESPONSE_GET_OBJECTS::request &arg,
            CryptoNoteConnectionContext &context);

        int handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request &arg, CryptoNoteConnectionContext &context);

        int handle_response_chain_entry(
            int command,
            NOTIFY_RESPONSE_CHAIN_ENTRY::request &arg,
            CryptoNoteConnectionContext &context);

        int handleRequestTxPool(
            int command,
            NOTIFY_REQUEST_TX_POOL::request &arg,
            CryptoNoteConnectionContext &context);

        int handle_notify_new_lite_block(
            int command,
            NOTIFY_NEW_LITE_BLOCK::request &arg,
            CryptoNoteConnectionContext &context);

        int handle_notify_missing_txs(
            int command,
            NOTIFY_MISSING_TXS::request &arg,
            CryptoNoteConnectionContext &context);

        //----------------- i_cryptonote_protocol ----------------------------------
        virtual void relayBlock(NOTIFY_NEW_BLOCK::request &arg) override;

        virtual void relayTransactions(const std::vector<BinaryArray> &transactions) override;

        //----------------------------------------------------------------------------------
        uint32_t get_current_blockchain_height() const;

        uint32_t getAdaptiveBatchSize(const CryptoNoteConnectionContext &context) const;

        void onSyncChunkSuccess(CryptoNoteConnectionContext &context, size_t blocks, size_t bytes);

        void onSyncChunkFailure(CryptoNoteConnectionContext &context);

        bool shouldDemoteSyncPeer(const CryptoNoteConnectionContext &context) const;

        bool request_missing_objects(CryptoNoteConnectionContext &context, bool check_having_blocks);

        bool on_connection_synchronized();

        void updateObservedHeight(uint32_t peerHeight, const CryptoNoteConnectionContext &context);

        void recalculateMaxObservedHeight(const CryptoNoteConnectionContext &context);

        void logSyncProgressLocked(uint64_t currentHeight, uint64_t remoteHeight);

        int processObjects(
            CryptoNoteConnectionContext &context,
            std::vector<RawBlock> &&rawBlocks,
            const std::vector<CachedBlock> &cachedBlocks);

        Logging::LoggerRef logger;

      private:
        int doPushLiteBlock(
            NOTIFY_NEW_LITE_BLOCK::request block,
            CryptoNoteConnectionContext &context,
            std::vector<BinaryArray> missingTxs);

      private:
        System::Dispatcher &m_dispatcher;

        ICore &m_core;

        const Currency &m_currency;

        p2p_endpoint_stub m_p2p_stub;

        IP2pEndpoint *m_p2p;

        std::atomic<bool> m_synchronized;

        std::atomic<bool> m_stop;

        mutable std::mutex m_observedHeightMutex;

        uint32_t m_observedHeight;

        mutable std::mutex m_blockchainHeightMutex;

        uint32_t m_blockchainHeight;

        bool m_syncLogInitialized;

        uint64_t m_syncLogStartHeight;

        uint64_t m_lastSyncLogHeight;

        std::chrono::steady_clock::time_point m_syncLogStartTime;

        std::chrono::steady_clock::time_point m_lastSyncLogTime;

        std::atomic<size_t> m_peersCount;

        bool m_isPrunedNode;

        uint32_t m_prunedNodeDepth;

        /* 0 = full node. Above 0, the height this node stores full blocks from. */
        uint32_t m_liteHeight = 0;

        /* The lite height is only safe once we know how tall the network is, and
           that is first knowable at the opening handshake. Checked once. */
        bool m_liteDepthChecked = false;

        uint32_t m_syncMaxPeers;

        uint32_t m_syncPeerFailureThreshold;

        uint32_t m_syncBatchMin;

        uint32_t m_syncBatchMax;

        uint32_t m_syncBlockSyncSize;

        uint64_t m_syncBlockSyncBytes;

        std::atomic<uint32_t> m_syncDemotedPeers;

        /* --- Network-consensus block trust (sync recovery) --- */

        /* Number of independent peers that must reject the same block before
           we trust network consensus and add a dynamic checkpoint. */
        static constexpr uint32_t NETWORK_TRUST_PEER_THRESHOLD = 6;

        /* The block must be at least this many blocks behind the observed
           network height before we consider trusting it. */
        static constexpr uint32_t DEEP_CONFIRMATION_THRESHOLD = 300;

        /* Maps block_hash → set of peer IPs that sent us this block and we rejected it. */
        std::mutex m_networkTrustMutex;
        std::unordered_map<Crypto::Hash, std::unordered_set<uint32_t>> m_rejectedBlockPeers;

        /* Block hashes already promoted to dynamic checkpoints (avoid re-adding). */
        std::unordered_set<Crypto::Hash> m_networkTrustedBlocks;

        Tools::ObserverManager<ICryptoNoteProtocolObserver> m_observerManager;
    };
} // namespace CryptoNote
