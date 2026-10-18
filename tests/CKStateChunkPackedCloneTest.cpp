#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

class CKTestPackedOwner {
public:
    explicit CKTestPackedOwner(CKStateChunk *chunk) : Chunk(chunk) {}
    ~CKTestPackedOwner() { DeleteCKStateChunk(Chunk); }
    CKStateChunk *operator->() const { return Chunk; }
    CKStateChunk *Get() const { return Chunk; }
private:
    CKTestPackedOwner(const CKTestPackedOwner &);
    CKTestPackedOwner &operator=(const CKTestPackedOwner &);
    CKStateChunk *Chunk;
};

static const int WordCount = 64;
static const int RepeatWord = 0x11223344;

static void LoadWords(CKStateChunk &chunk, bool mixed = false) {
    int envelope[66] = {};
    envelope[0] = (7 << 16) | (CKCID_PARAMETER << 8) | 9;
    envelope[1] = WordCount;
    unsigned int random = 0x12345678u;
    for (int i = 0; i < WordCount; ++i) {
        if (mixed && i < 24) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            envelope[i + 2] = static_cast<int>(random);
        } else envelope[i + 2] = mixed ? 0 : RepeatWord;
    }
    ASSERT_TRUE(chunk.ConvertFromBuffer(envelope));
}

static void ExpectRepeatedWords(CKStateChunk &chunk) {
    EXPECT_EQ(CKCID_PARAMETER, chunk.GetChunkClassID()); EXPECT_EQ(9, chunk.GetDataVersion());
    EXPECT_EQ(7, chunk.GetChunkVersion()); EXPECT_EQ(WordCount * 4, chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < WordCount; ++i) EXPECT_EQ(RepeatWord, chunk.ReadInt());
}

static unsigned int OracleAdler(const unsigned char *bytes, int size, unsigned int seed) {
    unsigned int first = seed & 0xFFFFu, second = seed >> 16;
    for (int i = 0; i < size; ++i) {
        first = (first + bytes[i]) % 65521u;
        second = (second + first) % 65521u;
    }
    return (second << 16) | first;
}

static void ExpectPackedView(CKStateChunk &chunk) {
    const int bytes = chunk.GetDataSize();
    const int compressedBytes = bytes / 4;
    ASSERT_GT(compressedBytes, 0); ASSERT_LT(compressedBytes, WordCount * 4);
    chunk.StartRead();
    const unsigned char *view = static_cast<const unsigned char *>(chunk.LockReadBuffer());
    ASSERT_NE(nullptr, view);
    // Public word view retains the existing packed size convention. Bytes
    // after the actual compressed stream have defined zero contents.
    for (int i = compressedBytes; i < bytes; ++i) EXPECT_EQ(0, view[i]) << "byte " << i;
    const unsigned int seed = 0x12345678u;
    EXPECT_EQ(OracleAdler(view, bytes, seed), chunk.ComputeCRC(seed));
    EXPECT_EQ(0, chunk.GetCurrentPos());
}

static CKStateChunk *CopyByEntry(CKStateChunk *source, int entry) {
    if (entry == 0) return new CKStateChunk(source);
    if (entry == 1) return CreateCKStateChunk(source);
    CKStateChunk *copy = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    copy->Clone(source);
    return copy;
}

static void CheckDeletedSourceCopy(int entry) {
    CKStateChunk *source = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    ASSERT_NE(nullptr, source); LoadWords(*source); source->Pack(6);
    const int packedSize = source->GetDataSize(); ASSERT_LT(packedSize / 4, WordCount * 4);
    CKTestPackedOwner copy(CopyByEntry(source, entry)); ASSERT_NE(nullptr, copy.Get());
    EXPECT_EQ(packedSize, copy->GetDataSize()); DeleteCKStateChunk(source);
    ASSERT_TRUE(copy->UnPack(WordCount * 4)); ExpectRepeatedWords(*copy.Get());
}

TEST(CKStateChunkPackedCloneTest, PackedWordViewHasDefinedPaddingAndIndependentCrc) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadWords(chunk); chunk.Pack(6); ExpectPackedView(chunk);
    ASSERT_TRUE(chunk.UnPack(WordCount * 4)); ExpectRepeatedWords(chunk);
}

TEST(CKStateChunkPackedCloneTest, PackedExportOwnsTheWholeReportedWordView) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadWords(chunk); chunk.Pack(6);
    const int compressedBytes = chunk.GetDataSize() / 4;
    ASSERT_LT(compressedBytes, WordCount * 4); chunk.StartRead();
    const unsigned char *view = static_cast<const unsigned char *>(chunk.LockReadBuffer());
    int wire[258] = {}; ASSERT_EQ((compressedBytes + 2) * 4, chunk.ConvertToBuffer(wire));
    EXPECT_EQ(compressedBytes, wire[1]);
    EXPECT_EQ(0, std::memcmp(view, wire + 2, compressedBytes));
    const unsigned char *exported = reinterpret_cast<const unsigned char *>(wire + 2);
    for (int i = compressedBytes; i < compressedBytes * 4; ++i) EXPECT_EQ(0, exported[i]);
}

