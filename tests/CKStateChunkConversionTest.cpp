#include <gtest/gtest.h>

#include <cstring>
#include <miniz.h>

#include "CKAll.h"

static int ModernHeader(int version, int dataVersion, int classId, int options) {
    return static_cast<int>((static_cast<unsigned int>(version | (options << 8)) << 16) |
                            static_cast<unsigned int>((classId << 8) | dataVersion));
}

static void ExpectList(IntListStruct *list, const int *values, int count) {
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(count, list->Size);
    for (int i = 0; i < count; ++i) EXPECT_EQ(values[i], list->Data[i]);
}

static void ExpectWords(CKStateChunk &chunk, const int *values, int count) {
    ASSERT_EQ(count * static_cast<int>(sizeof(int)), chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < count; ++i) EXPECT_EQ(values[i], chunk.ReadInt());
}

static void LoadCompressibleWords(CKStateChunk &chunk) {
    int wire[66] = {};
    wire[0] = ModernHeader(CHUNK_VERSION4, 3, CKCID_OBJECT, 0);
    wire[1] = 64;
    for (int i = 0; i < 64; ++i) wire[i + 2] = 0x11223344;
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

static void LoadPackedLiteral(CKStateChunk &chunk, const unsigned char *bytes, int byteCount) {
    // Pack's state stores a BYTE count in the same size field that ordinarily
    // counts DWORDs. A padded literal import builds that state without using
    // Pack; UnPack sees exactly byteCount bytes, independently of its encoder.
    int wire[66] = {};
    ASSERT_LE(byteCount, 64);
    wire[0] = ModernHeader(CHUNK_VERSION4, 3, CKCID_OBJECT, 0);
    wire[1] = byteCount;
    std::memcpy(wire + 2, bytes, byteCount);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

static void ExpectCompressedStream(CKStateChunk &chunk) {
    const int compressedBytes = chunk.GetDataSize() / static_cast<int>(sizeof(int));
    ASSERT_GT(compressedBytes, 0);
    ASSERT_LT(compressedBytes, 256);
    chunk.StartRead();
    unsigned char decoded[256] = {};
    mz_ulong decodedBytes = sizeof(decoded);
    ASSERT_EQ(MZ_OK, mz_uncompress(decoded, &decodedBytes,
                                  static_cast<const unsigned char *>(chunk.LockReadBuffer()), compressedBytes));
    ASSERT_EQ(sizeof(decoded), decodedBytes);
    const int expected = 0x11223344;
    for (int i = 0; i < 64; ++i) EXPECT_EQ(0, std::memcmp(decoded + i * sizeof(int), &expected, sizeof(int)));
}

TEST(CKStateChunkConversionTest, ModernVersionsDecodeLiteralHeaderAndPayload) {
    for (int version = CHUNK_VERSION3; version <= CHUNK_VERSION4; ++version) {
        int wire[] = {ModernHeader(version, 0xA5, CKCID_OBJECT, 0), 3, 0x11223344, -1, 0};
        CKStateChunk chunk(CKCID_SCENE, nullptr);
        ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
        EXPECT_EQ(version, chunk.GetChunkVersion());
        EXPECT_EQ(0xA5, chunk.GetDataVersion());
        EXPECT_EQ(CKCID_OBJECT, chunk.GetChunkClassID());
        EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks()); EXPECT_EQ(nullptr, chunk.GetManagers());
        ExpectWords(chunk, wire + 2, 3);
    }
}

TEST(CKStateChunkConversionTest, ModernNonemptyListsUseIdsChunksManagersWireOrder) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 9, CKCID_OBJECT, 7), 4, 11, 22, 33, 44,
                  2, 0, 3, 2, -1, 1, 3, 2, -1, 0};
    CKStateChunk chunk(CKCID_SCENE, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    const int ids[] = {0, 3}, children[] = {-1, 1}, managers[] = {2, -1, 0};
    ExpectList(chunk.GetIds(), ids, 2); ExpectList(chunk.GetChunks(), children, 2); ExpectList(chunk.GetManagers(), managers, 3);
    ExpectWords(chunk, wire + 2, 4);
    int output[17] = {};
    ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(nullptr));
    ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output));
    EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}

