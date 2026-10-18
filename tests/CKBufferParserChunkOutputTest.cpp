#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CKAll.h"

// Link CK2Static so the ledger observes the real chunk payload/list/parser owners.
struct CKTestAllocation {
    std::uintptr_t Address;
    int Frees;
    bool Array;
    bool ArrayFree;
};

static CKTestAllocation Ledger[128];
static int AllocationCount = 0;
static bool Tracking = false;
static bool LedgerOverflow = false;

static void Allocated(void *pointer, bool array) {
    if (!Tracking)
        return;
    if (AllocationCount == 128) {
        LedgerOverflow = true;
        return;
    }
    CKTestAllocation &entry = Ledger[AllocationCount++];
    entry.Address = reinterpret_cast<std::uintptr_t>(pointer);
    entry.Frees = 0;
    entry.Array = array;
    entry.ArrayFree = false;
}

static void Released(void *pointer, bool array) {
    if (!Tracking || !pointer)
        return;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    for (int i = AllocationCount - 1; i >= 0; --i) {
        if (Ledger[i].Address == address && Ledger[i].Frees == 0) {
            ++Ledger[i].Frees;
            Ledger[i].ArrayFree = array;
            return;
        }
    }
}

static int ModernHeader(int cid, int dataVersion, unsigned int options = 0) {
    return static_cast<int>((options << 24) | (CHUNK_VERSION4 << 16) |
                            (static_cast<unsigned int>(cid) << 8) | dataVersion);
}

static void SetEmbeddedParser(CKFileChunk &output) {
    output.parser.CurrentPos = 7;
    output.parser.DataSize = 19;
    output.parser.PrevIdentifierPos = 3;
}

static void ExpectEmbeddedParser(const CKFileChunk &output) {
    EXPECT_EQ(7, output.parser.CurrentPos);
    EXPECT_EQ(19, output.parser.DataSize);
    EXPECT_EQ(3, output.parser.PrevIdentifierPos);
}

static void ExpectOneEntry(const IntListStruct *list, int expected) {
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(1, list->Size);
    ASSERT_NE(nullptr, list->Data);
    EXPECT_EQ(expected, list->Data[0]);
}

class CKBufferParserChunkOutputObjectTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;

    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    }

    void TearDown() override {
        if (Context)
            CKCloseContext(Context);
        CKShutdown();
    }
};

void *operator new(std::size_t bytes) {
    void *pointer = std::malloc(bytes ? bytes : 1);
    if (!pointer)
        throw std::bad_alloc();
    Allocated(pointer, false);
    return pointer;
}

void *operator new[](std::size_t bytes) {
    void *pointer = std::malloc(bytes ? bytes : 1);
    if (!pointer)
        throw std::bad_alloc();
    Allocated(pointer, true);
    return pointer;
}

