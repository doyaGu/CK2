#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

static const CKDWORD LifecycleIdentifier = 0x10203040u;
static const CKGUID LifecycleManager(0x12345678u, 0x23456789u);

static void WriteRichState(CKStateChunk &chunk) {
    CKStateChunk child(CKCID_PARAMETER, nullptr);
    child.StartWrite();
    child.WriteInt(73);
    child.CloseChunk();
    chunk.StartWrite();
    chunk.SetDataVersion(17);
    chunk.WriteIdentifier(LifecycleIdentifier);
    chunk.WriteObjectID(41);
    chunk.WriteSubChunk(&child);
    chunk.WriteManagerInt(LifecycleManager, 7);
    chunk.CloseChunk();
}

static void ExpectSingleWordWire(CKStateChunk &chunk, int value, unsigned int options = 0) {
    ASSERT_EQ(4, chunk.GetDataSize());
    ASSERT_EQ(12, chunk.ConvertToBuffer(nullptr));
    int wire[3] = {};
    ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(wire));
    const unsigned int header = (CHUNK_VERSION4 << 16) | (options << 24) |
        (static_cast<unsigned int>(chunk.GetChunkClassID()) << 8) |
        static_cast<unsigned short>(chunk.GetDataVersion());
    EXPECT_EQ(header, static_cast<unsigned int>(wire[0]));
    EXPECT_EQ(1, wire[1]);
    EXPECT_EQ(value, wire[2]);
}

TEST(CKStateChunkLifecycleTest, ClassConstructorInitializesEmptyStateAndVersions) {
    CKStateChunk chunk(CKCID_MATERIAL, nullptr);
    EXPECT_EQ(CKCID_MATERIAL, chunk.GetChunkClassID());
    EXPECT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(0, chunk.GetDataVersion());
    EXPECT_EQ(0, chunk.GetChunkVersion());
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(nullptr, chunk.GetIds());
    EXPECT_EQ(nullptr, chunk.GetChunks());
    EXPECT_EQ(nullptr, chunk.GetManagers());
}

TEST(CKStateChunkLifecycleTest, VersionsRemainSignedSixteenBitValues) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.SetDataVersion(static_cast<short>(0x8123));
    EXPECT_EQ(static_cast<short>(0x8123), chunk.GetDataVersion());
    chunk.StartWrite();
    EXPECT_EQ(static_cast<short>(0x8123), chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion());
    chunk.SetDataVersion(0x1234);
    EXPECT_EQ(0x1234, chunk.GetDataVersion());
}

TEST(CKStateChunkLifecycleTest, FirstWriteDefersPublishedSizeUntilUpdateOrClose) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    chunk.WriteInt(11);
    chunk.WriteInt(22);
    EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.GetDataSize());
    chunk.UpdateDataSize();
    EXPECT_EQ(8, chunk.GetDataSize());
    chunk.WriteInt(33);
    EXPECT_EQ(8, chunk.GetDataSize());
    chunk.CloseChunk();
    EXPECT_EQ(12, chunk.GetDataSize());
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    chunk.StartRead();
    EXPECT_EQ(11, chunk.ReadInt());
    EXPECT_EQ(22, chunk.ReadInt());
    EXPECT_EQ(33, chunk.ReadInt());
}

TEST(CKStateChunkLifecycleTest, UpdateSizeAndCloseAreInertWithoutParser) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.UpdateDataSize();
    chunk.CheckSize(4096);
    chunk.CloseChunk();
    EXPECT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    chunk.StartWrite();
    chunk.WriteInt(53);
    chunk.CloseChunk();
    chunk.UpdateDataSize();
    chunk.CheckSize(4096);
    chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 53);
}

TEST(CKStateChunkLifecycleTest, EmptyCloseReleasesParserAndProducesEmptyModernWire) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    chunk.CheckSize(4096);
    chunk.CloseChunk();
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.GetDataSize());
    ASSERT_EQ(8, chunk.ConvertToBuffer(nullptr));
    int wire[2] = {};
    ASSERT_EQ(8, chunk.ConvertToBuffer(wire));
    EXPECT_EQ((CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), wire[0]);
    EXPECT_EQ(0, wire[1]);
    chunk.StartRead();
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.ReadInt());
}

