#include <gtest/gtest.h>

#include <cstring>
#include "CKAll.h"

namespace {

void FillNativeProbeBytes(unsigned char *bytes) {
    // Independent sentinels used by the actual shipping 2.5 parser probe.
    for (int i = 0; i < 32; ++i) bytes[i] = static_cast<unsigned char>(i * 7 + 5);
}

void FillNativeProbeKey(CKDWORD *key, int scenario) {
    if (scenario == 0) {
        key[0] = 0x01234567u; key[1] = 0x89ABCDEFu;
        key[2] = 0xFEDCBA98u; key[3] = 0x76543210u;
    } else {
        key[0] = 0xCC55AA11u; key[1] = 0xDEADBEEFu;
        key[2] = 0x1234ABCDu; key[3] = 0xFF102030u;
    }
}

TEST(CKBufferParserDecodedContractTest, UnsupportedDecodedBorrowedSlicesRetainNativeObservedStateAndAllowRawExtraction) {
    // The oracle is actual original 2.5 memory/file parser virtual dispatch:
    // null results and unchanged source at these keys, cursors and spans.
    // This certifies the current unsupported boundary, not a 2.1 cipher.
    unsigned char bytes[32], savedBytes[32];
    FillNativeProbeBytes(bytes);
    std::memcpy(savedBytes, bytes, sizeof(bytes));
    CKBufferParser source(bytes, sizeof(bytes));
    for (int scenario = 0; scenario < 2; ++scenario) {
        CKDWORD key[4], savedKey[4];
        FillNativeProbeKey(key, scenario);
        std::memcpy(savedKey, key, sizeof(key));
        const int cursor = scenario == 0 ? 3 : 7;
        const int sizes[] = {1, 8, 17, 32 - cursor};
        source.Seek(cursor);
        for (int i = 0; i < 4; ++i) {
            EXPECT_EQ(nullptr, source.ExtractDecoded(sizes[i], key));
            EXPECT_EQ(cursor, source.CursorPos());
            EXPECT_EQ(32, source.Size());
            EXPECT_FALSE(source.IsValid());
            EXPECT_EQ(reinterpret_cast<char *>(bytes), source.m_Buffer);
            EXPECT_EQ(0, std::memcmp(bytes, savedBytes, sizeof(bytes)));
            EXPECT_EQ(0, std::memcmp(key, savedKey, sizeof(key)));
        }
        unsigned char remaining[29];
        ASSERT_TRUE(source.Read(remaining, 32 - cursor));
        EXPECT_EQ(0, std::memcmp(remaining, savedBytes + cursor, 32 - cursor));
        EXPECT_EQ(32, source.CursorPos());
    }
    source.Seek(3);
    CKDWORD key[4];
    FillNativeProbeKey(key, 0);
    ASSERT_EQ(nullptr, source.ExtractDecoded(17, key));
    CKBufferParser *raw = source.Extract(17);
    ASSERT_NE(nullptr, raw);
    EXPECT_TRUE(raw->IsValid());
    EXPECT_NE(source.m_Buffer, raw->m_Buffer);
    EXPECT_EQ(0, raw->CursorPos());
    EXPECT_EQ(17, raw->Size());
    EXPECT_EQ(0, std::memcmp(raw->m_Buffer, savedBytes + 3, 17));
    EXPECT_EQ(20, source.CursorPos());
    delete raw;
    unsigned char tail[12];
    ASSERT_TRUE(source.Read(tail, sizeof(tail)));
    EXPECT_EQ(0, std::memcmp(tail, savedBytes + 20, sizeof(tail)));
    EXPECT_EQ(32, source.CursorPos());
    EXPECT_EQ(0, std::memcmp(bytes, savedBytes, sizeof(bytes)));
}

TEST(CKBufferParserDecodedContractTest, UnsupportedDecodedOwnedSlicePreservesOwnerAndCanStillBeReadAndReleased) {
    unsigned char bytes[32], savedBytes[32];
    FillNativeProbeBytes(bytes);
    std::memcpy(savedBytes, bytes, sizeof(bytes));
    CKBufferParser borrowed(bytes, sizeof(bytes));
    CKBufferParser *owned = borrowed.Extract(sizeof(bytes));
    ASSERT_NE(nullptr, owned);
    ASSERT_TRUE(owned->IsValid());
    ASSERT_NE(borrowed.m_Buffer, owned->m_Buffer);
    // A caller changing its original buffer must not affect the owned slice.
    std::memset(bytes, 0xA5, sizeof(bytes));
    CKDWORD key[4], savedKey[4];
    FillNativeProbeKey(key, 1);
    std::memcpy(savedKey, key, sizeof(key));
    owned->Seek(7);
    EXPECT_EQ(nullptr, owned->ExtractDecoded(17, key));
    EXPECT_EQ(7, owned->CursorPos());
    EXPECT_EQ(32, owned->Size());
    EXPECT_TRUE(owned->IsValid());
    EXPECT_EQ(0, std::memcmp(owned->m_Buffer, savedBytes, sizeof(bytes)));
    EXPECT_EQ(0, std::memcmp(key, savedKey, sizeof(key)));
    unsigned char remaining[25];
    EXPECT_TRUE(owned->Read(remaining, sizeof(remaining)));
    EXPECT_EQ(0, std::memcmp(remaining, savedBytes + 7, sizeof(remaining)));
    EXPECT_EQ(32, owned->CursorPos());
    delete owned;
    EXPECT_FALSE(borrowed.IsValid());
    EXPECT_EQ(reinterpret_cast<char *>(bytes), borrowed.m_Buffer);
    EXPECT_EQ(32, borrowed.CursorPos());
    for (int i = 0; i < 32; ++i) EXPECT_EQ(0xA5, bytes[i]);
}

} // namespace
