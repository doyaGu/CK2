#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

class CKTestCloneOwner {
public:
    explicit CKTestCloneOwner(CKStateChunk *chunk) : chunk_(chunk) {}
    ~CKTestCloneOwner() { DeleteCKStateChunk(chunk_); }
    CKStateChunk *Get() const { return chunk_; }
    CKStateChunk *operator->() const { return chunk_; }
private:
    CKTestCloneOwner(const CKTestCloneOwner &);
    CKTestCloneOwner &operator=(const CKTestCloneOwner &);
    CKStateChunk *chunk_;
};

static int Header(int version, int dataVersion, int classId, int options) {
    return static_cast<int>((static_cast<unsigned int>(version) << 16) |
                            (static_cast<unsigned int>(options) << 24) |
                            (classId << 8) | dataVersion);
}

static void LoadLiteral(CKStateChunk &chunk) {
    int wire[] = {Header(CHUNK_VERSION4, 0x5A, CKCID_PARAMETER, 7), 6,
                  0x1122, 3, 41, 0x3344, 0, 43,
                  2, 2, 5, 1, 4, 1, 2};
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

static void ExpectLiteral(CKStateChunk &chunk) {
    EXPECT_EQ(CKCID_PARAMETER, chunk.GetChunkClassID());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion());
    EXPECT_EQ(0x5A, chunk.GetDataVersion());
    EXPECT_EQ(24, chunk.GetDataSize());
    chunk.StartRead();
    const int expected[] = {0x1122, 3, 41, 0x3344, 0, 43};
    for (int i = 0; i < 6; ++i) EXPECT_EQ(expected[i], chunk.ReadInt());
    ASSERT_NE(nullptr, chunk.GetIds());
    ASSERT_NE(nullptr, chunk.GetChunks());
    ASSERT_NE(nullptr, chunk.GetManagers());
    EXPECT_EQ(2, chunk.GetIds()->Size);
    EXPECT_EQ(2, chunk.GetIds()->Data[0]); EXPECT_EQ(5, chunk.GetIds()->Data[1]);
    EXPECT_EQ(1, chunk.GetChunks()->Size); EXPECT_EQ(4, chunk.GetChunks()->Data[0]);
    EXPECT_EQ(1, chunk.GetManagers()->Size); EXPECT_EQ(2, chunk.GetManagers()->Data[0]);
}

TEST(CKStateChunkCloneTest, PointerConstructorNullInitializesAnEmptyOwner) {
    CKStateChunk chunk(static_cast<CKStateChunk *>(nullptr));
    EXPECT_EQ(0, chunk.GetChunkClassID()); EXPECT_EQ(0, chunk.GetDataVersion());
    EXPECT_EQ(0, chunk.GetChunkVersion()); EXPECT_EQ(0, chunk.GetDataSize());
    EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks());
    EXPECT_EQ(nullptr, chunk.GetManagers());
    chunk.StartWrite(); chunk.WriteInt(47); chunk.WriteObjectID(53); chunk.CloseChunk(); chunk.StartRead();
    EXPECT_EQ(47, chunk.ReadInt()); EXPECT_EQ(53u, chunk.ReadObjectID());
    ASSERT_NE(nullptr, chunk.GetIds()); ASSERT_EQ(1, chunk.GetIds()->Size);
    EXPECT_EQ(1, chunk.GetIds()->Data[0]);
}

TEST(CKStateChunkCloneTest, FactoryNullCreatesAnEmptyWritableOwner) {
    CKTestCloneOwner chunk(CreateCKStateChunk(static_cast<CKStateChunk *>(nullptr)));
    ASSERT_NE(nullptr, chunk.Get());
    EXPECT_EQ(0, chunk->GetChunkClassID()); EXPECT_EQ(0, chunk->GetDataSize());
    chunk->StartWrite(); chunk->WriteInt(41); chunk->CloseChunk(); chunk->StartRead();
    EXPECT_EQ(41, chunk->ReadInt());
}

TEST(CKStateChunkCloneTest, FactoryClassCreatesIndependentOwnersAndNullDeleteIsValid) {
    CKTestCloneOwner first(CreateCKStateChunk(CKCID_OBJECT, nullptr));
    CKTestCloneOwner second(CreateCKStateChunk(CKCID_PARAMETER, nullptr));
    ASSERT_NE(nullptr, first.Get()); ASSERT_NE(nullptr, second.Get());
    EXPECT_NE(first.Get(), second.Get()); EXPECT_EQ(CKCID_OBJECT, first->GetChunkClassID());
    EXPECT_EQ(CKCID_PARAMETER, second->GetChunkClassID());
    first->StartWrite(); first->WriteInt(41); first->CloseChunk();
    second->StartWrite(); second->WriteInt(43); second->CloseChunk();
    first->StartRead(); second->StartRead();
    EXPECT_EQ(41, first->ReadInt()); EXPECT_EQ(43, second->ReadInt());
    DeleteCKStateChunk(nullptr);
}