void operator delete(void *pointer) noexcept { Released(pointer, false); std::free(pointer); }
void operator delete[](void *pointer) noexcept { Released(pointer, true); std::free(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { Released(pointer, false); std::free(pointer); }
void operator delete[](void *pointer, std::size_t) noexcept { Released(pointer, true); std::free(pointer); }

TEST(CKBufferParserChunkOutputTest, ModernFrameLoadsOwnedPayloadAndConsumesPadding) {
    int words[] = {0x71717171, ModernHeader(CKCID_OBJECT, 9), 2, 41, 43,
                   0x61616161, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    parser.Seek(4);
    CKFileChunk output = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    SetEmbeddedParser(output);
    parser.ExtractChunk(20, nullptr, &output);
    EXPECT_EQ(24, parser.CursorPos());
    EXPECT_EQ(CKCID_OBJECT, output.chunk.GetChunkClassID());
    EXPECT_EQ(9, output.chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, output.chunk.GetChunkVersion());
    EXPECT_EQ(8, output.chunk.GetDataSize());
    words[3] = words[4] = 0;
    output.chunk.StartRead();
    EXPECT_EQ(41, output.chunk.ReadInt());
    EXPECT_EQ(43, output.chunk.ReadInt());
    EXPECT_EQ(0x72727272, parser.ReadInt());
    EXPECT_EQ(0x71717171, words[0]);
    ExpectEmbeddedParser(output);
}

TEST(CKBufferParserChunkOutputTest, LegacyVersionTwoCopiesAllRelocationLists) {
    int words[] = {(CHUNK_VERSION2 << 16) | 9, 0x123, 2, 0, 1, 1, 1,
                   47, 53, 11, 13, 17, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, nullptr), ChunkParser()};
    parser.ExtractChunk(48, nullptr, &output);
    EXPECT_EQ(48, parser.CursorPos());
    EXPECT_EQ(0x123, output.chunk.GetChunkClassID());
    EXPECT_EQ(9, output.chunk.GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION2, output.chunk.GetChunkVersion());
    words[7] = words[8] = words[9] = words[10] = words[11] = 0;
    output.chunk.StartRead();
    EXPECT_EQ(47, output.chunk.ReadInt());
    EXPECT_EQ(53, output.chunk.ReadInt());
    ExpectOneEntry(output.chunk.GetIds(), 11);
    ExpectOneEntry(output.chunk.GetChunks(), 13);
    ExpectOneEntry(output.chunk.GetManagers(), 17);
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserChunkOutputTest, LegacyVersionOnePreservesWideClassAndIndependentPayload) {
    int words[] = {(CHUNK_VERSION1 << 16) | 3, 0x234, 1, 0, 1, 1,
                   59, 19, 23, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, nullptr), ChunkParser()};
    parser.ExtractChunk(36, nullptr, &output);
    EXPECT_EQ(36, parser.CursorPos());
    EXPECT_EQ(0x234, output.chunk.GetChunkClassID());
    EXPECT_EQ(CHUNK_VERSION1, output.chunk.GetChunkVersion());
    words[6] = words[7] = words[8] = 0;
    output.chunk.StartRead();
    EXPECT_EQ(59, output.chunk.ReadInt());
    ExpectOneEntry(output.chunk.GetIds(), 19);
    ExpectOneEntry(output.chunk.GetChunks(), 23);
    EXPECT_EQ(nullptr, output.chunk.GetManagers());
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserChunkOutputTest, ModernEmptyListsReplacePreviousOwnedLists) {
    int previous[] = {ModernHeader(CKCID_OBJECT, 9, 7), 1, 61, 1, 0, 1, 0, 1, 0};
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, nullptr), ChunkParser()};
    ASSERT_TRUE(output.chunk.ConvertFromBuffer(previous));
    output.chunk.StartRead();
    int words[] = {ModernHeader(CKCID_PARAMETER, 5, 7), 0, 0, 0, 0, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    parser.ExtractChunk(20, nullptr, &output);
    EXPECT_EQ(20, parser.CursorPos());
    EXPECT_EQ(CKCID_PARAMETER, output.chunk.GetChunkClassID());
    EXPECT_EQ(0, output.chunk.GetDataSize());
    ASSERT_NE(nullptr, output.chunk.GetIds());
    ASSERT_NE(nullptr, output.chunk.GetChunks());
    ASSERT_NE(nullptr, output.chunk.GetManagers());
    EXPECT_EQ(0, output.chunk.GetIds()->Size);
    EXPECT_EQ(0, output.chunk.GetChunks()->Size);
    EXPECT_EQ(0, output.chunk.GetManagers()->Size);
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserChunkOutputTest, InvalidVersionReplacesOldStateWithFreshEmptyChunk) {
    int previous[] = {ModernHeader(CKCID_PARAMETER, 9, 7), 1, 67, 1, 0, 1, 0, 1, 0};
    CKFileChunk output = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    ASSERT_TRUE(output.chunk.ConvertFromBuffer(previous));
    output.chunk.StartRead();
    SetEmbeddedParser(output);
    int words[] = {(99 << 16) | 11, 0, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    parser.ExtractChunk(8, nullptr, &output);
    EXPECT_EQ(8, parser.CursorPos());
    EXPECT_EQ(0, output.chunk.GetChunkClassID());
    EXPECT_EQ(0, output.chunk.GetDataVersion());
    EXPECT_EQ(0, output.chunk.GetChunkVersion());
    EXPECT_EQ(0, output.chunk.GetDataSize());
    EXPECT_EQ(nullptr, output.chunk.GetIds());
    EXPECT_EQ(nullptr, output.chunk.GetChunks());
    EXPECT_EQ(nullptr, output.chunk.GetManagers());
    output.chunk.StartRead();
    EXPECT_EQ(0, output.chunk.ReadInt());
    EXPECT_EQ(0x72727272, parser.ReadInt());
    ExpectEmbeddedParser(output);
}

TEST(CKBufferParserChunkOutputTest, SequentialSuccessFailureSuccessReusesOneDestination) {
    int words[] = {ModernHeader(CKCID_OBJECT, 3), 1, 71,
                   (99 << 16) | 11, 0,
                   ModernHeader(CKCID_PARAMETER, 5), 2, 73, 79,
                   0x72727272};
    CKBufferParser parser(words, sizeof(words));
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, nullptr), ChunkParser()};
    parser.ExtractChunk(12, nullptr, &output);
    output.chunk.StartRead();
    EXPECT_EQ(71, output.chunk.ReadInt());
    parser.ExtractChunk(8, nullptr, &output);
    EXPECT_EQ(20, parser.CursorPos());
    EXPECT_EQ(0, output.chunk.GetDataSize());
    EXPECT_EQ(0, output.chunk.GetChunkVersion());
    parser.ExtractChunk(16, nullptr, &output);
    EXPECT_EQ(36, parser.CursorPos());
    EXPECT_EQ(CKCID_PARAMETER, output.chunk.GetChunkClassID());
    output.chunk.StartRead();
    EXPECT_EQ(73, output.chunk.ReadInt());
    EXPECT_EQ(79, output.chunk.ReadInt());
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserChunkOutputTest, SeparateDestinationsSurviveInputAndParserDestruction) {
    CKFileChunk first = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    CKFileChunk second = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    {
        int words[] = {ModernHeader(CKCID_OBJECT, 3), 1, 83,
                       ModernHeader(CKCID_PARAMETER, 5), 1, 89};
        CKBufferParser parser(words, sizeof(words));
        parser.ExtractChunk(12, nullptr, &first);
        parser.ExtractChunk(12, nullptr, &second);
        EXPECT_EQ(24, parser.CursorPos());
        std::memset(words, 0, sizeof(words));
    }
    first.chunk.StartRead();
    second.chunk.StartRead();
    EXPECT_EQ(83, first.chunk.ReadInt());
    EXPECT_EQ(89, second.chunk.ReadInt());
    first.chunk.Clear();
    second.chunk.StartRead();
    EXPECT_EQ(89, second.chunk.ReadInt());
}

