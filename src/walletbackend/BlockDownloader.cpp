// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////////////////////
#include <walletbackend/BlockDownloader.h>
//////////////////////////////////////////

#include <JsonHelper.h>
#include <config/Config.h>
#include <config/WalletConfig.h>
#include <logger/Logger.h>
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
    m_shouldStop = true;
    m_consumedData = true;
    m_shouldTryFetch.notify_one();
    m_storedBlocks.stop();

    if (m_downloadThread.joinable())
    {
        m_downloadThread.join();
    }
}

uint64_t BlockDownloader::getHeight() const
{
    return m_synchronizationStatus.getHeight();
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
            const bool blocksDownloaded = downloadBlocks();

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

    const auto blockCheckpoints = getBlockCheckpoints();

    if (blockCheckpoints.size() > 0 && Logger::logger.shouldLog(Logger::DEBUG))
    {
        std::stringstream stream;

        stream << "First checkpoint: " << blockCheckpoints.front() << "\nLast checkpoint: " << blockCheckpoints.back();

        Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::SYNC});
    }

    const auto [success, blocks, topBlock] = m_daemon->getWalletSyncData(
        blockCheckpoints, m_startHeight, m_startTimestamp, Config::config.wallet.skipCoinbaseTransactions);

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

    std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> blocksWithIndex;

    blocksWithIndex.reserve(blocks.size());

    size_t addedBytes = 0;

    for (const auto &block : blocks)
    {
        blocksWithIndex.push_back({block, m_arrivalIndex++});

        addedBytes += storedBlockMemoryUsage(blocksWithIndex.back());
    }

    if (m_storedBlocks.push_back_n(blocksWithIndex.begin(), blocksWithIndex.end()))
    {
        m_storedBlocksBytes += addedBytes;
    }

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