TEST(CKStateChunkPackedCloneTest, PointerConstructorPackedCopySurvivesSourceDeletion) { CheckDeletedSourceCopy(0); }
TEST(CKStateChunkPackedCloneTest, FactoryPackedCopySurvivesSourceDeletion) { CheckDeletedSourceCopy(1); }
TEST(CKStateChunkPackedCloneTest, ExplicitClonePackedCopySurvivesSourceDeletion) { CheckDeletedSourceCopy(2); }

TEST(CKStateChunkPackedCloneTest, UnpackingAndEditingOneOwnerDoesNotAffectTheOther) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadWords(source); source.Pack(6);
    const int packedSize = source.GetDataSize(); CKStateChunk copy(&source);
    ASSERT_TRUE(copy.UnPack(WordCount * 4)); copy.StartRead(); copy.WriteInt(47); copy.CloseChunk();
    EXPECT_EQ(packedSize, source.GetDataSize()); ASSERT_TRUE(source.UnPack(WordCount * 4)); ExpectRepeatedWords(source);
    copy.StartRead(); EXPECT_EQ(47, copy.ReadInt());
    for (int i = 1; i < WordCount; ++i) EXPECT_EQ(RepeatWord, copy.ReadInt());
}

TEST(CKStateChunkPackedCloneTest, PackedCloneExportsDefinedPaddingAfterSourceClear) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadWords(source); source.Pack(6); CKStateChunk copy(&source);
    source.Clear(); ExpectPackedView(copy);
    int wire[258] = {}; const int bytes = copy.GetDataSize(); const int compressedBytes = bytes / 4;
    ASSERT_EQ(bytes + 8, copy.ConvertToBuffer(wire));
    const unsigned char *exported = reinterpret_cast<const unsigned char *>(wire + 2);
    for (int i = compressedBytes; i < bytes; ++i) EXPECT_EQ(0, exported[i]);
    ASSERT_TRUE(copy.UnPack(WordCount * 4)); ExpectRepeatedWords(copy);
}

TEST(CKStateChunkPackedCloneTest, PackedCopyRetainsExistingParserCursorUntilUnpack) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadWords(source); source.StartRead(); source.Skip(WordCount);
    source.Pack(6); ASSERT_LT(source.GetDataSize() / 4, WordCount);
    EXPECT_EQ(WordCount, source.GetCurrentPos()); CKStateChunk copy(&source);
    EXPECT_EQ(WordCount, copy.GetCurrentPos()); ASSERT_TRUE(copy.UnPack(WordCount * 4));
    EXPECT_EQ(WordCount, copy.GetCurrentPos()); copy.WriteInt(47); copy.CloseChunk();
    EXPECT_EQ((WordCount + 1) * 4, copy.GetDataSize()); copy.StartRead();
    for (int i = 0; i < WordCount; ++i) EXPECT_EQ(RepeatWord, copy.ReadInt());
    EXPECT_EQ(47, copy.ReadInt()); ASSERT_TRUE(source.UnPack(WordCount * 4)); ExpectRepeatedWords(source);
}

TEST(CKStateChunkPackedCloneTest, UnpackRestoresActualCapacityForAppendingAfterAPartialCompression) {
    CKStateChunk source(CKCID_OBJECT, nullptr); LoadWords(source, true); source.Pack(6);
    const int compressedBytes = source.GetDataSize() / 4;
    ASSERT_GT(compressedBytes, WordCount); ASSERT_LT(compressedBytes, WordCount * 4);
    source.StartRead(); CKStateChunk copy(&source); ASSERT_TRUE(copy.UnPack(WordCount * 4));
    copy.Goto(WordCount); copy.WriteInt(47); copy.CloseChunk();
    EXPECT_EQ((WordCount + 1) * 4, copy.GetDataSize()); copy.StartRead();
    unsigned int random = 0x12345678u;
    for (int i = 0; i < WordCount; ++i) {
        if (i < 24) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            EXPECT_EQ(static_cast<int>(random), copy.ReadInt());
        } else EXPECT_EQ(0, copy.ReadInt());
    }
    EXPECT_EQ(47, copy.ReadInt());
}

