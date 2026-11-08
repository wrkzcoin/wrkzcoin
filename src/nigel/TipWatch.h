// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/* Defined in TipWatch.cpp; shared between a TipWatch and its thread. */
struct TipWatchState;

/* Follows a daemon's event stream (GET /ws, served when the daemon runs with
   --enable-websocket) so a wallet hears about a new block the moment its
   daemon does, instead of on its next poll.

   The stream only ever wakes the wallet up. Balances and history still come
   from the ordinary HTTP sync, so a message lost on the way costs no more
   than one poll interval, and a daemon that does not serve /ws - every
   release before the flag existed answers the upgrade with a 404 - leaves
   the wallet polling exactly as it always has.

   One background thread per watch, started on the first follow(). That
   thread is the only one that ever touches the socket: httplib's WebSocket
   reads and closes through the same unsynchronised buffer, so a close from
   another thread while a read is blocked would race it. Stopping therefore
   never interrupts a read; the thread notices on its next frame (the daemon
   sends a heartbeat every 30 s) or when its read times out, and everything
   it touches lives in shared state it holds a reference to, so it may
   outlive the TipWatch that started it. */
class TipWatch
{
  public:
    TipWatch();

    /* Stops following. Never calls the onBlock callback again once this
       returns, and never waits on a blocked read. */
    ~TipWatch();

    TipWatch(const TipWatch &) = delete;

    TipWatch &operator=(const TipWatch &) = delete;

    /* Starts following this daemon, replacing whatever was followed before.
       An IPC address is not followed (there is no /ws on the local socket),
       nor an https daemon where this build has no TLS for the stream. */
    void follow(const std::string &host, const uint16_t port, const bool ssl);

    /* Stops following. The connection, if any, is dropped once its current
       read returns; from now on nothing it receives counts. */
    void unfollow();

    /* Whether a frame (heartbeats included) has arrived from the followed
       daemon within the last 75 seconds. */
    bool isLive() const;

    /* Messages that should wake a sync: new blocks, reorgs, pool changes, and
       the greeting that starts every connection. Only ever increases; compare
       against a value read earlier to see whether anything happened since. */
    uint64_t events() const;

    /* The subset of events() that mean the chain tip moved. */
    uint64_t blockEvents() const;

    /* Called from the watch thread with the index of the daemon's top block,
       whenever a message names one. Set it before follow(). */
    void setOnBlock(std::function<void(uint64_t)> onBlock);

  private:
    std::shared_ptr<TipWatchState> m_state;
};
