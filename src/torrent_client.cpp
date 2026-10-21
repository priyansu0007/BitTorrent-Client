#include "bittorrent/torrent_client.hpp"
#include "bittorrent/crypto.hpp"
#include "bittorrent/peer_connection.hpp"
#include "bittorrent/piece_manager.hpp"
#include "bittorrent/storage.hpp"
#include "bittorrent/tracker.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace bittorrent {

namespace {
// ANSI Color Codes for Terminal UI
constexpr const char* RESET   = "\033[0m";
constexpr const char* BOLD    = "\033[1m";
constexpr const char* CYAN    = "\033[36m";
constexpr const char* GREEN   = "\033[32m";
constexpr const char* YELLOW  = "\033[33m";
constexpr const char* RED     = "\033[31m";
constexpr const char* MAGENTA = "\033[35m";

std::string format_bytes(double bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    while (bytes >= 1024.0 && i < 3) {
        bytes /= 1024.0;
        ++i;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << bytes << " " << suffixes[i];
    return oss.str();
}

std::string format_time(int seconds) {
    if (seconds < 0 || seconds > 86400) return "--:--";
    int mins = seconds / 60;
    int secs = seconds % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << mins << ":"
        << std::setw(2) << std::setfill('0') << secs;
    return oss.str();
}
} // namespace

TorrentClient::TorrentClient(const std::filesystem::path& torrent_path)
    : torrent_(TorrentFile::load_from_file(torrent_path)) {}

void TorrentClient::download(const std::filesystem::path& output_dir) {
    // 1. Metadata Banner
    std::cout << BOLD << CYAN << "\n==========================================================\n";
    std::cout << "                 MINIMAL BITTORRENT CLIENT                \n";
    std::cout << "==========================================================" << RESET << "\n";
    std::cout << BOLD << "  File Name   : " << RESET << torrent_.name << "\n";
    std::cout << BOLD << "  Total Size  : " << RESET << format_bytes(static_cast<double>(torrent_.total_length)) << "\n";
    std::cout << BOLD << "  Piece Size  : " << RESET << format_bytes(static_cast<double>(torrent_.piece_length)) << "\n";
    std::cout << BOLD << "  Total Pieces: " << RESET << torrent_.pieceHashes.size() << "\n";
    std::cout << BOLD << "  Info Hash   : " << RESET << MAGENTA << torrent_.to_hex(torrent_.info_hash) << RESET << "\n";
    std::cout << BOLD << CYAN << "----------------------------------------------------------\n" << RESET;

    // 2. Tracker Discovery
    std::string peer_id = Crypto::generate_peer_id();
    std::cout << YELLOW << "[1/3] Contacting Tracker: " << RESET << torrent_.announce << "...\n";
    TrackerResponse tracker_res = TrackerClient::announce(torrent_, peer_id);

    if (tracker_res.peers.empty()) {
        throw std::runtime_error("Tracker returned 0 peers");
    }
    std::cout << GREEN << "  ✓ Swarm discovered: " << tracker_res.peers.size() << " active peers.\n" << RESET;

    // 3. Storage & Piece Setup
    std::filesystem::path output_path = output_dir / torrent_.name;
    Storage storage(output_path, torrent_.total_length);
    PieceManager piece_mgr(torrent_);

    std::unique_ptr<PeerConnection> conn;
    size_t peer_idx = 0;

    auto connect_next_peer = [&]() -> bool {
        while (peer_idx < tracker_res.peers.size()) {
            const auto& candidate = tracker_res.peers[peer_idx++];
            try {
                std::cout << YELLOW << "[2/3] Handshaking peer " << RESET
                          << candidate.ip << ":" << candidate.port << "... " << std::flush;
                auto next_conn = std::make_unique<PeerConnection>(candidate, torrent_.info_hash, peer_id);
                next_conn->connect_and_handshake();
                next_conn->send_interested();
                next_conn->wait_for_unchoke();
                std::cout << GREEN << "Connected & Unchoked!\n" << RESET;
                conn = std::move(next_conn);
                return true;
            } catch (const std::exception& e) {
                std::cout << RED << "Failed (" << e.what() << ")\n" << RESET;
            }
        }
        return false;
    };

    if (!connect_next_peer()) {
        throw std::runtime_error("Could not establish an unchoked connection with any peer");
    }

    // 4. Live Progress Bar Loop
    std::cout << YELLOW << "\n[3/3] Downloading & Verifying SHA-1 Pieces:\n" << RESET;

    // NOTE: Currently set to 5 pieces for fast testing!
    // Change to: size_t target_pieces = piece_mgr.total_pieces(); for the full file.
    //size_t target_pieces = std::min<size_t>(5, piece_mgr.total_pieces());
    size_t target_pieces =  piece_mgr.total_pieces();

    int64_t downloaded_bytes = 0;
    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < target_pieces; ) {
        try {
            uint32_t current_size = piece_mgr.piece_size(i);
            std::vector<uint8_t> piece_data = conn->download_piece(static_cast<uint32_t>(i), current_size);

            if (!piece_mgr.verify_piece(i, piece_data)) {
                std::cerr << RED << "\n  ✗ Piece " << i << " failed SHA-1 check. Retrying...\n" << RESET;
                continue;
            }

            storage.write_piece(i, torrent_.piece_length, piece_data);
            piece_mgr.mark_completed(i);
            downloaded_bytes += current_size;
            ++i;

            // Calculate Speed & ETA
            auto now = std::chrono::steady_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
            double speed_bps = (elapsed_sec > 0.0) ? (static_cast<double>(downloaded_bytes) / elapsed_sec) : 0.0;

            int64_t remaining_bytes = torrent_.total_length - downloaded_bytes;
            int eta_sec = (speed_bps > 0.0) ? static_cast<int>(static_cast<double>(remaining_bytes) / speed_bps) : -1;

            // Render Progress Bar
            constexpr int BAR_WIDTH = 28;
            double fraction = static_cast<double>(i) / static_cast<double>(target_pieces);
            int filled = static_cast<int>(fraction * BAR_WIDTH);

            std::cout << "\r  " << BOLD << "[";
            for (int b = 0; b < BAR_WIDTH; ++b) {
                if (b < filled) std::cout << GREEN << "█";
                else if (b == filled) std::cout << CYAN << "▓";
                else std::cout << RESET << "░";
            }
            std::cout << RESET << BOLD << "] "
                      << std::setw(3) << static_cast<int>(fraction * 100.0) << "% " << RESET
                      << "(" << i << "/" << target_pieces << ") | "
                      << CYAN << format_bytes(speed_bps) << "/s" << RESET
                      << " | ETA: " << YELLOW << format_time(eta_sec) << RESET << "   " << std::flush;

        } catch (const std::exception& e) {
            std::cout << RED << "\n  ! Peer dropped on piece " << i << " (" << e.what() << "). Switching peer...\n" << RESET;
            if (!connect_next_peer()) {
                throw std::runtime_error("Exhausted all peers before finishing download");
            }
        }
    }

    std::cout << "\n\n" << BOLD << GREEN << "==========================================================\n";
    std::cout << "  ✓ DOWNLOAD COMPLETE!\n";
    std::cout << "  Saved to: " << RESET << std::filesystem::absolute(output_path).string() << "\n";
    std::cout << BOLD << GREEN << "==========================================================\n\n" << RESET;
}

} // namespace bittorrent
