// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <crypto/hash.h>
#include <logging/ILogger.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace CryptoNote
{
    class Core;
    class NodeServer;
} // namespace CryptoNote

namespace Simnet
{
    struct NodeOptions
    {
        /* Where the node keeps its chain. Created when missing. A directory
           that holds a mainnet chain is refused (settleNetworkProfile). */
        std::string dataDir;

        std::string bindIp = "127.0.0.1";

        uint16_t p2pPort = 0;

        uint16_t rpcPort = 0;

        /* The only peers this node dials, as ip:port. Nodes of a cluster know
           one another through these alone, so a peer list learned from one
           node can never join two that were meant to stay apart. */
        std::vector<std::string> exclusiveNodes;

        /* Serve GET /ws on the RPC. */
        bool websocket = true;

        /* --enable-cors for the RPC, so a browser wallet can use it. */
        std::string cors;

        uint32_t validationThreads = 2;

        /* Tests only: shake hands as another network - mainnet's, say - to
           show that a simnet turns it away. */
        std::optional<std::array<uint8_t, 16>> networkId;
    };

    /* One whole simnet node inside this process: the chain, the pool, the P2P
       engine, the RPC and its WebSocket stream, assembled as Wrkzd --simnet
       assembles them, on a thread of its own with its own dispatcher. */
    class Node
    {
      public:
        Node(const size_t index, NodeOptions options, std::shared_ptr<Logging::ILogger> logger);

        ~Node();

        Node(const Node &) = delete;

        Node &operator=(const Node &) = delete;

        /* Starts the node and returns once its RPC is being served, or with
           the reason it could not start. */
        bool start(std::string &error);

        /* Stops the node and waits for it to close its database. */
        void stop();

        size_t index() const;

        const NodeOptions &options() const;

        std::string p2pAddress() const;

        std::string rpcUrl() const;

        /* Empty when the node does not serve GET /ws. */
        std::string wsUrl() const;

        /* The top block's index, and its hash. Zero while not running. */
        uint32_t topIndex() const;

        Crypto::Hash topHash() const;

        /* Open P2P connections, either direction. */
        size_t connections() const;

      private:
        enum class State
        {
            Created,
            Starting,
            Running,
            Stopping,
            Stopped,
            Failed,
        };

        void run();

        const size_t m_index;

        const NodeOptions m_options;

        std::shared_ptr<Logging::ILogger> m_logger;

        std::thread m_thread;

        mutable std::mutex m_mutex;

        std::condition_variable m_stateChanged;

        State m_state = State::Created;

        std::string m_error;

        /* Set only while Running, and cleared before any teardown starts, so
           a query from another thread never reaches a closing database. */
        std::shared_ptr<CryptoNote::Core> m_core;

        std::shared_ptr<CryptoNote::NodeServer> m_p2p;
    };

    /* A TCP port on 127.0.0.1 nothing is listening on right now. Another
       process may still take it before it is used; good enough for tests. */
    uint16_t pickFreePort();
} // namespace Simnet
