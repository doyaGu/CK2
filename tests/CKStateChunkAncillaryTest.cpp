#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

static void LoadAncillaryWords(CKStateChunk &chunk, const int *words, int count) {
    int wire[128] = {};
    wire[0] = CHUNK_VERSION2 << 16;
    wire[1] = CKCID_BEOBJECT;
    wire[2] = count;
    ASSERT_LE(count, 121);
    std::memcpy(wire + 7, words, count * sizeof(int));
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead();
}

static void ExpectAncillaryWords(CKStateChunk &chunk, const int *words, int count) {
    ASSERT_EQ(count * static_cast<int>(sizeof(int)), chunk.GetDataSize());
    chunk.StartRead();
    const int *actual = static_cast<const int *>(chunk.LockReadBuffer());
    for (int i = 0; i < count; ++i) EXPECT_EQ(words[i], actual[i]) << "word " << i;
}

static void ExpectAttributeDescriptor(CKStateChunk &chunk, int position) {
    ASSERT_NE(nullptr, chunk.GetManagers());
    ASSERT_EQ(2, chunk.GetManagers()->Size);
    EXPECT_EQ(-1, chunk.GetManagers()->Data[0]);
    EXPECT_EQ(position, chunk.GetManagers()->Data[1]);
}

TEST(CKStateChunkAncillaryTest, AttributeFilePatchMapsTypesAndRegistersActualManagerHeader) {
    const int words[] = {0x11, 0, 4, 101, 102, 103, 104, 4, 0x3d242466, 0, 0, 3, -1, 4};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 14);
    ASSERT_TRUE(chunk.SeekIdentifier(CK_STATESAVE_NEWATTRIBUTES));
    int table[] = {10, 11, 12, 13}; chunk.AttributePatch(TRUE, table, 4);
    EXPECT_EQ(2, chunk.GetCurrentPos()); ExpectAttributeDescriptor(chunk, 7);
    const int patched[] = {0x11, 0, 4, 101, 102, 103, 104, 4, 0x3d242466, 0, 10, 13, -1, 4};
    ExpectAncillaryWords(chunk, patched, 14);
    int secondTable[14]; for (int i = 0; i < 14; ++i) secondTable[i] = 100 + i;
    EXPECT_EQ(4, chunk.RemapManagerInt(ATTRIBUTE_MANAGER_GUID, secondTable, 14));
    const int remapped[] = {0x11, 0, 4, 101, 102, 103, 104, 4, 0x3d242466, 0, 110, 113, -1, 104};
    ExpectAncillaryWords(chunk, remapped, 14);
}

TEST(CKStateChunkAncillaryTest, AttributeStatePatchWalksNullAndNonemptyChildrenWithoutChangingThem) {
    const int words[] = {0x11, 0, 2, 101, 102, 2, 0,
                         8, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 1, 0, 0, 0, 0, 0x12345678,
                         2, 0x3d242466, 0, 1, 0};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 21);
    ASSERT_TRUE(chunk.SeekIdentifier(CK_STATESAVE_NEWATTRIBUTES));
    int table[] = {20, 30}; chunk.AttributePatch(FALSE, table, 2);
    EXPECT_EQ(2, chunk.GetCurrentPos()); ExpectAttributeDescriptor(chunk, 16);
    int expected[21]; std::memcpy(expected, words, sizeof(words)); expected[19] = 30; expected[20] = 20;
    ExpectAncillaryWords(chunk, expected, 21);
    ASSERT_TRUE(chunk.SeekIdentifier(CK_STATESAVE_NEWATTRIBUTES));
    EXPECT_EQ(2, chunk.StartReadSequence()); EXPECT_EQ(101, chunk.ReadInt()); EXPECT_EQ(102, chunk.ReadInt());
    EXPECT_EQ(2, chunk.StartReadSequence()); EXPECT_EQ(nullptr, chunk.ReadSubChunk());
    CKStateChunk *child = chunk.ReadSubChunk(); ASSERT_NE(nullptr, child);
    EXPECT_EQ(CKCID_PARAMETER, child->GetChunkClassID()); child->StartRead();
    EXPECT_EQ(0x12345678, child->ReadInt()); DeleteCKStateChunk(child);
    CKGUID guid; EXPECT_EQ(2, chunk.StartManagerReadSequence(&guid)); EXPECT_EQ(ATTRIBUTE_MANAGER_GUID, guid);
    EXPECT_EQ(30, chunk.ReadInt()); EXPECT_EQ(20, chunk.ReadInt());
}

