// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "version.h"
#include "JsonHelper.h"

#include <boost/format.hpp>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/CryptoNoteFormatUtils.h>
#include <cryptonotecore/Currency.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandler.h>
#include <algorithm>
#include <ctime>
#include <daemon/DaemonCommandsHandler.h>
#include <p2p/NetNode.h>
#include <rpc/JsonRpc.h>
#include <serialization/SerializationTools.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Utilities.h>
#include <common/CheckDifficulty.h>
#include <common/StringTools.h>
#include <common/FileSystemShim.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/hash.h>
#include <map>
#include <chrono>
#include <thread>
#include <fstream>
#include <limits>
#include <random>
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
    if (m_config.rpcAccessToken.empty())
    {
        return m_rpcServer.Get(path.c_str());
    }

    httplib::Headers headers = {{"X-API-Key", m_config.rpcAccessToken}};
    return m_rpcServer.Get(path.c_str(), headers);
}

DaemonCommandsHandler::DaemonCommandsHandler(
    CryptoNote::Core &core,
    CryptoNote::NodeServer &srv,
    std::shared_ptr<Logging::LoggerManager> log,
    const std::string ip,
    const uint32_t port,
    const DaemonConfig::DaemonConfiguration &config):
    m_core(core),
    m_srv(srv),
    logger(log, "daemon"),
    m_logManager(log),
    m_rpcServer(ip.c_str(), port),
    m_config(config)
{
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
        "masternodes",
        std::bind(&DaemonCommandsHandler::masternodes, this, std::placeholders::_1),
        "Show masternode count/list: masternodes [limit] [offset]");
    m_consoleHandler.setHandler(
        "print_chainlocks",
        std::bind(&DaemonCommandsHandler::print_chainlocks, this, std::placeholders::_1),
        "Show recent ChainLock status: print_chainlocks [count]");
    m_consoleHandler.setHandler(
        "print_islocks",
        std::bind(&DaemonCommandsHandler::print_islocks, this, std::placeholders::_1),
        "Show active InstantSend locks: print_islocks");
    m_consoleHandler.setHandler(
        "mn_registration_string",
        std::bind(&DaemonCommandsHandler::mn_registration_string, this, std::placeholders::_1),
        "Generate masternode registration token: mn_registration_string [optional_mn_id_hex]");
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
        "Manage DB compaction: compact_db [start|status|wait]");
    m_consoleHandler.setHandler(
        "ban",
        std::bind(&DaemonCommandsHandler::ban, this, std::placeholders::_1),
        "Manage in-memory host bans: ban list | ban add <ip> [seconds] | ban delete <ip>");
}

DaemonCommandsHandler::~DaemonCommandsHandler()
{
    stop_compaction_scheduler();
}

