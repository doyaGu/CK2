#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CKAll.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// StartSave initializes the class-index table required by SaveObject. Keep
// the native save session balanced even when an assertion ends a test early.
class CKTestObjectSequenceSaveFile : public CKFile {
public:
    explicit CKTestObjectSequenceSaveFile(CKContext *context)
        : CKFile(context), Started(false) { Path[0] = 0; }
    ~CKTestObjectSequenceSaveFile() {
        if (Started) FinalizeSaveState();
        if (Path[0]) std::remove(Path);
    }
    bool Begin() {
#ifdef _WIN32
        char directory[MAX_PATH];
        const DWORD length = GetTempPathA(MAX_PATH, directory);
        if (!length || length >= MAX_PATH || !GetTempFileNameA(directory, "cks", 0, Path)) return false;
#else
        std::strcpy(Path, "/tmp/ck2-object-sequence-XXXXXX");
        const int handle = mkstemp(Path);
        if (handle < 0) return false;
        close(handle);
#endif
        Started = StartSave(Path) == CK_OK;
        return Started;
    }
private:
    CKTestObjectSequenceSaveFile(const CKTestObjectSequenceSaveFile &);
    CKTestObjectSequenceSaveFile &operator=(const CKTestObjectSequenceSaveFile &);
    char Path[1024];
    bool Started;
};

static void ExpectObjectWire(CKStateChunk &chunk, const int *words, int count,
                      const int *ids = nullptr, int idCount = 0, bool file = false) {
    ASSERT_LE(count + idCount + 3, 1100);
    chunk.CloseChunk();
    ASSERT_EQ(count * 4, chunk.GetDataSize());
    int wire[1100] = {};
    const int size = (2 + count + (ids ? idCount + 1 : 0)) * 4;
    ASSERT_EQ(size, chunk.ConvertToBuffer(nullptr));
    ASSERT_EQ(size, chunk.ConvertToBuffer(wire));
    const unsigned int options = (file ? 8u : 0u) | (ids ? 1u : 0u);
    EXPECT_EQ(static_cast<int>((7u << 16) | (options << 24) |
                              ((static_cast<unsigned int>(chunk.GetChunkClassID()) & 255u) << 8)), wire[0]);
    EXPECT_EQ(count, wire[1]);
    for (int i = 0; i < count; ++i) EXPECT_EQ(words[i], wire[i + 2]) << "payload " << i;
    if (ids) {
        EXPECT_EQ(idCount, wire[2 + count]);
        for (int i = 0; i < idCount; ++i) EXPECT_EQ(ids[i], wire[3 + count + i]) << "relocation " << i;
    } else EXPECT_EQ(nullptr, chunk.GetIds());
}

static void LoadLiteralWords(CKStateChunk &chunk, const int *words, int count, bool file = false) {
    int wire[40] = {};
    ASSERT_LE(count + 2, 40);
    wire[0] = static_cast<int>((7u << 16) | (file ? 8u << 24 : 0u) | (CKCID_OBJECT << 8));
    wire[1] = count;
    for (int i = 0; i < count; ++i) wire[i + 2] = words[i];
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead();
}

class CKStateChunkObjectSequenceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
        ASSERT_NE(nullptr, context);
    }
    static void TearDownTestSuite() {
        if (context) CKCloseContext(context);
        context = nullptr;
        CKShutdown();
    }
    static CKContext *context;
};

CKContext *CKStateChunkObjectSequenceTest::context = nullptr;

TEST_F(CKStateChunkObjectSequenceTest, SingleIdsRegisterOnlyNonzeroPositionsAndKeepRawBits) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteInt(73); chunk.WriteObjectID(0); chunk.WriteObjectID(41);
    chunk.WriteObjectID(0x80000000u); chunk.WriteObjectID(0xFFFFFFFFu);
    const int words[] = {73, 0, 41, static_cast<int>(0x80000000u), -1};
    const int ids[] = {2, 3, 4};
    EXPECT_EQ(5, chunk.GetCurrentPos()); ExpectObjectWire(chunk, words, 5, ids, 3);
}

TEST_F(CKStateChunkObjectSequenceTest, SequenceHeaderRegistersOneSpanIncludingZeroAndDuplicateIds) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteInt(73);
    chunk.StartObjectIDSequence(4); chunk.WriteObjectIDSequence(41); chunk.WriteObjectIDSequence(0);
    chunk.WriteObjectIDSequence(41); chunk.WriteObjectIDSequence(0xFFFFFFFFu);
    const int words[] = {73, 4, 41, 0, 41, -1}; const int ids[] = {-1, 1};
    EXPECT_EQ(6, chunk.GetCurrentPos()); ExpectObjectWire(chunk, words, 6, ids, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, EmptySequencesWriteCountsWithoutCreatingRelocationLists) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.StartObjectIDSequence(0); chunk.StartObjectIDSequence(0); chunk.WriteInt(73);
    const int words[] = {0, 0, 73}; ExpectObjectWire(chunk, words, 3);
}

