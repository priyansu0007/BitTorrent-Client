#include "bittorrent/storage.hpp"
#include <stdexcept>

namespace bittorrent {

Storage::Storage(const std::filesystem::path& file_path, int64_t total_length) {
    file_.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.clear();
        file_.open(file_path, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file_) {
        throw std::runtime_error("Storage: Failed to initialize output file at " + file_path.string());
    }

    if (total_length > 0) {
        file_.seekp(total_length - 1);
        file_.write("", 1);
        file_.flush();
    }
}

Storage::~Storage() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Storage::write_piece(size_t piece_index, int64_t piece_length, const std::vector<uint8_t>& data) {
    uint64_t offset = static_cast<uint64_t>(piece_index) * static_cast<uint64_t>(piece_length);
    file_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    file_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    file_.flush();
}

} // namespace bittorrent
