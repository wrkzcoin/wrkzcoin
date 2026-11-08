// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* wrkz-simnet: a private test network from the command line.

   - run   a cluster of simnet nodes in this process, each with its RPC and
           WebSocket stream on loopback, mining a block every few seconds.
           Point wrkz-wallet, wrkz-wallet-api or a GUI wallet at one.
   - mine  mine against a Wrkzd --simnet in another process, or on another
           host, through its getblocktemplate and submitblock.
   - keys  print a fresh address and its keys, to mine to and then import.
   - test  whole nodes in this process, checked end to end. */

//////////////////////////
#include "Simnet.h"

#include "httplib.h"
//////////////////////////

/* windows.h, which httplib brings in, defines ERROR - which the logging
   headers below need as Logging::ERROR. */
#undef ERROR

#include "SimnetTests.h"

#include <atomic>
#include <chrono>
#include <common/FileSystemShim.h>
#include <common/JsonValue.h>
#include <common/StringTools.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <csignal>
#include <iostream>
#include <logger/Logger.h>
#include <logging/LoggerManager.h>
#include <optional>
#include <random>
#include <thread>

namespace
{
    const char USAGE[] = R"(wrkz-simnet run [options]    a simnet cluster in this process
  --nodes N                  how many nodes (default 3)
  --topology line|ring|mesh  how they are joined (default line)
  --rpc-port PORT            node 0's RPC port; node i gets PORT+i
                             (default 27856; 0 picks free ports)
  --p2p-port PORT            node 0's P2P port; node i gets PORT+i
                             (default 27955; 0 picks free ports)
  --data-dir DIR             keep the nodes' chains here, node i in DIR/node<i>
                             (default: a temporary directory, removed on exit)
  --no-websocket             do not serve GET /ws on the RPCs
  --enable-cors ORIGIN       let a browser wallet at ORIGIN use the RPCs
  --mine-to ADDRESS          pay mined blocks here (default: fresh keys,
                             printed at start so a wallet can import them)
  --premine N                mine N blocks before the interval starts
                             (default 60, enough to unlock the first rewards)
  --block-interval SECS      then one block every SECS seconds (default 10;
                             0 mines none)
  --log-level N              node logging, 0 (fatal) to 5 (trace); default 1

wrkz-simnet mine [options]   mine against a Wrkzd --simnet
  --daemon URL               its RPC (default http://127.0.0.1:27856)
  --address ADDRESS          pay blocks here (default: fresh keys, printed)
  --blocks N                 mine N blocks, then exit (default: until Ctrl-C)
  --interval SECS            between blocks (default 10; 0 is back to back)

wrkz-simnet keys             print a fresh address, its keys and how to
                             import them

wrkz-simnet test [options]   whole nodes in this process, checked end to end
  --data-dir DIR             work here and keep it (default: a temporary
                             directory, removed on exit)
  --log-level N              node logging, as for run

A simnet is a private network: its own network id (it never peers with
mainnet), no proof of work, difficulty 1, and mainnet's rules otherwise. Its
coins are worthless. A simnet wallet must scan from height 0: import the keys
with scan height 0.
)";

    std::atomic<bool> stopRequested = false;

    void onSignal(int)
    {
        stopRequested = true;
    }

    /* Sleeps for up to seconds, returning early on Ctrl-C. */
    void sleepUnlessStopping(const uint64_t seconds)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

