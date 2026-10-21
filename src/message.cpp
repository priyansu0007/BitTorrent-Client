#include "bittorrent/message.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

namespace bittorrent {

std::vector<uint8_t> Handshake::serialize() const {
    std::vector<uint8_t> buf(68, 0);
    buf[0] = PROTOCOL_LEN;
    std::memcpy(&buf[1], PROTOCOL_STR, 19);
    std::memcpy(&buf[20], reserved.data(), 8);
    std::memcpy(&buf[28], info_hash.data(), 20);
    std::memcpy(&buf[48], peer_id.data(), 20);
    return buf;
}

Handshake Handshake::deserialize(const std::vector<uint8_t>& buffer) {
    if (buffer.size() != 68 || buffer[0] != PROTOCOL_LEN) {
        throw std::runtime_error("Handshake: Invalid length or protocol length");
    }
    if (std::memcmp(&buffer[1], PROTOCOL_STR, 19) != 0) {
        throw std::runtime_error("Handshake: Invalid protocol string");
    }

    Handshake hs;
    std::memcpy(hs.reserved.data(), &buffer[20], 8);
    std::memcpy(hs.info_hash.data(), &buffer[28], 20);
    std::memcpy(hs.peer_id.data(), &buffer[48], 20);
    return hs;
}

std::vector<uint8_t> Message::serialize() const {
    if (id == MessageId::KeepAlive) {
        return {0, 0, 0, 0};
    }

    uint32_t len = htonl(static_cast<uint32_t>(1 + payload.size()));
    std::vector<uint8_t> buf(4 + 1 + payload.size());

    std::memcpy(&buf[0], &len, 4);
    buf[4] = static_cast<uint8_t>(id);
    if (!payload.empty()) {
        std::memcpy(&buf[5], payload.data(), payload.size());
    }
    return buf;
}

Message Message::make_interested() {
    return Message{MessageId::Interested, {}};
}

Message Message::make_request(uint32_t index, uint32_t begin, uint32_t length) {
    Message msg{MessageId::Request, std::vector<uint8_t>(12)};
    uint32_t net_idx = htonl(index);
    uint32_t net_beg = htonl(begin);
    uint32_t net_len = htonl(length);

    std::memcpy(&msg.payload[0], &net_idx, 4);
    std::memcpy(&msg.payload[4], &net_beg, 4);
    std::memcpy(&msg.payload[8], &net_len, 4);
    return msg;
}

} // namespace bittorrent
