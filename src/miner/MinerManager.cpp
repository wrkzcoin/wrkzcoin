// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/////////////////////////
#include "MinerManager.h"
#include "httplib.h"
/////////////////////////

#include <common/CryptoNoteTools.h>
#include <common/StringTools.h>
#include <common/TransactionExtra.h>
#include <config/CryptoNoteConfig.h>
#include <miner/BlockUtilities.h>
#include <system/Timer.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Utilities.h>

using json = nlohmann::json;

namespace Miner
{
    namespace
    {
        MinerEvent BlockMinedEvent()
        {
            MinerEvent event;
            event.type = MinerEventType::BLOCK_MINED;
            return event;
        }

        MinerEvent BlockchainUpdatedEvent()
        {
            MinerEvent event;
            event.type = MinerEventType::BLOCKCHAIN_UPDATED;
            return event;
        }

        MinerEvent ShutdownEvent()
        {
            MinerEvent event;
            event.type = MinerEventType::SHUTDOWN;
            return event;
        }

        void adjustMergeMiningTag(CryptoNote::BlockTemplate &blockTemplate)
        {
            if (blockTemplate.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_2)
            {
                CryptoNote::TransactionExtraMergeMiningTag mmTag;
                mmTag.depth = 0;
                mmTag.merkleRoot = getMerkleRoot(blockTemplate);

                blockTemplate.parentBlock.baseTransaction.extra.clear();
                if (!CryptoNote::appendMergeMiningTagToExtra(blockTemplate.parentBlock.baseTransaction.extra, mmTag))
                {
                    throw std::runtime_error("Couldn't append merge mining tag");
                }
            }
        }

    } // namespace

    MinerManager::MinerManager(
        System::Dispatcher &dispatcher,
        const CryptoNote::MiningConfig &config,
        const std::shared_ptr<httplib::Client> httpClient):

        m_dispatcher(dispatcher),
        m_contextGroup(dispatcher),
        m_config(config),
        m_miner(dispatcher),
        m_blockchainMonitor(dispatcher, m_config.scanPeriod, httpClient),
        m_eventOccurred(dispatcher),
        m_lastBlockTimestamp(0),
        m_httpClient(httpClient)
    {
    }

    void MinerManager::start()
    {
        printStartupSummary();

        auto params = requestMiningParameters();

        if (!params)
        {
            return;
        }

        adjustBlockTemplate(params->blockTemplate);

        isRunning = true;

        startBlockchainMonitoring();

        std::thread reporter(std::bind(&MinerManager ::printHashRate, this));

        /* The reporter has to be stopped and joined on every way out of here,
           including an exception from the event loop. Destroying a std::thread
           that is still joinable calls std::terminate(), which is what --limit
           used to do to the process on its way out. */
        struct ReporterGuard
        {
            std::atomic<bool> &running;

            std::thread &thread;

            ~ReporterGuard()
            {
                running = false;

                if (thread.joinable())
                {
                    thread.join();
                }
            }
        } reporterGuard {isRunning, reporter};

        startMining(*params);

        eventLoop();
    }

    void MinerManager::printStartupSummary() const
    {
        const std::string daemon = Utilities::isIpcDaemonAddress(m_config.daemonHost)
                                       ? Utilities::ipcDaemonPath(m_config.daemonHost)
                                       : m_config.daemonHost + ":" + std::to_string(m_config.daemonPort);

        std::cout << InformationMsg("Mining to ") << InformationMsg(m_config.miningAddress) << "\n"
                  << InformationMsg("Daemon:    ") << InformationMsg(daemon) << "\n"
                  << InformationMsg("Threads:   ") << InformationMsg(m_config.threadCount) << "\n\n";
    }

    void MinerManager::sleepSeconds(const size_t seconds)
    {
        System::Timer timer(m_dispatcher);

        timer.sleep(std::chrono::seconds(seconds));
    }

