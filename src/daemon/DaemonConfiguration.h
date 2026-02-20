// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "common/PathTools.h"
#include "common/Util.h"

#include <config/CryptoNoteConfig.h>
#include <logging/ILogger.h>
#include <rapidjson/document.h>
#include <thread>

using namespace rapidjson;

namespace DaemonConfig
{
    struct DaemonConfiguration
    {
        static constexpr uint32_t MIN_PRUNE_DEPTH_DAYS = 7;
        static constexpr uint32_t MIN_PRUNE_DEPTH =
            CryptoNote::parameters::EXPECTED_NUMBER_OF_BLOCKS_PER_DAY * MIN_PRUNE_DEPTH_DAYS;
        static constexpr uint32_t DEFAULT_PRUNE_DEPTH =
            MIN_PRUNE_DEPTH;
        static constexpr const char *DAEMON_MODE_STANDARD = "standard";
        static constexpr const char *DAEMON_MODE_EXPLORER = "explorer";

        DaemonConfiguration()
        {
            std::stringstream logfile;
            logfile << CryptoNote::CRYPTONOTE_NAME << "d.log";

            dataDirectory = Tools::getDefaultDataDirectory();
            checkPoints = "default";
            logFile = logfile.str();
            logLevel = Logging::WARNING;
            rewindToHeight = 0;
            p2pInterface = "0.0.0.0";
            p2pPort = CryptoNote::P2P_DEFAULT_PORT;
            p2pExternalPort = 0;
            p2pOutPeers = CryptoNote::P2P_DEFAULT_CONNECTIONS_COUNT;
            p2pInPeers = CryptoNote::P2P_DEFAULT_CONNECTIONS_COUNT;
            transactionValidationThreads = std::thread::hardware_concurrency();
            rpcInterface = "127.0.0.1";
            rpcPort = CryptoNote::RPC_DEFAULT_PORT;
            noConsole = false;
            daemonMode = DAEMON_MODE_STANDARD;
            localIp = false;
            hideMyPort = false;
            p2pResetPeerstate = false;
            help = false;
            version = false;
            osVersion = false;
            printGenesisTx = false;
            dumpConfig = false;
            enableDbCompression = true;
            resync = false;
            importChain = false;
            exportChain = false;
            exportNumBlocks = 0;
            prune = false;
            pruneDepth = DEFAULT_PRUNE_DEPTH;
            syncMaxPeers = 3;
            syncPeerFailureThreshold = 2;
            syncBatchMin = 120;
            syncBatchMax = 600;
            blockSyncSize = syncBatchMax;
            blockSyncBytes = 16ULL * 1024ULL * 1024ULL;
            rpcAccessToken = "";
            rpcReadTimeout = 15;
            rpcWriteTimeout = 30;
            rpcMaxRequestBodyBytes = 2 * 1024 * 1024;
            rpcMaxRequestsPerMinute = 240;
            rpcMaxGlobalIndexesRange = 5000;
            rpcMaxBlockCount = 100;
            rpcTrustProxy = false;
            skipBootCompaction = false;
            autoPruneMinGapBlocks = 120;
            autoCompactionMinGapBlocks = 720;
            autoPruneMinFreeBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
            autoCompactionMinFreeBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
        }

        std::string dataDirectory;

        std::string logFile;

        std::string feeAddress;

        std::string rpcInterface;

        std::string p2pInterface;

        std::string checkPoints;

        std::vector<std::string> peers;

        std::vector<std::string> priorityNodes;

        std::vector<std::string> exclusiveNodes;

        std::vector<std::string> seedNodes;

        std::string enableCors;

        int logLevel;

        int feeAmount;

        int rpcPort;

        int p2pPort;

        int p2pExternalPort;

        uint32_t p2pOutPeers;

        uint32_t p2pInPeers;

        uint32_t transactionValidationThreads;

        uint64_t dbThreads;

        uint64_t dbMaxOpenFiles;

        uint64_t dbWriteBufferSizeMB;

        uint64_t dbReadCacheSizeMB;

        uint32_t rewindToHeight;

        bool noConsole;

        std::string daemonMode;

        bool localIp;

        bool hideMyPort;

        bool resync;

        bool p2pResetPeerstate;

        bool importChain;

        bool exportChain;

        uint32_t exportNumBlocks;

        bool prune;

        uint32_t pruneDepth;

        uint32_t syncMaxPeers;

        uint32_t syncPeerFailureThreshold;

        uint32_t syncBatchMin;

        uint32_t syncBatchMax;

        uint32_t blockSyncSize;

        uint64_t blockSyncBytes;

        std::string rpcAccessToken;

        uint32_t rpcReadTimeout;

        uint32_t rpcWriteTimeout;

        uint64_t rpcMaxRequestBodyBytes;

        uint32_t rpcMaxRequestsPerMinute;

        uint32_t rpcMaxGlobalIndexesRange;

        uint32_t rpcMaxBlockCount;

        bool rpcTrustProxy;

        bool skipBootCompaction;

        uint32_t autoPruneMinGapBlocks;

        uint32_t autoCompactionMinGapBlocks;

        uint64_t autoPruneMinFreeBytes;

        uint64_t autoCompactionMinFreeBytes;

        std::string configFile;

        std::string outputFile;

        std::vector<std::string> genesisAwardAddresses;

        bool help;

        bool version;

        bool osVersion;

        bool printGenesisTx;

        bool dumpConfig;

        bool enableDbCompression;
    };

    DaemonConfiguration initConfiguration(const char *path);

    bool updateConfigFormat(const std::string configFile, DaemonConfiguration &config);

    void handleSettings(int argc, char *argv[], DaemonConfiguration &config);

    void handleSettings(const std::string configFile, DaemonConfiguration &config);

    void asFile(const DaemonConfiguration &config, const std::string &filename);

    std::string asString(const DaemonConfiguration &config);

    Document asJSON(const DaemonConfiguration &config);
} // namespace DaemonConfig
