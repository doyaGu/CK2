#include <gtest/gtest.h>

#include "CKAll.h"

namespace {

const CKDWORD IdentifierA = 0x10010001u;
const CKDWORD IdentifierB = 0x10010002u;
const CKDWORD IdentifierC = 0x10010003u;
const CKDWORD IdentifierD = 0x10010004u;
const CKDWORD MissingIdentifier = 0x100100ffu;

// Independent original linked identifier wire. Offsets are in dwords:
// A@0 -> B@5 -> A@8 -> D@12 -> C@14 -> end.
// A payload deliberately contains B's identifier value; lookup must follow
// header links, rather than scan raw values. D and terminal C have no payload.
const CKDWORD IdentifierWire[] = {
    IdentifierA, 5, 11, IdentifierB, 22,
    IdentifierB, 8, 33,
    IdentifierA, 12, 44, 55,
    IdentifierD, 14,
    IdentifierC, 0
};

void LoadIdentifierWire(CKStateChunk &chunk) {
    chunk.StartWrite();
    chunk.WriteBufferNoSize_LEndian(sizeof(IdentifierWire), const_cast<CKDWORD *>(IdentifierWire));
    chunk.CloseChunk();
}

} // namespace

TEST(CKStateChunkIdentifierTest, RawWireReadsHeadersAndPayloadSequentially) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    chunk.StartRead();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(IdentifierA, chunk.ReadIdentifier());
    EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
    EXPECT_EQ(IdentifierB, chunk.ReadDword());
    EXPECT_EQ(22, chunk.ReadInt());
    EXPECT_EQ(IdentifierB, chunk.ReadIdentifier());
    EXPECT_EQ(7, chunk.GetCurrentPos());
    EXPECT_EQ(33, chunk.ReadInt());
    EXPECT_EQ(IdentifierA, chunk.ReadIdentifier());
    EXPECT_EQ(44, chunk.ReadInt());
    EXPECT_EQ(55, chunk.ReadInt());
    EXPECT_EQ(IdentifierD, chunk.ReadIdentifier());
    EXPECT_EQ(IdentifierC, chunk.ReadIdentifier());
    EXPECT_EQ(16, chunk.GetCurrentPos());
    EXPECT_EQ(0u, chunk.ReadIdentifier());
    EXPECT_EQ(16, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, SeekCreatesReadStateAndIgnoresPayloadLookalikes) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    ASSERT_EQ(-1, chunk.GetCurrentPos());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierB));
    EXPECT_EQ(7, chunk.GetCurrentPos());
    EXPECT_EQ(33, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierC));
    EXPECT_EQ(16, chunk.GetCurrentPos());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierB));
    EXPECT_EQ(7, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, DuplicateSeekStartsAfterPreviousAndWraps) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    chunk.StartRead();
    // Native previous-identifier offset starts at zero. The first lookup of
    // duplicate A therefore reaches A@8 before wrapping back to A@0.
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
    EXPECT_EQ(44, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, FailedSeekPreservesPayloadCursorAndDuplicateOrder) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    chunk.StartRead();
    ASSERT_EQ(IdentifierA, chunk.ReadIdentifier());
    ASSERT_EQ(11, chunk.ReadInt());
    ASSERT_EQ(3, chunk.GetCurrentPos());
    EXPECT_FALSE(chunk.SeekIdentifier(MissingIdentifier));
    EXPECT_EQ(3, chunk.GetCurrentPos());
    EXPECT_EQ(IdentifierB, chunk.ReadDword());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, ReadIdentifierUpdatesPreviousForTheNextSeek) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    chunk.StartRead();
    chunk.Goto(8); // A valid later header, not arbitrary payload scanning.
    ASSERT_EQ(IdentifierA, chunk.ReadIdentifier());
    ASSERT_EQ(10, chunk.GetCurrentPos());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
}

