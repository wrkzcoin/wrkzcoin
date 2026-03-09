// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "NetworkParameters.h"

#include <config/CryptoNoteConfig.h>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace CryptoNote
{
    namespace
    {
        std::vector<std::string> buildMainnetSeedNodes()
        {
            std::vector<std::string> nodes;
            for (const auto *seed : SEED_NODES)
            {
                nodes.emplace_back(seed);
            }
            return nodes;
        }

        const NetworkParameters MAINNET_PARAMETERS = {
            "mainnet",
            P2P_DEFAULT_PORT,
            RPC_DEFAULT_PORT,
            ZMQ_PUB_DEFAULT_PORT,
            parameters::P2P_NET_DATA_FILENAME,
            "",
            CRYPTONOTE_NETWORK,
            buildMainnetSeedNodes()};

        const NetworkParameters TESTNET_PARAMETERS = {
            "testnet",
            27855,
            27856,
            27857,
            "p2pstate.wrkz.testnet.bin",
            "testnet",
            {{0xc4, 0x4d, 0x78, 0xf2, 0x7a, 0xb3, 0x42, 0x4f, 0x9d, 0x56, 0x7f, 0x49, 0xe3, 0x85, 0x11, 0x2a}},
            {}};
    } // namespace

    const NetworkParameters &getNetworkParameters(NetworkType networkType)
    {
        switch (networkType)
        {
            case NetworkType::Mainnet:
                return MAINNET_PARAMETERS;
            case NetworkType::Testnet:
                return TESTNET_PARAMETERS;
            default:
                throw std::invalid_argument("Unknown network type");
        }
    }

    bool parseNetworkType(const std::string &value, NetworkType &networkType)
    {
        std::string normalized = value;
        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (normalized == "mainnet")
        {
            networkType = NetworkType::Mainnet;
            return true;
        }

        if (normalized == "testnet")
        {
            networkType = NetworkType::Testnet;
            return true;
        }

        return false;
    }

    const char *networkTypeToString(NetworkType networkType)
    {
        switch (networkType)
        {
            case NetworkType::Mainnet:
                return "mainnet";
            case NetworkType::Testnet:
                return "testnet";
            default:
                return "unknown";
        }
    }
} // namespace CryptoNote
