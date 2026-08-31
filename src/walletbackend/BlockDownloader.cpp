// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////////////////////
#include <walletbackend/BlockDownloader.h>
//////////////////////////////////////////

#include <JsonHelper.h>
#include <algorithm>
#include <config/Config.h>
#include <config/WalletConfig.h>
#include <future>
#include <logger/Logger.h>
#include <sstream>
#include <utilities/FormatTools.h>
#include <utilities/Utilities.h>
#include <walletbackend/Constants.h>

/* Constructor */
BlockDownloader::BlockDownloader(
    const std::shared_ptr<Nigel> daemon,
    const std::shared_ptr<SubWallets> subWallets,
    const uint64_t startHeight,
    const uint64_t startTimestamp):

    m_daemon(daemon),
    m_subWallets(subWallets),
    m_startHeight(startHeight),
    m_startTimestamp(startTimestamp)
{
}

/* Move constructor */
BlockDownloader::BlockDownloader(BlockDownloader &&old)
{
    /* Call the move assignment operator */
    *this = std::move(old);
}

/* Move assignment operator */
BlockDownloader &BlockDownloader::operator=(BlockDownloader &&old)
{
    /* Stop any running threads */
    stop();

    m_storedBlocks = std::move(old.m_storedBlocks);

    m_storedBlocksBytes = old.m_storedBlocksBytes.load();

    m_arrivalIndex = old.m_arrivalIndex;

    m_nextDownloadHeight = old.m_nextDownloadHeight.load();

    m_syncGapDetected = old.m_syncGapDetected.load();

    m_syncGapCoveredTo = old.m_syncGapCoveredTo.load();

    m_syncGapDaemonServesFrom = old.m_syncGapDaemonServesFrom.load();

    m_daemon = std::move(old.m_daemon);

    m_startTimestamp = std::move(old.m_startTimestamp);
    m_startHeight = std::move(old.m_startHeight);

    m_synchronizationStatus = std::move(old.m_synchronizationStatus);

    m_consumedData = std::move(old.m_consumedData.load());

    m_shouldStop = std::move(old.m_shouldStop.load());

    return *this;
}

/* Destructor */
BlockDownloader::~BlockDownloader()
{
    stop();
}

void BlockDownloader::start()
{
    m_shouldStop = false;
    m_storedBlocks.start();
    m_downloadThread = std::thread(&BlockDownloader::downloader, this);
}

void BlockDownloader::stop()
{
    /* Where we are on the chain is only ever established by a checkpoint
       driven request, and a restart may be resuming from a different place
       than we stopped at. Make the first download after this go through that
       path again rather than trusting a cursor from before the stop. */
    m_nextDownloadHeight = 0;

    m_shouldStop = true;
    m_consumedData = true;
    m_shouldTryFetch.notify_one();
    m_storedBlocks.stop();

    /* The download thread is very likely sitting in an HTTP request, and a
       daemon is given tens of seconds to answer one. Cut the socket so it
       returns now: a wallet saves by stopping the synchronizer, and waiting
       out a read timeout for every save is exactly the stall that stopping
       without a teardown was meant to avoid. */
    if (m_daemon != nullptr)
    {
        m_daemon->abortInFlightRequests();
    }

    if (m_downloadThread.joinable())
    {
        m_downloadThread.join();
    }
}

uint64_t BlockDownloader::getHeight() const
{
    return m_synchronizationStatus.getHeight();
}

std::tuple<bool, uint64_t, uint64_t> BlockDownloader::getSyncGap() const
{
    if (!m_syncGapDetected)
    {
        return {false, 0, 0};
    }

    return {true, m_syncGapCoveredTo.load(), m_syncGapDaemonServesFrom.load()};
}

uint64_t BlockDownloader::highestKnownHeight() const
{
    const auto storedHeights = m_storedBlocks.back_n_transform(
        1, [](const std::tuple<WalletTypes::WalletBlockInfo, uint32_t> &block) {
            return std::get<0>(block).blockHeight;
        });

    const uint64_t processed = m_synchronizationStatus.getHeight();

    return storedHeights.empty() ? processed : std::max(processed, storedHeights.front());
}

