#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

namespace {

// Independent on-disk chunk envelope; never use object writers to manufacture
// the IDs/counts under test. Old chunk versions carry a six-word envelope.
void LoadObjectWords(CKStateChunk &chunk, int version, const int *words, int count, bool file = false) {
    int envelope[64] = {};
    int start;
    if (version < CHUNK_VERSION2) {
        envelope[0] = version << 16;
        envelope[1] = CKCID_OBJECT;
        envelope[2] = count;
        start = 6;
    } else {
        const unsigned int options = file ? CHNK_OPTION_FILE : 0;
        envelope[0] = static_cast<int>(((static_cast<unsigned int>(version) | (options << 8)) << 16) | (CKCID_OBJECT << 8));
        envelope[1] = count;
        start = 2;
    }
    ASSERT_LE(start + count, 64);
    if (count) std::memcpy(envelope + start, words, static_cast<size_t>(count) * sizeof(int));
    ASSERT_TRUE(chunk.ConvertFromBuffer(envelope));
    chunk.StartRead();
}

void ExpectLinkedWords(CKObjectArray *array, const CK_ID *expected, int count) {
    ASSERT_NE(nullptr, array);
    ASSERT_EQ(count, array->GetCount());
    array->Reset();
    for (int i = 0; i < count; ++i) {
        ASSERT_FALSE(array->EndOfList());
        EXPECT_EQ(expected[i], array->GetDataId());
        array->Next();
    }
    EXPECT_TRUE(array->EndOfList());
}

class CKStateChunkObjectReadTest : public ::testing::Test {
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

CKContext *CKStateChunkObjectReadTest::context = nullptr;

TEST_F(CKStateChunkObjectReadTest, ModernSingleIdsPreserveRawBitsAndCursor) {
    const int words[] = {0, 41, -1, static_cast<int>(0x80000000u), 0x13572468};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSION4, words, 5);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(static_cast<CK_ID>(words[i]), chunk.ReadObjectID());
        EXPECT_EQ(i + 1, chunk.GetCurrentPos());
    }
    EXPECT_EQ(words[4], chunk.ReadInt());
    EXPECT_EQ(0u, chunk.ReadObjectID());
    EXPECT_EQ(5, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacySingleIdsConsumeVariableLengthRecords) {
    CKObject first(context, "LegacyFirst"), second(context, "LegacySecond");
    const int words[] = {1, 0x11223344, 0x55667788, static_cast<int>(first.GetID()),
                         0, 7, 0x12345678, 0x23456789, static_cast<int>(second.GetID()), 0x13572468};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 10);
    EXPECT_EQ(&first, chunk.ReadObject(context)); EXPECT_EQ(4, chunk.GetCurrentPos());
    EXPECT_EQ(0u, chunk.ReadObjectID()); EXPECT_EQ(5, chunk.GetCurrentPos());
    EXPECT_EQ(&second, chunk.ReadObject(context)); EXPECT_EQ(9, chunk.GetCurrentPos());
    EXPECT_EQ(words[9], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectReadTest, FileSingleIdsMapCreatedObjectsAndNegativeSentinels) {
    CKObject first(context, "FileFirst"), second(context, "FileSecond");
    CKFile file(context);
    file.m_FileObjects.Resize(3);
    file.m_FileObjects[0].Object = second.GetID(); file.m_FileObjects[0].CreatedObject = first.GetID();
    file.m_FileObjects[1].CreatedObject = 0;
    file.m_FileObjects[2].Object = first.GetID(); file.m_FileObjects[2].CreatedObject = second.GetID();
    const int words[] = {2, -1, 0, 1, 99, 0x13572468};
    CKStateChunk chunk(CKCID_OBJECT, &file);
    LoadObjectWords(chunk, CHUNK_VERSION4, words, 6, true);
    EXPECT_EQ(&second, chunk.ReadObject(context)); EXPECT_EQ(0u, chunk.ReadObjectID());
    EXPECT_EQ(&first, chunk.ReadObject(context)); EXPECT_EQ(0u, chunk.ReadObjectID());
    EXPECT_EQ(0u, chunk.ReadObjectID()); EXPECT_EQ(words[5], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectReadTest, ModernArraysPreserveOrderZerosAndDuplicates) {
    CKObject first(context, "ArrayFirst"), second(context, "ArraySecond");
    const CK_ID expected[] = {first.GetID(), 0, first.GetID(), 0x7FFFFFFEu, second.GetID()};
    const int words[] = {5, static_cast<int>(expected[0]), 0, static_cast<int>(expected[2]), 0x7FFFFFFE,
                         static_cast<int>(expected[4]), 0x13572468};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSION4, words, 7);
    const XObjectArray &ids = chunk.ReadXObjectArray();
    ASSERT_EQ(5, ids.Size()); for (int i = 0; i < 5; ++i) EXPECT_EQ(expected[i], ids[i]);
    EXPECT_EQ(words[6], chunk.ReadInt());
    chunk.StartRead(); CKObjectArray linked; linked.InsertRear(second.GetID());
    chunk.ReadObjectArray(&linked); ExpectLinkedWords(&linked, expected, 5); EXPECT_EQ(words[6], chunk.ReadInt());
    chunk.StartRead(); CKObjectArray *allocated = chunk.ReadObjectArray();
    ExpectLinkedWords(allocated, expected, 5); DeleteCKObjectArray(allocated); EXPECT_EQ(words[6], chunk.ReadInt());
    chunk.StartRead(); const XObjectPointerArray &pointers = chunk.ReadXObjectArray(context);
    ASSERT_EQ(3, pointers.Size()); EXPECT_EQ(&first, pointers[0]); EXPECT_EQ(&first, pointers[1]); EXPECT_EQ(&second, pointers[2]);
    EXPECT_EQ(words[6], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectReadTest, LegacyArraysDecodeEnvelopeCountAndRealObjects) {
    CKObject first(context, "LegacyArrayFirst"), second(context, "LegacyArraySecond");
    const CK_ID expected[] = {second.GetID(), 0, first.GetID(), second.GetID()};
    const int words[] = {1, 11, 22, 33, 44, 4, static_cast<int>(second.GetID()), 0,
                         static_cast<int>(first.GetID()), static_cast<int>(second.GetID()), 0x13572468};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 11);
    const XObjectArray &ids = chunk.ReadXObjectArray();
    ASSERT_EQ(4, ids.Size()); for (int i = 0; i < 4; ++i) EXPECT_EQ(expected[i], ids[i]);
    EXPECT_EQ(10, chunk.GetCurrentPos()); EXPECT_EQ(words[10], chunk.ReadInt());
    chunk.StartRead(); CKObjectArray linked; chunk.ReadObjectArray(&linked); ExpectLinkedWords(&linked, expected, 4);
    EXPECT_EQ(words[10], chunk.ReadInt());
    chunk.StartRead(); CKObjectArray *allocated = chunk.ReadObjectArray();
    ExpectLinkedWords(allocated, expected, 4); DeleteCKObjectArray(allocated); EXPECT_EQ(words[10], chunk.ReadInt());
    chunk.StartRead(); const XObjectPointerArray &pointers = chunk.ReadXObjectArray(context);
    ASSERT_EQ(3, pointers.Size()); EXPECT_EQ(&second, pointers[0]); EXPECT_EQ(&first, pointers[1]); EXPECT_EQ(&second, pointers[2]);
    EXPECT_EQ(words[10], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectReadTest, FileArraysUseCreatedIdsAndFilterOnlyPointerResults) {
    CKObject first(context, "MappedFirst"), second(context, "MappedSecond");
    CKFile file(context); file.m_FileObjects.Resize(3);
    file.m_FileObjects[0].Object = second.GetID(); file.m_FileObjects[0].CreatedObject = first.GetID();
    file.m_FileObjects[1].CreatedObject = 0;
    file.m_FileObjects[2].Object = first.GetID(); file.m_FileObjects[2].CreatedObject = second.GetID();
    const int words[] = {6, 2, -1, 0, 1, 0, 77, 0x13572468};
    const CK_ID expected[] = {second.GetID(), 0, first.GetID(), 0, first.GetID(), 0};
    CKStateChunk chunk(CKCID_OBJECT, &file); LoadObjectWords(chunk, CHUNK_VERSION4, words, 8, true);
    const XObjectArray &ids = chunk.ReadXObjectArray();
    ASSERT_EQ(6, ids.Size()); for (int i = 0; i < 6; ++i) EXPECT_EQ(expected[i], ids[i]);
    EXPECT_EQ(words[7], chunk.ReadInt());
    chunk.StartRead(); CKObjectArray linked; chunk.ReadObjectArray(&linked); ExpectLinkedWords(&linked, expected, 6);
    chunk.StartRead(); CKObjectArray *allocated = chunk.ReadObjectArray(); ExpectLinkedWords(allocated, expected, 6); DeleteCKObjectArray(allocated);
    chunk.StartRead(); const XObjectPointerArray &pointers = chunk.ReadXObjectArray(context);
    ASSERT_EQ(3, pointers.Size()); EXPECT_EQ(&second, pointers[0]); EXPECT_EQ(&first, pointers[1]); EXPECT_EQ(&first, pointers[2]);
    EXPECT_EQ(words[7], chunk.ReadInt());
}

TEST_F(CKStateChunkObjectReadTest, NullOutputSkipsValidModernAndLegacySequences) {
    const int modern[] = {2, 31, 47, 0, 0x13572468};
    const int legacy[] = {1, 11, 22, 33, 44, 2, 31, 47, 0, 0x13572468};
    for (int old = 0; old < 2; ++old) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr);
        LoadObjectWords(chunk, old ? CHUNK_VERSIONBASE : CHUNK_VERSION4, old ? legacy : modern, old ? 10 : 5);
        chunk.ReadObjectArray(static_cast<CKObjectArray *>(nullptr));
        EXPECT_EQ(old ? 8 : 3, chunk.GetCurrentPos());
        chunk.ReadObjectArray(static_cast<CKObjectArray *>(nullptr));
        EXPECT_EQ(0x13572468, chunk.ReadInt());
    }
}

