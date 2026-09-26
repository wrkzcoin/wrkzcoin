// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include "Simnet.h"

#include "httplib.h"
//////////////////////////

/* windows.h, which httplib brings in, defines ERROR - which the logging
   headers below need as Logging::ERROR. */
#undef ERROR

#include "json.hpp"

#include <algorithm>
#include <common/FileSystemShim.h>
#include <mnemonics/Mnemonics.h>
#include <thread>
#include <utilities/Addresses.h>

namespace Simnet
{
    Keys Keys::random()
    {
        Keys keys;

        Crypto::PublicKey publicSpendKey;
        Crypto::generate_keys(publicSpendKey, keys.spendKey);
        Crypto::crypto_ops::generateViewFromSpend(keys.spendKey, keys.viewKey);

        keys.address = Utilities::privateKeysToAddress(keys.spendKey, keys.viewKey);
        keys.mnemonic = Mnemonics::PrivateKeyToMnemonic(keys.spendKey);

        return keys;
    }

    Client::Client(const std::string &url): m_url(url)
    {
        m_client = std::make_unique<httplib::Client>(url);
        m_client->set_connection_timeout(std::chrono::seconds(5));
        m_client->set_read_timeout(std::chrono::seconds(30));
    }

    Client::~Client() = default;

    bool Client::jsonRpc(const std::string &method, const std::string &params, std::string &result, std::string &error)
    {
        const std::string body =
            "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"" + method + "\",\"params\":" + params + "}";

        const auto res = m_client->Post("/json_rpc", body, "application/json");

        if (!res)
        {
            error = m_url + " did not answer (" + httplib::to_string(res.error()) + ")";
            return false;
        }

        if (res->status != 200)
        {
            error = m_url + " answered " + std::to_string(res->status) + ": " + res->body;
            return false;
        }

        const auto j = nlohmann::json::parse(res->body, nullptr, false);

        if (j.is_discarded() || !j.is_object())
        {
            error = m_url + " answered something that is not JSON";
            return false;
        }

        if (j.contains("error"))
        {
            error = j["error"].value("message", std::string("unknown error"));
            return false;
        }

        if (!j.contains("result"))
        {
            error = m_url + " answered without a result";
            return false;
        }

        result = j["result"].dump();
        return true;
    }

    bool Client::blockTemplate(const std::string &address, std::string &blob, uint64_t &height, std::string &error)
    {
        std::string result;

        const nlohmann::json templateParams = {{"wallet_address", address}, {"reserve_size", 0}};

        if (!jsonRpc("getblocktemplate", templateParams.dump(), result, error))
        {
            error = "getblocktemplate: " + error;
            return false;
        }

        const auto answer = nlohmann::json::parse(result, nullptr, false);

        if (answer.is_discarded() || !answer.contains("blocktemplate_blob") || !answer["blocktemplate_blob"].is_string())
        {
            error = "getblocktemplate: no blocktemplate_blob in the answer";
            return false;
        }

        blob = answer["blocktemplate_blob"].get<std::string>();
        height = answer.value("height", uint64_t(0));
        return true;
    }

    bool Client::submitBlock(const std::string &blob, std::string &error)
    {
        std::string result;

        if (!jsonRpc("submitblock", nlohmann::json::array({blob}).dump(), result, error))
        {
            error = "submitblock: " + error + ". Is " + m_url
                    + " a Wrkzd --simnet? A mainnet daemon wants proof of work.";
            return false;
        }

        return true;
    }

    bool Client::mine(const std::string &address, uint64_t &height, std::string &error)
    {
        std::string blob;

        /* Submitted as it came: a simnet asks for no work. */
        return blockTemplate(address, blob, height, error) && submitBlock(blob, error);
    }

