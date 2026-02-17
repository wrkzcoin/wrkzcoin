// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2020, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "version.h"
#include "JsonHelper.h"

#include <boost/format.hpp>
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
#include <utilities/Utilities.h>
#include <common/CheckDifficulty.h>
#include <common/FileSystemShim.h>
#include <map>

namespace
{
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

} // namespace

std::shared_ptr<httplib::Response> DaemonCommandsHandler::rpc_get(const std::string &path)
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
        "Run local DB compaction (can take time and increase IO)");
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
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving information from RPC server.") << std::endl;
        return false;
    }

    rapidjson::Document resp;

    if (resp.Parse(res->body.c_str()).HasParseError())
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
        upgradeHeights.push_back(height.GetUint64());
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
    statusTable.push_back({"DB Engine",             m_config.enableLevelDB ? "LevelDB" : "RocksDB"});
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

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::prune_status(const std::vector<std::string> &args)
{
    auto res = rpc_get("/info");

    if (!res || res->status != 200)
    {
        std::cout << WarningMsg("Problem retrieving prune status from RPC server.") << std::endl;
        return false;
    }

    rapidjson::Document resp;

    if (resp.Parse(res->body.c_str()).HasParseError())
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

    rapidjson::Document resp;

    if (resp.Parse(res->body.c_str()).HasParseError())
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

    rapidjson::Document resp;

    if (resp.Parse(res->body.c_str()).HasParseError())
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
    std::cout << InformationMsg("Configured Sync Failure Threshold: ")
              << SuccessMsg(std::to_string(m_config.syncPeerFailureThreshold)) << std::endl;
    std::cout << InformationMsg("Configured Sync Batch Min/Max: ")
              << SuccessMsg(std::to_string(m_config.syncBatchMin) + "/" + std::to_string(m_config.syncBatchMax))
              << std::endl;

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

    rapidjson::Document resp;

    if (resp.Parse(res->body.c_str()).HasParseError())
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
    const std::string engine = m_config.enableLevelDB ? "LevelDB" : "RocksDB";
    const fs::path dbPath = fs::path(m_config.dataDirectory) / (m_config.enableLevelDB ? "LevelDB" : "DB");
    const DbDirStats stats = collectDbDirStats(dbPath);
    const bool compressionEnabled = m_config.enableDbCompression;

    std::cout << InformationMsg("DB Engine: ") << SuccessMsg(engine) << std::endl;
    std::cout << InformationMsg("Compression Enabled: ")
              << SuccessMsg(compressionEnabled ? "Yes" : "No") << std::endl;

    if (m_config.enableLevelDB)
    {
        std::cout << InformationMsg("Compression Mode: ")
                  << SuccessMsg(compressionEnabled ? "Snappy (if available)" : "Disabled") << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Compression Mode: ")
                  << SuccessMsg(compressionEnabled ? "RocksDB ZSTD (L2+; L0/L1 uncompressed)" : "Disabled")
                  << std::endl;
    }

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
    std::cout << InformationMsg("Starting DB compaction. This may take a while...") << std::endl;

    const auto error = m_core.compactDatabase();
    if (error)
    {
        std::cout << WarningMsg("DB compaction failed: " + error.message()) << std::endl;
        return false;
    }

    std::cout << SuccessMsg("DB compaction completed.") << std::endl;
    return true;
}