TEST(CKStateChunkCloneTest, PointerConstructorCopiesLiteralMetadataAndPayload) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadLiteral(source);
    CKStateChunk copy(&source);
    ExpectLiteral(copy); ExpectLiteral(source);
}

TEST(CKStateChunkCloneTest, FactoryCopySurvivesSourceDeletion) {
    CKStateChunk *source = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    ASSERT_NE(nullptr, source); LoadLiteral(*source);
    CKTestCloneOwner copy(CreateCKStateChunk(source)); ASSERT_NE(nullptr, copy.Get());
    EXPECT_NE(source->GetIds(), copy->GetIds());
    DeleteCKStateChunk(source);
    ExpectLiteral(*copy.Get());
}

TEST(CKStateChunkCloneTest, FactoryCopyDeletionLeavesSourceReadable) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadLiteral(source);
    CKStateChunk *copy = CreateCKStateChunk(&source); ASSERT_NE(nullptr, copy);
    EXPECT_NE(source.GetManagers(), copy->GetManagers());
    DeleteCKStateChunk(copy);
    ExpectLiteral(source);
}

TEST(CKStateChunkCloneTest, CloneNullPreservesAllDestinationStateAndReadCursor) {
    CKStateChunk destination(CKCID_OBJECT, nullptr); LoadLiteral(destination);
    destination.StartRead(); EXPECT_EQ(0x1122, destination.ReadInt());
    IntListStruct *ids = destination.GetIds();
    destination.Clone(nullptr);
    EXPECT_EQ(1, destination.GetCurrentPos()); EXPECT_EQ(ids, destination.GetIds());
    EXPECT_EQ(3, destination.ReadInt()); ExpectLiteral(destination);
}

TEST(CKStateChunkCloneTest, CloneSelfPreservesOwnedPayloadListsAndReadCursor) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadLiteral(chunk);
    chunk.StartRead(); EXPECT_EQ(0x1122, chunk.ReadInt());
    IntListStruct *ids = chunk.GetIds(), *children = chunk.GetChunks(), *managers = chunk.GetManagers();
    chunk.Clone(&chunk);
    EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(ids, chunk.GetIds());
    EXPECT_EQ(children, chunk.GetChunks()); EXPECT_EQ(managers, chunk.GetManagers());
    EXPECT_EQ(3, chunk.ReadInt()); ExpectLiteral(chunk);
}

TEST(CKStateChunkCloneTest, CloneSelfPreservesAClosedEmptyChunkMetadata) {
    CKStateChunk chunk(CKCID_PARAMETER, nullptr);
    chunk.StartWrite(); chunk.SetDataVersion(19); chunk.CloseChunk(); chunk.Clone(&chunk);
    EXPECT_EQ(CKCID_PARAMETER, chunk.GetChunkClassID()); EXPECT_EQ(19, chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion()); EXPECT_EQ(0, chunk.GetDataSize());
}

TEST(CKStateChunkCloneTest, CloneReplacesAnExistingDestinationOwner) {
    CKStateChunk source(CKCID_OBJECT, nullptr), destination(CKCID_SCENE, nullptr);
    LoadLiteral(source);
    destination.StartWrite(); destination.WriteIdentifier(0x7788); destination.WriteObjectID(97);
    destination.WriteManagerInt(CKGUID(1, 2), 3); destination.CloseChunk(); destination.StartRead();
    destination.Clone(&source); ExpectLiteral(destination); ExpectLiteral(source);
    EXPECT_NE(source.GetIds(), destination.GetIds());
    EXPECT_NE(source.GetChunks(), destination.GetChunks());
    EXPECT_NE(source.GetManagers(), destination.GetManagers());
}

TEST(CKStateChunkCloneTest, CloneEmptySourceClearsOldDataAndLists) {
    CKStateChunk source(CKCID_SCENE, nullptr), destination(CKCID_OBJECT, nullptr);
    source.StartWrite(); source.SetDataVersion(29); source.CloseChunk(); LoadLiteral(destination);
    destination.Clone(&source);
    EXPECT_EQ(CKCID_SCENE, destination.GetChunkClassID()); EXPECT_EQ(29, destination.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, destination.GetChunkVersion()); EXPECT_EQ(0, destination.GetDataSize());
    EXPECT_EQ(nullptr, destination.GetIds()); EXPECT_EQ(nullptr, destination.GetChunks());
    EXPECT_EQ(nullptr, destination.GetManagers());
    destination.StartRead(); EXPECT_EQ(0, destination.ReadInt());
}

