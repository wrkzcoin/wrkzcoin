#include <walletcapi/wallet_capi.h>

#include <common/StringTools.h>
#include <config/Config.h>
#include <common/FileSystemShim.h>
#if defined(__EMSCRIPTEN__)
#include <wasm_fs_bridge.h>
#endif
#include <errors/Errors.h>
#include <utilities/Addresses.h>
#include <utilities/Mixins.h>
#include <utilities/Utilities.h>
#include <walletbackend/JsonSerialization.h>
#include <walletbackend/PowProgress.h>
#include <walletbackend/WalletBackend.h>
#include <logger/Logger.h>

#include "json.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <sstream>

struct wallet_event_state
{
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::pair<uint32_t, std::string>> events;
    std::atomic<bool> closed{false};
};

struct wallet_handle
{
    std::shared_ptr<WalletBackend> wallet;
    std::shared_ptr<wallet_event_state> event_state;
};

namespace
{
    const std::string kVersion = "wallet-capi/0.1";
    thread_local std::string g_last_error_message;
    std::mutex g_log_mutex;
    std::deque<nlohmann::json> g_logs;
    std::once_flag g_log_bridge_once;
    constexpr size_t kMaxLogs = 2000;

    void set_last_error_message(const std::string &message)
    {
        g_last_error_message = message;
    }

    void clear_last_error_message()
    {
        g_last_error_message.clear();
    }

    std::string lower_copy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    void ensure_log_bridge()
    {
        std::call_once(g_log_bridge_once, [] {
            Logger::logger.setLogLevel(Logger::DISABLED);
            Logger::logger.setLogCallback([](
                const std::string prettyMessage,
                const std::string message,
                const Logger::LogLevel level,
                const std::vector<Logger::LogCategory> categories) {
                nlohmann::json entry;
                entry["pretty"] = prettyMessage;
                entry["message"] = message;
                entry["level"] = lower_copy(Logger::logLevelToString(level));
                nlohmann::json cats = nlohmann::json::array();
                for (const auto &c : categories)
                {
                    cats.push_back(lower_copy(Logger::logCategoryToString(c)));
                }
                entry["categories"] = cats;
                entry["ts"] = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());

                std::lock_guard<std::mutex> lock(g_log_mutex);
                g_logs.push_back(std::move(entry));
                while (g_logs.size() > kMaxLogs)
                {
                    g_logs.pop_front();
                }
            });
        });
    }

    bool json_has(const nlohmann::json &obj, const char *key)
    {
        return obj.find(key) != obj.end();
    }

    wallet_status_t alloc_out_string(const std::string &value, char **out_ptr, size_t *out_len)
    {
        if (out_ptr == nullptr || out_len == nullptr)
        {
            return static_cast<wallet_status_t>(UNKNOWN_ERROR);
        }

        const auto size = value.size();
        auto *buffer = new char[size + 1];
        std::memcpy(buffer, value.data(), size);
        buffer[size] = '\0';
        *out_ptr = buffer;
        *out_len = size;
        return static_cast<wallet_status_t>(SUCCESS);
    }

    wallet_status_t get_wallet(wallet_handle_t *handle, std::shared_ptr<WalletBackend> &out_wallet)
    {
        ensure_log_bridge();
        if (handle == nullptr || !handle->wallet)
        {
            return static_cast<wallet_status_t>(UNKNOWN_ERROR);
        }

        out_wallet = handle->wallet;
        return static_cast<wallet_status_t>(SUCCESS);
    }

    wallet_status_t set_out_wallet(
        const Error &error,
        const std::shared_ptr<WalletBackend> &wallet,
        wallet_handle_t **out_wallet)
    {
        if (out_wallet == nullptr)
        {
            return static_cast<wallet_status_t>(UNKNOWN_ERROR);
        }

        if (error)
        {
            *out_wallet = nullptr;
            return static_cast<wallet_status_t>(error.getErrorCode());
        }

        auto *handle = new wallet_handle_t();
        handle->wallet = wallet;
        handle->event_state = std::make_shared<wallet_event_state>();

        const auto state = handle->event_state;
        wallet->m_eventHandler->onSynced.subscribe([state](const uint64_t height) {
            if (!state || state->closed.load())
            {
                return;
            }
            nlohmann::json j{{"height", height}};
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->events.emplace_back(WALLET_EVENT_SYNCED, j.dump());
                if (state->events.size() > 1024)
                {
                    state->events.pop_front();
                }
            }
            state->cv.notify_one();
        });

        wallet->m_eventHandler->onTransaction.subscribe([state](const WalletTypes::Transaction tx) {
            if (!state || state->closed.load())
            {
                return;
            }
            nlohmann::json j = tx;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->events.emplace_back(WALLET_EVENT_TRANSACTION, j.dump());
                if (state->events.size() > 1024)
                {
                    state->events.pop_front();
                }
            }
            state->cv.notify_one();
        });

        *out_wallet = handle;
        return static_cast<wallet_status_t>(SUCCESS);
    }
}