TEST(CKBufferParserChunkOutputTest, ExtractionCanReadTheDestinationsPreviousOwnedBuffer) {
    const int words[] = {ModernHeader(CKCID_OBJECT, 3), 2, 97, 101};
    CKFileChunk output = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    output.chunk.StartWrite();
    for (int i = 0; i < 4; ++i)
        output.chunk.WriteInt(words[i]);
    output.chunk.CloseChunk();
    output.chunk.StartRead();
    CKBufferParser parser(output.chunk.LockReadBuffer(), sizeof(words));
    parser.ExtractChunk(sizeof(words), nullptr, &output);
    EXPECT_EQ(16, parser.CursorPos());
    EXPECT_EQ(CKCID_OBJECT, output.chunk.GetChunkClassID());
    EXPECT_EQ(3, output.chunk.GetDataVersion());
    output.chunk.StartRead();
    EXPECT_EQ(97, output.chunk.ReadInt());
    EXPECT_EQ(101, output.chunk.ReadInt());
}

TEST_F(CKBufferParserChunkOutputObjectTest, FileFlagMapsRealObjectsUsingTheSuppliedFile) {
    CKObject first(Context, "OutputFirst"), second(Context, "OutputSecond");
    CKFile oldFile(Context), newFile(Context);
    oldFile.m_FileObjects.Resize(1);
    newFile.m_FileObjects.Resize(1);
    oldFile.m_FileObjects[0].CreatedObject = first.GetID();
    newFile.m_FileObjects[0].CreatedObject = second.GetID();
    int words[] = {ModernHeader(CKCID_OBJECT, 3, CHNK_OPTION_FILE), 1, 0};
    CKBufferParser parser(words, sizeof(words));
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, &oldFile), ChunkParser()};
    parser.ExtractChunk(sizeof(words), &newFile, &output);
    output.chunk.StartRead();
    EXPECT_EQ(&second, output.chunk.ReadObject(Context));
    parser.Seek(0);
    parser.ExtractChunk(sizeof(words), &oldFile, &output);
    output.chunk.StartRead();
    EXPECT_EQ(&first, output.chunk.ReadObject(Context));
}