void BlockDownloader::recordSyncGap(const uint64_t haveCoveredTo, const uint64_t daemonServesFrom)
{
    const bool alreadyReported = m_syncGapDetected.exchange(true);

    m_syncGapCoveredTo = haveCoveredTo;
    m_syncGapDaemonServesFrom = daemonServesFrom;

    /* Where we are on the chain is no longer something we can vouch for, so
       the parallel path must not carry on from a cursor set before this. */
    m_nextDownloadHeight = 0;

    /* Nothing about the situation changes until the daemon does, so say it
       once rather than every five seconds for as long as the wallet is open. */
    if (!alreadyReported)
    {
        Logger::logger.log(
            "Sync stopped. This wallet has scanned to height " + std::to_string(haveCoveredTo)
                + ", but this daemon holds nothing below height " + std::to_string(daemonServesFrom)
                + ". Continuing would record those blocks as scanned without ever looking at them, so any "
                  "transaction between the two heights would be silently missing and the balance would be "
                  "too low. Connect a daemon that holds the whole chain, or reset this wallet to "
                + std::to_string(daemonServesFrom)
                + " and accept that transactions below that height cannot be found here.",
            Logger::FATAL,
            {Logger::SYNC, Logger::DAEMON});
    }
}

void BlockDownloader::clearSyncGap()
{
    if (m_syncGapDetected.exchange(false))
    {
        Logger::logger.log(
            "The daemon now holds the range this wallet still needs. Resuming sync.",
            Logger::INFO,
            {Logger::SYNC, Logger::DAEMON});
    }

    m_syncGapCoveredTo = 0;
    m_syncGapDaemonServesFrom = 0;
}

void BlockDownloader::downloader()
{
    while (!m_shouldStop)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_shouldTryFetch.wait(lock, [&] {
                if (m_shouldStop)
                {
                    return true;
                }

                return m_consumedData.load();
            });
        }

        if (m_shouldStop)
        {
            break;
        }

        while (shouldFetchMoreBlocks() && !m_shouldStop)
        {
            /* Once the sequential path has established where on the chain we
               are, and while that is far enough behind the tip that no window
               can straddle a reorganisation, fetch several windows at once.
               Falling back is always safe: the sequential path re-derives our
               position from the block hashes we hold. */
            const bool blocksDownloaded = downloadBlocksInParallel() || downloadBlocks();

            if (!blocksDownloaded)
            {
                /* A rate limited daemon will keep rejecting us for the rest of
                   its current window, and every rejected request burns another
                   slot. Wait out a larger part of the window instead of
                   retrying on the standard interval. */
                const auto backoff = m_daemon->lastRequestWasRateLimited()
                    ? std::chrono::seconds(20)
                    : std::chrono::seconds(5);

                Utilities::sleepUnlessStopping(backoff, m_shouldStop);
                break;
            }
        }

        m_consumedData = false;
    }
}

size_t BlockDownloader::storedBlockMemoryUsage(const std::tuple<WalletTypes::WalletBlockInfo, uint32_t> &block)
{
    return std::get<0>(block).memoryUsage();
}

bool BlockDownloader::shouldFetchMoreBlocks() const
{
    const size_t ramUsage = m_storedBlocksBytes.load();

    if (ramUsage + WalletConfig::maxBodyResponseSize < WalletConfig::blockStoreMemoryLimit)
    {
        if (Logger::logger.shouldLog(Logger::DEBUG))
        {
            std::stringstream stream;

            stream << "Approximate ram usage of stored blocks: " << Utilities::prettyPrintBytes(ramUsage)
                   << ", fetching more.";

            Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::SYNC});
        }

        return true;
    }

    return false;
}

