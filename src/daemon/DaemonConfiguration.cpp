// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DaemonConfiguration.h"

#include "common/PathTools.h"
#include "common/Util.h"
#include "json.hpp"

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cstdlib>
#include <cxxopts.hpp>
#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <logging/ILogger.h>
#include <regex>

namespace DaemonConfig
{
    namespace
    {
        uint32_t clampPruneDepth(const uint32_t depth, const std::string &source)
        {
            if (depth >= DaemonConfiguration::MIN_PRUNE_DEPTH)
            {
                return depth;
            }

            std::cout << CryptoNote::getProjectCLIHeader() << "The configured prune depth (" << depth
                      << ") from " << source << " is below the enforced minimum (" << DaemonConfiguration::MIN_PRUNE_DEPTH
                      << ", about " << DaemonConfiguration::MIN_PRUNE_DEPTH_DAYS
                      << " days). Using the minimum for network health." << std::endl;

            return DaemonConfiguration::MIN_PRUNE_DEPTH;
        }

        uint32_t clampSyncMaxPeersToOutPeers(
            const uint32_t syncMaxPeers,
            const uint32_t outPeers,
            const std::string &source)
        {
            if (syncMaxPeers <= outPeers)
            {
                return syncMaxPeers;
            }

            std::cout << CryptoNote::getProjectCLIHeader() << "The configured sync-max-peers (" << syncMaxPeers
                      << ") from " << source << " exceeds out-peers (" << outPeers
                      << "). Using out-peers value to avoid oversubscription." << std::endl;

            return outPeers;
        }

        std::string normalizeDaemonMode(const std::string &rawMode)
        {
            std::string mode = rawMode;
            std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (mode == DaemonConfiguration::DAEMON_MODE_STANDARD || mode == DaemonConfiguration::DAEMON_MODE_EXPLORER)
            {
                return mode;
            }

            throw std::runtime_error(
                "Invalid daemon-mode: '" + rawMode + "'. Allowed values are '" + std::string(DaemonConfiguration::DAEMON_MODE_STANDARD)
                + "' or '" + std::string(DaemonConfiguration::DAEMON_MODE_EXPLORER) + "'.");
        }
    } // namespace

    DaemonConfiguration initConfiguration(const char *path)
    {
        DaemonConfiguration config;
        config.logFile = Common::ReplaceExtenstion(Common::NativePathToGeneric(path), ".log");
        return config;
    }

