#include "bittorrent/torrent_file.hpp"
#include "bittorrent/bencode.hpp"

#include <fstream> //stream support to read .torrent file
#include <iomanip>  //to format hexadecimal text
#include <openssl/sha.h> //for sha-1 hashing function
#include <sstream>  //stream support for buffer operation
#include <stdexcept>
#include <cstring>

using namespace std;

namespace bittorrent{
namespace{
    infoHash compute_sha1(string_view data){
        infoHash hash{};
        SHA1(reinterpret_cast<const unsigned char*>(data.data()),data.size(),hash.data());
        return hash;

    }
}


string TorrentFile::to_hex(const array<uint8_t,20>& hash){
    ostringstream oss;
    oss<<hex<<setfill('0');
    for(uint8_t byte: hash){
        oss<<setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

TorrentFile TorrentFile::load_from_file(const filesystem::path & file_path){
    ifstream file(file_path, ios::binary);
    if(!file){
        throw runtime_error("TorrentFile: failed to open file at "+file_path.string());
    }
    ostringstream ss;
    ss<< file.rdbuf();
    string contents=ss.str();
    return parse(contents);
}
TorrentFile TorrentFile::parse(string_view raw_data){
    BencodeParser parser(raw_data);
    BValue root=parser.parse();
    if(!root.is_dict()){
        throw runtime_error("TorrentFile: root element is not a bencoded dictionary");
    }
    const auto& root_dict=root.as_dict();
    TorrentFile torrent;


    //extract announce url
    auto announce_it=root_dict.find("announce");
    if(announce_it==root_dict.end() || !announce_it->second.is_string()){
        throw runtime_error("TorrentFIle: missing/invalid announce url");
    }
    torrent.announce=announce_it->second.as_string();

    //extract announce list
    //structureis like list of list kinda stuff
    auto announce_list_it = root_dict.find("announce-list");
    if(announce_list_it!=root_dict.end() && announce_list_it->second.is_list()){
        for(const auto& tier: announce_list_it->second.as_list()){
            if(tier.is_list()){
                for (const auto& tracker : tier.as_list()) {
                    if (tracker.is_string()) {
                        torrent.announce_list.push_back(tracker.as_string());
                    }
                }
            }
        }
    }

    //compute sha1 infohash

    string_view raw_info=parser.get_raw_info_dict();
    torrent.info_hash=compute_sha1(raw_info);

    //check info dictionary
    auto info_it=root_dict.find("info");
    if(info_it==root_dict.end() || !info_it->second.is_dict()){
        throw runtime_error("TorrentFIle: invalid or missing info dict");
    }
    const auto& info_dict=info_it->second.as_dict();

    //get name
    auto name_it=info_dict.find("name");
    if(name_it==info_dict.end()|| !name_it->second.is_string()){
        throw runtime_error("TorrentFile: invalid/missing name of torrent file");
    }
    torrent.name=name_it->second.as_string();


    //extract piece length
    auto piece_len_it=info_dict.find("piece length");
    if(piece_len_it==info_dict.end()|| !piece_len_it->second.is_int()){
        throw runtime_error("TorrentFile: invalid/missing piece length of torrent file");
    }
    torrent.piece_length=piece_len_it->second.as_int();


    // Extract and Slice 20-Byte SHA1 Piece Hashes
    auto pieces_it = info_dict.find("pieces");
    if (pieces_it == info_dict.end() || !pieces_it->second.is_string()) {
        throw runtime_error("TorrentFile: Missing or invalid 'pieces' binary string");
    }
    const string& raw_pieces = pieces_it->second.as_string();
    if (raw_pieces.size()%20!=0) {
        throw runtime_error("TorrentFile: pieces byte length is not a multiple of 20");
    }

    size_t num_pieces = raw_pieces.size()/20;
    torrent.pieceHashes.reserve(num_pieces);

    for(size_t i=0;i<num_pieces;i++){
        pieceHash hash{};
        memcpy(hash.data(),&raw_pieces[i*20],20);
        torrent.pieceHashes.push_back(hash);
    }

    //  File Layout: Single File vs Multi-File
    auto files_it = info_dict.find("files");
    if (files_it != info_dict.end()) {
        // Multi-file torrent
        torrent.is_multi_file = true;
        if (!files_it->second.is_list()) {
            throw std::runtime_error("TorrentFile: 'files' entry must be a list");
        }

        int64_t accumulated_length = 0;
        for (const auto& file_val : files_it->second.as_list()) {
            if (!file_val.is_dict()) {
                throw std::runtime_error("TorrentFile: File entry inside 'files' must be a dictionary");
            }

            const auto& file_dict = file_val.as_dict();
            auto len_entry = file_dict.find("length");
            auto path_entry = file_dict.find("path");

            if (len_entry == file_dict.end() || !len_entry->second.is_int() ||
                path_entry == file_dict.end() || !path_entry->second.is_list()) {
                throw std::runtime_error("TorrentFile: Invalid file entry structure");
            }

            int64_t flen = len_entry->second.as_int();
            accumulated_length += flen;

            std::filesystem::path fpath = torrent.name;
            for (const auto& p_elem : path_entry->second.as_list()) {
                if (!p_elem.is_string()) {
                    throw std::runtime_error("TorrentFile: Path components must be strings");
                }
                fpath /= p_elem.as_string();
            }

            torrent.files.push_back(fileInfo{flen, std::move(fpath)});
        }
        torrent.total_length = accumulated_length;
    } else {
        // Single-file torrent
        torrent.is_multi_file = false;
        auto length_it = info_dict.find("length");
        if (length_it == info_dict.end() || !length_it->second.is_int()) {
            throw std::runtime_error("TorrentFile: Single file torrent missing 'length'");
        }

        torrent.total_length = length_it->second.as_int();
        torrent.files.push_back(fileInfo{torrent.total_length, torrent.name});
    }

    return torrent;

}
}