void BlockDownloader::dropBlock(const uint64_t blockHeight, const Crypto::Hash blockHash)
{
    const auto dropped = m_storedBlocks.pop_front_and_get();

    const size_t droppedBytes = storedBlockMemoryUsage(dropped);

    /* Guard against the counter going negative if a block was stored before
       tracking began (e.g. across a move) - saturate at zero instead. */
    size_t expected = m_storedBlocksBytes.load();

    while (!m_storedBlocksBytes.compare_exchange_weak(expected, expected > droppedBytes ? expected - droppedBytes : 0))
    {
    }

    m_synchronizationStatus.storeBlockHash(blockHash, blockHeight);

    /* Indicate to the downloader that it should try and download more */
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consumedData = true;
    m_shouldTryFetch.notify_one();
}

std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> BlockDownloader::fetchBlocks(const size_t blockCount)
{
    /* Attempt to fetch more blocks if we've run out */
    if (m_storedBlocks.size() == 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_consumedData = true;
        m_shouldTryFetch.notify_one();

        return {};
    }

    const auto blocks = m_storedBlocks.front_n(blockCount);

    if (Logger::logger.shouldLog(Logger::DEBUG))
    {
        Logger::logger.log(
            "Fetched " + std::to_string(blocks.size()) + " blocks from internal store", Logger::DEBUG, {Logger::SYNC});
    }

    return blocks;
}

std::vector<Crypto::Hash> BlockDownloader::getStoredBlockCheckpoints() const
{
    /* Project straight to the hashes under the queue's lock. Copying the
       blocks out first meant deep copying fifty blocks - transactions,
       outputs and all - before every sync request, to read one hash each. */
    return m_storedBlocks.back_n_transform(
        Constants::LAST_KNOWN_BLOCK_HASHES_SIZE,
        [](const std::tuple<WalletTypes::WalletBlockInfo, uint32_t> &block) { return std::get<0>(block).blockHash; });
}

std::vector<Crypto::Hash> BlockDownloader::getBlockCheckpoints() const
{
    /* Hashes of blocks we have downloaded but not processed */
    const auto unprocessedBlockHashes = getStoredBlockCheckpoints();

    std::vector<Crypto::Hash> result(unprocessedBlockHashes.size());

    std::copy(unprocessedBlockHashes.begin(), unprocessedBlockHashes.end(), result.begin());

    /* Hashes of blocks we have processed in the wallet */
    const auto recentProcessedBlockHashes = m_synchronizationStatus.getRecentBlockHashes();

    /* If we don't have the desired 50 blocks, add on the recently processed
       block checkpoints. This fixes us not passing the right data when
       we are fully synced or have no store built up yet */
    if (result.size() < Constants::LAST_KNOWN_BLOCK_HASHES_SIZE)
    {
        /* Copy the amount of hashes available, or the amount needed to make
           up the difference, whichever is less */
        const size_t numToCopy =
            std::min(recentProcessedBlockHashes.size(), Constants::LAST_KNOWN_BLOCK_HASHES_SIZE - result.size());

        std::copy(
            recentProcessedBlockHashes.begin(),
            recentProcessedBlockHashes.begin() + numToCopy,
            std::back_inserter(result));
    }

    /* Infrequent checkpoints to handle deep forks */
    const auto blockHashCheckpoints = m_synchronizationStatus.getBlockCheckpoints();

    std::copy(blockHashCheckpoints.begin(), blockHashCheckpoints.end(), std::back_inserter(result));

    return result;
}

bool BlockDownloader::downloadStep()
{
    return downloadBlocks();
}

void BlockDownloader::startStorageOnly()
{
    m_shouldStop = false;
    m_storedBlocks.start();
}

