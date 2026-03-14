/*
 * wallet_wasm_exports.cpp
 *
 * Emscripten entry point for the WRKZ web wallet.
 * Receives a JSON request string from JavaScript, dispatches to the
 * appropriate wallet_capi function, and returns a JSON response string.
 *
 * Exported function:
 *   char* wallet_wasm_request(const char* json_request)
 *
 * The caller (JavaScript) must free the returned pointer with _free().
 *
 * Request format:
 *   { "method": "<method_name>", "params": { ... } }
 *
 * Response format:
 *   { "ok": true,  "result": <value> }
 *   { "ok": false, "error": <code>, "errorMessage": "<msg>" }
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "walletcapi/wallet_capi.h"
#include "wasm_fs_bridge.h"

/* nlohmann/json — already available in the repo's external/ */
#include "json.hpp"

using json = nlohmann::json;

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Allocate a C string on the WASM heap so JS can read it and free(). */
static char *heap_strdup(const std::string &s)
{
    char *p = static_cast<char *>(malloc(s.size() + 1));
    if (p)
    {
        memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
    }
    return p;
}

static char *ok_json(const json &result)
{
    json r;
    r["ok"] = true;
    r["result"] = result;
    return heap_strdup(r.dump());
}

static char *err_json(int code, const std::string &msg)
{
    json r;
    r["ok"] = false;
    r["error"] = code;
    r["errorMessage"] = msg;
    return heap_strdup(r.dump());
}

static char *err_json(wallet_status_t code)
{
    const char *last = wallet_last_error_message();
    std::string msg = (last && last[0]) ? last : wallet_error_code_to_string(code);
    wallet_clear_last_error_message();
    return err_json(static_cast<int>(code), msg);
}

/* Read a required string param; returns empty string if missing. */
static std::string str_param(const json &p, const char *key)
{
    if (p.contains(key) && p[key].is_string())
        return p[key].get<std::string>();
    return {};
}

static uint64_t u64_param(const json &p, const char *key, uint64_t def = 0)
{
    if (p.contains(key) && p[key].is_number())
        return p[key].get<uint64_t>();
    return def;
}

static uint32_t u32_param(const json &p, const char *key, uint32_t def = 0)
{
    if (p.contains(key) && p[key].is_number())
        return p[key].get<uint32_t>();
    return def;
}

static uint16_t u16_param(const json &p, const char *key, uint16_t def = 0)
{
    return static_cast<uint16_t>(u32_param(p, key, def));
}

static bool bool_param(const json &p, const char *key, bool def = false)
{
    if (p.contains(key) && p[key].is_boolean())
        return p[key].get<bool>();
    return def;
}

/* Resolve syncThreads: honour an explicit non-zero caller value; if the
   caller passed 0 (no-thread mode), keep 0 in no-pthread builds so the JS
   syncStep timer drives sync, but auto-detect in pthread builds so the
   WASM background threads take over. */
static uint32_t resolve_sync_threads(const json &p)
{
    uint32_t requested = u32_param(p, "syncThreads", 0);
#ifdef __EMSCRIPTEN_PTHREADS__
    if (requested == 0)
    {
        uint32_t hw = static_cast<uint32_t>(std::thread::hardware_concurrency());
        requested = std::max(1u, std::min(hw, 4u)); // 1–4 threads
    }
#endif
    return requested;
}

/* Helper: extract string output from wallet_capi and build ok response. */
static char *string_result(wallet_status_t st, char *out, size_t /*len*/)
{
    if (st != 0)
        return err_json(st);
    std::string s(out ? out : "");
    wallet_string_free(out);
    return ok_json(s);
}

/* Helper: extract JSON output from wallet_capi and return as parsed result. */
static char *json_result(wallet_status_t st, char *out, size_t /*len*/)
{
    if (st != 0)
        return err_json(st);
    json parsed = json::parse(out ? out : "null", nullptr, false);
    wallet_string_free(out);
    return ok_json(parsed);
}

/* ------------------------------------------------------------------ */
/*  Global wallet handle (single wallet per WASM instance)             */
/* ------------------------------------------------------------------ */

static wallet_handle_t *g_wallet = nullptr;

/* ------------------------------------------------------------------ */
/*  Dispatch table                                                     */
/* ------------------------------------------------------------------ */

