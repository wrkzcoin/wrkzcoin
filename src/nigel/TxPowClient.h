// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/TransactionPoW.h>

#include <cstdint>
#include <string>

/* Client for an external transaction PoW server (wrkz-txpow-server).

   Nothing is configured by default, so every wallet computes its proof of
   work locally exactly as before. A GUI wallet that lets the user point at a
   server calls configure(); from then on the wallet backend asks the server
   first and falls back to the local CPU whenever the server is unreachable,
   too slow, or answers with anything the wallet cannot verify. The server
   only ever sees the unsigned transaction prefix - the same bytes the daemon
   sees at broadcast - and hands back 8 nonce bytes. */
namespace TxPowClient
{
    struct Settings
    {
        /* Empty host means "no server, compute locally". */
        std::string host;

        uint16_t port = 0;

        bool ssl = false;

        /* Path the server is mounted under when it sits behind a reverse
           proxy, e.g. "/txpow". Empty when it is served from the root. */
        std::string basePath;

        /* Total time the wallet is willing to wait on the server before it
           gives up and computes locally. */
        uint32_t timeoutSeconds = 120;
    };

    /* An empty host disables the remote path. The host may be a pasted URL:
       "https://node.example.com/txpow" sets SSL from the scheme and keeps the
       path as the mount point; the port always comes from `port`. */
    void configure(const std::string &host, const uint16_t port, const bool ssl);

    Settings settings();

    bool configured();

    /* A solver bound to the current settings, or an empty function when no
       server is configured. Pass straight to generateTransactionPoWHeight(). */
    CryptoNote::RemotePoWSolver solver();
} // namespace TxPowClient
