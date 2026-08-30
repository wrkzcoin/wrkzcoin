// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "PaymentIdTests.h"

#include <common/StringTools.h>
#include <common/TransactionExtra.h>
#include <config/Constants.h>
#include <crypto/crypto.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utilities/ParseExtra.h>
#include <utilities/PaymentIdEncryption.h>
#include <vector>

namespace PaymentIdTests
{
    namespace
    {
        void fail(const std::string &what, const std::string &expected, const std::string &actual)
        {
            std::cout << std::endl
                      << "Payment ID test FAILED: " << what << std::endl
                      << "  expected: " << expected << std::endl
                      << "  actual:   " << actual << std::endl
                      << "Terminating." << std::endl;

            exit(1);
        }

        void check(const std::string &what, const std::string &expected, const std::string &actual)
        {
            if (expected != actual)
            {
                fail(what, expected, actual);
            }
        }

        /* A fixed, non-random key pair so the known answer test is stable.
           These are test vectors, not keys that hold anything. */
        const std::string RECEIVER_PRIVATE_VIEW_KEY = "8d31b6a2b0e2d1c6f4e0b4d6b0b7d0a0e6b1b6e2e0b6d1a0c6b4e0d1b6a2b00d";

        const std::string TX_PRIVATE_KEY = "0c1b6a2b0e2d1c6f4e0b4d6b0b7d0a0e6b1b6e2e0b6d1a0c6b4e0d1b6a2b0d0f";

        const std::string PAYMENT_ID = "1122334455667788";

        /* Derived independently of this codebase, from a from-scratch ed25519
           and original-padding Keccak-256 implementation, so it checks the
           construction rather than merely pinning whatever we happen to
           produce. See docs/docs/guides/encrypted-payment-ids.md.

           For the fixed keys above:
             receiver public view key
               86f65b62d4e7443f150d4955100e159eca8b18a59fc8aa9667d6b905236e684a
             tx public key
               ec2dcf9ba1572f2d211ad676b5820ac09b35029ca8d51ccd704b8398d86e8a33 */
        const std::string KNOWN_CIPHERTEXT = "5a3c5490ba23c021";
    } // namespace

