#include "bittorrent/crypto.hpp"
#include <openssl/sha.h>
#include <random>

namespace bittorrent {

std::array<uint8_t, 20> Crypto::sha1(std::string_view data) {
    std::array<uint8_t, 20> hash{};
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash.data());
    return hash;
}

std::array<uint8_t, 20> Crypto::sha1(const std::vector<uint8_t>& data) {
    std::array<uint8_t, 20> hash{};
    SHA1(data.data(), data.size(), hash.data());
    return hash;
}

std::string Crypto::generate_peer_id() {
    std::string peer_id = "-MT0001-";
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    for (int i = 0; i < 12; ++i) {
        peer_id += alphanum[dis(gen)];
    }
    return peer_id;
}

} // namespace bittorrent
