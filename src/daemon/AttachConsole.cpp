// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "AttachConsole.h"

#include "httplib.h"
#include "json.hpp"

#include <common/ConsoleHandler.h>
#include <common/IpcSocket.h>
#include <iostream>
#include <string>
#include <utilities/ColouredMsg.h>
#include <utilities/Utilities.h>

namespace Daemon
{
    namespace
    {
        constexpr time_t CONNECT_TIMEOUT_SECONDS = 2;

        /* A command holds its connection until the daemon answers, and
           `compact_db wait` can legitimately take hours. Ctrl+C is the way out
           of one that never will. */
        constexpr time_t COMMAND_TIMEOUT_SECONDS = 24 * 60 * 60;

        /* Runs one command line in the daemon. On failure, error says why in a
           form worth showing. */
        bool runInDaemon(
            httplib::Client &client,
            const std::string &commandLine,
            std::string &output,
            std::string &error)
        {
            nlohmann::json request;
            request["command"] = commandLine;

            const auto result = client.Post("/console", request.dump(), "application/json");

            if (!result)
            {
                error = httplib::to_string(result.error());
                return false;
            }

            if (result->status != 200)
            {
                /* The daemon explains a refusal in the body: {"error": ...}
                   from its own checks, or a bare message if it never got that
                   far. A 404 is an older daemon that has no console route. */
                std::string reason = result->body;

                try
                {
                    const auto parsed = nlohmann::json::parse(result->body);

                    if (parsed.contains("error") && parsed["error"].is_string())
                    {
                        reason = parsed["error"].get<std::string>();
                    }
                }
                catch (const std::exception &)
                {
                }

                if (result->status == 404)
                {
                    reason = "this daemon does not serve console commands; it needs upgrading";
                }

                error = "HTTP " + std::to_string(result->status) + ": " + reason;
                return false;
            }

            try
            {
                output = nlohmann::json::parse(result->body).at("output").get<std::string>();
            }
            catch (const std::exception &e)
            {
                error = std::string("unreadable reply: ") + e.what();
                return false;
            }

            return true;
        }
    } // namespace

    int runAttachConsole(const std::string &endpoint)
    {
        if (!Common::Ipc::supported())
        {
            std::cout << WarningMsg("Cannot attach: " + Common::Ipc::unsupportedReason() + ".") << std::endl;
            return 1;
        }

        /* Console commands are served on the IPC socket only - they change
           log levels, ban peers and stop the node, and the mode on the socket
           file is what says who may do that. A host:port is refused here
           rather than failing later with a 404 that looks like a version
           mismatch. */
        if (!Utilities::isIpcDaemonAddress(endpoint))
        {
            std::cout << WarningMsg(
                             "attach takes the daemon's RPC IPC socket: an absolute path, an @name or ipc://path "
                             "(the daemon's --rpc-ipc-path). Console commands are not served over TCP.")
                      << std::endl;
            return 1;
        }

        const std::string path = Utilities::ipcDaemonPath(endpoint);

        httplib::Client client(path, 80);
        Common::Ipc::configureClient(client);
        client.set_connection_timeout(CONNECT_TIMEOUT_SECONDS, 0);
        client.set_read_timeout(COMMAND_TIMEOUT_SECONDS, 0);
        client.set_write_timeout(CONNECT_TIMEOUT_SECONDS, 0);

        /* help doubles as the connection test: it shows the socket answers,
           that what answered is a daemon with a console, and what that console
           takes. */
        std::string output;
        std::string error;

        if (!runInDaemon(client, "help", output, error))
        {
            std::cout << WarningMsg("Could not attach to " + Common::Ipc::describe(path) + ": " + error) << std::endl;
            return 1;
        }

        std::cout << InformationMsg("Attached to " + Common::Ipc::describe(path)) << std::endl;
        std::cout << output;
        std::cout << InformationMsg("exit or quit leaves this console. stop shuts the daemon down.") << std::endl;

        std::string line;

        /* Ends on exit, on Ctrl+C or Ctrl+D, and when a script piped in runs
           out of lines. */
        while (Common::readConsoleLine("> ", line))
        {
            const auto tokens = Common::ConsoleHandler::splitCommandLine(line);

            if (tokens.empty())
            {
                continue;
            }

            const std::string &command = tokens.front();

            if (command == "exit" || command == "quit")
            {
                break;
            }

            if (!runInDaemon(client, line, output, error))
            {
                std::cout << WarningMsg("Command failed: " + error) << std::endl;
                continue;
            }

            std::cout << output;

            if (!output.empty() && output.back() != '\n')
            {
                std::cout << std::endl;
            }

            if (command == "stop")
            {
                std::cout << InformationMsg("The daemon is shutting down; leaving the console.") << std::endl;
                break;
            }
        }

        return 0;
    }
} // namespace Daemon
