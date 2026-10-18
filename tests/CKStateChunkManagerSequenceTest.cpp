#include <gtest/gtest.h>

#include "CKAll.h"

static const int GuidA1 = 0x12345678;
static const int GuidA2 = 0x23456789;
static const int GuidB1 = 0x3456789A;
static const int GuidB2 = 0x456789AB;
static const CKGUID GuidA(GuidA1, GuidA2);
static const CKGUID GuidB(GuidB1, GuidB2);
static const CKGUID UntouchedGuid(0x11223344, 0x22334455);

// Literal modern envelopes have no manager relocation list. Reader fixtures
// therefore do not depend on any manager writer or on self-roundtrip output.
static void LoadManagerWords(CKStateChunk &chunk, const int *words, int count) {
    int wire[32] = {};
    ASSERT_LE(count, 30);
    wire[0] = (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8);
    wire[1] = count;
    for (int i = 0; i < count; ++i) wire[i + 2] = words[i];
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead();
}

static void ExpectManagerWire(CKStateChunk &chunk, const int *words, int count,
                       const int *offsets, int offsetCount) {
    int wire[1024] = {};
    ASSERT_LE(count + offsetCount + 3, 1024);
    ASSERT_EQ((count + offsetCount + 3) * 4, chunk.ConvertToBuffer(wire));
    EXPECT_EQ((CHNK_OPTION_MAN << 24) | (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), wire[0]);
    EXPECT_EQ(count, wire[1]);
    for (int i = 0; i < count; ++i) EXPECT_EQ(words[i], wire[i + 2]) << "payload " << i;
    EXPECT_EQ(offsetCount, wire[count + 2]);
    for (int i = 0; i < offsetCount; ++i) EXPECT_EQ(offsets[i], wire[count + 3 + i]) << "relocation " << i;
}

