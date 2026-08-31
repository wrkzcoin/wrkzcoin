// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The Karai Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DaemonCommandsHandler.h"
#include "DaemonConfiguration.h"
#include "ChainNotifier.h"
#include "ZmqPublisher.h"
#include "common/CryptoNoteTools.h"
#include "common/FileSystemShim.h"
#include "common/PathTools.h"
#include "common/ScopeExit.h"
#include "common/SignalHandler.h"
#include "common/IpcSocket.h"
#include "common/StdInputStream.h"
#include "common/StdOutputStream.h"
#include "common/Util.h"
#include "crypto/hash.h"
#include "cryptonotecore/Core.h"
#include "cryptonotecore/Currency.h"
#include "cryptonotecore/DatabaseBlockchainCache.h"
#include "cryptonotecore/DatabaseBlockchainCacheFactory.h"
#include "cryptonotecore/RocksDBWrapper.h"
#include "cryptonoteprotocol/CryptoNoteProtocolHandler.h"
#include "p2p/NetNode.h"
#include "p2p/NetNodeConfig.h"
#include "rpc/RpcServer.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"

#include <common/FileSystemShim.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteCheckpoints.h>
#include <logging/LoggerManager.h>
#include <logger/Logger.h>
#include <atomic>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>

#if defined(WIN32)

#undef ERROR
#include <crtdbg.h>
#include <io.h>

#else
#include <unistd.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;
using namespace DaemonConfig;

namespace
{
    const std::string DAEMON_MODE_PROFILE_KEY = "daemon_mode_profile";

    /* Records how this database was built, so a later run cannot silently treat
       an index-only chain as a complete one. Value is "full", or "lite:<height>".
       See LITENODE.md. */
    const std::string LITE_PROFILE_KEY = "lite_node_profile";

    /* Written by DatabaseBlockchainCache the first time a database is opened.
       Its absence is what tells us a database is brand new, which is the only
       point at which lite mode may be chosen. Must match DB_VERSION_KEY in
       DatabaseBlockchainCache.cpp. */
    const std::string DB_SCHEME_VERSION_KEY = "db_scheme_version";

    /* Sum of the SST and log files a database directory occupies. Zero if the
       directory cannot be walked, which the caller treats as "do not report it"
       rather than as a size. */
    uint64_t directorySize(const fs::path &directory)
    {
        std::error_code ec;
        uint64_t bytes = 0;

        fs::recursive_directory_iterator it(directory, ec), end;

        if (ec)
        {
            return 0;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                break;
            }

            std::error_code entryEc;

            if (!it->is_regular_file(entryEc) || entryEc)
            {
                continue;
            }

            const auto size = it->file_size(entryEc);

            if (!entryEc)
            {
                bytes += size;
            }
        }

