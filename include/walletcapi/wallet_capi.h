#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define WALLET_CAPI_EXPORT __declspec(dllexport)
#else
#define WALLET_CAPI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wallet_handle wallet_handle_t;
typedef int32_t wallet_status_t;

enum
{
    WALLET_CAPI_API_VERSION = 1
};

enum
{
    WALLET_EVENT_NONE = 0,
    WALLET_EVENT_SYNCED = 1,
    WALLET_EVENT_TRANSACTION = 2
};

WALLET_CAPI_EXPORT uint32_t wallet_capi_api_version(void);
WALLET_CAPI_EXPORT const char *wallet_capi_version_string(void);

WALLET_CAPI_EXPORT wallet_status_t wallet_open(
    const char *filename,
    const char *password,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_create(
    const char *filename,
    const char *password,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_restore_from_seed(
    const char *mnemonic_seed,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_restore_from_keys(
    const char *private_spend_key_hex,
    const char *private_view_key_hex,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_restore_view(
    const char *private_view_key_hex,
    const char *address,
    const char *filename,
    const char *password,
    uint64_t scan_height,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl,
    uint32_t sync_threads,
    wallet_handle_t **out_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_delete_file(const char *filename);

WALLET_CAPI_EXPORT void wallet_close(wallet_handle_t *wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_save(wallet_handle_t *wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_sync_status(
    wallet_handle_t *wallet,
    uint64_t *out_wallet_height,
    uint64_t *out_local_daemon_height,
    uint64_t *out_network_height);

/* Drive one synchronous sync step (download + process one batch of blocks).
   Intended for WASM single-threaded mode. Returns 1 if blocks were processed,
   0 if nothing to do, negative on error. */
WALLET_CAPI_EXPORT int wallet_sync_step(wallet_handle_t *wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_status_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_daemon_online(
    wallet_handle_t *wallet,
    bool *out_online);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_node_info_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_swap_node(
    wallet_handle_t *wallet,
    const char *daemon_host,
    uint16_t daemon_port,
    bool daemon_ssl);

/* Rescans the wallet from scan_height. Returns
   LITE_NODE_CANNOT_RESCAN_THAT_LOW (62) without touching the wallet when the
   connected daemon holds no data that far back and the wallet already holds
   transactions from below there - those would be lost with no way to find
   them again through that daemon. Connect a daemon holding the whole chain,
   or pass the daemon's lite start height (see daemonLiteStartHeight in
   wallet_get_status_json). */
WALLET_CAPI_EXPORT wallet_status_t wallet_reset(
    wallet_handle_t *wallet,
    uint64_t scan_height,
    uint64_t timestamp);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_transactions_json(
    wallet_handle_t *wallet,
    uint64_t start_height,
    uint64_t end_height_exclusive,
    bool include_unconfirmed,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_primary_address(
    wallet_handle_t *wallet,
    char **out_address,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_addresses_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_total_balance(
    wallet_handle_t *wallet,
    uint64_t *out_unlocked,
    uint64_t *out_locked);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_balance_for_address(
    wallet_handle_t *wallet,
    const char *address,
    uint64_t *out_unlocked,
    uint64_t *out_locked);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_balances_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_send_basic(
    wallet_handle_t *wallet,
    const char *destination,
    uint64_t amount,
    const char *payment_id,
    bool send_all,
    bool send_transaction,
    char **out_tx_hash,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_send_prepared(
    wallet_handle_t *wallet,
    const char *prepared_tx_hash_hex,
    char **out_tx_hash,
    size_t *out_len);

/* Drops a prepared-but-unsent transaction from the wallet's cache.
 * Preparing a transaction (send_transaction = false) stores it so it can be
 * relayed later; without this, an abandoned prepare stays cached until the
 * wallet is closed. Returns SUCCESS whether or not an entry was present. */
WALLET_CAPI_EXPORT wallet_status_t wallet_delete_prepared(
    wallet_handle_t *wallet,
    const char *prepared_tx_hash_hex);

WALLET_CAPI_EXPORT wallet_status_t wallet_send_advanced_json(
    wallet_handle_t *wallet,
    const char *request_json,
    bool send_transaction,
    char **out_result_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_tx_private_key(
    wallet_handle_t *wallet,
    const char *tx_hash_hex,
    char **out_tx_private_key_hex,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_transactions_status_json(
    wallet_handle_t *wallet,
    const char *request_json,
    char **out_result_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_private_view_key(
    wallet_handle_t *wallet,
    char **out_private_view_key_hex,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_spend_keys_json(
    wallet_handle_t *wallet,
    const char *address,
    char **out_result_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_mnemonic_seed(
    wallet_handle_t *wallet,
    char **out_mnemonic_seed,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_get_mnemonic_seed_for_address(
    wallet_handle_t *wallet,
    const char *address,
    char **out_mnemonic_seed,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_is_view_wallet(
    wallet_handle_t *wallet,
    bool *out_is_view_wallet);

WALLET_CAPI_EXPORT wallet_status_t wallet_change_password(
    wallet_handle_t *wallet,
    const char *new_password);

/* Export the full wallet state as a JSON string.
   The caller is responsible for writing the string to a file if needed.
   Free the returned buffer with wallet_string_free(). */
WALLET_CAPI_EXPORT wallet_status_t wallet_export_json(
    wallet_handle_t *wallet,
    char **out_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_add_subwallet_json(
    wallet_handle_t *wallet,
    char **out_result_json,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_import_subwallet_from_key(
    wallet_handle_t *wallet,
    const char *private_spend_key_hex,
    uint64_t scan_height,
    char **out_address,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_import_subwallet_from_index(
    wallet_handle_t *wallet,
    uint64_t wallet_index,
    uint64_t scan_height,
    char **out_address,
    size_t *out_len);

WALLET_CAPI_EXPORT wallet_status_t wallet_delete_subwallet(
    wallet_handle_t *wallet,
    const char *address);

WALLET_CAPI_EXPORT wallet_status_t wallet_create_integrated_address(
    const char *address,
    const char *payment_id,
    char **out_integrated_address,
    size_t *out_len);

/* Sweep amount_to_sweep (0 = all unlocked) to destination in multiple txs.
   out_result_json: {"results":[{"txHash":"..."} | {"error":N,"errorMessage":"..."},...]} */
WALLET_CAPI_EXPORT wallet_status_t wallet_sweep_to_address(
    wallet_handle_t *wallet,
    const char *destination,
    const char *payment_id,   /* nullable / empty = no payment ID */
    uint64_t amount_to_sweep, /* 0 = sweep entire unlocked balance */
    char **out_result_json,
    size_t *out_len);

/* Estimate how many transactions a sweep would produce and the total fee.
   Does NOT send anything. */
WALLET_CAPI_EXPORT wallet_status_t wallet_estimate_sweep(
    wallet_handle_t *wallet,
    uint64_t amount_to_sweep, /* 0 = sweep all */
    uint64_t *out_tx_count,
    uint64_t *out_total_fee);

WALLET_CAPI_EXPORT void wallet_string_free(char *p);

WALLET_CAPI_EXPORT wallet_status_t wallet_poll_event(
    wallet_handle_t *wallet,
    uint32_t timeout_ms,
    uint32_t *out_event_type,
    char **out_event_json,
    size_t *out_len);

/* Query TX PoW progress. Non-blocking, lock-free.
   out_active: true if PoW is currently running.
   out_elapsed_ms: milliseconds since PoW started (0 if not active).
   out_nonces: approximate nonces tried so far. */
WALLET_CAPI_EXPORT void wallet_get_pow_status(
    bool *out_active,
    uint64_t *out_elapsed_ms,
    uint64_t *out_nonces);

WALLET_CAPI_EXPORT const char *wallet_error_code_to_string(wallet_status_t code);
WALLET_CAPI_EXPORT const char *wallet_last_error_message(void);
WALLET_CAPI_EXPORT void wallet_clear_last_error_message(void);
WALLET_CAPI_EXPORT wallet_status_t wallet_set_log_level(const char *level);
WALLET_CAPI_EXPORT wallet_status_t wallet_take_logs_json(char **out_json, size_t *out_len);
WALLET_CAPI_EXPORT wallet_status_t wallet_clear_logs(void);

/* Set whether to scan coinbase (miner reward) transactions.
   By default coinbase transactions are skipped.
   Set scan=true to include them (needed if the wallet mines). */
WALLET_CAPI_EXPORT void wallet_set_scan_coinbase(bool scan);

/* Route the transaction proof of work through an external PoW server
   (wrkz-txpow-server) before falling back to this device's CPU. Applies to
   every wallet opened in this process. An empty host or a zero port turns it
   off again. The host may carry a scheme and a path ("https://node/txpow")
   for a server behind a reverse proxy; the scheme then overrides `ssl`. The
   wallet re-verifies every nonce the server returns, so a bad or slow server
   only ever costs time. */
WALLET_CAPI_EXPORT void wallet_set_tx_pow_server(const char *host, uint16_t port, bool ssl);

/* Checks a Tx PoW server without changing the configuration: one GET /health
   over the same client path a transaction would use, so it also catches a
   wallet build without SSL support. Blocks for up to ~15 seconds when the
   host does not answer; call it off the UI thread. Always returns SUCCESS
   with a JSON object in out_json: {"ok": true, "url": ..., "latency_ms": N,
   "threads": N, "queue": N, "capacity": N} or {"ok": false, "url": ...,
   "error": "..."}. Free out_json with wallet_string_free. */
WALLET_CAPI_EXPORT wallet_status_t wallet_test_tx_pow_server(
    const char *host,
    uint16_t port,
    bool ssl,
    char **out_json,
    size_t *out_len);

/* Checks a daemon without pointing the wallet at it: one /info request over
   the same client path the sync uses, against a throwaway connection, so
   nothing about the open wallet changes. Switching nodes is the one setting
   that can leave a wallet unable to sync with nothing on screen to explain
   it, and a daemon that answers but is behind is just as bad as one that does
   not answer at all - hence the heights. Blocks for up to ~10 seconds when
   the host does not answer; call it off the UI thread. Always returns SUCCESS
   with a JSON object in out_json: {"ok": true, "url": ..., "latency_ms": N,
   "height": N, "networkHeight": N, "peerCount": N, "synced": bool} or
   {"ok": false, "url": ..., "error": "..."}. Free out_json with
   wallet_string_free. */
WALLET_CAPI_EXPORT wallet_status_t wallet_test_node(
    const char *host,
    uint16_t port,
    bool ssl,
    char **out_json,
    size_t *out_len);

#ifdef __cplusplus
}
#endif

