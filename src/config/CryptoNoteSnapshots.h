// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <initializer_list>

namespace CryptoNote
{
    /* Lite node base snapshots this build will import, by height and by the
       digest of their payload. See LITESNAPSHOT.md.

       This table is the entire security of snapshot import, and it is worth
       being blunt about why. A lite node built by syncing derives its key output
       and key image tables from real blocks, so it validates exactly as strongly
       as a full node. An imported one takes those tables on trust from whoever
       made the file, and they cannot be checked without the block bodies the
       recipient does not have. A snapshot carrying key outputs that never
       existed lets its author spend coins that were never mined, into a wallet
       that will accept them when no other node does; one missing key images lets
       double spends through.

       So a file is importable only if its digest appears here. There is no
       override flag and there will not be one - the fix for an unrecognised
       snapshot is to sync, not to force. An empty table means this build imports
       nothing, which is the correct default until a digest has actually been
       published.

       Adding an entry is a release decision, not a code change to be made
       casually:

         - Produce the snapshot from a node whose chain you trust, at a height at
           least MIN_LITE_FULL_BLOCK_DEPTH below the tip.
         - Have at least one other person produce it independently, from their
           own node, and confirm the digests match. That is what the format's
           determinism is for; skipping it means the table records one machine's
           word rather than a reproducible fact.
         - Publish the file, the height and the digest together.

       The digest covers the uncompressed payload, not the file, so producers on
       different zstd versions still agree. `snapshot_export` prints it when it
       finishes. */
    struct LiteSnapshotDigest
    {
        /* The lite height the snapshot describes: it carries the chain below
           this, and an importing node must be started with the same value. */
        uint32_t liteHeight;

        /* Hex encoded, 64 characters. */
        const char *payloadDigest;
    };

    const std::initializer_list<LiteSnapshotDigest> LITE_SNAPSHOT_DIGESTS = {
        /* No snapshot has been published yet, so nothing can be imported.
           Deliberate: see above. The first blessed height is intended to be
           4,000,000. */
    };
} // namespace CryptoNote