    std::vector<std::vector<size_t>> links(const Topology topology, const size_t nodes)
    {
        std::vector<std::vector<size_t>> neighbours(nodes);

        const auto join = [&neighbours](const size_t a, const size_t b) {
            if (a == b)
            {
                return;
            }

            neighbours[a].push_back(b);
            neighbours[b].push_back(a);
        };

        if (topology == Topology::Mesh)
        {
            for (size_t i = 0; i < nodes; i++)
            {
                for (size_t j = i + 1; j < nodes; j++)
                {
                    join(i, j);
                }
            }

            return neighbours;
        }

        for (size_t i = 1; i < nodes; i++)
        {
            join(i - 1, i);
        }

        /* Two nodes are already joined, and three in a ring are a mesh. */
        if (topology == Topology::Ring && nodes > 2)
        {
            join(nodes - 1, 0);
        }

        return neighbours;
    }

    Cluster::Cluster(std::shared_ptr<Logging::ILogger> logger): m_logger(std::move(logger)) {}

    Cluster::~Cluster()
    {
        stop();
    }

    bool Cluster::start(const ClusterOptions &options, std::string &error)
    {
        if (options.nodes == 0)
        {
            error = "a cluster needs at least one node";
            return false;
        }

        std::vector<uint16_t> p2pPorts;
        std::vector<uint16_t> rpcPorts;

        for (size_t i = 0; i < options.nodes; i++)
        {
            const uint16_t p2pPort =
                options.p2pBasePort == 0 ? pickFreePort() : static_cast<uint16_t>(options.p2pBasePort + i);

            const uint16_t rpcPort =
                options.rpcBasePort == 0 ? pickFreePort() : static_cast<uint16_t>(options.rpcBasePort + i);

            if (p2pPort == 0 || rpcPort == 0)
            {
                error = "could not find a free port on 127.0.0.1";
                return false;
            }

            p2pPorts.push_back(p2pPort);
            rpcPorts.push_back(rpcPort);
        }

        const auto neighbours = links(options.topology, options.nodes);

        for (size_t i = 0; i < options.nodes; i++)
        {
            NodeOptions nodeOptions;
            nodeOptions.dataDir = (fs::path(options.dataDir) / ("node" + std::to_string(i))).string();
            nodeOptions.p2pPort = p2pPorts[i];
            nodeOptions.rpcPort = rpcPorts[i];
            nodeOptions.websocket = options.websocket;
            nodeOptions.cors = options.cors;

            for (const size_t neighbour : neighbours[i])
            {
                nodeOptions.exclusiveNodes.push_back("127.0.0.1:" + std::to_string(p2pPorts[neighbour]));
            }

            auto node = std::make_unique<Node>(i, nodeOptions, m_logger);

            std::string nodeError;

            if (!node->start(nodeError))
            {
                error = "node " + std::to_string(i) + ": " + nodeError;
                stop();
                return false;
            }

            m_nodes.push_back(std::move(node));
        }

        return true;
    }

    void Cluster::stop()
    {
        /* Asked to stop together, so no node sits out a timeout waiting on a
           neighbour that has already gone. */
        std::vector<std::thread> stopping;

        for (auto &node : m_nodes)
        {
            stopping.emplace_back([&node] { node->stop(); });
        }

        for (auto &thread : stopping)
        {
            thread.join();
        }

        m_nodes.clear();
    }

    size_t Cluster::size() const
    {
        return m_nodes.size();
    }

    Node &Cluster::node(const size_t index)
    {
        return *m_nodes.at(index);
    }

    bool Cluster::waitForConnections(const size_t perNode, const std::chrono::seconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const bool all = std::all_of(m_nodes.begin(), m_nodes.end(), [perNode](const auto &node) {
                return node->connections() >= perNode;
            });

            if (all)
            {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return false;
    }

    bool Cluster::waitForAgreement(
        const std::vector<size_t> &nodes,
        const std::chrono::seconds timeout,
        uint32_t &topIndex,
        Crypto::Hash &topHash) const
    {
        if (nodes.empty())
        {
            return false;
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            bool agreed = true;

            const Crypto::Hash first = m_nodes.at(nodes.front())->topHash();

            for (const size_t index : nodes)
            {
                agreed = agreed && m_nodes.at(index)->topHash() == first;
            }

            if (agreed)
            {
                topIndex = m_nodes.at(nodes.front())->topIndex();
                topHash = first;
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return false;
    }
} // namespace Simnet