TEST(CKStateChunkLifecycleTest, CheckSizeGrowthPreservesWrittenPrefixAcrossAllocations) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    for (int i = 0; i < 1101; ++i) chunk.WriteInt(i * 17 + 5);
    EXPECT_EQ(1101, chunk.GetCurrentPos());
    chunk.CheckSize(8192);
    EXPECT_EQ(1101, chunk.GetCurrentPos());
    chunk.WriteInt(0x24681357);
    chunk.CloseChunk();
    ASSERT_EQ(1102 * 4, chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < 1101; ++i) EXPECT_EQ(i * 17 + 5, chunk.ReadInt()) << i;
    EXPECT_EQ(0x24681357, chunk.ReadInt());
}

TEST(CKStateChunkLifecycleTest, CloseCompactsAllRelocationListsWithoutChangingOffsets) {
    CKStateChunk child(CKCID_OBJECT, nullptr);
    child.StartWrite(); child.WriteInt(42); child.CloseChunk();
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    chunk.WriteObjectID(41);
    chunk.WriteSubChunk(&child);
    chunk.WriteManagerInt(LifecycleManager, 7);
    ASSERT_NE(nullptr, chunk.GetIds());
    ASSERT_NE(nullptr, chunk.GetChunks());
    ASSERT_NE(nullptr, chunk.GetManagers());
    EXPECT_GT(chunk.GetIds()->AllocatedSize, chunk.GetIds()->Size);
    EXPECT_GT(chunk.GetChunks()->AllocatedSize, chunk.GetChunks()->Size);
    EXPECT_GT(chunk.GetManagers()->AllocatedSize, chunk.GetManagers()->Size);
    chunk.CloseChunk();
    EXPECT_EQ(1, chunk.GetIds()->Size);
    EXPECT_EQ(0, chunk.GetIds()->Data[0]);
    EXPECT_EQ(1, chunk.GetChunks()->Size);
    EXPECT_EQ(1, chunk.GetChunks()->Data[0]);
    EXPECT_EQ(1, chunk.GetManagers()->Size);
    EXPECT_EQ(10, chunk.GetManagers()->Data[0]);
    EXPECT_EQ(1, chunk.GetIds()->AllocatedSize);
    EXPECT_EQ(1, chunk.GetChunks()->AllocatedSize);
    EXPECT_EQ(1, chunk.GetManagers()->AllocatedSize);
    chunk.StartRead();
    EXPECT_EQ(41u, chunk.ReadObjectID());
    CKStateChunk *loaded = chunk.ReadSubChunk();
    ASSERT_NE(nullptr, loaded);
    loaded->StartRead(); EXPECT_EQ(42, loaded->ReadInt());
    DeleteCKStateChunk(loaded);
    CKGUID guid;
    EXPECT_EQ(7, chunk.ReadManagerInt(&guid));
    EXPECT_EQ(LifecycleManager, guid);
}

TEST(CKStateChunkLifecycleTest, SizeTracksHighWaterWhenOverwritingAValidPrefix) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    chunk.WriteInt(11); chunk.WriteInt(22); chunk.WriteInt(33);
    chunk.UpdateDataSize();
    chunk.Goto(0);
    chunk.WriteInt(44);
    chunk.UpdateDataSize();
    EXPECT_EQ(12, chunk.GetDataSize());
    chunk.CloseChunk();
    chunk.StartRead();
    EXPECT_EQ(44, chunk.ReadInt()); EXPECT_EQ(22, chunk.ReadInt()); EXPECT_EQ(33, chunk.ReadInt());
}

TEST(CKStateChunkLifecycleTest, ClearRemovesPayloadParserClassAndRelocationLists) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    WriteRichState(chunk);
    chunk.StartRead();
    chunk.Clear();
    EXPECT_EQ(0, chunk.GetChunkClassID());
    EXPECT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(nullptr, chunk.GetIds());
    EXPECT_EQ(nullptr, chunk.GetChunks());
    EXPECT_EQ(nullptr, chunk.GetManagers());
    EXPECT_EQ(17, chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion());
    chunk.Clear();
    chunk.StartWrite(); chunk.WriteInt(19); chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 19);
}

