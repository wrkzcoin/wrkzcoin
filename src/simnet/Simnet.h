// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "SimnetNode.h"

#include <chrono>
#include <crypto/crypto.h>
#include <memory>
#include <string>
#include <vector>

namespace httplib
{
    class Client;
}

namespace Simnet
{
    /* A fresh wallet to mine to. The view key is derived from the spend key,
       as every wallet here does, so the mnemonic alone restores it. */
    struct Keys
    {
        Crypto::SecretKey spendKey;

        Crypto::SecretKey viewKey;

        std::string address;

        std::string mnemonic;

        static Keys random();
    };

    /* A simnet daemon's RPC, from anywhere: a node of this process or a
       Wrkzd --simnet elsewhere. */
    class Client
    {
      public:
        /* http://host:port */
        explicit Client(const std::string &url);

        ~Client();

        /* A block template paying address; height is the new block's index. */
        bool blockTemplate(const std::string &address, std::string &blob, uint64_t &height, std::string &error);

        /* Submits a block. A template submitted unchanged is accepted by a
           simnet and refused by mainnet, which wants proof of work. */
        bool submitBlock(const std::string &blob, std::string &error);

        /* Both of the above. */
        bool mine(const std::string &address, uint64_t &height, std::string &error);

      private:
        bool jsonRpc(const std::string &method, const std::string &params, std::string &result, std::string &error);

        std::string m_url;

        std::unique_ptr<httplib::Client> m_client;
    };

    enum class Topology
    {
        /* 0 - 1 - 2 - ... */
        Line,
        /* a line whose ends are joined too */
        Ring,
        /* every node to every other */
        Mesh,
    };

    struct ClusterOptions
    {
        size_t nodes = 3;

        Topology topology = Topology::Line;

        /* Node i keeps its chain in <dataDir>/node<i>. */
        std::string dataDir;

        /* Node i listens on base + i. 0 picks free ports. */
        uint16_t p2pBasePort = 0;

        uint16_t rpcBasePort = 0;

        bool websocket = true;

        std::string cors;
    };

    /* Simnet nodes in this process, joined in a topology. Nodes dial only the
       neighbours the topology gives them, as exclusive nodes, so an address
       one of them learns from a peer list can never join two nodes the
       topology keeps apart. */
    class Cluster
    {
      public:
        explicit Cluster(std::shared_ptr<Logging::ILogger> logger);

        ~Cluster();

        bool start(const ClusterOptions &options, std::string &error);

        void stop();

        size_t size() const;

        Node &node(const size_t index);

        /* Waits until every node holds at least this many connections. */
        bool waitForConnections(const size_t perNode, const std::chrono::seconds timeout) const;

        /* Waits until the listed nodes stand on the same tip, and says which. */
        bool waitForAgreement(
            const std::vector<size_t> &nodes,
            const std::chrono::seconds timeout,
            uint32_t &topIndex,
            Crypto::Hash &topHash) const;

      private:
        std::shared_ptr<Logging::ILogger> m_logger;

        std::vector<std::unique_ptr<Node>> m_nodes;
    };

    /* The neighbours each node of an n-node topology dials. */
    std::vector<std::vector<size_t>> links(const Topology topology, const size_t nodes);
} // namespace Simnet