TEST(CKStateChunkAncillaryTest, EmptyFileAttributeSequenceRegistersHeaderAfterCount) {
    const int words[] = {0x11, 0, 0, 0, 0x3d242466, 0};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 6);
    ASSERT_TRUE(chunk.SeekIdentifier(0x11)); chunk.AttributePatch(TRUE, nullptr, 0);
    EXPECT_EQ(2, chunk.GetCurrentPos()); ExpectAttributeDescriptor(chunk, 3);
    ExpectAncillaryWords(chunk, words, 6);
}

TEST(CKStateChunkAncillaryTest, EmptyStateAttributeSequenceConsumesEmptyChildSequenceHeader) {
    const int words[] = {0x11, 0, 0, 0, 0, 0x3d242466, 0};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 7);
    ASSERT_TRUE(chunk.SeekIdentifier(0x11)); chunk.AttributePatch(FALSE, nullptr, 0);
    EXPECT_EQ(2, chunk.GetCurrentPos()); ExpectAttributeDescriptor(chunk, 4);
    ExpectAncillaryWords(chunk, words, 7);
}

TEST(CKStateChunkAncillaryTest, AttributePatchWithoutConversionStillRegistersManagerRelocation) {
    const int words[] = {0x11, 0, 1, 101, 1, 0x3d242466, 0, 2};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 8);
    ASSERT_TRUE(chunk.SeekIdentifier(0x11)); chunk.AttributePatch(TRUE, nullptr, 3);
    ExpectAttributeDescriptor(chunk, 4); EXPECT_EQ(2, chunk.GetCurrentPos());
    ExpectAncillaryWords(chunk, words, 8);
    int table[] = {20, 30, 40}; EXPECT_EQ(1, chunk.RemapManagerInt(ATTRIBUTE_MANAGER_GUID, table, 3));
    const int expected[] = {0x11, 0, 1, 101, 1, 0x3d242466, 0, 40}; ExpectAncillaryWords(chunk, expected, 8);
}

TEST(CKStateChunkAncillaryTest, ZeroConversionCountPreservesTypesAndRegistersDescriptor) {
    const int words[] = {0x11, 0, 1, 101, 1, 0x3d242466, 0, 0};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 8);
    ASSERT_TRUE(chunk.SeekIdentifier(0x11)); int table[] = {99}; chunk.AttributePatch(TRUE, table, 0);
    ExpectAttributeDescriptor(chunk, 4); ExpectAncillaryWords(chunk, words, 8);
}

TEST(CKStateChunkAncillaryTest, ExistingManagerListMakesAttributePatchAnExactNoOp) {
    int wire[] = {(CHNK_OPTION_MAN << 24) | (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8),
                  4, 1, 0x3d242466, 0, 2, 2, -1, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire)); chunk.StartRead(); chunk.Goto(3);
    IntListStruct *managers = chunk.GetManagers(); int table[] = {10, 11, 12};
    chunk.AttributePatch(FALSE, table, 3);
    EXPECT_EQ(managers, chunk.GetManagers()); EXPECT_EQ(3, chunk.GetCurrentPos());
    int exported[9] = {}; ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(exported));
    EXPECT_EQ(0, std::memcmp(wire, exported, sizeof(wire)));
}

TEST(CKStateChunkAncillaryTest, RepeatedAttributePatchDoesNotConvertTypesTwiceOrMoveCursor) {
    const int words[] = {0x11, 0, 1, 101, 1, 0x3d242466, 0, 0};
    CKStateChunk chunk(CKCID_BEOBJECT, nullptr); LoadAncillaryWords(chunk, words, 8);
    ASSERT_TRUE(chunk.SeekIdentifier(0x11)); int table[] = {1, 2, 3}; chunk.AttributePatch(TRUE, table, 3);
    chunk.Goto(7); IntListStruct *managers = chunk.GetManagers(); chunk.AttributePatch(TRUE, table, 3);
    EXPECT_EQ(7, chunk.GetCurrentPos()); EXPECT_EQ(managers, chunk.GetManagers());
    const int expected[] = {0x11, 0, 1, 101, 1, 0x3d242466, 0, 1}; ExpectAncillaryWords(chunk, expected, 8);
}

TEST(CKStateChunkAncillaryTest, ComputeCRCUsesAdler32WithCallerSeedAndSupportsChaining) {
    const int firstWords[] = {0x04030201}, secondWords[] = {0x08070605}, allWords[] = {0x04030201, 0x08070605};
    CKStateChunk first(CKCID_OBJECT, nullptr), second(CKCID_OBJECT, nullptr), all(CKCID_OBJECT, nullptr);
    LoadAncillaryWords(first, firstWords, 1); LoadAncillaryWords(second, secondWords, 1); LoadAncillaryWords(all, allWords, 2);
    // Bytes 1..8: s1=1+36=37, s2=8+8*1+7*2+...+1*8=128.
    EXPECT_EQ(0x0018000bu, first.ComputeCRC(1)); EXPECT_EQ(0x00800025u, all.ComputeCRC(1));
    EXPECT_EQ(0x00780024u, all.ComputeCRC(0));
    EXPECT_EQ(0x00800025u, second.ComputeCRC(first.ComputeCRC(1)));
}