TEST(CKStateChunkLifecycleTest, RestartWriteResetsPublishedSizeBeforeNewPayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    WriteRichState(chunk);
    ASSERT_GT(chunk.GetDataSize(), 4);
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(CKCID_OBJECT, chunk.GetChunkClassID());
    EXPECT_EQ(17, chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion());
    chunk.WriteInt(97); chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 97);
}

TEST(CKStateChunkLifecycleTest, RestartWriteDiscardsAllPreviousRelocations) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    WriteRichState(chunk);
    chunk.StartWrite();
    ASSERT_EQ(nullptr, chunk.GetIds());
    ASSERT_EQ(nullptr, chunk.GetChunks());
    ASSERT_EQ(nullptr, chunk.GetManagers());
    chunk.WriteInt(41); chunk.CloseChunk();
    EXPECT_EQ(0, chunk.RemapObject(41, 99));
    ExpectSingleWordWire(chunk, 41);
}

TEST(CKStateChunkLifecycleTest, RestartAfterReadingResetsCursorIdentifierAndOldPayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    WriteRichState(chunk);
    chunk.StartRead(); ASSERT_TRUE(chunk.SeekIdentifier(LifecycleIdentifier));
    ASSERT_EQ(41u, chunk.ReadObjectID());
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    ASSERT_EQ(nullptr, chunk.GetIds());
    ASSERT_EQ(nullptr, chunk.GetChunks());
    ASSERT_EQ(nullptr, chunk.GetManagers());
    chunk.WriteIdentifier(0x55667788u); chunk.WriteInt(32); chunk.CloseChunk();
    EXPECT_FALSE(chunk.SeekIdentifier(LifecycleIdentifier));
    ASSERT_TRUE(chunk.SeekIdentifier(0x55667788u)); EXPECT_EQ(32, chunk.ReadInt());
}

TEST(CKStateChunkLifecycleTest, RestartLargeWriteWithShortWriteDoesNotKeepOldExtent) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite();
    for (int i = 0; i < 1200; ++i) chunk.WriteInt(i);
    chunk.CloseChunk();
    chunk.StartWrite();
    // Stop before CloseChunk on the old implementation: its stale 1200-word
    // extent exceeds the fresh 500-word allocation and is not a legal payload.
    ASSERT_EQ(0, chunk.GetDataSize());
    chunk.WriteInt(79); chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 79);
}

TEST(CKStateChunkLifecycleTest, RestartActiveWriterDiscardingPartialStateIsIndependent) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite(); chunk.WriteObjectID(41); chunk.WriteManagerInt(LifecycleManager, 7);
    chunk.UpdateDataSize();
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    ASSERT_EQ(nullptr, chunk.GetIds()); ASSERT_EQ(nullptr, chunk.GetManagers());
    chunk.WriteObjectID(52); chunk.CloseChunk();
    ASSERT_NE(nullptr, chunk.GetIds()); ASSERT_EQ(1, chunk.GetIds()->Size);
    EXPECT_EQ(0, chunk.GetIds()->Data[0]);
    EXPECT_EQ(1, chunk.RemapObject(52, 61));
    chunk.StartRead(); EXPECT_EQ(61u, chunk.ReadObjectID());
}

TEST(CKStateChunkLifecycleTest, RestartThenEmptyCloseHasNoOldDataOrListPresence) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    WriteRichState(chunk);
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    ASSERT_EQ(nullptr, chunk.GetIds()); ASSERT_EQ(nullptr, chunk.GetChunks()); ASSERT_EQ(nullptr, chunk.GetManagers());
    chunk.CloseChunk();
    ASSERT_EQ(8, chunk.ConvertToBuffer(nullptr));
    int wire[2] = {};
    ASSERT_EQ(8, chunk.ConvertToBuffer(wire));
    EXPECT_EQ((CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8) | 17, wire[0]);
    EXPECT_EQ(0, wire[1]);
}

TEST(CKStateChunkLifecycleTest, ParserClearResetsAllThreeIndependentCursorFields) {
    ChunkParser parser;
    EXPECT_EQ(0, parser.CurrentPos); EXPECT_EQ(0, parser.PrevIdentifierPos); EXPECT_EQ(0, parser.DataSize);
    parser.CurrentPos = 43; parser.PrevIdentifierPos = 11; parser.DataSize = 500;
    parser.Clear();
    EXPECT_EQ(0, parser.CurrentPos); EXPECT_EQ(0, parser.PrevIdentifierPos); EXPECT_EQ(0, parser.DataSize);
}

