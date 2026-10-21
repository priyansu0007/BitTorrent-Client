#include "bittorrent/torrent_client.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./torrent_client <path_to_torrent_file> [output_dir]\n";
        return 1;
    }

    std::string torrent_path = argv[1];
    std::string output_dir = (argc >= 3) ? argv[2] : ".";

    try {
        bittorrent::TorrentClient client(torrent_path);
        client.download(output_dir);
    } catch (const std::exception& ex) {
        std::cerr << "Fatal Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
