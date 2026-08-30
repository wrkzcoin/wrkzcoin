// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>
#include <string_view>

/* Make sure everything in here is const - or it won't compile! */
namespace WalletConfig
{
    /* The prefix your coins address starts with */
    const std::string_view addressPrefix = "Wrkz";

    /* Your coins 'Ticker', e.g. Monero = XMR, Bitcoin = BTC */
    const std::string ticker = "WRKZ";

    /* The filename to output the CSV to in save_csv */
    const std::string csvFilename = "transactions.csv";

    /* The filename to read+write the address book to - consider starting with
       a leading '.' to make it hidden under mac+linux */
    const std::string addressBookFilename = ".addressBook.json";

    /* The name of your deamon */
    const std::string daemonName = "Wrkzd";

    /* The name to call this wallet */
    const std::string walletName = "zedwallet";

    /* The name of service/walletd, the programmatic rpc interface to a
       wallet */
    const std::string walletdName = "wrkz-service";

    /* The full name of your crypto */
    const std::string coinName = std::string(CryptoNote::CRYPTONOTE_NAME);

    /* Where can your users contact you for support? E.g. discord */
    const std::string contactLink = "https://chat.wrkz.work";


    /* The number of decimals your coin has */
    const uint8_t numDecimalPlaces = CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT;

    /* The length of a standard address for your coin */
    const uint16_t standardAddressLength = 98;

    /* Short payment IDs are 16 hex chars (8 bytes). These are encrypted
       against the shared secret between sender and receiver, so only the two
       parties to the transaction can read them. */
    const uint16_t shortPaymentIDLength = 16;

    /* Long payment IDs are 64 hex chars (32 bytes). These are stored in
       plaintext and are readable by anyone. */
    const uint16_t longPaymentIDLength = 64;

    /* The length of an integrated address for your coin - It's the same as
       a normal address, but there is a paymentID included in there - since
       base58 encoding is done by encoding chunks of 8 chars at once into
       blocks of 11 chars, we can calculate this automatically. */
    const uint16_t integratedAddressLength = standardAddressLength + ((shortPaymentIDLength * 11) / 8);
    const uint16_t integratedAddressLengthLong = standardAddressLength + ((longPaymentIDLength * 11) / 8);

    /* The default fee value to use with transactions (in ATOMIC units!) */
    const uint64_t defaultFee = CryptoNote::parameters::MINIMUM_FEE_V1;

    /* The minimum fee value to allow with transactions (in ATOMIC units!) */
    const uint64_t minimumFee = CryptoNote::parameters::MINIMUM_FEE_V1;

    /* The minimum amount allowed to be sent - usually 1 (in ATOMIC units!) */
    const uint64_t minimumSend = 1000;

    /* Is a mixin of zero disabled on your network? */
    const bool mixinZeroDisabled = true;

    /* If a mixin of zero is disabled, at what height was it disabled? E.g.
       fork height, or 0, if never allowed. This is ignored if a mixin of
       zero is allowed */
    const uint64_t mixinZeroDisabledHeight = CryptoNote::parameters::MIXIN_LIMITS_V4_HEIGHT;

    /**
     * Max size of a post body response - 10MB
     * Will decrease the amount of blocks requested from the daemon if this
     * is exceeded.
     * Note - blockStoreMemoryLimit - maxBodyResponseSize should be greater
     * than zero, or no data will get cached.
     * Further note: Currently blocks request are not decreased if this is
     * exceeded. Needs to be implemented in future?
     */
    const size_t maxBodyResponseSize = 1024 * 1024 * 25;

    /**
     * The amount of memory to use storing downloaded blocks - 200MB
     */
    const size_t blockStoreMemoryLimit = 1024 * 1024 * 200;

    /**
     * The largest number of blocks we will ask a daemon for in a single
     * /getwalletsyncdata request.
     *
     * The wallet starts at BLOCKS_SYNCHRONIZING_DEFAULT_COUNT and probes
     * upwards towards this value, backing off if the daemon rejects the
     * request with a 400 (its own --rpc-max-block-count is lower). Daemons
     * predating that option silently clamp instead, which costs us nothing.
     *
     * This matters because public nodes rate limit per IP - fewer, larger
     * requests move far more blocks per rate limit slot than many small ones.
     */
    const uint64_t maxBlocksPerSyncRequest = 1000;

    /**
     * How long to wait for a daemon to finish producing a response, in
     * seconds, as distinct from how long to wait to reach it at all.
     *
     * Nothing caps how long a daemon's request handler may run, and its own
     * --rpc-write-timeout defaults to thirty seconds. Giving up sooner than
     * that abandons a response the daemon is still building: the rate limit
     * slot is spent either way, the daemon finishes the work regardless, and
     * we halve our batch size for a request that would have arrived.
     *
     * The connection timeout stays short so an unreachable node still fails
     * quickly.
     */
    const uint64_t daemonResponseTimeoutSeconds = 30;

    /**
     * How many block windows to have in flight at once while catching up.
     *
     * A single stream of requests spends most of its time waiting: the round
     * trip, then the daemon assembling the answer, then the transfer, all
     * before the next request is even sent. Asking for several consecutive
     * windows at once overlaps those waits.
     *
     * Each one costs a connection and a rate limit slot, so this stays small.
     * Set to 1 to disable parallel fetching entirely.
     */
    const size_t syncRequestConcurrency = 4;
} // namespace WalletConfig
