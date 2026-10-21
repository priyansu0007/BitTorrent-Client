#pragma once

#include "bittorrent/message.hpp"
#include "bittorrent/peer_info.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bittorrent {

class PeerConnection {
public:
    PeerConnection(const PeerInfo& peer, const std::array<uint8_t, 20>& info_hash, const std::string& peer_id);
    ~PeerConnection();

    PeerConnection(const PeerConnection&) = delete;
    PeerConnection& operator=(const PeerConnection&) = delete;

    void connect_and_handshake();
    void send_interested();
    void wait_for_unchoke();
    std::vector<uint8_t> download_piece(uint32_t piece_index, uint32_t piece_size);

    bool is_choked() const { return choked_; }

private:
    PeerInfo peer_;
    std::array<uint8_t, 20> info_hash_;
    std::array<uint8_t, 20> peer_id_{};
    int sock_{-1};
    bool choked_{true};

    void send_all(const uint8_t* data, size_t length);
    void recv_all(uint8_t* buffer, size_t length);
    Message read_message();
};

} // namespace bittorrent
