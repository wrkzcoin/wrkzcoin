// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "Benchmark.h"
#include "MinerManager.h"
#include "httplib.h"

#include <atomic>
#include <common/IpcSocket.h>
#include <common/SignalHandler.h>
#include <memory>
#include <system/Dispatcher.h>
#include <utilities/ColouredMsg.h>
#include <utilities/Utilities.h>

namespace
{
    /* Set for as long as a manager is running, so the signal handler - which
       is installed once and outlives any single relaunch - always asks the
       one that exists now to stop, and never a destroyed one. */
    std::atomic<Miner::MinerManager *> runningManager {nullptr};
}

int main(int argc, char **argv)
{
    /* A miner is normally left running with its output redirected to a log,
       and block buffering meant a redirected run said nothing at all until
       4KB of messages had piled up - including the warnings about a daemon it
       could not reach. It prints a handful of lines per block, so flushing
       each one costs nothing. */
    std::cout << std::unitbuf;

    CryptoNote::MiningConfig config;

    /* Parsed once, and inside a try: it validates the thread count, the
       timings and the daemon address, and an unhandled throw out here would
       abort the process rather than print what was wrong with the option. */
    try
    {
        config.parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cout << WarningMsg("Error: ") << WarningMsg(e.what()) << std::endl;
        return 1;
    }

    if (config.benchmarkSeconds != 0)
    {
        Miner::runBenchmark(config.benchmarkSeconds, config.threadCount);
        return 0;
    }

    Tools::SignalHandler::install([]() {
        Miner::MinerManager *manager = runningManager.load();

        if (manager != nullptr)
        {
            manager->requestShutdown();
        }
        else
        {
            std::exit(0);
        }
    });

    while (true)
    {
        try
        {
            System::Dispatcher dispatcher;

            /* An absolute path or @name means the daemon's local IPC socket
               rather than a host, so it is addressed by path with the client
               switched to AF_UNIX. */
            const bool useIpc = Utilities::isIpcDaemonAddress(config.daemonHost);

            auto httpClient = useIpc
                ? std::make_shared<httplib::Client>(Utilities::ipcDaemonPath(config.daemonHost), 80)
                : std::make_shared<httplib::Client>(config.daemonHost, config.daemonPort);

            if (useIpc)
            {
                Common::Ipc::configureClient(*httpClient);
            }

            const auto timeout = std::chrono::seconds(config.daemonTimeout);

            httpClient->set_connection_timeout(timeout);
            httpClient->set_read_timeout(timeout);
            httpClient->set_write_timeout(timeout);

            Miner::MinerManager app(dispatcher, config, httpClient);

            /* Declared after app so it is destroyed before it, which clears
               the pointer on the way out of an exception too - otherwise a
               signal arriving while the stack unwound would find a manager
               that had already been destroyed. */
            struct Registration
            {
                ~Registration()
                {
                    runningManager = nullptr;
                }
            } registration;

            runningManager = &app;

            app.start();

            /* start() returning is the miner finishing what it was asked to
               do - the --limit reached, or a shutdown requested. Relaunching
               is for the exception path below, not for a clean finish. */
            return 0;
        }
        catch (const std::exception &e)
        {
            std::cout << "Unhandled exception caught: " << e.what() << "\nAttempting to relaunch..." << std::endl;
        }
    }
}