TEST(CKStateChunkConversionTest, ModernPresentEmptyListsRetainPresenceAndWireCounts) {
    for (int options = 1; options < 8; ++options) {
        int wire[5] = {ModernHeader(CHUNK_VERSION4, 7, CKCID_OBJECT, options), 0};
        int words = 2;
        for (int bit = 1; bit <= 4; bit <<= 1) if (options & bit) ++words;
        CKStateChunk chunk(CKCID_SCENE, nullptr);
        ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
        if (options & CHNK_OPTION_IDS) ExpectList(chunk.GetIds(), nullptr, 0); else EXPECT_EQ(nullptr, chunk.GetIds());
        if (options & CHNK_OPTION_CHN) ExpectList(chunk.GetChunks(), nullptr, 0); else EXPECT_EQ(nullptr, chunk.GetChunks());
        if (options & CHNK_OPTION_MAN) ExpectList(chunk.GetManagers(), nullptr, 0); else EXPECT_EQ(nullptr, chunk.GetManagers());
        int output[5] = {};
        ASSERT_EQ(words * static_cast<int>(sizeof(int)), chunk.ConvertToBuffer(output));
        EXPECT_EQ(0, std::memcmp(wire, output, words * sizeof(int)));
    }
}

TEST(CKStateChunkConversionTest, ModernImportOwnsCopiesOfAllLiteralData) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 2, CKCID_OBJECT, 7), 2, 41, 43, 1, 0, 1, 1, 1, 0};
    CKStateChunk chunk(CKCID_SCENE, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    std::memset(wire, 0, sizeof(wire));
    const int payload[] = {41, 43}, ids[] = {0}, children[] = {1}, managers[] = {0};
    ExpectWords(chunk, payload, 2); ExpectList(chunk.GetIds(), ids, 1);
    ExpectList(chunk.GetChunks(), children, 1); ExpectList(chunk.GetManagers(), managers, 1);
}

TEST(CKStateChunkConversionTest, LegacyVersionsDecodeSixWordHeadersAndListOffsets) {
    const int versions[] = {CHUNK_VERSIONBASE, CHUNK_VERSION1};
    for (int i = 0; i < 2; ++i) {
        int wire[] = {(versions[i] << 16) | 0xD5, 0x123, 3, 0x76543210, 2, 1, 41, 42, 43, 0, 2, 1};
        CKStateChunk chunk(CKCID_OBJECT, nullptr);
        ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
        EXPECT_EQ(versions[i], chunk.GetChunkVersion()); EXPECT_EQ(0xD5, chunk.GetDataVersion());
        EXPECT_EQ(0x123, chunk.GetChunkClassID());
        const int ids[] = {0, 2}, children[] = {1};
        ExpectWords(chunk, wire + 6, 3); ExpectList(chunk.GetIds(), ids, 2); ExpectList(chunk.GetChunks(), children, 1);
        EXPECT_EQ(nullptr, chunk.GetManagers());
    }
}

TEST(CKStateChunkConversionTest, VersionTwoDecodesSevenWordHeaderAndManagerTail) {
    int wire[] = {(CHUNK_VERSION2 << 16) | 0xA7, 0x234, 2, 0x76543210, 1, 1, 2, 41, 42, 0, 1, -1, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    EXPECT_EQ(CHUNK_VERSION2, chunk.GetChunkVersion()); EXPECT_EQ(0xA7, chunk.GetDataVersion()); EXPECT_EQ(0x234, chunk.GetChunkClassID());
    const int ids[] = {0}, children[] = {1}, managers[] = {-1, 0};
    ExpectWords(chunk, wire + 7, 2); ExpectList(chunk.GetIds(), ids, 1); ExpectList(chunk.GetChunks(), children, 1);
    ExpectList(chunk.GetManagers(), managers, 2);
}

TEST(CKStateChunkConversionTest, LegacyExportKeepsMatchingHeaderLayoutAndFullClassId) {
    const int versions[] = {CHUNK_VERSIONBASE, CHUNK_VERSION1};
    for (int i = 0; i < 2; ++i) {
        int wire[] = {(versions[i] << 16) | 7, 0x123, 2, 0, 1, 1, 41, 43, 0, 1};
        CKStateChunk chunk(CKCID_OBJECT, nullptr);
        ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
        int output[10] = {};
        ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(nullptr));
        ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output));
        EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
        CKStateChunk restored(CKCID_OBJECT, nullptr);
        ASSERT_TRUE(restored.ConvertFromBuffer(output));
        ExpectWords(restored, wire + 6, 2); EXPECT_EQ(0x123, restored.GetChunkClassID());
    }
}

