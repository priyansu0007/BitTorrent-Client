#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bittorrent {

enum class MessageId : uint8_t {
    Choke = 0,
    Unchoke = 1,
    Interested = 2,
    NotInterested = 3,
    Have = 4,
    Bitfield = 5,
    Request = 6,
    Piece = 7,
    Cancel = 8,
    KeepAlive = 255
};

struct Handshake {
    static constexpr uint8_t PROTOCOL_LEN = 19;
    static constexpr const char* PROTOCOL_STR = "BitTorrent protocol";

    std::array<uint8_t, 8> reserved{};
    std::array<uint8_t, 20> info_hash{};
    std::array<uint8_t, 20> peer_id{};

    std::vector<uint8_t> serialize() const;
    static Handshake deserialize(const std::vector<uint8_t>& buffer);
};

struct Message {
    MessageId id{MessageId::KeepAlive};
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const;

    static Message make_interested();
    static Message make_request(uint32_t index, uint32_t begin, uint32_t length);
};

} // namespace bittorrent
