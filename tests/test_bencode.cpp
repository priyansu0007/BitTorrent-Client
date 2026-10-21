#include <gtest/gtest.h>
#include "bittorrent/bencode.hpp"

using namespace bittorrent;

// --- Integer Parsing ---
TEST(BencodeTest, ParseValidIntegers) {
    EXPECT_EQ(BencodeParser("i42e").parse().as_int(), 42);
    EXPECT_EQ(BencodeParser("i-42e").parse().as_int(), -42);
    EXPECT_EQ(BencodeParser("i0e").parse().as_int(), 0);
    EXPECT_EQ(BencodeParser("i1048576e").parse().as_int(), 1048576);
}

TEST(BencodeTest, RejectInvalidIntegers) {
    // BEP 0003: no negative zero
    EXPECT_THROW(BencodeParser("i-0e").parse(), std::runtime_error);
    // BEP 0003: no leading zeros
    EXPECT_THROW(BencodeParser("i03e").parse(), std::runtime_error);
    EXPECT_THROW(BencodeParser("i-03e").parse(), std::runtime_error);
    // Empty integer content
    EXPECT_THROW(BencodeParser("ie").parse(), std::runtime_error);
    // Unterminated integer
    EXPECT_THROW(BencodeParser("i42").parse(), std::runtime_error);
}

// --- String Parsing ---
TEST(BencodeTest, ParseValidStrings) {
    EXPECT_EQ(BencodeParser("4:spam").parse().as_string(), "spam");
    EXPECT_EQ(BencodeParser("0:").parse().as_string(), "");
    EXPECT_EQ(BencodeParser("9:bittorrent").parse().as_string(), "bittorrent");
}

TEST(BencodeTest, RejectInvalidStrings) {
    // Declared length greater than available buffer
    EXPECT_THROW(BencodeParser("10:short").parse(), std::runtime_error);
    // Missing colon delimiter
    EXPECT_THROW(BencodeParser("4spam").parse(), std::runtime_error);
}

// --- List Parsing ---
TEST(BencodeTest, ParseLists) {
    auto val = BencodeParser("l4:spami42ee").parse();
    ASSERT_TRUE(val.is_list());
    const auto& list = val.as_list();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].as_string(), "spam");
    EXPECT_EQ(list[1].as_int(), 42);

    // Empty list
    auto empty = BencodeParser("le").parse();
    ASSERT_TRUE(empty.is_list());
    EXPECT_TRUE(empty.as_list().empty());
}

// --- Dictionary & Info-Hash Slice ---
TEST(BencodeTest, ParseDictAndCaptureInfoSlice) {
    // "http://tracker.com" has 18 characters
    std::string data = "d4:infod4:name4:test12:piece lengthi262144ee8:announce18:http://tracker.come";
    BencodeParser parser(data);
    auto root = parser.parse();

    ASSERT_TRUE(root.is_dict());
    const auto& dict = root.as_dict();
    EXPECT_TRUE(dict.contains("info"));
    EXPECT_TRUE(dict.contains("announce"));
    EXPECT_EQ(dict.at("announce").as_string(), "http://tracker.com");

    // Exact raw info dict bytes for SHA-1 calculation
    EXPECT_EQ(parser.get_raw_info_dict(), "d4:name4:test12:piece lengthi262144ee");
}