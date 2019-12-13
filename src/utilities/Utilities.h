// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <atomic>
#include <chrono>
#include <errors/Errors.h>
#include <iomanip>
#include <string>
#include <vector>

namespace Utilities
{
    uint64_t getTransactionSum(const std::vector<std::pair<std::string, uint64_t>> destinations);

    uint64_t getUpperBound(const uint64_t val, const uint64_t nearestMultiple);

    uint64_t getLowerBound(const uint64_t val, const uint64_t nearestMultiple);

    bool isInputUnlocked(const uint64_t unlockTime, const uint64_t currentHeight);

    uint64_t getMaxTxSize(const uint64_t currentHeight);

    void sleepUnlessStopping(const std::chrono::milliseconds duration, std::atomic<bool> &condition);

    uint64_t scanHeightToTimestamp(const uint64_t scanHeight);

    uint64_t timestampToScanHeight(const uint64_t timestamp);

    uint64_t getCurrentTimestampAdjusted();

    bool isSubtractionSafe(int64_t currentValue, uint64_t transferAmount);

    bool parseDaemonAddressFromString(std::string &host, uint16_t &port, std::string address);

    uint64_t getTransactionFee(
        const size_t transactionSize,
        const uint64_t height,
        const uint64_t feePerByte);

    uint64_t getMinimumTransactionFee(
        const size_t transactionSize,
        const uint64_t height);

    size_t estimateTransactionSize(
        const uint64_t mixin,
        const size_t numInputs,
        const size_t numOutputs,
        const bool havePaymentID,
        const size_t extraDataSize);

    size_t getApproximateMaximumInputCount(
        const size_t transactionSize,
        const size_t outputCount,
        const size_t mixinCount);

    /* Verify that a + b will not overflow when added. */
    /* 2 positive numbers - should always get greater (or equal) when summed. */
    /* Any negative numbers - should always get smaller (or equal) when summed. */
    template<typename T> bool additionWillOverflow(T a, T b)
    {
        static_assert(std::is_integral<T>::value, "additionWillOverflow can only be used for integral inputs!");

        /* The result of the addition */
        T result = a + b;

        /* The larger number */
        T larger = std::max(a, b);

        /* If either number is negative, the result should be smaller than the
           larger of the inputs. If not, it has overflowed. */
        if (a < 0 || b < 0)
        {
            return result > larger;
        }

        /* If both numbers are positive, the result should be greater than the
           larger of the two inputs. If not, it has overflowed. */
        return result < larger;
    }

    template<typename T> bool subtractionWillOverflow(T a, T b)
    {
        /* Subtraction is just addition where the second param is negated */
        return additionWillOverflow(a, -b);
    }

    /* Verify the sum of the vector amounts will not overflow */
    template<typename T> bool sumWillOverflow(std::vector<T> amounts)
    {
        T sum = 0;

        /* Add each item to the sum as we go, checking if any of the additions
           will make the total sum overflow */
        for (const auto item : amounts)
        {
            if (additionWillOverflow(item, sum))
            {
                return true;
            }

            sum += item;
        }

        return false;
    }
} // namespace Utilities
