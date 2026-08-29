// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <optional>

#include <common/IpcSocket.h>
#include <config/CryptoNoteConfig.h>
#include <logger/Logger.h>

struct ApiConfig
{
    /* The IP to listen for requests on */
    std::string rpcBindIp;

    /* What port should we listen on */
    uint16_t port;

    /* IPv6 address to listen on (empty = disabled) */
    std::string rpcBindIpv6Address;

    /* Whether to enable the IPv6 listener */
    bool rpcUseIpv6 = false;

    /* Local IPC socket to also serve the API on (empty = disabled) */
    std::string rpcIpcPath;

    /* Permission bits for the IPC socket file */
    uint32_t rpcIpcMode = Common::Ipc::DEFAULT_MODE;

    /* Optional group to own the IPC socket file */
    std::string rpcIpcGroup;

    /* Password the user must supply with each request */
    std::string rpcPassword;

    /* The value to use with the 'Access-Control-Allow-Origin' header */
    std::string corsHeader;

    /* Controls what level of messages to log */
    Logger::LogLevel logLevel = Logger::DISABLED;

    /* Optionally log to a file */
    std::optional<std::string> loggingFilePath;

    /* Controls whether an interactive console is provided */
    bool noConsole = false;

    unsigned int threads;

    /* Monero-style --tx-notify: http(s):// URL (JSON POST) or command template
       run for every transaction that gets confirmed for the open wallet. */
    std::string txNotify;

    /* Also fire while the wallet is far behind the daemon (rescan). */
    bool notifyDuringSync = false;
};

ApiConfig parseArguments(int argc, char **argv);