        while (!stopRequested && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    /* --name VALUE pairs and bare flags, in order. */
    class Options
    {
      public:
        Options(const int argc, char **argv, const int first): m_args(argv + first, argv + argc) {}

        std::optional<std::string> next()
        {
            if (m_position >= m_args.size())
            {
                return std::nullopt;
            }

            return m_args[m_position++];
        }

        std::string value(const std::string &name)
        {
            const auto v = next();

            if (!v)
            {
                throw std::invalid_argument(name + " needs a value");
            }

            return *v;
        }

        uint64_t number(const std::string &name)
        {
            const std::string v = value(name);

            try
            {
                size_t used = 0;
                const uint64_t n = std::stoull(v, &used);

                if (used == v.size())
                {
                    return n;
                }
            }
            catch (const std::exception &)
            {
            }

            throw std::invalid_argument(name + ": " + v + " is not a number");
        }

      private:
        std::vector<std::string> m_args;

        size_t m_position = 0;
    };

    std::shared_ptr<Logging::LoggerManager> makeLogger(const uint64_t level)
    {
        const auto logManager = std::make_shared<Logging::LoggerManager>();

        Common::JsonValue configuration(Common::JsonValue::OBJECT);
        configuration.insert("globalLevel", static_cast<int64_t>(std::min<uint64_t>(level, Logging::TRACE)));

        Common::JsonValue &loggers = configuration.insert("loggers", Common::JsonValue::ARRAY);
        Common::JsonValue &console = loggers.pushBack(Common::JsonValue::OBJECT);
        console.insert("type", "console");
        console.insert("level", static_cast<int64_t>(Logging::TRACE));
        console.insert("pattern", "%D %T %L ");

        logManager->configure(configuration);

        /* The RPC server logs through the newer logger, which stays silent
           unless asked for. */
        if (level >= Logging::INFO)
        {
            Logger::logger.setLogLevel(level >= Logging::DEBUGGING ? Logger::DEBUG : Logger::INFO);
        }

        return logManager;
    }

    /* A directory of our own under the system's temporary directory. */
    std::string temporaryDirectory()
    {
        std::random_device random;

        const auto name = "wrkz-simnet-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
                          + "-" + std::to_string(random() % 100000);

        return (fs::temp_directory_path() / name).string();
    }

    void removeDirectory(const std::string &dir)
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    void printKeys(const Simnet::Keys &keys)
    {
        std::cout << "address:           " << keys.address << std::endl
                  << "private spend key: " << Common::podToHex(keys.spendKey) << std::endl
                  << "private view key:  " << Common::podToHex(keys.viewKey) << std::endl
                  << "mnemonic seed:     " << keys.mnemonic << std::endl
                  << "import into a wallet with these keys and scan height 0 (simnet coins only)" << std::endl;
    }

    std::string mineTo(const std::optional<std::string> &address)
    {
        if (address)
        {
            return *address;
        }

        const auto keys = Simnet::Keys::random();

        std::cout << "Mining to fresh keys:" << std::endl;
        printKeys(keys);
        std::cout << std::endl;

        return keys.address;
    }

    int run(Options &o)
    {
        Simnet::ClusterOptions options;
        options.rpcBasePort = static_cast<uint16_t>(CryptoNote::SIMNET_RPC_DEFAULT_PORT);
        options.p2pBasePort = static_cast<uint16_t>(CryptoNote::SIMNET_P2P_DEFAULT_PORT + 100);

        std::optional<std::string> address;
        std::string dataDir;
        uint64_t premine = 60;
        uint64_t interval = 10;
        uint64_t logLevel = Logging::ERROR;

        while (const auto arg = o.next())
        {
            if (*arg == "--nodes")
            {
                options.nodes = o.number("--nodes");
            }
            else if (*arg == "--topology")
            {
                const std::string topology = o.value("--topology");

                if (topology == "line")
                {
                    options.topology = Simnet::Topology::Line;
                }
                else if (topology == "ring")
                {
                    options.topology = Simnet::Topology::Ring;
                }
                else if (topology == "mesh")
                {
                    options.topology = Simnet::Topology::Mesh;
                }
                else
                {
                    throw std::invalid_argument("--topology " + topology + ": line, ring or mesh");
                }
            }
            else if (*arg == "--rpc-port")
            {
                options.rpcBasePort = static_cast<uint16_t>(o.number("--rpc-port"));
            }
            else if (*arg == "--p2p-port")
            {
                options.p2pBasePort = static_cast<uint16_t>(o.number("--p2p-port"));
            }
            else if (*arg == "--data-dir")
            {
                dataDir = o.value("--data-dir");
            }
            else if (*arg == "--no-websocket")
            {
                options.websocket = false;
            }
            else if (*arg == "--enable-cors")
            {
                options.cors = o.value("--enable-cors");
            }
            else if (*arg == "--mine-to")
            {
                address = o.value("--mine-to");
            }
            else if (*arg == "--premine")
            {
                premine = o.number("--premine");
            }
            else if (*arg == "--block-interval")
            {
                interval = o.number("--block-interval");
            }
            else if (*arg == "--log-level")
            {
                logLevel = o.number("--log-level");
            }
            else
            {
                throw std::invalid_argument("unknown option " + *arg);
            }
        }

        if (options.nodes == 0)
        {
            throw std::invalid_argument("--nodes must be at least 1");
        }

        for (const uint16_t base : {options.rpcBasePort, options.p2pBasePort})
        {
            if (base != 0 && base + options.nodes - 1 > 65535)
            {
                throw std::invalid_argument("the ports of " + std::to_string(options.nodes) + " nodes run past 65535");
            }
        }

        const bool temporary = dataDir.empty();
        options.dataDir = temporary ? temporaryDirectory() : dataDir;

        const auto logger = makeLogger(logLevel);

        const std::string payTo = mineTo(address);

        Simnet::Cluster cluster(logger);

        std::string error;

        if (!cluster.start(options, error))
        {
            std::cerr << "wrkz-simnet: starting the simnet: " << error << std::endl;

            if (temporary)
            {
                removeDirectory(options.dataDir);
            }

            return 1;
        }

        for (size_t i = 0; i < cluster.size(); i++)
        {
            auto &node = cluster.node(i);

            std::cout << "node " << i << ": p2p " << node.p2pAddress() << "  rpc " << node.rpcUrl() << "  events "
                      << (node.wsUrl().empty() ? "-" : node.wsUrl()) << std::endl;
        }

        std::cout << "chains in " << options.dataDir << (temporary ? " (removed on exit)" : "") << std::endl;

        if (cluster.size() > 1 && !cluster.waitForConnections(1, std::chrono::seconds(10)))
        {
            std::cerr << "warning: not every node connected within 10 seconds" << std::endl;
        }

        Simnet::Client miner(cluster.node(0).rpcUrl());

        for (uint64_t i = 0; i < premine && !stopRequested; i++)
        {
            uint64_t height = 0;

            if (!miner.mine(payTo, height, error))
            {
                std::cerr << "wrkz-simnet: premining: " << error << std::endl;
                break;
            }
        }

        if (premine > 0)
        {
            std::cout << "premined " << premine << " blocks on node 0" << std::endl;
        }

        std::cout << "running; Ctrl-C to stop" << std::endl;

        auto nextBlock = std::chrono::steady_clock::now() + std::chrono::seconds(interval);
        auto nextStatus = std::chrono::steady_clock::now();

        while (!stopRequested)
        {
            const auto now = std::chrono::steady_clock::now();

            if (interval > 0 && now >= nextBlock)
            {
                uint64_t height = 0;

                if (!miner.mine(payTo, height, error))
                {
                    std::cerr << "mining: " << error << std::endl;
                }

                nextBlock = std::chrono::steady_clock::now() + std::chrono::seconds(interval);
            }

            if (now >= nextStatus)
            {
                for (size_t i = 0; i < cluster.size(); i++)
                {
                    auto &node = cluster.node(i);

                    std::cout << (i == 0 ? "" : " | ") << "node " << i << " height " << node.topIndex() << " peers "
                              << node.connections();
                }

                std::cout << std::endl;

                nextStatus = now + std::chrono::seconds(30);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "stopping" << std::endl;

        cluster.stop();

        if (temporary)
        {
            removeDirectory(options.dataDir);
        }

        return 0;
    }

    int mine(Options &o)
    {
        std::string url = "http://127.0.0.1:" + std::to_string(CryptoNote::SIMNET_RPC_DEFAULT_PORT);
        std::optional<std::string> address;
        std::optional<uint64_t> blocks;
        uint64_t interval = 10;

        while (const auto arg = o.next())
        {
            if (*arg == "--daemon")
            {
                url = o.value("--daemon");
            }
            else if (*arg == "--address")
            {
                address = o.value("--address");
            }
            else if (*arg == "--blocks")
            {
                blocks = o.number("--blocks");
            }
            else if (*arg == "--interval")
            {
                interval = o.number("--interval");
            }
            else
            {
                throw std::invalid_argument("unknown option " + *arg);
            }
        }

        const std::string payTo = mineTo(address);

        Simnet::Client daemon(url);

        uint64_t mined = 0;

        while (!stopRequested && (!blocks || mined < *blocks))
        {
            std::string blob;
            uint64_t height = 0;
            std::string error;

            /* A daemon that is not up yet - the miner of a compose file starts
               with its node - is asked again, and so is one that went away. */
            if (!daemon.blockTemplate(payTo, blob, height, error))
            {
                std::cerr << error << "; retrying in 5 s" << std::endl;
                sleepUnlessStopping(5);
                continue;
            }

            if (!daemon.submitBlock(blob, error))
            {
                std::cerr << "wrkz-simnet: " << error << std::endl;
                return 1;
            }

            mined++;
            std::cout << "mined block " << height << std::endl;

            if (!blocks || mined < *blocks)
            {
                sleepUnlessStopping(interval);
            }
        }

        return 0;
    }

    int test(Options &o)
    {
        std::string dataDir;
        uint64_t logLevel = Logging::ERROR;

        while (const auto arg = o.next())
        {
            if (*arg == "--data-dir")
            {
                dataDir = o.value("--data-dir");
            }
            else if (*arg == "--log-level")
            {
                logLevel = o.number("--log-level");
            }
            else
            {
                throw std::invalid_argument("unknown option " + *arg);
            }
        }

        const bool temporary = dataDir.empty();
        const std::string workDir = temporary ? temporaryDirectory() : dataDir;

        const int failed = Simnet::runTests(makeLogger(logLevel), workDir);

        if (temporary)
        {
            removeDirectory(workDir);
        }

        return failed == 0 ? 0 : 1;
    }
} // namespace

int main(int argc, char **argv)
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const std::string command = argc > 1 ? argv[1] : "";

    if (command.empty() || command == "-h" || command == "--help")
    {
        std::cout << USAGE;
        return 0;
    }

    if (command == "-v" || command == "--version")
    {
        std::cout << CryptoNote::getProjectCLIHeader();
        return 0;
    }

    Options options(argc, argv, 2);

    try
    {
        if (command == "run")
        {
            return run(options);
        }

        if (command == "mine")
        {
            return mine(options);
        }

        if (command == "keys")
        {
            printKeys(Simnet::Keys::random());
            return 0;
        }

        if (command == "test")
        {
            return test(options);
        }

        throw std::invalid_argument("unknown command " + command);
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "wrkz-simnet: " << e.what() << std::endl << std::endl << USAGE;
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "wrkz-simnet: " << e.what() << std::endl;
        return 1;
    }
}
