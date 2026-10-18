#include <gtest/gtest.h>

#include <cstring>

#include "CKFile.h"

// Original CK2 2.5.0.48 memory/file parser probes leave this literal unchanged
// after Encode(17, key), at cursors 3 and 7 with two nonzero keys. This is the
// unsupported legacy codec contract, not proof of successful 2.1 encryption.
static const unsigned char PlainBytes[32] = {
    0x05, 0x0c, 0x13, 0x1a, 0x21, 0x28, 0x2f, 0x36,
    0x3d, 0x44, 0x4b, 0x52, 0x59, 0x60, 0x67, 0x6e,
    0x75, 0x7c, 0x83, 0x8a, 0x91, 0x98, 0x9f, 0xa6,
    0xad, 0xb4, 0xbb, 0xc2, 0xc9, 0xd0, 0xd7, 0xde,
};

TEST(CKBufferParserEncodeContractTest, BorrowedLegacyEncodePreservesLiteralSpanAndFollowingRead) {
    unsigned char bytes[32];
    std::memcpy(bytes, PlainBytes, sizeof(bytes));
    CKDWORD key[4] = {0x01234567u, 0x89abcdefu, 0xfedcba98u, 0x76543210u};
    const CKDWORD expectedKey[4] = {0x01234567u, 0x89abcdefu, 0xfedcba98u, 0x76543210u};
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(3);

    parser.Encode(17, key);

    EXPECT_EQ(3, parser.CursorPos());
    EXPECT_EQ(32, parser.Size());
    EXPECT_FALSE(parser.IsValid()); // m_Valid is ownership, not codec success.
    EXPECT_EQ(reinterpret_cast<char *>(bytes), parser.m_Buffer);
    EXPECT_EQ(0, std::memcmp(PlainBytes, bytes, sizeof(bytes)));
    EXPECT_EQ(0, std::memcmp(expectedKey, key, sizeof(key)));
    EXPECT_EQ(0x2f28211a, parser.ReadInt());
    EXPECT_EQ(7, parser.CursorPos());
    EXPECT_EQ(0, std::memcmp(PlainBytes, bytes, sizeof(bytes)));
}

TEST(CKBufferParserEncodeContractTest, OwnedLegacyEncodeKeepsIndependentStorageAndOwner) {
    unsigned char bytes[32];
    std::memcpy(bytes, PlainBytes, sizeof(bytes));
    CKBufferParser source(bytes, sizeof(bytes));
    CKBufferParser *owned = source.Extract(sizeof(bytes));
    ASSERT_NE(nullptr, owned);
    ASSERT_TRUE(owned->IsValid());
    ASSERT_NE(reinterpret_cast<char *>(bytes), owned->m_Buffer);
    char *storage = owned->m_Buffer;
    bytes[7] = 0xff; // The owned child must retain its independently copied data.
    owned->Seek(7);
    CKDWORD key[4] = {0xcc55aa11u, 0xdeadbeefu, 0x1234abcdu, 0xff102030u};
    const CKDWORD expectedKey[4] = {0xcc55aa11u, 0xdeadbeefu, 0x1234abcdu, 0xff102030u};

    owned->Encode(17, key);

    EXPECT_EQ(storage, owned->m_Buffer);
    EXPECT_TRUE(owned->IsValid());
    EXPECT_EQ(7, owned->CursorPos());
    EXPECT_EQ(32, owned->Size());
    EXPECT_EQ(32, source.CursorPos());
    EXPECT_EQ(0, std::memcmp(PlainBytes, storage, sizeof(PlainBytes)));
    EXPECT_EQ(0, std::memcmp(expectedKey, key, sizeof(key)));
    unsigned char readback[17] = {};
    EXPECT_TRUE(owned->Read(readback, sizeof(readback)));
    EXPECT_EQ(0, std::memcmp(PlainBytes + 7, readback, sizeof(readback)));
    EXPECT_EQ(24, owned->CursorPos());
    delete owned;
    EXPECT_EQ(0xff, bytes[7]);
    EXPECT_EQ(PlainBytes[31], bytes[31]);
}

TEST(CKBufferParserEncodeContractTest, LegacyEncodeDoesNotChangeIndependentSeededChecksum) {
    unsigned char bytes[32];
    std::memcpy(bytes, PlainBytes, sizeof(bytes));
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(7);
    CKDWORD key[4] = {0xcc55aa11u, 0xdeadbeefu, 0x1234abcdu, 0xff102030u};

    parser.Encode(17, key);

    // Python zlib.adler32(PlainBytes[7:24], 0x10203040), independent of CK2.
    EXPECT_EQ(0x7b23378eu, parser.ComputeCRC(17, 0x10203040u));
    EXPECT_EQ(7, parser.CursorPos());
    unsigned char readback[17] = {};
    EXPECT_TRUE(parser.Read(readback, sizeof(readback)));
    EXPECT_EQ(0, std::memcmp(PlainBytes + 7, readback, sizeof(readback)));
    EXPECT_EQ(24, parser.CursorPos());
}
