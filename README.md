# BitTorrent Client (C++20)

A lightweight, from-scratch implementation of the BitTorrent protocol (BEP 0003) written in modern C++20. 

This project is being built in phases, starting with a custom Bencode parser and metainfo extraction, scaling up to tracker communication, peer wire protocol framing, and multi-peer TCP download orchestration.

## Features

- **Bencode Parsing:** Custom recursive descent parser for BitTorrent's `Bencode` format, featuring strict type safety using `std::variant`.
- **Metainfo Extraction:** Reads `.torrent` files, extracts piece hashes, and computes the exact raw SHA-1 `info_hash`.
- **Tracker Communication:** HTTP/HTTPS tracker support with compact peer list parsing.
- **Peer Wire Protocol:** Binary protocol serialization/deserialization for BitTorrent messages (Handshake, Choke, Unchoke, Interested, Have, Bitfield, Request, Piece).
- **Asynchronous Downloading:** Pipelined block requests (16 KB slices) and dynamic piece assembly.
- **Data Integrity:** SHA-1 verification of downloaded pieces before writing to disk.

## Directory Structure

```text
.
├── CMakeLists.txt
├── include/
│   └── bittorrent/       # Public headers (bencode, crypto, message, etc.)
├── src/                  # Implementation files
├── tests/                # GoogleTest unit testing suite
└── torrents/             # Directory for .torrent test files
```
## Prerequisites

To build this project, you will need:
- **CMake** (v3.20 or higher)
- **C++20 Compiler** (GCC 10+, Clang 11+, or MSVC 19.29+)
- **OpenSSL** (for SHA-1 hashing)
- **Threads** (Standard system threading library)

## Build Instructions

This project uses CMake for an out-of-source build, keeping your root directory clean.

1. **Configure the project:**
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   ```

2. **Compile the client and test suite:**
   ```bash
   cmake --build build -j$(nproc)
   ```

## Running the Tests

The testing suite uses GoogleTest, which is automatically fetched by CMake. You can run all unit tests using CTest:

```bash
cd build
ctest --output-on-failure
```

## Usage

*(Note: Command line interface is currently under active development)*

```bash
./build/torrent_client <path_to_file.torrent> -o <download_directory>
```

## References

- [BEP 0003: The BitTorrent Protocol Specification](http://www.bittorrent.org/beps/bep_0003.html)