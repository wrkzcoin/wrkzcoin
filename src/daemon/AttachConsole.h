// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

namespace Daemon
{
    /* An interactive console for a daemon that is already running, reached
       over its RPC IPC socket: every line typed here runs inside that daemon
       through its own command handler, and what it prints comes back. This is
       the way in when the daemon runs under a process manager and has no
       terminal of its own. Returns the process exit code. */
    int runAttachConsole(const std::string &endpoint);
} // namespace Daemon
