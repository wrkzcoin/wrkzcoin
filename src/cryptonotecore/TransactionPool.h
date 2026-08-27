// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ITransactionPool.h"
#include "TransactionValidatiorState.h"
#include "crypto/crypto.h"

#include <list>
#include <logging/LoggerMessage.h>
#include <logging/LoggerRef.h>
#include <mutex>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace CryptoNote
{
    class TransactionPool : public ITransactionPool
    {
      public:
        TransactionPool(std::shared_ptr<Logging::ILogger> logger);

        virtual bool
            pushTransaction(CachedTransaction &&transaction, TransactionValidatorState &&transactionState) override;

        virtual const CachedTransaction &getTransaction(const Crypto::Hash &hash) const override;

        virtual const std::optional<CachedTransaction> tryGetTransaction(const Crypto::Hash &hash) const override;

        virtual bool removeTransaction(const Crypto::Hash &hash) override;

        virtual size_t getFusionTransactionCount() const override;

        virtual size_t getTransactionCount() const override;

        virtual std::vector<Crypto::Hash> getTransactionHashes() const override;

        virtual bool checkIfTransactionPresent(const Crypto::Hash &hash) const override;

        virtual const TransactionValidatorState &getPoolTransactionValidationState() const override;

        virtual std::vector<CachedTransaction> getPoolTransactions() const override;

        virtual std::tuple<std::vector<CachedTransaction>, std::vector<CachedTransaction>>
            getPoolTransactionsForBlockTemplate() const override;

        virtual uint64_t getTransactionReceiveTime(const Crypto::Hash &hash) const override;

        virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash &paymentId) const override;

        virtual void flush() override;

      private:
        TransactionValidatorState poolState;

        struct PendingTransactionInfo
        {
            uint64_t receiveTime;

            CachedTransaction cachedTransaction;

            std::optional<Crypto::Hash> paymentId;

            const Crypto::Hash &getTransactionHash() const;
        };

        struct TransactionPriorityComparator
        {
            // lhs > hrs
            bool operator()(const PendingTransactionInfo &lhs, const PendingTransactionInfo &rhs) const;
        };

        /* Pending transactions live in a list so that the index maps below can
           hold iterators that stay valid across insert and erase. */
        typedef std::list<PendingTransactionInfo> PendingTransactions;

        PendingTransactions m_pendingTransactions;

        /* Primary index: transaction hash -> entry. Unique. */
        std::unordered_map<Crypto::Hash, PendingTransactions::iterator> m_transactionsByHash;

        /* Secondary index, non-unique. Only transactions that carry a payment
           id are present; the only query is by a concrete payment id, so the
           entries the old index kept under a null key were never looked up. */
        std::unordered_multimap<Crypto::Hash, PendingTransactions::iterator> m_transactionsByPaymentId;

        /* The priority ordering is materialised on demand rather than kept
           live. m_pendingTransactions is in insertion order and the sort is
           stable, which reproduces the old ordered_non_unique index exactly,
           ties included - see ContainerTests::testTransactionPoolContainer,
           which fails if the sort is not stable. Every caller walks the whole
           ordering anyway, and the pool holds at most a few thousand entries.

           Callers must hold m_transactionsMutex. */
        std::vector<const PendingTransactionInfo *> transactionsByPriority() const;

        mutable std::mutex m_transactionsMutex;

        Logging::LoggerRef logger;
    };

} // namespace CryptoNote