TEST_F(CKBufferParserChunkOutputObjectTest, NonFileAndLegacyChunksDiscardPreviousFileMapping) {
    CKObject first(Context, "OutputRaw"), second(Context, "OutputMapped");
    CKFile file(Context);
    file.m_FileObjects.Resize(1);
    file.m_FileObjects[0].CreatedObject = second.GetID();
    int modern[] = {ModernHeader(CKCID_OBJECT, 3), 1, static_cast<int>(first.GetID())};
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, &file), ChunkParser()};
    CKBufferParser modernParser(modern, sizeof(modern));
    modernParser.ExtractChunk(sizeof(modern), &file, &output);
    output.chunk.StartRead();
    EXPECT_EQ(&first, output.chunk.ReadObject(Context));
    int legacy[] = {(CHUNK_VERSION2 << 16) | 5, CKCID_OBJECT, 1, 0, 0, 0, 0,
                    static_cast<int>(first.GetID())};
    CKBufferParser legacyParser(legacy, sizeof(legacy));
    legacyParser.ExtractChunk(sizeof(legacy), &file, &output);
    EXPECT_EQ(CHUNK_VERSION2, output.chunk.GetChunkVersion());
    output.chunk.StartRead();
    EXPECT_EQ(&first, output.chunk.ReadObject(Context));
}

TEST_F(CKBufferParserChunkOutputObjectTest, ExtractionPreservesDestinationDynamicObjectPolicy) {
    CKObject fixed(Context, "OutputFixed"), dynamic(Context, "OutputDynamic");
    dynamic.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
    ASSERT_TRUE(dynamic.IsDynamic());
    CKFileChunk output = {CKStateChunk(CKCID_OBJECT, nullptr), ChunkParser()};
    output.chunk.SetDynamic(FALSE);
    int words[] = {ModernHeader(CKCID_OBJECT, 3), 1, 103};
    CKBufferParser parser(words, sizeof(words));
    parser.ExtractChunk(sizeof(words), nullptr, &output);
    output.chunk.StartWrite();
    output.chunk.WriteObject(&fixed);
    output.chunk.WriteObject(&dynamic);
    output.chunk.CloseChunk();
    output.chunk.StartRead();
    EXPECT_EQ(&fixed, output.chunk.ReadObject(Context));
    EXPECT_EQ(nullptr, output.chunk.ReadObject(Context));
}

TEST(CKBufferParserChunkOutputTest, AllTemporaryAndDestinationAllocationsAreReleasedExactlyOnce) {
    int words[] = {ModernHeader(CKCID_OBJECT, 9, 7), 1, 107, 1, 0, 1, 0, 1, 0};
    CKBufferParser parser(words, sizeof(words));
    AllocationCount = 0;
    LedgerOverflow = false;
    Tracking = true;
    int value = 0;
    int classId = 0;
    {
        CKFileChunk output = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
        output.chunk.StartWrite();
        output.chunk.WriteInt(109);
        output.chunk.CloseChunk();
        parser.ExtractChunk(sizeof(words), nullptr, &output);
        classId = output.chunk.GetChunkClassID();
        output.chunk.StartRead();
        value = output.chunk.ReadInt();
    }
    Tracking = false;
    EXPECT_EQ(CKCID_OBJECT, classId);
    EXPECT_EQ(107, value);
    EXPECT_EQ(sizeof(words), parser.CursorPos());
    EXPECT_FALSE(LedgerOverflow);
    EXPECT_GE(AllocationCount, 17) << "Observe temporary and deep destination owners, not the old no-op";
    for (int i = 0; i < AllocationCount; ++i) {
        EXPECT_EQ(1, Ledger[i].Frees) << "allocation " << i;
        EXPECT_EQ(Ledger[i].Array, Ledger[i].ArrayFree) << "allocation kind " << i;
    }
}
