// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include <algorithm>
#include <p2p/Peerlist.h>

Peerlist::Peerlist(std::vector<PeerlistEntry> &peers, size_t maxSize): m_peers(peers), m_maxSize(maxSize) {}

size_t Peerlist::count() const
{
    return m_peers.size();
}

bool Peerlist::get(PeerlistEntry &entry, size_t i) const
{
    if (i >= m_peers.size())
    {
        return false;
    }

    /* Sort the peers by last seen [Newer peers come first]. The selection loops
       call this hundreds of times per round, so skip the sort when nothing has
       changed since the last call. */
    const auto newerFirst = [](const auto &lhs, const auto &rhs) { return lhs.last_seen > rhs.last_seen; };

    if (!std::is_sorted(m_peers.begin(), m_peers.end(), newerFirst))
    {
        std::sort(m_peers.begin(), m_peers.end(), newerFirst);
    }

    entry = m_peers[i];

    return true;
}

/* Remove the oldest peers */
void Peerlist::trim()
{
    if (m_peers.size() <= m_maxSize)
    {
        return;
    }

    /* Sort the peers by last seen [Newer peers come first] */
    std::sort(
        m_peers.begin(), m_peers.end(), [](const auto &lhs, const auto &rhs) { return lhs.last_seen > rhs.last_seen; });

    /* Trim to max size */
    m_peers.erase(m_peers.begin() + m_maxSize, m_peers.end());
}