TEST(CKStateChunkIdentifierTest, SizeQueryAlwaysChoosesFirstDuplicateAndPositionsPayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA)); // Second A selected.
    ASSERT_EQ(10, chunk.GetCurrentPos());
    EXPECT_EQ(12, chunk.SeekIdentifierAndReturnSize(IdentifierA));
    EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(11, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, SizeQueryCountsBytesAndExcludesBothHeaderWords) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    EXPECT_EQ(4, chunk.SeekIdentifierAndReturnSize(IdentifierB));
    EXPECT_EQ(7, chunk.GetCurrentPos());
    EXPECT_EQ(33, chunk.ReadInt());
    EXPECT_EQ(0, chunk.SeekIdentifierAndReturnSize(IdentifierD));
    EXPECT_EQ(14, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.SeekIdentifierAndReturnSize(IdentifierC));
    EXPECT_EQ(16, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, TerminalBlockSizeUsesChunkLength) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    const CKDWORD wire[] = {IdentifierA, 4, 91, 92, IdentifierC, 0, 71, 72, 73};
    chunk.StartWrite();
    chunk.WriteBufferNoSize_LEndian(sizeof(wire), const_cast<CKDWORD *>(wire));
    chunk.CloseChunk();
    EXPECT_EQ(12, chunk.SeekIdentifierAndReturnSize(IdentifierC));
    EXPECT_EQ(6, chunk.GetCurrentPos());
    EXPECT_EQ(71, chunk.ReadInt());
    EXPECT_EQ(72, chunk.ReadInt());
    EXPECT_EQ(73, chunk.ReadInt());
    EXPECT_EQ(0u, chunk.ReadIdentifier());
    EXPECT_EQ(9, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, FailedSizeQueryPreservesCursorAndPreviousIdentifier) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    EXPECT_EQ(4, chunk.SeekIdentifierAndReturnSize(IdentifierB));
    EXPECT_EQ(-1, chunk.SeekIdentifierAndReturnSize(MissingIdentifier));
    EXPECT_EQ(7, chunk.GetCurrentPos());
    EXPECT_EQ(33, chunk.ReadInt());
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, MissingQueryLazilyCreatesReadStateButEmptyChunkDoesNot) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    ASSERT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(-1, chunk.SeekIdentifierAndReturnSize(MissingIdentifier));
    EXPECT_EQ(0, chunk.GetCurrentPos());
    CKStateChunk empty(CKCID_OBJECT, nullptr);
    EXPECT_FALSE(empty.SeekIdentifier(IdentifierA));
    EXPECT_EQ(-1, empty.SeekIdentifierAndReturnSize(IdentifierA));
    EXPECT_EQ(-1, empty.GetCurrentPos());
    empty.StartRead();
    EXPECT_EQ(0u, empty.ReadIdentifier());
    EXPECT_EQ(0, empty.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, StartReadResetsCursorAndPreviousIdentifier) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadIdentifierWire(chunk);
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(44, chunk.ReadInt());
    chunk.StartRead();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(IdentifierA, chunk.ReadIdentifier());
    EXPECT_EQ(11, chunk.ReadInt());
    chunk.StartRead();
    ASSERT_TRUE(chunk.SeekIdentifier(IdentifierA));
    EXPECT_EQ(10, chunk.GetCurrentPos());
}

TEST(CKStateChunkIdentifierTest, ZeroIdentifierIsAnActualHeaderNotTheLinkTerminator) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    const CKDWORD wire[] = {IdentifierA, 3, 13, 0, 0, 27};
    chunk.StartWrite();
    chunk.WriteBufferNoSize_LEndian(sizeof(wire), const_cast<CKDWORD *>(wire));
    chunk.CloseChunk();
    ASSERT_TRUE(chunk.SeekIdentifier(0));
    EXPECT_EQ(5, chunk.GetCurrentPos());
    EXPECT_EQ(27, chunk.ReadInt());
    EXPECT_EQ(4, chunk.SeekIdentifierAndReturnSize(0));
    EXPECT_EQ(5, chunk.GetCurrentPos());
    EXPECT_EQ(27, chunk.ReadInt());
}
