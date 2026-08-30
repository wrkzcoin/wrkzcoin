// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace PaymentIdTests
{
    /* Checks the encrypted short payment ID scheme.

       The property that matters is that the sender and the receiver, starting
       from different key material, arrive at the same keystream: the sender
       has the transaction private key and the receiver's public view key, the
       receiver has the transaction public key and their own private view key.
       If that ever stops holding, payments become unattributable, so it is
       checked directly rather than inferred from a round trip.

       Also pins a known answer so the wire format cannot drift, and so other
       implementations (the Flutter wallets, the explorer) have something to
       test themselves against. Calls exit(1) on the first failure. */
    void runAll();
} // namespace PaymentIdTests
