// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "WalletCryptoTests.h"

#include "crypto/WalletCrypto.h"
#include "crypto/aes_cbc.h"
#include "crypto/sha256.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace WalletCryptoTests
{
    namespace
    {
        void fail(const std::string &what, const std::string &expected, const std::string &actual)
        {
            std::cout << std::endl
                      << "Wallet crypto test FAILED: " << what << std::endl
                      << "  expected: " << expected << std::endl
                      << "  actual:   " << actual << std::endl
                      << "Terminating." << std::endl;

            exit(1);
        }

        std::string toHex(const uint8_t *data, const size_t length)
        {
            static const char *digits = "0123456789abcdef";

            std::string out;

            out.reserve(length * 2);

            for (size_t i = 0; i < length; i++)
            {
                out += digits[data[i] >> 4];
                out += digits[data[i] & 0x0f];
            }

            return out;
        }

        std::string toHex(const std::vector<uint8_t> &data)
        {
            return toHex(data.data(), data.size());
        }

        std::vector<uint8_t> fromHex(const std::string &hex)
        {
            std::vector<uint8_t> out;

            out.reserve(hex.size() / 2);

            for (size_t i = 0; i + 1 < hex.size(); i += 2)
            {
                out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            }

            return out;
        }

        void check(const std::string &what, const std::string &expected, const std::string &actual)
        {
            if (expected != actual)
            {
                fail(what, expected, actual);
            }
        }

        void testSha256()
        {
            struct
            {
                std::string input;
                std::string expected;
            } const cases[] = {
                /* FIPS 180-4 / NIST CAVP byte-oriented vectors. */
                {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
                {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
                {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
                {"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnop"
                 "qrstnopqrstu",
                 "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"}};

            for (const auto &c : cases)
            {
                uint8_t digest[SHA256_DIGEST_SIZE];

                sha256(c.input.data(), c.input.size(), digest);

                check("SHA-256 of a " + std::to_string(c.input.size()) + " byte input",
                      c.expected,
                      toHex(digest, sizeof(digest)));
            }

            /* One million 'a' - exercises the multi-block update path and the
               64 bit length encoding. */
            {
                sha256_ctx ctx;
                uint8_t digest[SHA256_DIGEST_SIZE];
                const std::string chunk(1000, 'a');

                sha256_init(&ctx);

                for (int i = 0; i < 1000; i++)
                {
                    sha256_update(&ctx, chunk.data(), chunk.size());
                }

                sha256_final(&ctx, digest);

                check("SHA-256 of one million 'a'",
                      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
                      toHex(digest, sizeof(digest)));
            }

            /* Feeding the same message one byte at a time must agree with the
               one-shot call - this is what catches buffer handling bugs. */
            {
                const std::string message = "The quick brown fox jumps over the lazy dog";

                uint8_t oneShot[SHA256_DIGEST_SIZE];
                uint8_t drip[SHA256_DIGEST_SIZE];
                sha256_ctx ctx;

                sha256(message.data(), message.size(), oneShot);

                sha256_init(&ctx);

                for (const char c : message)
                {
                    sha256_update(&ctx, &c, 1);
                }

                sha256_final(&ctx, drip);

                check("SHA-256 streamed one byte at a time",
                      toHex(oneShot, sizeof(oneShot)),
                      toHex(drip, sizeof(drip)));
            }
        }

        void testHmacSha256()
        {
            /* RFC 4231 test cases. Case 5 is a truncation test and is skipped;
               we never truncate. */
            struct
            {
                std::string name;
                std::vector<uint8_t> key;
                std::vector<uint8_t> data;
                std::string expected;
            } const cases[] = {
                {"RFC 4231 case 1",
                 std::vector<uint8_t>(20, 0x0b),
                 {'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e'},
                 "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
                {"RFC 4231 case 2",
                 {'J', 'e', 'f', 'e'},
                 {'w', 'h', 'a', 't', ' ', 'd', 'o', ' ', 'y', 'a', ' ', 'w', 'a', 'n', 't', ' ', 'f',
                  'o', 'r', ' ', 'n', 'o', 't', 'h', 'i', 'n', 'g', '?'},
                 "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
                {"RFC 4231 case 3",
                 std::vector<uint8_t>(20, 0xaa),
                 std::vector<uint8_t>(50, 0xdd),
                 "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"},
                {"RFC 4231 case 6 (key longer than the block size)",
                 std::vector<uint8_t>(131, 0xaa),
                 std::vector<uint8_t>(),
                 "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"},
                {"RFC 4231 case 7 (long key and long data)",
                 std::vector<uint8_t>(131, 0xaa),
                 std::vector<uint8_t>(),
                 "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"}};

            /* Case 4 uses the key 0x01..0x19. */
            std::vector<uint8_t> case4Key;

            for (uint8_t i = 1; i <= 25; i++)
            {
                case4Key.push_back(i);
            }

            for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
            {
                std::vector<uint8_t> data = cases[i].data;

                if (cases[i].name == "RFC 4231 case 6 (key longer than the block size)")
                {
                    const std::string text = "Test Using Larger Than Block-Size Key - Hash Key First";
                    data.assign(text.begin(), text.end());
                }
                else if (cases[i].name == "RFC 4231 case 7 (long key and long data)")
                {
                    const std::string text = "This is a test using a larger than block-size key and a larger than "
                                             "block-size data. The key needs to be hashed before being used by the "
                                             "HMAC algorithm.";
                    data.assign(text.begin(), text.end());
                }

                uint8_t mac[SHA256_DIGEST_SIZE];

                hmac_sha256(cases[i].key.data(), cases[i].key.size(), data.data(), data.size(), mac);

                check(cases[i].name, cases[i].expected, toHex(mac, sizeof(mac)));
            }

            {
                uint8_t mac[SHA256_DIGEST_SIZE];
                const std::vector<uint8_t> data(50, 0xcd);

                hmac_sha256(case4Key.data(), case4Key.size(), data.data(), data.size(), mac);

                check("RFC 4231 case 4",
                      "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
                      toHex(mac, sizeof(mac)));
            }
        }

        void testPbkdf2()
        {
            struct
            {
                std::string password;
                std::string salt;
                uint64_t iterations;
                size_t length;
                std::string expected;
            } const cases[] = {
                /* The widely published PBKDF2-HMAC-SHA256 vectors; the first
                   four also appear in RFC 7914 section 11. */
                {"password", "salt", 1, 32, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
                {"password", "salt", 2, 32, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
                {"password", "salt", 4096, 32, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"},
                {"passwordPASSWORDpassword",
                 "saltSALTsaltSALTsaltSALTsaltSALTsalt",
                 4096,
                 40,
                 "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9"}};

            for (const auto &c : cases)
            {
                std::vector<uint8_t> out(c.length);

                pbkdf2_hmac_sha256(
                    reinterpret_cast<const uint8_t *>(c.password.data()),
                    c.password.size(),
                    reinterpret_cast<const uint8_t *>(c.salt.data()),
                    c.salt.size(),
                    c.iterations,
                    out.data(),
                    out.size());

                check("PBKDF2-HMAC-SHA256 c=" + std::to_string(c.iterations) + " dkLen="
                          + std::to_string(c.length),
                      c.expected,
                      toHex(out));
            }

            /* Embedded NUL bytes in both password and salt. std::string keeps
               these; a naive strlen-based implementation would not. */
            {
                const std::string password("pass\0word", 9);
                const std::string salt("sa\0lt", 5);

                std::vector<uint8_t> out(16);

                pbkdf2_hmac_sha256(
                    reinterpret_cast<const uint8_t *>(password.data()),
                    password.size(),
                    reinterpret_cast<const uint8_t *>(salt.data()),
                    salt.size(),
                    4096,
                    out.data(),
                    out.size());

                check("PBKDF2-HMAC-SHA256 with embedded NULs", "89b69d0516f829893c696226650a8687", toHex(out));
            }

            /* An output longer than one SHA-256 block exercises the block
               counter; deriving 64 bytes must start with the 32 byte answer. */
            {
                std::vector<uint8_t> shortOut(32);
                std::vector<uint8_t> longOut(64);

                const std::string password = "password";
                const std::string salt = "salt";

                pbkdf2_hmac_sha256(
                    reinterpret_cast<const uint8_t *>(password.data()),
                    password.size(),
                    reinterpret_cast<const uint8_t *>(salt.data()),
                    salt.size(),
                    4096,
                    shortOut.data(),
                    shortOut.size());

                pbkdf2_hmac_sha256(
                    reinterpret_cast<const uint8_t *>(password.data()),
                    password.size(),
                    reinterpret_cast<const uint8_t *>(salt.data()),
                    salt.size(),
                    4096,
                    longOut.data(),
                    longOut.size());

                check("PBKDF2 multi-block prefix",
                      toHex(shortOut),
                      toHex(longOut.data(), shortOut.size()));
            }
        }

        void testAesCbc()
        {
            /* NIST SP 800-38A F.2.1 (encrypt) and F.2.2 (decrypt). Those
               vectors are unpadded, so we compare the leading 64 bytes and
               ignore the extra PKCS#7 block our API always appends. */
            const std::vector<uint8_t> key = fromHex("2b7e151628aed2a6abf7158809cf4f3c");
            const std::vector<uint8_t> iv = fromHex("000102030405060708090a0b0c0d0e0f");

            const std::vector<uint8_t> plaintext = fromHex(
                "6bc1bee22e409f96e93d7e117393172a"
                "ae2d8a571e03ac9c9eb76fac45af8e51"
                "30c81c46a35ce411e5fbc1191a0a52ef"
                "f69f2445df4f9b17ad2b417be66c3710");

            const std::string expected = "7649abac8119b246cee98e9b12e9197d"
                                         "5086cb9b507219ee95db113a917678b2"
                                         "73bed6b8e3c1743b7116e69e22229516"
                                         "3ff1caa1681fac09120eca307586e1a7";

            std::vector<uint8_t> out(plaintext.size() + AES_CBC_BLOCK_SIZE);

            const size_t written =
                aes128_cbc_encrypt(key.data(), iv.data(), plaintext.data(), plaintext.size(), out.data());

            if (written != plaintext.size() + AES_CBC_BLOCK_SIZE)
            {
                fail("AES-128-CBC output length",
                     std::to_string(plaintext.size() + AES_CBC_BLOCK_SIZE),
                     std::to_string(written));
            }

            check("AES-128-CBC NIST SP 800-38A F.2.1", expected, toHex(out.data(), plaintext.size()));

            /* Round trip back through the decrypt path. */
            std::vector<uint8_t> back(written);
            size_t backLength = 0;

            if (!aes128_cbc_decrypt(key.data(), iv.data(), out.data(), written, back.data(), &backLength))
            {
                fail("AES-128-CBC decrypt of our own ciphertext", "success", "padding rejected");
            }

            check("AES-128-CBC round trip", toHex(plaintext), toHex(back.data(), backLength));
        }

        void testPkcs7EdgeCases()
        {
            const uint8_t key[16] = {0};
            const uint8_t iv[16] = {0};

            /* Every length from 0 to 33 bytes, so both the "already a whole
               block" case (which must add a full block of padding) and every
               partial-block case are covered. */
            for (size_t length = 0; length <= 33; length++)
            {
                const std::string plaintext(length, static_cast<char>('a' + (length % 26)));

                const std::string ciphertext = WalletCrypto::encrypt(plaintext, key, iv);

                const size_t expectedLength = length + AES_CBC_BLOCK_SIZE - (length % AES_CBC_BLOCK_SIZE);

                if (ciphertext.size() != expectedLength)
                {
                    fail("PKCS#7 ciphertext length for a " + std::to_string(length) + " byte input",
                         std::to_string(expectedLength),
                         std::to_string(ciphertext.size()));
                }

                const auto recovered = WalletCrypto::decrypt(ciphertext, key, iv);

                if (!recovered)
                {
                    fail("PKCS#7 round trip for a " + std::to_string(length) + " byte input",
                         "success",
                         "padding rejected");
                }

                if (*recovered != plaintext)
                {
                    fail("PKCS#7 round trip for a " + std::to_string(length) + " byte input",
                         plaintext,
                         *recovered);
                }
            }

            /* Malformed inputs must be rejected rather than returning garbage. */
            if (WalletCrypto::decrypt("", key, iv))
            {
                fail("empty ciphertext", "rejected", "accepted");
            }

            if (WalletCrypto::decrypt(std::string(17, '\0'), key, iv))
            {
                fail("ciphertext that is not a whole number of blocks", "rejected", "accepted");
            }

            /* Flip a byte in the last block: the padding should not verify. A
               correct implementation rejects the overwhelming majority of
               these, and the caller reports a wrong password either way. */
            {
                std::string ciphertext = WalletCrypto::encrypt("some wallet json goes here", key, iv);

                ciphertext[ciphertext.size() - 1] ^= 0xff;
                ciphertext[ciphertext.size() - 2] ^= 0xff;

                if (WalletCrypto::decrypt(ciphertext, key, iv))
                {
                    fail("ciphertext with corrupted padding", "rejected", "accepted");
                }
            }
        }

        void testWalletShapedRoundTrip()
        {
            /* Exactly what WalletBackend does: derive a 16 byte key from the
               password with the 16 byte salt, then encrypt using that same
               salt as the IV. */
            const std::string password = "correct horse battery staple";

            uint8_t salt[WalletCrypto::SALT_SIZE];

            for (size_t i = 0; i < sizeof(salt); i++)
            {
                salt[i] = static_cast<uint8_t>(i * 7 + 3);
            }

            const std::string walletJson = "{\"walletFileFormatVersion\":0,\"subWallets\":[]}";

            const auto key = WalletCrypto::deriveKey(password, salt, sizeof(salt), 10000, WalletCrypto::KEY_SIZE);

            const std::string ciphertext = WalletCrypto::encrypt(walletJson, key.data(), salt);

            const auto recovered = WalletCrypto::decrypt(ciphertext, key.data(), salt);

            if (!recovered || *recovered != walletJson)
            {
                fail("wallet shaped round trip", walletJson, recovered ? *recovered : "<rejected>");
            }

            /* A wrong password must not decrypt. */
            const auto wrongKey =
                WalletCrypto::deriveKey(password + "!", salt, sizeof(salt), 10000, WalletCrypto::KEY_SIZE);

            const auto shouldFail = WalletCrypto::decrypt(ciphertext, wrongKey.data(), salt);

            if (shouldFail && *shouldFail == walletJson)
            {
                fail("wrong password", "rejected", "decrypted the wallet");
            }
        }
    } // namespace

    void runAll()
    {
        std::cout << std::endl << "Test Wallet Crypto Primitives" << std::endl << std::endl;

        std::cout << "  SHA-256 (FIPS 180-4)... " << std::flush;
        testSha256();
        std::cout << "PASSED" << std::endl;

        std::cout << "  HMAC-SHA256 (RFC 4231)... " << std::flush;
        testHmacSha256();
        std::cout << "PASSED" << std::endl;

        std::cout << "  PBKDF2-HMAC-SHA256 (RFC 7914)... " << std::flush;
        testPbkdf2();
        std::cout << "PASSED" << std::endl;

        std::cout << "  AES-128-CBC (NIST SP 800-38A)... " << std::flush;
        testAesCbc();
        std::cout << "PASSED" << std::endl;

        std::cout << "  PKCS#7 padding edge cases... " << std::flush;
        testPkcs7EdgeCases();
        std::cout << "PASSED" << std::endl;

        std::cout << "  Wallet shaped round trip... " << std::flush;
        testWalletShapedRoundTrip();
        std::cout << "PASSED" << std::endl;
    }
} // namespace WalletCryptoTests