    void handleSettings(int argc, char *argv[], DaemonConfiguration &config)
    {
        cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

        options.add_options("Core")(
            "help", "Display this help message", cxxopts::value<bool>()->implicit_value("true"))(
            "os-version",
            "Output Operating System version information",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "resync",
            "Forces the daemon to delete the blockchain data and start resyncing",
            cxxopts::value<bool>(config.resync)->default_value("false")->implicit_value("true"))(
            "prune",
            "Enable pruned-node mode for daemon sync behavior",
            cxxopts::value<bool>(config.prune)->default_value("false")->implicit_value("true"))(
            "prune-depth",
            "When prune mode is enabled, retain at least this many recent blocks locally",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.pruneDepth)),
            "#")(
            "rewind-to-height",
            "Rewinds the local blockchain cache to the specified height.",
            cxxopts::value<uint32_t>(),
            "#")(
            "version",
            "Output daemon version information",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Import / Export")(
            "import-blockchain",
            "Import blockchain DB from dump file",
            cxxopts::value<bool>(config.importChain)->default_value("false")->implicit_value("true"))(
            "export-blockchain",
            "Export blockchain DB to a dump file",
            cxxopts::value<bool>(config.exportChain)->default_value("false")->implicit_value("true"))(
            "max-export-blocks",
            "Maximum number of blocks for export to dump file.",
            cxxopts::value<uint32_t>(),
            "#");

        options.add_options("Genesis Block")(
            "print-genesis-tx",
            "Print the genesis block transaction hex and exits",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "c,config-file", "Specify the <path> to a configuration file", cxxopts::value<std::string>(), "<path>")(
            "data-dir",
            "Specify the <path> to the Blockchain data directory",
            cxxopts::value<std::string>()->default_value(config.dataDirectory),
            "<path>")(
            "dump-config",
            "Prints the current configuration to the screen",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "load-checkpoints",
            "Specify a file <path> containing a CSV of Blockchain checkpoints for faster sync. A value of 'default' "
            "uses the built-in checkpoints.",
            cxxopts::value<std::string>()->default_value(config.checkPoints),
            "<path>")(
            "log-file",
            "Specify the <path> to the log file",
            cxxopts::value<std::string>()->default_value(config.logFile),
            "<path>")(
            "log-level",
            "Specify log level",
            cxxopts::value<int>()->default_value(std::to_string(config.logLevel)),
            "#")(
            "no-console",
            "Disable daemon console commands",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "skip-boot-compaction",
            "Skip automatic DB compaction start/check at daemon boot",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "save-config", "Save the configuration to the specified <file>", cxxopts::value<std::string>(), "<file>");

        options.add_options("RPC")(
            "daemon-mode",
            "Daemon RPC mode: standard or explorer",
            cxxopts::value<std::string>()->default_value(config.daemonMode),
            "<standard|explorer>")(
            "enable-cors",
            "Adds header 'Access-Control-Allow-Origin' to the RPC responses using the <domain>. Uses the value "
            "specified as the domain. Use * for all.",
            cxxopts::value<std::string>(),
            "<domain>")(
            "rpc-access-token",
            "Require this token in RPC header X-API-Key (or Authorization: Bearer <token>)",
            cxxopts::value<std::string>()->default_value(config.rpcAccessToken),
            "<token>")(
            "rpc-read-timeout",
            "RPC read timeout in seconds",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcReadTimeout)),
            "#")(
            "rpc-write-timeout",
            "RPC write timeout in seconds",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcWriteTimeout)),
            "#")(
            "rpc-max-body-bytes",
            "Maximum RPC request body size in bytes",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.rpcMaxRequestBodyBytes)),
            "#")(
            "rpc-max-rpm",
            "Maximum RPC requests per minute per client IP (0 disables rate limiting)",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcMaxRequestsPerMinute)),
            "#")(
            "rpc-max-global-index-range",
            "Maximum allowed block range for get_global_indexes_for_range",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcMaxGlobalIndexesRange)),
            "#")(
            "rpc-max-block-count",
            "Maximum allowed blockCount for wallet/raw-block sync RPC methods",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcMaxBlockCount)),
            "#")(
            "rpc-trust-proxy",
            "Trust X-Forwarded-For header for client IP (enable only behind trusted reverse proxy)",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "zmq-pub",
            "ZMQ PUB endpoint (for example tcp://127.0.0.1:"
                + std::to_string(CryptoNote::ZMQ_PUB_DEFAULT_PORT) + "). Empty disables ZMQ publisher.",
            cxxopts::value<std::string>()->default_value(config.zmqPub),
            "<address>")(
            "no-zmq",
            "Disable ZMQ publisher even if zmq-pub is set",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "block-notify",
            "Run a command or POST to an http(s):// URL for each new main-chain block. "
            "Command placeholders: %s block hash, %h height (no shell; quotes group arguments)",
            cxxopts::value<std::string>()->default_value(config.blockNotify),
            "<cmd|url>")(
            "reorg-notify",
            "Run a command or POST to an http(s):// URL on every chain reorganisation. "
            "Placeholders: %s split height, %h new height, %n new blocks, %d discarded blocks",
            cxxopts::value<std::string>()->default_value(config.reorgNotify),
            "<cmd|url>")(
            "tx-notify",
            "Run a command or POST to an http(s):// URL for each transaction entering the pool. "
            "Placeholders: %s transaction hash",
            cxxopts::value<std::string>()->default_value(config.txNotify),
            "<cmd|url>")(
            "notify-during-sync",
            "Also fire *-notify hooks while the node is still synchronizing (default: suppressed)",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Network")(
            "allow-local-ip",
            "Allow the local IP to be added to the peer list",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "hide-my-port",
            "Do not announce yourself as a peerlist candidate",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "p2p-bind-ip",
            "Interface IP address for the P2P service",
            cxxopts::value<std::string>()->default_value(config.p2pInterface),
            "<ip>")(
            "p2p-bind-port",
            "TCP port for the P2P service",
            cxxopts::value<int>()->default_value(std::to_string(config.p2pPort)),
            "#")(
            "p2p-external-port",
            "External TCP port for the P2P service (NAT port forward)",
            cxxopts::value<int>()->default_value("0"),
            "#")(
            "out-peers",
            "Maximum number of outgoing P2P connections",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.p2pOutPeers)),
            "#")(
            "in-peers",
            "Maximum number of incoming P2P connections",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.p2pInPeers)),
            "#")(
            "p2p-reset-peerstate",
            "Generate a new peer ID and remove known peers saved previously",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "p2p-bind-ipv6-address",
            "IPv6 bind address for the P2P service (e.g. :: for all interfaces). Empty disables IPv6 P2P listener.",
            cxxopts::value<std::string>()->default_value(""),
            "<ipv6>")(
            "p2p-bind-port-ipv6",
            "TCP port for the IPv6 P2P listener (0 = same as --p2p-bind-port)",
            cxxopts::value<int>()->default_value("0"),
            "#")(
            "rpc-bind-ipv6-address",
            "IPv6 bind address for the RPC service (e.g. ::1). Empty disables IPv6 RPC.",
            cxxopts::value<std::string>()->default_value(""),
            "<ipv6>")(
            "rpc-use-ipv6",
            "Enable IPv6 support for the RPC service",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "rpc-bind-ip",
            "Interface IP address for the RPC service",
            cxxopts::value<std::string>()->default_value(config.rpcInterface),
            "<ip>")(
            "rpc-bind-port",
            "TCP port for the RPC service",
            cxxopts::value<int>()->default_value(std::to_string(config.rpcPort)),
            "#");

        options.add_options("Peer")(
            "add-exclusive-node",
            "Manually add a peer to the local peer list ONLY attempt connections to it. [ip:port]",
            cxxopts::value<std::vector<std::string>>(),
            "<ip:port>")(
            "add-peer",
            "Manually add a peer to the local peer list",
            cxxopts::value<std::vector<std::string>>(),
            "<ip:port>")(
            "add-priority-node",
            "Manually add a peer to the local peer list and attempt to maintain a connection to it [ip:port]",
            cxxopts::value<std::vector<std::string>>(),
            "<ip:port>")(
            "seed-node",
            "Connect to a node to retrieve the peer list and then disconnect",
            cxxopts::value<std::vector<std::string>>(),
            "<ip:port>");

        const std::string maxOpenFiles =
            "(default: " + std::to_string(CryptoNote::ROCKSDB_MAX_OPEN_FILES) + ")";

        const std::string readCache =
            "(default: " + std::to_string(CryptoNote::ROCKSDB_READ_BUFFER_MB) + ")";

        const std::string writeBuffer =
            "(default: " + std::to_string(CryptoNote::ROCKSDB_WRITE_BUFFER_MB) + ")";

        options.add_options("Database")
            ("db-enable-compression",
             "Enable database compression",
             cxxopts::value<bool>()->default_value("true")->implicit_value("true"))
            ("db-max-open-files",
             "Number of files that can be used by the database at one time " + maxOpenFiles,
             cxxopts::value<int>(),
             "#")
            ("db-read-buffer-size",
             "Size of the database read cache in megabytes (MB) " + readCache,
             cxxopts::value<int>(),
             "#")
            ("db-threads",
             "Number of background threads used for compaction and flush operations (RocksDB only)",
             cxxopts::value<int>()->default_value(std::to_string(CryptoNote::ROCKSDB_BACKGROUND_THREADS)),
             "#")
            ("db-write-buffer-size",
             "Size of the database write buffer in megabytes (MB) " + writeBuffer,
             cxxopts::value<int>(),
             "#");

        options.add_options("Syncing")(
            "transaction-validation-threads",
            "Number of threads to use to validate a transaction's inputs in parallel",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.transactionValidationThreads)),
            "#")(
            "sync-max-peers",
            "Maximum number of peers to synchronize blocks from in parallel",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.syncMaxPeers)),
            "#")(
            "sync-peer-failure-threshold",
            "Failures allowed for a sync peer before demotion",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.syncPeerFailureThreshold)),
            "#")(
            "sync-batch-min",
            "Minimum adaptive block request batch size",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.syncBatchMin)),
            "#")(
            "sync-batch-max",
            "Maximum adaptive block request batch size",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.syncBatchMax)),
            "#")(
            "block-sync-size",
            "Maximum number of blocks requested per sync chunk",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.blockSyncSize)),
            "#")(
            "block-sync-bytes",
            "Maximum approximate bytes requested per sync chunk",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.blockSyncBytes)),
            "#")(
            "auto-prune-min-gap-blocks",
            "Minimum block gap between automatic prune passes (0 disables periodic auto-prune)",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.autoPruneMinGapBlocks)),
            "#")(
            "auto-compaction-min-gap-blocks",
            "Minimum block gap between automatic DB compactions (0 disables periodic auto-compaction)",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.autoCompactionMinGapBlocks)),
            "#")(
            "auto-prune-min-free-bytes",
            "Minimum free bytes required before regular auto-prune schedule (low-space mode can still force prune)",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.autoPruneMinFreeBytes)),
            "#")(
            "auto-compaction-min-free-bytes",
            "Minimum free bytes required to start automatic DB compaction",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.autoCompactionMinFreeBytes)),
            "#");

        try
        {
            auto cli = options.parse(argc, argv);

            if (cli.count("config-file") > 0)
            {
                config.configFile = cli["config-file"].as<std::string>();
            }

            if (cli.count("save-config") > 0)
            {
                config.outputFile = cli["save-config"].as<std::string>();
            }

            if (cli.count("help") > 0)
            {
                config.help = cli["help"].as<bool>();
            }

            if (cli.count("version") > 0)
            {
                config.version = cli["version"].as<bool>();
            }

            if (cli.count("os-version") > 0)
            {
                config.osVersion = cli["os-version"].as<bool>();
            }

            if (cli.count("rewind-to-height") > 0)
            {
                uint32_t rewindHeight = cli["rewind-to-height"].as<uint32_t>();
                if (rewindHeight == 0)
                {
                    std::cout << CryptoNote::getProjectCLIHeader()
                              << "Please use the `--resync` option instead of `--rewind-to-height 0` to completely "
                                 "reset the synchronization state."
                              << std::endl;
                    exit(1);
                }
                else
                {
                    config.rewindToHeight = rewindHeight;
                }
            }

            if (cli.count("max-export-blocks") > 0)
            {
                uint32_t exportBlocks = cli["max-export-blocks"].as<uint32_t>();
                if (exportBlocks == 0)
                {
                    std::cout << CryptoNote::getProjectCLIHeader()
                              << "`--max-export-blocks` can not be 0. "
                              << std::endl;
                    exit(1);
                }
                else
                {
                    config.exportNumBlocks = exportBlocks;
                }
            }

            if (cli.count("prune") > 0)
            {
                config.prune = cli["prune"].as<bool>();
            }

            if (cli.count("prune-depth") > 0)
            {
                config.pruneDepth = clampPruneDepth(cli["prune-depth"].as<uint32_t>(), "CLI");
            }

            if (cli.count("print-genesis-tx") > 0)
            {
                config.printGenesisTx = cli["print-genesis-tx"].as<bool>();
            }

            if (cli.count("dump-config") > 0)
            {
                config.dumpConfig = cli["dump-config"].as<bool>();
            }

            if (cli.count("data-dir") > 0)
            {
                config.dataDirectory = cli["data-dir"].as<std::string>();
            }

            if (cli.count("load-checkpoints") > 0)
            {
                config.checkPoints = cli["load-checkpoints"].as<std::string>();
            }

            if (cli.count("log-file") > 0)
            {
                config.logFile = cli["log-file"].as<std::string>();
            }

            if (cli.count("log-level") > 0)
            {
                config.logLevel = cli["log-level"].as<int>();
            }

            if (cli.count("db-enable-compression") > 0)
            {
                config.enableDbCompression = cli["db-enable-compression"].as<bool>();
            }

            if (cli.count("no-console") > 0)
            {
                config.noConsole = cli["no-console"].as<bool>();
            }

            if (cli.count("skip-boot-compaction") > 0)
            {
                config.skipBootCompaction = cli["skip-boot-compaction"].as<bool>();
            }

            config.dbMaxOpenFiles = CryptoNote::ROCKSDB_MAX_OPEN_FILES;
            config.dbReadCacheSizeMB = CryptoNote::ROCKSDB_READ_BUFFER_MB;
            config.dbWriteBufferSizeMB = CryptoNote::ROCKSDB_WRITE_BUFFER_MB;
            config.dbThreads = CryptoNote::ROCKSDB_BACKGROUND_THREADS;

            if (cli.count("db-max-open-files") > 0)
            {
                config.dbMaxOpenFiles = cli["db-max-open-files"].as<int>();
            }

            if (cli.count("db-read-buffer-size") > 0)
            {
                config.dbReadCacheSizeMB = cli["db-read-buffer-size"].as<int>();
            }

            if (cli.count("db-threads") > 0)
            {
                config.dbThreads = cli["db-threads"].as<int>();
            }

            if (cli.count("db-write-buffer-size") > 0)
            {
                config.dbWriteBufferSizeMB = cli["db-write-buffer-size"].as<int>();
            }

            if (cli.count("local-ip") > 0)
            {
                config.localIp = cli["local-ip"].as<bool>();
            }

            if (cli.count("hide-my-port") > 0)
            {
                config.hideMyPort = cli["hide-my-port"].as<bool>();
            }

            if (cli.count("p2p-bind-ip") > 0)
            {
                config.p2pInterface = cli["p2p-bind-ip"].as<std::string>();
            }

            if (cli.count("p2p-bind-port") > 0)
            {
                config.p2pPort = cli["p2p-bind-port"].as<int>();
            }

            if (cli.count("p2p-external-port") > 0)
            {
                config.p2pExternalPort = cli["p2p-external-port"].as<int>();
            }

            if (cli.count("out-peers") > 0)
            {
                config.p2pOutPeers = std::max<uint32_t>(1, cli["out-peers"].as<uint32_t>());
            }

            if (cli.count("in-peers") > 0)
            {
                config.p2pInPeers = cli["in-peers"].as<uint32_t>();
            }

            if (cli.count("p2p-reset-peerstate") > 0)
            {
                config.p2pResetPeerstate = cli["p2p-reset-peerstate"].as<bool>();
            }

            if (cli.count("p2p-bind-ipv6-address") > 0)
            {
                config.p2pBindIpv6Address = cli["p2p-bind-ipv6-address"].as<std::string>();
            }

            if (cli.count("p2p-bind-port-ipv6") > 0)
            {
                config.p2pBindPortIpv6 = cli["p2p-bind-port-ipv6"].as<int>();
            }

            if (cli.count("rpc-bind-ipv6-address") > 0)
            {
                config.rpcBindIpv6Address = cli["rpc-bind-ipv6-address"].as<std::string>();
            }

            if (cli.count("rpc-use-ipv6") > 0)
            {
                config.rpcUseIpv6 = cli["rpc-use-ipv6"].as<bool>();
            }

            if (cli.count("rpc-bind-ip") > 0)
            {
                config.rpcInterface = cli["rpc-bind-ip"].as<std::string>();
            }

            if (cli.count("rpc-bind-port") > 0)
            {
                config.rpcPort = cli["rpc-bind-port"].as<int>();
            }

            if (cli.count("add-exclusive-node") > 0)
            {
                config.exclusiveNodes = cli["add-exclusive-node"].as<std::vector<std::string>>();
            }

            if (cli.count("add-peer") > 0)
            {
                config.peers = cli["add-peer"].as<std::vector<std::string>>();
            }

            if (cli.count("add-priority-node") > 0)
            {
                config.priorityNodes = cli["add-priority-node"].as<std::vector<std::string>>();
            }

            if (cli.count("seed-node") > 0)
            {
                config.seedNodes = cli["seed-node"].as<std::vector<std::string>>();
            }

            if (cli.count("daemon-mode") > 0)
            {
                config.daemonMode = normalizeDaemonMode(cli["daemon-mode"].as<std::string>());
            }

            if (cli.count("enable-cors") > 0)
            {
                config.enableCors = cli["enable-cors"].as<std::string>();
            }

            if (cli.count("rpc-access-token") > 0)
            {
                config.rpcAccessToken = cli["rpc-access-token"].as<std::string>();
            }

            if (cli.count("rpc-read-timeout") > 0)
            {
                config.rpcReadTimeout = std::max<uint32_t>(1, cli["rpc-read-timeout"].as<uint32_t>());
            }

            if (cli.count("rpc-write-timeout") > 0)
            {
                config.rpcWriteTimeout = std::max<uint32_t>(1, cli["rpc-write-timeout"].as<uint32_t>());
            }

            if (cli.count("rpc-max-body-bytes") > 0)
            {
                config.rpcMaxRequestBodyBytes = std::max<uint64_t>(1024, cli["rpc-max-body-bytes"].as<uint64_t>());
            }

            if (cli.count("rpc-max-rpm") > 0)
            {
                config.rpcMaxRequestsPerMinute = cli["rpc-max-rpm"].as<uint32_t>();
            }

            if (cli.count("rpc-max-global-index-range") > 0)
            {
                config.rpcMaxGlobalIndexesRange =
                    std::max<uint32_t>(100, cli["rpc-max-global-index-range"].as<uint32_t>());
            }

            if (cli.count("rpc-max-block-count") > 0)
            {
                config.rpcMaxBlockCount = std::max<uint32_t>(1, cli["rpc-max-block-count"].as<uint32_t>());
            }

            if (cli.count("rpc-trust-proxy") > 0)
            {
                config.rpcTrustProxy = cli["rpc-trust-proxy"].as<bool>();
            }

            if (cli.count("zmq-pub") > 0)
            {
                config.zmqPub = cli["zmq-pub"].as<std::string>();
            }

            if (cli.count("no-zmq") > 0)
            {
                config.noZmq = cli["no-zmq"].as<bool>();
            }

            if (cli.count("block-notify") > 0)
            {
                config.blockNotify = cli["block-notify"].as<std::string>();
            }

            if (cli.count("reorg-notify") > 0)
            {
                config.reorgNotify = cli["reorg-notify"].as<std::string>();
            }

            if (cli.count("tx-notify") > 0)
            {
                config.txNotify = cli["tx-notify"].as<std::string>();
            }

            if (cli.count("notify-during-sync") > 0)
            {
                config.notifyDuringSync = cli["notify-during-sync"].as<bool>();
            }

            if (cli.count("transaction-validation-threads") > 0)
            {
                config.transactionValidationThreads = cli["transaction-validation-threads"].as<uint32_t>();
            }

            if (cli.count("sync-max-peers") > 0)
            {
                config.syncMaxPeers = std::max<uint32_t>(1, cli["sync-max-peers"].as<uint32_t>());
            }

            if (cli.count("sync-peer-failure-threshold") > 0)
            {
                config.syncPeerFailureThreshold =
                    std::max<uint32_t>(1, cli["sync-peer-failure-threshold"].as<uint32_t>());
            }

            if (cli.count("sync-batch-min") > 0)
            {
                config.syncBatchMin = std::max<uint32_t>(1, cli["sync-batch-min"].as<uint32_t>());
            }

            if (cli.count("sync-batch-max") > 0)
            {
                config.syncBatchMax = std::max<uint32_t>(config.syncBatchMin, cli["sync-batch-max"].as<uint32_t>());
            }

            if (cli.count("block-sync-size") > 0)
            {
                config.blockSyncSize = std::max<uint32_t>(1, cli["block-sync-size"].as<uint32_t>());
            }

            if (cli.count("block-sync-bytes") > 0)
            {
                config.blockSyncBytes = std::max<uint64_t>(2 * 1024 * 1024, cli["block-sync-bytes"].as<uint64_t>());
            }

            if (cli.count("auto-prune-min-gap-blocks") > 0)
            {
                config.autoPruneMinGapBlocks = cli["auto-prune-min-gap-blocks"].as<uint32_t>();
            }

            if (cli.count("auto-compaction-min-gap-blocks") > 0)
            {
                config.autoCompactionMinGapBlocks = cli["auto-compaction-min-gap-blocks"].as<uint32_t>();
            }

            if (cli.count("auto-prune-min-free-bytes") > 0)
            {
                config.autoPruneMinFreeBytes = cli["auto-prune-min-free-bytes"].as<uint64_t>();
            }

            if (cli.count("auto-compaction-min-free-bytes") > 0)
            {
                config.autoCompactionMinFreeBytes = cli["auto-compaction-min-free-bytes"].as<uint64_t>();
            }

            config.syncMaxPeers = clampSyncMaxPeersToOutPeers(config.syncMaxPeers, config.p2pOutPeers, "CLI/default");

            if (config.help) // Do we want to display the help message?
            {
                std::cout << options.help({}) << std::endl;
                exit(0);
            }
            else if (config.version) // Do we want to display the software version?
            {
                std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
                exit(0);
            }
            else if (config.osVersion) // Do we want to display the OS version information?
            {
                std::cout << CryptoNote::getProjectCLIHeader() << "OS: " << Tools::get_os_version_string() << std::endl;
                exit(0);
            }
        }
        catch (const cxxopts::exceptions::exception &e)
        {
            std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl
                      << std::endl
                      << options.help({}) << std::endl;
            exit(1);
        }
    }

    bool updateConfigFormat(const std::string configFile, DaemonConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }

        static const std::regex cfgItem {R"x(\s*(\S[^ \t=]*)\s*=\s*((\s?\S+)+)\s*$)x"};
        static const std::regex cfgComment {R"x(\s*[;#])x"};
        std::smatch item;
        std::string cfgKey;
        std::string cfgValue;
        std::vector<std::string> exclusiveNodes;
        std::vector<std::string> priorityNodes;
        std::vector<std::string> seedNodes;
        std::vector<std::string> peers;
        std::string cors;
        bool updated = false;

        for (std::string line; std::getline(data, line);)
        {
            if (line.empty() || std::regex_match(line, item, cfgComment))
            {
                continue;
            }

            if (std::regex_match(line, item, cfgItem))
            {
                if (item.size() != 4)
                {
                    continue;
                }

                cfgKey = item[1].str();
                cfgValue = item[2].str();

                if (cfgKey.compare("data-dir") == 0)
                {
                    config.dataDirectory = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("load-checkpoints") == 0)
                {
                    config.checkPoints = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("log-file") == 0)
                {
                    config.logFile = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("log-level") == 0)
                {
                    try
                    {
                        config.logLevel = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("db-enable-compression") == 0)
                {
                    config.enableDbCompression = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("no-console") == 0)
                {
                    config.noConsole = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("skip-boot-compaction") == 0)
                {
                    config.skipBootCompaction = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("db-max-open-files") == 0)
                {
                    try
                    {
                        config.dbMaxOpenFiles = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("db-read-buffer-size") == 0)
                {
                    try
                    {
                        config.dbReadCacheSizeMB = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("db-threads") == 0)
                {
                    try
                    {
                        config.dbThreads = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("db-write-buffer-size") == 0)
                {
                    try
                    {
                        config.dbWriteBufferSizeMB = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("allow-local-ip") == 0)
                {
                    config.localIp = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("hide-my-port") == 0)
                {
                    config.hideMyPort = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("p2p-bind-ip") == 0)
                {
                    config.p2pInterface = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("p2p-bind-port") == 0)
                {
                    try
                    {
                        config.p2pPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("p2p-external-port") == 0)
                {
                    try
                    {
                        config.p2pExternalPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("out-peers") == 0)
                {
                    try
                    {
                        config.p2pOutPeers = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("in-peers") == 0)
                {
                    try
                    {
                        config.p2pInPeers = std::stoul(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-bind-ip") == 0)
                {
                    config.rpcInterface = cfgValue;
                    updated = true;
                }
                else if (cfgKey.find("rpc-bind-port") == 0)
                {
                    try
                    {
                        config.rpcPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("p2p-reset-peerstate") == 0)
                {
                    config.p2pResetPeerstate = cfgValue.at(0) == '1' ? true : false;
                    updated = true;
                }
                else if (cfgKey.compare("p2p-bind-ipv6-address") == 0)
                {
                    config.p2pBindIpv6Address = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("p2p-bind-port-ipv6") == 0)
                {
                    try
                    {
                        config.p2pBindPortIpv6 = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-bind-ipv6-address") == 0)
                {
                    config.rpcBindIpv6Address = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-use-ipv6") == 0)
                {
                    config.rpcUseIpv6 = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("add-exclusive-node") == 0)
                {
                    exclusiveNodes.push_back(cfgValue);
                    config.exclusiveNodes = exclusiveNodes;
                    updated = true;
                }
                else if (cfgKey.compare("add-peer") == 0)
                {
                    peers.push_back(cfgValue);
                    config.peers = peers;
                    updated = true;
                }
                else if (cfgKey.compare("add-priority-node") == 0)
                {
                    priorityNodes.push_back(cfgValue);
                    config.priorityNodes = priorityNodes;
                    updated = true;
                }
                else if (cfgKey.compare("seed-node") == 0)
                {
                    seedNodes.push_back(cfgValue);
                    config.seedNodes = seedNodes;
                    updated = true;
                }
                else if (cfgKey.compare("daemon-mode") == 0)
                {
                    config.daemonMode = normalizeDaemonMode(cfgValue);
                    updated = true;
                }
                else if (cfgKey.compare("enable-cors") == 0)
                {
                    cors = cfgValue;
                    config.enableCors = cors;
                    updated = true;
                }
                else if (cfgKey.compare("fee-address") == 0)
                {
                    /* Deprecated: accepted for backward compatibility, ignored. */
                    updated = true;
                }
                else if (cfgKey.compare("fee-amount") == 0)
                {
                    /* Deprecated: accepted for backward compatibility, ignored. */
                    updated = true;
                }
                else if (cfgKey.compare("rpc-access-token") == 0)
                {
                    config.rpcAccessToken = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-read-timeout") == 0)
                {
                    try
                    {
                        config.rpcReadTimeout = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-write-timeout") == 0)
                {
                    try
                    {
                        config.rpcWriteTimeout = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-max-body-bytes") == 0)
                {
                    try
                    {
                        config.rpcMaxRequestBodyBytes = std::max<uint64_t>(1024, std::stoull(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-max-rpm") == 0)
                {
                    try
                    {
                        config.rpcMaxRequestsPerMinute = std::stoul(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-max-global-index-range") == 0)
                {
                    try
                    {
                        config.rpcMaxGlobalIndexesRange = std::max<uint32_t>(100, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-max-block-count") == 0)
                {
                    try
                    {
                        config.rpcMaxBlockCount = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("rpc-trust-proxy") == 0)
                {
                    config.rpcTrustProxy = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("zmq-pub") == 0)
                {
                    config.zmqPub = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("no-zmq") == 0)
                {
                    config.noZmq = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("block-notify") == 0)
                {
                    config.blockNotify = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("reorg-notify") == 0)
                {
                    config.reorgNotify = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("tx-notify") == 0)
                {
                    config.txNotify = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("notify-during-sync") == 0)
                {
                    config.notifyDuringSync = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("transaction-validation-threads") == 0)
                {
                    try
                    {
                        config.transactionValidationThreads = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("sync-max-peers") == 0)
                {
                    try
                    {
                        config.syncMaxPeers = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("sync-peer-failure-threshold") == 0)
                {
                    try
                    {
                        config.syncPeerFailureThreshold = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("sync-batch-min") == 0)
                {
                    try
                    {
                        config.syncBatchMin = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("sync-batch-max") == 0)
                {
                    try
                    {
                        config.syncBatchMax = std::max<uint32_t>(config.syncBatchMin, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("block-sync-size") == 0)
                {
                    try
                    {
                        config.blockSyncSize = std::max<uint32_t>(1, std::stoul(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("block-sync-bytes") == 0)
                {
                    try
                    {
                        config.blockSyncBytes = std::max<uint64_t>(2 * 1024 * 1024, std::stoull(cfgValue));
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("auto-prune-min-gap-blocks") == 0)
                {
                    try
                    {
                        config.autoPruneMinGapBlocks = std::stoul(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("auto-compaction-min-gap-blocks") == 0)
                {
                    try
                    {
                        config.autoCompactionMinGapBlocks = std::stoul(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("auto-prune-min-free-bytes") == 0)
                {
                    try
                    {
                        config.autoPruneMinFreeBytes = std::stoull(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("auto-compaction-min-free-bytes") == 0)
                {
                    try
                    {
                        config.autoCompactionMinFreeBytes = std::stoull(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else
                {
                    for (auto c : cfgKey)
                    {
                        if (static_cast<unsigned char>(c) > 127)
                        {
                            throw std::runtime_error("Bad/invalid config file");
                        }
                    }
                    throw std::runtime_error("Unknown option: " + cfgKey);
                }
            }
        }

        config.syncMaxPeers = clampSyncMaxPeersToOutPeers(config.syncMaxPeers, config.p2pOutPeers, "config file");

        if (!updated)
        {
            return updated;
        }

        try
        {
            std::ifstream orig(configFile, std::ios::binary);
            std::ofstream backup(configFile + ".ini.bak", std::ios::binary);
            backup << orig.rdbuf();
        }
        catch (std::exception &e)
        {
            // pass
        }
        return updated;
    }

    void handleSettings(const std::string configFile, DaemonConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(data);
        } catch (const nlohmann::json::parse_error &) {
            throw std::runtime_error("Failed to parse the config file as JSON.");
        }

        if (j.contains("data-dir"))
        {
            config.dataDirectory = j["data-dir"].get<std::string>();
        }

        if (j.contains("load-checkpoints"))
        {
            config.checkPoints = j["load-checkpoints"].get<std::string>();
        }

        if (j.contains("log-file"))
        {
            config.logFile = j["log-file"].get<std::string>();
        }

        if (j.contains("log-level"))
        {
            config.logLevel = j["log-level"].get<int>();
        }

        config.dbMaxOpenFiles = CryptoNote::ROCKSDB_MAX_OPEN_FILES;
        config.dbReadCacheSizeMB = CryptoNote::ROCKSDB_READ_BUFFER_MB;
        config.dbWriteBufferSizeMB = CryptoNote::ROCKSDB_WRITE_BUFFER_MB;
        config.dbThreads = CryptoNote::ROCKSDB_BACKGROUND_THREADS;

        if (j.contains("db-enable-compression"))
        {
            config.enableDbCompression = j["db-enable-compression"].get<bool>();
        }

        if (j.contains("no-console"))
        {
            config.noConsole = j["no-console"].get<bool>();
        }

        if (j.contains("skip-boot-compaction"))
        {
            config.skipBootCompaction = j["skip-boot-compaction"].get<bool>();
        }

        if (j.contains("db-max-open-files"))
        {
            config.dbMaxOpenFiles = j["db-max-open-files"].get<int>();
        }

        if (j.contains("db-read-buffer-size"))
        {
            config.dbReadCacheSizeMB = j["db-read-buffer-size"].get<int>();
        }

        if (j.contains("db-threads"))
        {
            config.dbThreads = j["db-threads"].get<int>();
        }

        if (j.contains("db-write-buffer-size"))
        {
            config.dbWriteBufferSizeMB = j["db-write-buffer-size"].get<int>();
        }

        if (j.contains("allow-local-ip"))
        {
            config.localIp = j["allow-local-ip"].get<bool>();
        }

        if (j.contains("hide-my-port"))
        {
            config.hideMyPort = j["hide-my-port"].get<bool>();
        }

        if (j.contains("p2p-bind-ip"))
        {
            config.p2pInterface = j["p2p-bind-ip"].get<std::string>();
        }

        if (j.contains("p2p-bind-port"))
        {
            config.p2pPort = j["p2p-bind-port"].get<int>();
        }

        if (j.contains("p2p-external-port"))
        {
            config.p2pExternalPort = j["p2p-external-port"].get<int>();
        }

        if (j.contains("out-peers"))
        {
            config.p2pOutPeers = std::max<uint32_t>(1, j["out-peers"].get<uint32_t>());
        }

        if (j.contains("in-peers"))
        {
            config.p2pInPeers = j["in-peers"].get<uint32_t>();
        }

        if (j.contains("p2p-reset-peerstate"))
        {
            config.p2pResetPeerstate = j["p2p-reset-peerstate"].get<bool>();
        }

        if (j.contains("p2p-bind-ipv6-address"))
        {
            config.p2pBindIpv6Address = j["p2p-bind-ipv6-address"].get<std::string>();
        }

        if (j.contains("p2p-bind-port-ipv6"))
        {
            config.p2pBindPortIpv6 = j["p2p-bind-port-ipv6"].get<int>();
        }

        if (j.contains("rpc-bind-ipv6-address"))
        {
            config.rpcBindIpv6Address = j["rpc-bind-ipv6-address"].get<std::string>();
        }

        if (j.contains("rpc-use-ipv6"))
        {
            config.rpcUseIpv6 = j["rpc-use-ipv6"].get<bool>();
        }

        if (j.contains("rpc-bind-ip"))
        {
            config.rpcInterface = j["rpc-bind-ip"].get<std::string>();
        }

        if (j.contains("rpc-bind-port"))
        {
            config.rpcPort = j["rpc-bind-port"].get<int>();
        }

        if (j.contains("add-exclusive-node") && j["add-exclusive-node"].is_array())
        {
            for (const auto &v : j["add-exclusive-node"])
            {
                config.exclusiveNodes.push_back(v.get<std::string>());
            }
        }

        if (j.contains("add-peer") && j["add-peer"].is_array())
        {
            for (const auto &v : j["add-peer"])
            {
                config.peers.push_back(v.get<std::string>());
            }
        }

        if (j.contains("add-priority-node") && j["add-priority-node"].is_array())
        {
            for (const auto &v : j["add-priority-node"])
            {
                config.priorityNodes.push_back(v.get<std::string>());
            }
        }

        if (j.contains("seed-node") && j["seed-node"].is_array())
        {
            for (const auto &v : j["seed-node"])
            {
                config.seedNodes.push_back(v.get<std::string>());
            }
        }

        if (j.contains("daemon-mode"))
        {
            config.daemonMode = normalizeDaemonMode(j["daemon-mode"].get<std::string>());
        }

        if (j.contains("enable-cors"))
        {
            config.enableCors = j["enable-cors"].get<std::string>();
        }

        if (j.contains("fee-address"))
        {
            /* Deprecated: accepted for backward compatibility, ignored. */
        }

        if (j.contains("fee-amount"))
        {
            /* Deprecated: accepted for backward compatibility, ignored. */
        }

        if (j.contains("rpc-access-token"))
        {
            config.rpcAccessToken = j["rpc-access-token"].get<std::string>();
        }

        if (j.contains("rpc-read-timeout"))
        {
            config.rpcReadTimeout = std::max<uint32_t>(1, j["rpc-read-timeout"].get<uint32_t>());
        }

        if (j.contains("rpc-write-timeout"))
        {
            config.rpcWriteTimeout = std::max<uint32_t>(1, j["rpc-write-timeout"].get<uint32_t>());
        }

        if (j.contains("rpc-max-body-bytes"))
        {
            config.rpcMaxRequestBodyBytes = std::max<uint64_t>(1024, j["rpc-max-body-bytes"].get<uint64_t>());
        }

        if (j.contains("rpc-max-rpm"))
        {
            config.rpcMaxRequestsPerMinute = j["rpc-max-rpm"].get<uint32_t>();
        }

        if (j.contains("rpc-max-global-index-range"))
        {
            config.rpcMaxGlobalIndexesRange = std::max<uint32_t>(100, j["rpc-max-global-index-range"].get<uint32_t>());
        }

        if (j.contains("rpc-max-block-count"))
        {
            config.rpcMaxBlockCount = std::max<uint32_t>(1, j["rpc-max-block-count"].get<uint32_t>());
        }

        if (j.contains("rpc-trust-proxy"))
        {
            config.rpcTrustProxy = j["rpc-trust-proxy"].get<bool>();
        }

        if (j.contains("zmq-pub"))
        {
            config.zmqPub = j["zmq-pub"].get<std::string>();
        }

        if (j.contains("no-zmq"))
        {
            config.noZmq = j["no-zmq"].get<bool>();
        }

        if (j.contains("block-notify"))
        {
            config.blockNotify = j["block-notify"].get<std::string>();
        }

        if (j.contains("reorg-notify"))
        {
            config.reorgNotify = j["reorg-notify"].get<std::string>();
        }

        if (j.contains("tx-notify"))
        {
            config.txNotify = j["tx-notify"].get<std::string>();
        }

        if (j.contains("notify-during-sync"))
        {
            config.notifyDuringSync = j["notify-during-sync"].get<bool>();
        }

        if (j.contains("transaction-validation-threads"))
        {
            config.transactionValidationThreads = j["transaction-validation-threads"].get<int>();
        }

        if (j.contains("sync-max-peers"))
        {
            config.syncMaxPeers = std::max<uint32_t>(1, j["sync-max-peers"].get<uint32_t>());
        }

        if (j.contains("sync-peer-failure-threshold"))
        {
            config.syncPeerFailureThreshold = std::max<uint32_t>(1, j["sync-peer-failure-threshold"].get<uint32_t>());
        }

        if (j.contains("sync-batch-min"))
        {
            config.syncBatchMin = std::max<uint32_t>(1, j["sync-batch-min"].get<uint32_t>());
        }

        if (j.contains("sync-batch-max"))
        {
            config.syncBatchMax = std::max<uint32_t>(config.syncBatchMin, j["sync-batch-max"].get<uint32_t>());
        }

        if (j.contains("block-sync-size"))
        {
            config.blockSyncSize = std::max<uint32_t>(1, j["block-sync-size"].get<uint32_t>());
        }

        if (j.contains("block-sync-bytes"))
        {
            config.blockSyncBytes = std::max<uint64_t>(2 * 1024 * 1024, j["block-sync-bytes"].get<uint64_t>());
        }

        if (j.contains("auto-prune-min-gap-blocks"))
        {
            config.autoPruneMinGapBlocks = j["auto-prune-min-gap-blocks"].get<uint32_t>();
        }

        if (j.contains("auto-compaction-min-gap-blocks"))
        {
            config.autoCompactionMinGapBlocks = j["auto-compaction-min-gap-blocks"].get<uint32_t>();
        }

        if (j.contains("auto-prune-min-free-bytes"))
        {
            config.autoPruneMinFreeBytes = j["auto-prune-min-free-bytes"].get<uint64_t>();
        }

        if (j.contains("auto-compaction-min-free-bytes"))
        {
            config.autoCompactionMinFreeBytes = j["auto-compaction-min-free-bytes"].get<uint64_t>();
        }

        if (j.contains("prune"))
        {
            config.prune = j["prune"].get<bool>();
        }

        if (j.contains("prune-depth"))
        {
            config.pruneDepth = clampPruneDepth(j["prune-depth"].get<uint32_t>(), "config file");
        }

        config.syncMaxPeers = clampSyncMaxPeersToOutPeers(config.syncMaxPeers, config.p2pOutPeers, "config file");
    }

    nlohmann::json asJSON(const DaemonConfiguration &config)
    {
        nlohmann::json j;

        j["data-dir"] = config.dataDirectory;
        j["load-checkpoints"] = config.checkPoints;
        j["log-file"] = config.logFile;
        j["log-level"] = config.logLevel;
        j["no-console"] = config.noConsole;
        j["skip-boot-compaction"] = config.skipBootCompaction;
        j["db-enable-compression"] = config.enableDbCompression;
        j["db-max-open-files"] = config.dbMaxOpenFiles;
        j["db-read-buffer-size"] = config.dbReadCacheSizeMB;
        j["db-threads"] = config.dbThreads;
        j["db-write-buffer-size"] = config.dbWriteBufferSizeMB;
        j["allow-local-ip"] = config.localIp;
        j["hide-my-port"] = config.hideMyPort;
        j["p2p-bind-ip"] = config.p2pInterface;
        j["p2p-bind-port"] = config.p2pPort;
        j["p2p-external-port"] = config.p2pExternalPort;
        j["out-peers"] = config.p2pOutPeers;
        j["in-peers"] = config.p2pInPeers;
        j["p2p-reset-peerstate"] = config.p2pResetPeerstate;
        j["p2p-bind-ipv6-address"] = config.p2pBindIpv6Address;
        j["p2p-bind-port-ipv6"] = config.p2pBindPortIpv6;
        j["rpc-bind-ipv6-address"] = config.rpcBindIpv6Address;
        j["rpc-use-ipv6"] = config.rpcUseIpv6;
        j["rpc-bind-ip"] = config.rpcInterface;
        j["rpc-bind-port"] = config.rpcPort;
        j["add-exclusive-node"] = config.exclusiveNodes;
        j["add-peer"] = config.peers;
        j["add-priority-node"] = config.priorityNodes;
        j["seed-node"] = config.seedNodes;
        j["enable-cors"] = config.enableCors;
        j["daemon-mode"] = config.daemonMode;
        j["rpc-access-token"] = config.rpcAccessToken;
        j["rpc-read-timeout"] = config.rpcReadTimeout;
        j["rpc-write-timeout"] = config.rpcWriteTimeout;
        j["rpc-max-body-bytes"] = config.rpcMaxRequestBodyBytes;
        j["rpc-max-rpm"] = config.rpcMaxRequestsPerMinute;
        j["rpc-max-global-index-range"] = config.rpcMaxGlobalIndexesRange;
        j["rpc-max-block-count"] = config.rpcMaxBlockCount;
        j["rpc-trust-proxy"] = config.rpcTrustProxy;
        j["zmq-pub"] = config.zmqPub;
        j["no-zmq"] = config.noZmq;
        j["block-notify"] = config.blockNotify;
        j["reorg-notify"] = config.reorgNotify;
        j["tx-notify"] = config.txNotify;
        j["notify-during-sync"] = config.notifyDuringSync;
        j["transaction-validation-threads"] = config.transactionValidationThreads;
        j["prune"] = config.prune;
        j["prune-depth"] = config.pruneDepth;
        j["sync-max-peers"] = config.syncMaxPeers;
        j["sync-peer-failure-threshold"] = config.syncPeerFailureThreshold;
        j["sync-batch-min"] = config.syncBatchMin;
        j["sync-batch-max"] = config.syncBatchMax;
        j["block-sync-size"] = config.blockSyncSize;
        j["block-sync-bytes"] = config.blockSyncBytes;
        j["auto-prune-min-gap-blocks"] = config.autoPruneMinGapBlocks;
        j["auto-compaction-min-gap-blocks"] = config.autoCompactionMinGapBlocks;
        j["auto-prune-min-free-bytes"] = config.autoPruneMinFreeBytes;
        j["auto-compaction-min-free-bytes"] = config.autoCompactionMinFreeBytes;

        return j;
    }

    std::string asString(const DaemonConfiguration &config)
    {
        nlohmann::json j = asJSON(config);
        return j.dump(2);
    }

    void asFile(const DaemonConfiguration &config, const std::string &filename)
    {
        nlohmann::json j = asJSON(config);
        std::ofstream data(filename);
        data << std::setw(2) << j;
    }
} // namespace DaemonConfig
