#include "bittorrent/peer_connection.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace bittorrent {

PeerConnection::PeerConnection(
    const PeerInfo& peer,
    const std::array<uint8_t, 20>& info_hash,
    const std::string& peer_id)
    : peer_(peer), info_hash_(info_hash)
{
    std::memcpy(peer_id_.data(), peer_id.data(), std::min<size_t>(20, peer_id.size()));
}

PeerConnection::~PeerConnection() {
    if (sock_ >= 0) {
        close(sock_);
    }
}

void PeerConnection::connect_and_handshake() {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(peer_.port);
    if (getaddrinfo(peer_.ip.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        throw std::runtime_error("PeerConnection: Failed to resolve peer address " + peer_.ip);
    }

    sock_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_ < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("PeerConnection: Failed to create socket");
    }

    // Set 8-second socket timeout for both send and receive
    struct timeval tv{8, 0};
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock_, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(sock_);
        sock_ = -1;
        throw std::runtime_error("PeerConnection: Failed to connect to " + peer_.ip + ":" + port_str);
    }
    freeaddrinfo(res);

    // Send 68-byte Handshake
    Handshake hs_out{};
    hs_out.info_hash = info_hash_;
    hs_out.peer_id = peer_id_;
    std::vector<uint8_t> out_bytes = hs_out.serialize();
    send_all(out_bytes.data(), out_bytes.size());

    // Receive 68-byte Handshake
    std::vector<uint8_t> in_bytes(68);
    recv_all(in_bytes.data(), 68);
    Handshake hs_in = Handshake::deserialize(in_bytes);

    if (hs_in.info_hash != info_hash_) {
        throw std::runtime_error("PeerConnection: InfoHash mismatch in peer handshake");
    }
}

void PeerConnection::send_interested() {
    Message msg = Message::make_interested();
    std::vector<uint8_t> bytes = msg.serialize();
    send_all(bytes.data(), bytes.size());
}

void PeerConnection::wait_for_unchoke() {
    while (choked_) {
        Message msg = read_message();
        if (msg.id == MessageId::Unchoke) {
            choked_ = false;
        } else if (msg.id == MessageId::Choke) {
            choked_ = true;
        }
    }
}

std::vector<uint8_t> PeerConnection::download_piece(uint32_t piece_index, uint32_t piece_size) {
    constexpr uint32_t BLOCK_SIZE = 16384; // 16 KiB
    std::vector<uint8_t> piece_buffer(piece_size);
    uint32_t bytes_received = 0;

    for (uint32_t offset = 0; offset < piece_size; offset += BLOCK_SIZE) {
        uint32_t block_len = std::min(BLOCK_SIZE, piece_size - offset);
        Message req = Message::make_request(piece_index, offset, block_len);
        std::vector<uint8_t> raw_req = req.serialize();
        send_all(raw_req.data(), raw_req.size());
    }

    while (bytes_received < piece_size) {
        Message msg = read_message();
        if (msg.id == MessageId::Unchoke) {
            choked_ = false;
        } else if (msg.id == MessageId::Choke) {
            choked_ = true;
            throw std::runtime_error("PeerConnection: Choked while downloading piece");
        } else if (msg.id == MessageId::Piece) {
            if (msg.payload.size() < 8) {
                throw std::runtime_error("PeerConnection: Malformed Piece message");
            }
            uint32_t net_idx = 0, net_begin = 0;
            std::memcpy(&net_idx, &msg.payload[0], 4);
            std::memcpy(&net_begin, &msg.payload[4], 4);

            uint32_t idx = ntohl(net_idx);
            uint32_t begin = ntohl(net_begin);
            size_t block_data_len = msg.payload.size() - 8;

            if (idx == piece_index && begin + block_data_len <= piece_size) {
                std::memcpy(&piece_buffer[begin], &msg.payload[8], block_data_len);
                bytes_received += static_cast<uint32_t>(block_data_len);
            }
        }
    }

    return piece_buffer;
}

void PeerConnection::send_all(const uint8_t* data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(sock_, data + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            throw std::runtime_error("PeerConnection: Socket send error or timeout");
        }
        total_sent += static_cast<size_t>(sent);
    }
}

void PeerConnection::recv_all(uint8_t* buffer, size_t length) {
    size_t total_read = 0;
    while (total_read < length) {
        ssize_t n = recv(sock_, buffer + total_read, length - total_read, 0);
        if (n <= 0) {
            throw std::runtime_error("PeerConnection: Socket closed or receive timeout");
        }
        total_read += static_cast<size_t>(n);
    }
}

Message PeerConnection::read_message() {
    uint32_t net_len = 0;
    recv_all(reinterpret_cast<uint8_t*>(&net_len), 4);
    uint32_t len = ntohl(net_len);

    if (len == 0) {
        return Message{MessageId::KeepAlive, {}};
    }

    uint8_t id = 0;
    recv_all(&id, 1);

    Message msg;
    msg.id = static_cast<MessageId>(id);
    if (len > 1) {
        msg.payload.resize(len - 1);
        recv_all(msg.payload.data(), len - 1);
    }
    return msg;
}

} // namespace bittorrent
