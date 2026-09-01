// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <IDataBase.h>
#include <logging/LoggerRef.h>
#include <string>

namespace CryptoNote
{
    namespace LiteSnapshotImport
    {
        /* Loads a lite node base snapshot into an empty database, so a node can
           start from the index only region [0, H) instead of spending days
           rebuilding it from the chain. See LITESNAPSHOT.md.

           Refuses, without writing anything, unless all of these hold:

             - the file is a snapshot this build understands
             - its genesis hash is this network's
             - its height is the one the daemon was started with
             - its digest appears in LITE_SNAPSHOT_DIGESTS
             - the database holds nothing but the genesis block
             - the payload hashes to the digest the header claims, and the block
               info in it is internally consistent and agrees with the compiled
               in checkpoints

           The digest is the load bearing one. Everything below H in a snapshot
           is taken on trust - it cannot be checked without the block bodies the
           importing node does not have - so a file whose digest is not pinned in
           this build is not importable, and there is no flag to say otherwise.

           The payload is read twice: once to verify, once to write. Nothing
           reaches the database until the first pass has finished and agreed,
           because a half imported chain that looks whole is the failure this
           whole design exists to avoid.

           Throws std::runtime_error with a message meant for an operator. */
        void importSnapshot(
            IDataBase &database,
            const std::string &path,
            uint32_t liteHeight,
            const Crypto::Hash &genesisHash,
            const std::string &scratchDirectory,
            Logging::LoggerRef &logger);
    } // namespace LiteSnapshotImport
} // namespace CryptoNote
