#include "bittorrent/message.hpp"
#include <gtest/gtest.h>

TEST(HandshakeTest, SerializeDeserializeRoundTrip) {
    bittorrent::Handshake hs;
    hs.info_hash.fill(0xAA);
    hs.peer_id.fill(0xBB);

    std::vector<uint8_t> raw = hs.serialize();
    ASSERT_EQ(raw.size(), 68u);

    bittorrent::Handshake parsed = bittorrent::Handshake::deserialize(raw);
    EXPECT_EQ(parsed.info_hash, hs.info_hash);
    EXPECT_EQ(parsed.peer_id, hs.peer_id);
}
