// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ChainNotifier.h"

#include <sstream>
#include <system/InterruptedException.h>

using namespace Logging;

namespace Daemon
{
    ChainNotifier::ChainNotifier(
        System::Dispatcher &dispatcher,
        CryptoNote::ICore &core,
        const CryptoNote::ICryptoNoteProtocolQuery &protocol,
        std::shared_ptr<Logging::ILogger> logger,
        const std::string &blockNotify,
        const std::string &reorgNotify,
        const std::string &txNotify,
        bool notifyDuringSync):
        m_dispatcher(dispatcher),
        m_core(core),
        m_protocol(protocol),
        m_logger(logger, "ChainNotifier"),
        m_notifyDuringSync(notifyDuringSync),
        m_blockNotifier("block-notify", blockNotify, makeLogFn()),
        m_reorgNotifier("reorg-notify", reorgNotify, makeLogFn()),
        m_txNotifier("tx-notify", txNotify, makeLogFn()),
        m_queue(dispatcher),
        m_contextGroup(dispatcher),
        m_running(false),
        m_topIndex(0)
    {
    }

    ChainNotifier::~ChainNotifier()
    {
        stop();
    }

    Tools::Notifier::LogFn ChainNotifier::makeLogFn()
    {
        return [this](Tools::Notifier::LogLevel level, const std::string &message) {
            m_logger(level == Tools::Notifier::LogLevel::Warning ? Logging::WARNING : Logging::INFO) << message;
        };
    }

    bool ChainNotifier::anyEnabled() const
    {
        return m_blockNotifier.enabled() || m_reorgNotifier.enabled() || m_txNotifier.enabled();
    }

    bool ChainNotifier::start()
    {
        if (m_running)
        {
            return true;
        }

        if (!anyEnabled())
        {
            return false;
        }

        m_topIndex = m_core.getTopBlockIndex();

        m_queueGuard.reset(new QueueGuard(m_core, m_queue));
        m_running = true;
        m_contextGroup.spawn([this] { consumeLoop(); });

        m_logger(Logging::INFO) << "Chain notifier started"
                                << (m_notifyDuringSync ? " (notifying during sync)" : " (suppressed until synchronized)");
        return true;
    }

    void ChainNotifier::stop()
    {
        if (m_running)
        {
            m_running = false;
            m_queue.stop();
            m_contextGroup.interrupt();
            m_contextGroup.wait();
            m_queueGuard.reset();
        }

        /* Idempotent; also stops worker threads that were started by
           construction even if start() was never called. */
        m_blockNotifier.stop();
        m_reorgNotifier.stop();
        m_txNotifier.stop();
    }

    void ChainNotifier::consumeLoop()
    {
        while (true)
        {
            try
            {
                const CryptoNote::BlockchainMessage message = m_queue.front();
                m_queue.pop();
                handleMessage(message);
            }
            catch (const System::InterruptedException &)
            {
                break;
            }
            catch (const std::exception &e)
            {
                m_logger(Logging::WARNING) << "Chain notifier loop error: " << e.what();
            }
        }
    }

    bool ChainNotifier::shouldNotifyAt(uint32_t blockIndex) const
    {
        if (m_notifyDuringSync)
        {
            return true;
        }

        /* isSynchronized() is sticky once a peer sync completed. The observed
           height clause keeps isolated nodes (no peers, e.g. a solo testnet)
           from being muted forever: with nothing observed we are the tip. */
        return m_protocol.isSynchronized() || blockIndex + 1 >= m_protocol.getObservedHeight();
    }

    void ChainNotifier::notifyBlock(uint32_t blockIndex, const Crypto::Hash &hash)
    {
        if (!m_blockNotifier.enabled())
        {
            return;
        }

        const std::string hashStr = hashToString(hash);
        const std::string heightStr = std::to_string(blockIndex);

        Tools::Notifier::Notification n;
        n.event = "block";
        n.placeholders = {{'s', hashStr}, {'h', heightStr}};
        n.fields = {{"height", heightStr, false}, {"hash", hashStr, true}};
        m_blockNotifier.notify(std::move(n));
    }

    void ChainNotifier::handleMessage(const CryptoNote::BlockchainMessage &message)
    {
        message.match(
            [this](const CryptoNote::Messages::NewBlock &m) {
                m_topIndex = m.blockIndex;

                if (shouldNotifyAt(m.blockIndex))
                {
                    notifyBlock(m.blockIndex, m.blockHash);
                }
            },
            [](const CryptoNote::Messages::NewAlternativeBlock &) {
                /* Alternative blocks are not announced (same as Monero). */
            },
            [this](const CryptoNote::Messages::ChainSwitch &m) {
                /* blocksFromCommonRoot[0] is the common root itself; the rest
                   are the blocks of the new main chain. */
                const uint32_t commonRoot = m.commonRootIndex;
                const uint32_t newBlocks =
                    m.blocksFromCommonRoot.empty() ? 0 : static_cast<uint32_t>(m.blocksFromCommonRoot.size() - 1);
                const uint32_t newTop = commonRoot + newBlocks;
                const uint32_t discarded = m_topIndex > commonRoot ? m_topIndex - commonRoot : 0;

                m_topIndex = newTop;

                if (!shouldNotifyAt(newTop))
                {
                    return;
                }

                if (m_reorgNotifier.enabled())
                {
                    /* Monero semantics: %s split height (first replaced
                       height), %h new top height, %n new blocks, %d discarded. */
                    const std::string splitStr = std::to_string(commonRoot + 1);
                    const std::string newTopStr = std::to_string(newTop);
                    const std::string newBlocksStr = std::to_string(newBlocks);
                    const std::string discardedStr = std::to_string(discarded);

                    Tools::Notifier::Notification n;
                    n.event = "reorg";
                    n.placeholders = {{'s', splitStr}, {'h', newTopStr}, {'n', newBlocksStr}, {'d', discardedStr}};
                    n.fields = {
                        {"split_height", splitStr, false},
                        {"new_height", newTopStr, false},
                        {"new_blocks", newBlocksStr, false},
                        {"discarded_blocks", discardedStr, false}};
                    m_reorgNotifier.notify(std::move(n));
                }

                /* Re-announce the blocks of the new main chain so block-notify
                   subscribers see the replacement blocks, as monerod does. */
                for (size_t i = 1; i < m.blocksFromCommonRoot.size(); ++i)
                {
                    notifyBlock(commonRoot + static_cast<uint32_t>(i), m.blocksFromCommonRoot[i]);
                }
            },
            [this](const CryptoNote::Messages::AddTransaction &m) {
                if (!m_txNotifier.enabled() || !shouldNotifyAt(m_topIndex))
                {
                    return;
                }

                for (const auto &hash : m.hashes)
                {
                    const std::string hashStr = hashToString(hash);

                    Tools::Notifier::Notification n;
                    n.event = "tx";
                    n.placeholders = {{'s', hashStr}};
                    n.fields = {{"hash", hashStr, true}};
                    m_txNotifier.notify(std::move(n));
                }
            },
            [](const CryptoNote::Messages::DeleteTransaction &) {
                /* Pool removals are not announced. */
            });
    }

    std::string ChainNotifier::hashToString(const Crypto::Hash &hash)
    {
        std::ostringstream out;
        out << hash;
        return out.str();
    }
} // namespace Daemon
