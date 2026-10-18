#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

// Literal wire records establish the expected format independently of writers.
static void LoadSubWords(CKStateChunk &chunk, int version, const int *words, int count,
                  unsigned int options = 0, const int *lists = nullptr, int listWords = 0) {
    int envelope[128] = {};
    int start = version < CHUNK_VERSION2 ? 6 : (version == CHUNK_VERSION2 ? 7 : 2);
    if (version <= CHUNK_VERSION2) {
        envelope[0] = version << 16;
        envelope[1] = CKCID_OBJECT;
        envelope[2] = count;
    } else {
        envelope[0] = static_cast<int>((static_cast<unsigned int>(version) << 16) |
                                      (options << 24) | (CKCID_OBJECT << 8));
        envelope[1] = count;
    }
    ASSERT_LE(start + count + listWords, 128);
    if (count) std::memcpy(envelope + start, words, count * sizeof(int));
    if (listWords) std::memcpy(envelope + start + count, lists, listWords * sizeof(int));
    ASSERT_TRUE(chunk.ConvertFromBuffer(envelope));
    chunk.StartRead();
}

class CKTestChildOwner {
public:
    explicit CKTestChildOwner(CKStateChunk *chunk) : chunk_(chunk) {}
    ~CKTestChildOwner() { if (chunk_) DeleteCKStateChunk(chunk_); }
    CKStateChunk *Get() const { return chunk_; }
    CKStateChunk *operator->() const { return chunk_; }
private:
    CKTestChildOwner(const CKTestChildOwner &);
    CKTestChildOwner &operator=(const CKTestChildOwner &);
    CKStateChunk *chunk_;
};

static void ExpectDataWords(CKStateChunk &chunk, const int *expected, int count) {
    ASSERT_EQ(count * sizeof(int), chunk.GetDataSize());
    chunk.StartRead();
    const int *data = static_cast<const int *>(chunk.LockReadBuffer());
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], data[i]) << "word " << i;
}

