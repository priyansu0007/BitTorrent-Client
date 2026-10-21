#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bittorrent {

class Crypto {
public:
    static std::array<uint8_t, 20> sha1(std::string_view data);
    static std::array<uint8_t, 20> sha1(const std::vector<uint8_t>& data);
    static std::string generate_peer_id();
};

} // namespace bittorrent
