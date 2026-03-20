// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteBasicImpl.h"

#include <logging/LoggerRef.h>
#include <map>
#include <memory>
#include <mutex>

namespace CryptoNote
{
    class Checkpoints
    {
      public:
        Checkpoints(std::shared_ptr<Logging::ILogger> log);

        bool addCheckpoint(uint32_t index, const std::string &hash_str);

        bool loadCheckpointsFromFile(const std::string &fileName);

        bool isInCheckpointZone(uint32_t index) const;

        bool checkBlock(uint32_t index, const Crypto::Hash &h) const;

        bool checkBlock(uint32_t index, const Crypto::Hash &h, bool &isCheckpoint) const;

        /* Add a dynamic checkpoint at runtime (e.g. when network consensus confirms
           a block that local validation rejected).  Returns true if inserted. */
        bool addDynamicCheckpoint(uint32_t height, const Crypto::Hash &hash);

      private:
        std::map<uint32_t, Crypto::Hash> points;

        /* Protects `points` against concurrent reads/writes when dynamic
           checkpoints are inserted while validation threads are reading.
           Heap-allocated so Checkpoints remains movable (std::mutex is not). */
        mutable std::unique_ptr<std::mutex> m_mutex;

        Logging::LoggerRef logger;
    };
} // namespace CryptoNote
