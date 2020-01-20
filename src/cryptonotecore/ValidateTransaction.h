// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The Galaxia Project Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <system_error>

#include <CryptoNote.h>
#include <cryptonotecore/CachedTransaction.h>
#include <cryptonotecore/Checkpoints.h>
#include <cryptonotecore/Currency.h>
#include <cryptonotecore/IBlockchainCache.h>
#include <utilities/ThreadPool.h>

struct TransactionValidationResult
{
    /* A programmatic error code of the result */
    std::error_code errorCode;

    /* An error message describing the error code */
    std::string errorMessage;

    /* Whether the transaction is valid */
    bool valid = false;

    /* The fee of the transaction */
    uint64_t fee = 0;

    /* Is this transaction a fusion transaction */
    bool isFusionTransaction = false;
};

class ValidateTransaction
{
    public:
        /////////////////
        /* CONSTRUCTOR */
        /////////////////
        ValidateTransaction(
            const CryptoNote::CachedTransaction &cachedTransaction,
            CryptoNote::TransactionValidatorState &state,
            CryptoNote::IBlockchainCache *cache,
            const CryptoNote::Currency &currency,
            const CryptoNote::Checkpoints &checkpoints,
            Utilities::ThreadPool<bool> &threadPool,
            const uint64_t blockHeight,
            const uint64_t blockSizeMedian,
            const bool isPoolTransaction);

        /////////////////////////////
        /* PUBLIC MEMBER FUNCTIONS */
        /////////////////////////////
        TransactionValidationResult validate();

        TransactionValidationResult revalidateAfterHeightChange();

    private:
        //////////////////////////////
        /* PRIVATE MEMBER FUNCTIONS */
        //////////////////////////////
        bool validateTransactionSize();

        bool validateTransactionInputs();

        bool validateTransactionOutputs();

        bool validateTransactionFee();

        bool validateTransactionExtra();

        bool validateInputOutputRatio();

        bool validateTransactionMixin();

        bool validateTransactionInputsExpensive();

        void setTransactionValidationResult(const std::error_code &error_code, const std::string &error_message = "");

        /////////////////////////
        /* PRIVATE MEMBER VARS */
        /////////////////////////
        const CryptoNote::Transaction m_transaction;

        const CryptoNote::CachedTransaction &m_cachedTransaction;

        CryptoNote::TransactionValidatorState &m_validatorState;

        const CryptoNote::IBlockchainCache *m_blockchainCache;

        const CryptoNote::Currency &m_currency;

        const CryptoNote::Checkpoints &m_checkpoints;

        const uint64_t m_blockHeight;

        const uint64_t m_blockSizeMedian;

        const bool m_isPoolTransaction;

        TransactionValidationResult m_validationResult;

        uint64_t m_sumOfOutputs = 0;
        uint64_t m_sumOfInputs = 0;

        Utilities::ThreadPool<bool> &m_threadPool;

        std::mutex m_mutex;
};