TEST(CKStateChunkLifecycleTest, CompactPreservesEmptyListAllocationContract) {
    IntListStruct list;
    list.Compact();
    EXPECT_EQ(0, list.Size); EXPECT_EQ(0, list.AllocatedSize); EXPECT_EQ(nullptr, list.Data);
    list.AddEntry(7); list.AddEntry(17); list.Size = 0;
    int *allocation = list.Data;
    const int capacity = list.AllocatedSize;
    list.Compact();
    EXPECT_EQ(allocation, list.Data); EXPECT_EQ(capacity, list.AllocatedSize); EXPECT_EQ(0, list.Size);
    list.AddEntry(27); list.Compact();
    EXPECT_EQ(1, list.Size); EXPECT_EQ(1, list.AllocatedSize); EXPECT_EQ(27, list.Data[0]);
}

class CKStateChunkFileLifecycleTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    static void TearDownTestSuite() {
        if (context) CKCloseContext(context);
        context = nullptr;
        CKShutdown();
    }
    static CKContext *context;
};

CKContext *CKStateChunkFileLifecycleTest::context = nullptr;

TEST_F(CKStateChunkFileLifecycleTest, RestartPreservesFileAssociationAndWritesFreshFileIndices) {
    CKFile file(context);
    file.m_FileObjects.Resize(2);
    file.m_FileObjects[0].Object = 41; file.m_FileObjects[0].CreatedObject = 141;
    file.m_FileObjects[1].Object = 52; file.m_FileObjects[1].CreatedObject = 152;
    file.m_ObjectsHashTable.Insert(41, 0); file.m_ObjectsHashTable.Insert(52, 1);
    CKStateChunk chunk(CKCID_OBJECT, &file);
    chunk.StartWrite(); chunk.SetDataVersion(9); chunk.WriteObjectID(41); chunk.WriteInt(42); chunk.CloseChunk();
    chunk.StartRead(); EXPECT_EQ(141u, chunk.ReadObjectID());
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    chunk.WriteObjectID(52); chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 1, CHNK_OPTION_FILE);
    EXPECT_EQ(nullptr, chunk.GetIds());
    chunk.StartRead(); EXPECT_EQ(152u, chunk.ReadObjectID());
}

TEST_F(CKStateChunkFileLifecycleTest, ClearRetainsFileAndVersionsButRemovesClassAndState) {
    CKFile file(context);
    CKStateChunk chunk(CKCID_OBJECT, &file);
    chunk.StartWrite(); chunk.SetDataVersion(19); chunk.WriteInt(53); chunk.CloseChunk();
    chunk.Clear();
    EXPECT_EQ(0, chunk.GetChunkClassID()); EXPECT_EQ(19, chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion()); EXPECT_EQ(0, chunk.GetDataSize());
    chunk.StartWrite(); chunk.WriteInt(73); chunk.CloseChunk();
    ExpectSingleWordWire(chunk, 73, CHNK_OPTION_FILE);
}

TEST_F(CKStateChunkFileLifecycleTest, RestartAndClearPreserveDynamicObjectFilteringPolicy) {
    CKObject object(context, "LifecycleDynamicProbe");
    object.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    ASSERT_TRUE(object.IsDynamic());
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.SetDynamic(FALSE);
    chunk.StartWrite(); chunk.WriteObject(&object); chunk.CloseChunk();
    chunk.StartRead(); EXPECT_EQ(0u, chunk.ReadObjectID());
    chunk.StartWrite();
    ASSERT_EQ(0, chunk.GetDataSize());
    chunk.WriteObject(&object); chunk.CloseChunk();
    chunk.StartRead(); EXPECT_EQ(0u, chunk.ReadObjectID());
    chunk.Clear();
    chunk.StartWrite(); chunk.WriteObject(&object); chunk.CloseChunk();
    chunk.StartRead(); EXPECT_EQ(0u, chunk.ReadObjectID());
    chunk.SetDynamic(TRUE);
    chunk.StartWrite(); chunk.WriteObject(&object); chunk.CloseChunk();
    chunk.StartRead(); EXPECT_EQ(object.GetID(), chunk.ReadObjectID());
}
