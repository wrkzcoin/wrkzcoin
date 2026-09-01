// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "common/ConsoleHandler.h"
#include "daemon/DaemonConfiguration.h"
#include "httplib.h"
#include "rpc/CoreRpcServerCommandsDefinitions.h"
#include "rpc/JsonRpc.h"
#include "rpc/RpcServer.h"
#include <daemon/LiteSnapshot.h>

#include <logging/LoggerManager.h>
#include <logging/LoggerRef.h>
#include <atomic>
#include <future>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

namespace CryptoNote
{
    class Core;

    class NodeServer;
} // namespace CryptoNote

class DaemonCommandsHandler
{
  public:
    ~DaemonCommandsHandler();

    DaemonCommandsHandler(
        CryptoNote::Core &core,
        CryptoNote::NodeServer &srv,
        std::shared_ptr<Logging::LoggerManager> log,
        const std::string ip,
        const uint32_t port,
        const DaemonConfig::DaemonConfiguration &config,
        /* When the RPC server bound a local socket, the console talks to it
           over that instead of looping back out through TCP. */
        const std::string rpcIpcPath = "");

    bool start_handling()
    {
        m_consoleHandler.start();
        return true;
    }

    void stop_handling()
    {
        m_consoleHandler.stop();
    }

    bool exit(const std::vector<std::string> &args);

    void start_boot_compaction_if_needed();

    void stop_compaction_scheduler();

    void wait_for_background_compaction();

  private:
    Common::ConsoleHandler m_consoleHandler;

    CryptoNote::Core &m_core;

    CryptoNote::NodeServer &m_srv;

    httplib::Client m_rpcServer;

    /* The maintenance scheduler polls /info on its own client rather than
       sharing the console's. httplib serialises requests per client, so one
       client meant an operator's `status` could queue behind a background poll
       - a console that looks stuck for reasons that have nothing to do with
       the command just typed. */
    httplib::Client m_maintenanceRpcServer;

    Logging::LoggerRef logger;

    DaemonConfig::DaemonConfiguration m_config;

    std::shared_ptr<Logging::LoggerManager> m_logManager;

    std::string get_commands_str();

    bool print_block_by_height(uint32_t height);

    bool print_block_by_hash(const std::string &arg);

    bool help(const std::vector<std::string> &args);

    bool print_pl(const std::vector<std::string> &args);

    bool print_cn(const std::vector<std::string> &args);

    bool set_log(const std::vector<std::string> &args);

    bool print_block(const std::vector<std::string> &args);

    bool print_tx(const std::vector<std::string> &args);

    bool print_pool(const std::vector<std::string> &args);

    bool print_pool_sh(const std::vector<std::string> &args);

    bool status(const std::vector<std::string> &args);

    bool prune_status(const std::vector<std::string> &args);

    bool sync_info(const std::vector<std::string> &args);

    bool save(const std::vector<std::string> &args);

    bool sync_tune(const std::vector<std::string> &args);

    bool sync_peers(const std::vector<std::string> &args);

    bool db_status(const std::vector<std::string> &args);

    bool compact_db(const std::vector<std::string> &args);

    /* Writes the index only region of the chain to a lite node snapshot file,
       so another machine can import it rather than spend days rebuilding it.
       Runs on its own thread - it walks the three biggest tables in the
       database - so the console stays usable and the work can be cancelled.
       See LITESNAPSHOT.md. */
    bool snapshot_export(const std::vector<std::string> &args);

    void refresh_snapshot_state_locked();

    bool ban(const std::vector<std::string> &args);

    httplib::Result rpc_get(const std::string &path);

    httplib::Result rpc_get(httplib::Client &client, const std::string &path);

    void refresh_compaction_state_locked();

    void compaction_scheduler_loop();

    std::string get_compaction_marker_path() const;

    bool compaction_marker_exists_locked() const;

    void create_compaction_marker_locked();

    void clear_compaction_marker_locked();

    std::mutex m_compactionMutex;

    std::future<std::pair<std::error_code, std::string>> m_compactionTask;

    bool m_compactionRunning = false;

    bool m_compactionHasResult = false;

    std::error_code m_compactionLastError;

    std::string m_compactionLastErrorDetails;

    uint64_t m_compactionStartedAt = 0;

    uint64_t m_compactionFinishedAt = 0;

    std::thread m_compactionSchedulerThread;

    std::atomic<bool> m_stopCompactionScheduler {false};

    uint64_t m_lastAutoPruneHeight = 0;

    uint64_t m_compactionStartedAtHeight = 0;

    uint64_t m_compactionFinishedAtHeight = 0;

    uint64_t m_schedulerCheckIntervalSeconds = 60;

    uint32_t m_nearSyncStreak = 0;

    std::mutex m_snapshotMutex;

    std::future<void> m_snapshotTask;

    bool m_snapshotRunning = false;

    bool m_snapshotHasResult = false;

    /* Empty means the last export succeeded. The worker never throws out of
       itself: an operator reads the reason here or from `snapshot_export
       status`, not from a stack unwinding through a console thread. */
    std::string m_snapshotError;

    std::string m_snapshotPath;

    std::string m_snapshotStage;

    uint64_t m_snapshotScanned = 0;

    uint64_t m_snapshotKept = 0;

    uint64_t m_snapshotStartedAt = 0;

    std::atomic<bool> m_snapshotCancel {false};

    CryptoNote::LiteSnapshot::Header m_snapshotResult;
};
