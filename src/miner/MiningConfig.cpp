// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MiningConfig.h"

#include "common/IpcSocket.h"
#include "common/StringTools.h"
#include "logging/ILogger.h"
#include "version.h"

#include <algorithm>
#include <common/Util.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cstdlib>
#include <cxxopts.hpp>
#include <errors/ValidateParameters.h>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <utilities/ColouredMsg.h>
#include <utilities/String.h>
#include <utilities/Utilities.h>
#include <vector>

namespace CryptoNote
{
    namespace
    {
        /* hardware_concurrency() is allowed to answer 0 when it cannot work the
           count out. Taken literally that made the --threads default "0", which
           the validator below then rejected with "must be 1..0". */
        const size_t CONCURRENCY_LEVEL = std::max<size_t>(1, std::thread::hardware_concurrency());
    }

    MiningConfig::MiningConfig(): help(false), version(false) {}

    void MiningConfig::parse(int argc, char **argv)
    {
        cxxopts::Options options(argv[0], getProjectCLIHeader());

        options.add_options("Core")(
            "help",
            "Display this help message",
            cxxopts::value<bool>(help)->default_value("false")->implicit_value("true"))(
            "version",
            "Output software version information",
            cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "daemon-address",
            "The daemon [host:port] combination to use for node operations. This option overrides --daemon-host and "
            "--daemon-rpc-port. An absolute path, an @name or an ipc://path connects over the daemon's local IPC "
            "socket instead (--rpc-ipc-path on the daemon)",
            cxxopts::value<std::string>(daemonAddress),
            "<host:port>")(
            "daemon-host",
            "The daemon host to use for node operations",
            cxxopts::value<std::string>(daemonHost)->default_value("127.0.0.1"),
            "<host>")(
            "daemon-rpc-port",
            "The daemon RPC port to use for node operations",
            cxxopts::value<uint16_t>(daemonPort)->default_value(std::to_string(CryptoNote::RPC_DEFAULT_PORT)),
            "#")(
            "scan-time",
            "Blockchain polling interval (seconds). How often miner will check the Blockchain for updates",
            cxxopts::value<size_t>(scanPeriod)->default_value("1"),
            "#")(
            "daemon-timeout",
            "How long to wait on a daemon request (seconds) before giving up on it",
            cxxopts::value<size_t>(daemonTimeout)->default_value("10"),
            "#")(
            "retry-interval",
            "How long to wait (seconds) before asking again after a failed daemon request",
            cxxopts::value<size_t>(retryInterval)->default_value("1"),
            "#");

        options.add_options("Mining")(
            "address", "The valid CryptoNote miner's address", cxxopts::value<std::string>(miningAddress), "<address>")(
            "benchmark",
            "Hash for this many seconds and report the rate, then exit. Needs no daemon and no address, and measures "
            "the proof of work alone. 0 means mine normally",
            cxxopts::value<size_t>(benchmarkSeconds)->default_value("0"),
            "#")(
            "hash-rate-interval",
            "How often to report the hash rate (seconds). 0 turns the report off",
            cxxopts::value<size_t>(hashRateInterval)->default_value("60"),
            "#")(
            "block-timestamp-interval",
            "Timestamp incremental step for each subsequent block. May be set only if --first-block-timestamp has been "
            "set.",
            cxxopts::value<int64_t>(blockTimestampInterval)->default_value("0"),
            "#")(
            "first-block-timestamp",
            "Set timestamp to the first mined block. 0 means leave timestamp unchanged",
            cxxopts::value<uint64_t>(firstBlockTimestamp)->default_value("0"),
            "#")(
            "limit",
            "Mine this exact quantity of blocks and then stop. 0 means no limit",
            cxxopts::value<size_t>(blocksLimit)->default_value("0"),
            "#")(
            "threads",
            "The mining threads count. Going above what the hardware reports is allowed but warned about - the best "
            "count for CryptoNight is usually set by cache, not by cores.",
            cxxopts::value<size_t>(threadCount)->default_value(std::to_string(CONCURRENCY_LEVEL)),
            "#");

        try
        {
            auto result = options.parse(argc, argv);
        }
        catch (const cxxopts::exceptions::exception &e)
        {
            std::cout << WarningMsg("Error: Unable to parse command line argument options: ") << WarningMsg(e.what())
                      << "\n\n";
            std::cout << options.help({}) << std::endl;
            exit(1);
        }

        if (help) // Do we want to display the help message?
        {
            std::cout << options.help({}) << std::endl;
            exit(0);
        }
        else if (version) // Do we want to display the software version?
        {
            std::cout << InformationMsg(getProjectCLIHeader()) << std::endl;
            exit(0);
        }

        if (threadCount == 0)
        {
            throw std::runtime_error("--threads must be at least 1");
        }

        /* Not an error: the best thread count for CryptoNight is set by how
           many scratchpads fit in cache, not by the core count, so both
           over- and under-subscribing are legitimate things to try. */
        if (threadCount > CONCURRENCY_LEVEL)
        {
            std::cout << WarningMsg("--threads is ") << WarningMsg(threadCount)
                      << WarningMsg(", above the ") << WarningMsg(CONCURRENCY_LEVEL)
                      << WarningMsg(" this machine reports. Expect them to fight over the CPU.\n");
        }

        if (scanPeriod == 0)
        {
            throw std::runtime_error("--scan-time must not be zero");
        }

        if (daemonTimeout == 0)
        {
            throw std::runtime_error("--daemon-timeout must not be zero");
        }

        if (retryInterval == 0)
        {
            throw std::runtime_error("--retry-interval must not be zero");
        }

        if (firstBlockTimestamp == 0 && blockTimestampInterval != 0)
        {
            throw std::runtime_error(
                "If you specify --block-timestamp-interval you must also specify --first-block-timestamp");
        }

        /* A benchmark talks to nothing and mines to nobody, so everything
           below - the daemon address and the mining address it would
           otherwise stop and ask for - is not its business. */
        if (benchmarkSeconds != 0)
        {
            return;
        }

        /* Checked before the interactive address prompt below, so a build that
           cannot do IPC says so straight away instead of asking for a mining
           address first. Reported here rather than thrown, because main()
           parses outside its try block and would exit without a word. */
        if (Utilities::isIpcDaemonAddress(daemonAddress) && !Common::Ipc::supported())
        {
            std::cout << WarningMsg("--daemon-address names an IPC socket, but ")
                      << WarningMsg(Common::Ipc::unsupportedReason()) << WarningMsg(".") << std::endl;
            exit(1);
        }

        if (!daemonAddress.empty())
        {
            if (!Utilities::parseDaemonAddressFromString(daemonHost, daemonPort, daemonAddress))
            {
                throw std::runtime_error("Could not parse --daemon-address option");
            }
        }

        const bool integratedAddressesAllowed = false;

        Error error = validateAddresses({miningAddress}, integratedAddressesAllowed);

        while (error != SUCCESS)
        {
            /* If they didn't enter an address, don't report an error. Probably just
             unaware of cli options. */
            if (!miningAddress.empty())
            {
                std::cout << WarningMsg("Address is not valid: ") << WarningMsg(error) << std::endl;
            }

            std::cout << InformationMsg("What address do you want to mine to?: ");
            std::getline(std::cin, miningAddress);
            Utilities::trim(miningAddress);

            error = validateAddresses({miningAddress}, integratedAddressesAllowed);
        }
    }

} // namespace CryptoNote
