#pragma once

#include <cstdint>
#include <string>

namespace bittorrent {

struct PeerInfo {
    std::string ip;
    uint16_t port{0};
};

} // namespace bittorrent