    void MinerManager::printHashRate()
    {
        if (m_config.hashRateInterval == 0)
        {
            return;
        }

        const auto reportEvery = std::chrono::seconds(m_config.hashRateInterval);

        uint64_t lastHashCount = m_miner.getHashCount();
        uint64_t lastActiveNanoseconds = m_miner.getActiveMiningNanoseconds();
        auto lastReport = std::chrono::steady_clock::now();

        while (isRunning)
        {
            /* Woken once a second rather than once an interval, so shutting
               down does not have to wait out a whole reporting period. This
               is its own OS thread, so it sleeps on the thread - sleeping on
               the dispatcher from here would be touching it off-thread. */
            std::this_thread::sleep_for(std::chrono::seconds(1));

            const auto now = std::chrono::steady_clock::now();

            if (!isRunning || now - lastReport < reportEvery)
            {
                continue;
            }

            const uint64_t currentHashCount = m_miner.getHashCount();
            const uint64_t currentActiveNanoseconds = m_miner.getActiveMiningNanoseconds();

            lastReport = now;

            /* Measured against the time the workers were actually running,
               not against wall clock: the seconds spent fetching a template,
               submitting a block or waiting on a daemon that is down are not
               seconds the hardware was given anything to hash. */
            const double elapsed = (currentActiveNanoseconds - lastActiveNanoseconds) / 1e9;

            const uint64_t hashes = currentHashCount - lastHashCount;

            lastHashCount = currentHashCount;
            lastActiveNanoseconds = currentActiveNanoseconds;

            if (elapsed <= 0)
            {
                std::cout << WarningMsg("\nNot mining - waiting on the daemon.\n\n");
                continue;
            }

            std::cout << SuccessMsg("\nMining at ") << SuccessMsg(Utilities::get_mining_speed(hashes / elapsed))
                      << "\n\n";
        }
    }

    void MinerManager::requestShutdown()
    {
        if (m_shutdownRequested.exchange(true))
        {
            std::cout << WarningMsg("\nStill shutting down. Leaving now.\n");
            std::exit(1);
        }

        std::cout << InformationMsg("\nShutting down, finishing the block in progress first...\n");

        /* The dispatcher owns the miner, the monitor and the event queue, so
           the signal thread only asks it to push the event. */
        m_dispatcher.remoteSpawn([this]() { pushEvent(ShutdownEvent()); });
    }

    void MinerManager::eventLoop()
    {
        size_t blocksMined = 0;

        while (true)
        {
            MinerEvent event = waitEvent();

            switch (event.type)
            {
                case MinerEventType::BLOCK_MINED:
                {
                    stopBlockchainMonitoring();

                    if (submitBlock(m_minedBlock))
                    {
                        m_lastBlockTimestamp = m_minedBlock.timestamp;

                        if (m_config.blocksLimit != 0 && ++blocksMined == m_config.blocksLimit)
                        {
                            std::cout << InformationMsg("Mined requested amount of blocks (")
                                      << InformationMsg(m_config.blocksLimit) << InformationMsg("). Quitting.\n");
                            return;
                        }
                    }

                    auto params = requestMiningParameters();

                    if (!params)
                    {
                        return;
                    }

                    adjustBlockTemplate(params->blockTemplate);

                    startBlockchainMonitoring();
                    startMining(*params);
                    break;
                }
                case MinerEventType::BLOCKCHAIN_UPDATED:
                {
                    stopMining();
                    stopBlockchainMonitoring();

                    auto params = requestMiningParameters();

                    if (!params)
                    {
                        return;
                    }

                    adjustBlockTemplate(params->blockTemplate);

                    startBlockchainMonitoring();
                    startMining(*params);
                    break;
                }
                case MinerEventType::SHUTDOWN:
                {
                    stopMining();
                    stopBlockchainMonitoring();
                    return;
                }
            }
        }
    }

    MinerEvent MinerManager::waitEvent()
    {
        while (m_events.empty())
        {
            m_eventOccurred.wait();
            m_eventOccurred.clear();
        }

        MinerEvent event = std::move(m_events.front());
        m_events.pop();

        return event;
    }

    void MinerManager::pushEvent(MinerEvent &&event)
    {
        m_events.push(std::move(event));
        m_eventOccurred.set();
    }

    void MinerManager::startMining(const CryptoNote::BlockMiningParameters &params)
    {
        m_contextGroup.spawn([this, params]() {
            try
            {
                m_minedBlock = m_miner.mine(params, m_config.threadCount);
                pushEvent(BlockMinedEvent());
            }
            catch (const std::exception &)
            {
            }
        });
    }

    void MinerManager::stopMining()
    {
        m_miner.stop();
    }

    void MinerManager::startBlockchainMonitoring()
    {
        m_contextGroup.spawn([this]() {
            try
            {
                m_blockchainMonitor.waitBlockchainUpdate();
                pushEvent(BlockchainUpdatedEvent());
            }
            catch (const std::exception &)
            {
            }
        });
    }

    void MinerManager::stopBlockchainMonitoring()
    {
        m_blockchainMonitor.stop();
    }