TEST_F(CKStateChunkObjectReadTest, EmptyAndEndOfInputResetDllStaticScratchIndependently) {
    CKObject object(context, "CacheObject");
    const int one[] = {1, static_cast<int>(object.GetID())}; const int empty[] = {0};
    CKStateChunk populated(CKCID_OBJECT, nullptr), blank(CKCID_OBJECT, nullptr);
    LoadObjectWords(populated, CHUNK_VERSION4, one, 2); LoadObjectWords(blank, CHUNK_VERSION4, empty, 1);
    const XObjectPointerArray &pointers = populated.ReadXObjectArray(context); ASSERT_EQ(1, pointers.Size());
    populated.StartRead(); const XObjectArray &ids = populated.ReadXObjectArray(); ASSERT_EQ(1, ids.Size());
    EXPECT_EQ(0, blank.ReadXObjectArray().Size()); EXPECT_EQ(0, ids.Size());
    ASSERT_EQ(1, pointers.Size()); EXPECT_EQ(&object, pointers[0]);
    blank.StartRead(); EXPECT_EQ(0, blank.ReadXObjectArray(context).Size()); EXPECT_EQ(0, pointers.Size());
    EXPECT_EQ(0, blank.ReadXObjectArray().Size());
    EXPECT_EQ(0, blank.ReadXObjectArray(context).Size());
}

