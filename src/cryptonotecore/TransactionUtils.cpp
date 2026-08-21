// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TransactionUtils.h"

#include "CryptoNoteFormatUtils.h"
#include "common/TransactionExtra.h"
#include "crypto/crypto.h"

#include <unordered_set>
#include <variant>

using namespace Crypto;

namespace CryptoNote
{
    bool checkInputsKeyimagesDiff(const CryptoNote::TransactionPrefix &tx)
    {
        std::unordered_set<Crypto::KeyImage> ki;
        for (const auto &in : tx.inputs)
        {
            if (std::holds_alternative<KeyInput>(in))
            {
                if (!ki.insert(std::get<KeyInput>(in).keyImage).second)
                {
                    return false;
                }
            }
        }

        return true;
    }

    // TransactionInput helper functions

    size_t getRequiredSignaturesCount(const TransactionInput &in)
    {
        if (std::holds_alternative<KeyInput>(in))
        {
            return std::get<KeyInput>(in).outputIndexes.size();
        }

        return 0;
    }

    uint64_t getTransactionInputAmount(const TransactionInput &in)
    {
        if (std::holds_alternative<KeyInput>(in))
        {
            return std::get<KeyInput>(in).amount;
        }

        return 0;
    }

    TransactionTypes::InputType getTransactionInputType(const TransactionInput &in)
    {
        if (std::holds_alternative<KeyInput>(in))
        {
            return TransactionTypes::InputType::Key;
        }

        if (std::holds_alternative<BaseInput>(in))
        {
            return TransactionTypes::InputType::Generating;
        }

        return TransactionTypes::InputType::Invalid;
    }

    const TransactionInput &getInputChecked(const CryptoNote::TransactionPrefix &transaction, size_t index)
    {
        if (transaction.inputs.size() <= index)
        {
            throw std::runtime_error("Transaction input index out of range");
        }

        return transaction.inputs[index];
    }

    const TransactionInput &getInputChecked(
        const CryptoNote::TransactionPrefix &transaction,
        size_t index,
        TransactionTypes::InputType type)
    {
        const auto &input = getInputChecked(transaction, index);
        if (getTransactionInputType(input) != type)
        {
            throw std::runtime_error("Unexpected transaction input type");
        }

        return input;
    }

    // TransactionOutput helper functions

    TransactionTypes::OutputType getTransactionOutputType(const TransactionOutputTarget &out)
    {
        if (std::holds_alternative<KeyOutput>(out))
        {
            return TransactionTypes::OutputType::Key;
        }

        return TransactionTypes::OutputType::Invalid;
    }

    const TransactionOutput &getOutputChecked(const CryptoNote::TransactionPrefix &transaction, size_t index)
    {
        if (transaction.outputs.size() <= index)
        {
            throw std::runtime_error("Transaction output index out of range");
        }

        return transaction.outputs[index];
    }

    const TransactionOutput &getOutputChecked(
        const CryptoNote::TransactionPrefix &transaction,
        size_t index,
        TransactionTypes::OutputType type)
    {
        const auto &output = getOutputChecked(transaction, index);
        if (getTransactionOutputType(output.target) != type)
        {
            throw std::runtime_error("Unexpected transaction output target type");
        }

        return output;
    }

    bool findOutputsToAccount(
        const CryptoNote::TransactionPrefix &transaction,
        const AccountPublicAddress &addr,
        const SecretKey &viewSecretKey,
        std::vector<uint32_t> &out,
        uint64_t &amount)
    {
        AccountKeys keys;
        keys.address = addr;
        // only view secret key is used, spend key is not needed
        keys.viewSecretKey = viewSecretKey;

        Crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(transaction.extra);

        amount = 0;
        size_t keyIndex = 0;
        uint32_t outputIndex = 0;

        Crypto::KeyDerivation derivation;
        generate_key_derivation(txPubKey, keys.viewSecretKey, derivation);

        for (const TransactionOutput &o : transaction.outputs)
        {
            assert(std::holds_alternative<KeyOutput>(o.target));
            if (std::holds_alternative<KeyOutput>(o.target))
            {
                if (is_out_to_acc(keys, std::get<KeyOutput>(o.target), derivation, keyIndex))
                {
                    out.push_back(outputIndex);
                    amount += o.amount;
                }

                ++keyIndex;
            }

            ++outputIndex;
        }

        return true;
    }

} // namespace CryptoNote
