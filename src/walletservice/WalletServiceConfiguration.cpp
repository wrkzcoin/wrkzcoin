// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "WalletServiceConfiguration.h"

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cstdlib>
#include <cxxopts.hpp>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <logging/ILogger.h>
#include <regex>
#include <string>
#include "json.hpp"

namespace PaymentService
{
    void handleSettings(int argc, char *argv[], WalletServiceConfiguration &config)
    {
        cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

        options.add_options("Core")(
            "h,help", "Display this help message", cxxopts::value<bool>()->implicit_value("true"))(
            "v,version",
            "Output software version information",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "daemon-address",
            "The daemon host to use for node operations",
            cxxopts::value<std::string>()->default_value(config.daemonAddress),
            "<ip>")(
            "daemon-port",
            "The daemon RPC port to use for node operations",
            cxxopts::value<int>()->default_value(std::to_string(config.daemonPort)),
            "<port>");

        options.add_options("Service")(
            "c,config",
            "Specify the configuration <file> to use instead of CLI arguments",
            cxxopts::value<std::string>(),
            "<file>")(
            "dump-config",
            "Prints the current configuration to the screen",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "l,log-file",
            "Specify log <file> location",
            cxxopts::value<std::string>()->default_value(config.logFile),
            "<file>")(
            "log-level",
            "Specify log level",
            cxxopts::value<int>()->default_value(std::to_string(config.logLevel)),
            "#")(
            "server-root",
            "The service will use this <path> as the working directory",
            cxxopts::value<std::string>(),
            "<path>")(
            "save-config", "Save the configuration to the specified <file>", cxxopts::value<std::string>(), "<file>")(
            "init-timeout",
            "Amount of time in seconds to wait for initial connection",
            cxxopts::value<int>()->default_value("10"),
            "<seconds>");

        options.add_options("Wallet")(
            "address",
            "Print the wallet addresses and then exit",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "w,container-file", "Wallet container <file>", cxxopts::value<std::string>(), "<file>")(
            "p,container-password", "Wallet container <password>", cxxopts::value<std::string>(), "<password>")(
            "g,generate-container",
            "Generate a new wallet container",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "view-key",
            "Generate a wallet container with this secret view <key>",
            cxxopts::value<std::string>(),
            "<key>")(
            "spend-key",
            "Generate a wallet container with this secret spend <key>",
            cxxopts::value<std::string>(),
            "<key>")(
            "mnemonic-seed",
            "Generate a wallet container with this Mnemonic <seed>",
            cxxopts::value<std::string>(),
            "<seed>")(
            "scan-height",
            "Start scanning for transactions from this Blockchain height",
            cxxopts::value<uint64_t>()->default_value("0"),
            "#")(
            "SYNC_FROM_ZERO",
            "Force the wallet to sync from 0",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Network")(
            "bind-address",
            "Interface IP address for the RPC service",
            cxxopts::value<std::string>()->default_value(config.bindAddress),
            "<ip>")(
            "bind-port",
            "TCP port for the RPC service",
            cxxopts::value<int>()->default_value(std::to_string(config.bindPort)),
            "<port>");

        options.add_options("Notifications")(
            "tx-notify",
            "Run a command or POST to an http(s):// URL for every new wallet transaction. "
            "Placeholders: %s hash, %h height (0 = unconfirmed), %a amount, %f fee, %p payment id, %c confirmed 0/1",
            cxxopts::value<std::string>()->default_value(config.txNotify),
            "<cmd|url>")(
            "tx-confirmed-notify",
            "Same as tx-notify but fired once when a transaction gets into a block",
            cxxopts::value<std::string>()->default_value(config.txConfirmedNotify),
            "<cmd|url>")(
            "notify-during-sync",
            "Also fire tx notifications while the wallet is far behind the daemon (default: suppressed)",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("RPC")(
            "enable-cors",
            "Adds header 'Access-Control-Allow-Origin' to the RPC responses. Uses the value specified as the domain. "
            "Use * for all.",
            cxxopts::value<std::string>(),
            "<domain>")(
            "rpc-legacy-security",
            "Enable legacy mode (no password for RPC). WARNING: INSECURE. USE ONLY AS A LAST RESORT.",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "rpc-password",
            "Specify the <password> to access the RPC server.",
            cxxopts::value<std::string>(),
            "<password>");

#ifdef WIN32
        options.add_options("Windows Only")(
            "daemonize",
            "Run the service as a daemon",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "register-service",
            "Registers this program as a Windows service",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "unregister-service",
            "Unregisters this program from being a Windows service",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));
#endif

        try
        {
            auto cli = options.parse(argc, argv);

            if (cli.count("help") > 0)
            {
                config.help = cli["help"].as<bool>();
            }

            if (cli.count("version") > 0)
            {
                config.version = cli["version"].as<bool>();
            }

            if (cli.count("config") > 0)
            {
                config.configFile = cli["config"].as<std::string>();
            }

            if (cli.count("save-config") > 0)
            {
                config.outputFile = cli["save-config"].as<std::string>();
            }

            if (cli.count("dump-config") > 0)
            {
                config.dumpConfig = cli["dump-config"].as<bool>();
            }

            if (cli.count("daemon-address") > 0)
            {
                config.daemonAddress = cli["daemon-address"].as<std::string>();
            }

            if (cli.count("daemon-port") > 0)
            {
                config.daemonPort = cli["daemon-port"].as<int>();
            }

            if (cli.count("init-timeout") > 0)
            {
                config.initTimeout = cli["init-timeout"].as<int>();
            }

            if (cli.count("log-file") > 0)
            {
                config.logFile = cli["log-file"].as<std::string>();
            }

            if (cli.count("log-level") > 0)
            {
                config.logLevel = cli["log-level"].as<int>();
            }

            if (cli.count("container-file") > 0)
            {
                config.containerFile = cli["container-file"].as<std::string>();
            }

            if (cli.count("container-password") > 0)
            {
                config.containerPassword = cli["container-password"].as<std::string>();
            }

            if (cli.count("bind-address") > 0)
            {
                config.bindAddress = cli["bind-address"].as<std::string>();
            }

            if (cli.count("tx-notify") > 0)
            {
                config.txNotify = cli["tx-notify"].as<std::string>();
            }

            if (cli.count("tx-confirmed-notify") > 0)
            {
                config.txConfirmedNotify = cli["tx-confirmed-notify"].as<std::string>();
            }

            if (cli.count("notify-during-sync") > 0)
            {
                config.notifyDuringSync = cli["notify-during-sync"].as<bool>();
            }

            if (cli.count("bind-port") > 0)
            {
                config.bindPort = cli["bind-port"].as<int>();
            }

            if (cli.count("enable-cors") > 0)
            {
                config.corsHeader = cli["enable-cors"].as<std::string>();
            }

            if (cli.count("rpc-legacy-security") > 0)
            {
                config.legacySecurity = cli["rpc-legacy-security"].as<bool>();
            }

            if (cli.count("rpc-password") > 0)
            {
                config.rpcPassword = cli["rpc-password"].as<std::string>();
            }

            if (cli.count("server-root") > 0)
            {
                config.serverRoot = cli["server-root"].as<std::string>();
            }

            if (cli.count("view-key") > 0)
            {
                config.secretViewKey = cli["view-key"].as<std::string>();
            }

            if (cli.count("spend-key") > 0)
            {
                config.secretSpendKey = cli["spend-key"].as<std::string>();
            }

            if (cli.count("mnemonic-seed") > 0)
            {
                config.mnemonicSeed = cli["mnemonic-seed"].as<std::string>();
            }

            if (cli.count("generate-container") > 0)
            {
                config.generateNewContainer = cli["generate-container"].as<bool>();
            }

            if (cli.count("daemonize") > 0)
            {
                config.daemonize = cli["daemonize"].as<bool>();
            }

            if (cli.count("register-service") > 0)
            {
                config.registerService = cli["register-service"].as<bool>();
            }

            if (cli.count("unregister-service") > 0)
            {
                config.unregisterService = cli["unregister-service"].as<bool>();
            }

            if (cli.count("address") > 0)
            {
                config.printAddresses = cli["address"].as<bool>();
            }

            if (cli.count("SYNC_FROM_ZERO") > 0)
            {
                config.syncFromZero = cli["SYNC_FROM_ZERO"].as<bool>();
            }

            if (cli.count("scan-height") > 0)
            {
                config.scanHeight = cli["scan-height"].as<uint64_t>();
            }

            if (config.help) // Do we want to display the help message?
            {
                std::cout << options.help({}) << std::endl;
                exit(0);
            }
            else if (config.version) // Do we want to display the software version?
            {
                std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
                exit(0);
            }
        }
        catch (const cxxopts::exceptions::exception &e)
        {
            std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl
                      << std::endl
                      << options.help({}) << std::endl;
            exit(1);
        }
    }