//--------------------------------------------------------------------------------
std::string DaemonCommandsHandler::get_commands_str()
{
    std::stringstream ss;
    ss << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
    ss << "Commands: " << ENDL;
    std::string usage = m_consoleHandler.getUsage();
    boost::replace_all(usage, "\n", "\n  ");
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
    try
    {
        uint32_t height = boost::lexical_cast<uint32_t>(arg);
        print_block_by_height(height);
    }
    catch (boost::bad_lexical_cast &)
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

bool DaemonCommandsHandler::masternodes(const std::vector<std::string> &args)
{
    size_t limit = 20;
    size_t offset = 0;

    if (args.size() > 1)
    {
        try
        {
            limit = static_cast<size_t>(std::stoull(args[1]));
        }
        catch (const std::exception &)
        {
            std::cout << WarningMsg("Invalid limit value. Usage: masternodes [limit] [offset]") << std::endl;
            return false;
        }
    }

    if (args.size() > 2)
    {
        try
        {
            offset = static_cast<size_t>(std::stoull(args[2]));
        }
        catch (const std::exception &)
        {
            std::cout << WarningMsg("Invalid offset value. Usage: masternodes [limit] [offset]") << std::endl;
            return false;
        }
    }

    const auto countRes = rpc_get("/masternodes/count");
    if (!countRes || countRes->status != 200)
    {
        std::cout << WarningMsg("Failed to query /masternodes/count") << std::endl;
        return false;
    }

    rapidjson::Document countDoc;
    if (countDoc.Parse(countRes->body.c_str()).HasParseError() || !countDoc.IsObject())
    {
        std::cout << WarningMsg("Invalid response from /masternodes/count") << std::endl;
        return false;
    }

    const uint64_t total = getUint64FromJSON(countDoc, "count");
    std::cout << InformationMsg("Masternodes total: ") << SuccessMsg(std::to_string(total)) << std::endl;

    const std::string path =
        "/masternodes?limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset);
    const auto listRes = rpc_get(path);
    if (!listRes || listRes->status != 200)
    {
        std::cout << WarningMsg("Failed to query " + path) << std::endl;
        return false;
    }

    rapidjson::Document listDoc;
    if (listDoc.Parse(listRes->body.c_str()).HasParseError() || !listDoc.IsObject())
    {
        std::cout << WarningMsg("Invalid response from /masternodes list") << std::endl;
        return false;
    }

    if (!hasMember(listDoc, "masternodes"))
    {
        std::cout << WarningMsg("Missing 'masternodes' field in RPC response") << std::endl;
        return false;
    }

    const auto &mnArray = getArrayFromJSON(listDoc, "masternodes");
    if (mnArray.Empty())
    {
        std::cout << InformationMsg("No masternodes in requested page.") << std::endl;
        return true;
    }

    for (const auto &entry : mnArray)
    {
        const auto id = getStringFromJSON(entry, "mn_id");
        const auto state = getStringFromJSON(entry, "state");
        const auto bonded = getBoolFromJSON(entry, "bonded");
        const auto bondAmount = getUint64FromJSON(entry, "bond_amount");
        const auto hasCollateral = getBoolFromJSON(entry, "has_collateral");
        const auto collateralAmount = getUint64FromJSON(entry, "collateral_amount");
        const auto collateralIndex = getUint64FromJSON(entry, "collateral_global_output_index");
        const auto hasEndpointCommitment = getBoolFromJSON(entry, "has_endpoint_commitment");
        const auto endpointCommitment = getStringFromJSON(entry, "endpoint_commitment");
        const auto health = getUint64FromJSON(entry, "health_percent");
        const auto spendLocked = getBoolFromJSON(entry, "spend_locked");
        const auto lastPaid = getUint64FromJSON(entry, "last_paid_height");

        std::cout << InformationMsg(id.substr(0, std::min<size_t>(16, id.size())) + "...")
                  << " state=" << state
                  << " bonded=" << (bonded ? "yes" : "no")
                  << " bond_amount=" << bondAmount
                  << " has_collateral=" << (hasCollateral ? "yes" : "no")
                  << " collateral_amount=" << collateralAmount
                  << " collateral_index=" << collateralIndex
                  << " endpoint_commitment="
                  << (hasEndpointCommitment ? endpointCommitment.substr(0, std::min<size_t>(16, endpointCommitment.size())) + "..." : "none")
                  << " health=" << health << "%"
                  << " spend_locked=" << (spendLocked ? "yes" : "no")
                  << " last_paid_height=" << lastPaid << std::endl;
    }

    return true;
}

bool DaemonCommandsHandler::mn_registration_string(const std::vector<std::string> &args)
{
    Crypto::Hash masternodeId;
    Crypto::Hash tokenId;

    if (args.size() > 1)
    {
        if (!Common::podFromHex(args[1], masternodeId))
        {
            std::cout << WarningMsg("Invalid masternode id hex. Usage: mn_registration_string [optional_mn_id_hex]")
                      << std::endl;
            return false;
        }
    }
    else
    {
        std::random_device rd;
        for (auto &b : masternodeId.data)
        {
            b = static_cast<uint8_t>(rd() & 0xff);
        }
    }

    {
        std::random_device rd;
        for (auto &b : tokenId.data)
        {
            b = static_cast<uint8_t>(rd() & 0xff);
        }
    }

    const uint32_t currentHeight = m_core.getTopBlockIndex();
    const uint32_t expiresAtHeight =
        currentHeight + static_cast<uint32_t>(CryptoNote::parameters::MASTERNODE_REGISTRATION_TOKEN_TTL_BLOCKS);

    const std::string token = "MNREG2:" + Common::podToHex(masternodeId) + ":" + Common::podToHex(tokenId) + ":"
                              + std::to_string(expiresAtHeight);
    const uint64_t bondAmount = CryptoNote::parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT;

    std::cout << InformationMsg("Masternode registration token:") << std::endl;
    std::cout << SuccessMsg(token) << std::endl;
    std::cout << InformationMsg("Token expires at height: ")
              << SuccessMsg(std::to_string(expiresAtHeight))
              << InformationMsg(" (current: ")
              << SuccessMsg(std::to_string(currentHeight))
              << InformationMsg(")")
              << std::endl;
    std::cout << InformationMsg("Required collateral minimum: ")
              << SuccessMsg(Utilities::formatAmount(bondAmount))
              << " (" << bondAmount << " atomic units)" << std::endl;
    std::cout << InformationMsg("Wallet CLI command: ")
              << SuccessMsg("mn_register " + token + " <addr:port | [ipv6]:port>") << std::endl;

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_chainlocks(const std::vector<std::string> &args)
{
    // Default: show last 10 heights from the top.
    uint32_t count = 10;
    if (!args.empty())
    {
        try
        {
            count = static_cast<uint32_t>(std::stoul(args[0]));
        }
        catch (...)
        {
            std::cout << WarningMsg("Invalid count. Usage: print_chainlocks [count]") << std::endl;
            return false;
        }
    }

    const uint32_t topHeight = m_core.getTopBlockIndex();
    const uint32_t startHeight = (topHeight >= count) ? (topHeight - count + 1) : 1;

    std::cout << InformationMsg("ChainLock status for heights " + std::to_string(startHeight)
                                + " - " + std::to_string(topHeight) + ":") << std::endl;

    bool anyLocked = false;
    for (uint32_t h = startHeight; h <= topHeight; ++h)
    {
        if (m_core.hasChainLock(h))
        {
            anyLocked = true;
            const auto lock = m_core.getChainLock(h);
            std::cout << SuccessMsg("  [LOCKED] height=" + std::to_string(h))
                      << InformationMsg(" votes=" + std::to_string(lock ? lock->votes.size() : 0))
                      << std::endl;
        }
    }

    if (!anyLocked)
    {
        std::cout << InformationMsg("  No ChainLocks in this range.") << std::endl;
    }

    return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_islocks(const std::vector<std::string> &args)
{
    std::cout << InformationMsg("Active InstantSend locks are tracked per key-image. "
                                "Use GET /instantsend/<txhash> via the RPC to query a specific TX.")
              << std::endl;
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
bool DaemonCommandsHandler::compact_db(const std::vector<std::string> &args)
{
    const std::string sub = args.empty() ? "start" : args[0];

    if (sub != "start" && sub != "status" && sub != "wait")
    {
        std::cout << "Usage: compact_db [start|status|wait]" << std::endl;
        return true;
    }

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
    logger(Logging::INFO) << "Starting DB compaction (manual console request).";
    m_compactionTask = std::async(std::launch::async, [this]() { return m_core.compactDatabaseDetailed(); });

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

        std::lock_guard<std::mutex> lock(m_compactionMutex);
        refresh_compaction_state_locked();

        const uint64_t now = static_cast<uint64_t>(time(nullptr));
        const uint64_t currentHeight = static_cast<uint64_t>(m_core.getTopBlockIndex()) + 1;

        {
            auto res = rpc_get("/info");
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
