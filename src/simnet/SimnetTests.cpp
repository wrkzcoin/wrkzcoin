// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include "SimnetTests.h"

#include "httplib.h"
//////////////////////////

/* windows.h, which httplib brings in, defines ERROR - which the logging
   headers below need as Logging::ERROR. */
#undef ERROR

#include "Simnet.h"
#include "json.hpp"

#include <atomic>
#include <common/FileSystemShim.h>
#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/Checkpoints.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/Currency.h>
#include <cryptonotecore/DatabaseBlockchainCache.h>
#include <cryptonotecore/DatabaseBlockchainCacheFactory.h>
#include <cryptonotecore/NetworkProfile.h>
#include <cryptonotecore/RocksDBWrapper.h>
#include <functional>
#include <iostream>
#include <logging/LoggerRef.h>
#include <nigel/TipWatch.h>
#include <optional>
#include <stdexcept>
#include <system/Dispatcher.h>
#include <thread>

namespace Simnet
{
    namespace
    {
        using namespace std::chrono_literals;

        struct Failure : std::runtime_error
        {
            using std::runtime_error::runtime_error;
        };

        void check(const bool condition, const std::string &what)
        {
            if (!condition)
            {
                throw Failure(what);
            }
        }

        /* Waits for condition, checking every 50 ms. */
        bool eventually(const std::chrono::milliseconds timeout, const std::function<bool()> &condition)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (condition())
                {
                    return true;
                }

                std::this_thread::sleep_for(50ms);
            }

