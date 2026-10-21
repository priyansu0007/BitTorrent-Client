#include "bittorrent/url.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace bittorrent {

namespace {

// RFC 3986 section 2.3: Unreserved Characters
// ALPHA / DIGIT / "-" / "." / "_" / "~"
bool is_unreserved(uint8_t c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

} // namespace

std::string UrlEncoder::encode(const std::array<uint8_t, 20>& hash) {
    std::ostringstream escaped;
    escaped << std::hex << std::uppercase << std::setfill('0');

    for (uint8_t byte : hash) {
        if (is_unreserved(byte)) {
            escaped << static_cast<char>(byte);
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(byte);
        }
    }

    return escaped.str();
}

} // namespace bittorrent