uint32_t wallet_capi_api_version(void)
{
    ensure_log_bridge();
    return WALLET_CAPI_API_VERSION;
}

const char *wallet_capi_version_string(void)
{
    return kVersion.c_str();
}

wallet_status_t wallet_open(
    const char *filename,
    const char *password,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet)
{
    clear_last_error_message();
    if (out_wallet == nullptr || filename == nullptr || password == nullptr || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    try
    {
        const auto [error, wallet] = WalletBackend::openWallet(
            std::string(filename),
            std::string(password),
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);
        return set_out_wallet(error, wallet, out_wallet);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_open_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_open_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
}

wallet_status_t wallet_create(
    const char *filename,
    const char *password,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet)
{
    clear_last_error_message();
    if (out_wallet == nullptr || filename == nullptr || password == nullptr || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    try
    {
        const auto [error, wallet] = WalletBackend::createWallet(
            std::string(filename),
            std::string(password),
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);
        return set_out_wallet(error, wallet, out_wallet);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_create_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_create_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
}

wallet_status_t wallet_restore_from_seed(
    const char *mnemonic_seed,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet)
{
    clear_last_error_message();
    if (
        out_wallet == nullptr
        || mnemonic_seed == nullptr
        || filename == nullptr
        || password == nullptr
        || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    try
    {
        const auto [error, wallet] = WalletBackend::importWalletFromSeed(
            std::string(mnemonic_seed),
            std::string(filename),
            std::string(password),
            scan_height,
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);
        return set_out_wallet(error, wallet, out_wallet);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_restore_from_seed_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_restore_from_seed_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
}

wallet_status_t wallet_restore_from_keys(
    const char *private_spend_key_hex,
    const char *private_view_key_hex,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet)
{
    clear_last_error_message();
    if (
        out_wallet == nullptr
        || private_spend_key_hex == nullptr
        || private_view_key_hex == nullptr
        || filename == nullptr
        || password == nullptr
        || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

#if defined(__EMSCRIPTEN__)
    (void)filename;
#endif

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;
    if (
        !Common::podFromHex(std::string(private_spend_key_hex), privateSpendKey)
        || !Common::podFromHex(std::string(private_view_key_hex), privateViewKey))
    {
        return static_cast<wallet_status_t>(INVALID_KEY_FORMAT);
    }

    std::tuple<Error, std::shared_ptr<WalletBackend>> importResult;
#if defined(__EMSCRIPTEN__)
    try
    {
        importResult = WalletBackend::importWalletFromKeysTransient(
            privateSpendKey,
            privateViewKey,
            std::string(password),
            scan_height,
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_restore_from_keys_transient_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_restore_from_keys_transient_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
#else
    try
    {
        importResult = WalletBackend::importWalletFromKeys(
            privateSpendKey,
            privateViewKey,
            std::string(filename),
            std::string(password),
            scan_height,
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_restore_from_keys_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_restore_from_keys_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
#endif

    const auto &[error, wallet] = importResult;

    return set_out_wallet(error, wallet, out_wallet);
}

wallet_status_t wallet_restore_view(
    const char *private_view_key_hex,
    const char *address,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet)
{
    clear_last_error_message();
    if (
        out_wallet == nullptr
        || private_view_key_hex == nullptr
        || address == nullptr
        || filename == nullptr
        || password == nullptr
        || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    Crypto::SecretKey privateViewKey;
    if (!Common::podFromHex(std::string(private_view_key_hex), privateViewKey))
    {
        return static_cast<wallet_status_t>(INVALID_KEY_FORMAT);
    }

    try
    {
        const auto [error, wallet] = WalletBackend::importViewWallet(
            privateViewKey,
            std::string(address),
            std::string(filename),
            std::string(password),
            scan_height,
            std::string(daemon_host),
            daemon_port,
            daemon_ssl,
            sync_threads);

        return set_out_wallet(error, wallet, out_wallet);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_restore_view_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_restore_view_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
}

wallet_status_t wallet_delete_file(const char *filename)
{
    if (filename == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

#if defined(__EMSCRIPTEN__)
    const bool removed = WasmFs::remove(std::string(filename));
    if (removed)
    {
        return static_cast<wallet_status_t>(SUCCESS);
    }
    return static_cast<wallet_status_t>(FILENAME_NON_EXISTENT);
#else
    std::error_code ec;
    const bool removed = fs::remove(std::string(filename), ec);

    if (removed)
    {
        return static_cast<wallet_status_t>(SUCCESS);
    }

    if (ec)
    {
        return static_cast<wallet_status_t>(INVALID_WALLET_FILENAME);
    }

    return static_cast<wallet_status_t>(FILENAME_NON_EXISTENT);
#endif
}

void wallet_close(wallet_handle_t *wallet)
{
    if (wallet != nullptr)
    {
        if (wallet->event_state)
        {
            wallet->event_state->closed.store(true);
            wallet->event_state->cv.notify_all();
        }
        if (wallet->wallet && wallet->wallet->m_eventHandler)
        {
            wallet->wallet->m_eventHandler->onSynced.unsubscribe();
            wallet->wallet->m_eventHandler->onTransaction.unsubscribe();
        }
    }
    delete wallet;
}

wallet_status_t wallet_save(wallet_handle_t *wallet)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const auto error = instance->save();
    return static_cast<wallet_status_t>(error.getErrorCode());
}

int wallet_sync_step(wallet_handle_t *wallet)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return -1;
    }
    return instance->syncStep() ? 1 : 0;
}

wallet_status_t wallet_get_sync_status(
    wallet_handle_t *wallet,
    uint64_t *out_wallet_height,
    uint64_t *out_local_daemon_height,
    uint64_t *out_network_height)
{
    if (out_wallet_height == nullptr || out_local_daemon_height == nullptr || out_network_height == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const auto [walletHeight, localHeight, networkHeight] = instance->getSyncStatus();
    *out_wallet_height = walletHeight;
    *out_local_daemon_height = localHeight;
    *out_network_height = networkHeight;

    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_get_status_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const auto s = instance->getStatus();
    const bool isDaemonSynced = s.networkBlockCount > 0 &&
                                s.localDaemonBlockCount >= s.networkBlockCount;
    const bool isWalletSynced = s.networkBlockCount > 0 &&
                                s.walletBlockCount >= s.networkBlockCount;
    nlohmann::json j{
        {"walletBlockCount", s.walletBlockCount},
        {"localDaemonBlockCount", s.localDaemonBlockCount},
        {"networkBlockCount", s.networkBlockCount},
        {"peerCount", s.peerCount},
        {"hashrate", s.lastKnownHashrate},
        {"isDaemonSynced", isDaemonSynced},
        {"isWalletSynced", isWalletSynced},
        {"isOutOfSync", !isDaemonSynced && !isWalletSynced},
        {"isViewWallet", instance->isViewWallet()},
        {"subWalletCount", static_cast<int>(instance->getAddresses().size())},
    };

    return alloc_out_string(j.dump(), out_json, out_len);
}

wallet_status_t wallet_daemon_online(
    wallet_handle_t *wallet,
    bool *out_online)
{
    if (out_online == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    *out_online = instance->daemonOnline();
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_get_node_info_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const auto [daemonHost, daemonPort, daemonSSL] = instance->getNodeAddress();
    const auto [nodeFee, nodeAddress] = instance->getNodeFee();

    nlohmann::json j{
        {"daemonHost", daemonHost},
        {"daemonPort", daemonPort},
        {"daemonSSL", daemonSSL},
        {"daemonOnline", instance->daemonOnline()},
        {"nodeFee", nodeFee},
        {"nodeAddress", nodeAddress},
    };

    return alloc_out_string(j.dump(), out_json, out_len);
}

wallet_status_t wallet_swap_node(
    wallet_handle_t *wallet,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl)
{
    if (daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    instance->swapNode(std::string(daemon_host), daemon_port, daemon_ssl);
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_reset(
    wallet_handle_t *wallet,
    uint64_t scan_height,
    uint64_t timestamp)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    instance->reset(scan_height, timestamp);
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_get_transactions_json(
    wallet_handle_t *wallet,
    uint64_t start_height,
    uint64_t end_height_exclusive,
    bool include_unconfirmed,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    std::vector<WalletTypes::Transaction> txs;
    if (end_height_exclusive > start_height)
    {
        txs = instance->getTransactionsRange(start_height, end_height_exclusive);
    }
    else
    {
        txs = instance->getTransactions();
    }

    /* Serialise a single transaction into the format the Flutter UI expects:
       - transfers: [{amount, type}]  where type=0 outgoing, 1 incoming
         (C++ map uses positive for incoming, negative for outgoing)
       - totalAmount: net signed amount (sum of transfer map)
       - isConfirmed: true for on-chain txs, false for mempool
       - address: empty string (multi-subwallet disambiguation not needed here) */
    auto serializeTx = [](const WalletTypes::Transaction &tx, bool confirmed) -> nlohmann::json {
        nlohmann::json transferArr = nlohmann::json::array();
        for (const auto &[pubKey, amount] : tx.transfers)
        {
            transferArr.push_back({
                {"amount", amount},
                {"type", amount >= 0 ? 1 : 0},
            });
        }
        return nlohmann::json{
            {"hash", Common::podToHex(tx.hash.data)},
            {"timestamp", tx.timestamp},
            {"blockHeight", tx.blockHeight},
            {"paymentID", tx.paymentID},
            {"unlockTime", tx.unlockTime},
            {"fee", tx.fee},
            {"isCoinbaseTransaction", tx.isCoinbaseTransaction},
            {"totalAmount", tx.totalAmount()},
            {"isConfirmed", confirmed},
            {"address", ""},
            {"transfers", transferArr},
        };
    };

    nlohmann::json txArray = nlohmann::json::array();
    for (const auto &tx : txs)
    {
        txArray.push_back(serializeTx(tx, true));
    }
    nlohmann::json j{{"transactions", txArray}};

    if (include_unconfirmed)
    {
        nlohmann::json unconfirmedArray = nlohmann::json::array();
        for (const auto &tx : instance->getUnconfirmedTransactions())
        {
            unconfirmedArray.push_back(serializeTx(tx, false));
        }
        j["unconfirmedTransactions"] = unconfirmedArray;
    }

    return alloc_out_string(j.dump(), out_json, out_len);
}

wallet_status_t wallet_get_primary_address(
    wallet_handle_t *wallet,
    char **out_address,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    return alloc_out_string(instance->getPrimaryAddress(), out_address, out_len);
}

wallet_status_t wallet_get_addresses_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    nlohmann::json j{{"addresses", instance->getAddresses()}};
    return alloc_out_string(j.dump(), out_json, out_len);
}

wallet_status_t wallet_get_total_balance(
    wallet_handle_t *wallet,
    uint64_t *out_unlocked,
    uint64_t *out_locked)
{
    if (out_unlocked == nullptr || out_locked == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [unlocked, locked] = instance->getTotalBalance();
    *out_unlocked = unlocked;
    *out_locked = locked;
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_get_balance_for_address(
    wallet_handle_t *wallet,
    const char *address,
    uint64_t *out_unlocked,
    uint64_t *out_locked)
{
    if (address == nullptr || out_unlocked == nullptr || out_locked == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, unlocked, locked] = instance->getBalance(std::string(address));
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    *out_unlocked = unlocked;
    *out_locked = locked;
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_get_balances_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto rows = instance->getBalances();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &[address, unlocked, locked] : rows)
    {
        arr.push_back({{"address", address}, {"unlocked", unlocked}, {"locked", locked}});
    }
    nlohmann::json j{{"balances", arr}};
    return alloc_out_string(j.dump(), out_json, out_len);
}

wallet_status_t wallet_send_basic(
    wallet_handle_t *wallet,
    const char *destination,
    uint64_t amount,
    const char *payment_id,
    bool send_all,
    bool send_transaction,
    char **out_tx_hash,
    size_t *out_len)
{
    if (destination == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const std::string payment = payment_id ? payment_id : "";
    const auto [error, txHash, prepared] =
        instance->sendTransactionBasic(std::string(destination), amount, payment, send_all, send_transaction);
    (void)prepared;

    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    return alloc_out_string(Common::podToHex(txHash), out_tx_hash, out_len);
}

wallet_status_t wallet_send_advanced_json(
    wallet_handle_t *wallet,
    const char *request_json,
    bool send_transaction,
    char **out_result_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    nlohmann::json body = nlohmann::json::object();
    if (request_json != nullptr && request_json[0] != '\0')
    {
        try
        {
            body = nlohmann::json::parse(request_json);
        }
        catch (...)
        {
            return static_cast<wallet_status_t>(UNKNOWN_ERROR);
        }
    }

    if (!json_has(body, "destinations") || !body["destinations"].is_array())
    {
        return static_cast<wallet_status_t>(NO_DESTINATIONS_GIVEN);
    }

    std::vector<std::pair<std::string, uint64_t>> destinations;
    for (const auto &destination : body["destinations"])
    {
        if (!json_has(destination, "address") || !json_has(destination, "amount"))
        {
            return static_cast<wallet_status_t>(UNKNOWN_ERROR);
        }
        destinations.emplace_back(
            destination["address"].get<std::string>(),
            destination["amount"].get<uint64_t>());
    }

    uint64_t mixin;
    if (json_has(body, "mixin") && body["mixin"].is_number_unsigned())
    {
        mixin = body["mixin"].get<uint64_t>();
    }
    else
    {
        std::tie(std::ignore, std::ignore, mixin) =
            Utilities::getMixinAllowableRange(instance->getStatus().networkBlockCount);
    }

    auto fee = WalletTypes::FeeType::MinimumFee();
    if (json_has(body, "fee") && body["fee"].is_number_unsigned())
    {
        fee = WalletTypes::FeeType::FixedFee(body["fee"].get<uint64_t>());
    }
    else if (json_has(body, "feePerByte") && body["feePerByte"].is_number())
    {
        fee = WalletTypes::FeeType::FeePerByte(body["feePerByte"].get<float>());
    }

    std::vector<std::string> sourceAddresses;
    if (json_has(body, "sourceAddresses") && body["sourceAddresses"].is_array())
    {
        sourceAddresses = body["sourceAddresses"].get<std::vector<std::string>>();
    }

    std::string paymentID;
    if (json_has(body, "paymentID") && body["paymentID"].is_string())
    {
        paymentID = body["paymentID"].get<std::string>();
    }

    std::string changeAddress;
    if (json_has(body, "changeAddress") && body["changeAddress"].is_string())
    {
        changeAddress = body["changeAddress"].get<std::string>();
    }

    uint64_t unlockTime = 0;
    if (json_has(body, "unlockTime") && body["unlockTime"].is_number_unsigned())
    {
        unlockTime = body["unlockTime"].get<uint64_t>();
    }

    std::vector<uint8_t> extraData;
    if (json_has(body, "extra") && body["extra"].is_string())
    {
        const std::string extra = body["extra"].get<std::string>();
        if (!Common::fromHex(extra, extraData))
        {
            return static_cast<wallet_status_t>(INVALID_EXTRA_DATA);
        }
    }

    const auto [error, hash, prepared] = instance->sendTransactionAdvanced(
        destinations,
        mixin,
        fee,
        paymentID,
        sourceAddresses,
        changeAddress,
        unlockTime,
        extraData,
        false,
        send_transaction);

    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    nlohmann::json result{
        {"transactionHash", hash},
        {"fee", prepared.fee},
        {"relayedToNetwork", send_transaction}};
    return alloc_out_string(result.dump(), out_result_json, out_len);
}

wallet_status_t wallet_get_tx_private_key(
    wallet_handle_t *wallet,
    const char *tx_hash_hex,
    char **out_tx_private_key_hex,
    size_t *out_len)
{
    if (tx_hash_hex == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    Crypto::Hash hash;
    try
    {
        hash.fromString(std::string(tx_hash_hex));
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(HASH_INVALID);
    }
    const auto [error, key] = instance->getTxPrivateKey(hash);
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    return alloc_out_string(Common::podToHex(key), out_tx_private_key_hex, out_len);
}

wallet_status_t wallet_get_transactions_status_json(
    wallet_handle_t *wallet,
    const char *request_json,
    char **out_result_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    if (request_json == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    nlohmann::json body;
    try
    {
        body = nlohmann::json::parse(request_json);
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    if (!json_has(body, "hashes") || !body["hashes"].is_array())
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::unordered_set<Crypto::Hash> hashes;
    for (const auto &item : body["hashes"])
    {
        if (!item.is_string())
        {
            return static_cast<wallet_status_t>(HASH_INVALID);
        }
        Crypto::Hash h;
        try
        {
            h.fromString(item.get<std::string>());
        }
        catch (...)
        {
            return static_cast<wallet_status_t>(HASH_INVALID);
        }
        hashes.insert(h);
    }

    std::unordered_set<Crypto::Hash> inPool;
    std::unordered_set<Crypto::Hash> inBlock;
    std::unordered_set<Crypto::Hash> unknown;
    const bool ok = instance->getTransactionsStatus(hashes, inPool, inBlock, unknown);

    auto toHexArray = [](const std::unordered_set<Crypto::Hash> &set) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto &h : set)
        {
            arr.push_back(Common::podToHex(h));
        }
        return arr;
    };

    nlohmann::json result{
        {"requestSucceeded", ok},
        {"inPool", toHexArray(inPool)},
        {"inBlock", toHexArray(inBlock)},
        {"unknown", toHexArray(unknown)}};

    return alloc_out_string(result.dump(), out_result_json, out_len);
}

wallet_status_t wallet_get_private_view_key(
    wallet_handle_t *wallet,
    char **out_private_view_key_hex,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    return alloc_out_string(Common::podToHex(instance->getPrivateViewKey()), out_private_view_key_hex, out_len);
}

wallet_status_t wallet_get_spend_keys_json(
    wallet_handle_t *wallet,
    const char *address,
    char **out_result_json,
    size_t *out_len)
{
    if (address == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, publicSpendKey, privateSpendKey, walletIndex] = instance->getSpendKeys(std::string(address));
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    nlohmann::json j{
        {"publicSpendKey", publicSpendKey},
        {"privateSpendKey", privateSpendKey},
        {"walletIndex", walletIndex}};
    return alloc_out_string(j.dump(), out_result_json, out_len);
}

wallet_status_t wallet_get_mnemonic_seed(
    wallet_handle_t *wallet,
    char **out_mnemonic_seed,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, seed] = instance->getMnemonicSeed();
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    return alloc_out_string(seed, out_mnemonic_seed, out_len);
}

wallet_status_t wallet_get_mnemonic_seed_for_address(
    wallet_handle_t *wallet,
    const char *address,
    char **out_mnemonic_seed,
    size_t *out_len)
{
    if (address == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, seed] = instance->getMnemonicSeedForAddress(std::string(address));
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    return alloc_out_string(seed, out_mnemonic_seed, out_len);
}

wallet_status_t wallet_is_view_wallet(
    wallet_handle_t *wallet,
    bool *out_is_view_wallet)
{
    if (out_is_view_wallet == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    *out_is_view_wallet = instance->isViewWallet();
    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_change_password(
    wallet_handle_t *wallet,
    const char *new_password)
{
    if (new_password == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto error = instance->changePassword(std::string(new_password));
    return static_cast<wallet_status_t>(error.getErrorCode());
}

wallet_status_t wallet_export_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    return alloc_out_string(instance->toJSON(), out_json, out_len);
}

wallet_status_t wallet_add_subwallet_json(
    wallet_handle_t *wallet,
    char **out_result_json,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, address, privateSpendKey, walletIndex] = instance->addSubWallet();
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    nlohmann::json j{
        {"address", address},
        {"privateSpendKey", privateSpendKey},
        {"walletIndex", walletIndex}};
    return alloc_out_string(j.dump(), out_result_json, out_len);
}

wallet_status_t wallet_import_subwallet_from_key(
    wallet_handle_t *wallet,
    const char *private_spend_key_hex,
    uint64_t scan_height,
    char **out_address,
    size_t *out_len)
{
    if (private_spend_key_hex == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    Crypto::SecretKey spendKey;
    try
    {
        spendKey.fromString(std::string(private_spend_key_hex));
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(INVALID_KEY_FORMAT);
    }
    const auto [error, address] = instance->importSubWallet(spendKey, scan_height);
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    return alloc_out_string(address, out_address, out_len);
}

wallet_status_t wallet_import_subwallet_from_index(
    wallet_handle_t *wallet,
    uint64_t wallet_index,
    uint64_t scan_height,
    char **out_address,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto [error, address] = instance->importSubWallet(wallet_index, scan_height);
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }
    return alloc_out_string(address, out_address, out_len);
}

wallet_status_t wallet_delete_subwallet(
    wallet_handle_t *wallet,
    const char *address)
{
    if (address == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }
    const auto error = instance->deleteSubWallet(std::string(address));
    return static_cast<wallet_status_t>(error.getErrorCode());
}

wallet_status_t wallet_send_prepared(
    wallet_handle_t *wallet,
    const char *prepared_tx_hash_hex,
    char **out_tx_hash,
    size_t *out_len)
{
    if (prepared_tx_hash_hex == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    Crypto::Hash hash;
    try
    {
        hash.fromString(std::string(prepared_tx_hash_hex));
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(HASH_INVALID);
    }

    const auto [error, txHash] = instance->sendPreparedTransaction(hash);
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    return alloc_out_string(Common::podToHex(txHash), out_tx_hash, out_len);
}

wallet_status_t wallet_sweep_to_address(
    wallet_handle_t *wallet,
    const char *destination,
    const char *payment_id,
    uint64_t amount_to_sweep,
    char **out_result_json,
    size_t *out_len)
{
    if (destination == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const std::string payment = payment_id ? payment_id : "";

    std::vector<std::tuple<Error, Crypto::Hash>> results;
    try
    {
        results = instance->sweepToAddress(std::string(destination), payment, amount_to_sweep);
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_sweep_to_address_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
    catch (...)
    {
        set_last_error_message("wallet_sweep_to_address_exception: unknown");
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto &[error, hash] : results)
    {
        if (error)
        {
            arr.push_back({
                {"error", error.getErrorCode()},
                {"errorMessage", error.getErrorMessage()}
            });
        }
        else
        {
            arr.push_back({{"txHash", Common::podToHex(hash)}});
        }
    }
    nlohmann::json j{{"results", arr}};
    return alloc_out_string(j.dump(), out_result_json, out_len);
}

wallet_status_t wallet_estimate_sweep(
    wallet_handle_t *wallet,
    uint64_t amount_to_sweep,
    uint64_t *out_tx_count,
    uint64_t *out_total_fee)
{
    if (out_tx_count == nullptr || out_total_fee == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    try
    {
        const auto [txCount, totalFee] = instance->estimateSweep("", amount_to_sweep);
        *out_tx_count = static_cast<uint64_t>(txCount);
        *out_total_fee = totalFee;
    }
    catch (const std::exception &e)
    {
        set_last_error_message(std::string("wallet_estimate_sweep_exception: ") + e.what());
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    return static_cast<wallet_status_t>(SUCCESS);
}

wallet_status_t wallet_create_integrated_address(
    const char *address,
    const char *payment_id,
    char **out_integrated_address,
    size_t *out_len)
{
    if (address == nullptr || payment_id == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    const auto [error, integrated] = Utilities::createIntegratedAddress(std::string(address), std::string(payment_id));
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    return alloc_out_string(integrated, out_integrated_address, out_len);
}

wallet_status_t wallet_poll_event(
    wallet_handle_t *wallet,
    uint32_t timeout_ms,
    uint32_t *out_event_type,
    char **out_event_json,
    size_t *out_len)
{
    if (out_event_type == nullptr || out_event_json == nullptr || out_len == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    *out_event_type = WALLET_EVENT_NONE;
    *out_event_json = nullptr;
    *out_len = 0;

    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    if (!wallet->event_state)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    auto &state = *wallet->event_state;
    std::unique_lock<std::mutex> lock(state.mutex);
    if (state.events.empty())
    {
        if (timeout_ms == 0)
        {
            return static_cast<wallet_status_t>(SUCCESS);
        }
        state.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&state] {
            return state.closed.load() || !state.events.empty();
        });
    }

    if (state.events.empty())
    {
        return static_cast<wallet_status_t>(SUCCESS);
    }

    const auto event = state.events.front();
    state.events.pop_front();
    lock.unlock();

    *out_event_type = event.first;
    return alloc_out_string(event.second, out_event_json, out_len);
}

void wallet_string_free(char *p)
{
    delete[] p;
}

void wallet_get_pow_status(
    bool *out_active,
    uint64_t *out_elapsed_ms,
    uint64_t *out_nonces)
{
    const bool running = PowProgress::active.load(std::memory_order_acquire);
    if (out_active != nullptr)
    {
        *out_active = running;
    }
    if (out_elapsed_ms != nullptr)
    {
        if (running)
        {
            const auto now = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            const auto start = PowProgress::startMs.load(std::memory_order_relaxed);
            *out_elapsed_ms = now > start ? now - start : 0;
        }
        else
        {
            *out_elapsed_ms = 0;
        }
    }
    if (out_nonces != nullptr)
    {
        *out_nonces = PowProgress::nonces.load(std::memory_order_relaxed);
    }
}

const char *wallet_error_code_to_string(wallet_status_t code)
{
    static thread_local std::string last;
    try
    {
        last = Error(static_cast<ErrorCode>(code)).getErrorMessage();
    }
    catch (...)
    {
        last = "Unknown wallet error code: " + std::to_string(code);
    }
    return last.c_str();
}

const char *wallet_last_error_message(void)
{
    return g_last_error_message.c_str();
}

void wallet_clear_last_error_message(void)
{
    clear_last_error_message();
}

wallet_status_t wallet_set_log_level(const char *level)
{
    ensure_log_bridge();
    if (level == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    try
    {
        Logger::logger.setLogLevel(Logger::stringToLogLevel(std::string(level)));
        return static_cast<wallet_status_t>(SUCCESS);
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }
}

wallet_status_t wallet_take_logs_json(char **out_json, size_t *out_len)
{
    ensure_log_bridge();
    if (out_json == nullptr || out_len == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    nlohmann::json payload;
    payload["entries"] = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        while (!g_logs.empty())
        {
            payload["entries"].push_back(g_logs.front());
            g_logs.pop_front();
        }
    }

    return alloc_out_string(payload.dump(), out_json, out_len);
}

wallet_status_t wallet_clear_logs(void)
{
    ensure_log_bridge();
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_logs.clear();
    return static_cast<wallet_status_t>(SUCCESS);
}

void wallet_set_scan_coinbase(bool scan)
{
    Config::config.wallet.skipCoinbaseTransactions = !scan;
}

