#pragma once

#include "bittorrent/torrent_file.hpp"
#include <filesystem>

namespace bittorrent {

class TorrentClient {
public:
    explicit TorrentClient(const std::filesystem::path& torrent_path);
    void download(const std::filesystem::path& output_dir = ".");

private:
    TorrentFile torrent_;
};

} // namespace bittorrent