        return bytes;
    }

    class DaemonModeProfileReadBatch : public IReadBatch
    {
      public:
        explicit DaemonModeProfileReadBatch(std::string key = DAEMON_MODE_PROFILE_KEY): key(std::move(key)) {}

        virtual ~DaemonModeProfileReadBatch() {}

        virtual std::vector<std::string> getRawKeys() const override
        {
            return {key};
        }

        virtual void submitRawResult(const std::vector<std::string> &values, const std::vector<bool> &resultStates) override
        {
            if (values.size() != 1 || resultStates.size() != 1 || !resultStates[0])
            {
                return;
            }

            storedMode = values[0];
        }

        std::optional<std::string> getStoredMode() const
        {
            return storedMode;
        }

      private:
        std::string key;

        std::optional<std::string> storedMode;
    };

    class DaemonModeProfileWriteBatch : public IWriteBatch
    {
      public:
        explicit DaemonModeProfileWriteBatch(std::string mode, std::string key = DAEMON_MODE_PROFILE_KEY):
            key(std::move(key)),
            daemonMode(std::move(mode))
        {
        }

        virtual ~DaemonModeProfileWriteBatch() {}

        virtual std::vector<std::pair<std::string, std::string>> extractRawDataToInsert() override
        {
            return {std::make_pair(key, daemonMode)};
        }

        virtual std::vector<std::string> extractRawKeysToRemove() override
        {
            return {};
        }

      private:
        std::string key;

        std::string daemonMode;
    };

    std::optional<std::string> readStringSetting(IDataBase &database, const std::string &key)
    {
        DaemonModeProfileReadBatch readBatch(key);
        const auto error = database.read(readBatch);
        if (error)
        {
            throw std::system_error(error);
        }

        return readBatch.getStoredMode();
    }

    void writeStringSetting(IDataBase &database, const std::string &key, const std::string &value)
    {
        DaemonModeProfileWriteBatch writeBatch(value, key);
        const auto error = database.write(writeBatch);
        if (error)
        {
            throw std::system_error(error);
        }
    }

    std::optional<std::string> readDaemonModeProfile(IDataBase &database)
    {
        return readStringSetting(database, DAEMON_MODE_PROFILE_KEY);
    }

    void writeDaemonModeProfile(IDataBase &database, const std::string &mode)
    {
        writeStringSetting(database, DAEMON_MODE_PROFILE_KEY, mode);
    }

    /* Settles what lite height this database runs at, and refuses to run at all
       when the flags and the database disagree. Whether a chain is stored in full
       or index-only is baked in the moment the first block is written, so it can
       never be changed later - only rebuilt from scratch.

       Every disagreement here exits rather than recreating the database. Dropping
       a chain because an operator forgot a flag would be the worst possible
       reading of their intent, so the removal is always left to them.

       Returns the lite height to build the cache with; 0 means full storage. */
    uint32_t resolveLiteProfile(
        IDataBase &database,
        const DaemonConfiguration &config,
        Logging::LoggerRef &logger)
    {
        const auto storedProfile = readStringSetting(database, LITE_PROFILE_KEY);

        /* No scheme version yet means DatabaseBlockchainCache has never opened
           this database, so there is nothing in it to contradict. */
        const bool databaseIsNew = !readStringSetting(database, DB_SCHEME_VERSION_KEY).has_value();

        std::optional<uint32_t> storedLiteHeight;

        if (storedProfile && storedProfile->rfind("lite:", 0) == 0)
        {
            try
            {
                storedLiteHeight = static_cast<uint32_t>(std::stoul(storedProfile->substr(5)));
            }
            catch (const std::exception &)
            {
                logger(ERROR, BRIGHT_RED)
                    << "The lite-node marker in this database is unreadable ('" << *storedProfile
                    << "'). Refusing to start rather than guess how it was built. Remove the data directory to "
                       "rebuild.";
                exit(1);
            }
        }

        if (!config.lite)
        {
            if (storedLiteHeight)
            {
                logger(ERROR, BRIGHT_RED)
                    << "This database was built as a lite node from height " << *storedLiteHeight
                    << ", so it does not hold the block data a full node serves. Restart with --lite --lite-height "
                    << *storedLiteHeight << ", or delete the data directory to sync a full node from scratch.";
                exit(1);
            }

            return 0;
        }

        /* --lite from here down. */
        if (config.liteHeight == 0)
        {
            logger(ERROR, BRIGHT_RED) << "--lite requires --lite-height, the height from which full block data is "
                                         "kept. There is no sensible default: it decides what this node can never "
                                         "serve or rescan again.";
            exit(1);
        }

        if (config.prune)
        {
            logger(ERROR, BRIGHT_RED)
                << "--lite and --prune cannot be combined. Pruning below the lite height would remove nothing, and "
                   "above it would break the promise a lite node makes to serve every block from its lite height up.";
            exit(1);
        }

        /* Every explorer endpoint reads the transaction records a lite node drops,
           so below the lite height they answer with nothing or fail outright. That
           is a node that looks like it works and quietly reports an incomplete
           chain, which is worse than one that refuses to start. */
        if (config.daemonMode == DaemonConfiguration::DAEMON_MODE_EXPLORER)
        {
            logger(ERROR, BRIGHT_RED)
                << "--lite and --daemon-mode explorer cannot be combined. Block and transaction lookups below the "
                   "lite height need the transaction records a lite node never stores, so the explorer endpoints "
                   "would return nothing for those heights rather than report an error.";
            exit(1);
        }

        if (storedLiteHeight)
        {
            if (*storedLiteHeight != config.liteHeight)
            {
                logger(ERROR, BRIGHT_RED)
                    << "This database was built as a lite node from height " << *storedLiteHeight << ", not "
                    << config.liteHeight
                    << ". The stored height cannot be changed - blocks below it were never written. Restart with "
                       "--lite-height "
                    << *storedLiteHeight << ", or delete the data directory to rebuild at a different height.";
                exit(1);
            }

            return *storedLiteHeight;
        }

        if (!databaseIsNew)
        {
            logger(ERROR, BRIGHT_RED)
                << "--lite can only be chosen for a new database. This one already holds a chain that was synced in "
                   "full, and nothing here will delete it for you. Point --data-dir at an empty directory, or remove "
                   "this one yourself, to build a lite node.";
            exit(1);
        }

        writeStringSetting(database, LITE_PROFILE_KEY, "lite:" + std::to_string(config.liteHeight));

        logger(INFO, BRIGHT_GREEN) << "Lite node mode enabled from height " << config.liteHeight
                                   << ". This is permanent for this database.";

        return config.liteHeight;
    }
} // namespace

