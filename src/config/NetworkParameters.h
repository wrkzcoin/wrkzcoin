// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <boost/uuid/uuid.hpp>
#include <string>
#include <vector>

namespace CryptoNote
{
    enum class NetworkType
    {
        Mainnet,
        Testnet
    };

    struct NetworkParameters
    {
        std::string name;
        int p2pPort;
        int rpcPort;
        int zmqPubPort;
        std::string p2pStateFilename;
        std::string dataDirectorySuffix;
        boost::uuids::uuid networkId;
        std::vector<std::string> seedNodes;
    };

    const NetworkParameters &getNetworkParameters(NetworkType networkType);

    bool parseNetworkType(const std::string &value, NetworkType &networkType);

    const char *networkTypeToString(NetworkType networkType);
} // namespace CryptoNote
