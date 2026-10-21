#pragma once

#include "bittorrent/peer_info.hpp"
#include "bittorrent/torrent_file.hpp"
#include <string>
#include <vector>

namespace bittorrent {

struct TrackerResponse {
    int64_t interval{0};
    std::vector<PeerInfo> peers;
};

class TrackerClient {
public:
    // Discovers peers by sending an HTTP GET request to the announce URL
    static TrackerResponse announce(
        const TorrentFile& torrent,
        const std::string& peer_id,
        uint16_t listen_port = 6881
    );

private:
    // Helper to unpack the compact 6-byte peer representation
    static std::vector<PeerInfo> parse_compact_peers(const std::string& binary_peers);
};

} // namespace bittorrent