    bool MinerManager::submitBlock(const CryptoNote::BlockTemplate &minedBlock)
    {
        json j = {
            {"jsonrpc", "2.0"}, {"method", "submitblock"}, {"params", {Common::toHex(toBinaryArray(minedBlock))}}};

        auto res = m_httpClient->Post("/json_rpc", j.dump(), "application/json");

        if (!res)
        {
            std::cout << WarningMsg("Failed to submit block - the daemon did not answer. Is it still running?\n");
            return false;
        }

        if (res->status != 200)
        {
            std::cout << WarningMsg("Failed to submit block, possibly daemon offline or syncing?\n");
            return false;
        }

        /* A 200 still carries a JSON-RPC error when the daemon would not take
           the block, so the status alone does not mean it was accepted. */
        try
        {
            const json response = json::parse(res->body);

            if (response.contains("error") && !response.at("error").is_null())
            {
                std::stringstream stream;

                stream << "Block was not accepted by the daemon: " << response.at("error").dump() << std::endl;

                std::cout << WarningMsg(stream.str());
                return false;
            }
        }
        catch (const json::exception &e)
        {
            std::stringstream stream;

            stream << "Could not read the daemon's reply to our block: " << e.what() << std::endl;

            std::cout << WarningMsg(stream.str());
            return false;
        }

        std::cout << SuccessMsg("\nBlock found! Hash: ") << SuccessMsg(getBlockHash(minedBlock)) << "\n\n";

        return true;
    }

    std::optional<CryptoNote::BlockMiningParameters> MinerManager::requestMiningParameters()
    {
        while (!m_shutdownRequested)
        {
            json j = {{"jsonrpc", "2.0"},
                      {"method", "getblocktemplate"},
                      {"params", {{"wallet_address", m_config.miningAddress}, {"reserve_size", 0}}}};

            auto res = m_httpClient->Post("/json_rpc", j.dump(), "application/json");

            if (!res)
            {
                std::cout << WarningMsg("Failed to get block template - Is your daemon open?\n");

                sleepSeconds(m_config.retryInterval);
                continue;
            }

            if (res->status != 200)
            {
                std::stringstream stream;

                stream << "Failed to get block template - received unexpected http "
                       << "code from server: " << res->status << std::endl;

                std::cout << WarningMsg(stream.str()) << std::endl;

                sleepSeconds(m_config.retryInterval);
                continue;
            }

            try
            {
                json j = json::parse(res->body);

                const std::string status = j.at("result").at("status").get<std::string>();

                if (status != "OK")
                {
                    std::stringstream stream;

                    stream << "Failed to get block template from daemon. Response: " << status << std::endl;

                    std::cout << WarningMsg(stream.str());

                    sleepSeconds(m_config.retryInterval);
                    continue;
                }

                CryptoNote::BlockMiningParameters params;
                params.difficulty = j.at("result").at("difficulty").get<uint64_t>();
                params.height = j.at("result").at("height").get<uint32_t>();

                std::vector<uint8_t> blob = Common::fromHex(j.at("result").at("blocktemplate_blob").get<std::string>());

                if (!fromBinaryArray(params.blockTemplate, blob))
                {
                    std::cout << WarningMsg("Couldn't parse block template from daemon.") << std::endl;

                    sleepSeconds(m_config.retryInterval);
                    continue;
                }

                return params;
            }
            catch (const json::exception &e)
            {
                std::stringstream stream;

                stream << "Failed to parse block template from daemon. Received data:\n"
                       << res->body << "\nParse error: " << e.what() << std::endl;

                std::cout << WarningMsg(stream.str());

                sleepSeconds(m_config.retryInterval);
                continue;
            }
        }

        /* Only reachable by the loop condition failing, which means a
           shutdown was asked for while it was retrying. */
        return std::nullopt;
    }

    void MinerManager::adjustBlockTemplate(CryptoNote::BlockTemplate &blockTemplate) const
    {
        adjustMergeMiningTag(blockTemplate);

        if (m_config.firstBlockTimestamp == 0)
        {
            /* no need to fix timestamp */
            return;
        }

        if (m_lastBlockTimestamp == 0)
        {
            blockTemplate.timestamp = m_config.firstBlockTimestamp;
        }
        else if (m_lastBlockTimestamp != 0 && m_config.blockTimestampInterval != 0)
        {
            blockTemplate.timestamp = m_lastBlockTimestamp + m_config.blockTimestampInterval;
        }
    }

} // namespace Miner