TEST(CKStateChunkCloneTest, ClonePreservesPresentEmptyLists) {
    int wire[] = {Header(CHUNK_VERSION4, 9, CKCID_OBJECT, 7), 0, 0, 0, 0};
    CKStateChunk source(CKCID_PARAMETER, nullptr), copy(CKCID_SCENE, nullptr);
    ASSERT_TRUE(source.ConvertFromBuffer(wire)); copy.Clone(&source);
    ASSERT_NE(nullptr, copy.GetIds()); ASSERT_NE(nullptr, copy.GetChunks()); ASSERT_NE(nullptr, copy.GetManagers());
    EXPECT_EQ(0, copy.GetIds()->Size); EXPECT_EQ(0, copy.GetChunks()->Size); EXPECT_EQ(0, copy.GetManagers()->Size);
    EXPECT_NE(source.GetIds(), copy.GetIds()); EXPECT_NE(source.GetChunks(), copy.GetChunks());
    EXPECT_NE(source.GetManagers(), copy.GetManagers());
    int output[5] = {}; ASSERT_EQ(sizeof(wire), copy.ConvertToBuffer(output));
    EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}

TEST(CKStateChunkCloneTest, CloneListStorageAndPayloadCanBeMutatedIndependently) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_SCENE, nullptr);
    LoadLiteral(source); copy.Clone(&source);
    source.GetIds()->Data[0] = 0; source.GetChunks()->Data[0] = 1; source.GetManagers()->Data[0] = 3;
    source.StartRead(); source.Goto(2); source.WriteInt(99); source.CloseChunk();
    ExpectLiteral(copy);
    copy.GetIds()->AddEntry(1); copy.GetChunks()->AddEntry(2); copy.GetManagers()->AddEntry(3);
    EXPECT_EQ(2, source.GetIds()->Size); EXPECT_EQ(1, source.GetChunks()->Size); EXPECT_EQ(1, source.GetManagers()->Size);
    source.StartRead(); source.Goto(2); EXPECT_EQ(99, source.ReadInt());
}

TEST(CKStateChunkCloneTest, CloneReadParserPositionIsIndependent) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadLiteral(source);
    source.StartRead(); source.Skip(2); CKStateChunk copy(&source);
    EXPECT_EQ(2, copy.GetCurrentPos()); EXPECT_EQ(41, copy.ReadInt());
    EXPECT_EQ(2, source.GetCurrentPos()); EXPECT_EQ(41, source.ReadInt());
    copy.StartRead(); EXPECT_EQ(0x1122, copy.ReadInt());
    EXPECT_EQ(3, source.GetCurrentPos());
}

TEST(CKStateChunkCloneTest, CloneReadParserPreservesPreviousIdentifierLookupState) {
    int wire[] = {Header(CHUNK_VERSION4, 1, CKCID_OBJECT, 0), 9,
                  0x1111, 3, 41, 0x2222, 6, 43, 0x1111, 0, 47};
    CKStateChunk source(CKCID_OBJECT, nullptr); ASSERT_TRUE(source.ConvertFromBuffer(wire));
    source.StartRead(); source.Goto(3); ASSERT_EQ(0x2222u, source.ReadIdentifier());
    CKStateChunk copy(&source); EXPECT_EQ(5, copy.GetCurrentPos());
    ASSERT_TRUE(copy.SeekIdentifier(0x1111)); EXPECT_EQ(47, copy.ReadInt());
    EXPECT_EQ(5, source.GetCurrentPos()); EXPECT_EQ(43, source.ReadInt());
    ASSERT_TRUE(source.SeekIdentifier(0x1111)); EXPECT_EQ(47, source.ReadInt());
}

TEST(CKStateChunkCloneTest, ClonePublishedOpenWriterCanContinueBothOwners) {
    CKStateChunk source(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.WriteIdentifier(0x1111); source.WriteInt(41); source.UpdateDataSize();
    CKStateChunk copy(&source);
    EXPECT_EQ(3, copy.GetCurrentPos()); EXPECT_EQ(12, copy.GetDataSize());
    copy.WriteIdentifier(0x2222); copy.WriteInt(43); copy.CloseChunk();
    source.WriteIdentifier(0x3333); source.WriteInt(47); source.CloseChunk();
    copy.StartRead(); ASSERT_TRUE(copy.SeekIdentifier(0x1111)); EXPECT_EQ(41, copy.ReadInt());
    ASSERT_TRUE(copy.SeekIdentifier(0x2222)); EXPECT_EQ(43, copy.ReadInt()); EXPECT_FALSE(copy.SeekIdentifier(0x3333));
    source.StartRead(); ASSERT_TRUE(source.SeekIdentifier(0x1111)); EXPECT_EQ(41, source.ReadInt());
    ASSERT_TRUE(source.SeekIdentifier(0x3333)); EXPECT_EQ(47, source.ReadInt()); EXPECT_FALSE(source.SeekIdentifier(0x2222));
}

TEST(CKStateChunkCloneTest, EmptyOpenParserIsIndependentAndBothOwnersCanStartWriting) {
    CKStateChunk source(CKCID_PARAMETER, nullptr); source.StartWrite();
    CKStateChunk copy(&source); EXPECT_EQ(0, copy.GetCurrentPos());
    copy.WriteInt(41); copy.CloseChunk(); source.WriteInt(43); source.CloseChunk();
    copy.StartRead(); source.StartRead(); EXPECT_EQ(41, copy.ReadInt()); EXPECT_EQ(43, source.ReadInt());
}

class CKStateChunkCloneContextTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
        ASSERT_NE(nullptr, context);
    }
    static void TearDownTestSuite() { if (context) CKCloseContext(context); context = nullptr; CKShutdown(); }
    static CKContext *context;
};

