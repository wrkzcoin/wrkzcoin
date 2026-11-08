// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include "SimnetNode.h"

#include "httplib.h"
//////////////////////////

/* windows.h, which httplib brings in, defines ERROR - which the logging
   headers below need as Logging::ERROR. */
#undef ERROR

#include <common/Util.h>
#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/Checkpoints.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/Currency.h>
#include <cryptonotecore/DatabaseBlockchainCache.h>
#include <cryptonotecore/DatabaseBlockchainCacheFactory.h>
#include <cryptonotecore/NetworkProfile.h>
#include <cryptonotecore/RocksDBWrapper.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandler.h>
#include <logging/LoggerRef.h>
#include <p2p/NetNode.h>
#include <p2p/NetNodeConfig.h>
#include <rpc/EventStream.h>
#include <rpc/EventStreamFeed.h>
#include <rpc/RpcServer.h>
#include <stdexcept>
#include <system/Dispatcher.h>

namespace Simnet
{
    Node::Node(const size_t index, NodeOptions options, std::shared_ptr<Logging::ILogger> logger):
        m_index(index),
        m_options(std::move(options)),
        m_logger(std::move(logger))
    {
    }

    Node::~Node()
    {
        stop();
    }

    bool Node::start(std::string &error)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_state != State::Created)
            {
                error = "a node can only be started once";
                return false;
            }

            m_state = State::Starting;
        }

        m_thread = std::thread(&Node::run, this);

        std::unique_lock<std::mutex> lock(m_mutex);

        m_stateChanged.wait(lock, [this] { return m_state != State::Starting; });

        if (m_state == State::Running)
        {
            return true;
        }

        error = m_error;
        lock.unlock();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        return false;
    }

    void Node::stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_p2p)
            {
                m_p2p->sendStopSignal();
            }
        }

        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void Node::run()
    {
        const auto fail = [this](const std::string &error) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = State::Failed;
            m_error = error;
            m_stateChanged.notify_all();
        };

        std::shared_ptr<CryptoNote::IDataBase> database;

        try
        {
            Logging::LoggerRef logger(m_logger, "simnet-node-" + std::to_string(m_index));

            System::Dispatcher dispatcher;

            CryptoNote::CurrencyBuilder currencyBuilder(m_logger);
            currencyBuilder.isSimnet(true);
            CryptoNote::Currency currency = currencyBuilder.currency();

            if (!Tools::create_directories_if_necessary(m_options.dataDir))
            {
                throw std::runtime_error("cannot create the data directory " + m_options.dataDir);
            }

            /* Sized for a chain of a few thousand blocks, and for several of
               these in one process. */
            CryptoNote::DataBaseConfig dbConfig(m_options.dataDir, 2, 256, 8, 8, true);

            database = std::make_shared<CryptoNote::RocksDBWrapper>(m_logger, dbConfig);
            database->init();

            if (!CryptoNote::DatabaseBlockchainCache::checkDBSchemeVersion(*database, m_logger))
            {
                throw std::runtime_error(
                    m_options.dataDir + " holds a database from an older version; remove it to start afresh");
            }

            const std::string networkError = CryptoNote::settleNetworkProfile(*database, true);

            if (!networkError.empty())
            {
                throw std::runtime_error(m_options.dataDir + ": " + networkError);
            }

            CryptoNote::Checkpoints checkpoints(m_logger);

            const auto core = std::make_shared<CryptoNote::Core>(
                currency,
                m_logger,
                std::move(checkpoints),
                dispatcher,
                std::unique_ptr<CryptoNote::IBlockchainCacheFactory>(
                    new CryptoNote::DatabaseBlockchainCacheFactory(*database, logger.getLogger(), 0)),
                m_options.validationThreads);

            core->load();

            const auto protocol = std::make_shared<CryptoNote::CryptoNoteProtocolHandler>(
                currency, dispatcher, *core, nullptr, m_logger);

            protocol->setSimnet(true);

            const auto p2p = std::make_shared<CryptoNote::NodeServer>(dispatcher, *protocol, m_logger);

            CryptoNote::NetNodeConfig netNodeConfig;

            if (!netNodeConfig.init(
                    m_options.bindIp,
                    m_options.p2pPort,
                    0,
                    8,
                    8,
                    true,
                    false,
                    m_options.dataDir,
                    {},
                    m_options.exclusiveNodes,
                    {},
                    {},
                    false))
            {
                throw std::runtime_error("an exclusive node address is not ip:port");
            }

            netNodeConfig.setNetworkId(m_options.networkId ? *m_options.networkId : CryptoNote::SIMNET_NETWORK);
            netNodeConfig.setUseDefaultSeeds(false);
            netNodeConfig.setUpnp(false);
            netNodeConfig.setTimedSyncIntervalSeconds(2);

            std::shared_ptr<EventStream> eventStream;

            if (m_options.websocket)
            {
                eventStream = std::make_shared<EventStream>(EventStreamConfig());
            }

            RpcServer rpcServer(
                m_options.rpcPort,
                m_options.bindIp,
                "",
                false,
                m_options.cors,
                "",
                15,
                30,
                2 * 1024 * 1024,
                0,
                5000,
                1000,
                16ULL * 1024 * 1024,
                false,
                "",
                0,
                "",
                false,
                RpcMode::Standard,
                core,
                p2p,
                protocol,
                eventStream);

            protocol->set_p2p_endpoint(p2p.get());

            if (!p2p->init(netNodeConfig))
            {
                throw std::runtime_error("the P2P server did not start on port " + std::to_string(m_options.p2pPort));
            }

            rpcServer.start();

            EventStreamFeed eventStreamFeed(dispatcher, *core, m_logger, eventStream, 0);

            if (eventStream)
            {
                eventStreamFeed.start();
            }

            /* The RPC listens on its own thread; ask it something before
               telling the caller it is up. */
            {
                httplib::Client client(m_options.bindIp, m_options.rpcPort);
                client.set_connection_timeout(std::chrono::seconds(1));

                bool answered = false;

                for (int attempt = 0; attempt < 50 && !answered; attempt++)
                {
                    answered = static_cast<bool>(client.Get("/height"));

                    if (!answered)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }

                if (!answered)
                {
                    throw std::runtime_error("the RPC did not answer on port " + std::to_string(m_options.rpcPort));
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_core = core;
                m_p2p = p2p;
                m_state = State::Running;
                m_stateChanged.notify_all();
            }

            p2p->run();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state = State::Stopping;
                m_core.reset();
                m_p2p.reset();
            }

            eventStreamFeed.stop();
            rpcServer.stop();
            p2p->deinit();
            protocol->set_p2p_endpoint(nullptr);
            core->save();
            database->shutdown();
            database.reset();

            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = State::Stopped;
            m_stateChanged.notify_all();
        }
        catch (const std::exception &e)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_core.reset();
                m_p2p.reset();
            }

            if (database)
            {
                database->shutdown();
            }

            fail(e.what());
        }
    }

    size_t Node::index() const
    {
        return m_index;
    }

    const NodeOptions &Node::options() const
    {
        return m_options;
    }

    std::string Node::p2pAddress() const
    {
        return m_options.bindIp + ":" + std::to_string(m_options.p2pPort);
    }

    std::string Node::rpcUrl() const
    {
        return "http://" + m_options.bindIp + ":" + std::to_string(m_options.rpcPort);
    }

    std::string Node::wsUrl() const
    {
        if (!m_options.websocket)
        {
            return "";
        }

        return "ws://" + m_options.bindIp + ":" + std::to_string(m_options.rpcPort) + "/ws";
    }

    uint32_t Node::topIndex() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_core ? m_core->getTopBlockIndex() : 0;
    }

    Crypto::Hash Node::topHash() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_core ? m_core->getTopBlockHash() : Crypto::Hash();
    }

    size_t Node::connections() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_p2p ? m_p2p->get_connections_count() : 0;
    }

    uint16_t pickFreePort()
    {
        const auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (sock == INVALID_SOCKET)
        {
            return 0;
        }

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        uint16_t port = 0;

        if (::bind(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0)
        {
            socklen_t length = sizeof(address);

            if (::getsockname(sock, reinterpret_cast<sockaddr *>(&address), &length) == 0)
            {
                port = ntohs(address.sin_port);
            }
        }

        httplib::detail::close_socket(sock);

        return port;
    }
} // namespace Simnet
