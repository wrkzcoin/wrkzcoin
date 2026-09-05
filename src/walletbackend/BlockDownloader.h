// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <WalletTypes.h>
#include <atomic>
#include <nigel/Nigel.h>
#include <subwallets/SubWallets.h>
#include <tuple>
#include <utilities/ThreadSafeDeque.h>
#include <vector>
#include <walletbackend/SynchronizationStatus.h>

class BlockDownloader
{
  public:
    BlockDownloader() {};

    /* Constructor */
    BlockDownloader(
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t startHeight,
        const uint64_t startTimestamp);

    /* Move constructor */
    BlockDownloader(BlockDownloader &&old);

    /* Move assignment operator */
    BlockDownloader &operator=(BlockDownloader &&old);

    /* Destructor */
    ~BlockDownloader();

    /////////////////////////////
    /* Public member functions */
    /////////////////////////////

    /* Retrieve blockCount blocks from the internal store. does not remove
       them. Returns as many as possible if the amount requested is not
       available. May be empty (this is the norm when synced.) */
    std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> fetchBlocks(const size_t blockCount);

    /* Drops the oldest block from the internal queue */
    void dropBlock(const uint64_t blockHeight, const Crypto::Hash blockHash);

    /* Start block downloading process */
    void start();

    /* Stop block downloading process */
    void stop();

    /* Returns height of processed blocks */
    uint64_t getHeight() const;

    /* {stalled, the height we have covered to, the lowest height the daemon
       can serve}. Set when the two do not meet, which means a stretch of
       chain that neither side can account for. Downloading stops rather than
       stepping over it - see downloadBlocks(). Both heights are zero when
       nothing is wrong. */
    std::tuple<bool, uint64_t, uint64_t> getSyncGap() const;

    void fromJSON(const JSONObject &j, const uint64_t startHeight, const uint64_t startTimestamp);

    nlohmann::json toJSON() const;

    void setSubWallets(const std::shared_ptr<SubWallets> subWallets);

    void initializeAfterLoad(const std::shared_ptr<Nigel> daemon);

    /* Single synchronous download attempt — used in WASM no-thread mode
       instead of the background downloader thread. */
    bool downloadStep();

    /* Re-enable the internal block store without starting the download thread.
       Called by WalletSynchronizer::start() in WASM/single-threaded mode after
       a stop()/start() cycle (e.g. save()) to prevent push_back_n from silently
       discarding downloaded blocks. */
    void startStorageOnly();

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    /* Synchronizes pre-fetching blocks */
    void downloader();

    /* Determines if we should prefetch more blocks */
    bool shouldFetchMoreBlocks() const;

    /* Gets checkpoints of stored (not processed) blocks */
    std::vector<Crypto::Hash> getStoredBlockCheckpoints() const;

    /* Gets checkpoints of stored, processed, and infrequent checkpoints */
    std::vector<Crypto::Hash> getBlockCheckpoints() const;

    /* Downloads a set of blocks, if needed */
    bool downloadBlocks();

    /* Fetches several consecutive height windows at once, so the wait for one
       response overlaps the wait for the next instead of following it. Only
       used a long way behind the chain tip, where the windows cannot straddle
       a reorganisation, and only once downloadBlocks() has established where
       we are. Returns false when it could not run or did not finish a window,
       which sends the caller back to the sequential path. */
    bool downloadBlocksInParallel();

    /* Puts the assembled blocks into the store and updates the running byte
       total. Returns the number added. */
    size_t storeDownloadedBlocks(const std::vector<WalletTypes::WalletBlockInfo> &blocks);

    /* The highest height we hold a block for, processed or merely downloaded.
       This is what the daemon resolves our request to, because the block
       hashes we send it as checkpoints are the hashes of these blocks. Zero
       means we hold nothing and the scan has not begun. */
    uint64_t highestKnownHeight() const;

    /* Records that the chain between the two heights can be covered by
       neither side, and stands the download down until that changes. */
    void recordSyncGap(const uint64_t haveCoveredTo, const uint64_t daemonServesFrom);

    /* Clears a gap previously recorded, for when the daemon that could not
       serve us has been swapped for one that can. */
    void clearSyncGap();

    /* Approximate heap footprint of one stored block */
    static size_t storedBlockMemoryUsage(const std::tuple<WalletTypes::WalletBlockInfo, uint32_t> &block);

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* Cached blocks */
    ThreadSafeDeque<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> m_storedBlocks;

    /* Running total of the approximate memory used by m_storedBlocks. Kept
       incrementally so shouldFetchMoreBlocks() is O(1) - the queue can hold
       hundreds of thousands of blocks, and walking it before every request
       was costing more than the download it was gating. */
    std::atomic<size_t> m_storedBlocksBytes = 0;

    /* The daemon connection */
    std::shared_ptr<Nigel> m_daemon;

    /* Timestamp to begin syncing at */
    uint64_t m_startTimestamp;

    /* Height to begin syncing at */
    uint64_t m_startHeight;

    /* Sync progress */
    SynchronizationStatus m_synchronizationStatus;

    std::shared_ptr<SubWallets> m_subWallets;

    /* For synchronizing block downloading */
    std::mutex m_mutex;

    /* Are we ready to go attempt to retrieve more data */
    std::atomic<bool> m_consumedData = true;

    /* Should we try and fetch more data (Used in conjunction with m_consumedData) */
    std::condition_variable m_shouldTryFetch;

    /* Should we stop downloading */
    std::atomic<bool> m_shouldStop = false;

    /* Thread that performs the actual downloading of blocks */
    std::thread m_downloadThread;

    uint32_t m_arrivalIndex = 0;

    /* The next height the parallel path may ask for. Zero means it is not
       primed: only a checkpoint driven download can say where we are on the
       chain, so the sequential path sets this and any failure clears it,
       sending us back through the path that can recover from a fork.

       Atomic because stop() clears it from the stopping thread while the
       download thread may still be setting it. */
    std::atomic<uint64_t> m_nextDownloadHeight = 0;

    /* Set when the daemon cannot serve the range we still need, so continuing
       would mean recording heights we never scanned. Kept as three atomics
       rather than a struct behind a lock because the status endpoints read
       them from other threads while the downloader writes them.

       Not sticky: every sequential attempt re-derives it, so pointing the
       wallet at a daemon that does hold the range clears it by itself. */
    std::atomic<bool> m_syncGapDetected = false;

    std::atomic<uint64_t> m_syncGapCoveredTo = 0;

    std::atomic<uint64_t> m_syncGapDaemonServesFrom = 0;

    /* How many times in a row a daemon has answered from higher up than the
       next block we asked for, without declaring a lite height that explains
       it.

       A daemon that structurally cannot serve a range says so - it reports a
       lite start height, and that case is caught on its own and stops sync at
       once. This counter is for the other reason the same symptom appears: a
       reorg at the tip dropping the block we were about to ask for, or a
       daemon resolving our checkpoints one block further on than our own
       height accounting. Those are transient and one block wide, and stopping
       a wallet dead over one is out of all proportion - the first time it
       happened here the recorded gap was 4202338 to 4202340, on a chain
       4.2 million blocks long.

       So the anomaly is retried, and only a run of them is treated as real. */
    std::atomic<uint64_t> m_unexplainedStartCount = 0;
};
