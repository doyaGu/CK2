#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

static void LoadCursorPayload(CKStateChunk &chunk) {
    int wire[] = {0x00070103, 6, 11, 22, 33, 44, 55, 66};
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

static void ExpectCursorPayload(CKStateChunk &chunk, const int *expected, int count) {
    ASSERT_EQ(count * static_cast<int>(sizeof(int)), chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], chunk.ReadInt());
}

TEST(CKStateChunkCursorTest, NoParserOperationsDoNotCreateStorageOrMovePosition) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(nullptr, chunk.LockReadBuffer());
    EXPECT_EQ(nullptr, chunk.LockWriteBuffer(8));
    chunk.Skip(3);
    chunk.Goto(2);
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.GetDataSize());
}

TEST(CKStateChunkCursorTest, LiteralReadLocksExposeCurrentWordWithoutAdvancing) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadCursorPayload(chunk);
    chunk.StartRead();
    const int *begin = static_cast<const int *>(chunk.LockReadBuffer());
    ASSERT_NE(nullptr, begin);
    EXPECT_EQ(11, begin[0]);
    EXPECT_EQ(0, chunk.GetCurrentPos());
    chunk.Skip(3);
    EXPECT_EQ(begin + 3, chunk.LockReadBuffer());
    EXPECT_EQ(44, chunk.ReadInt());
    chunk.Goto(1);
    EXPECT_EQ(begin + 1, chunk.LockReadBuffer());
    EXPECT_EQ(22, chunk.ReadInt());
    EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST(CKStateChunkCursorTest, ValidBackwardSkipAndGotoPreservePayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadCursorPayload(chunk);
    chunk.StartRead();
    chunk.Skip(5);
    chunk.Skip(-2);
    EXPECT_EQ(3, chunk.GetCurrentPos());
    EXPECT_EQ(44, chunk.ReadInt());
    chunk.Goto(0);
    EXPECT_EQ(11, chunk.ReadInt());
    const int expected[] = {11, 22, 33, 44, 55, 66};
    ExpectCursorPayload(chunk, expected, 6);
}

TEST(CKStateChunkCursorTest, WriteReservationDoesNotPublishWordsUntilCallerAdvances) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    int *words = static_cast<int *>(chunk.LockWriteBuffer(3));
    ASSERT_NE(nullptr, words);
    words[0] = 11; words[1] = 22; words[2] = 33;
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(words, chunk.LockReadBuffer());
    chunk.Skip(3);
    EXPECT_EQ(3, chunk.GetCurrentPos());
    chunk.UpdateDataSize();
    EXPECT_EQ(12, chunk.GetDataSize());
    chunk.CloseChunk();
    const int expected[] = {11, 22, 33};
    ExpectCursorPayload(chunk, expected, 3);
}

TEST(CKStateChunkCursorTest, LargeWriteReservationPreservesPreviouslyWrittenPrefix) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    for (int i = 0; i < 512; ++i) chunk.WriteInt(0x10000 + i);
    int *tail = static_cast<int *>(chunk.LockWriteBuffer(2000));
    ASSERT_NE(nullptr, tail);
    EXPECT_EQ(512, chunk.GetCurrentPos());
    for (int i = 0; i < 2000; ++i) tail[i] = 0x20000 + i;
    chunk.Skip(2000);
    chunk.CloseChunk();
    EXPECT_EQ(2512 * static_cast<int>(sizeof(int)), chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < 512; ++i) EXPECT_EQ(0x10000 + i, chunk.ReadInt());
    for (int i = 0; i < 2000; ++i) EXPECT_EQ(0x20000 + i, chunk.ReadInt());
}

TEST(CKStateChunkCursorTest, WriteLocksAndGotoAllowBackpatchWithoutTruncatingPublishedData) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    int *data = static_cast<int *>(chunk.LockWriteBuffer(7));
    ASSERT_NE(nullptr, data);
    const int initial[] = {0xAAA1, 0, 10, 11, 0xBBB1, 0, 12};
    std::memcpy(data, initial, sizeof(initial));
    chunk.Skip(7);
    chunk.UpdateDataSize();
    chunk.Goto(1);
    int *link = static_cast<int *>(chunk.LockWriteBuffer(1));
    ASSERT_NE(nullptr, link);
    *link = 4;
    EXPECT_EQ(1, chunk.GetCurrentPos());
    chunk.CloseChunk();
    EXPECT_EQ(28, chunk.GetDataSize());
    chunk.StartRead();
    ASSERT_TRUE(chunk.SeekIdentifier(0xBBB1));
    EXPECT_EQ(12, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(0xAAA1));
    EXPECT_EQ(10, chunk.ReadInt());
    EXPECT_EQ(11, chunk.ReadInt());
}

TEST(CKStateChunkCursorTest, StartReadAfterCloseAndExistingParserAlwaysRewindsReadPosition) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadCursorPayload(chunk);
    chunk.StartRead();
    EXPECT_EQ(11, chunk.ReadInt());
    chunk.Skip(4);
    EXPECT_EQ(66, chunk.ReadInt());
    chunk.StartRead();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
    chunk.CloseChunk();
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    chunk.StartRead();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
}

TEST(CKStateChunkCursorTest, EndPositionLockCanBeRewoundWithoutAccessingPastPayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadCursorPayload(chunk);
    chunk.StartRead();
    const int *begin = static_cast<const int *>(chunk.LockReadBuffer());
    chunk.Skip(6);
    EXPECT_EQ(6, chunk.GetCurrentPos());
    EXPECT_EQ(begin + 6, chunk.LockReadBuffer());
    chunk.Goto(5);
    EXPECT_EQ(66, chunk.ReadInt());
    chunk.Skip(0);
    EXPECT_EQ(6, chunk.GetCurrentPos());
}
