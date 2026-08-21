// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <functional>
#include <thread>

template<typename T> class Event
{
  public:
    void subscribe(std::function<void(T)> function)
    {
        m_function = function;
    }

    void unsubscribe()
    {
        m_function = {};
    }

    void pause()
    {
        m_paused = true;
    }

    void resume()
    {
        m_paused = false;
    }

    void fire(T args)
    {
        /* If we have a function to run, and we're not ignoring events */
        if (m_function && !m_paused)
        {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            /* WASM no-pthread mode: std::thread construction fails with EAGAIN.
               Call the handler inline instead. In pthread builds the normal
               detach path below is used. */
            m_function(args);
#else
            /* Launch the function, and return instantly. This way we
               can have multiple functions running at once.

               If you use std::cout in your function, you may experience
               interleaved characters when printing. To resolve this,
               consider using std::osyncstream.
               Further reading: https://stackoverflow.com/q/14718124/8737306 */
            std::thread(m_function, args).detach();
#endif
        }
    }

  private:
    std::function<void(T)> m_function;

    bool m_paused = false;
};

class EventHandler
{
  public:
    Event<uint64_t> onSynced;

    Event<WalletTypes::Transaction> onTransaction;
};