TEST(CKStateChunkManagerSequenceTest, SingleLiteralReadsGuidSignedValueAndLeavesFollower) {
    const int words[] = {GuidA1, GuidA2, -43, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 4);
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(-43, chunk.ReadManagerInt(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(47, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, SingleNullGuidSkipsExactlyTwoGuidWords) {
    const int words[] = {GuidA1, GuidA2, 0, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 4);
    EXPECT_EQ(0, chunk.ReadManagerInt(nullptr)); EXPECT_EQ(3, chunk.GetCurrentPos());
    EXPECT_EQ(47, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, SequenceLiteralReadsOnlyHeaderThenSignedValues) {
    const int words[] = {3, GuidA1, GuidA2, 41, -43, 0, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 7);
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(3, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(41, chunk.ReadManagerIntSequence());
    EXPECT_EQ(-43, chunk.ReadManagerIntSequence()); EXPECT_EQ(0, chunk.ReadManagerIntSequence());
    EXPECT_EQ(6, chunk.GetCurrentPos()); EXPECT_EQ(47, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, ZeroCountNullGuidStillConsumesWholeHeader) {
    const int words[] = {0, GuidA1, GuidA2, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 4);
    EXPECT_EQ(0, chunk.StartManagerReadSequence(nullptr)); EXPECT_EQ(3, chunk.GetCurrentPos());
    EXPECT_EQ(47, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, SequenceHeaderReturnsRawSignedCount) {
    const int words[] = {-3, GuidA1, GuidA2, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 4);
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(-3, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(47, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, CompleteHeaderDoesNotPrevalidateDeclaredPayload) {
    const int words[] = {5, GuidA1, GuidA2, 41};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 4);
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(5, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(41, chunk.ReadManagerIntSequence());
    EXPECT_EQ(0, chunk.ReadManagerIntSequence()); EXPECT_EQ(4, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SequenceValueEOFIsInertAtNonzeroCursor) {
    const int words[] = {-41};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 1);
    EXPECT_EQ(-41, chunk.ReadManagerIntSequence()); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.ReadManagerIntSequence()); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.ReadManagerIntSequence()); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SequenceValueWithoutParserIsInert) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(0, chunk.ReadManagerIntSequence()); EXPECT_EQ(-1, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SingleWithoutParserPreservesGuid) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); CKGUID guid = UntouchedGuid;
    EXPECT_EQ(0, chunk.ReadManagerInt(&guid)); EXPECT_EQ(UntouchedGuid, guid);
    EXPECT_EQ(-1, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SequenceHeaderWithoutParserPreservesGuid) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); CKGUID guid = UntouchedGuid;
    EXPECT_EQ(0, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(UntouchedGuid, guid);
    EXPECT_EQ(-1, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SingleIncompleteFrameDoesNotConsumeAvailableWords) {
    const int words[] = {GuidA1, GuidA2};
    for (int count = 1; count < 3; ++count) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, count);
        CKGUID guid = UntouchedGuid;
        EXPECT_EQ(0, chunk.ReadManagerInt(&guid)); EXPECT_EQ(UntouchedGuid, guid);
        EXPECT_EQ(0, chunk.GetCurrentPos()); EXPECT_EQ(GuidA1, chunk.ReadInt());
    }
}

TEST(CKStateChunkManagerSequenceTest, SequenceIncompleteHeaderDoesNotConsumeAvailableWords) {
    const int words[] = {5, GuidA1};
    for (int count = 1; count < 3; ++count) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, count);
        CKGUID guid = UntouchedGuid;
        EXPECT_EQ(0, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(UntouchedGuid, guid);
        EXPECT_EQ(0, chunk.GetCurrentPos()); EXPECT_EQ(5, chunk.ReadInt());
    }
}

TEST(CKStateChunkManagerSequenceTest, SingleEOFWithNullGuidDoesNotAdvancePastPayload) {
    const int words[] = {41};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 1);
    EXPECT_EQ(41, chunk.ReadInt());
    EXPECT_EQ(0, chunk.ReadManagerInt(nullptr)); EXPECT_EQ(1, chunk.GetCurrentPos());
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(0, chunk.ReadManagerInt(&guid)); EXPECT_EQ(UntouchedGuid, guid);
    EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SequenceHeaderEOFAfterZeroCountDoesNotAdvanceOrAssignGuid) {
    const int words[] = {0, GuidA1, GuidA2};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadManagerWords(chunk, words, 3);
    EXPECT_EQ(0, chunk.StartManagerReadSequence(nullptr)); EXPECT_EQ(3, chunk.GetCurrentPos());
    CKGUID guid = UntouchedGuid;
    EXPECT_EQ(0, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(UntouchedGuid, guid);
    EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkManagerSequenceTest, SingleWriterMatchesLiteralPayloadAndRelocationOffset) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteInt(41);
    chunk.WriteManagerInt(GuidA, -43); chunk.WriteInt(47); chunk.CloseChunk();
    const int words[] = {41, GuidA1, GuidA2, -43, 47}; const int offsets[] = {1};
    ExpectManagerWire(chunk, words, 5, offsets, 1);
}

TEST(CKStateChunkManagerSequenceTest, ZeroCountWriterStillEmitsGuidAndSequenceRelocation) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.StartManagerSequence(GuidA, 0); chunk.CloseChunk();
    const int words[] = {0, GuidA1, GuidA2}; const int offsets[] = {-1, 0};
    ExpectManagerWire(chunk, words, 3, offsets, 2);
}

TEST(CKStateChunkManagerSequenceTest, InterleavedWritersHaveLiteralOffsetsAndIndependentManagerRemaps) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteManagerInt(GuidA, 1);
    chunk.StartManagerSequence(GuidB, 3);
    chunk.WriteManagerSequence(2); chunk.WriteManagerSequence(-1); chunk.WriteManagerSequence(0);
    chunk.WriteManagerInt(GuidA, 2); chunk.StartManagerSequence(GuidA, 0); chunk.WriteInt(73);
    chunk.CloseChunk();
    const int offsets[] = {0, -1, 3, 9, -1, 12};
    const int words[] = {GuidA1, GuidA2, 1, 3, GuidB1, GuidB2, 2, -1, 0,
                         GuidA1, GuidA2, 2, 0, GuidA1, GuidA2, 73};
    ExpectManagerWire(chunk, words, 16, offsets, 6);
    chunk.StartRead(); EXPECT_EQ(GuidA1, chunk.ReadInt());
    int table[] = {101, 103, 107};
    EXPECT_EQ(2, chunk.RemapManagerInt(GuidA, table, 3)); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(3, chunk.RemapManagerInt(GuidB, table, 3)); EXPECT_EQ(1, chunk.GetCurrentPos());
    const int mapped[] = {GuidA1, GuidA2, 103, 3, GuidB1, GuidB2, 107, -1, 101,
                          GuidA1, GuidA2, 107, 0, GuidA1, GuidA2, 73};
    ExpectManagerWire(chunk, mapped, 16, offsets, 6);
    chunk.StartRead(); CKGUID guid;
    EXPECT_EQ(103, chunk.ReadManagerInt(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(3, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(GuidB, guid);
    EXPECT_EQ(107, chunk.ReadManagerIntSequence()); EXPECT_EQ(-1, chunk.ReadManagerIntSequence());
    EXPECT_EQ(101, chunk.ReadManagerIntSequence()); EXPECT_EQ(107, chunk.ReadManagerInt(nullptr));
    EXPECT_EQ(0, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(GuidA, guid);
    EXPECT_EQ(73, chunk.ReadInt());
}

TEST(CKStateChunkManagerSequenceTest, LargeSequenceReservationPreservesEveryDeclaredValue) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.StartManagerSequence(GuidA, 601);
    for (int i = 0; i < 601; ++i) chunk.WriteManagerSequence(1000 + i);
    chunk.WriteManagerInt(GuidB, -43); chunk.CloseChunk();
    EXPECT_EQ(607 * 4, chunk.GetDataSize());
    int words[607]; words[0] = 601; words[1] = GuidA1; words[2] = GuidA2;
    for (int i = 0; i < 601; ++i) words[i + 3] = 1000 + i;
    words[604] = GuidB1; words[605] = GuidB2; words[606] = -43;
    const int offsets[] = {-1, 0, 604}; ExpectManagerWire(chunk, words, 607, offsets, 3);
}

TEST(CKStateChunkManagerSequenceTest, MetadataGrowthRetainsAlternatingSingleAndSequenceOffsets) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    int words[210], offsets[105];
    for (int i = 0; i < 35; ++i) {
        chunk.WriteManagerInt(GuidA, i); chunk.StartManagerSequence(GuidB, 0);
        words[i * 6] = GuidA1; words[i * 6 + 1] = GuidA2; words[i * 6 + 2] = i;
        words[i * 6 + 3] = 0; words[i * 6 + 4] = GuidB1; words[i * 6 + 5] = GuidB2;
        offsets[i * 3] = i * 6; offsets[i * 3 + 1] = -1; offsets[i * 3 + 2] = i * 6 + 3;
    }
    chunk.CloseChunk(); ExpectManagerWire(chunk, words, 210, offsets, 105);
}
