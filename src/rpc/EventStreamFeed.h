// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/BlockchainMessages.h>
#include <cryptonotecore/ICore.h>
#include <cryptonotecore/MessageQueue.h>
#include <cstdint>
#include <logging/ILogger.h>
#include <logging/LoggerRef.h>
#include <memory>
#include <rpc/EventStream.h>
#include <system/ContextGroup.h>
#include <system/Dispatcher.h>

/* Feeds the core's BlockchainMessage stream into an EventStream (GET /ws), as
   ZmqPublisher feeds the ZMQ socket: a consumer fiber on the dispatcher that
   only formats and hands over. The hub queues per subscriber and never blocks,
   so the node is never held up by a slow one.

   Construct, start and stop it on the dispatcher's thread. Started before the
   P2P loop so it hears the first block. */
class EventStreamFeed
{
  public:
    /* liteHeight is 0 for a full node; see ChainEvents::describe. */
    EventStreamFeed(
        System::Dispatcher &dispatcher,
        CryptoNote::ICore &core,
        std::shared_ptr<Logging::ILogger> logger,
        std::shared_ptr<EventStream> stream,
        uint32_t liteHeight = 0);

    ~EventStreamFeed();

    void start();

    void stop();

  private:
    using QueueGuard = CryptoNote::MesageQueueGuard<CryptoNote::ICore, CryptoNote::BlockchainMessage>;

    void consumeLoop();

    CryptoNote::ICore &m_core;

    Logging::LoggerRef m_logger;

    std::shared_ptr<EventStream> m_stream;

    uint32_t m_liteHeight;

    CryptoNote::MessageQueue<CryptoNote::BlockchainMessage> m_queue;

    std::unique_ptr<QueueGuard> m_queueGuard;

    System::ContextGroup m_contextGroup;

    bool m_running = false;
};
