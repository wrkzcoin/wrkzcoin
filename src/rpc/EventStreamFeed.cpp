// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include <rpc/EventStreamFeed.h>

#include <rpc/ChainEvents.h>
#include <system/InterruptedException.h>

EventStreamFeed::EventStreamFeed(
    System::Dispatcher &dispatcher,
    CryptoNote::ICore &core,
    std::shared_ptr<Logging::ILogger> logger,
    std::shared_ptr<EventStream> stream,
    uint32_t liteHeight):
    m_core(core),
    m_logger(logger, "EventStream"),
    m_stream(std::move(stream)),
    m_liteHeight(liteHeight),
    m_queue(dispatcher),
    m_contextGroup(dispatcher)
{
}

EventStreamFeed::~EventStreamFeed()
{
    stop();
}

void EventStreamFeed::start()
{
    if (m_running || !m_stream)
    {
        return;
    }

    m_queueGuard.reset(new QueueGuard(m_core, m_queue));
    m_running = true;
    m_contextGroup.spawn([this] { consumeLoop(); });
}

void EventStreamFeed::stop()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;
    m_queue.stop();
    m_contextGroup.interrupt();
    m_contextGroup.wait();
    m_queueGuard.reset();

    m_logger(Logging::INFO) << "WebSocket event stream stopped. Published=" << m_stream->published()
                            << ", subscribers disconnected for falling behind=" << m_stream->disconnectedSlow();
}

void EventStreamFeed::consumeLoop()
{
    while (true)
    {
        try
        {
            const CryptoNote::BlockchainMessage message = m_queue.front();
            m_queue.pop();

            /* The one message that costs a read to describe - a block's
               transaction hashes - is not worth building for nobody. */
            if (m_stream->clients() == 0)
            {
                continue;
            }

            for (const auto &[topic, body] : ChainEvents::describe(message, m_core, m_liteHeight, m_logger))
            {
                m_stream->publish(topic, body);
            }
        }
        catch (const System::InterruptedException &)
        {
            break;
        }
        catch (const std::exception &e)
        {
            m_logger(Logging::WARNING) << "WebSocket event stream loop error: " << e.what();
        }
    }
}