TEST(CKStateChunkPackedCloneTest, UnpublishedWriterCopyKeepsPrefixAndBothOwnersCanContinueWriting) {
    CKStateChunk source(CKCID_PARAMETER, nullptr); source.StartWrite();
    source.WriteIdentifier(0x1111); source.WriteInt(41); ASSERT_EQ(0, source.GetDataSize());
    CKStateChunk copy(&source); EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(3, copy.GetCurrentPos());
    copy.WriteIdentifier(0x2222); copy.WriteInt(43); copy.CloseChunk();
    source.WriteIdentifier(0x3333); source.WriteInt(47); source.CloseChunk();
    copy.StartRead(); ASSERT_TRUE(copy.SeekIdentifier(0x1111)); EXPECT_EQ(41, copy.ReadInt());
    ASSERT_TRUE(copy.SeekIdentifier(0x2222)); EXPECT_EQ(43, copy.ReadInt()); EXPECT_FALSE(copy.SeekIdentifier(0x3333));
    source.StartRead(); ASSERT_TRUE(source.SeekIdentifier(0x1111)); EXPECT_EQ(41, source.ReadInt());
    ASSERT_TRUE(source.SeekIdentifier(0x3333)); EXPECT_EQ(47, source.ReadInt()); EXPECT_FALSE(source.SeekIdentifier(0x2222));
}

TEST(CKStateChunkPackedCloneTest, PublishedWriterCopyKeepsTheNewUnpublishedTail) {
    CKStateChunk source(CKCID_PARAMETER, nullptr); source.StartWrite();
    source.WriteIdentifier(0x1111); source.WriteInt(41); source.UpdateDataSize();
    source.WriteIdentifier(0x2222); source.WriteInt(43); ASSERT_EQ(12, source.GetDataSize());
    CKStateChunk copy(&source); EXPECT_EQ(12, copy.GetDataSize()); EXPECT_EQ(6, copy.GetCurrentPos());
    source.Clear(); copy.CloseChunk(); EXPECT_EQ(24, copy.GetDataSize());
    copy.StartRead(); ASSERT_TRUE(copy.SeekIdentifier(0x1111)); EXPECT_EQ(41, copy.ReadInt());
    ASSERT_TRUE(copy.SeekIdentifier(0x2222)); EXPECT_EQ(43, copy.ReadInt());
}

TEST(CKStateChunkPackedCloneTest, UnpublishedWriterRelocationMetadataStillDescribesCopiedPayload) {
    CKStateChunk source(CKCID_OBJECT, nullptr); source.StartWrite();
    source.WriteObjectID(41); source.WriteManagerInt(CKGUID(1, 2), 3);
    CKStateChunk child(CKCID_PARAMETER, nullptr); child.StartWrite(); child.SetDataVersion(9); child.WriteInt(47); child.CloseChunk();
    source.WriteSubChunk(&child); ASSERT_EQ(0, source.GetDataSize()); CKStateChunk copy(&source);
    EXPECT_NE(source.GetIds(), copy.GetIds()); EXPECT_NE(source.GetChunks(), copy.GetChunks());
    EXPECT_NE(source.GetManagers(), copy.GetManagers()); source.Clear(); copy.CloseChunk();
    ASSERT_EQ(1, copy.RemapObject(41, 43)); copy.StartRead(); EXPECT_EQ(43u, copy.ReadObjectID());
    CKGUID guid; EXPECT_EQ(3, copy.ReadManagerInt(&guid)); EXPECT_EQ(CKGUID(1, 2), guid);
    CKTestPackedOwner loaded(copy.ReadSubChunk()); ASSERT_NE(nullptr, loaded.Get());
    EXPECT_EQ(CKCID_PARAMETER, loaded->GetChunkClassID()); EXPECT_EQ(9, loaded->GetDataVersion());
    loaded->StartRead(); EXPECT_EQ(47, loaded->ReadInt());
}

TEST(CKStateChunkPackedCloneTest, PackAndUnpackPreservePreviousIdentifierWhenContinuingTheParser) {
    int wire[66] = {}; wire[0] = (7 << 16) | (CKCID_OBJECT << 8); wire[1] = WordCount;
    for (int i = 2; i < 66; ++i) wire[i] = 43;
    wire[2] = 0x1111; wire[3] = 3; wire[4] = 41; wire[5] = 0x2222; wire[6] = 0;
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead(); chunk.Goto(3); ASSERT_EQ(0x2222u, chunk.ReadIdentifier()); EXPECT_EQ(5, chunk.GetCurrentPos());
    chunk.Pack(6); EXPECT_EQ(5, chunk.GetCurrentPos()); ASSERT_TRUE(chunk.UnPack(WordCount * 4));
    EXPECT_EQ(5, chunk.GetCurrentPos()); chunk.WriteIdentifier(0x3333); chunk.WriteInt(47); chunk.CloseChunk();
    chunk.StartRead(); ASSERT_TRUE(chunk.SeekIdentifier(0x2222)); ASSERT_TRUE(chunk.SeekIdentifier(0x3333));
    EXPECT_EQ(47, chunk.ReadInt());
}
