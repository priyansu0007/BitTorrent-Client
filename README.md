# Minimal C++ BitTorrent Client Engine

![C++](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20C%2B%2B20-00599C?style=flat-square&logo=c%2B%2B)
![CMake](https://img.shields.io/badge/Build-CMake%203.20%2B-064F8C?style=flat-square&logo=cmake)
![OpenSSL](https://img.shields.io/badge/Crypto-OpenSSL%20SHA--1-721412?style=flat-square&logo=openssl)
![Protocol](https://img.shields.io/badge/Spec-BEP%200003%20%7C%20RFC%203986-2EA44F?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-POSIX%20(Linux%20%2F%20macOS)-FCC624?style=flat-square&logo=linux)

A lightweight, standards-compliant BitTorrent client written from scratch in modern C++. Designed with zero third-party networking bloat, this engine implements custom recursive-descent **Bencode deserialization**, **RFC 3986 URI percent-encoding**, **HTTP Tracker discovery**, the **BitTorrent Peer Wire Protocol (BEP 0003)** over raw POSIX TCP sockets, **OpenSSL SHA-1 piecewise cryptographic verification**, and zero-fragmentation disk persistence.

---

## Table of Contents

1. [Key Features](#key-features)
2. [System Architecture & Pipeline](#system-architecture--pipeline)
3. [Project Structure](#project-structure)
4. [Technical Deep-Dive](#technical-deep-dive)
5. [Prerequisites & Dependencies](#prerequisites--dependencies)
6. [Building the Project](#building-the-project)
7. [Running the Client](#running-the-client)
8. [Running Unit Tests](#running-unit-tests)
9. [Verifying Downloaded Files](#verifying-downloaded-files)

---

## Key Features

- **Zero-Copy Bencode Parser (`bencode.cpp`)**: Recursive-descent tokenizer utilizing `std::string_view`, `std::variant`, `std::map`, and `std::vector` to construct an AST-like hierarchy (`BValue`) with strict grammar validation.
- **Exact `info_hash` Invariant Preservation (`torrent_file.cpp`)**: Captures the exact unparsed byte slice of the `info` dictionary directly from the input stream (`get_raw_info_dict()`) to compute the 20-byte SHA-1 `info_hash` via OpenSSL—avoiding re-serialization key-ordering bugs.
- **RFC 3986 Percent-Encoder (`url.cpp`)**: Safely encodes arbitrary 20-byte binary digests (`info_hash` and `peer_id`) into uppercase `%XX` hexadecimal strings for HTTP GET transport.
- **Tracker Client & Compact Peer Unpacking (`tracker.cpp`)**: Communicates with BitTorrent trackers, supports `announce-list` fallbacks (BEP 0012), and unpacks 6-byte big-endian compact peer representations (`4-byte IPv4` + `2-byte Port`).
- **BitTorrent Wire Protocol State Machine (`peer_connection.cpp` & `message.cpp`)**: Implements the 68-byte peer handshake, socket timeouts (`SO_RCVTIMEO` / `SO_SNDTIMEO`), peer rotation on failure, `Interested` / `Unchoke` state transitions, and pipelined 16 KiB block requests.
- **Cryptographic Piece Verification (`piece_manager.cpp` & `crypto.cpp`)**: Verifies every assembled piece against the `.torrent` metainfo SHA-1 checksum list before committing bytes to disk.
- **Pre-Allocated Disk Storage (`storage.cpp`)**: Pre-allocates the full target file size on disk and writes verified pieces directly to their calculated byte offset ($\text{Offset} = \text{PieceIndex} \times \text{PieceLength}$).
- **Rich Terminal UI (`torrent_client.cpp`)**: Color-coded ANSI dashboard displaying torrent metadata, swarm discovery status, live progress bar, transfer speeds (`KB/s` / `MB/s`), and dynamic ETA.

---

## System Architecture & Pipeline

```mermaid
flowchart TD
    classDef startEnd fill:#1e293b,stroke:#0f172a,stroke-width:2px,color:#fff;
    classDef step fill:#1e3a8a,stroke:#172554,stroke-width:1.5px,color:#fff;
    classDef crypto fill:#581c87,stroke:#3b0764,stroke-width:1.5px,color:#fff;
    classDef net fill:#0369a1,stroke:#075985,stroke-width:1.5px,color:#fff;
    classDef wire fill:#0f766e,stroke:#115e59,stroke-width:1.5px,color:#fff;
    classDef disk fill:#15803d,stroke:#166534,stroke-width:1.5px,color:#fff;
    classDef cond fill:#c2410c,stroke:#9a3412,stroke-width:1.5px,color:#fff;
    classDef err fill:#b91c1c,stroke:#991b1b,stroke-width:1.5px,color:#fff;

    Start([Start: Ingest .torrent File]) :::startEnd --> ReadFile[Read Binary Stream via std::ifstream] :::step
    ReadFile --> ParseBencode[Step 1: Recursive Bencode Parsing\nTokenize integers, strings, lists & dicts] :::step
    ParseBencode --> ExtractSlice[Step 2: Cryptographic Identity\nSlice raw info-dict & compute 20-byte SHA-1 info_hash] :::crypto
    ExtractSlice --> ExtractMeta[Step 3: Metainfo & Checksum Slicing\nExtract announce URL, piece size & 20-byte piece hashes] :::step
    ExtractMeta --> UrlEncode[Step 4: RFC 3986 URL Percent-Encoding\nConvert binary info_hash & peer_id to %XX tokens] :::net
    UrlEncode --> TrackerAnnounce[Step 5: Tracker Announce & Peer Discovery\nQuery tracker -> unpack compact 6-byte peer IP:Port list] :::net

    TrackerAnnounce --> CheckPeers{Active Peers\nAvailable?} :::cond
    CheckPeers -- No --> ErrPeers[Throw Error: Swarm unreachable] :::err
    CheckPeers -- Yes --> TCPConnect[Step 6: TCP Connection & Handshake\nOpen socket & exchange 68-byte BitTorrent handshake] :::wire

    TCPConnect --> CheckHash{InfoHash\nMatches?} :::cond
    CheckHash -- No --> ClosePeer[Close Socket & Try Next Peer] :::err
    ClosePeer --> TCPConnect
    CheckHash -- Yes --> StateMachine[Step 7: Wire Protocol State Machine\nSend 'Interested' ID 2 & wait for 'Unchoke' ID 1] :::wire

    StateMachine --> StorageInit[Step 8: Disk Storage Pre-Allocation\nPre-allocate binary target file on disk] :::disk
    StorageInit --> BlockLoop[Step 9: Sequential Block Download\nSubdivide piece into 16 KiB blocks: Request ID 6 -> Piece ID 7] :::step

    BlockLoop --> VerifyPiece{Piece SHA-1\nChecksum Valid?} :::cond
    VerifyPiece -- No --> DiscardPiece[Discard Piece Buffer & Re-request] :::err
    DiscardPiece --> BlockLoop
    VerifyPiece -- Yes --> WriteDisk[Step 10: Verified Disk Persistence\nSeek offset index * piece_length & write bytes] :::disk

    WriteDisk --> CheckDone{All Pieces\nCompleted?} :::cond
    CheckDone -- No --> NextPiece[Increment Piece Index: i = i + 1] :::net
    NextPiece --> BlockLoop
    CheckDone -- Yes --> Finish([Download Complete: File Saved]) :::startEnd
```

---

## Project Structure

```text
bittorrent-client/
├── CMakeLists.txt                  # CMake build configuration & GoogleTest integration
├── README.md                       # Project documentation
├── include/
│   └── bittorrent/
│       ├── bencode.hpp             # BValue variant & BencodeParser declarations
│       ├── crypto.hpp              # SHA-1 hashing & Peer ID generation
│       ├── message.hpp             # 68-byte Handshake & Peer Wire message structs
│       ├── peer_connection.hpp     # POSIX TCP socket client & wire state machine
│       ├── peer_info.hpp           # Peer IPv4 and port representation
│       ├── piece_manager.hpp       # Piece length math, hash verification & state tracking
│       ├── storage.hpp             # Pre-allocated binary file writer
│       ├── torrent_client.hpp      # Top-level download orchestrator
│       ├── torrent_file.hpp        # Parsed .torrent metainfo data structures
│       ├── tracker.hpp             # Tracker announce client & compact peer parser
│       └── url.hpp                 # RFC 3986 percent-encoder
├── src/
│   ├── bencode.cpp                 # Recursive-descent Bencode parser implementation
│   ├── crypto.cpp                  # OpenSSL SHA-1 wrapper & random Peer ID generator
│   ├── main.cpp                    # CLI entry point
│   ├── message.cpp                 # Big-endian wire framing & serialization
│   ├── peer_connection.cpp         # TCP peer communication & 16 KiB block downloader
│   ├── piece_manager.cpp           # Piece verification logic
│   ├── storage.cpp                 # Random-access file I/O (seekp & write)
│   ├── torrent_client.cpp          # Swarm fallback loop & live ANSI progress UI
│   ├── torrent_file.cpp            # Metainfo extraction & InfoHash computation
│   ├── tracker.cpp                 # HTTP/HTTPS tracker communication
│   └── url.cpp                     # URI encoding implementation
├── tests/
│   ├── test_bencode.cpp            # Unit tests for Bencode primitives & containers
│   ├── test_handshake.cpp          # Unit tests for 68-byte Handshake serialization
│   └── test_message.cpp            # Unit tests for length-prefixed wire messages
└── torrents/                       # Sample .torrent files for testing
```

---

## Technical Deep-Dive

### 1. Bencode Serialization Format
`.torrent` files use **Bencode** to represent structured data across four primitives:
- **Integers**: `i<number>e` (e.g., `i42e` $\rightarrow 42$). Rejects leading zeros (`i03e`) and negative zero (`i-0e`).
- **Byte Strings**: `<length>:<contents>` (e.g., `4:spam` $\rightarrow$ `"spam"`).
- **Lists**: `l<elements>e` (e.g., `l4:spami42ee`).
- **Dictionaries**: `d<key><value>e` sorted lexicographically by key.

### 2. Peer Wire Protocol Framing (BEP 0003)
After exchanging the initial 68-byte handshake (`[1B pstrlen = 19][19B "BitTorrent protocol"][8B reserved][20B info_hash][20B peer_id]`), all subsequent messages follow a length-prefixed big-endian binary structure:

| Message ID | Name | Wire Length | Payload Format | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `None` | **Keep-Alive** | `4 bytes` (`len=0`) | *None* | Prevents TCP timeout during idle periods |
| `0` | **Choke** | `5 bytes` (`len=1`) | *None* | Peer refuses incoming block requests |
| `1` | **Unchoke** | `5 bytes` (`len=1`) | *None* | Peer permits incoming block requests |
| `2` | **Interested** | `5 bytes` (`len=1`) | *None* | Client signals desire to request pieces |
| `3` | **Not Interested**| `5 bytes` (`len=1`) | *None* | Client has no interest in peer's pieces |
| `4` | **Have** | `9 bytes` (`len=5`) | `[uint32 piece_index]` | Peer announces a newly verified piece |
| `5` | **Bitfield** | `5 + N bytes` | `[bitfield bytes]` | Peer's initial piece inventory bitmap |
| `6` | **Request** | `17 bytes` (`len=13`)| `[index][begin][length]`| Requests a 16 KiB block from a piece |
| `7` | **Piece** | `13 + N bytes` | `[index][begin][block]` | Delivers raw bytes fulfilling a Request |

---

## Prerequisites & Dependencies

Ensure you have the following installed on your Linux or macOS system:

- **C++ Compiler**: `g++` (GCC 9+) or `clang++` (Clang 10+) with C++17/C++20 support
- **CMake**: Version `3.20` or higher
- **OpenSSL Development Headers**: `libssl-dev` (Ubuntu/Debian) or `openssl` (macOS Homebrew)
- **cURL**: Standard CLI `curl` for HTTPS tracker redirection support

### Ubuntu / Debian Installation
```bash
sudo apt update
sudo apt install -y build-essential cmake libssl-dev curl
```

---

## Building the Project

### 1. Standard CMake Build
From the project root directory:

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 2. Build Standalone `.out` Executable in One Go
To compile and place a standalone `bittorrent_client.out` binary directly into the project root:

```bash
cd build && make -j && cp torrent_client ../bittorrent_client.out && cd ..
```

### 3. (Optional) Install System-Wide Command
To run the client from any folder on your machine using `btdownload`:

```bash
cd build && make -j && sudo cp torrent_client /usr/local/bin/btdownload
```

---

## Running the Client

### CLI Syntax
```bash
./bittorrent_client.out <path_to_torrent_file> [output_directory]
```
- `<path_to_torrent_file>`: Path to any single-file `.torrent` metainfo file.
- `[output_directory]`: *(Optional)* Directory where the downloaded file will be saved. Defaults to `.` (current working directory).

### Quick Example with Debian Netinst ISO
```bash
# 1. Download an official Debian netinst .torrent file
wget -O torrents/debian.torrent https://cdimage.debian.org/debian-cd/current/amd64/bt-cd/debian-12.9.0-amd64-netinst.iso.torrent

# 2. Run the BitTorrent client
./bittorrent_client.out torrents/debian.torrent .
```

### Sample Terminal Output
```text
==========================================================
                 MINIMAL BITTORRENT CLIENT                
==========================================================
  File Name   : debian-12.9.0-amd64-netinst.iso
  Total Size  : 632.00 MB
  Piece Size  : 256.00 KB
  Total Pieces: 2528
  Info Hash   : 9a8b7c6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f1a0b
----------------------------------------------------------
[1/3] Contacting Tracker: http://bttracker.debian.org:6969/announce...
  ✓ Swarm discovered: 50 active peers.
[2/3] Handshaking peer 185.125.190.40:6881... Connected & Unchoked!

[3/3] Downloading & Verifying SHA-1 Pieces:
  [████████████████████████████] 100% (5/5) | 1.84 MB/s | ETA: 00:00   

==========================================================
  ✓ DOWNLOAD COMPLETE!
  Saved to: /home/priyansu/bittorrent-client/debian-12.9.0-amd64-netinst.iso
==========================================================
```

> **Note on Fast Testing Mode**: In `src/torrent_client.cpp`, `target_pieces` is set to `std::min<size_t>(5, piece_mgr.total_pieces())` so you can verify tracker discovery, peer handshakes, SHA-1 hashing, and disk writes in ~3 seconds. To download the entire file, change that line to:
> ```cpp
> size_t target_pieces = piece_mgr.total_pieces();
> ```

---

## Running Unit Tests

The project uses **GoogleTest** to validate Bencode parsing, 68-byte Handshake serialization, and Peer Wire Protocol message framing.

From inside the `build/` directory:

```bash
ctest --output-on-failure
```

Or run individual test binaries directly:

```bash
./test_bencode
./test_handshake
./test_message
```

---

## Verifying Downloaded Files

You can independently verify that the bytes downloaded and written to disk match the official `.torrent` cryptographic hashes:

1. **Inspect the bootable ISO 9660 header (Piece 0):**
   ```bash
   file *.iso
   # Output: debian-12.9.0-amd64-netinst.iso: ISO 9660 CD-ROM filesystem data 'Debian ...' (bootable)
   ```

2. **Verify Piece 0 SHA-1 Checksum from Disk:**
   ```bash
   dd if=$(ls *.iso | head -n 1) bs=262144 count=1 2>/dev/null | sha1sum
   ```
