// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/CachedTransaction.h>
#include <cryptonotecore/TransactionApi.h>
#include <limits>
#include <utilities/Mixins.h>

namespace CryptoNote
{
    class Mixins
    {
      public:
        /* This method is commonly used by the node to determine if the transactions in the vector have
           the correct mixin (anonymity) as defined by the current rules */
        static std::tuple<bool, std::string> validate(std::vector<CachedTransaction> transactions, uint64_t height)
        {
            auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(height);

            for (const auto &transaction : transactions)
            {
                auto [success, error] = validate(transaction, minMixin, maxMixin, height);

                if (!success)
                {
                    return {false, error};
                }
            }

            return {true, std::string()};
        }

        /* This method is commonly used by the node to determine if the transaction has
           the correct mixin (anonymity) as defined by the current rules */
        static std::tuple<bool, std::string>
            validate(const CachedTransaction &transaction, uint64_t minMixin, uint64_t maxMixin, uint64_t height)
        {
            uint64_t largestRing = 1;

            uint64_t smallestRing = std::numeric_limits<uint64_t>::max();

            bool haveKeyInput = false;

            const auto tx = createTransaction(transaction.getTransaction());

            for (size_t i = 0; i < tx->getInputCount(); ++i)
            {
                if (tx->getInputType(i) != TransactionTypes::InputType::Key)
                {
                    continue;
                }

                KeyInput input;
                tx->getInput(i, input);
                const uint64_t currentRingSize = input.outputIndexes.size();

                haveKeyInput = true;

                if (currentRingSize > largestRing)
                {
                    largestRing = currentRingSize;
                }

                if (currentRingSize < smallestRing)
                {
                    smallestRing = currentRingSize;
                }
            }

            /* No key inputs at all leaves the smallest where the largest starts,
               which keeps the verdict on such a transaction exactly what it was
               before the floor was split out. An input carrying no ring members
               is nonsense the input checks reject, but those run after this one,
               so clamp rather than let the subtraction below wrap and turn the
               smallest ring into the largest number there is. */
            if (!haveKeyInput || smallestRing < 1)
            {
                smallestRing = haveKeyInput ? 1 : largestRing;
            }

            /* Ring size = mixin + 1 - your transaction plus the others you mix with */
            const uint64_t largestMixin = largestRing - 1;

            /* Nothing requires every input to carry the same ring size, and the
               ceiling has always been judged on the largest. The floor was too,
               which meant it was not a floor at all: one input at the full ring
               satisfied it on behalf of every other input in the transaction, so
               a transaction could mix a full ring with rings of two and pass.
               That is harmless while the minimum is 1 and every wallet builds
               uniform rings, but it would quietly hollow out a fork that raises
               the minimum to pin the ring size - the rule it relies on would not
               hold. Judge the floor on the smallest ring from the V6 fork on.

               Gated on height rather than applied outright because blocks below
               it were accepted under the old reading, and a node re-validating
               them has to reach the same verdict or it cannot sync the chain. */
            const uint64_t smallestMixin =
                height >= CryptoNote::parameters::MIXIN_LIMITS_V6_HEIGHT ? smallestRing - 1 : largestMixin;

            std::stringstream str;

            if (largestMixin > maxMixin)
            {
                str << "Transaction " << transaction.getTransactionHash()
                    << " is not valid. Reason: transaction mixin is too large (" << largestMixin
                    << "). Maximum mixin allowed is " << maxMixin;

                return {false, str.str()};
            }
            else if (smallestMixin < minMixin)
            {
                str << "Transaction " << transaction.getTransactionHash()
                    << " is not valid. Reason: transaction mixin is too small (" << smallestMixin
                    << "). Minimum mixin allowed is " << minMixin;

                return {false, str.str()};
            }

            return {true, std::string()};
        }
    };
} // namespace CryptoNote
