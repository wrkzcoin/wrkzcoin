// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <optional>

#include <config/CryptoNoteConfig.h>
#include <logger/Logger.h>

struct ZedConfig
{
    /* Was the wallet file specified on CLI */
    bool walletGiven = false;

    /* Was the wallet pass specified on CLI */
    bool passGiven = false;

    /* The daemon host */
    std::string host;

    /* The daemon port */
    uint16_t port = CryptoNote::RPC_DEFAULT_PORT;

    /* The wallet file path */
    std::string walletFile;

    /* The wallet password */
    std::string walletPass;

    /* Controls what level of messages to log */
    Logger::LogLevel logLevel = Logger::FATAL;

    /* Optionally log to a file */
    std::optional<std::string> loggingFilePath;

    /* Use SSL with daemon */
    bool ssl = false;

    unsigned int threads;
};

ZedConfig parseArguments(int argc, char **argv);
