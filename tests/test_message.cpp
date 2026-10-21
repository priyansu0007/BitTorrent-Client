#include "bittorrent/message.hpp"
#include <gtest/gtest.h>

TEST(MessageTest, InterestedSerialization) {
    auto msg = bittorrent::Message::make_interested();
    auto raw = msg.serialize();
    ASSERT_EQ(raw.size(), 5u);
    EXPECT_EQ(raw[3], 1);
    EXPECT_EQ(raw[4], static_cast<uint8_t>(bittorrent::MessageId::Interested));
}

TEST(MessageTest, RequestSerialization) {
    auto msg = bittorrent::Message::make_request(1, 16384, 16384);
    auto raw = msg.serialize();
    ASSERT_EQ(raw.size(), 17u);
    EXPECT_EQ(raw[3], 13);
    EXPECT_EQ(raw[4], static_cast<uint8_t>(bittorrent::MessageId::Request));
}