TEST(CKStateChunkAncillaryTest, ComputeCRCEmptyNullDataReturnsInitialAdler) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(1u, chunk.ComputeCRC(0)); EXPECT_EQ(1u, chunk.ComputeCRC(0x12345678));
    chunk.StartWrite(); chunk.CloseChunk(); EXPECT_EQ(1u, chunk.ComputeCRC(0x12345678));
}

TEST(CKStateChunkAncillaryTest, ComputeCRCUsesPublishedExtentAndDoesNotMoveCursor) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteInt(0x04030201); chunk.UpdateDataSize();
    chunk.WriteInt(0x08070605); EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(0x0018000bu, chunk.ComputeCRC(1)); EXPECT_EQ(2, chunk.GetCurrentPos());
    chunk.UpdateDataSize(); EXPECT_EQ(0x00800025u, chunk.ComputeCRC(1));
    chunk.Goto(1); EXPECT_EQ(0x00800025u, chunk.ComputeCRC(1)); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkAncillaryTest, ComputeCRCSeesBackpatchedBytesWithoutChangingMetadata) {
    const int words[] = {0x04030201, 0x08070605}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadAncillaryWords(chunk, words, 2); chunk.Goto(1);
    int *word = static_cast<int *>(chunk.LockWriteBuffer(1)); *word = 0x08070609;
    // Changing byte 5 to 9 adds 4 to s1 and 4*4 to s2.
    EXPECT_EQ(0x00900029u, chunk.ComputeCRC(1)); EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(8, chunk.GetDataSize());
}

TEST(CKStateChunkAncillaryTest, WriteIdentifierLazilyStartsWriterAndEmitsTerminalHeader) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.WriteIdentifier(0xf1234567u);
    EXPECT_EQ(2, chunk.GetCurrentPos()); EXPECT_EQ(0, chunk.GetDataSize()); chunk.CloseChunk();
    const int expected[] = {static_cast<int>(0xf1234567u), 0}; ExpectAncillaryWords(chunk, expected, 2);
    EXPECT_NE(0, chunk.SeekIdentifier(0xf1234567u)); EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST(CKStateChunkAncillaryTest, WriteIdentifierLinksPayloadAndEmptySectionsByAbsoluteWordPosition) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteIdentifier(0x21); chunk.WriteInt(73); chunk.WriteIdentifier(0x22); chunk.WriteIdentifier(0x23); chunk.WriteInt(79); chunk.CloseChunk();
    const int expected[] = {0x21, 3, 73, 0x22, 5, 0x23, 0, 79}; ExpectAncillaryWords(chunk, expected, 8);
    EXPECT_EQ(1, chunk.SeekIdentifier(0x21)); EXPECT_EQ(73, chunk.ReadInt());
    EXPECT_EQ(1, chunk.SeekIdentifier(0x22)); EXPECT_EQ(5, chunk.GetCurrentPos());
    EXPECT_EQ(1, chunk.SeekIdentifier(0x23)); EXPECT_EQ(79, chunk.ReadInt());
}

TEST(CKStateChunkAncillaryTest, WriteIdentifierPreservesZeroAndDuplicateIdentifiersInLiteralWire) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteIdentifier(0); chunk.WriteIdentifier(0x21); chunk.WriteInt(73); chunk.WriteIdentifier(0x21); chunk.WriteInt(79); chunk.CloseChunk();
    const int expected[] = {0, 2, 0x21, 5, 73, 0x21, 0, 79}; ExpectAncillaryWords(chunk, expected, 8);
}

TEST(CKStateChunkAncillaryTest, WriteIdentifierRetainsEarlierPayloadAcrossCapacityGrowth) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteIdentifier(0x21);
    for (int i = 0; i < 300; ++i) chunk.WriteInt(1000 + i);
    chunk.WriteIdentifier(0x22); chunk.WriteInt(77); chunk.CloseChunk();
    EXPECT_EQ(305 * static_cast<int>(sizeof(int)), chunk.GetDataSize()); chunk.StartRead();
    EXPECT_EQ(0x21, chunk.ReadInt()); EXPECT_EQ(302, chunk.ReadInt());
    for (int i = 0; i < 300; ++i) EXPECT_EQ(1000 + i, chunk.ReadInt());
    EXPECT_EQ(0x22, chunk.ReadInt()); EXPECT_EQ(0, chunk.ReadInt()); EXPECT_EQ(77, chunk.ReadInt());
}