static char *dispatch(const std::string &method, const json &p)
{
    /* -------- version / info -------- */

    if (method == "apiVersion")
    {
        return ok_json(static_cast<int>(wallet_capi_api_version()));
    }

    if (method == "versionString")
    {
        const char *v = wallet_capi_version_string();
        return ok_json(v ? v : "");
    }

    if (method == "isPthreadsEnabled")
    {
#ifdef __EMSCRIPTEN_PTHREADS__
        return ok_json(true);
#else
        return ok_json(false);
#endif
    }

    /* -------- lifecycle (create / open / restore / close) -------- */

    if (method == "create")
    {
        if (g_wallet)
            return err_json(-1, "wallet already open");
        auto filename = str_param(p, "filename");
        auto password = str_param(p, "password");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        auto threads = resolve_sync_threads(p);
        wallet_status_t st = wallet_create(
            filename.c_str(), password.c_str(),
            host.c_str(), port, ssl, threads, &g_wallet);
        if (st != 0)
        {
            g_wallet = nullptr;
            return err_json(st);
        }
        return ok_json(true);
    }

    if (method == "open")
    {
        if (g_wallet)
            return err_json(-1, "wallet already open");
        auto filename = str_param(p, "filename");
        auto password = str_param(p, "password");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        auto threads = resolve_sync_threads(p);
        wallet_status_t st = wallet_open(
            filename.c_str(), password.c_str(),
            host.c_str(), port, ssl, threads, &g_wallet);
        if (st != 0)
        {
            g_wallet = nullptr;
            return err_json(st);
        }
        return ok_json(true);
    }

    if (method == "restoreFromSeed")
    {
        if (g_wallet)
            return err_json(-1, "wallet already open");
        auto seed = str_param(p, "mnemonicSeed");
        auto filename = str_param(p, "filename");
        auto password = str_param(p, "password");
        auto scanHeight = u64_param(p, "scanHeight");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        auto threads = resolve_sync_threads(p);
        wallet_status_t st = wallet_restore_from_seed(
            seed.c_str(), filename.c_str(), password.c_str(),
            scanHeight, host.c_str(), port, ssl, threads, &g_wallet);
        if (st != 0)
        {
            g_wallet = nullptr;
            return err_json(st);
        }
        return ok_json(true);
    }

    if (method == "restoreFromKeys")
    {
        if (g_wallet)
            return err_json(-1, "wallet already open");
        auto spendKey = str_param(p, "privateSpendKey");
        auto viewKey = str_param(p, "privateViewKey");
        auto filename = str_param(p, "filename");
        auto password = str_param(p, "password");
        auto scanHeight = u64_param(p, "scanHeight");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        auto threads = resolve_sync_threads(p);
        wallet_status_t st = wallet_restore_from_keys(
            spendKey.c_str(), viewKey.c_str(),
            filename.c_str(), password.c_str(),
            scanHeight, host.c_str(), port, ssl, threads, &g_wallet);
        if (st != 0)
        {
            g_wallet = nullptr;
            return err_json(st);
        }
        return ok_json(true);
    }

    if (method == "restoreViewWallet")
    {
        if (g_wallet)
            return err_json(-1, "wallet already open");
        auto viewKey = str_param(p, "privateViewKey");
        auto address = str_param(p, "address");
        auto filename = str_param(p, "filename");
        auto password = str_param(p, "password");
        auto scanHeight = u64_param(p, "scanHeight");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        auto threads = resolve_sync_threads(p);
        wallet_status_t st = wallet_restore_view(
            viewKey.c_str(), address.c_str(),
            filename.c_str(), password.c_str(),
            scanHeight, host.c_str(), port, ssl, threads, &g_wallet);
        if (st != 0)
        {
            g_wallet = nullptr;
            return err_json(st);
        }
        return ok_json(true);
    }

    if (method == "close")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        wallet_close(g_wallet);
        g_wallet = nullptr;
        return ok_json(true);
    }

    if (method == "save")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        wallet_status_t st = wallet_save(g_wallet);
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    if (method == "deleteFile")
    {
        auto filename = str_param(p, "filename");
        wallet_status_t st = wallet_delete_file(filename.c_str());
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    /* -------- sync step (WASM single-threaded mode) -------- */

    if (method == "syncStep")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        const int progressed = wallet_sync_step(g_wallet);
        json r;
        r["progressed"] = (progressed > 0);
        return ok_json(r);
    }

    /* -------- sync & node status -------- */

    if (method == "getSyncStatus")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        uint64_t wh = 0, ldh = 0, nh = 0;
        wallet_status_t st = wallet_get_sync_status(g_wallet, &wh, &ldh, &nh);
        if (st != 0)
            return err_json(st);
        json r;
        r["walletHeight"] = wh;
        r["localDaemonHeight"] = ldh;
        r["networkHeight"] = nh;
        return ok_json(r);
    }

    if (method == "getStatusJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_status_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "isDaemonOnline")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        bool online = false;
        wallet_status_t st = wallet_daemon_online(g_wallet, &online);
        if (st != 0)
            return err_json(st);
        return ok_json(online);
    }

    if (method == "getNodeInfoJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_node_info_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "swapNode")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto host = str_param(p, "daemonHost");
        auto port = u16_param(p, "daemonPort");
        auto ssl = bool_param(p, "daemonSsl");
        wallet_status_t st = wallet_swap_node(g_wallet, host.c_str(), port, ssl);
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    if (method == "reset")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto scanHeight = u64_param(p, "scanHeight");
        auto timestamp = u64_param(p, "timestamp");
        wallet_status_t st = wallet_reset(g_wallet, scanHeight, timestamp);
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    /* -------- balance -------- */

    if (method == "getTotalBalance")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        uint64_t unlocked = 0, locked = 0;
        wallet_status_t st = wallet_get_total_balance(g_wallet, &unlocked, &locked);
        if (st != 0)
            return err_json(st);
        json r;
        r["unlocked"] = unlocked;
        r["locked"] = locked;
        return ok_json(r);
    }

    if (method == "getBalanceForAddress")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto address = str_param(p, "address");
        uint64_t unlocked = 0, locked = 0;
        wallet_status_t st = wallet_get_balance_for_address(
            g_wallet, address.c_str(), &unlocked, &locked);
        if (st != 0)
            return err_json(st);
        json r;
        r["unlocked"] = unlocked;
        r["locked"] = locked;
        return ok_json(r);
    }

    if (method == "getBalancesJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_balances_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    /* -------- addresses -------- */

    if (method == "getPrimaryAddress")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_primary_address(g_wallet, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "getAddressesJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_addresses_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    /* -------- transactions -------- */

    if (method == "getTransactionsJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto startH = u64_param(p, "startHeight");
        auto endH = u64_param(p, "endHeight");
        auto inclUnconf = bool_param(p, "includeUnconfirmed", true);
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_transactions_json(
            g_wallet, startH, endH, inclUnconf, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "getTransactionsStatusJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto reqJson = str_param(p, "requestJson");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_transactions_status_json(
            g_wallet, reqJson.c_str(), &out, &len);
        return json_result(st, out, len);
    }

    if (method == "getTxPrivateKey")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto txHash = str_param(p, "txHash");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_tx_private_key(
            g_wallet, txHash.c_str(), &out, &len);
        return string_result(st, out, len);
    }

    /* -------- send / sweep -------- */

    if (method == "sendBasic")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto dest = str_param(p, "destination");
        auto amount = u64_param(p, "amount");
        auto paymentId = str_param(p, "paymentId");
        auto sendAll = bool_param(p, "sendAll");
        auto broadcast = bool_param(p, "broadcast", true);
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_send_basic(
            g_wallet, dest.c_str(), amount,
            paymentId.empty() ? nullptr : paymentId.c_str(),
            sendAll, broadcast, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "sendPrepared")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto hash = str_param(p, "preparedTxHash");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_send_prepared(
            g_wallet, hash.c_str(), &out, &len);
        return string_result(st, out, len);
    }

    if (method == "sendAdvancedJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto reqJson = str_param(p, "requestJson");
        auto broadcast = bool_param(p, "broadcast", true);
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_send_advanced_json(
            g_wallet, reqJson.c_str(), broadcast, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "sweepToAddress")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto dest = str_param(p, "destination");
        auto paymentId = str_param(p, "paymentId");
        auto amount = u64_param(p, "amountToSweep");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_sweep_to_address(
            g_wallet, dest.c_str(),
            paymentId.empty() ? nullptr : paymentId.c_str(),
            amount, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "estimateSweep")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto amount = u64_param(p, "amountToSweep");
        uint64_t txCount = 0, totalFee = 0;
        wallet_status_t st = wallet_estimate_sweep(g_wallet, amount, &txCount, &totalFee);
        if (st != 0)
            return err_json(st);
        json r;
        r["txCount"] = txCount;
        r["totalFee"] = totalFee;
        return ok_json(r);
    }

    /* -------- keys / seeds -------- */

    if (method == "getPrivateViewKey")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_private_view_key(g_wallet, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "getSpendKeysJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto address = str_param(p, "address");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_spend_keys_json(
            g_wallet, address.c_str(), &out, &len);
        return json_result(st, out, len);
    }

    if (method == "getMnemonicSeed")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_mnemonic_seed(g_wallet, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "getMnemonicSeedForAddress")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto address = str_param(p, "address");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_get_mnemonic_seed_for_address(
            g_wallet, address.c_str(), &out, &len);
        return string_result(st, out, len);
    }

    if (method == "isViewWallet")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        bool isView = false;
        wallet_status_t st = wallet_is_view_wallet(g_wallet, &isView);
        if (st != 0)
            return err_json(st);
        return ok_json(isView);
    }

    /* -------- password -------- */

    if (method == "changePassword")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto newPw = str_param(p, "newPassword");
        wallet_status_t st = wallet_change_password(g_wallet, newPw.c_str());
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    /* -------- export -------- */

    if (method == "exportJson")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_export_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    /* -------- subwallets -------- */

    if (method == "addSubwallet")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_add_subwallet_json(g_wallet, &out, &len);
        return json_result(st, out, len);
    }

    if (method == "importSubwalletFromKey")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto key = str_param(p, "privateSpendKey");
        auto scanH = u64_param(p, "scanHeight");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_import_subwallet_from_key(
            g_wallet, key.c_str(), scanH, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "importSubwalletFromIndex")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto idx = u64_param(p, "walletIndex");
        auto scanH = u64_param(p, "scanHeight");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_import_subwallet_from_index(
            g_wallet, idx, scanH, &out, &len);
        return string_result(st, out, len);
    }

    if (method == "deleteSubwallet")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto address = str_param(p, "address");
        wallet_status_t st = wallet_delete_subwallet(g_wallet, address.c_str());
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    /* -------- integrated address (no wallet needed) -------- */

    if (method == "createIntegratedAddress")
    {
        auto address = str_param(p, "address");
        auto paymentId = str_param(p, "paymentId");
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_create_integrated_address(
            address.c_str(), paymentId.c_str(), &out, &len);
        return string_result(st, out, len);
    }

    /* -------- events -------- */

    if (method == "pollEvent")
    {
        if (!g_wallet)
            return err_json(-1, "no wallet open");
        auto timeout = u32_param(p, "timeoutMs", 100);
        uint32_t evType = WALLET_EVENT_NONE;
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_poll_event(
            g_wallet, timeout, &evType, &out, &len);
        if (st != 0)
            return err_json(st);
        json r;
        r["eventType"] = evType;
        if (out)
        {
            r["eventData"] = json::parse(out, nullptr, false);
            wallet_string_free(out);
        }
        else
        {
            r["eventData"] = nullptr;
        }
        return ok_json(r);
    }

    /* -------- PoW status -------- */

    if (method == "getPowStatus")
    {
        bool active = false;
        uint64_t elapsed = 0, nonces = 0;
        wallet_get_pow_status(&active, &elapsed, &nonces);
        json r;
        r["active"] = active;
        r["elapsedMs"] = elapsed;
        r["nonces"] = nonces;
        return ok_json(r);
    }

    /* -------- logging -------- */

    if (method == "setLogLevel")
    {
        auto level = str_param(p, "level");
        wallet_status_t st = wallet_set_log_level(level.c_str());
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    if (method == "takeLogsJson")
    {
        char *out = nullptr;
        size_t len = 0;
        wallet_status_t st = wallet_take_logs_json(&out, &len);
        return json_result(st, out, len);
    }

    if (method == "clearLogs")
    {
        wallet_status_t st = wallet_clear_logs();
        if (st != 0)
            return err_json(st);
        return ok_json(true);
    }

    /* -------- coinbase scan -------- */

    if (method == "setScanCoinbase")
    {
        auto scan = bool_param(p, "scan");
        wallet_set_scan_coinbase(scan);
        return ok_json(true);
    }

    /* -------- browser storage bridge -------- */

    /*
     * importFileData — JS pushes binary wallet file into the in-memory store.
     *   params: { "filename": "...", "dataBase64": "<base64-encoded binary>" }
     * This lets JS load wallet data from IndexedDB/localStorage and inject it
     * before calling "open".
     */
    if (method == "importFileData")
    {
        auto filename = str_param(p, "filename");
        auto b64 = str_param(p, "dataBase64");
        if (filename.empty() || b64.empty())
            return err_json(-3, "importFileData requires filename and dataBase64");

        /* Decode base64 — nlohmann/json doesn't have b64, but we can use
           a simple inline decoder. For robustness, wallet_capi already
           links Common which has base64 utils, but we keep it self-contained
           with a minimal decoder. */
        static const std::string b64chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        auto b64val = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };

        std::vector<char> decoded;
        decoded.reserve(b64.size() * 3 / 4);
        int val = 0, bits = -8;
        for (char c : b64)
        {
            int v = b64val(c);
            if (v < 0) continue; /* skip padding / whitespace */
            val = (val << 6) | v;
            bits += 6;
            if (bits >= 0)
            {
                decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
                bits -= 8;
            }
        }

        WasmFs::write(filename, decoded);
        return ok_json(true);
    }

    /*
     * exportFileData — JS pulls binary wallet file from the in-memory store.
     *   params: { "filename": "..." }
     *   result: { "dataBase64": "<base64-encoded binary>" }
     * This lets JS persist wallet data to IndexedDB/localStorage after "save".
     */
    if (method == "exportFileData")
    {
        auto filename = str_param(p, "filename");
        if (filename.empty())
            return err_json(-3, "exportFileData requires filename");

        auto data = WasmFs::read(filename);
        if (data.empty())
            return err_json(-1, "file not found in store: " + filename);

        /* Encode to base64 */
        static const char b64table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        encoded.reserve(((data.size() + 2) / 3) * 4);
        for (size_t i = 0; i < data.size(); i += 3)
        {
            unsigned int n = (static_cast<unsigned char>(data[i]) << 16);
            if (i + 1 < data.size()) n |= (static_cast<unsigned char>(data[i + 1]) << 8);
            if (i + 2 < data.size()) n |= static_cast<unsigned char>(data[i + 2]);

            encoded += b64table[(n >> 18) & 0x3F];
            encoded += b64table[(n >> 12) & 0x3F];
            encoded += (i + 1 < data.size()) ? b64table[(n >> 6) & 0x3F] : '=';
            encoded += (i + 2 < data.size()) ? b64table[n & 0x3F] : '=';
        }

        json r;
        r["dataBase64"] = encoded;
        return ok_json(r);
    }

    /*
     * listFiles — List all filenames in the in-memory store.
     *   params: {}
     *   result: ["file1.wallet", "file2.wallet", ...]
     */
    if (method == "listFiles")
    {
        auto names = WasmFs::list();
        json arr = json::array();
        for (const auto &n : names)
            arr.push_back(n);
        return ok_json(arr);
    }

    /* -------- unknown method -------- */

    return err_json(-2, "unknown method: " + method);
}