void print_genesis_tx_hex(const bool blockExplorerMode, std::shared_ptr<LoggerManager> logManager)
{
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.isBlockexplorer(blockExplorerMode);

    CryptoNote::Currency currency = currencyBuilder.currency();

    const auto transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();

    std::string transactionHex = Common::toHex(CryptoNote::toBinaryArray(transaction));
    std::cout << getProjectCLIHeader() << std::endl
              << std::endl
              << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:"
              << std::endl
              << "const char GENESIS_COINBASE_TX_HEX[] = \"" << transactionHex << "\";" << std::endl;

    return;
}

JsonValue buildLoggerConfiguration(Level level, const std::string &logfile)
{
    JsonValue loggerConfiguration(JsonValue::OBJECT);
    loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

    JsonValue &cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

    JsonValue &fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    fileLogger.insert("type", "file");
    fileLogger.insert("filename", logfile);
    fileLogger.insert("level", static_cast<int64_t>(TRACE));

    JsonValue &consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    consoleLogger.insert("type", "console");
    consoleLogger.insert("level", static_cast<int64_t>(TRACE));
    consoleLogger.insert("pattern", "%D %T %L ");

    return loggerConfiguration;
}

int main(int argc, char *argv[])
{
    fs::path temp = fs::path(argv[0]).filename();
    DaemonConfiguration config = initConfiguration(temp.string().c_str());

#ifdef WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const auto logManager = std::make_shared<LoggerManager>();
    LoggerRef logger(logManager, "daemon");

    // Initial loading of CLI parameters
    handleSettings(argc, argv, config);

    if (config.printGenesisTx) // Do we weant to generate the Genesis Tx?
    {
        print_genesis_tx_hex(false, logManager);
        exit(0);
    }

    // If the user passed in the --config-file option, we need to handle that first
    if (!config.configFile.empty())
    {
        try
        {
            if (updateConfigFormat(config.configFile, config))
            {
                std::cout << std::endl << "Updating daemon configuration format..." << std::endl;
                asFile(config, config.configFile);
            }
        }
        catch (std::runtime_error &e)
        {
            std::cout
                << std::endl
                << "There was an error parsing the specified configuration file. Please check the file and try again:"
                << std::endl
                << e.what() << std::endl;
            exit(1);
        }
        catch (std::exception &e)
        {
            // pass
        }

        try
        {
            handleSettings(config.configFile, config);
        }
        catch (std::exception &e)
        {
            std::cout
                << std::endl
                << "There was an error parsing the specified configuration file. Please check the file and try again"
                << std::endl
                << e.what() << std::endl;
            exit(1);
        }
    }

    // Load in the CLI specified parameters again to overwrite anything from the config file
    handleSettings(argc, argv, config);

    if (config.dumpConfig)
    {
        std::cout << getProjectCLIHeader() << asString(config) << std::endl;
        exit(0);
    }
    else if (!config.outputFile.empty())
    {
        try
        {
            asFile(config, config.outputFile);
            std::cout << getProjectCLIHeader() << "Configuration saved to: " << config.outputFile << std::endl;
            exit(0);
        }
        catch (std::exception &e)
        {
            std::cout << getProjectCLIHeader() << "Could not save configuration to: " << config.outputFile << std::endl
                      << e.what() << std::endl;
            exit(1);
        }
    }

    /* If we were given the resync arg, we're deleting everything */
    if (config.resync)
    {
        std::error_code ec;

        std::vector<fs::path> removablePaths = {
            fs::path(config.dataDirectory) / CryptoNote::parameters::P2P_NET_DATA_FILENAME,
            fs::path(config.dataDirectory) / "DB"
        };

        for (const auto &path : removablePaths)
        {
            fs::remove_all(path, ec);

            if (ec)
            {
                std::cout << "Could not delete data path: " << path << std::endl;
                exit(1);
            }
        }
    }

    if (config.p2pPort <= 1024 || config.p2pPort > 65535)
    {
        std::cout << "P2P Port must be between 1024 and 65,535" << std::endl;
        exit(1);
    }

    if (config.p2pExternalPort < 0 || config.p2pExternalPort > 65535)
    {
        std::cout << "P2P External Port must be between 0 and 65,535" << std::endl;
        exit(1);
    }

    if (config.rpcPort <= 1024 || config.rpcPort > 65535)
    {
        std::cout << "RPC Port must be between 1024 and 65,535" << std::endl;
        exit(1);
    }

    try
    {
        fs::path cwdPath = fs::current_path();
        auto modulePath = cwdPath / temp;
        auto cfgLogFile = fs::path(config.logFile);

        if (cfgLogFile.empty())
        {
            cfgLogFile = modulePath.replace_extension(".log");
        }
        else
        {
            if (!cfgLogFile.has_parent_path())
            {
                cfgLogFile = modulePath.parent_path() / cfgLogFile;
            }
        }

        Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + config.logLevel);

        // configure logging
        logManager->configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile.string()));

        Logger::logger.setLogLevel(Logger::DEBUG);

        /* New logger, for now just passing through messages to old logger */
        Logger::logger.setLogCallback([&logger](
                const std::string prettyMessage,
                const std::string message,
                const Logger::LogLevel level,
                const std::vector<Logger::LogCategory> categories) {
            Logging::Level oldLogLevel;
            std::string logColour;

            if (level == Logger::DEBUG)
            {
                oldLogLevel = Logging::DEBUGGING;
                logColour = Logging::DEFAULT;
            }
            else if (level == Logger::INFO)
            {
                oldLogLevel = Logging::INFO;
                logColour = Logging::DEFAULT;
            }
            else if (level == Logger::WARNING)
            {
                oldLogLevel = Logging::WARNING;
                logColour = Logging::RED;
            }
            else if (level == Logger::FATAL)
            {
                oldLogLevel = Logging::FATAL;
                logColour = Logging::RED;
            }
            /* setLogCallback shouldn't get called if log level is DISABLED */
            else
            {
                throw std::runtime_error("Programmer error @ setLogCallback in Daemon.cpp");
            }

            logger(oldLogLevel, logColour) << message;
        });

        logger(INFO, BRIGHT_GREEN) << getProjectCLIHeader() << std::endl;

        logger(INFO) << "Program Working Directory: " << cwdPath;

        // create objects and link them
        const bool explorerMode = config.daemonMode == DaemonConfiguration::DAEMON_MODE_EXPLORER;
        logger(INFO) << "Daemon mode: " << config.daemonMode;

        CryptoNote::CurrencyBuilder currencyBuilder(logManager);
        currencyBuilder.isBlockexplorer(explorerMode);

        try
        {
            currencyBuilder.currency();
        }
        catch (std::exception &)
        {
            std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: "
                      << CryptoNote::CRYPTONOTE_NAME << "d --print-genesis-tx" << std::endl;
            return 1;
        }
        CryptoNote::Currency currency = currencyBuilder.currency();

        DataBaseConfig dbConfig(
            config.dataDirectory,
            config.dbThreads,
            config.dbMaxOpenFiles,
            config.dbWriteBufferSizeMB,
            config.dbReadCacheSizeMB,
            config.enableDbCompression
        );

        dbConfig.compressionDictBytes = config.dbCompressionDictBytes;
        dbConfig.blockSize = std::max<uint64_t>(config.dbBlockSizeKB, 1) * 1024;
        dbConfig.compressionLevel = config.dbCompressionLevel;
        dbConfig.rowCachePercent = config.dbRowCachePercent;
        dbConfig.bottommostFilters = config.dbBottommostFilters;

        bool use_checkpoints = !config.checkPoints.empty();
        CryptoNote::Checkpoints checkpoints(logManager);

        if (use_checkpoints)
        {
            logger(INFO) << "Loading Checkpoints for faster initial sync...";
            if (config.checkPoints == "default")
            {
                for (const auto &cp : CryptoNote::CHECKPOINTS)
                {
                    checkpoints.addCheckpoint(cp.index, cp.blockId);
                }
                logger(INFO) << "Loaded " << CryptoNote::CHECKPOINTS.size() << " default checkpoints";
            }
            else
            {
                bool results = checkpoints.loadCheckpointsFromFile(config.checkPoints);
                if (!results)
                {
                    throw std::runtime_error("Failed to load checkpoints");
                }
            }
        }

        NetNodeConfig netNodeConfig;
        netNodeConfig.init(
            config.p2pInterface,
            config.p2pPort,
            config.p2pExternalPort,
            config.p2pOutPeers,
            config.p2pInPeers,
            config.localIp,
            config.hideMyPort,
            config.dataDirectory,
            config.peers,
            config.exclusiveNodes,
            config.priorityNodes,
            config.seedNodes,
            config.p2pResetPeerstate,
            config.p2pBindIpv6Address,
            config.p2pBindPortIpv6);

        if (!Tools::create_directories_if_necessary(dbConfig.dataDir))
        {
            throw std::runtime_error("Can't create directory: " + dbConfig.dataDir);
        }

        std::shared_ptr<IDataBase> database;
        database = std::make_shared<RocksDBWrapper>(logManager, dbConfig);

        database->init();
        Tools::ScopeExit dbShutdownOnExit([&database]() { database->shutdown(); });

        if (!DatabaseBlockchainCache::checkDBSchemeVersion(*database, logManager))
        {
            dbShutdownOnExit.cancel();

            database->shutdown();
            database->destroy();
            database->init();

            dbShutdownOnExit.resume();
        }

        /* Settled before the cache exists, because the lite height decides what
           the very first block written stores. Exits on any disagreement between
           the flags and what this database was built as. */
        const uint32_t liteHeight = resolveLiteProfile(*database, config, logger);

        System::Dispatcher dispatcher;
        logger(INFO) << "Initializing core...";

        const auto ccore = std::make_shared<CryptoNote::Core>(
            currency,
            logManager,
            std::move(checkpoints),
            dispatcher,
            std::unique_ptr<IBlockchainCacheFactory>(
                new DatabaseBlockchainCacheFactory(*database, logger.getLogger(), liteHeight)),
            config.transactionValidationThreads
        );

        ccore->load();

        logger(INFO) << "Core initialized OK";

        const auto storedDaemonMode = readDaemonModeProfile(*database);
        const bool hasHistoricalBlocks = ccore->getTopBlockIndex() > 0;

        if (storedDaemonMode)
        {
            if (*storedDaemonMode != config.daemonMode)
            {
                logger(ERROR) << "Daemon mode mismatch for existing database. Stored mode: " << *storedDaemonMode
                              << ", requested mode: " << config.daemonMode
                              << ". Reindex is required before switching mode. Restart with --resync --daemon-mode "
                              << config.daemonMode;
                return 1;
            }
        }
        else
        {
            if (hasHistoricalBlocks && config.daemonMode == DaemonConfiguration::DAEMON_MODE_EXPLORER)
            {
                logger(ERROR) << "Daemon database has historical blocks but no stored daemon mode profile. "
                              << "To ensure explorer index consistency, reindex is required. "
                              << "Restart with --resync --daemon-mode explorer";
                return 1;
            }

            writeDaemonModeProfile(*database, config.daemonMode);
            logger(INFO) << "Stored daemon mode profile in database: " << config.daemonMode;
        }

        std::string error;
        std::string filepath = "blockchain.dump";

        auto startTimer = std::chrono::high_resolution_clock::now();

        const bool performExpensiveValidation = false;
        auto elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;
        if (config.importChain)
        {
            logger(INFO) << "Importing blockchain...";
            error = ccore->importBlockchain(filepath, performExpensiveValidation);
            elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;
            if (error != "")
            {
                logger(ERROR) << "Failed to import "
                              << "blockchain: " << error;
                exit(1);
            }
            else
            {
                std::cout << "Time to import "
                          << std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count()
                          << " seconds." << std::endl
                          << std::endl;
                exit(0);
            }
        } else if (config.exportChain)
        {
            logger(INFO) << "Exporting blockchain...";
            error = ccore->exportBlockchain(filepath, config.exportNumBlocks);
            elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;
            if (error != "")
            {
                logger(ERROR) << "Failed to export "
                              << "blockchain: " << error;
                exit(1);
            }
            else
            {
                std::cout << "Time to export "
                          << std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count()
                          << " seconds." << std::endl
                          << std::endl;
                exit(0);
            }
        }

        /* If we were told to rewind the blockchain to a certain height
           we will remove blocks until we're back at the height specified */
        if (config.rewindToHeight > 0)
        {
            /* Rewinding into the index-only region would need block bodies that
               were never stored, and would leave the chain unable to move
               forward again. */
            if (liteHeight != 0 && config.rewindToHeight < liteHeight)
            {
                logger(ERROR, BRIGHT_RED)
                    << "Cannot rewind to " << config.rewindToHeight << " on a lite node whose full block data starts "
                    << "at " << liteHeight << ". The blocks below that height were never stored.";
                exit(1);
            }

            logger(INFO) << "Rewinding blockchain to: " << config.rewindToHeight << std::endl;

            ccore->rewind(config.rewindToHeight);
        }

        if (config.snapshotStats)
        {
            logger(INFO) << "Measuring database storage. This walks every key and takes minutes on a synced chain...";

            const auto stats = ccore->measureStorage();

            uint64_t snapshotBytes = 0;
            uint64_t snapshotRecords = 0;
            uint64_t totalBytes = 0;

            const auto mb = [](const uint64_t bytes) {
                std::ostringstream out;
                out << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
                return out.str();
            };

            std::cout << std::endl
                      << "Storage by table, logical bytes (top block " << ccore->getTopBlockIndex() << ")" << std::endl
                      << std::string(78, '-') << std::endl;

            for (const auto &entry : stats)
            {
                totalBytes += entry.second.totalBytes();

                if (entry.first.find("(snapshot)") != std::string::npos)
                {
                    snapshotBytes += entry.second.totalBytes();
                    snapshotRecords += entry.second.records;
                }

                std::cout << std::left << std::setw(36) << entry.first << std::right << std::setw(14)
                          << entry.second.records << std::setw(16) << mb(entry.second.totalBytes()) << std::endl;
            }

            std::cout << std::string(78, '-') << std::endl
                      << std::left << std::setw(36) << "TOTAL measured (logical)" << std::right << std::setw(14) << ""
                      << std::setw(16) << mb(totalBytes) << std::endl
                      << std::left << std::setw(36) << "Snapshot payload (logical)" << std::right << std::setw(14)
                      << snapshotRecords << std::setw(16) << mb(snapshotBytes) << std::endl;

            /* Without this the totals read as a directory size and are wrong by
               the compression ratio - which on a synced chain is about three.
               Every decision about snapshot size was being made against the
               logical number, so print what the thing actually occupies. */
            const uint64_t onDiskBytes = directorySize(fs::path(dbConfig.dataDir) / "DB");

            if (onDiskBytes > 0)
            {
                std::ostringstream ratio;
                ratio << std::fixed << std::setprecision(2)
                      << (static_cast<double>(totalBytes) / static_cast<double>(onDiskBytes)) << "x";

                std::cout << std::left << std::setw(36) << "On disk (compressed)" << std::right << std::setw(14) << ""
                          << std::setw(16) << mb(onDiskBytes) << std::endl
                          << std::left << std::setw(36) << "Compression ratio" << std::right << std::setw(14) << ""
                          << std::setw(16) << ratio.str() << std::endl;
            }

            std::cout << std::endl
                      << "These are logical key and value bytes, not what the database occupies." << std::endl
                      << "RocksDB compresses them on the way to disk, and what compresses best is" << std::endl
                      << "the per-key field-name framing - the very thing a packed snapshot format" << std::endl
                      << "would remove. So a packed dump saves far less than the logical totals" << std::endl
                      << "suggest; the floor is the high-entropy payload. The (snapshot) rows are" << std::endl
                      << "what a lite node snapshot must carry. See LITENODE.md." << std::endl
                      << std::endl;

            exit(0);
        }

        if (config.prune)
        {
            logger(INFO) << "Prune mode enabled. Pruning stored raw blocks with depth " << config.pruneDepth << "...";
            const auto prunedBlocks = ccore->pruneRawBlocks(config.pruneDepth);
            logger(INFO) << "Prune pass completed. Raw block slots processed: " << prunedBlocks;
        }

        const auto cprotocol = std::make_shared<CryptoNote::CryptoNoteProtocolHandler>(
            currency,
            dispatcher,
            *ccore,
            nullptr,
            logManager
        );

        cprotocol->setPrunedNodeConfig(config.prune, config.pruneDepth);
        cprotocol->setLiteNodeConfig(liteHeight);
        cprotocol->setSyncTuning(
            config.syncMaxPeers,
            config.syncPeerFailureThreshold,
            config.syncBatchMin,
            config.syncBatchMax,
            config.blockSyncSize,
            config.blockSyncBytes);

        const auto p2psrv = std::make_shared<CryptoNote::NodeServer>(
            dispatcher,
            *cprotocol,
            logManager
        );

        RpcMode rpcMode = explorerMode ? RpcMode::Explorer : RpcMode::Standard;

        std::string rpcIpcPath = config.rpcIpcPath;

        if (!rpcIpcPath.empty() && !Common::Ipc::supported())
        {
            logger(WARNING) << "Ignoring --rpc-ipc-path: " << Common::Ipc::unsupportedReason() << ".";
            rpcIpcPath = "";
        }

        if (!rpcIpcPath.empty())
        {
            if ((config.rpcIpcMode & 0007) != 0)
            {
                logger(WARNING) << "--rpc-ipc-mode " << Common::Ipc::formatMode(config.rpcIpcMode)
                                << " leaves the RPC socket open to every user on this machine.";
            }
            else if ((config.rpcIpcMode & 0070) != 0 && config.rpcIpcGroup.empty())
            {
                logger(WARNING) << "--rpc-ipc-mode " << Common::Ipc::formatMode(config.rpcIpcMode)
                                << " grants group access, but no --rpc-ipc-group was given, so the socket keeps the "
                                   "daemon user's primary group.";
            }

            if (Common::Ipc::isAbstract(rpcIpcPath))
            {
                logger(WARNING) << "Abstract namespace socket " << rpcIpcPath
                                << " carries no permissions; every process in this network namespace can reach the RPC.";
            }
        }

        RpcServer rpcServer(
            config.rpcPort,
            config.rpcInterface,
            config.rpcBindIpv6Address,
            config.rpcUseIpv6,
            config.enableCors,
            config.rpcAccessToken,
            config.rpcReadTimeout,
            config.rpcWriteTimeout,
            config.rpcMaxRequestBodyBytes,
            config.rpcMaxRequestsPerMinute,
            config.rpcMaxGlobalIndexesRange,
            config.rpcMaxBlockCount,
            config.rpcSyncCacheBytes,
            config.rpcTrustProxy,
            rpcIpcPath,
            config.rpcIpcMode,
            config.rpcIpcGroup,
            config.rpcIpcRequireToken,
            rpcMode,
            ccore,
            p2psrv,
            cprotocol
        );

        cprotocol->set_p2p_endpoint(&(*p2psrv));
        logger(INFO) << "Initializing p2p server...";
        if (!p2psrv->init(netNodeConfig))
        {
            logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
            return 1;
        }

        logger(INFO) << "P2p server initialized OK";

        // Fire up the RPC Server
        logger(INFO) << "Starting core rpc server on address " << config.rpcInterface << ":" << config.rpcPort;

        /* Whether responses get compressed is fixed when the daemon is built.
           Without it every syncing wallet pulls several times as many bytes,
           which is easy to miss because nothing else reports it. */
        if (std::string(RpcServer::compressionAlgorithm()) == "none")
        {
            logger(WARNING) << "RPC response compression is disabled - this build found no zlib at configure time. "
                            << "Syncing wallets will transfer several times more data than necessary. "
                            << "Install the zlib development package and rebuild to enable it.";
        }
        else
        {
            logger(INFO) << "RPC response compression: " << RpcServer::compressionAlgorithm();
        }

        if (config.rpcUseIpv6 && !config.rpcBindIpv6Address.empty())
        {
            logger(INFO) << "Starting core rpc server on IPv6 address [" << config.rpcBindIpv6Address << "]:" << config.rpcPort;
        }

        if (!rpcIpcPath.empty())
        {
            logger(INFO) << "Starting core rpc server on local " << Common::Ipc::describe(rpcIpcPath);
        }

        rpcServer.start();

        std::unique_ptr<Daemon::ZmqPublisher> zmqPublisher;
