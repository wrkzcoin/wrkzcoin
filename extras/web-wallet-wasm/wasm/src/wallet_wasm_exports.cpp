#include "wallet_wasm_exports.h"

#include <walletcapi/wallet_capi.h>
#include "json.hpp"
#include <WalletTypes.h>
#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <mnemonics/Mnemonics.h>
#include <subwallets/SubWallet.h>
#include <utilities/Addresses.h>
#include <utilities/Utilities.h>
#include <errors/ValidateParameters.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace
{
    using json = nlohmann::json;

    std::mutex g_mutex;
    std::unordered_map<int32_t, wallet_handle_t *> g_wallets;
    struct ScannerInput
    {
        uint64_t amount;
        uint64_t unlockTime;
    };

    struct BalanceScannerState
    {
        Crypto::SecretKey privateSpendKey{};
        Crypto::SecretKey privateViewKey{};
        Crypto::PublicKey publicSpendKey{};
        std::unordered_map<std::string, ScannerInput> unspent;
        std::unordered_map<std::string, uint64_t> ownedAmounts;
        uint64_t scannedBlocks = 0;
        uint64_t scannedTransactions = 0;
        uint64_t scannedOutputs = 0;
        uint64_t ownedOutputsSeen = 0;
        uint64_t spentOwnedOutputs = 0;
        bool initialized = false;
    };

    std::unordered_map<std::string, BalanceScannerState> g_scanners;
    int32_t g_next_id = 1;

    thread_local std::string g_response;

    json ok(const json &data = json::object())
    {
        return json{{"ok", true}, {"data", data}};
    }

    json err(const int32_t code)
    {
        const char *msg = wallet_error_code_to_string(code);
        return json{{"ok", false}, {"code", code}, {"error", (msg != nullptr) ? msg : "unknown"}};
    }

    bool has(const json &j, const char *key)
    {
        return j.find(key) != j.end();
    }

    json scannerError(const char *message)
    {
        return json{{"ok", false}, {"code", -1}, {"error", message}};
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE const char *wallet_wasm_request(const char *request_json)
{
    try
    {
        if (request_json == nullptr)
        {
            g_response = err(-1).dump();
            return g_response.c_str();
        }

        const json req = json::parse(request_json);
        const std::string command = req.value("command", "");

        if (command == "apiVersion")
        {
            g_response = ok(json{{"apiVersion", wallet_capi_api_version()}}).dump();
            return g_response.c_str();
        }

        if (command == "version")
        {
            g_response = ok(json{{"version", wallet_capi_version_string()}}).dump();
            return g_response.c_str();
        }

        if (command == "open" || command == "create")
        {
            if (!has(req, "filename") || !has(req, "password") || !has(req, "daemonHost") || !has(req, "daemonPort"))
            {
                g_response = err(-1).dump();
                return g_response.c_str();
            }

            wallet_handle_t *handle = nullptr;
            const std::string filename = req.at("filename").get<std::string>();
            const std::string password = req.at("password").get<std::string>();
            const std::string daemonHost = req.at("daemonHost").get<std::string>();
            const uint16_t daemonPort = static_cast<uint16_t>(req.at("daemonPort").get<uint32_t>());
            const bool daemonSsl = req.value("daemonSsl", false);
            const uint32_t syncThreads = req.value("syncThreads", 2u);

            wallet_status_t status = 0;
            if (command == "open")
            {
                status = wallet_open(
                    filename.c_str(),
                    password.c_str(),
                    daemonHost.c_str(),
                    daemonPort,
                    daemonSsl,
                    syncThreads,
                    &handle);
            }
            else
            {
                status = wallet_create(
                    filename.c_str(),
                    password.c_str(),
                    daemonHost.c_str(),
                    daemonPort,
                    daemonSsl,
                    syncThreads,
                    &handle);
            }

            if (status != 0 || handle == nullptr)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            std::lock_guard<std::mutex> lock(g_mutex);
            const int32_t id = g_next_id++;
            g_wallets[id] = handle;
            g_response = ok(json{{"walletId", id}}).dump();
            return g_response.c_str();
        }

        if (command == "restoreFromSeed" || command == "restoreFromKeys")
        {
            if (!has(req, "filename") || !has(req, "password") || !has(req, "daemonHost") || !has(req, "daemonPort"))
            {
                g_response = err(-1).dump();
                return g_response.c_str();
            }

            wallet_handle_t *handle = nullptr;
            const std::string filename = req.at("filename").get<std::string>();
            const std::string password = req.at("password").get<std::string>();
            const uint64_t scanHeight = req.value("scanHeight", 0ull);
            const std::string daemonHost = req.at("daemonHost").get<std::string>();
            const uint16_t daemonPort = static_cast<uint16_t>(req.at("daemonPort").get<uint32_t>());
            const bool daemonSsl = req.value("daemonSsl", false);
            const uint32_t syncThreads = req.value("syncThreads", 2u);

            wallet_status_t status = 0;
            if (command == "restoreFromSeed")
            {
                const std::string mnemonicSeed = req.value("mnemonicSeed", "");
                status = wallet_restore_from_seed(
                    mnemonicSeed.c_str(),
                    filename.c_str(),
                    password.c_str(),
                    scanHeight,
                    daemonHost.c_str(),
                    daemonPort,
                    daemonSsl,
                    syncThreads,
                    &handle);
            }
            else
            {
                const std::string privateSpendKey = req.value("privateSpendKey", "");
                const std::string privateViewKey = req.value("privateViewKey", "");
                status = wallet_restore_from_keys(
                    privateSpendKey.c_str(),
                    privateViewKey.c_str(),
                    filename.c_str(),
                    password.c_str(),
                    scanHeight,
                    daemonHost.c_str(),
                    daemonPort,
                    daemonSsl,
                    syncThreads,
                    &handle);
            }

            if (status != 0 || handle == nullptr)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            std::lock_guard<std::mutex> lock(g_mutex);
            const int32_t id = g_next_id++;
            g_wallets[id] = handle;
            g_response = ok(json{{"walletId", id}}).dump();
            return g_response.c_str();
        }

        if (command == "close")
        {
            const int32_t walletId = req.value("walletId", 0);
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto it = g_wallets.find(walletId);
            if (it == g_wallets.end())
            {
                g_response = err(-1).dump();
                return g_response.c_str();
            }

            wallet_close(it->second);
            g_wallets.erase(it);
            g_response = ok().dump();
            return g_response.c_str();
        }

        if (command == "deriveKeysFromSeed")
        {
            const std::string mnemonicSeed = req.value("mnemonicSeed", "");
            const auto [mnemonicError, privateSpendKey] = Mnemonics::MnemonicToPrivateKey(mnemonicSeed);
            if (mnemonicError)
            {
                g_response = json{
                    {"ok", false},
                    {"code", static_cast<int32_t>(mnemonicError.getErrorCode())},
                    {"error", mnemonicError.getErrorMessage()}}
                                 .dump();
                return g_response.c_str();
            }

            Crypto::SecretKey privateViewKey;
            Crypto::crypto_ops::generateViewFromSpend(privateSpendKey, privateViewKey);
            const std::string address = Utilities::privateKeysToAddress(privateSpendKey, privateViewKey);

            g_response = ok(json{
                                {"mnemonicSeed", mnemonicSeed},
                                {"privateSpendKey", Common::podToHex(privateSpendKey)},
                                {"privateViewKey", Common::podToHex(privateViewKey)},
                                {"address", address}})
                             .dump();
            return g_response.c_str();
        }

        if (command == "generateSeedKeys")
        {
            Crypto::PublicKey publicSpendKey;
            Crypto::SecretKey privateSpendKey;
            Crypto::generate_keys(publicSpendKey, privateSpendKey);

            Crypto::SecretKey privateViewKey;
            Crypto::crypto_ops::generateViewFromSpend(privateSpendKey, privateViewKey);
            const std::string mnemonicSeed = Mnemonics::PrivateKeyToMnemonic(privateSpendKey);
            const std::string address = Utilities::privateKeysToAddress(privateSpendKey, privateViewKey);

            g_response = ok(json{
                                {"mnemonicSeed", mnemonicSeed},
                                {"privateSpendKey", Common::podToHex(privateSpendKey)},
                                {"privateViewKey", Common::podToHex(privateViewKey)},
                                {"address", address}})
                             .dump();
            return g_response.c_str();
        }

        if (command == "deriveAddressFromKeys")
        {
            const std::string privateSpendHex = req.value("privateSpendKey", "");
            const std::string privateViewHex = req.value("privateViewKey", "");
            Crypto::SecretKey privateSpendKey;
            Crypto::SecretKey privateViewKey;
            if (!Common::podFromHex(privateSpendHex, privateSpendKey) || !Common::podFromHex(privateViewHex, privateViewKey))
            {
                g_response = scannerError("derive_address_invalid_keys").dump();
                return g_response.c_str();
            }
            const std::string address = Utilities::privateKeysToAddress(privateSpendKey, privateViewKey);
            g_response = ok(json{{"address", address}}).dump();
            return g_response.c_str();
        }

        if (command == "validateAddress")
        {
            const std::string address = req.value("address", "");
            const bool allowIntegrated = req.value("allowIntegrated", true);
            const Error error = validateAddresses({address}, allowIntegrated);
            if (error != SUCCESS)
            {
                g_response = ok(json{
                                    {"valid", false},
                                    {"reason", error.getErrorMessage()}})
                                 .dump();
                return g_response.c_str();
            }
            g_response = ok(json{{"valid", true}}).dump();
            return g_response.c_str();
        }

        if (command == "createIntegratedAddress")
        {
            const std::string address = req.value("address", "");
            const std::string paymentID = req.value("paymentId", "");
            const auto [error, integratedAddress] = Utilities::createIntegratedAddress(address, paymentID);
            if (error != SUCCESS)
            {
                g_response = ok(json{
                                    {"integratedAddress", ""},
                                    {"error", error.getErrorMessage()}})
                                 .dump();
                return g_response.c_str();
            }
            g_response = ok(json{{"integratedAddress", integratedAddress}}).dump();
            return g_response.c_str();
        }

        if (command == "scanSyncDataBalance")
        {
            if (!has(req, "scannerId") || !has(req, "privateSpendKey") || !has(req, "privateViewKey") || !has(req, "items"))
            {
                g_response = scannerError("scan_sync_data_missing_fields").dump();
                return g_response.c_str();
            }

            const std::string scannerId = req.at("scannerId").get<std::string>();
            const std::string privateSpendHex = req.at("privateSpendKey").get<std::string>();
            const std::string privateViewHex = req.at("privateViewKey").get<std::string>();
            const bool reset = req.value("reset", false);
            const uint64_t daemonHeight = req.value("daemonHeight", 0ULL);

            BalanceScannerState *scanner = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                auto &entry = g_scanners[scannerId];
                scanner = &entry;
            }

            if (reset || !scanner->initialized)
            {
                Crypto::SecretKey privateSpendKey;
                Crypto::SecretKey privateViewKey;

                if (!Common::podFromHex(privateSpendHex, privateSpendKey) || !Common::podFromHex(privateViewHex, privateViewKey))
                {
                    g_response = scannerError("scan_sync_data_invalid_keys").dump();
                    return g_response.c_str();
                }

                Crypto::PublicKey publicSpendKey;
                if (!Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey))
                {
                    g_response = scannerError("scan_sync_data_public_spend_derivation_failed").dump();
                    return g_response.c_str();
                }

                scanner->privateSpendKey = privateSpendKey;
                scanner->privateViewKey = privateViewKey;
                scanner->publicSpendKey = publicSpendKey;
                scanner->unspent.clear();
                scanner->ownedAmounts.clear();
                scanner->scannedBlocks = 0;
                scanner->scannedTransactions = 0;
                scanner->scannedOutputs = 0;
                scanner->ownedOutputsSeen = 0;
                scanner->spentOwnedOutputs = 0;
                scanner->initialized = true;
            }

            json txEntries = json::array();

            const auto scanTransaction = [&](const WalletTypes::RawCoinbaseTransaction &rawTX,
                                             const std::vector<CryptoNote::KeyInput> *keyInputs,
                                             const uint64_t blockHeight,
                                             const uint64_t blockTimestamp,
                                             const std::string &paymentID) {
                Crypto::KeyDerivation derivation;
                if (!Crypto::generate_key_derivation(rawTX.transactionPublicKey, scanner->privateViewKey, derivation))
                {
                    return;
                }

                SubWallet subwallet(
                    scanner->publicSpendKey,
                    scanner->privateSpendKey,
                    "",
                    0,
                    0,
                    true,
                    0);

                uint64_t outputIndex = 0;
                uint64_t incoming = 0;
                for (const auto &output : rawTX.keyOutputs)
                {
                    scanner->scannedOutputs += 1;
                    Crypto::PublicKey derivedSpendKey;
                    if (!Crypto::underive_public_key(derivation, outputIndex, output.key, derivedSpendKey))
                    {
                        outputIndex += 1;
                        continue;
                    }

                    if (derivedSpendKey == scanner->publicSpendKey)
                    {
                        const auto [keyImage, _privateEphemeral] = subwallet.getTxInputKeyImage(derivation, outputIndex, false);
                        const std::string keyImageHex = Common::podToHex(keyImage);
                        incoming += output.amount;
                        scanner->ownedAmounts[keyImageHex] = output.amount;
                        if (!scanner->unspent.count(keyImageHex))
                        {
                            scanner->unspent[keyImageHex] = ScannerInput{output.amount, rawTX.unlockTime};
                            scanner->ownedOutputsSeen += 1;
                        }
                    }

                    outputIndex += 1;
                }

                uint64_t outgoing = 0;
                if (keyInputs != nullptr)
                {
                    for (const auto &input : *keyInputs)
                    {
                        const std::string keyImageHex = Common::podToHex(input.keyImage);
                        const auto unspentIt = scanner->unspent.find(keyImageHex);
                        if (unspentIt != scanner->unspent.end())
                        {
                            outgoing += unspentIt->second.amount;
                            scanner->unspent.erase(unspentIt);
                            scanner->spentOwnedOutputs += 1;
                        }
                        else
                        {
                            const auto amountIt = scanner->ownedAmounts.find(keyImageHex);
                            if (amountIt != scanner->ownedAmounts.end())
                            {
                                outgoing += amountIt->second;
                            }
                        }
                    }
                }

                if (incoming > 0 || outgoing > 0)
                {
                    const std::string direction = incoming > 0 && outgoing > 0
                        ? "self"
                        : (incoming > 0 ? "incoming" : "outgoing");
                    const int64_t net = static_cast<int64_t>(incoming) - static_cast<int64_t>(outgoing);
                    txEntries.push_back(json{
                        {"txHash", Common::podToHex(rawTX.hash)},
                        {"blockHeight", blockHeight},
                        {"blockTimestamp", blockTimestamp},
                        {"paymentId", paymentID},
                        {"incomingAtomic", std::to_string(incoming)},
                        {"outgoingAtomic", std::to_string(outgoing)},
                        {"netAtomic", std::to_string(net)},
                        {"direction", direction}
                    });
                }
            };

            try
            {
                const auto items = req.at("items").get<std::vector<WalletTypes::WalletBlockInfo>>();
                for (const auto &block : items)
                {
                    scanner->scannedBlocks += 1;
                    if (block.coinbaseTransaction)
                    {
                        scanner->scannedTransactions += 1;
                        scanTransaction(*(block.coinbaseTransaction), nullptr, block.blockHeight, block.blockTimestamp, "");
                    }

                    for (const auto &tx : block.transactions)
                    {
                        scanner->scannedTransactions += 1;
                        scanTransaction(tx, &(tx.keyInputs), block.blockHeight, block.blockTimestamp, tx.paymentID);
                    }
                }
            }
            catch (...)
            {
                g_response = scannerError("scan_sync_data_parse_failed").dump();
                return g_response.c_str();
            }

            uint64_t unlocked = 0;
            uint64_t locked = 0;
            for (const auto &[_, input] : scanner->unspent)
            {
                if (Utilities::isInputUnlocked(input.unlockTime, daemonHeight))
                {
                    unlocked += input.amount;
                }
                else
                {
                    locked += input.amount;
                }
            }

            g_response = ok(json{
                                {"unlocked", std::to_string(unlocked)},
                                {"locked", std::to_string(locked)},
                                {"unspentOwnedOutputs", scanner->unspent.size()},
                                {"spentOwnedOutputs", scanner->spentOwnedOutputs},
                                {"scannedBlocks", scanner->scannedBlocks},
                                {"scannedTransactions", scanner->scannedTransactions},
                                {"scannedOutputs", scanner->scannedOutputs},
                                {"transactions", txEntries}})
                             .dump();
            return g_response.c_str();
        }

        wallet_handle_t *wallet = nullptr;
        {
            const int32_t walletId = req.value("walletId", 0);
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto it = g_wallets.find(walletId);
            if (it == g_wallets.end())
            {
                g_response = err(-1).dump();
                return g_response.c_str();
            }
            wallet = it->second;
        }

        if (command == "status")
        {
            char *jsonOut = nullptr;
            size_t len = 0;
            const wallet_status_t status = wallet_get_status_json(wallet, &jsonOut, &len);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            const std::string payload(jsonOut, len);
            wallet_string_free(jsonOut);
            g_response = ok(json::parse(payload)).dump();
            return g_response.c_str();
        }

        if (command == "balance")
        {
            uint64_t unlocked = 0;
            uint64_t locked = 0;
            const wallet_status_t status = wallet_get_total_balance(wallet, &unlocked, &locked);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            g_response = ok(json{{"unlocked", unlocked}, {"locked", locked}}).dump();
            return g_response.c_str();
        }

        if (command == "swapNode")
        {
            const std::string daemonHost = req.value("daemonHost", "");
            const uint16_t daemonPort = static_cast<uint16_t>(req.value("daemonPort", 0u));
            const bool daemonSsl = req.value("daemonSsl", false);

            const wallet_status_t status = wallet_swap_node(wallet, daemonHost.c_str(), daemonPort, daemonSsl);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            g_response = ok().dump();
            return g_response.c_str();
        }

        if (command == "daemonOnline")
        {
            bool online = false;
            const wallet_status_t status = wallet_daemon_online(wallet, &online);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            g_response = ok(json{{"online", online}}).dump();
            return g_response.c_str();
        }

        if (command == "backupSecrets")
        {
            char *addressOut = nullptr;
            size_t addressLen = 0;
            wallet_status_t status = wallet_get_primary_address(wallet, &addressOut, &addressLen);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            const std::string address(addressOut, addressLen);
            wallet_string_free(addressOut);

            char *seedOut = nullptr;
            size_t seedLen = 0;
            status = wallet_get_mnemonic_seed(wallet, &seedOut, &seedLen);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            const std::string mnemonicSeed(seedOut, seedLen);
            wallet_string_free(seedOut);

            char *viewKeyOut = nullptr;
            size_t viewKeyLen = 0;
            status = wallet_get_private_view_key(wallet, &viewKeyOut, &viewKeyLen);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            const std::string privateViewKey(viewKeyOut, viewKeyLen);
            wallet_string_free(viewKeyOut);

            char *spendKeysOut = nullptr;
            size_t spendKeysLen = 0;
            status = wallet_get_spend_keys_json(wallet, address.c_str(), &spendKeysOut, &spendKeysLen);
            if (status != 0)
            {
                g_response = err(status).dump();
                return g_response.c_str();
            }

            const std::string spendKeysPayload(spendKeysOut, spendKeysLen);
            wallet_string_free(spendKeysOut);

            const json spendKeys = json::parse(spendKeysPayload);
            g_response = ok(json{
                                {"address", address},
                                {"mnemonicSeed", mnemonicSeed},
                                {"privateViewKey", privateViewKey},
                                {"privateSpendKey", spendKeys.value("privateSpendKey", "")}})
                             .dump();
            return g_response.c_str();
        }

        g_response = err(-1).dump();
        return g_response.c_str();
    }
    catch (...)
    {
        g_response = err(-1).dump();
        return g_response.c_str();
    }
}