TEST(CKStateChunkConversionTest, VersionTwoExportRetainsManagersAndVersion) {
    int wire[] = {(CHUNK_VERSION2 << 16) | 13, 0x234, 2, 0, 1, 1, 2, 41, 43, 0, 1, -1, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    int output[13] = {};
    ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output));
    EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
    CKStateChunk restored(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(restored.ConvertFromBuffer(output));
    const int managers[] = {-1, 0}; ExpectList(restored.GetManagers(), managers, 2);
}

TEST(CKStateChunkConversionTest, ModernExportDoesNotMutateVersionsOrParser) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 0xA5, CKCID_OBJECT, 0), 2, 41, 43};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire)); chunk.StartRead(); EXPECT_EQ(41, chunk.ReadInt());
    for (int i = 0; i < 3; ++i) {
        int output[4] = {};
        ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(nullptr)); ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output));
        EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
        EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(CHUNK_VERSION4, chunk.GetChunkVersion()); EXPECT_EQ(0xA5, chunk.GetDataVersion());
    }
    EXPECT_EQ(43, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, ImportReplacesOldPayloadListsAndParser) {
    int first[] = {ModernHeader(CHUNK_VERSION4, 1, CKCID_OBJECT, 7), 1, 41, 1, 0, 1, 0, 1, 0};
    int second[] = {ModernHeader(CHUNK_VERSION3, 2, CKCID_SCENE, 0), 2, 43, 47};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(first)); chunk.StartRead(); EXPECT_EQ(41, chunk.ReadInt());
    ASSERT_TRUE(chunk.ConvertFromBuffer(second)); EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(nullptr, chunk.GetIds()); EXPECT_EQ(nullptr, chunk.GetChunks()); EXPECT_EQ(nullptr, chunk.GetManagers());
    EXPECT_EQ(CKCID_SCENE, chunk.GetChunkClassID()); ExpectWords(chunk, second + 2, 2);
}

TEST(CKStateChunkConversionTest, NullInputPreservesExistingChunkAndParser) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 5, CKCID_OBJECT, 0), 2, 41, 43};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire)); chunk.StartRead(); EXPECT_EQ(41, chunk.ReadInt());
    EXPECT_FALSE(chunk.ConvertFromBuffer(nullptr)); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(43, chunk.ReadInt()); EXPECT_EQ(5, chunk.GetDataVersion());
}

TEST(CKStateChunkConversionTest, UnsupportedVersionFailsInsteadOfClaimingLoadedData) {
    int wire[] = {(8 << 16) | 9, 0};
    CKStateChunk chunk(CKCID_SCENE, nullptr);
    EXPECT_FALSE(chunk.ConvertFromBuffer(wire)); EXPECT_EQ(0, chunk.GetDataSize()); EXPECT_EQ(0, chunk.GetChunkClassID());
}

TEST(CKStateChunkConversionTest, PackProducesExternallyDecodableZlibStream) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk);
    chunk.Pack(6); ExpectCompressedStream(chunk);
}

TEST(CKStateChunkConversionTest, UnPackExactCapacityRestoresWordPayload) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk); chunk.Pack(6);
    ASSERT_TRUE(chunk.UnPack(256)); EXPECT_EQ(256, chunk.GetDataSize());
    chunk.StartRead(); for (int i = 0; i < 64; ++i) EXPECT_EQ(0x11223344, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, UnPackLargerCapacityUsesActualDecodedSize) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk); chunk.Pack(6);
    ASSERT_TRUE(chunk.UnPack(512)); EXPECT_EQ(256, chunk.GetDataSize());
    chunk.StartRead(); for (int i = 0; i < 64; ++i) EXPECT_EQ(0x11223344, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, UnPackCapacityNeedNotBeWordAligned) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk); chunk.Pack(6);
    ASSERT_TRUE(chunk.UnPack(257)); EXPECT_EQ(256, chunk.GetDataSize());
    chunk.StartRead(); for (int i = 0; i < 64; ++i) EXPECT_EQ(0x11223344, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, UnPackReadsIndependentLiteralZlibStream) {
    const unsigned char bytes[] = {120, 156, 115, 49, 86, 18, 116, 25, 193, 24, 0, 108, 187, 42, 129};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPackedLiteral(chunk, bytes, sizeof(bytes));
    ASSERT_TRUE(chunk.UnPack(256)); EXPECT_EQ(256, chunk.GetDataSize());
    chunk.StartRead(); for (int i = 0; i < 64; ++i) EXPECT_EQ(0x11223344, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, UnPackCanDecodeEmptyLiteralStreamIntoEmptyChunk) {
    const unsigned char bytes[] = {120, 156, 3, 0, 0, 0, 0, 1};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPackedLiteral(chunk, bytes, sizeof(bytes));
    ASSERT_TRUE(chunk.UnPack(64)); EXPECT_EQ(0, chunk.GetDataSize());
}

TEST(CKStateChunkConversionTest, UnPackRejectsNonWordPayloadWithoutDiscardingStream) {
    // This independent stream contains exactly "abc". It cannot be stored as
    // complete chunk words; the native floor(size / 4) allocation would be too
    // small for the subsequent three-byte copy.
    const unsigned char bytes[] = {120, 156, 75, 76, 74, 6, 0, 2, 77, 1, 39};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPackedLiteral(chunk, bytes, sizeof(bytes));
    const int beforeSize = chunk.GetDataSize();
    EXPECT_FALSE(chunk.UnPack(64)); EXPECT_EQ(beforeSize, chunk.GetDataSize());
    chunk.StartRead(); EXPECT_EQ(0, std::memcmp(bytes, chunk.LockReadBuffer(), sizeof(bytes)));
}

TEST(CKStateChunkConversionTest, FailedUnPackPreservesCompressedStreamForRetry) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk); chunk.Pack(6);
    const int packedSize = chunk.GetDataSize();
    EXPECT_FALSE(chunk.UnPack(128)); EXPECT_EQ(packedSize, chunk.GetDataSize()); ExpectCompressedStream(chunk);
    ASSERT_TRUE(chunk.UnPack(256)); EXPECT_EQ(256, chunk.GetDataSize());
}

