// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "Probe.h"

#include "httplib.h"
#include "json.hpp"

#include <chrono>
#include <common/StringTools.h>
#include <config/CryptoNoteCheckpoints.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/random.h>
#include <p2p/LevinProtocol.h>
#include <p2p/P2pProtocolDefinitions.h>
#include <system/Context.h>
#include <system/Dispatcher.h>
#include <system/TcpConnection.h>
#include <system/TcpConnector.h>
#include <system/Timer.h>

namespace NetMon
{
    namespace
    {
        template<typename T> void safeInterrupt(T &obj)
        {
            try
            {
                obj.interrupt();
            }
            catch (...)
            {
                /* interrupt() on an already finished context is not an error
                   worth a line; the caller is about to give up either way. */
            }
        }

        uint64_t nowMs()
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count());
        }

        /* PeerlistEntry carries a host-order uint32 and a uint32 port; the v6
           form carries 16 network-order bytes. Both become a printable address
           plus a 16-bit port, or are dropped. */
        void appendHarvested(std::vector<HarvestedPeer> &out, const uint32_t ip, const uint32_t port)
        {
            if (ip == 0 || port == 0 || port > 65535)
            {
                return;
            }

            HarvestedPeer peer;
            peer.address = Common::ipAddressToString(ip);
            peer.port = static_cast<uint16_t>(port);

            out.push_back(std::move(peer));
        }

        void appendHarvested6(std::vector<HarvestedPeer> &out, const uint8_t ip[16], const uint32_t port)
        {
            if (port == 0 || port > 65535)
            {
                return;
            }

            bool allZero = true;

            for (size_t i = 0; i < 16; i++)
            {
                if (ip[i] != 0)
                {
                    allZero = false;
                    break;
                }
            }

            if (allZero)
            {
                return;
            }

            HarvestedPeer peer;

            try
            {
                peer.address = System::IpAddress(ip).toString();
            }
            catch (...)
            {
                return;
            }

            peer.port = static_cast<uint16_t>(port);

            out.push_back(std::move(peer));
        }
    } // namespace

    bool buildCrawlerIdentity(CrawlerIdentity &identity, std::string &error)
    {
        identity.peerId = Random::randomValue<uint64_t>();
        identity.p2pVersion = CryptoNote::P2P_CURRENT_VERSION;
        identity.networkId = CryptoNote::CRYPTONOTE_NETWORK;

        /* The genesis hash is not a constant anywhere in the config, but the
           height-0 checkpoint is exactly it, and it is the same value every
           node validates its own genesis block against. Reusing it keeps the
           crawler from carrying a second copy that could drift. */
        if (CryptoNote::CHECKPOINTS.size() == 0)
        {
            error = "CryptoNoteCheckpoints.h has no checkpoints, so the genesis hash cannot be recovered";
            return false;
        }

        const CryptoNote::CheckpointData &first = *CryptoNote::CHECKPOINTS.begin();

        if (first.index != 0)
        {
            error = "the first checkpoint is height " + std::to_string(first.index)
                    + ", not 0, so it is not the genesis hash";
            return false;
        }

        if (!Common::podFromHex(std::string(first.blockId), identity.genesisHash))
        {
            error = "the height-0 checkpoint is not valid hex";
            return false;
        }

        return true;
    }

    ProbeResult probePeer(
        System::Dispatcher &dispatcher,
        const System::IpAddress &address,
        const uint16_t port,
        const uint32_t timeoutMs,
        const CrawlerIdentity &identity)
    {
        ProbeResult result;
        result.address = address.toString();
        result.port = port;
        result.reported.address = result.address;
        result.reported.port = port;

        const uint64_t started = nowMs();

        CryptoNote::COMMAND_HANDSHAKE::request request;
        CryptoNote::COMMAND_HANDSHAKE::response response;

        request.node_data.network_id = identity.networkId;
        request.node_data.version = identity.p2pVersion;
        request.node_data.peer_id = identity.peerId;
        request.node_data.local_time = static_cast<uint64_t>(std::time(nullptr));

        /* Zero. See CrawlerIdentity: this is what keeps the crawler out of
           every peer list on the network. */
        request.node_data.my_port = 0;

        /* Zero height plus the genesis hash. See CrawlerIdentity for why the
           pair matters, and why neither may be "something plausible". */
        request.payload_data.current_height = 0;
        request.payload_data.top_id = identity.genesisHash;
        request.payload_data.capability_flags = 0;
        request.payload_data.pruned_node_height = 0;
        request.payload_data.lite_start_height = 0;

        bool invoked = false;
        bool refused = false;
        std::string failure;

        try
        {
            System::Context<> work(dispatcher, [&] {
                try
                {
                    System::TcpConnector connector(dispatcher);
                    System::TcpConnection connection = connector.connect(address, port);

                    CryptoNote::LevinProtocol protocol(connection);

                    invoked = protocol.invoke(CryptoNote::COMMAND_HANDSHAKE::ID, request, response);

                    if (!invoked)
                    {
                        failure = "peer answered, but not with a handshake response";
                    }

                    /* connection goes out of scope here: one command, then we
                       hang up. Every node runs a hard inbound cap (--in-peers,
                       15 by default) and a crawler that lingers would hold one
                       of those slots on every node in the network at once. */
                }
                catch (const std::exception &e)
                {
                    failure = e.what();

                    /* Distinguishing "refused" from "timed out" matters to an
                       operator reading the table: one is a host that is up with
                       the port shut, the other is a host that is gone or
                       firewalled to drop. The dispatcher surfaces both as
                       exceptions, so this is the only signal available. */
                    const std::string what = failure;

                    refused = what.find("refused") != std::string::npos
                              || what.find("WSAECONNREFUSED") != std::string::npos
                              || what.find("unreachable") != std::string::npos;
                }
            });

            System::Context<> timeout(dispatcher, [&] {
                System::Timer(dispatcher).sleep(std::chrono::milliseconds(timeoutMs));
                safeInterrupt(work);
            });

            work.get();
        }
        catch (const std::exception &e)
        {
            if (failure.empty())
            {
                failure = e.what();
            }
        }
        catch (...)
        {
            if (failure.empty())
            {
                failure = "unknown error";
            }
        }

        result.rttMs = nowMs() - started;

        if (!invoked)
        {
            result.reach = refused ? Reach::Refused : Reach::Timeout;
            result.error = failure.empty() ? "no response" : failure;
            return result;
        }

        if (response.node_data.network_id != identity.networkId)
        {
            result.reach = Reach::Failed;
            result.error = "wrong network id " + Common::podToHex(response.node_data.network_id);
            return result;
        }

        result.reach = Reach::Open;

        result.reported.haveHandshake = true;
        result.reported.p2pVersion = response.node_data.version;
        result.reported.peerId = response.node_data.peer_id;
        result.reported.advertisedPort = response.node_data.my_port;
        result.reported.height = response.payload_data.current_height;
        result.reported.topId = response.payload_data.top_id;
        result.reported.capabilityFlags = response.payload_data.capability_flags;
        result.reported.prunedHeight = response.payload_data.pruned_node_height;
        result.reported.liteStartHeight = response.payload_data.lite_start_height;

        /* The peer's own clock, as it reported it, against ours at the moment
           we read the answer. The round trip is in here too, which is why the
           dashboard buckets this rather than showing seconds. */
        result.reported.clockSkewSeconds = static_cast<int64_t>(response.node_data.local_time)
                                           - static_cast<int64_t>(std::time(nullptr));

        for (const auto &entry : response.local_peerlist)
        {
            appendHarvested(result.harvested, entry.adr.ip, entry.adr.port);
        }

        for (const auto &entry : response.local_peerlist6)
        {
            appendHarvested6(result.harvested, entry.adr.ip, entry.adr.port);
        }

        result.reported.peersAdvertised = static_cast<uint32_t>(result.harvested.size());

        return result;
    }

    std::string probeSoftwareVersion(const std::string &address, const uint16_t rpcPort, const uint32_t timeoutMs)
    {
        try
        {
            /* httplib wants a bare host, so an IPv6 literal arrives here still
               wearing the brackets IpAddress::toString() put on it. */
            std::string host = address;

            if (host.size() > 2 && host.front() == '[' && host.back() == ']')
            {
                host = host.substr(1, host.size() - 2);
            }

            httplib::Client client(host, rpcPort);

            client.set_connection_timeout(0, static_cast<int>(timeoutMs) * 1000);
            client.set_read_timeout(0, static_cast<int>(timeoutMs) * 1000);
            client.set_write_timeout(0, static_cast<int>(timeoutMs) * 1000);
            client.set_keep_alive(false);

            const auto response = client.Get("/info");

            if (!response || response->status != 200)
            {
                return {};
            }

            const auto body = nlohmann::json::parse(response->body, nullptr, false);

            if (body.is_discarded() || !body.is_object())
            {
                return {};
            }

            const auto version = body.find("version");

            if (version == body.end() || !version->is_string())
            {
                return {};
            }

            return version->get<std::string>();
        }
        catch (...)
        {
            return {};
        }
    }
} // namespace NetMon