TEST_F(CKStateChunkObjectSequenceTest, SequenceItemDoesNotCreateASingleIdRelocation) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteObjectIDSequence(41); chunk.WriteObjectIDSequence(0);
    const int words[] = {41, 0}; ExpectObjectWire(chunk, words, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, InterleavedSinglesAndSequencesHaveLiteralPositionsAndRemapAllMatches) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteInt(73);
    chunk.WriteObjectID(41); chunk.StartObjectIDSequence(3);
    chunk.WriteObjectIDSequence(41); chunk.WriteObjectIDSequence(0); chunk.WriteObjectIDSequence(43);
    chunk.StartObjectIDSequence(0); chunk.WriteObjectID(47);
    chunk.StartObjectIDSequence(1); chunk.WriteObjectIDSequence(41); chunk.WriteInt(79);
    const int words[] = {73, 41, 3, 41, 0, 43, 0, 47, 1, 41, 79};
    const int ids[] = {1, -1, 2, 7, -1, 8}; ExpectObjectWire(chunk, words, 11, ids, 6);
    // Return value counts all six visited relocation entries, not only the
    // three values that match the requested old ID.
    ASSERT_EQ(6, chunk.RemapObject(41, 53)); chunk.StartRead();
    const int remapped[] = {73, 53, 3, 53, 0, 43, 0, 47, 1, 53, 79};
    for (int i = 0; i < 11; ++i) EXPECT_EQ(remapped[i], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectSequenceTest, ObjectSinglesFilterDynamicReferencesAccordingToDestinationPolicy) {
    CKObject fixed(context, "FixedSingle"), dynamic(context, "DynamicSingle");
    dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0); ASSERT_TRUE(dynamic.IsDynamic());
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite();
    chunk.WriteObject(&fixed); chunk.WriteObject(&dynamic); chunk.WriteObject(nullptr);
    chunk.SetDynamic(TRUE); chunk.WriteObject(&dynamic);
    const int words[] = {static_cast<int>(fixed.GetID()), 0, 0, static_cast<int>(dynamic.GetID())};
    const int ids[] = {0, 3}; ExpectObjectWire(chunk, words, 4, ids, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, ObjectSequenceFilteringKeepsDeclaredCountAndSpanMetadata) {
    CKObject fixed(context, "FixedSequence"), dynamic(context, "DynamicSequence");
    dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite();
    chunk.StartObjectIDSequence(4); chunk.WriteObjectSequence(&fixed); chunk.WriteObjectSequence(&dynamic);
    chunk.WriteObjectSequence(nullptr); chunk.SetDynamic(TRUE); chunk.WriteObjectSequence(&dynamic);
    const int words[] = {4, static_cast<int>(fixed.GetID()), 0, 0, static_cast<int>(dynamic.GetID())};
    const int ids[] = {-1, 0}; ExpectObjectWire(chunk, words, 5, ids, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, ClassMinusOneAllowsDynamicSingleAndSequenceReferences) {
    CKObject dynamic(context, "UnrestrictedDynamic"); dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKStateChunk chunk(-1, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite();
    chunk.WriteObject(&dynamic); chunk.WriteObject(nullptr); chunk.StartObjectIDSequence(2);
    chunk.WriteObjectSequence(&dynamic); chunk.WriteObjectSequence(nullptr);
    const int words[] = {static_cast<int>(dynamic.GetID()), 0, 2, static_cast<int>(dynamic.GetID()), 0};
    const int ids[] = {0, -1, 2}; ExpectObjectWire(chunk, words, 5, ids, 3);
}

TEST_F(CKStateChunkObjectSequenceTest, RawObjectArrayPreservesUnresolvedIdsZerosDuplicatesAndIteratorContract) {
    CKObjectArray array; array.InsertRear(static_cast<CK_ID>(41)); array.InsertRear(static_cast<CK_ID>(0));
    array.InsertRear(static_cast<CK_ID>(41)); array.InsertRear(static_cast<CK_ID>(0xFFFFFFFFu));
    ASSERT_TRUE(array.PositionSeek(2));
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite(); chunk.WriteObjectArray(&array);
    const int words[] = {4, 41, 0, 41, -1}; const int ids[] = {-1, 0};
    EXPECT_EQ(4, array.GetCount()); EXPECT_TRUE(array.EndOfList()); ExpectObjectWire(chunk, words, 5, ids, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, ContextObjectArrayFiltersInvalidAndDynamicSlotsWithoutShrinkingCount) {
    CKObject fixed(context, "ArrayFixed"), dynamic(context, "ArrayDynamic");
    dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKObjectArray array; array.InsertRear(&fixed); array.InsertRear(&dynamic);
    array.InsertRear(static_cast<CK_ID>(0)); array.InsertRear(static_cast<CK_ID>(0x7FFFFFFEu)); array.InsertRear(&fixed);
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.SetDynamic(FALSE); chunk.StartWrite();
    chunk.WriteObjectArray(&array, context);
    const int words[] = {5, static_cast<int>(fixed.GetID()), 0, 0, 0, static_cast<int>(fixed.GetID())};
    const int ids[] = {-1, 0}; ExpectObjectWire(chunk, words, 6, ids, 2); EXPECT_TRUE(array.EndOfList());
}

TEST_F(CKStateChunkObjectSequenceTest, ContextArrayAllowsDynamicDestinationAndClassMinusOne) {
    CKObject dynamic(context, "AllowedArrayDynamic"); dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKObjectArray array; array.InsertRear(&dynamic); array.InsertRear(static_cast<CK_ID>(0));
    const int words[] = {2, static_cast<int>(dynamic.GetID()), 0}; const int ids[] = {-1, 0};
    CKStateChunk allowed(CKCID_OBJECT, nullptr); allowed.StartWrite(); allowed.WriteObjectArray(&array, context);
    ExpectObjectWire(allowed, words, 3, ids, 2);
    CKStateChunk unrestricted(-1, nullptr); unrestricted.SetDynamic(FALSE); unrestricted.StartWrite();
    unrestricted.WriteObjectArray(&array, context); ExpectObjectWire(unrestricted, words, 3, ids, 2);
}

TEST_F(CKStateChunkObjectSequenceTest, NullAndEmptyArraysWriteZeroCountsAndDoNotRegisterSpans) {
    CKObjectArray empty;
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteObjectArray(nullptr); chunk.WriteObjectArray(nullptr, context);
    chunk.WriteObjectArray(&empty); chunk.WriteObjectArray(&empty, context); chunk.WriteInt(73);
    const int words[] = {0, 0, 0, 0, 73}; ExpectObjectWire(chunk, words, 5);
}

TEST_F(CKStateChunkObjectSequenceTest, FileSingleAndSequenceIdsUseSaveObjectIndicesAndMissingSentinels) {
    CKObject first(context, "SavedFirst"), second(context, "SavedSecond"), absent(context, "NotSaved");
    CKTestObjectSequenceSaveFile file(context); ASSERT_TRUE(file.Begin());
    file.SaveObject(&second); file.SaveObject(&first);
    ASSERT_EQ(2, file.m_FileObjects.Size());
    EXPECT_EQ(second.GetID(), file.m_FileObjects[0].Object); EXPECT_EQ(first.GetID(), file.m_FileObjects[1].Object);
    CKStateChunk chunk(CKCID_OBJECT, &file); chunk.StartWrite();
    chunk.WriteObjectID(first.GetID()); chunk.WriteObjectID(0); chunk.WriteObject(&absent);
    chunk.StartObjectIDSequence(4); chunk.WriteObjectIDSequence(second.GetID()); chunk.WriteObjectIDSequence(0);
    chunk.WriteObjectSequence(&first); chunk.WriteObjectSequence(nullptr);
    const int words[] = {1, -1, -1, 4, 0, -1, 1, -1}; ExpectObjectWire(chunk, words, 8, nullptr, 0, true);
}

TEST_F(CKStateChunkObjectSequenceTest, FileRawObjectArrayMapsEverySlotWithoutAddingRelocations) {
    CKObject first(context, "RawSavedFirst"), second(context, "RawSavedSecond"), absent(context, "RawAbsent");
    CKTestObjectSequenceSaveFile file(context); ASSERT_TRUE(file.Begin());
    file.SaveObject(&second); file.SaveObject(&first);
    CKObjectArray array; array.InsertRear(&first); array.InsertRear(static_cast<CK_ID>(0));
    array.InsertRear(&absent); array.InsertRear(&second); array.InsertRear(&first);
    CKStateChunk chunk(CKCID_OBJECT, &file); chunk.StartWrite(); chunk.WriteObjectArray(&array);
    const int words[] = {5, 1, -1, -1, 0, 1}; ExpectObjectWire(chunk, words, 6, nullptr, 0, true);
}

TEST_F(CKStateChunkObjectSequenceTest, FileContextObjectArrayFiltersBeforeMappingIncludingSyntheticZeroKey) {
    CKObject fixed(context, "FileFixed"), dynamic(context, "FileDynamic"), absent(context, "FileAbsent");
    dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    CKTestObjectSequenceSaveFile file(context); ASSERT_TRUE(file.Begin()); file.SaveObject(&fixed);
    // Explicit zero-key mapping distinguishes filtering then lookup from an
    // unconditional -1 write, and records the exact native lookup contract.
    file.m_ObjectsHashTable.Insert(0, 7); file.m_ObjectsHashTable.Insert(dynamic.GetID(), 9);
    CKObjectArray array; array.InsertRear(&fixed); array.InsertRear(&dynamic); array.InsertRear(&absent);
    array.InsertRear(static_cast<CK_ID>(0)); array.InsertRear(static_cast<CK_ID>(0x7FFFFFFEu));
    CKStateChunk chunk(CKCID_OBJECT, &file); chunk.SetDynamic(FALSE); chunk.StartWrite();
    chunk.WriteObjectArray(&array, context);
    const int words[] = {5, 0, 7, -1, 7, 7}; ExpectObjectWire(chunk, words, 6, nullptr, 0, true);
}

TEST_F(CKStateChunkObjectSequenceTest, LiteralFileSequenceReadsCreatedIdsRatherThanSavedObjectIds) {
    CKObject saved(context, "SavedIdentity"), created(context, "CreatedIdentity");
    CKFile file(context); file.m_FileObjects.Resize(1);
    file.m_FileObjects[0].Object = saved.GetID(); file.m_FileObjects[0].CreatedObject = created.GetID();
    const int words[] = {4, 0, -1, 0, -1, 73}; CKStateChunk chunk(CKCID_OBJECT, &file);
    LoadLiteralWords(chunk, words, 6, true); ASSERT_EQ(4, chunk.StartReadSequence());
    EXPECT_EQ(created.GetID(), chunk.ReadObjectID()); EXPECT_EQ(0u, chunk.ReadObjectID());
    EXPECT_EQ(&created, chunk.ReadObject(context)); EXPECT_EQ(nullptr, chunk.ReadObject(context));
    EXPECT_EQ(73, chunk.ReadInt()); EXPECT_EQ(6, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectSequenceTest, LiteralSequenceNullOutputSkipsWholeSpanAndLeavesFollowingRecords) {
    const int words[] = {3, 41, 0, 43, 0, 73}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadLiteralWords(chunk, words, 6); chunk.ReadObjectArray(static_cast<CKObjectArray *>(nullptr));
    EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(0, chunk.StartReadSequence()); EXPECT_EQ(73, chunk.ReadInt());
    EXPECT_EQ(0, chunk.StartReadSequence()); EXPECT_EQ(6, chunk.GetCurrentPos());
}

static void CheckArrayCapacityBoundary(int count, CKContext *context, bool fileMode, bool contextMode) {
    CKObject first(context, "BoundaryFirst"), saved(context, "BoundaryObject");
    CKTestObjectSequenceSaveFile file(context); ASSERT_TRUE(file.Begin());
    file.SaveObject(&first); file.SaveObject(&saved);
    CKObjectArray array; for (int i = 0; i < count; ++i) array.InsertRear(saved.GetID());
    CKStateChunk chunk(CKCID_OBJECT, fileMode ? &file : nullptr); chunk.StartWrite();
    chunk.WriteObjectArray(&array, contextMode ? context : nullptr); EXPECT_EQ(count + 1, chunk.GetCurrentPos());
    // Append forces a genuine reservation after the count+payload boundary.
    // Missing space for the header loses the final ID when capacity is copied.
    chunk.WriteInt(73); const int id = fileMode ? 1 : static_cast<int>(saved.GetID());
    int expected[1002]; expected[0] = count;
    for (int i = 0; i < count; ++i) expected[i + 1] = id;
    expected[count + 1] = 73; const int ids[] = {-1, 0};
    ExpectObjectWire(chunk, expected, count + 2, fileMode ? nullptr : ids, fileMode ? 0 : 2, fileMode);
}

TEST_F(CKStateChunkObjectSequenceTest, RawArrayCount500ReservesTheCountHeaderAndLastId) { CheckArrayCapacityBoundary(500, context, false, false); }
TEST_F(CKStateChunkObjectSequenceTest, ContextArrayCount1000ReservesTheCountHeaderAndLastId) { CheckArrayCapacityBoundary(1000, context, false, true); }
TEST_F(CKStateChunkObjectSequenceTest, FileRawArrayCount500ReservesTheCountHeaderAndLastIndex) { CheckArrayCapacityBoundary(500, context, true, false); }
TEST_F(CKStateChunkObjectSequenceTest, FileContextArrayCount1000ReservesTheCountHeaderAndLastIndex) { CheckArrayCapacityBoundary(1000, context, true, true); }
