#include "wallet_wasm_exports.h"

#include <walletcapi/wallet_capi.h>
#include "json.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
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

        g_response = err(-1).dump();
        return g_response.c_str();
    }
    catch (...)
    {
        g_response = err(-1).dump();
        return g_response.c_str();
    }
}