CKContext *CKStateChunkCloneContextTest::context = nullptr;

TEST_F(CKStateChunkCloneContextTest, CloneTransfersBorrowedFileAssociationWithoutOwningTheFile) {
    CKObject first(context, "CloneMappedFirst"), second(context, "CloneMappedSecond");
    CKFile file(context); file.m_FileObjects.Resize(2);
    file.m_FileObjects[0].CreatedObject = first.GetID(); file.m_FileObjects[1].CreatedObject = second.GetID();
    int wire[] = {Header(CHUNK_VERSION4, 9, CKCID_OBJECT, CHNK_OPTION_FILE), 3, 1, -1, 0};
    CKStateChunk source(CKCID_PARAMETER, &file), copy(CKCID_SCENE, nullptr);
    ASSERT_TRUE(source.ConvertFromBuffer(wire)); source.StartRead(); copy.Clone(&source);
    EXPECT_EQ(second.GetID(), copy.ReadObjectID()); EXPECT_EQ(0u, copy.ReadObjectID());
    EXPECT_EQ(first.GetID(), copy.ReadObjectID()); source.Clear();
    copy.StartRead(); EXPECT_EQ(second.GetID(), copy.ReadObjectID());
    copy.Clear(); ASSERT_TRUE(source.ConvertFromBuffer(wire)); source.StartRead(); source.Goto(2);
    EXPECT_EQ(first.GetID(), source.ReadObjectID());
}

TEST_F(CKStateChunkCloneContextTest, ClonePreservesDestinationDynamicFilteringPolicy) {
    CKObject *dynamic = context->CreateObject(CKCID_OBJECT, "CloneDynamic", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, dynamic); ASSERT_TRUE(dynamic->IsDynamic());
    CKStateChunk source(CKCID_OBJECT, nullptr), destination(CKCID_OBJECT, nullptr);
    source.StartWrite(); source.CloseChunk(); source.SetDynamic(TRUE); destination.SetDynamic(FALSE);
    destination.Clone(&source); destination.StartWrite(); destination.WriteObject(dynamic); destination.CloseChunk();
    destination.StartRead(); EXPECT_EQ(0u, destination.ReadObjectID());
    source.SetDynamic(FALSE); destination.SetDynamic(TRUE); destination.Clone(&source);
    destination.StartWrite(); destination.WriteObject(dynamic); destination.CloseChunk(); destination.StartRead();
    EXPECT_EQ(dynamic->GetID(), destination.ReadObjectID());
    context->DestroyObject(dynamic);
}

TEST_F(CKStateChunkCloneContextTest, PointerAndFactoryCopiesUseFreshDynamicPolicy) {
    CKObject *dynamic = context->CreateObject(CKCID_OBJECT, "CloneFactoryDynamic", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, dynamic); ASSERT_TRUE(dynamic->IsDynamic());
    CKStateChunk source(CKCID_OBJECT, nullptr); source.StartWrite(); source.CloseChunk(); source.SetDynamic(FALSE);
    CKStateChunk pointerCopy(&source); CKTestCloneOwner factoryCopy(CreateCKStateChunk(&source));
    ASSERT_NE(nullptr, factoryCopy.Get());
    pointerCopy.StartWrite(); pointerCopy.WriteObject(dynamic); pointerCopy.CloseChunk(); pointerCopy.StartRead();
    factoryCopy->StartWrite(); factoryCopy->WriteObject(dynamic); factoryCopy->CloseChunk(); factoryCopy->StartRead();
    EXPECT_EQ(dynamic->GetID(), pointerCopy.ReadObjectID()); EXPECT_EQ(dynamic->GetID(), factoryCopy->ReadObjectID());
    context->DestroyObject(dynamic);
}