TEST(CKStateChunkConversionTest, InvalidCompressionLevelPreservesOriginalWords) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadCompressibleWords(chunk); chunk.Pack(99);
    EXPECT_EQ(256, chunk.GetDataSize()); chunk.StartRead(); for (int i = 0; i < 64; ++i) EXPECT_EQ(0x11223344, chunk.ReadInt());
}

TEST(CKStateChunkConversionTest, IneffectiveCompressionRetainsOriginalSmallChunk) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 0, CKCID_OBJECT, 0), 1, 0x12345678};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire)); chunk.Pack(6);
    ExpectWords(chunk, wire + 2, 1);
}

TEST(CKStateChunkConversionTest, EmptyPackAndUnPackLeaveChunkEmpty) {
    int wire[] = {ModernHeader(CHUNK_VERSION4, 0, CKCID_OBJECT, 0), 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.Pack(6); EXPECT_FALSE(chunk.UnPack(256)); EXPECT_EQ(0, chunk.GetDataSize());
}

class CKStateChunkFileConversionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    static void TearDownTestSuite() { if (context) CKCloseContext(context); context = nullptr; CKShutdown(); }
    static CKContext *context;
};
CKContext *CKStateChunkFileConversionTest::context = nullptr;

TEST_F(CKStateChunkFileConversionTest, FileOptionRetainsDestinationMappingAndExportFlag) {
    CKFile file(context); file.m_FileObjects.Resize(1); file.m_FileObjects[0].CreatedObject = 123;
    int wire[] = {ModernHeader(CHUNK_VERSION4, 3, CKCID_OBJECT, CHNK_OPTION_FILE), 1, 0};
    CKStateChunk chunk(CKCID_OBJECT, &file); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead(); EXPECT_EQ(123u, chunk.ReadObjectID());
    int output[3] = {}; ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output)); EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}

TEST_F(CKStateChunkFileConversionTest, NoFileOptionClearsExistingDestinationMapping) {
    CKFile file(context); file.m_FileObjects.Resize(1); file.m_FileObjects[0].CreatedObject = 123;
    int wire[] = {ModernHeader(CHUNK_VERSION4, 3, CKCID_OBJECT, 0), 1, 0};
    CKStateChunk chunk(CKCID_OBJECT, &file); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead(); EXPECT_EQ(0u, chunk.ReadObjectID());
    int output[3] = {}; ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output)); EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}

TEST_F(CKStateChunkFileConversionTest, LegacyImportClearsFileAssociation) {
    CKFile file(context); file.m_FileObjects.Resize(1); file.m_FileObjects[0].CreatedObject = 123;
    int wire[] = {(CHUNK_VERSION2 << 16), CKCID_OBJECT, 1, 0, 0, 0, 0, 0};
    CKStateChunk chunk(CKCID_OBJECT, &file); ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead(); EXPECT_EQ(0u, chunk.ReadObjectID());
    int output[8] = {}; ASSERT_EQ(sizeof(wire), chunk.ConvertToBuffer(output)); EXPECT_EQ(0, std::memcmp(wire, output, sizeof(wire)));
}
