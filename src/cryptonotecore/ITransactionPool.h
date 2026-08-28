// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CachedTransaction.h"

#include <tuple>

namespace CryptoNote
{
    struct TransactionValidatorState;

    class ITransactionPool
    {
      public:
        virtual ~ITransactionPool() {};

        virtual bool pushTransaction(CachedTransaction &&tx, TransactionValidatorState &&transactionState) = 0;

        virtual const CachedTransaction &getTransaction(const Crypto::Hash &hash) const = 0;

        virtual const std::optional<CachedTransaction> tryGetTransaction(const Crypto::Hash &hash) const = 0;

        /* Borrowing variant of tryGetTransaction. Returns nullptr when the
           transaction is not in the pool. Callers that only need to inspect
           a transaction should prefer this - the optional returning form
           deep copies the transaction and its binary array, which is very
           expensive when sweeping the whole pool. The pointer has the same
           lifetime guarantees as the reference from getTransaction(). */
        virtual const CachedTransaction *tryGetTransactionRef(const Crypto::Hash &hash) const = 0;

        virtual bool removeTransaction(const Crypto::Hash &hash) = 0;

        virtual size_t getFusionTransactionCount() const = 0;

        virtual size_t getTransactionCount() const = 0;

        virtual std::vector<Crypto::Hash> getTransactionHashes() const = 0;

        virtual bool checkIfTransactionPresent(const Crypto::Hash &hash) const = 0;

        virtual const TransactionValidatorState &getPoolTransactionValidationState() const = 0;

        virtual std::vector<CachedTransaction> getPoolTransactions() const = 0;

        virtual std::tuple<std::vector<CachedTransaction>, std::vector<CachedTransaction>>
            getPoolTransactionsForBlockTemplate() const = 0;

        virtual uint64_t getTransactionReceiveTime(const Crypto::Hash &hash) const = 0;

        virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash &paymentId) const = 0;

        virtual void flush() = 0;

        /* Hashes dropped to keep the pool inside its size budget since this was
           last called. The caller is expected to notify observers for them. */
        virtual std::vector<Crypto::Hash> takeEvictedTransactions() = 0;
    };

} // namespace CryptoNote
