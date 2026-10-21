#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace bittorrent {

class Storage {
public:
    Storage(const std::filesystem::path& file_path, int64_t total_length);
    ~Storage();

    void write_piece(size_t piece_index, int64_t piece_length, const std::vector<uint8_t>& data);

private:
    std::fstream file_;
};

} // namespace bittorrent
