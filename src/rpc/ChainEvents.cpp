// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ChainEvents.h"

#include <common/CryptoNoteTools.h>
#include <sstream>

namespace ChainEvents
{
    namespace
    {
        const char *deleteReasonToString(const CryptoNote::Messages::DeleteTransaction::Reason reason)
        {
            switch (reason)
            {
                case CryptoNote::Messages::DeleteTransaction::Reason::InBlock:
                    return "InBlock";
                case CryptoNote::Messages::DeleteTransaction::Reason::Outdated:
                    return "Outdated";
                case CryptoNote::Messages::DeleteTransaction::Reason::NotActual:
                    return "NotActual";
            }

            return "Unknown";
        }
    } // namespace

    const std::vector<std::string> &topics()
    {
        static const std::vector<std::string> all = {
            "hashblock", "chain_main", "hashblock_alt", "chainswitch", "txpool_add", "txpool_del"};

        return all;
    }

    std::vector<Message> describe(
        const CryptoNote::BlockchainMessage &message,
        const CryptoNote::ICore &core,
        const uint32_t liteHeight,
        Logging::LoggerRef &logger)
    {
        std::vector<Message> messages;

        message.match(
            [&](const CryptoNote::Messages::NewBlock &m) {
                std::ostringstream body;
                body << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash) << "\"}";
                messages.emplace_back("hashblock", body.str());

                /* A lite node stores no body for blocks below its lite height, so
                   the transaction hash list this payload carries cannot be built
                   for them. Catching the read failure per block would work but
                   would warn once for every block of the initial sync. */
                if (liteHeight != 0 && m.blockIndex < liteHeight)
                {
                    return;
                }

                try
                {
                    const auto block = core.getBlockByHash(m.blockHash);
                    std::vector<Crypto::Hash> transactionHashes;
                    transactionHashes.reserve(block.transactionHashes.size() + 1);
                    transactionHashes.push_back(CryptoNote::getObjectHash(block.baseTransaction));
                    transactionHashes.insert(
                        transactionHashes.end(), block.transactionHashes.begin(), block.transactionHashes.end());

                    std::ostringstream prefetchBody;
                    prefetchBody << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash)
                                 << "\",\"transaction_hashes\":" << hashesToJsonArray(transactionHashes) << "}";
                    messages.emplace_back("chain_main", prefetchBody.str());
                }
                catch (const std::exception &e)
                {
                    logger(Logging::WARNING) << "Failed to build chain_main prefetch payload for block "
                                             << hashToString(m.blockHash) << ": " << e.what();
                }
            },
            [&](const CryptoNote::Messages::NewAlternativeBlock &m) {
                std::ostringstream body;
                body << "{\"height\":" << m.blockIndex << ",\"hash\":\"" << hashToString(m.blockHash) << "\"}";
                messages.emplace_back("hashblock_alt", body.str());
            },
            [&](const CryptoNote::Messages::ChainSwitch &m) {
                std::ostringstream body;
                body << "{\"common_root_height\":" << m.commonRootIndex
                     << ",\"hashes\":" << hashesToJsonArray(m.blocksFromCommonRoot) << "}";
                messages.emplace_back("chainswitch", body.str());
            },
            [&](const CryptoNote::Messages::AddTransaction &m) {
                std::ostringstream body;
                body << "{\"hashes\":" << hashesToJsonArray(m.hashes) << "}";
                messages.emplace_back("txpool_add", body.str());
            },
            [&](const CryptoNote::Messages::DeleteTransaction &m) {
                std::ostringstream body;
                body << "{\"hashes\":" << hashesToJsonArray(m.hashes) << ",\"reason\":\""
                     << deleteReasonToString(m.reason) << "\"}";
                messages.emplace_back("txpool_del", body.str());
            });

        return messages;
    }

    std::string hello(const uint32_t topIndex, const Crypto::Hash &topHash, const std::vector<std::string> &topics)
    {
        std::ostringstream body;
        body << "{\"height\":" << topIndex << ",\"hash\":\"" << hashToString(topHash) << "\",\"topics\":[";

        for (size_t i = 0; i < topics.size(); ++i)
        {
            if (i != 0)
            {
                body << ",";
            }

            body << "\"" << topics[i] << "\"";
        }

        body << "]}";
        return body.str();
    }

    std::string hashToString(const Crypto::Hash &hash)
    {
        std::ostringstream out;
        out << hash;
        return out.str();
    }

    std::string hashesToJsonArray(const std::vector<Crypto::Hash> &hashes)
    {
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < hashes.size(); ++i)
        {
            if (i != 0)
            {
                out << ",";
            }

            out << "\"" << hashToString(hashes[i]) << "\"";
        }
        out << "]";
        return out.str();
    }
} // namespace ChainEvents
