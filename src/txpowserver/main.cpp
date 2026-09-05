// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* wrkz-txpow-server: computes the transaction proof of work on behalf of
   wallets that would rather not spend their own CPU on it, such as phones
   and browsers. It only ever sees the unsigned transaction prefix and hands
   back the 8 nonce bytes; it holds no keys and cannot alter or spend
   anything. See TXPOWSERVER.md. */

#include "HttpApi.h"
#include "PowService.h"
#include "TxPowServerConfig.h"

#include <common/SignalHandler.h>
#include <config/CliHeader.h>
#include <logger/Logger.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char **argv)
{
    const TxPowServerConfig config = parseTxPowServerArguments(argc, argv);

    Logger::logger.setLogLevel(config.logLevel);

    std::ofstream logFile;

    if (config.logFile)
    {
        logFile.open(*config.logFile, std::ios_base::app);

        if (!logFile)
        {
            std::cout << "Could not open log file " << *config.logFile << std::endl;
            return 1;
        }
    }

    Logger::logger.setLogCallback([&config, &logFile](
                                      const std::string prettyMessage,
                                      const std::string message,
                                      const Logger::LogLevel level,
                                      const std::vector<Logger::LogCategory> categories) {
        std::cout << prettyMessage << std::endl;

        if (config.logFile)
        {
            logFile << prettyMessage << std::endl;
        }
    });

    std::cout << CryptoNote::getProjectCLIHeader() << std::endl;

    PowServiceLimits limits;
    limits.threads = config.threads;
    limits.maxQueue = config.maxQueue;
    limits.maxDifficulty = config.maxDifficulty;
    limits.jobTimeoutSeconds = config.jobTimeoutSeconds;
    limits.resultTtlSeconds = config.resultTtlSeconds;

    PowService service(limits);

    service.start();

    HttpApi api(config, service);

    if (!api.start())
    {
        service.stop();
        return 1;
    }

    std::cout << "Tx PoW server listening on http://" << config.bindIp << ":" << config.bindPort;

    if (!config.bindIpv6Address.empty())
    {
        std::cout << " and http://[" << config.bindIpv6Address << "]:" << config.bindPort;
    }

    std::cout << "\n  hashing threads: " << config.threads << "\n  queue: " << config.maxQueue
              << " jobs, max difficulty " << config.maxDifficulty << "\n  limits: "
              << (config.rateLimitPerMinute == 0 ? std::string("unlimited")
                                                 : std::to_string(config.rateLimitPerMinute))
              << " requests/min per address, "
              << (config.maxJobsPerMinute == 0 ? std::string("unlimited") : std::to_string(config.maxJobsPerMinute))
              << " jobs/min overall\n  api key: " << (config.apiKey.empty() ? "not required" : "required")
              << "\n  statistics: GET /stats\n"
              << std::endl;

    std::atomic<bool> stopRequested(false);

    Tools::SignalHandler::install([&stopRequested] { stopRequested = true; });

    while (!stopRequested)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::cout << "Shutting down..." << std::endl;

    api.stop();
    service.stop();

    return 0;
}
