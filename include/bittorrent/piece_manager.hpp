#pragma once

#include "bittorrent/torrent_file.hpp"
#include <cstdint>
#include <vector>

namespace bittorrent {

class PieceManager {
public:
    explicit PieceManager(const TorrentFile& torrent);

    size_t total_pieces() const { return torrent_.pieceHashes.size(); }
    uint32_t piece_size(size_t piece_index) const;
    bool verify_piece(size_t piece_index, const std::vector<uint8_t>& data) const;
    void mark_completed(size_t piece_index);
    bool is_complete() const;

private:
    const TorrentFile& torrent_;
    std::vector<bool> completed_;
};

} // namespace bittorrent
