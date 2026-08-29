// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* Out-of-line nlohmann::json (de)serialisers for the basic types declared in
   CryptoTypes.h and CryptoNote.h. Keeping the bodies here lets those headers
   include json_fwd.hpp instead of the full json.hpp, which roughly every
   translation unit in the tree would otherwise parse. */

#include "json.hpp"

#include <CryptoNote.h>
#include <CryptoTypes.h>
#include <common/StringTools.h>
#include <string>
#include <vector>

namespace
{
    [[noreturn]] void throwHexParseError()
    {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) && defined(NLOHMANN_JSON_VERSION_MINOR) \
    && ((NLOHMANN_JSON_VERSION_MAJOR > 3) || (NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR >= 12))
        throw nlohmann::detail::parse_error::create(100, 0, "Wrong length, or neither hex nor base64!", nullptr);
#else
        throw nlohmann::detail::parse_error::create(100, 0, "Wrong length, or neither hex nor base64!");
#endif
    }

    /* Accepts either encoding, told apart by length alone: a fixed size value
       is 2N characters as hex and about 1.33N as base64, which can never
       collide. Doing it here means a peer that sends the shorter form needs no
       flag threaded through every enclosing structure's deserialiser. */
    template<typename T> void podFromJson(const nlohmann::json &j, T &value)
    {
        const std::string text = j.get<std::string>();

        constexpr size_t byteCount = sizeof(value.data);
        constexpr size_t hexLength = byteCount * 2;
        constexpr size_t base64Length = ((byteCount + 2) / 3) * 4;

        if (text.size() == hexLength)
        {
            if (Common::podFromHex(text, value.data))
            {
                return;
            }
        }
        else if (text.size() == base64Length)
        {
            if (Common::podFromBase64(text, value.data))
            {
                return;
            }
        }

        throwHexParseError();
    }
} // namespace

namespace Crypto
{
    void to_json(nlohmann::json &j, const Hash &h)
    {
        j = Common::podToHex(h);
    }

    void from_json(const nlohmann::json &j, Hash &h)
    {
        podFromJson(j, h);
    }

    void to_json(nlohmann::json &j, const PublicKey &p)
    {
        j = Common::podToHex(p);
    }

    void from_json(const nlohmann::json &j, PublicKey &p)
    {
        podFromJson(j, p);
    }

    void to_json(nlohmann::json &j, const SecretKey &s)
    {
        j = Common::podToHex(s);
    }

    void from_json(const nlohmann::json &j, SecretKey &s)
    {
        podFromJson(j, s);
    }

    void to_json(nlohmann::json &j, const KeyDerivation &k)
    {
        j = Common::podToHex(k);
    }

    void from_json(const nlohmann::json &j, KeyDerivation &k)
    {
        podFromJson(j, k);
    }

    void to_json(nlohmann::json &j, const KeyImage &k)
    {
        j = Common::podToHex(k);
    }

    void from_json(const nlohmann::json &j, KeyImage &k)
    {
        podFromJson(j, k);
    }
} // namespace Crypto

namespace CryptoNote
{
    void to_json(nlohmann::json &j, const CryptoNote::KeyInput &k)
    {
        j = {{"amount", k.amount}, {"key_offsets", k.outputIndexes}, {"k_image", k.keyImage}};
    }

    void from_json(const nlohmann::json &j, CryptoNote::KeyInput &k)
    {
        k.amount = j.at("amount").get<uint64_t>();
        if (j.find("key_offsets") != j.end())
        {
            k.outputIndexes = j.at("key_offsets").get<std::vector<uint32_t>>();
        }
        k.keyImage = j.at("k_image").get<Crypto::KeyImage>();
    }

    void to_json(nlohmann::json &j, const CryptoNote::RawBlock &block)
    {
        std::vector<std::string> transactions;

        for (const auto &transaction : block.transactions)
        {
            transactions.push_back(Common::toHex(transaction));
        }

        j = {{"block", Common::toHex(block.block)}, {"transactions", transactions}};
    }

    void from_json(const nlohmann::json &j, CryptoNote::RawBlock &block)
    {
        block.transactions.clear();

        std::string blockString = j.at("block").get<std::string>();

        block.block = Common::fromHex(blockString);

        std::vector<std::string> transactions = j.at("transactions").get<std::vector<std::string>>();

        for (const auto &transaction : transactions)
        {
            block.transactions.push_back(Common::fromHex(transaction));
        }
    }
} // namespace CryptoNote
