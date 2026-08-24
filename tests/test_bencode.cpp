#include <gtest/gtest.h>
#include "bittorrent/bencode.hpp"

using namespace bittorrent;

TEST(BencodeTest, ParseIntegers) {
    EXPECT_EQ(BencodeParser("i42e").parse().as_int(), 42);
    EXPECT_EQ(BencodeParser("i-42e").parse().as_int(), -42);
    EXPECT_EQ(BencodeParser("i0e").parse().as_int(), 0);
    EXPECT_THROW(BencodeParser("i-0e").parse(), std::runtime_error);
    EXPECT_THROW(BencodeParser("i03e").parse(), std::runtime_error);
}

TEST(BencodeTest, ParseStrings) {
    EXPECT_EQ(BencodeParser("4:spam").parse().as_string(), "spam");
    EXPECT_EQ(BencodeParser("0:").parse().as_string(), "");
    EXPECT_THROW(BencodeParser("10:short").parse(), std::runtime_error);
}

TEST(BencodeTest, ParseList) {
    auto val = BencodeParser("l4:spami42ee").parse();
    ASSERT_TRUE(val.is_list());
    const auto& list = val.as_list();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].as_string(), "spam");
    EXPECT_EQ(list[1].as_int(), 42);
}

TEST(BencodeTest, ParseDictAndCaptureInfoSlice) {
    // 18:http://tracker.com (18 bytes)
    std::string data = "d4:infod4:name4:test12:piece lengthi262144ee8:announce18:http://tracker.come";
    BencodeParser parser(data);
    auto root = parser.parse();
    
    ASSERT_TRUE(root.is_dict());
    EXPECT_EQ(parser.get_raw_info_dict(), "d4:name4:test12:piece lengthi262144ee");
}