    bool updateConfigFormat(const std::string configFile, WalletServiceConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }
        // find key=value pair, respect whitespace before/after "="
        // g0: full match, g1: match key, g2: match value
        static const std::regex cfgItem {R"x(\s*(\S[^ \t=]*)\s*=\s*((\s?\S+)+)\s*$)x"};

        // comments, first non space starts with # or ;
        static const std::regex cfgComment {R"x(\s*[;#])x"};
        std::smatch item;
        std::string cfgKey;
        std::string cfgValue;
        bool updated = false;

        for (std::string line; std::getline(data, line);)
        {
            if (line.empty() || std::regex_match(line, item, cfgComment))
            {
                continue;
            }

            if (std::regex_match(line, item, cfgItem))
            {
                if (item.size() != 4)
                {
                    continue;
                }

                cfgKey = item[1].str();
                cfgValue = item[2].str();

                if (cfgKey.compare("daemon-address") == 0)
                {
                    config.daemonAddress = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("daemon-port") == 0)
                {
                    try
                    {
                        config.daemonPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("init-timeout") == 0)
                {
                    try
                    {
                        config.initTimeout = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("log-file") == 0)
                {
                    config.logFile = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("log-level") == 0)
                {
                    try
                    {
                        config.logLevel = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("container-file") == 0)
                {
                    config.containerFile = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("container-password") == 0)
                {
                    config.containerPassword = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("bind-address") == 0)
                {
                    config.bindAddress = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("tx-notify") == 0)
                {
                    config.txNotify = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("tx-confirmed-notify") == 0)
                {
                    config.txConfirmedNotify = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("notify-during-sync") == 0)
                {
                    config.notifyDuringSync = cfgValue.at(0) == '1';
                    updated = true;
                }
                else if (cfgKey.compare("bind-port") == 0)
                {
                    try
                    {
                        config.bindPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("enable-cors") == 0)
                {
                    config.corsHeader = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-legacy-security") == 0)
                {
                    config.legacySecurity = cfgValue.at(0) == '1' ? true : false;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-password") == 0)
                {
                    config.rpcPassword = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("server-root") == 0)
                {
                    config.serverRoot = cfgValue;
                    updated = true;
                }
                else
                {
                    for (auto c : cfgKey)
                    {
                        if (static_cast<unsigned char>(c) > 127)
                        {
                            throw std::runtime_error(std::string("Bad/invalid config file"));
                        }
                    }
                    throw std::runtime_error("Unknown option: " + cfgKey);
                }
            }
        }

        if (!updated)
        {
            return false;
        }

        try
        {
            std::ifstream orig(configFile, std::ios::binary);
            std::ofstream backup(configFile + ".ini.bak", std::ios::binary);
            backup << orig.rdbuf();
        }
        catch (std::exception &e)
        {
            // pass
        }
        return updated;
    }

    void handleSettings(const std::string configFile, WalletServiceConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(data);
        }
        catch (const nlohmann::json::parse_error &)
        {
            throw std::runtime_error("Failed to parse the config file as JSON.");
        }

        if (j.contains("daemon-address"))
        {
            config.daemonAddress = j["daemon-address"].get<std::string>();
        }

        if (j.contains("daemon-port"))
        {
            config.daemonPort = j["daemon-port"].get<int>();
        }

        if (j.contains("init-timeout"))
        {
            config.initTimeout = j["init-timeout"].get<int>();
        }

        if (j.contains("log-file"))
        {
            config.logFile = j["log-file"].get<std::string>();
        }

        if (j.contains("log-level"))
        {
            config.logLevel = j["log-level"].get<int>();
        }

        if (j.contains("container-file"))
        {
            config.containerFile = j["container-file"].get<std::string>();
        }

        if (j.contains("container-password"))
        {
            config.containerPassword = j["container-password"].get<std::string>();
        }

        if (j.contains("bind-address"))
        {
            config.bindAddress = j["bind-address"].get<std::string>();
        }

        if (j.contains("tx-notify"))
        {
            config.txNotify = j["tx-notify"].get<std::string>();
        }

        if (j.contains("tx-confirmed-notify"))
        {
            config.txConfirmedNotify = j["tx-confirmed-notify"].get<std::string>();
        }

        if (j.contains("notify-during-sync"))
        {
            config.notifyDuringSync = j["notify-during-sync"].get<bool>();
        }

        if (j.contains("bind-port"))
        {
            config.bindPort = j["bind-port"].get<int>();
        }

        if (j.contains("enable-cors"))
        {
            config.corsHeader = j["enable-cors"].get<std::string>();
        }

        if (j.contains("rpc-legacy-security"))
        {
            config.legacySecurity = j["rpc-legacy-security"].get<bool>();
        }

        if (j.contains("rpc-password"))
        {
            config.rpcPassword = j["rpc-password"].get<std::string>();
        }

        if (j.contains("server-root"))
        {
            config.serverRoot = j["server-root"].get<std::string>();
        }
    }

    nlohmann::json asJSON(const WalletServiceConfiguration &config)
    {
        nlohmann::json j;

        j["daemon-address"] = config.daemonAddress;
        j["daemon-port"] = config.daemonPort;
        j["log-file"] = config.logFile;
        j["log-level"] = config.logLevel;
        j["init-timeout"] = config.initTimeout;
        j["container-file"] = config.containerFile;
        j["container-password"] = config.containerPassword;
        j["bind-address"] = config.bindAddress;
        j["bind-port"] = config.bindPort;
        j["tx-notify"] = config.txNotify;
        j["tx-confirmed-notify"] = config.txConfirmedNotify;
        j["notify-during-sync"] = config.notifyDuringSync;
        j["enable-cors"] = config.corsHeader;
        j["rpc-legacy-security"] = config.legacySecurity;
        j["rpc-password"] = config.rpcPassword;
        j["server-root"] = config.serverRoot;

        return j;
    }

    std::string asString(const WalletServiceConfiguration &config)
    {
        return asJSON(config).dump(2);
    }

    void asFile(const WalletServiceConfiguration &config, const std::string &filename)
    {
        nlohmann::json j = asJSON(config);
        std::ofstream data(filename);
        data << std::setw(2) << j;
    }
} // namespace PaymentService
