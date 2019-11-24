// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CachedTransaction.h"

#include <common/CryptoNoteTools.h>
#include <common/Varint.h>
#include <config/CryptoNoteConfig.h>

using namespace Crypto;
using namespace CryptoNote;

CachedTransaction::CachedTransaction(Transaction &&transaction): transaction(std::move(transaction)) {}

CachedTransaction::CachedTransaction(const Transaction &transaction): transaction(transaction) {}

CachedTransaction::CachedTransaction(const BinaryArray &transactionBinaryArray):
    transactionBinaryArray(transactionBinaryArray)
{
    if (!fromBinaryArray<Transaction>(transaction, this->transactionBinaryArray.value()))
    {
        throw std::runtime_error("CachedTransaction::CachedTransaction(BinaryArray&&), deserealization error.");
    }
}

const Transaction &CachedTransaction::getTransaction() const
{
    return transaction;
}

const Crypto::Hash &CachedTransaction::getTransactionHash() const
{
    if (!transactionHash)
    {
        transactionHash = getBinaryArrayHash(getTransactionBinaryArray());
    }

    return transactionHash.value();
}

const Crypto::Hash &CachedTransaction::getTransactionPrefixHash() const
{
    if (!transactionPrefixHash)
    {
        transactionPrefixHash = getObjectHash(static_cast<const TransactionPrefix &>(transaction));
    }

    return transactionPrefixHash.value();
}

const BinaryArray &CachedTransaction::getTransactionBinaryArray() const
{
    if (!transactionBinaryArray)
    {
        transactionBinaryArray = toBinaryArray(transaction);
    }

    return transactionBinaryArray.value();
}

uint64_t CachedTransaction::getTransactionFee() const
{
    if (!transactionFee)
    {
        uint64_t summaryInputAmount = 0;
        uint64_t summaryOutputAmount = 0;
        for (auto &out : transaction.outputs)
        {
            summaryOutputAmount += out.amount;
        }

        for (auto &in : transaction.inputs)
        {
            if (in.type() == typeid(KeyInput))
            {
                summaryInputAmount += boost::get<KeyInput>(in).amount;
            }
            else if (in.type() == typeid(BaseInput))
            {
                return 0;
            }
            else
            {
                assert(false && "Unknown out type");
            }
        }

        transactionFee = summaryInputAmount - summaryOutputAmount;
    }

    return transactionFee.value();
}

uint64_t CachedTransaction::getTransactionAmount() const
{
    if (!transactionAmount)
    {
        uint64_t summaryOutputAmount = 0;

        for (const auto &out : transaction.outputs)
        {
            summaryOutputAmount += out.amount;
        }

        transactionAmount = summaryOutputAmount;
    }

    return transactionAmount.value();
}