TEST(CKStateChunkSubChunkTest, NullWriterUsesOneWordAndNoChildMetadata) {
    CKStateChunk parent(CKCID_OBJECT, nullptr);
    parent.StartWrite(); parent.WriteSubChunk(nullptr); parent.WriteInt(0x1357); parent.CloseChunk();
    const int expected[] = {0, 0x1357}; ExpectDataWords(parent, expected, 2);
    int envelope[16] = {}; ASSERT_EQ(16, parent.ConvertToBuffer(envelope));
    EXPECT_EQ(0u, static_cast<unsigned int>(envelope[0]) >> 24);
    EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(1, parent.GetCurrentPos());
    EXPECT_EQ(0x1357, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, EmptyChildKeepsItsClassAndVersions) {
    CKStateChunk child(CKCID_PARAMETER, nullptr), parent(CKCID_OBJECT, nullptr);
    child.StartWrite(); child.SetDataVersion(0x1234); child.CloseChunk();
    parent.StartWrite(); parent.WriteSubChunk(&child); parent.CloseChunk();
    const int expected[] = {7, CKCID_PARAMETER, (CHUNK_VERSION4 << 16) | 0x1234, 0, 0, 0, 0, 0};
    ExpectDataWords(parent, expected, 8);
    CKTestChildOwner loaded(parent.ReadSubChunk()); ASSERT_NE(nullptr, loaded.Get());
    EXPECT_EQ(CKCID_PARAMETER, loaded->GetChunkClassID()); EXPECT_EQ(0x1234, loaded->GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, loaded->GetChunkVersion()); EXPECT_EQ(0, loaded->GetDataSize());
}

TEST(CKStateChunkSubChunkTest, SignedDataVersionDoesNotOverwriteChunkVersion) {
    CKStateChunk child(CKCID_PARAMETER, nullptr), parent(CKCID_OBJECT, nullptr);
    child.StartWrite(); child.SetDataVersion(static_cast<short>(0x8001)); child.CloseChunk();
    parent.StartWrite(); parent.WriteSubChunk(&child); parent.CloseChunk(); parent.StartRead();
    const int *wire = static_cast<const int *>(parent.LockReadBuffer());
    EXPECT_EQ((CHUNK_VERSION4 << 16) | 0x8001, wire[2]);
    CKTestChildOwner loaded(parent.ReadSubChunk()); ASSERT_NE(nullptr, loaded.Get());
    EXPECT_EQ(static_cast<short>(0x8001), loaded->GetDataVersion());
    EXPECT_EQ(CHUNK_VERSION4, loaded->GetChunkVersion());
}

TEST(CKStateChunkSubChunkTest, SequenceSignedDataVersionUsesIndependentHalfwords) {
    CKStateChunk child(CKCID_PARAMETER, nullptr), parent(CKCID_OBJECT, nullptr);
    child.StartWrite(); child.SetDataVersion(static_cast<short>(0xff01)); child.CloseChunk();
    parent.StartWrite(); parent.StartSubChunkSequence(1); parent.WriteSubChunkSequence(&child);
    parent.CloseChunk(); parent.StartRead();
    const int *wire = static_cast<const int *>(parent.LockReadBuffer());
    EXPECT_EQ((CHUNK_VERSION4 << 16) | 0xff01, wire[3]);
    ASSERT_EQ(1, parent.StartReadSequence()); CKTestChildOwner loaded(parent.ReadSubChunk());
    ASSERT_NE(nullptr, loaded.Get()); EXPECT_EQ(CHUNK_VERSION4, loaded->GetChunkVersion());
}

TEST(CKStateChunkSubChunkTest, WriterPreservesAllThreeChildListsAndSingleEntryPosition) {
    const int words[] = {11, 22, 33};
    const int lists[] = {1, 1, 2, -1, 0, 2, 1, 2};
    CKStateChunk child(CKCID_OBJECT, nullptr), parent(CKCID_OBJECT, nullptr);
    LoadSubWords(child, CHUNK_VERSION4, words, 3, CHNK_OPTION_IDS | CHNK_OPTION_CHN | CHNK_OPTION_MAN, lists, 8);
    parent.StartWrite(); parent.WriteInt(0x2468); parent.WriteSubChunk(&child); parent.CloseChunk();
    const int expected[] = {0x2468, 15, CKCID_OBJECT, CHUNK_VERSION4 << 16, 3, 0, 1, 2, 2,
                            11, 22, 33, 1, -1, 0, 1, 2};
    ExpectDataWords(parent, expected, 17);
    int envelope[64] = {}; ASSERT_EQ(21 * sizeof(int), parent.ConvertToBuffer(envelope));
    EXPECT_EQ(CHNK_OPTION_CHN, static_cast<unsigned int>(envelope[0]) >> 24);
    EXPECT_EQ(1, envelope[19]); EXPECT_EQ(1, envelope[20]);
}

TEST(CKStateChunkSubChunkTest, LiteralModernChildPreservesPayloadAndMetadataIndependently) {
    const int words[] = {15, CKCID_PARAMETER, (CHUNK_VERSION4 << 16) | 23, 3, 0, 1, 2, 2,
                         11, 22, 33, 1, -1, 0, 1, 2, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 17);
    CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get());
    EXPECT_EQ(CKCID_PARAMETER, child->GetChunkClassID()); EXPECT_EQ(23, child->GetDataVersion());
    EXPECT_EQ(16, parent.GetCurrentPos()); EXPECT_EQ(0x1357, parent.ReadInt());
    const int expectedData[] = {11, 22, 33}; ExpectDataWords(*child.Get(), expectedData, 3);
    int envelope[32] = {}; ASSERT_EQ(13 * sizeof(int), child->ConvertToBuffer(envelope));
    EXPECT_EQ(CHNK_OPTION_IDS | CHNK_OPTION_CHN | CHNK_OPTION_MAN, static_cast<unsigned int>(envelope[0]) >> 24);
    const int expectedLists[] = {1, 1, 2, -1, 0, 2, 1, 2};
    for (int i = 0; i < 8; ++i) EXPECT_EQ(expectedLists[i], envelope[5 + i]);
}

