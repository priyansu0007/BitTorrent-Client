#include "bittorrent/tracker.hpp"
#include "bittorrent/bencode.hpp"
#include "bittorrent/url.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace bittorrent {

namespace {

struct ParsedUrl {
    std::string host;
    std::string port{"80"};
    std::string path{"/"};
};

ParsedUrl parse_url(const std::string& url) {
    ParsedUrl res;
    std::string target = url;

    // Strip http://
    if (target.rfind("http://", 0) == 0) {
        target = target.substr(7);
    } else {
        throw std::runtime_error("Tracker: Only HTTP trackers are supported in minimal mode (got: " + url + ")");
    }

    // Split host:port from path
    size_t slash_pos = target.find('/');
    std::string host_port;
    if (slash_pos != std::string::npos) {
        host_port = target.substr(0, slash_pos);
        res.path = target.substr(slash_pos);
    } else {
        host_port = target;
        res.path = "/";
    }

    // Split host and port
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        res.host = host_port.substr(0, colon_pos);
        res.port = host_port.substr(colon_pos + 1);
    } else {
        res.host = host_port;
        res.port = "80";
    }

    return res;
}

} // namespace

std::vector<PeerInfo> TrackerClient::parse_compact_peers(const std::string& binary_peers) {
    if (binary_peers.size() % 6 != 0) {
        throw std::runtime_error("Tracker: Compact peers length must be a multiple of 6");
    }

    std::vector<PeerInfo> peers;
    size_t peer_count = binary_peers.size() / 6;
    peers.reserve(peer_count);

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(binary_peers.data());
    for (size_t i = 0; i < peer_count; ++i) {
        // First 4 bytes are IP
        std::string ip = std::to_string(ptr[0]) + "." +
                         std::to_string(ptr[1]) + "." +
                         std::to_string(ptr[2]) + "." +
                         std::to_string(ptr[3]);

        // Next 2 bytes are Port in big-endian
        uint16_t port = (static_cast<uint16_t>(ptr[4]) << 8) | static_cast<uint16_t>(ptr[5]);

        peers.push_back(PeerInfo{std::move(ip), port});
        ptr += 6;
    }

    return peers;
}

TrackerResponse TrackerClient::announce(
    const TorrentFile& torrent,
    const std::string& peer_id,
    uint16_t listen_port)
{
    // 0. Locate a valid HTTP tracker (resolve HTTPS and multi-tracker tiers)
    std::string announce_url = torrent.announce;
    if (announce_url.rfind("http://", 0) != 0) {
        bool found = false;
        for (const auto& alt_url : torrent.announce_list) {
            if (alt_url.rfind("http://", 0) == 0) {
                announce_url = alt_url;
                found = true;
                break;
            }
        }
        // Fallback specifically for Ubuntu tracker
        if (!found && announce_url.find("torrent.ubuntu.com") != std::string::npos) {
            announce_url = "http://torrent.ubuntu.com:6969/announce";
        }
    }

    std::cout << "Using tracker: " << announce_url << "\n";
    ParsedUrl parsed_url = parse_url(announce_url);

    // 1. Build Query Parameters
    std::string encoded_info_hash = UrlEncoder::encode(torrent.info_hash);

    std::array<uint8_t, 20> pid_arr{};
    std::memcpy(pid_arr.data(), peer_id.data(), std::min<size_t>(20, peer_id.size()));
    std::string encoded_peer_id = UrlEncoder::encode(pid_arr);

    std::ostringstream query;
    query << parsed_url.path
          << "?info_hash=" << encoded_info_hash
          << "&peer_id=" << encoded_peer_id
          << "&port=" << listen_port
          << "&uploaded=0"
          << "&downloaded=0"
          << "&left=" << torrent.total_length
          << "&compact=1";

    // 2. DNS Resolution
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(parsed_url.host.c_str(), parsed_url.port.c_str(), &hints, &res);
    if (status != 0 || !res) {
        throw std::runtime_error("Tracker DNS resolution failed: " + std::string(gai_strerror(status)));
    }

    // 3. Connect via TCP Socket
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("Tracker: Failed to create socket");
    }

    // 6-second timeout for tracker network operations
    struct timeval tv{6, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        throw std::runtime_error("Tracker: Failed to connect to " + parsed_url.host + ":" + parsed_url.port);
    }
    freeaddrinfo(res);

    // 4. Send HTTP GET Request
    std::string host_header = parsed_url.host;
    if (parsed_url.port != "80") {
        host_header += ":" + parsed_url.port;
    }

    std::ostringstream request;
    request << "GET " << query.str() << " HTTP/1.1\r\n"
            << "Host: " << host_header << "\r\n"
            << "User-Agent: MinimalTorrent/1.0\r\n"
            << "Connection: close\r\n\r\n";

    std::string req_str = request.str();
    if (send(sock, req_str.data(), req_str.size(), 0) < 0) {
        close(sock);
        throw std::runtime_error("Tracker: Failed to send HTTP request");
    }

    // 5. Read HTTP Response
    std::string response;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, static_cast<size_t>(bytes_read));
    }
    close(sock);

    // 6. Separate HTTP Headers from Body
    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("Tracker: Invalid HTTP response (no header terminator)");
    }
    std::string bencoded_body = response.substr(header_end + 4);

    // 7. Parse Bencoded Tracker Response
    BencodeParser parser(bencoded_body);
    BValue root = parser.parse();
    if (!root.is_dict()) {
        throw std::runtime_error("Tracker: Response body is not a bencoded dictionary");
    }
    const auto& dict = root.as_dict();

    // Check if tracker reported a failure
    if (dict.contains("failure reason") && dict.at("failure reason").is_string()) {
        throw std::runtime_error("Tracker failed: " + dict.at("failure reason").as_string());
    }

    TrackerResponse tracker_res;
    if (dict.contains("interval") && dict.at("interval").is_int()) {
        tracker_res.interval = dict.at("interval").as_int();
    }

    // Extract peers
    if (!dict.contains("peers")) {
        throw std::runtime_error("Tracker: Response missing 'peers' key");
    }

    const auto& peers_entry = dict.at("peers");
    if (peers_entry.is_string()) {
        tracker_res.peers = parse_compact_peers(peers_entry.as_string());
    } else if (peers_entry.is_list()) {
        for (const auto& p_elem : peers_entry.as_list()) {
            if (p_elem.is_dict()) {
                const auto& pd = p_elem.as_dict();
                if (pd.contains("ip") && pd.at("ip").is_string() &&
                    pd.contains("port") && pd.at("port").is_int()) {
                    tracker_res.peers.push_back(PeerInfo{
                        pd.at("ip").as_string(),
                        static_cast<uint16_t>(pd.at("port").as_int())
                    });
                }
            }
        }
    }

    return tracker_res;
}

} // namespace bittorrent
