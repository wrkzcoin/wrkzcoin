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

        /* There is deliberately no way to add a checkpoint once the node is
           running. Every height at or below the last checkpoint skips proof of
           work and ring signatures, so the set is fixed at start-up: the
           compiled table plus --load-checkpoints. */

      private:
        std::map<uint32_t, Crypto::Hash> points;

        /* Protects `points` between start-up loading and the validation
           threads reading it. Heap-allocated so Checkpoints remains movable
           (std::mutex is not). */
        mutable std::unique_ptr<std::mutex> m_mutex;

        Logging::LoggerRef logger;
    };
} // namespace CryptoNote
