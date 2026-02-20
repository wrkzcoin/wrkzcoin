#include <walletcapi/wallet_capi.h>

#include <common/StringTools.h>
#include <common/FileSystemShim.h>
#include <errors/Errors.h>
#include <utilities/Addresses.h>
#include <utilities/Mixins.h>
#include <utilities/Utilities.h>
#include <walletbackend/JsonSerialization.h>
#include <walletbackend/WalletBackend.h>

#include "json.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

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
    if (out_wallet == nullptr || filename == nullptr || password == nullptr || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    const auto [error, wallet] = WalletBackend::openWallet(
        std::string(filename),
        std::string(password),
        std::string(daemon_host),
        daemon_port,
        daemon_ssl,
        sync_threads);

    return set_out_wallet(error, wallet, out_wallet);
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
    if (out_wallet == nullptr || filename == nullptr || password == nullptr || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

    const auto [error, wallet] = WalletBackend::createWallet(
        std::string(filename),
        std::string(password),
        std::string(daemon_host),
        daemon_port,
        daemon_ssl,
        sync_threads);

    return set_out_wallet(error, wallet, out_wallet);
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
    if (
        out_wallet == nullptr
        || mnemonic_seed == nullptr
        || filename == nullptr
        || password == nullptr
        || daemon_host == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

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

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;
    try
    {
        privateSpendKey.fromString(std::string(private_spend_key_hex));
        privateViewKey.fromString(std::string(private_view_key_hex));
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(INVALID_KEY_FORMAT);
    }

    const auto [error, wallet] = WalletBackend::importWalletFromKeys(
        privateSpendKey,
        privateViewKey,
        std::string(filename),
        std::string(password),
        scan_height,
        std::string(daemon_host),
        daemon_port,
        daemon_ssl,
        sync_threads);

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
    try
    {
        privateViewKey.fromString(std::string(private_view_key_hex));
    }
    catch (...)
    {
        return static_cast<wallet_status_t>(INVALID_KEY_FORMAT);
    }

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

wallet_status_t wallet_delete_file(const char *filename)
{
    if (filename == nullptr)
    {
        return static_cast<wallet_status_t>(UNKNOWN_ERROR);
    }

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
    nlohmann::json j{
        {"walletBlockCount", s.walletBlockCount},
        {"localDaemonBlockCount", s.localDaemonBlockCount},
        {"networkBlockCount", s.networkBlockCount},
        {"peerCount", s.peerCount},
        {"lastKnownHashrate", s.lastKnownHashrate},
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

    nlohmann::json j{
        {"transactions", txs},
    };

    if (include_unconfirmed)
    {
        j["unconfirmedTransactions"] = instance->getUnconfirmedTransactions();
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

wallet_status_t wallet_send_fusion_basic(
    wallet_handle_t *wallet,
    char **out_tx_hash,
    size_t *out_len)
{
    std::shared_ptr<WalletBackend> instance;
    const auto status = get_wallet(wallet, instance);
    if (status != static_cast<wallet_status_t>(SUCCESS))
    {
        return status;
    }

    const auto [error, txHash] = instance->sendFusionTransactionBasic();
    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    return alloc_out_string(Common::podToHex(txHash), out_tx_hash, out_len);
}

wallet_status_t wallet_send_fusion_advanced_json(
    wallet_handle_t *wallet,
    const char *request_json,
    char **out_tx_hash,
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

    std::string destination = instance->getPrimaryAddress();
    if (json_has(body, "destination") && body["destination"].is_string())
    {
        destination = body["destination"].get<std::string>();
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

    std::vector<std::string> sourceAddresses;
    if (json_has(body, "sourceAddresses") && body["sourceAddresses"].is_array())
    {
        sourceAddresses = body["sourceAddresses"].get<std::vector<std::string>>();
    }

    std::vector<uint8_t> extraData;
    if (json_has(body, "extra") && body["extra"].is_string())
    {
        const auto extraHex = body["extra"].get<std::string>();
        if (!Common::fromHex(extraHex, extraData))
        {
            return static_cast<wallet_status_t>(INVALID_EXTRA_DATA);
        }
    }

    std::optional<uint64_t> optimizeTarget;
    if (json_has(body, "optimizeTarget") && body["optimizeTarget"].is_number_unsigned())
    {
        optimizeTarget = body["optimizeTarget"].get<uint64_t>();
    }

    const auto [error, txHash] = instance->sendFusionTransactionAdvanced(
        mixin,
        sourceAddresses,
        destination,
        extraData,
        optimizeTarget);

    if (error)
    {
        return static_cast<wallet_status_t>(error.getErrorCode());
    }

    return alloc_out_string(Common::podToHex(txHash), out_tx_hash, out_len);
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

const char *wallet_error_code_to_string(wallet_status_t code)
{
    static thread_local std::string last;
    last = Error(static_cast<ErrorCode>(code)).getErrorMessage();
    return last.c_str();
}

