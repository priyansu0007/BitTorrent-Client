#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bittorrent{
    using infoHash=std::array<uint8_t,20>;
    using pieceHash=std::array<uint8_t,20>;

    struct fileInfo{
        int64_t length{0};
        std::filesystem::path path;
    };

    struct TorrentFile{
        std::string announce;
        std::vector<std::string> announce_list;
        std::string name;
        int64_t piece_length{0};
        int64_t total_length{0};
        std::vector<pieceHash> pieceHashes;
        infoHash info_hash{};

        std::vector<fileInfo> files;
        bool is_multi_file{false};


        static TorrentFile load_from_file(const std::filesystem::path & file_path); //parse .torrent file directly from disk

        static TorrentFile parse(std::string_view raw_data);//parses .torrent metainfo from memory buffer

        std::string to_hex(const std::array<uint8_t,20> &hash);//helper to format 20 byte hash to 40 char hex string

    };

    


}