bool BlockDownloader::downloadBlocks()
{
    const uint64_t localDaemonBlockCount = m_daemon->localDaemonBlockCount();

    const uint64_t walletBlockCount = m_synchronizationStatus.getHeight();

    if (localDaemonBlockCount < walletBlockCount)
    {
        return false;
    }

    /* The highest block we hold, processed or merely downloaded. The block
       hashes we are about to send as checkpoints are the hashes of these
       blocks, so this is also the height the daemon will resolve our request
       to. Read once: the processing thread can only move blocks from the
       store into the processed count, which leaves this unchanged. */
    const uint64_t coveredTo = highestKnownHeight();

    /* Whether the response is allowed to have holes in it. Read here rather
       than after the response, so it is the same answer the request itself
       was built from - the daemon can be swapped underneath us, and reading
       it twice could say the holes we asked for were never asked for. */
    const bool holesExpected =
        Config::config.wallet.skipCoinbaseTransactions && m_daemon->daemonSkipsEmptyBlocks();

    const uint64_t liteStartHeight = m_daemon->liteStartHeight();

    if (liteStartHeight != 0 && coveredTo == 0 && m_startHeight < liteStartHeight)
    {
        /* Nothing scanned yet, so there is nothing to lose by starting higher.
           A lite daemon holds nothing below its lite start height and would
           answer from there whatever we asked for, so meet it and say plainly
           what that costs: funds received below that height are invisible
           through this daemon. See LITENODE.md. */
        Logger::logger.log(
            "Daemon is a lite node holding no data below height " + std::to_string(liteStartHeight)
                + ". Starting the scan there instead of " + std::to_string(m_startHeight)
                + ". Transactions below that height cannot be found through this daemon.",
            Logger::WARNING,
            {Logger::SYNC, Logger::DAEMON});

        /* A wallet created from a timestamp carries it until the first
           response resolves it to a height. That resolution happens further
           down and only while a timestamp is still set, so it has to happen
           here instead - we are about to clear it. */
        if (m_startTimestamp != 0)
        {
            m_subWallets->convertSyncTimestampToHeight(m_startTimestamp, liteStartHeight);
        }

        m_startHeight = liteStartHeight;
        m_startTimestamp = 0;
    }
    else if (liteStartHeight != 0 && coveredTo != 0 && coveredTo + 1 < liteStartHeight)
    {
        /* We have already scanned part of the chain, and this daemon cannot
           serve the part we still need. It would answer from its lite height
           regardless, and storing that would move our recorded position over
           blocks we never looked at - a silently wrong balance rather than an
           error. Stop instead, and let getSyncGap() say why. */
        recordSyncGap(coveredTo, liteStartHeight);

        return false;
    }

    const auto blockCheckpoints = getBlockCheckpoints();

    if (blockCheckpoints.size() > 0 && Logger::logger.shouldLog(Logger::DEBUG))
    {
        std::stringstream stream;

        stream << "First checkpoint: " << blockCheckpoints.front() << "\nLast checkpoint: " << blockCheckpoints.back();

        Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::SYNC});
    }

    const auto [success, blocks, topBlock, scannedToHeight] = m_daemon->getWalletSyncData(
        blockCheckpoints, m_startHeight, m_startTimestamp, Config::config.wallet.skipCoinbaseTransactions);

    /* Anything but a clean, complete answer leaves us unsure where we are, so
       the parallel path has to stand down until this path re-establishes it. */
    if (!success)
    {
        m_nextDownloadHeight = 0;
    }

    /* Synced, store the top block so sync status displayes correctly if
       we are not scanning coinbase tx only blocks */
    /* We can have an issue where we download a block, say, block 1000,
       then because we have space for more blocks, we go to fetch more,
       and this time get none, because we're synced. We then store the
       topblock, which is also 1000, as having being processed, when in
       fact, we're still waiting for it to be processed. So, if we only store
       it if we have no blocks waiting to be processed, it fixes this issue */
    if (success && blocks.empty() && topBlock && m_storedBlocks.size() == 0)
    {
        m_synchronizationStatus.storeBlockHash(topBlock->hash, topBlock->height);
        return false;
    }
    /* If we get no blocks, we are fully synced.
       (Or timed out/failed to get blocks)
       Sleep a bit so we don't spam the daemon. */
    else if (!success || blocks.empty())
    {
        /* We may have also failed because we requested
           more data than could be returned in a reasonable
           amount of time, so we'll back off a little bit */
        m_daemon->decreaseRequestedBlockCount();

        Logger::logger.log("Zero blocks received from daemon, possibly fully synced", Logger::DEBUG, {Logger::SYNC});

        return false;
    }

    /* If we received data back, we'll make sure we're back
       to running at full speed in case we backed off a little
       bit before */
    m_daemon->resetRequestedBlockCount();

    /* What we get back has to carry on from where we are. The one hole we can
       expect is a hole we asked for: a daemon told to leave coinbase only
       blocks out returns heights that are deliberately not contiguous.
       Anything else means the daemon could not serve the range we asked for
       and answered from higher up - a lite or pruned node, a blockchain cache
       API, or a fault - and storing it would move our recorded position over
       blocks nobody ever looked at.

       A timestamp start is exempt, because there the daemon decides which
       height the timestamp resolves to and we have nothing to compare against. */
    if (!holesExpected && m_startTimestamp == 0)
    {
        /* Mirrors what the daemon does with the same request: the higher of
           the block after our last one and the height we were told to start
           at. */
        const uint64_t expectedStart =
            coveredTo == 0 ? m_startHeight : std::max(coveredTo + 1, m_startHeight);

        if (blocks.front().blockHeight > expectedStart)
        {
            recordSyncGap(expectedStart == 0 ? 0 : expectedStart - 1, blocks.front().blockHeight);

            return false;
        }
    }

    /* Timestamp is transient and can change - block height is constant. */
    if (m_startTimestamp != 0)
    {
        const uint64_t previousStartTimestamp = m_startTimestamp;
        m_startTimestamp = 0;
        m_startHeight = blocks.front().blockHeight;

        m_subWallets->convertSyncTimestampToHeight(previousStartTimestamp, m_startHeight);
    }

    if (Logger::logger.shouldLog(Logger::DEBUG))
    {
        std::stringstream stream;

        stream << "Downloaded " << blocks.size() << " blocks from daemon, [" << blocks.front().blockHeight
               << ", " << blocks.back().blockHeight << "]";

        Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::SYNC});
    }

    storeDownloadedBlocks(blocks);

    /* The daemon told us how far it looked, which is not the same as the
       highest block it sent - the heights in between held nothing for us.
       That is what lets the parallel path pick up from the right place. */
    m_nextDownloadHeight = scannedToHeight != 0
        ? scannedToHeight + 1
        : blocks.back().blockHeight + 1;

    /* Only here, where a response has actually been accepted and stored. A
       gap that is still there re-records itself silently on the next attempt;
       clearing it earlier would make a permanent gap log itself as recovered
       and re-broken once every retry. */
    clearSyncGap();

    return true;
}

