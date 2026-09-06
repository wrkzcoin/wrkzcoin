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

        /* The connections table log_connections writes to the log, as text. */
        std::string connections_to_string();

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

        /* Fluffs any transaction whose Dandelion++ stem phase has run out of
           time. Driven from the p2p idle worker, which ticks once a second. */
        void processDandelionEmbargo();

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

        uint32_t getPeerServingFloor(const CryptoNoteConnectionContext &context) const;

        bool peerCanServeOurChain(const CryptoNoteConnectionContext &context) const;

        bool requestChainIfPeerCanServe(CryptoNoteConnectionContext &context, const std::string &reason);

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
           that is first knowable at the opening handshake. Set once the question
           is settled, either way, and never revisited. */
        bool m_liteDepthChecked = false;

        /* Tallest chain any peer has claimed so far. A max, so a peer reporting a
           short chain - honestly or otherwise - cannot drag the answer down. */
        uint64_t m_liteMaxPeerHeight = 0;

        /* How many peers have contributed to the above. The verdict that kills the
           daemon waits for several, so one peer cannot deliver it alone. */
        uint32_t m_liteDepthSamples = 0;

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

        /* --- Dandelion++ stem relay --- */

        /* Plain flood relay tells anyone watching enough connections which node
           a transaction started at, because the first node to announce it is
           almost always its author. Dandelion++ splits relay into two phases: a
           transaction first travels a short private path, one peer per hop
           (stem), and only then gets broadcast to everyone (fluff). An observer
           sees the transaction appear first at whichever node fluffed it, which
           is not the node that made it.
           This rides the existing NOTIFY_NEW_TRANSACTIONS message, so no new
           command and no version gate is needed, and a peer that knows nothing
           about any of this simply fluffs immediately - that costs privacy for
           one transaction and never correctness. */

        /* How long a node keeps the same stem peer and the same role. Re-rolling
           per transaction instead would let an observer average the choices away
           and recover the origin. */
        static constexpr uint64_t DANDELION_EPOCH_SECONDS = 600;

        /* How long to wait for a stemmed transaction to come back to us as a
           fluff before assuming the stem died and broadcasting it ourselves.
           This is a failure timeout, not a per-hop delay: a healthy stem
           forwards at network speed and never reaches it.
           Kept short on purpose. A transaction on this chain is only valid for
           about twenty blocks after the tip its sender saw, because the wallet
           sets an unlock time of that tip plus a fixed offset and the daemon
           demands the unlock time stay ahead of the height it is mined at. The
           whole stem plus this timeout has to fit inside that window with room
           to spare, so seconds are the right unit here, not minutes. */
        static constexpr uint64_t DANDELION_EMBARGO_SECONDS = 30;

        /* Chance, per epoch, that this node stems rather than fluffs. Some nodes
           must fluff or nothing ever reaches the whole network. */
        static constexpr uint32_t DANDELION_STEM_PERCENT = 90;

        /* Derives the transaction hash from a relayed blob, the same way the
           pool does, so an embargoed transaction can be recognised again.
           Returns false for a blob that will not deserialize. Takes no locks, so
           it is safe to call with m_dandelionMutex held. */
        static bool transactionHashFromBlob(const BinaryArray &blob, Crypto::Hash &hash);

        /* Sends to the single stem peer while this node is stemming, and to
           everyone otherwise, marking the message either way so the receiver
           knows which it is looking at.
           mayStem says whether these transactions are still private: true for
           one of ours and for one that reached us on a stem, false for anything
           that arrived by broadcast, which must be passed on by broadcast. */
        void relayOrStemTransactions(
            NOTIFY_NEW_TRANSACTIONS::request &arg,
            const std::array<uint8_t, 16> *excludeConnection,
            const bool mayStem);

        /* The outbound, fully connected peers a stem may be handed to. Outbound
           only: an inbound connection may be the observer, and picking it would
           give them every stemmed transaction directly. Peers that understand
           the stem flag are preferred, since only they can carry the path any
           further; the older ones are offered only when there is no other
           choice. */
        void outboundStemCandidates(std::vector<std::array<uint8_t, 16>> &candidates);

        /* Ends the stem phase of any of these transactions that we are still
           holding, because a peer has just broadcast them to us and they are
           therefore already public - there is nothing left to wait for and
           nothing left to hide. Called only for a broadcast, never for a stem.
           `from` is the peer the broadcast came in on. */
        void cancelStemEmbargo(const std::vector<BinaryArray> &txs, const std::array<uint8_t, 16> &from);

        mutable std::mutex m_dandelionMutex;

        std::chrono::steady_clock::time_point m_dandelionEpochEnd {};

        bool m_dandelionEpochIsStem = false;

        bool m_dandelionHaveStemPeer = false;

        std::array<uint8_t, 16> m_dandelionStemPeer {};

        struct DandelionEmbargo
        {
            /* When we give up on the stem and broadcast the transaction here. */
            std::chrono::steady_clock::time_point fluffAt;

            /* The peer we handed the transaction to. Remembered so that peer
               alone cannot end the wait by handing it straight back: a stem peer
               that wanted the transaction suppressed could otherwise return it to
               us, cancel the fallback broadcast and then drop it, and nobody else
               would ever see it. A broadcast from any other peer is real
               evidence the transaction got out. */
            std::array<uint8_t, 16> stemPeer;
        };

        /* Transactions in their stem phase, and when each stops waiting. Also
           the set held back from peer-facing pool listings and from the pool
           hashes we offer a peer at connection setup. */
        std::unordered_map<Crypto::Hash, DandelionEmbargo> m_dandelionEmbargo;

        Tools::ObserverManager<ICryptoNoteProtocolObserver> m_observerManager;
    };
} // namespace CryptoNote
