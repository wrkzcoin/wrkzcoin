// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteFormatUtils.h"

#include <algorithm>
#include <common/TransactionExtra.h>
#include <variant>

namespace CryptoNote
{
    class TransactionExtra
    {
      public:
        TransactionExtra() {}

        TransactionExtra(const std::vector<uint8_t> &extra)
        {
            parse(extra);
        }

        bool parse(const std::vector<uint8_t> &extra)
        {
            fields.clear();
            return CryptoNote::parseTransactionExtra(extra, fields);
        }

        template<typename T> bool get(T &value) const
        {
            auto it = find<T>();
            if (it == fields.end())
            {
                return false;
            }
            value = std::get<T>(*it);
            return true;
        }

        template<typename T> void set(const T &value)
        {
            auto it = find<T>();
            if (it != fields.end())
            {
                *it = value;
            }
            else
            {
                fields.push_back(value);
            }
        }

        template<typename T> void append(const T &value)
        {
            fields.push_back(value);
        }

        bool getPublicKey(Crypto::PublicKey &pk) const
        {
            CryptoNote::TransactionExtraPublicKey extraPk;
            if (!get(extraPk))
            {
                return false;
            }
            pk = extraPk.publicKey;
            return true;
        }

        std::vector<uint8_t> serialize() const
        {
            std::vector<uint8_t> extra;
            writeTransactionExtra(extra, fields);
            return extra;
        }

      private:
        template<typename T> std::vector<CryptoNote::TransactionExtraField>::const_iterator find() const
        {
            return std::find_if(fields.begin(), fields.end(), [](const CryptoNote::TransactionExtraField &f) {
                return std::holds_alternative<T>(f);
            });
        }

        template<typename T> std::vector<CryptoNote::TransactionExtraField>::iterator find()
        {
            return std::find_if(fields.begin(), fields.end(), [](const CryptoNote::TransactionExtraField &f) {
                return std::holds_alternative<T>(f);
            });
        }

        std::vector<CryptoNote::TransactionExtraField> fields;
    };

} // namespace CryptoNote
