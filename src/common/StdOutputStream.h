// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IOutputStream.h"

#include <ostream>

namespace Common
{
    class StdOutputStream : public IOutputStream
    {
      public:
        StdOutputStream(std::ostream &out);

        StdOutputStream &operator=(const StdOutputStream &) = delete;

        uint64_t writeSome(const void *data, uint64_t size) override;

      private:
        std::ostream &out;
    };

} // namespace Common
