// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DaemonConfiguration.h"

#include "common/FileSystemShim.h"
#include "common/PathTools.h"
#include "common/Util.h"
#include "json.hpp"

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <config/NetworkParameters.h>
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

        std::string defaultDataDirectoryForNetwork(const CryptoNote::NetworkType networkType)
        {
            const std::string base = Tools::getDefaultDataDirectory();
            const auto &network = CryptoNote::getNetworkParameters(networkType);
            if (network.dataDirectorySuffix.empty())
            {
                return base;
            }

            return (fs::path(base) / network.dataDirectorySuffix).string();
        }

        std::string normalizeNetworkName(const std::string &rawNetwork)
        {
            CryptoNote::NetworkType networkType;
            if (!CryptoNote::parseNetworkType(rawNetwork, networkType))
            {
                throw std::runtime_error("Invalid network: '" + rawNetwork + "'. Allowed values are 'mainnet' or 'testnet'.");
            }

            return CryptoNote::networkTypeToString(networkType);
        }

        void applyNetworkDefaults(
            DaemonConfiguration &config,
            const bool setDataDir,
            const bool setP2pPort,
            const bool setRpcPort,
            const bool setZmqPub,
            const bool setSeedNodes)
        {
            const auto &network = CryptoNote::getNetworkParameters(config.networkType);
            if (setDataDir)
            {
                config.dataDirectory = defaultDataDirectoryForNetwork(config.networkType);
            }
            if (setP2pPort)
            {
                config.p2pPort = network.p2pPort;
            }
            if (setRpcPort)
            {
                config.rpcPort = network.rpcPort;
            }
            if (setZmqPub)
            {
                config.zmqPub = "tcp://127.0.0.1:" + std::to_string(network.zmqPubPort);
            }
            if (setSeedNodes)
            {
                config.seedNodes = network.seedNodes;
            }
        }
    } // namespace

    DaemonConfiguration initConfiguration(const char *path)
    {
        DaemonConfiguration config;
        applyNetworkDefaults(config, true, true, true, true, true);
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
            "lite",
            "Enable lite-node mode: store full blocks only from --lite-height upward. Permanent for this database",
            cxxopts::value<bool>(config.lite)->default_value("false")->implicit_value("true"))(
            "lite-height",
            "Height at and above which a lite node stores full block data (required with --lite)",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.liteHeight)),
            "#")(
            "snapshot-stats",
            "Report per-table record counts and byte totals, then exit. Takes minutes on a synced chain",
            cxxopts::value<bool>(config.snapshotStats)->default_value("false")->implicit_value("true"))(
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
            "#")(
            "snapshot-info",
            "Print what a lite node snapshot file contains, as JSON, and exit",
            cxxopts::value<std::string>()->default_value(config.snapshotInfo),
            "<file>")(
            "import-lite-snapshot",
            "Load a lite node snapshot into an empty database, then exit. Needs --lite and the --lite-height the "
            "snapshot was made at",
            cxxopts::value<std::string>()->default_value(config.importLiteSnapshot),
            "<file>");

        options.add_options("Genesis Block")(
            "print-genesis-tx",
            "Print the genesis block transaction hex and exits",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "c,config-file", "Specify the <path> to a configuration file", cxxopts::value<std::string>(), "<path>")(
            "network",
            "Network profile to use (mainnet or testnet)",
            cxxopts::value<std::string>(),
            "<mainnet|testnet>")(
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
            "attach",
            "Attach a console to a daemon already running on this machine, over its RPC IPC socket "
            "(an absolute path, @name or ipc://path), instead of starting a node",
            cxxopts::value<std::string>(),
            "<socket>")(
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
            "Maximum allowed blockCount for wallet/raw-block sync RPC methods. Larger values let wallets sync in "
            "fewer, bigger requests, which matters most for remote wallets bounded by --rpc-max-requests-per-minute. "
            "Responses are additionally capped by size, so raising this is safe during a transaction flood",
            cxxopts::value<uint32_t>()->default_value(std::to_string(config.rpcMaxBlockCount)),
            "#")(
            "rpc-sync-cache-size",
            "Megabytes of finished wallet sync responses to keep. Wallets syncing past the same height ask "
            "for the same range, so this serves them from one build instead of rebuilding it per wallet. "
            "0 disables the cache",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.rpcSyncCacheBytes / (1024 * 1024))),
            "#")(
            "rpc-trust-proxy",
            "Trust X-Forwarded-For header for client IP (enable only behind trusted reverse proxy)",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "rpc-ipc-path",
            "Also serve RPC on a local IPC socket at this path, for example /run/wrkzd/wrkzd.sock. "
            "Prefix with @ for the Linux abstract namespace. Empty disables IPC (default). Not available on Windows",
            cxxopts::value<std::string>()->default_value(config.rpcIpcPath),
            "<path>")(
            "rpc-ipc-mode",
            "Octal permissions for the IPC socket file. The default 0600 restricts it to the user running the "
            "daemon; use 0660 together with --rpc-ipc-group to share it",
            cxxopts::value<std::string>()->default_value(Common::Ipc::formatMode(config.rpcIpcMode)),
            "<mode>")(
            "rpc-ipc-group",
            "Group to own the IPC socket file, for a 0660 shared setup",
            cxxopts::value<std::string>()->default_value(config.rpcIpcGroup),
            "<group>")(
            "rpc-ipc-require-token",
            "Also demand --rpc-access-token from IPC callers. Off by default: the socket permissions already "
            "decide who may connect, and the kernel enforces them",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "zmq-pub",
            "ZMQ PUB endpoint (for example tcp://127.0.0.1:"
                + std::to_string(CryptoNote::ZMQ_PUB_DEFAULT_PORT) + "). Empty disables ZMQ publisher.",
            cxxopts::value<std::string>()->default_value(config.zmqPub),
            "<address>")(
            "no-zmq",
            "Disable ZMQ publisher even if zmq-pub is set",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "stratum-bind-ip",
            "Interface for the built-in stratum server to listen on",
            cxxopts::value<std::string>()->default_value(config.stratumBindIp),
            "<ip>")(
            "stratum-bind-port",
            "Port for the built-in stratum server, so a miner can mine straight to this node. 0 disables it",
            cxxopts::value<uint16_t>()->default_value(std::to_string(config.stratumBindPort)),
            "#")(
            "stratum-share-difficulty",
            "Difficulty stratum miners are given. 0 uses the network difficulty, so a miner only reports when it has "
            "found a block; a lower value makes it report progress as well",
            cxxopts::value<uint64_t>()->default_value(std::to_string(config.stratumShareDifficulty)),
            "#")(
            "stratum-max-connections",
            "Maximum number of miners allowed on the stratum server at once",
            cxxopts::value<size_t>()->default_value(std::to_string(config.stratumMaxConnections)),
            "#")(
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
            "mn-signing-key",
            "Masternode signing private key (64 hex chars). Enables ChainLock/InstantSend signing.",
            cxxopts::value<std::string>()->default_value(""),
            "<hex>")(
            "mn-operator-key",
            "Masternode operator private key (64 hex chars, printed by mn_register). Enables automated heartbeat submission.",
            cxxopts::value<std::string>()->default_value(""),
            "<hex>")(
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
            ("db-compression-dict-bytes",
             "Size of the per-SST ZSTD dictionary in bytes, 0 to disable. Trades compaction time for a "
             "smaller database; only applies to newly written files, so run `compact_db force` after changing it",
             cxxopts::value<uint64_t>()->default_value("0"),
             "#")
            ("db-block-size",
             "Size of an uncompressed SST data block in kilobytes. Larger compresses better and reads slower",
             cxxopts::value<uint64_t>()->default_value("4"),
             "#")
            ("db-compression-level",
             "ZSTD level for the bottommost database level, 0 for RocksDB's default of 3. Higher compresses "
             "harder for the same decompression speed, paying only in compaction time",
             cxxopts::value<int>()->default_value("0"),
             "#")
            ("db-row-cache-percent",
             "Percentage of the read buffer given to the row cache, 0 for the built-in eighth. A row cache hit "
             "skips block decompression entirely",
             cxxopts::value<uint64_t>()->default_value("0"),
             "#")
            ("db-bottom-filters",
             "Keep bloom filters on the bottommost database level. Costs space, but spent key image checks are "
             "lookups that are meant to miss, and without filters each one reads and decompresses a block",
             cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
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
            const bool hasCliNetwork = cli.count("network") > 0;
            const bool hasCliDataDir = cli.count("data-dir") > 0;
            const bool hasCliP2pPort = cli.count("p2p-bind-port") > 0;
            const bool hasCliRpcPort = cli.count("rpc-bind-port") > 0;
            const bool hasCliZmqPub = cli.count("zmq-pub") > 0;
            const bool hasCliSeedNodes = cli.count("seed-node") > 0;

            if (hasCliNetwork)
            {
                const auto normalizedNetwork = normalizeNetworkName(cli["network"].as<std::string>());
                CryptoNote::NetworkType networkType;
                CryptoNote::parseNetworkType(normalizedNetwork, networkType);
                config.networkType = networkType;
                applyNetworkDefaults(
                    config,
                    !hasCliDataDir,
                    !hasCliP2pPort,
                    !hasCliRpcPort,
                    !hasCliZmqPub,
                    !hasCliSeedNodes);
            }

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

            if (cli.count("lite") > 0)
            {
                config.lite = cli["lite"].as<bool>();
            }

            if (cli.count("lite-height") > 0)
            {
                config.liteHeight = cli["lite-height"].as<uint32_t>();
            }

            if (cli.count("snapshot-info") > 0)
            {
                config.snapshotInfo = cli["snapshot-info"].as<std::string>();
            }

            if (cli.count("import-lite-snapshot") > 0)
            {
                config.importLiteSnapshot = cli["import-lite-snapshot"].as<std::string>();
            }

            if (cli.count("snapshot-stats") > 0)
            {
                config.snapshotStats = cli["snapshot-stats"].as<bool>();
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

            if (cli.count("db-compression-level") > 0)
            {
                config.dbCompressionLevel = cli["db-compression-level"].as<int>();
            }

            if (cli.count("db-row-cache-percent") > 0)
            {
                config.dbRowCachePercent = cli["db-row-cache-percent"].as<uint64_t>();
            }

            if (cli.count("db-bottom-filters") > 0)
            {
                config.dbBottommostFilters = cli["db-bottom-filters"].as<bool>();
            }

            if (cli.count("db-compression-dict-bytes") > 0)
            {
                config.dbCompressionDictBytes = cli["db-compression-dict-bytes"].as<uint64_t>();
            }

            if (cli.count("db-block-size") > 0)
            {
                config.dbBlockSizeKB = cli["db-block-size"].as<uint64_t>();
            }

            if (cli.count("db-enable-compression") > 0)
            {
                config.enableDbCompression = cli["db-enable-compression"].as<bool>();
            }

            if (cli.count("no-console") > 0)
            {
                config.noConsole = cli["no-console"].as<bool>();
            }

            if (cli.count("attach") > 0)
            {
                config.attach = cli["attach"].as<std::string>();
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

            if (cli.count("mn-signing-key") > 0)
            {
                config.mnSigningKey = cli["mn-signing-key"].as<std::string>();
            }

            if (cli.count("mn-operator-key") > 0)
            {
                config.mnOperatorKey = cli["mn-operator-key"].as<std::string>();
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

            if (cli.count("rpc-sync-cache-size") > 0)
            {
                config.rpcSyncCacheBytes = cli["rpc-sync-cache-size"].as<uint64_t>() * 1024 * 1024;
            }

            if (cli.count("rpc-trust-proxy") > 0)
            {
                config.rpcTrustProxy = cli["rpc-trust-proxy"].as<bool>();
            }

            if (cli.count("rpc-ipc-path") > 0)
            {
                config.rpcIpcPath = cli["rpc-ipc-path"].as<std::string>();
            }

            if (cli.count("rpc-ipc-mode") > 0)
            {
                const std::string mode = cli["rpc-ipc-mode"].as<std::string>();

                if (!Common::Ipc::parseMode(mode, config.rpcIpcMode))
                {
                    throw std::runtime_error("rpc-ipc-mode must be octal permissions such as 0600: " + mode);
                }
            }

            if (cli.count("rpc-ipc-group") > 0)
            {
                config.rpcIpcGroup = cli["rpc-ipc-group"].as<std::string>();
            }

            if (cli.count("rpc-ipc-require-token") > 0)
            {
                config.rpcIpcRequireToken = cli["rpc-ipc-require-token"].as<bool>();
            }

            if (cli.count("zmq-pub") > 0)
            {
                config.zmqPub = cli["zmq-pub"].as<std::string>();
            }

            if (cli.count("no-zmq") > 0)
            {
                config.noZmq = cli["no-zmq"].as<bool>();
            }

            if (cli.count("stratum-bind-ip") > 0)
            {
                config.stratumBindIp = cli["stratum-bind-ip"].as<std::string>();
            }

            if (cli.count("stratum-bind-port") > 0)
            {
                config.stratumBindPort = cli["stratum-bind-port"].as<uint16_t>();
            }

            if (cli.count("stratum-share-difficulty") > 0)
            {
                config.stratumShareDifficulty = cli["stratum-share-difficulty"].as<uint64_t>();
            }

            if (cli.count("stratum-max-connections") > 0)
            {
                config.stratumMaxConnections = cli["stratum-max-connections"].as<size_t>();
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
        bool hasConfigDataDir = false;
        bool hasConfigP2pPort = false;
        bool hasConfigRpcPort = false;
        bool hasConfigZmqPub = false;
        bool hasConfigSeedNodes = false;
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
                    hasConfigDataDir = true;
                    updated = true;
                }
                else if (cfgKey.compare("network") == 0)
                {
                    const auto normalizedNetwork = normalizeNetworkName(cfgValue);
                    CryptoNote::NetworkType networkType;
                    CryptoNote::parseNetworkType(normalizedNetwork, networkType);
                    config.networkType = networkType;
                    applyNetworkDefaults(
                        config,
                        !hasConfigDataDir,
                        !hasConfigP2pPort,
                        !hasConfigRpcPort,
                        !hasConfigZmqPub,
                        !hasConfigSeedNodes);
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
                else if (cfgKey.compare("db-compression-level") == 0)
                {
                    config.dbCompressionLevel = std::stoi(cfgValue);
                }
                else if (cfgKey.compare("db-row-cache-percent") == 0)
                {
                    config.dbRowCachePercent = std::stoull(cfgValue);
                }
                else if (cfgKey.compare("db-bottom-filters") == 0)
                {
                    config.dbBottommostFilters = (cfgValue == "true" || cfgValue == "1");
                }
                else if (cfgKey.compare("db-compression-dict-bytes") == 0)
                {
                    config.dbCompressionDictBytes = std::stoull(cfgValue);
                }
                else if (cfgKey.compare("db-block-size") == 0)
                {
                    config.dbBlockSizeKB = std::stoull(cfgValue);
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
                        hasConfigP2pPort = true;
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
                        hasConfigRpcPort = true;
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
                else if (cfgKey.compare("mn-signing-key") == 0)
                {
                    config.mnSigningKey = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("mn-operator-key") == 0)
                {
                    config.mnOperatorKey = cfgValue;
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
                    hasConfigSeedNodes = true;
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
                else if (cfgKey.compare("rpc-sync-cache-size") == 0)
                {
                    try
                    {
                        config.rpcSyncCacheBytes = std::stoull(cfgValue) * 1024 * 1024;
                    }
                    catch (const std::exception &)
                    {
                        throw std::runtime_error("rpc-sync-cache-size must be a number: " + cfgValue);
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
                else if (cfgKey.compare("rpc-ipc-path") == 0)
                {
                    config.rpcIpcPath = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-ipc-mode") == 0)
                {
                    if (!Common::Ipc::parseMode(cfgValue, config.rpcIpcMode))
                    {
                        throw std::runtime_error("Invalid value for " + cfgKey + ", expected octal such as 0600");
                    }

                    updated = true;
                }
                else if (cfgKey.compare("rpc-ipc-group") == 0)
                {
                    config.rpcIpcGroup = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-ipc-require-token") == 0)
                {
                    config.rpcIpcRequireToken = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("zmq-pub") == 0)
                {
                    config.zmqPub = cfgValue;
                    hasConfigZmqPub = true;
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

        if (j.contains("network"))
        {
            const auto normalizedNetwork = normalizeNetworkName(j["network"].get<std::string>());
            CryptoNote::NetworkType networkType;
            CryptoNote::parseNetworkType(normalizedNetwork, networkType);
            config.networkType = networkType;
            applyNetworkDefaults(
                config,
                !j.contains("data-dir"),
                !j.contains("p2p-bind-port"),
                !j.contains("rpc-bind-port"),
                !j.contains("zmq-pub"),
                !j.contains("seed-node"));
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

        if (j.contains("db-compression-level"))
        {
            config.dbCompressionLevel = j["db-compression-level"].get<int>();
        }

        if (j.contains("db-row-cache-percent"))
        {
            config.dbRowCachePercent = j["db-row-cache-percent"].get<uint64_t>();
        }

        if (j.contains("db-bottom-filters"))
        {
            config.dbBottommostFilters = j["db-bottom-filters"].get<bool>();
        }

        if (j.contains("db-compression-dict-bytes"))
        {
            config.dbCompressionDictBytes = j["db-compression-dict-bytes"].get<uint64_t>();
        }

        if (j.contains("db-block-size"))
        {
            config.dbBlockSizeKB = j["db-block-size"].get<uint64_t>();
        }

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

        if (j.contains("mn-signing-key"))
        {
            config.mnSigningKey = j["mn-signing-key"].get<std::string>();
        }

        if (j.contains("mn-operator-key"))
        {
            config.mnOperatorKey = j["mn-operator-key"].get<std::string>();
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

        if (j.contains("rpc-sync-cache-size"))
        {
            config.rpcSyncCacheBytes = j["rpc-sync-cache-size"].get<uint64_t>() * 1024 * 1024;
        }

        if (j.contains("rpc-max-block-count"))
        {
            config.rpcMaxBlockCount = std::max<uint32_t>(1, j["rpc-max-block-count"].get<uint32_t>());
        }

        if (j.contains("rpc-trust-proxy"))
        {
            config.rpcTrustProxy = j["rpc-trust-proxy"].get<bool>();
        }

        if (j.contains("rpc-ipc-path"))
        {
            config.rpcIpcPath = j["rpc-ipc-path"].get<std::string>();
        }

        if (j.contains("rpc-ipc-mode"))
        {
            const std::string mode = j["rpc-ipc-mode"].get<std::string>();

            if (!Common::Ipc::parseMode(mode, config.rpcIpcMode))
            {
                throw std::runtime_error("Invalid value for rpc-ipc-mode, expected octal such as 0600: " + mode);
            }
        }

        if (j.contains("rpc-ipc-group"))
        {
            config.rpcIpcGroup = j["rpc-ipc-group"].get<std::string>();
        }

        if (j.contains("rpc-ipc-require-token"))
        {
            config.rpcIpcRequireToken = j["rpc-ipc-require-token"].get<bool>();
        }

        if (j.contains("zmq-pub"))
        {
            config.zmqPub = j["zmq-pub"].get<std::string>();
        }

        if (j.contains("no-zmq"))
        {
            config.noZmq = j["no-zmq"].get<bool>();
        }

        if (j.contains("stratum-bind-ip"))
        {
            config.stratumBindIp = j["stratum-bind-ip"].get<std::string>();
        }

        if (j.contains("stratum-bind-port"))
        {
            config.stratumBindPort = j["stratum-bind-port"].get<uint16_t>();
        }

        if (j.contains("stratum-share-difficulty"))
        {
            config.stratumShareDifficulty = j["stratum-share-difficulty"].get<uint64_t>();
        }

        if (j.contains("stratum-max-connections"))
        {
            config.stratumMaxConnections = j["stratum-max-connections"].get<size_t>();
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
        j["network"] = CryptoNote::networkTypeToString(config.networkType);
        j["load-checkpoints"] = config.checkPoints;
        j["log-file"] = config.logFile;
        j["log-level"] = config.logLevel;
        j["no-console"] = config.noConsole;
        j["skip-boot-compaction"] = config.skipBootCompaction;
        j["db-enable-compression"] = config.enableDbCompression;
        j["db-compression-dict-bytes"] = config.dbCompressionDictBytes;
        j["db-compression-level"] = config.dbCompressionLevel;
        j["db-row-cache-percent"] = config.dbRowCachePercent;
        j["db-bottom-filters"] = config.dbBottommostFilters;
        j["db-block-size"] = config.dbBlockSizeKB;
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
        j["mn-signing-key"] = config.mnSigningKey;
        j["mn-operator-key"] = config.mnOperatorKey;
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
        j["rpc-sync-cache-size"] = config.rpcSyncCacheBytes / (1024 * 1024);
        j["rpc-trust-proxy"] = config.rpcTrustProxy;
        j["rpc-ipc-path"] = config.rpcIpcPath;
        j["rpc-ipc-mode"] = Common::Ipc::formatMode(config.rpcIpcMode);
        j["rpc-ipc-group"] = config.rpcIpcGroup;
        j["rpc-ipc-require-token"] = config.rpcIpcRequireToken;
        j["zmq-pub"] = config.zmqPub;
        j["no-zmq"] = config.noZmq;
        j["stratum-bind-ip"] = config.stratumBindIp;
        j["stratum-bind-port"] = config.stratumBindPort;
        j["stratum-share-difficulty"] = config.stratumShareDifficulty;
        j["stratum-max-connections"] = config.stratumMaxConnections;
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
