#include "bittorrent/piece_manager.hpp"
#include "bittorrent/crypto.hpp"
#include <algorithm>

namespace bittorrent {

PieceManager::PieceManager(const TorrentFile& torrent)
    : torrent_(torrent), completed_(torrent.pieceHashes.size(), false) {}

uint32_t PieceManager::piece_size(size_t piece_index) const {
    int64_t offset = static_cast<int64_t>(piece_index) * torrent_.piece_length;
    int64_t remaining = torrent_.total_length - offset;
    return static_cast<uint32_t>(std::min(torrent_.piece_length, remaining));
}

bool PieceManager::verify_piece(size_t piece_index, const std::vector<uint8_t>& data) const {
    auto hash = Crypto::sha1(data);
    return hash == torrent_.pieceHashes[piece_index];
}

void PieceManager::mark_completed(size_t piece_index) {
    if (piece_index < completed_.size()) {
        completed_[piece_index] = true;
    }
}

bool PieceManager::is_complete() const {
    for (bool done : completed_) {
        if (!done) return false;
    }
    return true;
}

} // namespace bittorrent
