// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace WalletCryptoTests
{
    /* Known-answer tests for the primitives that replaced Crypto++: SHA-256
       (FIPS 180-4 / NIST CAVP), HMAC-SHA256 (RFC 4231), PBKDF2-HMAC-SHA256
       (RFC 7914 section 11) and AES-128-CBC (NIST SP 800-38A F.2), plus the
       PKCS#7 padding edge cases and a wallet-shaped round trip.

       These are the standing guard on the wallet file format: the parameters
       are fixed by files already on disk, so a change that breaks these breaks
       every existing wallet. Calls exit(1) on the first failure. */
    void runAll();
} // namespace WalletCryptoTests