TEST_F(CKStateChunkObjectReadTest, EndOfInputPreservesCallerOutputAndEmptyAllocatedResult) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadObjectWords(chunk, CHUNK_VERSION4, nullptr, 0);
    CKObjectArray linked; linked.InsertRear(static_cast<CK_ID>(73));
    chunk.ReadObjectArray(&linked); const CK_ID expected[] = {73}; ExpectLinkedWords(&linked, expected, 1);
    EXPECT_EQ(nullptr, chunk.ReadObjectArray()); EXPECT_EQ(0, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, TruncatedModernIdSequenceDoesNotPublishStaleIds) {
    const int complete[] = {3, 31, 47, 59}; const int shortWords[] = {3, 71};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadObjectWords(chunk, CHUNK_VERSION4, complete, 4);
    ASSERT_EQ(3, chunk.ReadXObjectArray().Size());
    LoadObjectWords(chunk, CHUNK_VERSION4, shortWords, 2);
    EXPECT_EQ(0, chunk.ReadXObjectArray().Size()); EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, TruncatedModernSequencesStopBeforePublishingOrSkipping) {
    const int words[] = {3, 71}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSION4, words, 2); CKObjectArray linked; linked.InsertRear(static_cast<CK_ID>(99));
    chunk.ReadObjectArray(&linked); EXPECT_EQ(0, linked.GetCount()); EXPECT_EQ(2, chunk.GetCurrentPos());
    chunk.StartRead(); EXPECT_EQ(nullptr, chunk.ReadObjectArray()); EXPECT_EQ(2, chunk.GetCurrentPos());
    chunk.StartRead(); EXPECT_EQ(0, chunk.ReadXObjectArray(context).Size()); EXPECT_EQ(2, chunk.GetCurrentPos());
    chunk.StartRead(); chunk.ReadObjectArray(static_cast<CKObjectArray *>(nullptr)); EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacyShortEnvelopeProvidedArrayStopsAtChunkEnd) {
    const int words[] = {1, 11, 22}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 3); CKObjectArray linked; linked.InsertRear(static_cast<CK_ID>(99));
    chunk.ReadObjectArray(&linked); EXPECT_EQ(0, linked.GetCount()); EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacyShortEnvelopeAllocatedArrayStopsAtChunkEnd) {
    const int words[] = {1, 11, 22}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 3);
    CKObjectArray *result = chunk.ReadObjectArray(); EXPECT_EQ(nullptr, result); DeleteCKObjectArray(result);
    EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacyShortEnvelopeScratchArraysStopAtChunkEnd) {
    const int words[] = {1, 11, 22}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 3);
    EXPECT_EQ(0, chunk.ReadXObjectArray().Size()); EXPECT_EQ(3, chunk.GetCurrentPos());
    chunk.StartRead(); EXPECT_EQ(0, chunk.ReadXObjectArray(context).Size()); EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacyTruncatedPayloadDoesNotPublishNeighborMemory) {
    const int words[] = {1, 11, 22, 33, 44, 3, 71}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 7);
    EXPECT_EQ(0, chunk.ReadXObjectArray().Size()); EXPECT_EQ(7, chunk.GetCurrentPos());
    chunk.StartRead(); EXPECT_EQ(0, chunk.ReadXObjectArray(context).Size()); EXPECT_EQ(7, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, LegacyShortSingleObjectRecordReturnsNullWithoutReadingMissingId) {
    const int words[] = {1, 11, 22}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadObjectWords(chunk, CHUNK_VERSIONBASE, words, 3);
    EXPECT_EQ(0u, chunk.ReadObjectID()); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkObjectReadTest, NegativeSequenceCountsProduceEmptyResultsWithoutSkippingBackward) {
    const int modern[] = {-1};
    const int legacy[] = {1, 11, 22, 33, 44, -1};
    for (int old = 0; old < 2; ++old) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr);
        const int wordCount = old ? 6 : 1;
        LoadObjectWords(chunk, old ? CHUNK_VERSIONBASE : CHUNK_VERSION4, old ? legacy : modern, wordCount);
        CKObjectArray linked; linked.InsertRear(static_cast<CK_ID>(99));
        chunk.ReadObjectArray(&linked); EXPECT_EQ(0, linked.GetCount()); EXPECT_EQ(wordCount, chunk.GetCurrentPos());
        chunk.StartRead(); EXPECT_EQ(nullptr, chunk.ReadObjectArray()); EXPECT_EQ(wordCount, chunk.GetCurrentPos());
        chunk.StartRead(); EXPECT_EQ(0, chunk.ReadXObjectArray().Size()); EXPECT_EQ(wordCount, chunk.GetCurrentPos());
        chunk.StartRead(); EXPECT_EQ(0, chunk.ReadXObjectArray(context).Size()); EXPECT_EQ(wordCount, chunk.GetCurrentPos());
        chunk.StartRead(); chunk.ReadObjectArray(static_cast<CKObjectArray *>(nullptr)); EXPECT_EQ(wordCount, chunk.GetCurrentPos());
    }
}

} // namespace