            return condition();
        }

        uint64_t mineOn(Node &node, const std::string &address, const size_t blocks)
        {
            Client client(node.rpcUrl());

            uint64_t height = 0;

            for (size_t i = 0; i < blocks; i++)
            {
                std::string error;
                check(client.mine(address, height, error), "mining on node " + std::to_string(node.index()) + ": " + error);
            }

            return height;
        }

        /* The next message on a stream whose topic is not a heartbeat. */
        std::optional<nlohmann::json> nextMessage(httplib::ws::WebSocketClient &ws)
        {
            std::string text;

            while (ws.read(text) == httplib::ws::Text)
            {
                auto message = nlohmann::json::parse(text, nullptr, false);

                if (message.is_discarded())
                {
                    return std::nullopt;
                }

                if (message.value("topic", std::string()) != "heartbeat")
                {
                    return message;
                }
            }

            return std::nullopt;
        }

        std::unique_ptr<Cluster> startCluster(
            std::shared_ptr<Logging::ILogger> logger,
            const std::string &dir,
            const size_t nodes,
            const Topology topology)
        {
            ClusterOptions options;
            options.nodes = nodes;
            options.topology = topology;
            options.dataDir = dir;

            auto cluster = std::make_unique<Cluster>(logger);

            std::string error;
            check(cluster->start(options, error), "starting the cluster: " + error);
            check(cluster->waitForConnections(1, 30s), "the nodes did not connect within 30 seconds");

            return cluster;
        }

        void relayAlongALine(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            auto cluster = startCluster(logger, dir, 3, Topology::Line);

            const auto keys = Keys::random();
            const uint64_t height = mineOn(cluster->node(0), keys.address, 5);

            uint32_t top = 0;
            Crypto::Hash hash;

            check(cluster->waitForAgreement({0, 1, 2}, 30s, top, hash), "the line did not agree on a tip");
            check(top == height, "the tip is " + std::to_string(top) + ", not the block mined at " + std::to_string(height));
        }

        void streamAnnouncesABlockMinedElsewhere(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            auto cluster = startCluster(logger, dir, 3, Topology::Line);

            httplib::ws::WebSocketClient ws(cluster->node(2).wsUrl() + "?topics=hashblock");
            ws.set_read_timeout(30);
            check(ws.connect(), "node 2 did not upgrade GET /ws");

            const auto hello = nextMessage(ws);
            check(hello && hello->value("topic", std::string()) == "hello", "the stream did not open with a hello");

            const auto carried = hello->at("data").at("topics");
            check(
                carried == nlohmann::json::array({"hashblock", "hashblock_alt"}),
                "?topics=hashblock should carry hashblock and hashblock_alt, not " + carried.dump());

            const auto keys = Keys::random();
            const uint64_t height = mineOn(cluster->node(0), keys.address, 1);

            /* Two hops away, so this is the relay announcing it, not the
               submission. */
            const auto announced = nextMessage(ws);
            check(announced.has_value(), "node 2's stream said nothing after a block was mined on node 0");
            check(announced->value("topic", std::string()) == "hashblock", "expected hashblock, got " + announced->dump());
            check(
                announced->at("data").at("height") == height,
                "the announced height is not the mined one: " + announced->dump());

            ws.close();
        }

        void refusesBadTopicsAndServesNoPlainGet(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            auto cluster = startCluster(logger, dir, 1, Topology::Line);

            httplib::ws::WebSocketClient misspelt(cluster->node(0).wsUrl() + "?topics=hashblok");
            check(!misspelt.connect(), "a misspelt topic was accepted");

            httplib::Client client(cluster->node(0).rpcUrl());
            const auto plain = client.Get("/ws");
            check(plain && plain->status == 426, "a GET /ws that asked for no upgrade was not answered 426");

            httplib::Headers fromPage = {{"Origin", "https://evil.example"}};
            httplib::ws::WebSocketClient page(cluster->node(0).wsUrl(), fromPage);
            check(!page.connect(), "a page from an origin --enable-cors does not allow was let subscribe");
        }

        void walletWatchIsWoken(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            auto cluster = startCluster(logger, dir, 2, Topology::Line);

            std::atomic<uint64_t> announcedHeight = 0;

            TipWatch watch;
            watch.setOnBlock([&announcedHeight](const uint64_t height) { announcedHeight = height; });
            watch.follow("127.0.0.1", cluster->node(1).options().rpcPort, false);

            check(eventually(15s, [&watch] { return watch.isLive(); }), "the wallet's watch never went live");

            const uint64_t seen = watch.blockEvents();

            const auto keys = Keys::random();
            const uint64_t height = mineOn(cluster->node(0), keys.address, 1);

            check(
                eventually(15s, [&] { return watch.blockEvents() > seen && announcedHeight == height; }),
                "the wallet's watch was not woken by the block at " + std::to_string(height));
        }

        void anotherNetworkIsTurnedAway(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            auto cluster = startCluster(logger, dir, 1, Topology::Line);

            const auto keys = Keys::random();
            mineOn(cluster->node(0), keys.address, 3);

            NodeOptions options;
            options.dataDir = (fs::path(dir) / "mainnet-id").string();
            options.p2pPort = pickFreePort();
            options.rpcPort = pickFreePort();
            options.websocket = false;
            options.exclusiveNodes = {cluster->node(0).p2pAddress()};
            options.networkId = CryptoNote::CRYPTONOTE_NETWORK;

            Node stranger(1, options, logger);

            std::string error;
            check(stranger.start(error), "starting the stranger: " + error);

            /* Plenty of dial attempts at a two second timed sync. */
            std::this_thread::sleep_for(8s);

            /* Its dial attempts come and go, so a connection count sampled now
               could catch one mid-handshake; that it never synced is what
               shows no handshake ever completed. */
            check(stranger.topIndex() == 0, "a node with mainnet's network id synced from the simnet");
        }

        void longerChainWinsAfterAPartition(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            /* Two nodes that do not know each other are a partition. */
            NodeOptions a;
            a.dataDir = (fs::path(dir) / "a").string();
            a.p2pPort = pickFreePort();
            a.rpcPort = pickFreePort();

            NodeOptions b;
            b.dataDir = (fs::path(dir) / "b").string();
            b.p2pPort = pickFreePort();
            b.rpcPort = pickFreePort();

            auto nodeA = std::make_unique<Node>(0, a, logger);
            Node nodeB(1, b, logger);

            std::string error;
            check(nodeA->start(error), "starting node a: " + error);
            check(nodeB.start(error), "starting node b: " + error);

            mineOn(*nodeA, Keys::random().address, 3);
            const uint64_t longer = mineOn(nodeB, Keys::random().address, 6);
            const Crypto::Hash longerTop = nodeB.topHash();

            /* Heal it: a comes back knowing b. */
            nodeA->stop();
            a.exclusiveNodes = {nodeB.p2pAddress()};
            nodeA = std::make_unique<Node>(0, a, logger);
            check(nodeA->start(error), "restarting node a: " + error);

            check(
                eventually(30s, [&] { return nodeA->topHash() == longerTop; }),
                "node a did not reorganise onto node b's longer chain");
            check(nodeA->topIndex() == longer, "node a's tip is not at node b's height");
        }

        void databaseRemembersItsNetwork(std::shared_ptr<Logging::ILogger> logger, const std::string &dir)
        {
            const auto open = [&logger](const std::string &path) {
                CryptoNote::DataBaseConfig config(path, 1, 64, 4, 4, false);
                auto database = std::make_shared<CryptoNote::RocksDBWrapper>(logger, config);
                database->init();
                return database;
            };

            /* A simnet's database, with a chain in it. */
            const std::string simnetDir = (fs::path(dir) / "simnet").string();
            {
                NodeOptions options;
                options.dataDir = simnetDir;
                options.p2pPort = pickFreePort();
                options.rpcPort = pickFreePort();

                Node node(0, options, logger);
                std::string error;
                check(node.start(error), "starting a simnet node: " + error);
                mineOn(node, Keys::random().address, 2);
            }

            {
                auto database = open(simnetDir);
                const std::string asMainnet = CryptoNote::settleNetworkProfile(*database, false);
                const std::string asSimnet = CryptoNote::settleNetworkProfile(*database, true);
                database->shutdown();

                check(!asMainnet.empty(), "a simnet database opened as mainnet");
                check(asSimnet.empty(), "a simnet database did not open as a simnet: " + asSimnet);
            }

            /* A mainnet database, with its genesis block written. */
            const std::string mainnetDir = (fs::path(dir) / "mainnet").string();
            {
                fs::create_directories(mainnetDir);
                auto database = open(mainnetDir);
                check(CryptoNote::settleNetworkProfile(*database, false).empty(), "an empty database refused mainnet");

                System::Dispatcher dispatcher;
                CryptoNote::CurrencyBuilder currencyBuilder(logger);
                const CryptoNote::Currency currency = currencyBuilder.currency();
                Logging::LoggerRef loggerRef(logger, "simnet-test");

                {
                    CryptoNote::Core core(
                        currency,
                        logger,
                        CryptoNote::Checkpoints(logger),
                        dispatcher,
                        std::unique_ptr<CryptoNote::IBlockchainCacheFactory>(
                            new CryptoNote::DatabaseBlockchainCacheFactory(*database, loggerRef.getLogger(), 0)),
                        1);

                    core.load();
                    core.save();
                }

                const std::string asSimnet = CryptoNote::settleNetworkProfile(*database, true);
                database->shutdown();

                check(!asSimnet.empty(), "a mainnet database with a chain opened as a simnet");
            }
        }
    } // namespace

    int runTests(std::shared_ptr<Logging::ILogger> logger, const std::string &workDir)
    {
        const std::vector<std::pair<std::string, std::function<void(std::shared_ptr<Logging::ILogger>, const std::string &)>>>
            scenarios = {
                {"blocks relay along a line", relayAlongALine},
                {"the stream announces a block mined elsewhere", streamAnnouncesABlockMinedElsewhere},
                {"GET /ws refuses bad topics, other origins and plain GETs", refusesBadTopicsAndServesNoPlainGet},
                {"a wallet's watch is woken by the stream", walletWatchIsWoken},
                {"a node of another network is turned away", anotherNetworkIsTurnedAway},
                {"the longer chain wins after a partition", longerChainWinsAfterAPartition},
                {"a database never changes network", databaseRemembersItsNetwork},
            };

        int failed = 0;
        size_t number = 0;

        for (const auto &[name, scenario] : scenarios)
        {
            const std::string dir = (fs::path(workDir) / ("test" + std::to_string(++number))).string();

            std::cout << "test " << name << " ... " << std::flush;

            const auto started = std::chrono::steady_clock::now();

            try
            {
                scenario(logger, dir);

                const auto seconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
                        .count()
                    / 1000.0;

                std::cout << "ok (" << seconds << " s)" << std::endl;
            }
            catch (const std::exception &e)
            {
                failed++;
                std::cout << "FAILED" << std::endl << "    " << e.what() << std::endl;
            }
        }

        std::cout << std::endl
                  << (failed == 0 ? "all " + std::to_string(scenarios.size()) + " passed"
                                  : std::to_string(failed) + " of " + std::to_string(scenarios.size()) + " failed")
                  << std::endl;

        return failed;
    }
} // namespace Simnet