TEST(CKStateChunkSubChunkTest, VersionFourHeaderHasNoManagerCount) {
    const int words[] = {8, CKCID_PARAMETER, (CHUNK_VERSION1 << 16) | 9, 1, 0, 1, 0, 41, 0, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION1, words, 10);
    CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get());
    EXPECT_EQ(9, child->GetDataVersion()); EXPECT_EQ(CHUNK_VERSION1, child->GetChunkVersion());
    child->StartRead(); EXPECT_EQ(41, child->ReadInt());
    EXPECT_EQ(9, parent.GetCurrentPos()); EXPECT_EQ(0x1357, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, LegacyParentReadsLegacyEmbeddedPayload) {
    const int words[] = {5, CKCID_PARAMETER, 2, 0, 41, 42, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSIONBASE, words, 7);
    CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get());
    EXPECT_EQ(CHUNK_VERSIONBASE, child->GetChunkVersion()); EXPECT_EQ(0, child->GetDataVersion());
    child->StartRead(); EXPECT_EQ(41, child->ReadInt()); EXPECT_EQ(42, child->ReadInt());
    EXPECT_EQ(6, parent.GetCurrentPos()); EXPECT_EQ(0x1357, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, ModernParentRecognizesNativeLegacyEmbeddedFallback) {
    // Native detection uses payload count + 4 == the embedded size. The legacy
    // reader consumes three header words and the payload, leaving the extra word.
    const int words[] = {6, CKCID_PARAMETER, 2, 0, 41, 42, 0x2468, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 8);
    CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get());
    EXPECT_EQ(CHUNK_VERSIONBASE, child->GetChunkVersion()); EXPECT_EQ(8, child->GetDataSize());
    child->StartRead(); EXPECT_EQ(41, child->ReadInt()); EXPECT_EQ(42, child->ReadInt());
    EXPECT_EQ(6, parent.GetCurrentPos()); EXPECT_EQ(0x2468, parent.ReadInt()); EXPECT_EQ(0x1357, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, MixedSequenceKeepsNullSlotsAndOneSequenceMetadataEntry) {
    const int empty[] = {7, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 0, 0, 0, 0, 0};
    CKStateChunk child(CKCID_PARAMETER, nullptr), parent(CKCID_OBJECT, nullptr);
    child.StartWrite(); child.CloseChunk();
    parent.StartWrite(); parent.WriteInt(0x2468); parent.StartSubChunkSequence(3);
    parent.WriteSubChunkSequence(nullptr); parent.WriteSubChunkSequence(&child);
    parent.WriteSubChunkSequence(nullptr); parent.WriteInt(0x1357); parent.CloseChunk();
    parent.StartRead(); EXPECT_EQ(0x2468, parent.ReadInt()); EXPECT_EQ(3, parent.StartReadSequence());
    EXPECT_EQ(nullptr, parent.ReadSubChunk()); CKTestChildOwner loaded(parent.ReadSubChunk());
    ASSERT_NE(nullptr, loaded.Get()); EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(0x1357, parent.ReadInt());
    int envelope[64] = {}; ASSERT_EQ(18 * sizeof(int), parent.ConvertToBuffer(envelope));
    EXPECT_EQ(2, envelope[15]); EXPECT_EQ(-1, envelope[16]); EXPECT_EQ(1, envelope[17]);
    for (int i = 0; i < 8; ++i) EXPECT_EQ(empty[i], envelope[5 + i]);
}

TEST(CKStateChunkSubChunkTest, ZeroLengthSequenceStillRecordsItsCountPosition) {
    CKStateChunk parent(CKCID_OBJECT, nullptr);
    EXPECT_EQ(0, parent.StartReadSequence()); EXPECT_EQ(-1, parent.GetCurrentPos());
    parent.StartWrite(); parent.StartSubChunkSequence(0);
    parent.WriteInt(0x1357); parent.CloseChunk(); parent.StartRead();
    EXPECT_EQ(0, parent.StartReadSequence()); EXPECT_EQ(0x1357, parent.ReadInt());
    EXPECT_EQ(0, parent.StartReadSequence()); EXPECT_EQ(2, parent.GetCurrentPos());
    int envelope[16] = {}; ASSERT_EQ(7 * sizeof(int), parent.ConvertToBuffer(envelope));
    EXPECT_EQ(2, envelope[4]); EXPECT_EQ(-1, envelope[5]); EXPECT_EQ(0, envelope[6]);
}