    void runAll()
    {
        std::cout << std::endl << "Test Encrypted Payment IDs" << std::endl << std::endl;

        /* Rebuild the two key pairs from the fixed secrets above. */
        Crypto::SecretKey receiverPrivateViewKey;
        Crypto::SecretKey txPrivateKey;

        if (!Common::podFromHex(RECEIVER_PRIVATE_VIEW_KEY, receiverPrivateViewKey)
            || !Common::podFromHex(TX_PRIVATE_KEY, txPrivateKey))
        {
            fail("test vector key parsing", "valid hex secret keys", "parse failure");
        }

        Crypto::PublicKey receiverPublicViewKey;
        Crypto::PublicKey txPublicKey;

        if (!Crypto::secret_key_to_public_key(receiverPrivateViewKey, receiverPublicViewKey)
            || !Crypto::secret_key_to_public_key(txPrivateKey, txPublicKey))
        {
            fail("test vector key derivation", "valid public keys", "derivation failure");
        }

        /* The sender encrypts with the receiver's public view key and the
           transaction private key. */
        std::cout << "Sender encrypts to receiver: ";

        const std::string ciphertext =
            Utilities::encryptPaymentIdHex(PAYMENT_ID, receiverPublicViewKey, txPrivateKey);

        if (ciphertext.empty())
        {
            fail("sender encryption", "8 bytes of ciphertext", "empty (encryption failed)");
        }

        if (ciphertext == PAYMENT_ID)
        {
            fail("sender encryption", "ciphertext different from plaintext", ciphertext);
        }

        std::cout << "PASSED (" << ciphertext << ")" << std::endl;

        /* The receiver reaches the same keystream from the other side: the
           transaction public key and their own private view key. This is the
           property the whole scheme rests on. */
        std::cout << "Receiver recovers plaintext:  ";

        const std::string recovered =
            Utilities::encryptPaymentIdHex(ciphertext, txPublicKey, receiverPrivateViewKey);

        check("receiver decryption", PAYMENT_ID, recovered);

        std::cout << "PASSED" << std::endl;

        /* Encrypting from either side must produce identical ciphertext -
           8*r*A and 8*a*R are the same point. */
        std::cout << "Both sides agree on keystream: ";

        const std::string fromReceiverSide =
            Utilities::encryptPaymentIdHex(PAYMENT_ID, txPublicKey, receiverPrivateViewKey);

        check("shared secret agreement", ciphertext, fromReceiverSide);

        std::cout << "PASSED" << std::endl;

        /* A different view key must not recover the payment ID. */
        std::cout << "Wrong view key fails:         ";

        Crypto::SecretKey wrongPrivateViewKey;
        Crypto::PublicKey wrongPublicViewKey;

        Crypto::generate_keys(wrongPublicViewKey, wrongPrivateViewKey);

        const std::string wrongRecovery =
            Utilities::encryptPaymentIdHex(ciphertext, txPublicKey, wrongPrivateViewKey);

        if (wrongRecovery == PAYMENT_ID)
        {
            fail("wrong key rejection", "anything but " + PAYMENT_ID, wrongRecovery);
        }

        std::cout << "PASSED" << std::endl;

        /* Applying the transform twice is the identity - it is a XOR. */
        std::cout << "Transform is involutive:      ";

        const std::string twice =
            Utilities::encryptPaymentIdHex(ciphertext, receiverPublicViewKey, txPrivateKey);

        check("double encryption", PAYMENT_ID, twice);

        std::cout << "PASSED" << std::endl;

        /* Inputs that are not exactly 8 bytes of hex must be refused rather
           than silently truncated or padded. */
        std::cout << "Bad input refused:            ";

        const std::vector<std::string> badInputs = {
            "",
            "11223344556677",                                                   /* 7 bytes */
            "112233445566778899",                                               /* 9 bytes */
            "zzzzzzzzzzzzzzzz",                                                 /* not hex */
            "1122334455667788112233445566778811223344556677881122334455667788", /* long pid */
        };

        for (const auto &bad : badInputs)
        {
            const std::string result =
                Utilities::encryptPaymentIdHex(bad, receiverPublicViewKey, txPrivateKey);

            if (!result.empty())
            {
                fail("rejecting malformed input \"" + bad + "\"", "empty string", result);
            }
        }

        std::cout << "PASSED" << std::endl;

        /* Known answer. This pins the wire format: the key derivation, the
           0x8d domain separation byte, and taking the first 8 bytes of the
           keccak output. Other implementations should reproduce this. */
        std::cout << "Known answer:                 ";

        check("known answer for the fixed test vectors", KNOWN_CIPHERTEXT, ciphertext);

        std::cout << "PASSED" << std::endl;

        /* The whole path, not just the cipher: build a tx_extra the way a
           transaction does, then read it back the way the daemon and the
           wallet do. This is what catches a sub-tag that the writer and the
           reader disagree about. */
        std::cout << "tx_extra round trip:          ";

        std::vector<uint8_t> paymentIdBin;

        Common::fromHex(ciphertext, paymentIdBin);

        std::vector<uint8_t> extraNonce;
        extraNonce.push_back(Constants::TX_EXTRA_ENCRYPTED_SHORT_PAYMENT_ID_IDENTIFIER);
        extraNonce.insert(extraNonce.end(), paymentIdBin.begin(), paymentIdBin.end());

        std::vector<uint8_t> extra;

        if (!CryptoNote::addTransactionPublicKeyToExtra(extra, txPublicKey)
            || !CryptoNote::addExtraNonceToTransactionExtra(extra, extraNonce))
        {
            fail("building tx_extra", "a public key and a nonce field", "write failure");
        }

        Utilities::ParsedExtra parsed = Utilities::parseExtra(extra);

        check("transaction public key survives the round trip",
              Common::podToHex(txPublicKey),
              Common::podToHex(parsed.transactionPublicKey));

        if (!parsed.paymentIDEncrypted)
        {
            fail("parsing the encrypted payment ID", "paymentIDEncrypted set", "not set");
        }

        check("ciphertext survives the round trip", ciphertext, parsed.paymentID);

        /* And the receiver gets back to the plaintext using nothing but what
           the transaction carries plus their own view key. */
        check(
            "receiver recovers the payment ID from tx_extra alone",
            PAYMENT_ID,
            Utilities::encryptPaymentIdHex(parsed.paymentID, parsed.transactionPublicKey, receiverPrivateViewKey));

        std::cout << "PASSED" << std::endl;

        /* The two legacy plaintext sub-tags must be consumed but never
           reported - a wrong payment ID credits the wrong account, a missing
           one gets investigated. Anything after them still has to parse,
           which is the reason we consume rather than stop. */
        std::cout << "Legacy sub-tags ignored:      ";

        for (const uint8_t legacyTag : {Constants::TX_EXTRA_SHORT_PAYMENT_ID_IDENTIFIER,
                                        Constants::TX_EXTRA_SHORT_PAYMENT_ID_IDENTIFIER_PRERELEASE})
        {
            std::vector<uint8_t> legacyNonce;
            legacyNonce.push_back(legacyTag);
            legacyNonce.insert(legacyNonce.end(), paymentIdBin.begin(), paymentIdBin.end());

            /* An arbitrary data field behind it, so we can tell the parser
               stepped over the legacy field by exactly the right amount. */
            const std::vector<uint8_t> arbitrary = {0xde, 0xad, 0xbe, 0xef};

            legacyNonce.push_back(Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER);
            legacyNonce.push_back(static_cast<uint8_t>(arbitrary.size()));
            legacyNonce.insert(legacyNonce.end(), arbitrary.begin(), arbitrary.end());

            std::vector<uint8_t> legacyExtra;

            if (!CryptoNote::addTransactionPublicKeyToExtra(legacyExtra, txPublicKey)
                || !CryptoNote::addExtraNonceToTransactionExtra(legacyExtra, legacyNonce))
            {
                fail("building legacy tx_extra", "a public key and a nonce field", "write failure");
            }

            const Utilities::ParsedExtra legacyParsed = Utilities::parseExtra(legacyExtra);

            check("legacy sub-tag reports no payment ID", std::string(), legacyParsed.paymentID);

            check(
                "the field after a legacy sub-tag still parses",
                Common::toHex(arbitrary),
                Common::toHex(legacyParsed.extraData));
        }

        std::cout << "PASSED" << std::endl;
    }
} // namespace PaymentIdTests