size_t BlockDownloader::storeDownloadedBlocks(const std::vector<WalletTypes::WalletBlockInfo> &blocks)
{
    if (blocks.empty())
    {
        return 0;
    }

    std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> blocksWithIndex;

    blocksWithIndex.reserve(blocks.size());

    size_t addedBytes = 0;

    for (const auto &block : blocks)
    {
        blocksWithIndex.push_back({block, m_arrivalIndex++});

        addedBytes += storedBlockMemoryUsage(blocksWithIndex.back());
    }

    if (!m_storedBlocks.push_back_n(blocksWithIndex.begin(), blocksWithIndex.end()))
    {
        return 0;
    }

    m_storedBlocksBytes += addedBytes;

    return blocks.size();
}

bool BlockDownloader::downloadBlocksInParallel()
{
    const uint64_t firstHeight = m_nextDownloadHeight.load();

    if (WalletConfig::syncRequestConcurrency < 2 || firstHeight == 0 || m_shouldStop
        || m_syncGapDetected || !m_daemon->daemonSupportsHeightRange())
    {
        return false;
    }

    const uint64_t daemonHeight = m_daemon->localDaemonBlockCount();

    /* Each window is addressed by height alone and carries no block hashes, so
       nothing about it would notice a fork. Only reach for it far enough below
       the tip that the daemon could not adopt one that deep - it prunes any
       alternative chain forking further back than that, so it cannot switch to
       one either. */
    const uint64_t window = std::min(
        m_daemon->requestedBlockCount() * CryptoNote::BLOCKS_SYNCHRONIZING_SKIP_EMPTY_SCAN_MULTIPLIER,
        CryptoNote::BLOCKS_SYNCHRONIZING_SKIP_EMPTY_MAX_SCAN);

    const uint64_t reorgSafetyMargin = 4 * CryptoNote::parameters::CRYPTONOTE_MAX_ALT_BLOCK_DEPTH;

    const uint64_t span = window * WalletConfig::syncRequestConcurrency + reorgSafetyMargin;

    if (window == 0 || daemonHeight < firstHeight || daemonHeight - firstHeight < span)
    {
        return false;
    }

    /* Name every window up front. That is the whole point of asking by height:
       we do not have to see one answer to know where the next request starts,
       so they can all be in flight together. */
    std::vector<std::future<WalletSyncResponse>> pending;

    pending.reserve(WalletConfig::syncRequestConcurrency);

    for (size_t i = 0; i < WalletConfig::syncRequestConcurrency; i++)
    {
        const uint64_t start = firstHeight + (window * i);

        pending.push_back(std::async(std::launch::async, [this, i, start, window] {
            return m_daemon->getWalletSyncDataRange(
                i, start, start + window, Config::config.wallet.skipCoinbaseTransactions);
        }));
    }

    size_t storedBlocks = 0;
    size_t completedWindows = 0;

    for (size_t i = 0; i < pending.size(); i++)
    {
        /* Every future has to be waited on regardless - each holds a reference
           to this object - but once we are stopping there is no point putting
           what they return anywhere. */
        const auto response = pending[i].get();

        if (m_shouldStop)
        {
            continue;
        }

        const uint64_t windowStart = firstHeight + (window * i);
        const uint64_t windowEnd = windowStart + window;

        /* Windows have to be stored in order, and a window we cannot vouch for
           ends the run - taking the ones behind it would leave a hole in the
           chain we scanned, and a hole is a transaction we never see. The
           remaining futures are simply dropped; the next round asks again from
           wherever we actually got to. */
        if (!response.success || response.scannedToHeight == 0)
        {
            break;
        }

        storedBlocks += storeDownloadedBlocks(response.blocks);

        m_nextDownloadHeight = response.scannedToHeight + 1;

        completedWindows++;

        /* The daemon stopped short of the window, almost certainly on its
           response size budget. Everything after this is a hole, so stop and
           let the next round resume from where it reached. */
        if (response.scannedToHeight + 1 < windowEnd)
        {
            break;
        }
    }

    if (m_shouldStop)
    {
        /* Report success so the caller does not follow up with a sequential
           request on the way out. */
        return true;
    }

    if (completedWindows == 0)
    {
        /* Nothing usable came back. Hand the next attempt to the sequential
           path, which can also tell us whether the chain moved under us. */
        m_nextDownloadHeight = 0;

        return false;
    }

    if (Logger::logger.shouldLog(Logger::DEBUG))
    {
        std::stringstream stream;

        stream << "Fetched " << completedWindows << " block windows in parallel, " << storedBlocks
               << " blocks, now at height " << m_nextDownloadHeight.load();

        Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::SYNC});
    }

    m_daemon->resetRequestedBlockCount();

    return true;
}

void BlockDownloader::fromJSON(const JSONObject &j, const uint64_t startHeight, const uint64_t startTimestamp)
{
    m_synchronizationStatus.fromJSON(j);
    m_startHeight = startHeight;
    m_startTimestamp = startTimestamp;
}

nlohmann::json BlockDownloader::toJSON() const
{
    return m_synchronizationStatus.toJSON();
}

void BlockDownloader::setSubWallets(const std::shared_ptr<SubWallets> subWallets)
{
    m_subWallets = subWallets;
}

void BlockDownloader::initializeAfterLoad(const std::shared_ptr<Nigel> daemon)
{
    m_daemon = daemon;
}
