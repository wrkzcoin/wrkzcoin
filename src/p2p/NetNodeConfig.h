// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "P2pProtocolTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CryptoNote
{
    class NetNodeConfig
    {
      public:
        NetNodeConfig();

        bool init(
            const std::string interface,
            const int port,
            const int external,
            const uint32_t outPeers,
            const uint32_t inPeers,
            const bool localIp,
            const bool hidePort,
            const std::string dataDir,
            const std::vector<std::string> addPeers,
            const std::vector<std::string> addExclusiveNodes,
            const std::vector<std::string> addPriorityNodes,
            const std::vector<std::string> addSeedNodes,
            const bool p2pResetPeerState,
            const std::string p2pBindIpv6Address = "",
            const int p2pBindPortIpv6 = 0);

        std::string getP2pStateFilename() const;

        bool getP2pStateReset() const;

        std::string getBindIp() const;

        uint16_t getBindPort() const;

        uint16_t getExternalPort() const;

        uint32_t getOutPeers() const;

        uint32_t getInPeers() const;

        bool getAllowLocalIp() const;

        std::vector<PeerlistEntry> getPeers() const;

        std::vector<NetworkAddress> getPriorityNodes() const;

        std::vector<NetworkAddress> getExclusiveNodes() const;

        std::vector<NetworkAddress> getSeedNodes() const;
        std::vector<std::string> getSeedNodeAddresses() const;

        bool getHideMyPort() const;

        std::string getConfigFolder() const;

        std::string getBindIpv6Address() const;

        uint16_t getBindPortIpv6() const;

        /* The network id every handshake carries and demands of the peer.
           Mainnet's CRYPTONOTE_NETWORK unless a simnet sets its own. */
        void setNetworkId(const std::array<uint8_t, 16> &networkId);

        std::array<uint8_t, 16> getNetworkId() const;

        /* Whether the compiled-in SEED_NODES and DNS_SEED_NODES are used. Off
           for a simnet, which must never dial a mainnet node. --seed-node
           addresses are used either way. */
        void setUseDefaultSeeds(const bool useDefaultSeeds);

        bool getUseDefaultSeeds() const;

        /* Whether to ask the router for a UPnP port mapping. */
        void setUpnp(const bool upnp);

        bool getUpnp() const;

        /* How often peers compare heights (timed sync). A simnet mines far
           faster than mainnet and wants a node that missed a relay to notice
           within seconds, not a minute. */
        void setTimedSyncIntervalSeconds(const uint32_t seconds);

        uint32_t getTimedSyncIntervalSeconds() const;

      private:
        std::string bindIp;

        uint16_t bindPort;

        uint16_t externalPort;

        uint32_t outPeers;

        uint32_t inPeers;

        bool allowLocalIp;

        std::vector<PeerlistEntry> peers;

        std::vector<NetworkAddress> priorityNodes;

        std::vector<NetworkAddress> exclusiveNodes;

        std::vector<NetworkAddress> seedNodes;
        std::vector<std::string> seedNodeAddresses;

        bool hideMyPort;

        std::string configFolder;

        std::string p2pStateFilename;

        bool p2pStateReset;

        std::string m_bindIpv6Address;

        uint16_t m_bindPortIpv6;

        std::array<uint8_t, 16> m_networkId;

        bool m_useDefaultSeeds;

        bool m_upnp;

        uint32_t m_timedSyncIntervalSeconds;
    };

} // namespace CryptoNote