#ifdef WRKZ_ENABLE_ZMQ
        if (config.noZmq)
        {
            if (!config.zmqPub.empty())
            {
                logger(INFO) << "ZMQ publisher disabled by --no-zmq.";
            }
        }
        else if (!config.zmqPub.empty())
        {
            zmqPublisher =
                std::make_unique<Daemon::ZmqPublisher>(dispatcher, *ccore, logManager, config.zmqPub, liteHeight);
            if (!zmqPublisher->start())
            {
                logger(WARNING) << "Failed to start ZMQ publisher on " << config.zmqPub << ". Continuing without ZMQ.";
                zmqPublisher.reset();
            }
        }
#endif

        /* Monero-style --block-notify / --reorg-notify / --tx-notify hooks.
           Delivery runs on its own worker threads; the dispatcher only enqueues. */
        std::unique_ptr<Daemon::ChainNotifier> chainNotifier;
        if (!config.blockNotify.empty() || !config.reorgNotify.empty() || !config.txNotify.empty())
        {
            chainNotifier = std::make_unique<Daemon::ChainNotifier>(
                dispatcher,
                *ccore,
                *cprotocol,
                logManager,
                config.blockNotify,
                config.reorgNotify,
                config.txNotify,
                config.notifyDuringSync);

            if (!chainNotifier->start())
            {
                logger(WARNING) << "No usable notification hook configured. Continuing without notifications.";
                chainNotifier.reset();
            }
        }

        /* Get the RPC IP address and port we are bound to */
        auto [ip, port] = rpcServer.getConnectionInfo();

        /* If we bound the RPC to 0.0.0.0, we can't reach that with a
           standard HTTP client from anywhere. Instead, let's use the
           localhost IP address to reach ourselves */
        if (ip == "0.0.0.0")
        {
            ip = "127.0.0.1";
        }

        /* Empty unless the IPC listener actually came up, so the console never
           gets pointed at a socket that failed to bind */
        DaemonCommandsHandler dch(*ccore, *p2psrv, logManager, ip, port, config, rpcServer.getIpcPath());
        dch.start_boot_compaction_if_needed();

        if (!config.noConsole)
        {
            dch.start_handling();
        }

        std::atomic<uint32_t> interruptCount {0};
        Tools::SignalHandler::install([&dch, &interruptCount] {
            const uint32_t count = ++interruptCount;

            if (count == 1)
            {
                std::cout
                    << "SIGINT received. Starting graceful shutdown and waiting for safe DB close "
                       "(flush/WAL sync/background compaction). Press CTRL+C again to force exit."
                          << std::endl;
                dch.exit({});
                return;
            }

            std::cerr << "Second interrupt received. Forcing immediate exit without waiting for shutdown." << std::endl;
            std::_Exit(1);
        });

        logger(INFO) << "Starting p2p net loop...";
        p2psrv->run();
        logger(INFO) << "p2p net loop stopped";

        dch.stop_handling();
        dch.stop_compaction_scheduler();
        dch.wait_for_background_compaction();

        // stop components
        if (chainNotifier)
        {
            logger(INFO) << "Stopping chain notifier...";
            chainNotifier->stop();
        }

        if (zmqPublisher)
        {
            logger(INFO) << "Stopping ZMQ publisher...";
            zmqPublisher->stop();
        }

        logger(INFO) << "Stopping core rpc server...";
        rpcServer.stop();

        // deinitialize components
        logger(INFO) << "Deinitializing p2p...";
        p2psrv->deinit();

        cprotocol->set_p2p_endpoint(nullptr);
        ccore->save();

        logger(INFO) << "Flushing and closing blockchain DB...";
        dbShutdownOnExit.cancel();
        database->shutdown();
    }
    catch (const std::exception &e)
    {
        logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
        return 1;
    }

    logger(INFO) << "Node stopped.";
    return 0;
}
