// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "httplib.h"
#include "version.h"
#include "JsonHelper.h"

#include <charconv>
#include <cmath>
#include <common/IpcSocket.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/CryptoNoteFormatUtils.h>
#include <cryptonotecore/Currency.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandler.h>
#include <ctime>
#include <daemon/DaemonCommandsHandler.h>
#include <p2p/NetNode.h>
#include <rpc/JsonRpc.h>
#include <serialization/SerializationTools.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/String.h>
#include <utilities/Utilities.h>
#include <common/CheckDifficulty.h>
#include <common/StringTools.h>
#include <common/FileSystemShim.h>
#include <map>
#include <chrono>
#include <thread>
#include <fstream>
#include <limits>
#include <system/IpAddress.h>
#include <system/Ipv4Address.h>
#include <p2p/P2pProtocolTypes.h>

namespace
{
    const char *COMPACTION_MARKER_FILE = ".compact_db_in_progress";
    constexpr uint64_t AUTO_COMPACTION_CHECK_INTERVAL_FAST_SECONDS = 60;
    constexpr uint64_t AUTO_COMPACTION_CHECK_INTERVAL_SLOW_SECONDS = 30 * 60;
    constexpr uint64_t AUTO_COMPACTION_NEAR_SYNC_LAG_BLOCKS = 2;
    constexpr uint64_t AUTO_COMPACTION_RESYNC_LAG_BLOCKS = 20;
    constexpr uint32_t AUTO_COMPACTION_NEAR_SYNC_STREAK_REQUIRED = 3;

    /* Floor on wall clock time between automatic compactions, whatever the block
       gap says. See the check in the maintenance loop for why the block gap alone
       is not enough while a node is catching up. */
    constexpr uint64_t AUTO_COMPACTION_MIN_WALL_SECONDS = 30 * 60;

    template<typename T> static bool print_as_json(const T &obj)
    {
        std::cout << CryptoNote::storeToJson(obj) << ENDL;
        return true;
    }

    std::string printTransactionShortInfo(const CryptoNote::CachedTransaction &transaction)
    {
        std::stringstream ss;

        ss << "id: " << transaction.getTransactionHash() << std::endl;
        ss << "fee: " << transaction.getTransactionFee() << std::endl;
        ss << "blobSize: " << transaction.getTransactionBinaryArray().size() << std::endl;

        return ss.str();
    }

    std::string printTransactionFullInfo(const CryptoNote::CachedTransaction &transaction)
    {
        std::stringstream ss;
        ss << printTransactionShortInfo(transaction);
        ss << "JSON: \n" << CryptoNote::storeToJson(transaction.getTransaction()) << std::endl;

        return ss.str();
    }

    struct DbDirStats
    {
        uint64_t bytes = 0;
        uint64_t files = 0;
        uint64_t directories = 0;
        std::map<std::string, uint64_t> extensionCounts;
    };

    DbDirStats collectDbDirStats(const fs::path &path)
    {
        DbDirStats stats;
        std::error_code ec;

        if (!fs::exists(path, ec) || ec)
        {
            return stats;
        }

        fs::recursive_directory_iterator it(path, ec), end;

        if (ec)
        {
            return stats;
        }

        for (; it != end; it.increment(ec))
        {
            if (ec)
            {
                break;
            }

            const auto &entry = *it;

            if (entry.is_directory(ec))
            {
                ++stats.directories;
                continue;
            }

            if (entry.is_regular_file(ec))
            {
                ++stats.files;

                const auto fileSize = entry.file_size(ec);
                if (!ec)
                {
                    stats.bytes += fileSize;
                }

                std::string ext = entry.path().extension().string();
                if (ext.empty())
                {
                    ext = "<none>";
                }

                ++stats.extensionCounts[ext];
            }
        }

        return stats;
    }

    uint64_t getAvailableBytes(const fs::path &path)
    {
        std::error_code ec;
        const auto spaceInfo = fs::space(path, ec);
        if (ec)
        {
            return std::numeric_limits<uint64_t>::max();
        }
        return spaceInfo.available;
    }

} // namespace

httplib::Result DaemonCommandsHandler::rpc_get(const std::string &path)
{
    return rpc_get(m_rpcServer, path);
}

httplib::Result DaemonCommandsHandler::rpc_get(httplib::Client &client, const std::string &path)
{
    if (m_config.rpcAccessToken.empty())
    {
        return client.Get(path.c_str());
    }

    httplib::Headers headers = {{"X-API-Key", m_config.rpcAccessToken}};
    return client.Get(path.c_str(), headers);
}

DaemonCommandsHandler::DaemonCommandsHandler(
    CryptoNote::Core &core,
    CryptoNote::NodeServer &srv,
    std::shared_ptr<Logging::LoggerManager> log,
    const std::string ip,
    const uint32_t port,
    const DaemonConfig::DaemonConfiguration &config,
    const std::string rpcIpcPath):
    m_core(core),
    m_srv(srv),
    logger(log, "daemon"),
    m_logManager(log),
    m_rpcServer(rpcIpcPath.empty() ? ip : rpcIpcPath, port),
    m_maintenanceRpcServer(rpcIpcPath.empty() ? ip : rpcIpcPath, port),
    m_config(config)
{
    /* The console is the most local caller the daemon has. When an IPC socket
       is up it goes over that: one less loopback TCP connection, and it keeps
       working without the console needing the access token. */
    if (!rpcIpcPath.empty())
    {
        Common::Ipc::configureClient(m_rpcServer);
        Common::Ipc::configureClient(m_maintenanceRpcServer);
    }

    /* Both clients talk to a server in this same process over the loopback, so
       anything that takes seconds means the daemon is busy, not that the network
       is slow. Stated explicitly rather than inherited from httplib's defaults,
       because what these bound is how long the console can appear to hang. */
    for (httplib::Client *client : {&m_rpcServer, &m_maintenanceRpcServer})
    {
        client->set_connection_timeout(2, 0);
        client->set_read_timeout(5, 0);
        client->set_write_timeout(5, 0);
    }

    m_consoleHandler.setHandler(
        "?",
        std::bind(&DaemonCommandsHandler::help, this, std::placeholders::_1),
        "Show this help");
    m_consoleHandler.setHandler(
        "exit",
        std::bind(&DaemonCommandsHandler::exit, this, std::placeholders::_1),
        "Shutdown the daemon");
    m_consoleHandler.setHandler(
        "help",
        std::bind(&DaemonCommandsHandler::help, this, std::placeholders::_1),
        "Show this help");
    m_consoleHandler.setHandler(
        "print_pl",
        std::bind(&DaemonCommandsHandler::print_pl, this, std::placeholders::_1),
        "Print peer list");
    m_consoleHandler.setHandler(
        "print_cn",
        std::bind(&DaemonCommandsHandler::print_cn, this, std::placeholders::_1),
        "Print connections");
    m_consoleHandler.setHandler(
        "print_block",
        std::bind(&DaemonCommandsHandler::print_block, this, std::placeholders::_1),
        "Print block, print_block <block_hash> | <block_height>");
    m_consoleHandler.setHandler(
        "print_tx",
        std::bind(&DaemonCommandsHandler::print_tx, this, std::placeholders::_1),
        "Print transaction, print_tx <transaction_hash>");
    m_consoleHandler.setHandler(
        "print_pool",
        std::bind(&DaemonCommandsHandler::print_pool, this, std::placeholders::_1),
        "Print transaction pool (long format)");
    m_consoleHandler.setHandler(
        "print_pool_sh",
        std::bind(&DaemonCommandsHandler::print_pool_sh, this, std::placeholders::_1),
        "Print transaction pool (short format)");
    m_consoleHandler.setHandler(
        "set_log",
        std::bind(&DaemonCommandsHandler::set_log, this, std::placeholders::_1),
        "set_log <level> - Change current log level, <level> is a number 0-4");
    m_consoleHandler.setHandler(
        "status",
        std::bind(&DaemonCommandsHandler::status, this, std::placeholders::_1),
        "Show daemon status");
    m_consoleHandler.setHandler(
        "prune_status",
        std::bind(&DaemonCommandsHandler::prune_status, this, std::placeholders::_1),
        "Show prune mode and capability status");
    m_consoleHandler.setHandler(
        "sync_info",
        std::bind(&DaemonCommandsHandler::sync_info, this, std::placeholders::_1),
        "Show compact synchronization information");
    m_consoleHandler.setHandler(
        "save",
        std::bind(&DaemonCommandsHandler::save, this, std::placeholders::_1),
        "Force-save blockchain state to disk");
    m_consoleHandler.setHandler(
        "sync_tune",
        std::bind(&DaemonCommandsHandler::sync_tune, this, std::placeholders::_1),
        "Show current sync tuning and adaptive sync stats");
    m_consoleHandler.setHandler(
        "sync_peers",
        std::bind(&DaemonCommandsHandler::sync_peers, this, std::placeholders::_1),
        "Show current sync peer diagnostics");
    m_consoleHandler.setHandler(
        "db_status",
        std::bind(&DaemonCommandsHandler::db_status, this, std::placeholders::_1),
        "Show on-disk DB status for the active DB engine");
    m_consoleHandler.setHandler(
        "compact_db",
        std::bind(&DaemonCommandsHandler::compact_db, this, std::placeholders::_1),
        "Manage DB compaction: compact_db [start|status|wait|force]");
    m_consoleHandler.setHandler(
        "snapshot_export",
        std::bind(&DaemonCommandsHandler::snapshot_export, this, std::placeholders::_1),
        "Export a lite node snapshot: snapshot_export [start [height] [path] | status | cancel]");
    m_consoleHandler.setHandler(
        "ban",
        std::bind(&DaemonCommandsHandler::ban, this, std::placeholders::_1),
        "Manage in-memory host bans: ban list | ban add <ip> [seconds] | ban delete <ip>");
}

