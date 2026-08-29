// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace EncodingTests
{
    /* Known-answer and round trip tests for the base64 codec in
       common/StringTools.

       This carries hashes, public keys and key images on the wallet sync path,
       where a decoder that quietly produced the wrong bytes would show up as a
       wallet that simply does not see its own transactions - so the malformed
       input cases matter as much as the valid ones.

       Calls exit(1) on the first failure. */
    void runAll();
} // namespace EncodingTests