TEST(CKStateChunkAncillaryTest, ListGettersExposeNullForAbsentRelocationsWithoutCreatingParser) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks()); EXPECT_EQ(nullptr, chunk.GetManagers());
    EXPECT_EQ(-1, chunk.GetCurrentPos()); EXPECT_EQ(0, chunk.GetDataSize());
    chunk.StartWrite(); chunk.WriteInt(73); chunk.CloseChunk();
    EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks()); EXPECT_EQ(nullptr, chunk.GetManagers());
}

TEST(CKStateChunkAncillaryTest, ListGettersBorrowDistinctMutableListsVisibleToSerialization) {
    int wire[] = {(7u << 24) | (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), 2, 41, 73, 1, 0, 1, 1, 1, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    IntListStruct *ids = chunk.GetIds(), *children = chunk.GetChunks(), *managers = chunk.GetManagers();
    ASSERT_NE(nullptr, ids); ASSERT_NE(nullptr, children); ASSERT_NE(nullptr, managers);
    EXPECT_NE(ids, children); EXPECT_NE(ids, managers); EXPECT_NE(children, managers);
    ASSERT_EQ(1, ids->Size); ASSERT_EQ(1, children->Size); ASSERT_EQ(1, managers->Size);
    ids->Data[0] = 1; children->Data[0] = 0; managers->Data[0] = 1; chunk.StartRead(); chunk.Goto(1);
    EXPECT_EQ(ids, chunk.GetIds()); EXPECT_EQ(children, chunk.GetChunks()); EXPECT_EQ(managers, chunk.GetManagers());
    EXPECT_EQ(1, chunk.GetCurrentPos()); int output[10] = {}; ASSERT_EQ(sizeof(output), chunk.ConvertToBuffer(output));
    wire[5] = 1; wire[7] = 0; wire[9] = 1; EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}

TEST(CKStateChunkAncillaryTest, ListGettersDistinguishPresentEmptyListsAndClearResetsOwnership) {
    int wire[] = {(7u << 24) | (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), 0, 0, 0, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    ASSERT_NE(nullptr, chunk.GetIds()); ASSERT_NE(nullptr, chunk.GetChunks()); ASSERT_NE(nullptr, chunk.GetManagers());
    EXPECT_EQ(0, chunk.GetIds()->Size); EXPECT_EQ(0, chunk.GetChunks()->Size); EXPECT_EQ(0, chunk.GetManagers()->Size);
    chunk.Clear(); EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks()); EXPECT_EQ(nullptr, chunk.GetManagers());
}

class CKStateChunkAncillaryContextTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0)); }
    static void TearDownTestSuite() { if (context) CKCloseContext(context); context = nullptr; CKShutdown(); }
    static CKContext *context;
};
CKContext *CKStateChunkAncillaryContextTest::context = nullptr;

TEST_F(CKStateChunkAncillaryContextTest, SetDynamicSwitchesSubsequentReferencePolicyWithoutChangingExistingWords) {
    CKObject fixed(context, "AncillaryFixed"), dynamic(context, "AncillaryDynamic"); dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    ASSERT_TRUE(dynamic.IsDynamic()); CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.SetDynamic(FALSE); chunk.WriteObject(&fixed); chunk.WriteObject(&dynamic);
    chunk.SetDynamic(TRUE); chunk.WriteObject(&dynamic); chunk.SetDynamic(FALSE); chunk.WriteObject(&dynamic); chunk.CloseChunk();
    const int expected[] = {static_cast<int>(fixed.GetID()), 0, static_cast<int>(dynamic.GetID()), 0}; ExpectAncillaryWords(chunk, expected, 4);
    ASSERT_NE(nullptr, chunk.GetIds()); ASSERT_EQ(2, chunk.GetIds()->Size);
    EXPECT_EQ(0, chunk.GetIds()->Data[0]); EXPECT_EQ(2, chunk.GetIds()->Data[1]);
}

TEST_F(CKStateChunkAncillaryContextTest, SetDynamicFalseDoesNotOverrideUnrestrictedClassMinusOne) {
    CKObject dynamic(context, "AncillaryUnrestricted"); dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKStateChunk chunk(-1, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite(); chunk.WriteObject(&dynamic); chunk.CloseChunk();
    const int expected[] = {static_cast<int>(dynamic.GetID())}; ExpectAncillaryWords(chunk, expected, 1);
}
