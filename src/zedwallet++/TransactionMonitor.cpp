// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////////////
#include <zedwallet++/TransactionMonitor.h>
///////////////////////////////////////////

#include <iostream>
#include <utilities/ColouredMsg.h>
#include <zedwallet++/CommandImplementations.h>
#include <zedwallet++/GetInput.h>
#include <chrono>
#include <thread>

void TransactionMonitor::start()
{
    /* Grab new transactions and push them into a queue for processing */
    m_walletBackend->m_eventHandler->onTransaction.subscribe([this](const auto tx) { m_queuedTransactions.push(tx); });

    auto lastSummary = std::chrono::steady_clock::now();
    uint64_t lastWalletHeight = 0;
    uint64_t lastNetworkHeight = 0;

    while (!m_shouldStop)
    {
        /* Make sure we're not printing a garbage tx */
        if (m_shouldStop)
        {
            break;
        }

        if (m_queuedTransactions.size() > 0)
        {
            const auto tx = m_queuedTransactions.front();

            /* Don't print out fusion or outgoing transactions */
            if (!tx.isFusionTransaction() && tx.totalAmount() > 0)
            {
                /* Aquire the lock, so we're not interleaving our output when a
                   command is being handled, for example, transferring */
                std::scoped_lock lock(*m_mutex);

                printTransferOneLine(tx);

                /* Write out the prompt after every transfer. This prevents the
                   wallet being in a 'ready' state, waiting for input, but looking
                   like it's not. */
                std::cout << InformationMsg(getPrompt(m_walletBackend)) << std::flush;
            }

            m_queuedTransactions.deleteFront();
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastSummary >= std::chrono::seconds(10))
        {
            const auto [walletBlockCount, localDaemonBlockCount, networkBlockCount] = m_walletBackend->getSyncStatus();

            if (walletBlockCount != lastWalletHeight || networkBlockCount != lastNetworkHeight)
            {
                std::scoped_lock lock(*m_mutex);
                std::cout << InformationMsg("[sync] wallet ")
                          << SuccessMsg(walletBlockCount) << InformationMsg(" / local ")
                          << SuccessMsg(localDaemonBlockCount) << InformationMsg(" / network ")
                          << SuccessMsg(networkBlockCount) << std::endl;
                std::cout << InformationMsg(getPrompt(m_walletBackend)) << std::flush;

                lastWalletHeight = walletBlockCount;
                lastNetworkHeight = networkBlockCount;
            }

            lastSummary = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    m_walletBackend->m_eventHandler->onTransaction.unsubscribe();
}

void TransactionMonitor::stop()
{
    m_shouldStop = true;
    m_queuedTransactions.stop();
}

std::shared_ptr<std::mutex> TransactionMonitor::getMutex() const
{
    return m_mutex;
}
