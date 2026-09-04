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
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>

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
        CryptoNote::BlockMiningParameters params = requestMiningParameters();
        adjustBlockTemplate(params.blockTemplate);

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

        startMining(params);

        eventLoop();
    }

    void MinerManager::printHashRate()
    {
        const auto reportEvery = std::chrono::seconds(60);

        uint64_t lastHashCount = m_miner.getHashCount();
        auto lastReport = std::chrono::steady_clock::now();

        while (isRunning)
        {
            /* Woken once a second rather than once a minute, so shutting down
               does not have to wait out a whole reporting interval. */
            std::this_thread::sleep_for(std::chrono::seconds(1));

            const auto now = std::chrono::steady_clock::now();

            if (!isRunning || now - lastReport < reportEvery)
            {
                continue;
            }

            const uint64_t currentHashCount = m_miner.getHashCount();
            const double elapsed = std::chrono::duration<double>(now - lastReport).count();
            const double hashes = static_cast<double>(currentHashCount - lastHashCount) / elapsed;

            lastHashCount = currentHashCount;
            lastReport = now;

            std::cout << SuccessMsg("\nMining at ")
                      << SuccessMsg(Utilities::get_mining_speed(static_cast<uint64_t>(hashes))) << "\n\n";
        }
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

                    CryptoNote::BlockMiningParameters params = requestMiningParameters();
                    adjustBlockTemplate(params.blockTemplate);

                    startBlockchainMonitoring();
                    startMining(params);
                    break;
                }
                case MinerEventType::BLOCKCHAIN_UPDATED:
                {
                    stopMining();
                    stopBlockchainMonitoring();
                    CryptoNote::BlockMiningParameters params = requestMiningParameters();
                    adjustBlockTemplate(params.blockTemplate);
                    startBlockchainMonitoring();
                    startMining(params);
                    break;
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

    CryptoNote::BlockMiningParameters MinerManager::requestMiningParameters()
    {
        while (true)
        {
            json j = {{"jsonrpc", "2.0"},
                      {"method", "getblocktemplate"},
                      {"params", {{"wallet_address", m_config.miningAddress}, {"reserve_size", 0}}}};

            auto res = m_httpClient->Post("/json_rpc", j.dump(), "application/json");

            if (!res)
            {
                std::cout << WarningMsg("Failed to get block template - Is your daemon open?\n");

                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (res->status != 200)
            {
                std::stringstream stream;

                stream << "Failed to get block template - received unexpected http "
                       << "code from server: " << res->status << std::endl;

                std::cout << WarningMsg(stream.str()) << std::endl;

                std::this_thread::sleep_for(std::chrono::seconds(1));
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

                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                CryptoNote::BlockMiningParameters params;
                params.difficulty = j.at("result").at("difficulty").get<uint64_t>();

                std::vector<uint8_t> blob = Common::fromHex(j.at("result").at("blocktemplate_blob").get<std::string>());

                if (!fromBinaryArray(params.blockTemplate, blob))
                {
                    std::cout << WarningMsg("Couldn't parse block template from daemon.") << std::endl;

                    std::this_thread::sleep_for(std::chrono::seconds(1));
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

                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }
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