/* ------------------------------------------------------------------ */
/*  Exported entry point                                               */
/* ------------------------------------------------------------------ */

extern "C"
{
    /*
     * wallet_wasm_request
     *
     * Called from JavaScript via:
     *   const resultPtr = Module._wallet_wasm_request(requestPtr);
     *   const resultStr = Module.UTF8ToString(resultPtr);
     *   Module._free(resultPtr);
     *
     * Input:  JSON string  { "method": "...", "params": { ... } }
     * Output: JSON string  { "ok": true/false, ... }
     *
     * The returned pointer is heap-allocated; the caller MUST free() it.
     */
    char *wallet_wasm_request(const char *request_json)
    {
        if (!request_json || !request_json[0])
            return err_json(-3, "empty request");

        json req;
        try
        {
            req = json::parse(request_json);
        }
        catch (const std::exception &e)
        {
            return err_json(-3, std::string("invalid JSON: ") + e.what());
        }

        if (!req.contains("method") || !req["method"].is_string())
            return err_json(-3, "missing 'method' field");

        std::string method = req["method"].get<std::string>();
        json params = req.contains("params") ? req["params"] : json::object();

        try
        {
            return dispatch(method, params);
        }
        catch (const std::exception &e)
        {
            return err_json(-4, std::string("exception: ") + e.what());
        }
        catch (...)
        {
            return err_json(-4, "unknown exception");
        }
    }
}