DaemonCommandsHandler::~DaemonCommandsHandler()
{
    stop_compaction_scheduler();

    /* std::future's destructor blocks until the worker finishes, and a snapshot
       export is tens of minutes. Ask it to stop first so shutting the daemon
       down does not appear to hang; the partial file removes itself. */
    m_snapshotCancel = true;

    if (m_snapshotTask.valid())
    {
        m_snapshotTask.wait();
    }
}

//--------------------------------------------------------------------------------
std::string DaemonCommandsHandler::get_commands_str()
{
    std::stringstream ss;
    ss << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
    ss << "Commands: " << ENDL;
    std::string usage = m_consoleHandler.getUsage();
    Utilities::replaceAll(usage, "\n", "\n  ");
    usage.insert(0, "  ");
    ss << usage << ENDL;
    return ss.str();
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::exit(const std::vector<std::string> &args)
{
    std::cout << InformationMsg("================= EXITING ==================\n"
                                "== PLEASE WAIT, THIS MAY TAKE A LONG TIME ==\n"
                                "============================================\n");

    /* Set log to max when exiting. Sometimes this takes a while, and it helps
       to let users know the daemon is still doing stuff */
    m_logManager->setMaxLevel(Logging::TRACE);
    m_consoleHandler.requestStop();
    m_srv.sendStopSignal();
    return true;
}

void DaemonCommandsHandler::start_boot_compaction_if_needed()
{
    if (m_config.skipBootCompaction)
    {
        std::cout << InformationMsg("Boot DB compaction: skipped by configuration (--skip-boot-compaction).")
                  << std::endl;
    }
    else
    {
        std::lock_guard<std::mutex> lock(m_compactionMutex);
        refresh_compaction_state_locked();

        if (!m_compactionRunning)
        {
            const bool markerExists = compaction_marker_exists_locked();

            m_compactionHasResult = false;
            m_compactionLastError = std::error_code();
            m_compactionLastErrorDetails.clear();
            m_compactionStartedAt = static_cast<uint64_t>(time(nullptr));
            m_compactionStartedAtHeight = static_cast<uint64_t>(m_core.getTopBlockIndex()) + 1;
            m_compactionRunning = true;
            create_compaction_marker_locked();
            logger(Logging::INFO) << "Starting DB compaction (boot background task).";
            m_compactionTask = std::async(std::launch::async, [this]() { return m_core.compactDatabaseDetailed(); });

            if (markerExists)
            {
                std::cout << WarningMsg(
                                 "Detected unfinished DB compaction marker from previous run. Restarting DB compaction in "
                                 "background.")
                          << std::endl;
            }
            else
            {
                std::cout << InformationMsg("Boot DB compaction started in background.") << std::endl;
            }
        }
    }

    m_stopCompactionScheduler = false;
    if (!m_compactionSchedulerThread.joinable())
    {
        m_compactionSchedulerThread = std::thread(std::bind(&DaemonCommandsHandler::compaction_scheduler_loop, this));
        std::cout << InformationMsg("Automatic periodic DB compaction monitoring is enabled.") << std::endl;
    }
}

void DaemonCommandsHandler::stop_compaction_scheduler()
{
    m_stopCompactionScheduler = true;

    if (m_compactionSchedulerThread.joinable())
    {
        m_compactionSchedulerThread.join();
    }
}

void DaemonCommandsHandler::wait_for_background_compaction()
{
    std::lock_guard<std::mutex> lock(m_compactionMutex);
    refresh_compaction_state_locked();

    if (!m_compactionRunning)
    {
        return;
    }

    std::cout << InformationMsg("Waiting for background DB compaction to finish before shutdown...") << std::endl;
    m_compactionTask.wait();
    refresh_compaction_state_locked();
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::help(const std::vector<std::string> &args)
{
    std::cout << get_commands_str() << ENDL;
    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pl(const std::vector<std::string> &args)
{
    m_srv.log_peerlist();
    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_cn(const std::vector<std::string> &args)
{
    m_srv.get_payload_object().log_connections();
    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::set_log(const std::vector<std::string> &args)
{
    if (args.size() != 1)
    {
        std::cout << "use: set_log <log_level_number_0-4>" << ENDL;
        return true;
    }

    uint16_t l = 0;
    if (!Common::fromString(args[0], l))
    {
        std::cout << "wrong number format, use: set_log <log_level_number_0-4>" << ENDL;
        return true;
    }

    ++l;

    if (l > Logging::TRACE)
    {
        std::cout << "wrong number range, use: set_log <log_level_number_0-4>" << ENDL;
        return true;
    }

    m_logManager->setMaxLevel(static_cast<Logging::Level>(l));
    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block_by_height(uint32_t height)
{
    if (height - 1 > m_core.getTopBlockIndex())
    {
        std::cout << "block wasn't found. Current block chain height: " << m_core.getTopBlockIndex() + 1
                  << ", requested: " << height << std::endl;
        return false;
    }

    auto hash = m_core.getBlockHashByIndex(height - 1);
    std::cout << "block_id: " << hash << ENDL;
    print_as_json(m_core.getBlockByIndex(height - 1));

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block_by_hash(const std::string &arg)
{
    Crypto::Hash block_hash;
    if (!parse_hash256(arg, block_hash))
    {
        return false;
    }

    if (m_core.hasBlock(block_hash))
    {
        print_as_json(m_core.getBlockByHash(block_hash));
    }
    else
    {
        std::cout << "block wasn't found: " << arg << std::endl;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        std::cout << "expected: print_block (<block_hash> | <block_height>)" << std::endl;
        return true;
    }

    const std::string &arg = args.front();
    uint32_t height = 0;
    const auto parsed = std::from_chars(arg.data(), arg.data() + arg.size(), height);

    /* Only treat the argument as a height if it parsed completely. Anything
       else - a block hash, junk, or an out-of-range number - falls through to
       the hash lookup, which is what lexical_cast's throw used to do. */
    if (parsed.ec == std::errc() && parsed.ptr == arg.data() + arg.size())
    {
        print_block_by_height(height);
    }
    else
    {
        print_block_by_hash(arg);
    }

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_tx(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        std::cout << "expected: print_tx <transaction hash>" << std::endl;
        return true;
    }

    const std::string &str_hash = args.front();
    Crypto::Hash tx_hash;
    if (!parse_hash256(str_hash, tx_hash))
    {
        return true;
    }

    std::vector<Crypto::Hash> tx_ids;
    tx_ids.push_back(tx_hash);
    std::vector<CryptoNote::BinaryArray> txs;
    std::vector<Crypto::Hash> missed_ids;
    m_core.getTransactions(tx_ids, txs, missed_ids);

    if (1 == txs.size())
    {
        CryptoNote::CachedTransaction tx(txs.front());
        print_as_json(tx.getTransaction());
    }
    else
    {
        std::cout << "transaction wasn't found: <" << str_hash << '>' << std::endl;
    }

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pool(const std::vector<std::string> &args)
{
    std::cout << "Pool state: \n";
    auto pool = m_core.getPoolTransactions();

    for (const auto &tx : pool)
    {
        CryptoNote::CachedTransaction ctx(tx);
        std::cout << printTransactionFullInfo(ctx) << "\n";
    }

    std::cout << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pool_sh(const std::vector<std::string> &args)
{
    const auto pool = m_core.getPoolTransactions();

    const uint64_t height = m_core.getTopBlockIndex();

    if (pool.size() == 0)
    {
        std::cout << InformationMsg("\nPool state: ") << SuccessMsg("Empty.") << std::endl;
        return true;
    }

    std::cout << InformationMsg("\nPool state:\n");

    uint64_t totalSize = 0;

    const float maxTxSize = Utilities::getMaxTxSize(m_core.getTopBlockIndex());

    for (const auto &tx : pool)
    {
        CryptoNote::CachedTransaction ctx(tx);

        std::vector<uint8_t> data = toBinaryArray(static_cast<CryptoNote::TransactionPrefix>(tx));

        Crypto::Hash hash;

        Crypto::cn_upx(data.data(), data.size(), hash);

        uint64_t txInputSize = 0;
        try
        {
            txInputSize = tx.inputs.size();
        }
        catch (const std::exception &e)
        {
        }

        uint64_t txOutputSize = 0;
        try
        {
            txOutputSize = tx.outputs.size();
        }
        catch (const std::exception &e)
        {
        }

        bool isFusion = ctx.getTransactionFee() == 0;

        uint64_t diff = 0;

        try
        {
            uint64_t checked_diff = 0;
            if (height >= CryptoNote::parameters::TRANSACTION_POW_HEIGHT && 
            height <= CryptoNote::parameters::TRANSACTION_POW_HEIGHT_DYN_V1)
            {
                checked_diff = isFusion ? CryptoNote::parameters::FUSION_TRANSACTION_POW_DIFFICULTY : CryptoNote::parameters::TRANSACTION_POW_DIFFICULTY;
            } else if (height > CryptoNote::parameters::TRANSACTION_POW_HEIGHT_DYN_V1)
            {
                checked_diff = isFusion ? CryptoNote::parameters::FUSION_TRANSACTION_POW_DIFFICULTY_V2 : 
                (CryptoNote::parameters::TRANSACTION_POW_DIFFICULTY_DYN_V1 
                + (txInputSize + txOutputSize * CryptoNote::parameters::MULTIPLIER_TRANSACTION_POW_DIFFICULTY_FACTORED_OUT_V1) 
                * CryptoNote::parameters::MULTIPLIER_TRANSACTION_POW_DIFFICULTY_PER_IO_V1);
            }
            if (CryptoNote::check_hash(hash, checked_diff))
            {
                diff = checked_diff;
            }
        }
        catch (const std::exception &e)
        {
        }

        std::cout << InformationMsg("Hash: ") << SuccessMsg(ctx.getTransactionHash())
                  << InformationMsg(", Size: ") << SuccessMsg(Utilities::prettyPrintBytes(ctx.getTransactionBinaryArray().size()))
                  << InformationMsg(", Fee: ") << SuccessMsg(Utilities::formatAmount(ctx.getTransactionFee()))
                  << InformationMsg(", Tx Diff: ") << SuccessMsg(std::to_string(diff))
                  << InformationMsg(", Fusion: ");

        if (isFusion)
        {
            std::cout << SuccessMsg("Yes") << std::endl;
        }
        else
        {
            std::cout << WarningMsg("No") << std::endl;
        }

        totalSize += ctx.getTransactionBinaryArray().size();
    }

    const float blocksRequiredToClear = std::ceil(totalSize / maxTxSize);

    std::cout << InformationMsg("\nTotal transactions: ") << SuccessMsg(pool.size())
              << InformationMsg("\nTotal size of transactions: ") << SuccessMsg(Utilities::prettyPrintBytes(totalSize))
              << InformationMsg("\nEstimated full blocks to clear: ") << SuccessMsg(blocksRequiredToClear) << std::endl << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::status(const std::vector<std::string> &args)
{
    httplib::Result res;

    /* Retry briefly if the RPC server is still binding its socket at startup */
    constexpr int RPC_READY_RETRIES = 6;
    for (int attempt = 0; attempt < RPC_READY_RETRIES; ++attempt)
    {
        res = rpc_get("/info");
        if (res)
            break;

        if (attempt == 0)
            std::cout << InformationMsg("Daemon is still starting up, waiting for RPC...") << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!res)
    {
        std::cout << WarningMsg("Daemon RPC server is not yet ready. Please try again in a moment.") << std::endl;
        return false;
    }

    if (res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving information from RPC server.") << std::endl;
        return false;
    }

    nlohmann::json resp;
    try { resp = nlohmann::json::parse(res->body); }
    catch (const nlohmann::json::parse_error &)
    {
        std::cout << WarningMsg("Problem retrieving information from RPC server.") << std::endl;
        return false;
    }

    const std::time_t uptime = std::time(nullptr) - getUint64FromJSON(resp, "start_time");

    const uint64_t seconds = uptime;
    const uint64_t minutes = seconds / 60;
    const uint64_t hours = minutes / 60;
    const uint64_t days = hours / 24;

    const std::string uptimeStr = std::to_string(days) + "d "
                                + std::to_string(hours % 24) + "h "
                                + std::to_string(minutes % 60) + "m "
                                + std::to_string(seconds % 60) + "s";

    const uint64_t height = getUint64FromJSON(resp, "height");
    const uint64_t networkHeight = getUint64FromJSON(resp, "network_height");
    const uint64_t supportedHeight = getUint64FromJSON(resp, "supported_height");
    std::vector<uint64_t> upgradeHeights;

    for (const auto &height : getArrayFromJSON(resp, "upgrade_heights"))
    {
        upgradeHeights.push_back(height.get<uint64_t>());
    }

    const auto forkStatus = Utilities::get_fork_status(networkHeight, upgradeHeights, supportedHeight);

    std::vector<std::tuple<std::string, std::string>> statusTable;

    statusTable.push_back({"Local Height",          std::to_string(height)});
    statusTable.push_back({"Network Height",        std::to_string(networkHeight)});
    statusTable.push_back({"Percentage Synced",     Utilities::get_sync_percentage(height, networkHeight) + "%"});
    statusTable.push_back({"Network Hashrate",      Utilities::get_mining_speed(getUint64FromJSON(resp, "hashrate"))});
    statusTable.push_back({"Block Version",         "v" + std::to_string(getUint64FromJSON(resp, "major_version"))});
    statusTable.push_back({"Incoming Connections",  std::to_string(getUint64FromJSON(resp, "incoming_connections_count"))});
    statusTable.push_back({"Outgoing Connections",  std::to_string(getUint64FromJSON(resp, "outgoing_connections_count"))});
    statusTable.push_back({"Uptime",                uptimeStr});
    statusTable.push_back({"Fork Status",           Utilities::get_update_status(forkStatus)});
    statusTable.push_back({"Next Fork",             Utilities::get_fork_time(networkHeight, upgradeHeights)});
    statusTable.push_back({"Transaction Pool Size", std::to_string(m_core.getPoolTransactionHashes().size())});
    statusTable.push_back({"Alternative Block Count", std::to_string(m_core.getAlternativeBlockCount())});
    statusTable.push_back({"DB Engine",             "RocksDB"});
    statusTable.push_back({"Pruned Node",          getBoolFromJSON(resp, "pruned") ? "Yes" : "No"});
    statusTable.push_back({"Prune Depth",          std::to_string(getUint64FromJSON(resp, "prune_depth"))});
    statusTable.push_back({"Prune Capability Fork Active", getBoolFromJSON(resp, "prune_capability_active") ? "Yes" : "No"});

    /* A lite node holds no block bodies below its lite height, so an operator
       reading this table needs to know which mode the database was built in -
       it is permanent, and it decides which wallets this node can serve. */
    const bool liteNode = getBoolFromJSON(resp, "lite");
    const uint64_t liteStartHeight = getUint64FromJSON(resp, "lite_start_height");

    statusTable.push_back({"Lite Node",            liteNode ? "Yes" : "No"});

    if (liteNode)
    {
        statusTable.push_back({"Full Block Data From", std::to_string(liteStartHeight)});

        /* Below the lite height the node is still building the indexes that the
           full block region needs, and /info reports no block version down
           there. Say so rather than leave the table looking like a full node
           that has lost its block version. */
        if (height < liteStartHeight)
        {
            statusTable.push_back({"Lite Sync Stage", "Index only (below lite height)"});
        }
    }

    statusTable.push_back({"Active Sync Peers",    std::to_string(getUint64FromJSON(resp, "sync_active_peers"))});
    statusTable.push_back({"Avg Sync Batch Size",  std::to_string(getUint64FromJSON(resp, "sync_avg_batch_size"))});
    statusTable.push_back({"Demoted Sync Peers",   std::to_string(getUint64FromJSON(resp, "sync_demoted_peers"))});
    statusTable.push_back({"WrkzCoin Version", PROJECT_VERSION});

    size_t longestValue = 0;
    size_t longestDescription = 0;

    /* Figure out the dimensions of the table */
    for (const auto &[value, description] : statusTable)
    {
        if (value.length() > longestValue)
        {
            longestValue = value.length();
        }

        if (description.length() > longestDescription)
        {
            longestDescription = description.length();
        }
    }

    /* Need 7 extra chars for all the padding and borders in addition to the
     * values inside the table */
    const size_t totalTableWidth = longestValue + longestDescription + 7;

    /* Table border */
    std::cout << std::string(totalTableWidth, '-') << std::endl;

    /* Output the table itself */
    for (const auto &[value, description] : statusTable)
    {
        std::cout << "| " << InformationMsg(value, longestValue) << " ";
        std::cout << "| " << SuccessMsg(description, longestDescription) << " |" << std::endl;
    }

    /* Table border */
    std::cout << std::string(totalTableWidth, '-') << std::endl;

    if (forkStatus == Utilities::OutOfDate)
    {
        std::cout << WarningMsg(Utilities::get_upgrade_info(supportedHeight, upgradeHeights)) << std::endl;
    }

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::prune_status(const std::vector<std::string> &args)
{
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving prune status from RPC server.") << std::endl;
        return false;
    }

    nlohmann::json resp;
    try { resp = nlohmann::json::parse(res->body); }
    catch (const nlohmann::json::parse_error &)
    {
        std::cout << WarningMsg("Problem parsing prune status response.") << std::endl;
        return false;
    }

    const bool pruned = getBoolFromJSON(resp, "pruned");
    const uint64_t pruneDepth = getUint64FromJSON(resp, "prune_depth");
    const bool pruneCapabilityActive = getBoolFromJSON(resp, "prune_capability_active");
    const uint64_t height = getUint64FromJSON(resp, "height");
    const uint64_t pruneFloor = height > pruneDepth ? height - pruneDepth : 0;

    std::cout << InformationMsg("Pruned Node: ") << SuccessMsg(pruned ? "Yes" : "No") << std::endl;
    std::cout << InformationMsg("Prune Depth: ") << SuccessMsg(pruneDepth) << std::endl;
    std::cout << InformationMsg("Approx Prune Floor Height: ") << SuccessMsg(pruneFloor) << std::endl;
    std::cout << InformationMsg("Prune Capability Fork Active: ") << SuccessMsg(pruneCapabilityActive ? "Yes" : "No")
              << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::sync_info(const std::vector<std::string> &args)
{
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving sync information from RPC server.") << std::endl;
        return false;
    }

    nlohmann::json resp;
    try { resp = nlohmann::json::parse(res->body); }
    catch (const nlohmann::json::parse_error &)
    {
        std::cout << WarningMsg("Problem parsing sync information response.") << std::endl;
        return false;
    }

    const uint64_t height = getUint64FromJSON(resp, "height");
    const uint64_t networkHeight = getUint64FromJSON(resp, "network_height");
    const std::string percentage = Utilities::get_sync_percentage(height, networkHeight) + "%";

    std::cout << InformationMsg("Height: ") << SuccessMsg(height) << InformationMsg(" / ")
              << SuccessMsg(networkHeight) << InformationMsg(" (") << SuccessMsg(percentage) << InformationMsg(")")
              << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::save(const std::vector<std::string> &args)
{
    m_core.save();
    std::cout << SuccessMsg("Core state saved.") << std::endl;
    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::sync_tune(const std::vector<std::string> &args)
{
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving sync tuning from RPC server.") << std::endl;
        return false;
    }

    nlohmann::json resp;
    try { resp = nlohmann::json::parse(res->body); }
    catch (const nlohmann::json::parse_error &)
    {
        std::cout << WarningMsg("Problem parsing sync tuning response.") << std::endl;
        return false;
    }

    std::cout << InformationMsg("Active Sync Peers: ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_active_peers"))) << std::endl;
    std::cout << InformationMsg("Average Sync Batch Size: ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_avg_batch_size"))) << std::endl;
    std::cout << InformationMsg("Demoted Sync Peers: ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_demoted_peers"))) << std::endl;
    std::cout << InformationMsg("Configured Sync Max Peers: ")
              << SuccessMsg(std::to_string(m_config.syncMaxPeers)) << std::endl;
    std::cout << InformationMsg("Configured P2P Out/In Peers: ")
              << SuccessMsg(std::to_string(m_config.p2pOutPeers) + "/" + std::to_string(m_config.p2pInPeers))
              << std::endl;
    std::cout << InformationMsg("Configured Sync Failure Threshold: ")
              << SuccessMsg(std::to_string(m_config.syncPeerFailureThreshold)) << std::endl;
    std::cout << InformationMsg("Configured Sync Batch Min/Max: ")
              << SuccessMsg(std::to_string(m_config.syncBatchMin) + "/" + std::to_string(m_config.syncBatchMax))
              << std::endl;
    std::cout << InformationMsg("Configured Block Sync Size: ")
              << SuccessMsg(std::to_string(m_config.blockSyncSize)) << std::endl;
    std::cout << InformationMsg("Configured Block Sync Bytes: ")
              << SuccessMsg(Utilities::prettyPrintBytes(m_config.blockSyncBytes)) << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::sync_peers(const std::vector<std::string> &args)
{
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving sync peer diagnostics from RPC server.") << std::endl;
        return false;
    }

    nlohmann::json resp;
    try { resp = nlohmann::json::parse(res->body); }
    catch (const nlohmann::json::parse_error &)
    {
        std::cout << WarningMsg("Problem parsing sync peer diagnostics response.") << std::endl;
        return false;
    }

    std::cout << InformationMsg("Sync Active Peers: ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_active_peers"))) << std::endl;
    std::cout << InformationMsg("Average Sync Batch Size: ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_avg_batch_size"))) << std::endl;
    std::cout << InformationMsg("Demoted Sync Peers (lifetime): ")
              << SuccessMsg(std::to_string(getUint64FromJSON(resp, "sync_demoted_peers"))) << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::db_status(const std::vector<std::string> &args)
{
    const std::string engine = "RocksDB";
    const fs::path dbPath = fs::path(m_config.dataDirectory) / "DB";
    const DbDirStats stats = collectDbDirStats(dbPath);
    const bool compressionEnabled = m_config.enableDbCompression;

    std::cout << InformationMsg("DB Engine: ") << SuccessMsg(engine) << std::endl;
    std::cout << InformationMsg("Compression Enabled: ")
              << SuccessMsg(compressionEnabled ? "Yes" : "No") << std::endl;

    std::cout << InformationMsg("Compression Mode: ")
              << SuccessMsg(compressionEnabled ? "RocksDB ZSTD (L2+; L0/L1 uncompressed)" : "Disabled")
              << std::endl;

    std::cout << InformationMsg("DB Path: ") << SuccessMsg(dbPath.string()) << std::endl;
    std::cout << InformationMsg("DB Size: ") << SuccessMsg(Utilities::prettyPrintBytes(stats.bytes)) << std::endl;
    std::cout << InformationMsg("Files: ") << SuccessMsg(std::to_string(stats.files)) << std::endl;
    std::cout << InformationMsg("Directories: ") << SuccessMsg(std::to_string(stats.directories)) << std::endl;

    if (stats.extensionCounts.empty())
    {
        std::cout << WarningMsg("No DB files found in the selected path.") << std::endl;
        return true;
    }

    std::cout << InformationMsg("File type counts:") << std::endl;

    for (const auto &[ext, count] : stats.extensionCounts)
    {
        std::cout << "  " << ext << ": " << count << std::endl;
    }

    return true;
}

//--------------------------------------------------------------------------------
void DaemonCommandsHandler::refresh_snapshot_state_locked()
{
    if (!m_snapshotRunning || !m_snapshotTask.valid())
    {
        return;
    }

    if (m_snapshotTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }

    m_snapshotTask.get();
    m_snapshotRunning = false;
    m_snapshotHasResult = true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::snapshot_export(const std::vector<std::string> &args)
{
    const std::string sub = args.empty() ? "start" : args[0];

    if (sub != "start" && sub != "status" && sub != "cancel")
    {
        std::cout << "Usage: snapshot_export [start [height] [path] | status | cancel]" << std::endl;
        return true;
    }

    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    refresh_snapshot_state_locked();

    if (sub == "status")
    {
        if (m_snapshotRunning)
        {
            const uint64_t now = static_cast<uint64_t>(time(nullptr));
            const uint64_t elapsed = now > m_snapshotStartedAt ? (now - m_snapshotStartedAt) : 0;

            std::cout << InformationMsg("Snapshot export: ")
                      << SuccessMsg("running (" + std::to_string(elapsed) + "s elapsed)") << std::endl;
            std::cout << InformationMsg("  Writing: ") << SuccessMsg(m_snapshotPath) << std::endl;
            std::cout << InformationMsg("  Table:   ") << SuccessMsg(m_snapshotStage) << std::endl;
            std::cout << InformationMsg("  Records: ")
                      << SuccessMsg(std::to_string(m_snapshotKept) + " kept of " + std::to_string(m_snapshotScanned)
                                    + " scanned")
                      << std::endl;

            return true;
        }

        std::cout << InformationMsg("Snapshot export: ") << SuccessMsg("idle") << std::endl;

        if (m_snapshotHasResult)
        {
            if (!m_snapshotError.empty())
            {
                std::cout << WarningMsg("Last result: failed - " + m_snapshotError) << std::endl;
            }
            else
            {
                std::cout << SuccessMsg("Last result: wrote " + m_snapshotPath) << std::endl;
                std::cout << InformationMsg("  Lite height: ")
                          << SuccessMsg(std::to_string(m_snapshotResult.liteHeight)) << std::endl;
                std::cout << InformationMsg("  Records:     ")
                          << SuccessMsg(std::to_string(m_snapshotResult.totalRecords())) << std::endl;
                std::cout << InformationMsg("  Digest:      ")
                          << SuccessMsg(Common::podToHex(m_snapshotResult.payloadDigest)) << std::endl;
            }
        }

        return true;
    }

    if (sub == "cancel")
    {
        if (!m_snapshotRunning)
        {
            std::cout << InformationMsg("No snapshot export is running.") << std::endl;

            return true;
        }

        m_snapshotCancel = true;

        std::cout << InformationMsg("Cancelling the snapshot export. The partial file will be removed.") << std::endl;

        return true;
    }

    if (m_snapshotRunning)
    {
        std::cout << WarningMsg("A snapshot export is already running. Use `snapshot_export status`.") << std::endl;

        return true;
    }

    /* start [height] [path]. The height is optional on a lite node, where the
       only sensible value is the one this database was built at - exporting at
       any other height would describe a chain region this node does not hold in
       the form a snapshot needs. A full node has no lite height, so it has to
       say. */
    uint32_t height = m_config.liteHeight;

    std::string path;

    for (size_t i = 1; i < args.size(); i++)
    {
        const std::string &arg = args[i];

        const bool numeric = !arg.empty() && arg.find_first_not_of("0123456789") == std::string::npos;

        if (numeric && height == m_config.liteHeight && path.empty())
        {
            try
            {
                height = static_cast<uint32_t>(std::stoul(arg));
            }
            catch (const std::exception &)
            {
                std::cout << WarningMsg("Could not read " + arg + " as a height.") << std::endl;

                return true;
            }
        }
        else
        {
            path = arg;
        }
    }

    if (height == 0)
    {
        std::cout << WarningMsg("This node has no lite height, so a snapshot height has to be given:") << std::endl;
        std::cout << "  snapshot_export start <height> [path]" << std::endl;

        return true;
    }

    if (m_config.liteHeight != 0 && height != m_config.liteHeight)
    {
        std::cout << WarningMsg(
                         "This is a lite node built at height " + std::to_string(m_config.liteHeight)
                         + ", and it does not hold the block data a snapshot at " + std::to_string(height)
                         + " would need.")
                  << std::endl;

        return true;
    }

    /* The exported region can never be reorganised away, so it has to be
       settled by the same margin lite mode already demands of its own boundary
       rather than some looser rule invented here. */
    const uint32_t topIndex = m_core.getTopBlockIndex();
    const uint64_t settledFrom =
        static_cast<uint64_t>(height) + CryptoNote::parameters::MIN_LITE_FULL_BLOCK_DEPTH;

    if (static_cast<uint64_t>(topIndex) + 1 < settledFrom)
    {
        std::cout << WarningMsg(
                         "This node is at height " + std::to_string(topIndex + 1) + " and a snapshot at "
                         + std::to_string(height) + " needs it to be at least " + std::to_string(settledFrom)
                         + ", so the exported region is beyond any reorg.")
                  << std::endl;

        return true;
    }

    fs::path output;

    if (path.empty())
    {
        std::error_code ec;
        const fs::path dataDir = fs::absolute(fs::path(m_config.dataDirectory), ec);

        const fs::path parent = ec ? fs::path(m_config.dataDirectory).parent_path() : dataDir.parent_path();

        output = parent / CryptoNote::LiteSnapshot::defaultFileName(height);
    }
    else
    {
        output = fs::path(path);

        std::error_code ec;

        if (fs::is_directory(output, ec))
        {
            output /= CryptoNote::LiteSnapshot::defaultFileName(height);
        }
    }

    std::error_code ec;

    if (fs::exists(output, ec))
    {
        std::cout << WarningMsg(output.string() + " already exists. Move it aside or name another path.") << std::endl;

        return true;
    }

    /* A snapshot is smaller than the database it comes out of, so the database
       size is a conservative floor that costs nothing to compute and beats
       discovering the problem 4 GB in. */
    const DbDirStats dbStats = collectDbDirStats(fs::path(m_config.dataDirectory) / "DB");
    const uint64_t available = getAvailableBytes(output.parent_path());

    if (available < dbStats.bytes)
    {
        std::cout << WarningMsg(
                         "Only " + Utilities::prettyPrintBytes(available) + " free where the snapshot would go, and "
                         + "the database it comes from is " + Utilities::prettyPrintBytes(dbStats.bytes) + ".")
                  << std::endl;

        return true;
    }

    m_snapshotCancel = false;
    m_snapshotRunning = true;
    m_snapshotHasResult = false;
    m_snapshotError.clear();
    m_snapshotPath = output.string();
    m_snapshotStage = "starting";
    m_snapshotScanned = 0;
    m_snapshotKept = 0;
    m_snapshotStartedAt = static_cast<uint64_t>(time(nullptr));
    m_snapshotResult = CryptoNote::LiteSnapshot::Header {};

    const std::string outputPath = m_snapshotPath;

    m_snapshotTask = std::async(std::launch::async, [this, outputPath, height]() {
        try
        {
            const auto header = m_core.exportLiteSnapshot(
                outputPath, height, [this](const std::string &table, const uint64_t scanned, const uint64_t kept) {
                    std::lock_guard<std::mutex> progressLock(m_snapshotMutex);

                    m_snapshotStage = table;
                    m_snapshotScanned = scanned;
                    m_snapshotKept = kept;

                    return !m_snapshotCancel.load();
                });

            std::lock_guard<std::mutex> resultLock(m_snapshotMutex);

            m_snapshotResult = header;
            m_snapshotStage = "done";
        }
        catch (const std::exception &e)
        {
            std::lock_guard<std::mutex> resultLock(m_snapshotMutex);

            m_snapshotError = e.what();
            m_snapshotStage = "failed";
        }
    });

    std::cout << InformationMsg("Exporting a lite node snapshot at height ") << SuccessMsg(std::to_string(height))
              << InformationMsg(" to ") << SuccessMsg(outputPath) << std::endl;
    std::cout << InformationMsg(
                     "This walks the whole database and takes tens of minutes. Follow it with `snapshot_export "
                     "status`.")
              << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::compact_db(const std::vector<std::string> &args)
{
    const std::string sub = args.empty() ? "start" : args[0];

    if (sub != "start" && sub != "status" && sub != "wait" && sub != "force")
    {
        std::cout << "Usage: compact_db [start|status|wait|force]" << std::endl;
        return true;
    }

    /* An ordinary compaction leaves the bottommost level alone, and after the
       first one that level holds essentially the whole database. That is the
       right default - it reclaims space from deletes without rewriting terabytes
       of settled data - but it means changed compression settings never reach
       what is already written. `force` is how you apply them: it rewrites
       everything, so it is slow and wants free space of about the database's
       size. */
    const bool rewriteBottommost = (sub == "force");

    std::lock_guard<std::mutex> lock(m_compactionMutex);
    refresh_compaction_state_locked();

    if (sub == "status")
    {
        if (m_compactionRunning)
        {
            const uint64_t now = static_cast<uint64_t>(time(nullptr));
            const uint64_t elapsed = now > m_compactionStartedAt ? (now - m_compactionStartedAt) : 0;
            std::cout << InformationMsg("DB compaction status: ")
                      << SuccessMsg("running (" + std::to_string(elapsed) + "s elapsed)") << std::endl;
            return true;
        }

        std::cout << InformationMsg("DB compaction status: ") << SuccessMsg("idle") << std::endl;

        if (compaction_marker_exists_locked())
        {
            std::cout << WarningMsg(
                             "Persistent compaction marker present (previous run may have terminated mid-compaction).")
                      << std::endl;
        }

        if (m_compactionHasResult)
        {
            if (m_compactionLastError)
            {
                std::string message = "Last result: failed - " + m_compactionLastError.message();
                if (!m_compactionLastErrorDetails.empty())
                {
                    message += " (" + m_compactionLastErrorDetails + ")";
                }
                std::cout << WarningMsg(message) << std::endl;
            }
            else
            {
                std::cout << SuccessMsg("Last result: completed successfully") << std::endl;
            }
        }

        return true;
    }

    if (sub == "wait")
    {
        if (!m_compactionRunning)
        {
            std::cout << InformationMsg("No DB compaction is running.") << std::endl;
            return true;
        }

        std::cout << InformationMsg("Waiting for DB compaction to complete...") << std::endl;
        m_compactionTask.wait();
        refresh_compaction_state_locked();

        if (m_compactionLastError)
        {
            std::string message = "DB compaction failed: " + m_compactionLastError.message();
            if (!m_compactionLastErrorDetails.empty())
            {
                message += " (" + m_compactionLastErrorDetails + ")";
            }
            std::cout << WarningMsg(message) << std::endl;
            return false;
        }

        std::cout << SuccessMsg("DB compaction completed.") << std::endl;
        return true;
    }

    if (m_compactionRunning)
    {
        std::cout << WarningMsg("DB compaction is already running. Use `compact_db status` or `compact_db wait`.")
                  << std::endl;
        return true;
    }

    m_compactionHasResult = false;
    m_compactionLastError = std::error_code();
    m_compactionLastErrorDetails.clear();
    m_compactionStartedAt = static_cast<uint64_t>(time(nullptr));
    m_compactionStartedAtHeight = static_cast<uint64_t>(m_core.getTopBlockIndex()) + 1;
    m_compactionRunning = true;
    create_compaction_marker_locked();
    logger(Logging::INFO) << "Starting DB compaction (manual console request)"
                          << (rewriteBottommost ? ", rewriting the bottommost level." : ".");
    m_compactionTask = std::async(
        std::launch::async, [this, rewriteBottommost]() { return m_core.compactDatabaseDetailed(rewriteBottommost); });

    std::cout << InformationMsg("DB compaction started in background. Use `compact_db status` or `compact_db wait`.")
              << std::endl;
    return true;
}

void DaemonCommandsHandler::refresh_compaction_state_locked()
{
    if (!m_compactionRunning || !m_compactionTask.valid())
    {
        return;
    }

    if (m_compactionTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }

    const auto compactionResult = m_compactionTask.get();
    m_compactionLastError = compactionResult.first;
    m_compactionLastErrorDetails = compactionResult.second;
    m_compactionRunning = false;
    m_compactionHasResult = true;
    m_compactionFinishedAt = static_cast<uint64_t>(time(nullptr));
    m_compactionFinishedAtHeight = static_cast<uint64_t>(m_core.getTopBlockIndex()) + 1;
    clear_compaction_marker_locked();
}

std::string DaemonCommandsHandler::get_compaction_marker_path() const
{
    const fs::path markerPath = fs::path(m_config.dataDirectory) / "DB" / COMPACTION_MARKER_FILE;
    return markerPath.string();
}

bool DaemonCommandsHandler::compaction_marker_exists_locked() const
{
    std::error_code ec;
    const bool exists = fs::exists(get_compaction_marker_path(), ec);
    return !ec && exists;
}

void DaemonCommandsHandler::create_compaction_marker_locked()
{
    std::error_code ec;
    const fs::path markerPath = get_compaction_marker_path();
    fs::create_directories(markerPath.parent_path(), ec);
    ec.clear();

    std::ofstream marker(markerPath.string(), std::ios::trunc);
    if (!marker.good())
    {
        std::cout << WarningMsg("Failed to create DB compaction marker file: " + markerPath.string()) << std::endl;
        return;
    }

    marker << static_cast<uint64_t>(time(nullptr)) << "\n";
}

void DaemonCommandsHandler::clear_compaction_marker_locked()
{
    std::error_code ec;
    fs::remove(get_compaction_marker_path(), ec);
}

void DaemonCommandsHandler::compaction_scheduler_loop()
{
    while (!m_stopCompactionScheduler)
    {
        const uint64_t sleepSeconds = m_schedulerCheckIntervalSeconds;

        for (uint64_t i = 0; i < sleepSeconds; ++i)
        {
            if (m_stopCompactionScheduler)
            {
                return;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        /* Polled before the lock is taken, and on this thread's own client.
           This is a round trip to an HTTP server inside this same process, and
           holding the compaction mutex across it made a console `compact_db`
           wait on a network call for no reason at all. The state it updates -
           the near sync streak and the check interval - is touched only by this
           thread, so it needs no lock of its own. */
        {
            auto res = rpc_get(m_maintenanceRpcServer, "/info");
            if (res && res->status == 200)
            {
                nlohmann::json resp;
                bool parseOk = true;
                try { resp = nlohmann::json::parse(res->body); } catch (...) { parseOk = false; }
                if (parseOk)
                {
                    const uint64_t localHeight = getUint64FromJSON(resp, "height");
                    const uint64_t networkHeight = getUint64FromJSON(resp, "network_height");
                    const uint64_t lag = networkHeight > localHeight ? (networkHeight - localHeight) : 0;

                    if (lag <= AUTO_COMPACTION_NEAR_SYNC_LAG_BLOCKS)
                    {
                        m_nearSyncStreak += 1;
                    }
                    else if (lag >= AUTO_COMPACTION_RESYNC_LAG_BLOCKS)
                    {
                        m_nearSyncStreak = 0;
                    }

                    const uint64_t desiredInterval =
                        m_nearSyncStreak >= AUTO_COMPACTION_NEAR_SYNC_STREAK_REQUIRED
                        ? AUTO_COMPACTION_CHECK_INTERVAL_SLOW_SECONDS
                        : AUTO_COMPACTION_CHECK_INTERVAL_FAST_SECONDS;

                    if (desiredInterval != m_schedulerCheckIntervalSeconds)
                    {
                        m_schedulerCheckIntervalSeconds = desiredInterval;
                        logger(Logging::INFO)
                            << "Adaptive maintenance scheduler interval switched to "
                            << m_schedulerCheckIntervalSeconds << "s (local height: " << localHeight
                            << ", network height: " << networkHeight
                            << ", lag: " << lag << ").";
                    }
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_compactionMutex);
        refresh_compaction_state_locked();

        const uint64_t now = static_cast<uint64_t>(time(nullptr));
        const uint64_t currentHeight = static_cast<uint64_t>(m_core.getTopBlockIndex()) + 1;

        const fs::path dbPath = fs::path(m_config.dataDirectory) / "DB";
        const uint64_t freeBytes = getAvailableBytes(dbPath);
        const bool lowSpaceForPrune = freeBytes < m_config.autoPruneMinFreeBytes;
        const bool lowSpaceForCompaction = freeBytes < m_config.autoCompactionMinFreeBytes;
        const bool pruneGapReached = m_config.autoPruneMinGapBlocks == 0
            ? false
            : (m_lastAutoPruneHeight == 0
                || (currentHeight > m_lastAutoPruneHeight
                    && (currentHeight - m_lastAutoPruneHeight) >= m_config.autoPruneMinGapBlocks));

        if (m_config.prune
            && (pruneGapReached || lowSpaceForPrune))
        {
            if (lowSpaceForPrune)
            {
                logger(Logging::WARNING)
                    << "Low free disk space (" << freeBytes
                    << " bytes). Forcing periodic prune pass regardless of block gap.";
            }

            logger(Logging::INFO)
                << "Starting periodic prune pass in background (depth " << m_config.pruneDepth << ").";

            try
            {
                const auto prunedBlocks = m_core.pruneRawBlocks(m_config.pruneDepth);
                logger(Logging::INFO) << "Periodic prune pass completed. Raw block slots processed: " << prunedBlocks;
            }
            catch (const std::exception &e)
            {
                logger(Logging::WARNING) << "Periodic prune pass failed: " << e.what();
            }

            m_lastAutoPruneHeight = currentHeight;
        }

        if (m_compactionRunning)
        {
            continue;
        }

        if (lowSpaceForCompaction)
        {
            logger(Logging::WARNING)
                << "Skipping automatic DB compaction due to low free disk space ("
                << freeBytes << " bytes, required at least " << m_config.autoCompactionMinFreeBytes << " bytes).";
            continue;
        }

        if (m_config.autoCompactionMinGapBlocks == 0)
        {
            continue;
        }

        const uint64_t lastActivityHeight = std::max(m_compactionStartedAtHeight, m_compactionFinishedAtHeight);

        if (lastActivityHeight != 0
            && currentHeight > lastActivityHeight
            && (currentHeight - lastActivityHeight) < m_config.autoCompactionMinGapBlocks)
        {
            continue;
        }

        /* The block gap above assumes blocks arrive at chain rate, where 720 of
           them is about twelve hours. While catching up they arrive hundreds of
           times faster - a node syncing at 22,000 blocks a minute passes 720 in
           two seconds - so the gap is always satisfied and the check interval
           alone decides how often we compact. That meant a full database rewrite
           every 60 seconds for the whole of a multi hour sync, each one
           rewriting a database that had grown since the last.

           Requiring wall clock time to have passed as well keeps the cost bounded
           however fast blocks arrive. Measured on a sync at 4.2 million blocks:
           15,000 blocks/min with a compaction every minute, 22,000 blocks/min
           without - about 90 minutes off a four and a half hour sync. */
        const uint64_t lastActivityAt = std::max(m_compactionStartedAt, m_compactionFinishedAt);

        /* Compared with >= rather than >, because refresh_compaction_state_locked
           stamps m_compactionFinishedAt with the moment this loop *notices* a
           compaction finished, not the moment it actually did. On the first check
           after one completes those are the same second, and a strict > would skip
           the guard entirely and compact again immediately. */
        if (lastActivityAt != 0 && now >= lastActivityAt
            && (now - lastActivityAt) < AUTO_COMPACTION_MIN_WALL_SECONDS)
        {
            continue;
        }

        m_compactionHasResult = false;
        m_compactionLastError = std::error_code();
        m_compactionLastErrorDetails.clear();
        m_compactionStartedAt = now;
        m_compactionStartedAtHeight = currentHeight;
        m_compactionRunning = true;
        create_compaction_marker_locked();
        logger(Logging::INFO) << "Starting DB compaction (automatic periodic background task).";
        m_compactionTask = std::async(std::launch::async, [this]() { return m_core.compactDatabaseDetailed(); });
        std::cout << InformationMsg("Automatic periodic DB compaction started in background.") << std::endl;
    }
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::ban(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        std::cout << "Usage: ban list | ban add <ip> [seconds] | ban delete <ip>" << std::endl;
        return true;
    }

    const std::string sub = args[0];

    if (sub == "list")
    {
        const auto bans4 = m_srv.get_banned_hosts();
        const auto bans6 = m_srv.get_banned_hosts6();

        if (bans4.empty() && bans6.empty())
        {
            std::cout << InformationMsg("Ban list is empty.") << std::endl;
            return true;
        }

        const uint64_t now = static_cast<uint64_t>(time(nullptr));
        std::cout << InformationMsg("Banned hosts:") << std::endl;
        for (const auto &[ip, until] : bans4)
        {
            const uint64_t remaining = until > now ? (until - now) : 0;
            std::cout << "  " << Common::ipAddressToString(ip) << " (" << remaining << "s remaining)" << std::endl;
        }
        for (const auto &[addr, until] : bans6)
        {
            const uint64_t remaining = until > now ? (until - now) : 0;
            std::cout << "  " << addr << " (" << remaining << "s remaining)" << std::endl;
        }

        return true;
    }

    if (sub == "add")
    {
        if (args.size() < 2 || args.size() > 3)
        {
            std::cout << "Usage: ban add <ip> [seconds]" << std::endl;
            return true;
        }

        uint64_t seconds = 900;
        if (args.size() == 3 && !Common::fromString(args[2], seconds))
        {
            std::cout << WarningMsg("Invalid ban seconds value.") << std::endl;
            return true;
        }

        if (seconds == 0)
        {
            std::cout << WarningMsg("Ban seconds must be greater than zero.") << std::endl;
            return true;
        }

        // Try IPv4 first, then IPv6
        try
        {
            const uint32_t ipHostOrder = System::Ipv4Address(args[1]).getValue();
            const uint32_t ip = hostToNetwork(ipHostOrder);
            m_srv.ban_host(ip, seconds);
            std::cout << SuccessMsg("Ban added for " + Common::ipAddressToString(ip) + " (" + std::to_string(seconds) + "s)")
                      << std::endl;
            return true;
        }
        catch (const std::exception &) {}

        // Try IPv6
        try
        {
            System::IpAddress addr6(args[1]);
            if (!addr6.isV6())
            {
                std::cout << WarningMsg("Invalid IP address: " + args[1]) << std::endl;
                return true;
            }
            const std::string normalized = addr6.toString();
            m_srv.ban_host6(normalized, seconds);
            std::cout << SuccessMsg("Ban added for " + normalized + " (" + std::to_string(seconds) + "s)")
                      << std::endl;
            return true;
        }
        catch (const std::exception &)
        {
            std::cout << WarningMsg("Invalid IP address: " + args[1]) << std::endl;
            return true;
        }
    }

    if (sub == "delete")
    {
        if (args.size() != 2)
        {
            std::cout << "Usage: ban delete <ip>" << std::endl;
            return true;
        }

        // Try IPv4 first, then IPv6
        try
        {
            const uint32_t ipHostOrder = System::Ipv4Address(args[1]).getValue();
            const uint32_t ip = hostToNetwork(ipHostOrder);
            const bool removed = m_srv.unban_host(ip);

            if (!removed)
            {
                std::cout << WarningMsg("IP not found in ban list.") << std::endl;
                return true;
            }

            std::cout << SuccessMsg("Ban removed for " + Common::ipAddressToString(ip)) << std::endl;
            return true;
        }
        catch (const std::exception &) {}

        // Try IPv6
        try
        {
            System::IpAddress addr6(args[1]);
            if (!addr6.isV6())
            {
                std::cout << WarningMsg("Invalid IP address: " + args[1]) << std::endl;
                return true;
            }
            const std::string normalized = addr6.toString();
            const bool removed = m_srv.unban_host6(normalized);

            if (!removed)
            {
                std::cout << WarningMsg("IP not found in ban list.") << std::endl;
                return true;
            }

            std::cout << SuccessMsg("Ban removed for " + normalized) << std::endl;
            return true;
        }
        catch (const std::exception &)
        {
            std::cout << WarningMsg("Invalid IP address: " + args[1]) << std::endl;
            return true;
        }
    }

    std::cout << "Usage: ban list | ban add <ip> [seconds] | ban delete <ip>" << std::endl;
    return true;
}
