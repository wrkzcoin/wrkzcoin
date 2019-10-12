// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////
#include <utilities/Fees.h>
/////////////////////////////

#include <config/CryptoNoteConfig.h>
#include <sstream>

namespace Utilities
{
    /* Returns minFee */
    uint64_t getMinimumFee(const uint64_t height)
    {
        uint64_t minFee = CryptoNote::parameters::MINIMUM_FEE;

        if (height >= CryptoNote::parameters::MINIMUM_FEE_V1_HEIGHT)
        {
            minFee = CryptoNote::parameters::MINIMUM_FEE_V1;
        }
        return minFee;
    }
} // namespace Utilities
