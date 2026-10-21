#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace bittorrent {

class UrlEncoder {
public:
    static std::string encode(const std::array<uint8_t, 20>& hash);
};

} // namespace bittorrent