TEST(CKStateChunkSubChunkTest, ChildPayloadCannotConsumeFollowingParentWords) {
    const int words[] = {7, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 2, 0, 0, 0, 0, 41, 42, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 11);
    CKTestChildOwner child(parent.ReadSubChunk()); EXPECT_EQ(nullptr, child.Get());
    EXPECT_EQ(8, parent.GetCurrentPos()); EXPECT_EQ(41, parent.ReadInt()); EXPECT_EQ(42, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, ChildListCannotConsumeFollowingParentWords) {
    const int words[] = {7, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 0, 0, 1, 0, 0, 41, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 10);
    CKTestChildOwner child(parent.ReadSubChunk()); EXPECT_EQ(nullptr, child.Get());
    EXPECT_EQ(8, parent.GetCurrentPos()); EXPECT_EQ(41, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, ShortDeclaredHeaderDoesNotReadTheNextParentRecord) {
    const int words[] = {1, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 0, 0, 0, 0, 0, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 9);
    CKTestChildOwner child(parent.ReadSubChunk()); EXPECT_EQ(nullptr, child.Get());
    EXPECT_EQ(2, parent.GetCurrentPos()); EXPECT_EQ(CHUNK_VERSION4 << 16, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, NegativeListCountRejectsTheChildInsteadOfDroppingItsList) {
    const int words[] = {7, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 0, 0, -1, 0, 0, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 9);
    CKTestChildOwner child(parent.ReadSubChunk()); EXPECT_EQ(nullptr, child.Get());
    EXPECT_EQ(8, parent.GetCurrentPos()); EXPECT_EQ(0x1357, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, LegacyPayloadAlsoStaysInsideItsDeclaredFrame) {
    const int words[] = {3, CKCID_PARAMETER, 2, 0, 41, 42, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSIONBASE, words, 7);
    CKTestChildOwner child(parent.ReadSubChunk()); EXPECT_EQ(nullptr, child.Get());
    EXPECT_EQ(4, parent.GetCurrentPos()); EXPECT_EQ(41, parent.ReadInt());
}

TEST(CKStateChunkSubChunkTest, InvalidOuterSizeAndEndOfChunkRetainNativeConsumption) {
    const int words[] = {0, -1, 99, 0x1357};
    CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 4);
    EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(1, parent.GetCurrentPos());
    EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(2, parent.GetCurrentPos());
    EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(3, parent.GetCurrentPos());
    EXPECT_EQ(0x1357, parent.ReadInt()); EXPECT_EQ(nullptr, parent.ReadSubChunk()); EXPECT_EQ(4, parent.GetCurrentPos());
}

TEST(CKStateChunkSubChunkTest, FileAssociationRequiresTheNativeOneFlagAndParentFile) {
    ASSERT_EQ(CK_OK, CKStartUp());
    CKContext *context = nullptr; ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    {
        CKFile file(context); file.m_FileObjects.Resize(1); file.m_FileObjects[0].CreatedObject = 44;
        for (int flag = 0; flag <= 2; ++flag) {
            int words[] = {8, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, flag, 0, 0, 0, 0, 0x1357};
            CKStateChunk parent(CKCID_OBJECT, &file);
            LoadSubWords(parent, CHUNK_VERSION4, words, 10, CHNK_OPTION_FILE);
            CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get()); child->StartRead();
            EXPECT_EQ(flag == 1 ? 44u : 0u, child->ReadObjectID()) << "flag " << flag;
            EXPECT_EQ(9, parent.GetCurrentPos()); EXPECT_EQ(0x1357, parent.ReadInt());
        }
        const int words[] = {8, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, 1, 0, 0, 0, 44};
        CKStateChunk parent(CKCID_OBJECT, nullptr); LoadSubWords(parent, CHUNK_VERSION4, words, 9);
        CKTestChildOwner child(parent.ReadSubChunk()); ASSERT_NE(nullptr, child.Get()); child->StartRead();
        EXPECT_EQ(44u, child->ReadObjectID());
    }
    CKCloseContext(context); CKShutdown();
}
