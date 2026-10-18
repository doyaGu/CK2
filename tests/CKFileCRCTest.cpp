#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

static CKDWORD OracleAdler(const unsigned char *bytes, int count, CKDWORD seed = 0) {
    unsigned int first = seed & 0xFFFFu;
    unsigned int second = seed >> 16;
    for (int i = 0; i < count; ++i) {
        first = (first + bytes[i]) % 65521u;
        second = (second + first) % 65521u;
    }
    return (second << 16) | first;
}

static void PutWord(unsigned char *bytes, int offset, CKDWORD value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

static CKDWORD GetWord(const unsigned char *bytes, int offset) {
    CKDWORD value = 0;
    for (int i = 0; i < 4; ++i) value |= static_cast<CKDWORD>(bytes[offset + i]) << (8 * i);
    return value;
}

struct CKTestLiteralFrame {
    unsigned char Bytes[160];
    int Size;
    int HeaderBytes;
    int DataBytes;

    explicit CKTestLiteralFrame(bool compressed) : Size(0), HeaderBytes(0), DataBytes(0) {
        std::memset(Bytes, 0, sizeof(Bytes));
        // Independent v8 format: no objects; one manager record with a valid
        // 16-byte StateChunk carrying two known integers. No unused fake data.
        unsigned char header[12] = {};
        PutWord(header, 4, 4); // included-files header size; count below is zero
        unsigned char data[28] = {};
        PutWord(data, 0, 0x11223344); PutWord(data, 4, 0x55667788);
        PutWord(data, 8, 16);
        PutWord(data, 12, (7u << 16) | (1u << 8) | 9u);
        PutWord(data, 16, 2); PutWord(data, 20, 0x13572468); PutWord(data, 24, 0x24681357);
        // Fixed zlib streams generated independently from the literal records
        // above with Python zlib. Runtime encoders are never the input oracle.
        const unsigned char packedHeader[] = {
            0x78, 0x9C, 0x63, 0x60, 0x60, 0x60, 0x60, 0x61, 0x80, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x05
        };
        const unsigned char packedData[] = {
            0x78, 0x9C, 0x73, 0x31, 0x56, 0x12, 0xEC, 0x28, 0x4F, 0x0B, 0x15, 0x60, 0x60, 0x60, 0xE0,
            0x64, 0x64, 0x67, 0x60, 0x02, 0xD2, 0x19, 0x2A, 0xE1, 0xC2, 0xE1, 0xC2, 0x19, 0x2A, 0x00,
            0x44, 0xBF, 0x04, 0x74
        };
        HeaderBytes = compressed ? static_cast<int>(sizeof(packedHeader)) : static_cast<int>(sizeof(header));
        DataBytes = compressed ? static_cast<int>(sizeof(packedData)) : static_cast<int>(sizeof(data));
        Size = 64 + HeaderBytes + DataBytes;
        std::memcpy(Bytes, "Nemo Fi", 7);
        PutWord(Bytes, 12, 0x13022002); PutWord(Bytes, 16, 8);
        PutWord(Bytes, 24, compressed ? 12 : 0); PutWord(Bytes, 28, HeaderBytes);
        PutWord(Bytes, 32, DataBytes); PutWord(Bytes, 36, sizeof(data)); PutWord(Bytes, 40, 1);
        PutWord(Bytes, 56, 0x01010000); PutWord(Bytes, 60, sizeof(header));
        std::memcpy(Bytes + 64, compressed ? packedHeader : header, HeaderBytes);
        std::memcpy(Bytes + 64 + HeaderBytes, compressed ? packedData : data, DataBytes);
        UpdateChecksum();
    }

    void UpdateChecksum() {
        PutWord(Bytes, 8, 0);
        PutWord(Bytes, 8, OracleAdler(Bytes, 64 + HeaderBytes + DataBytes));
    }
};

class CKTestCRCFile : public CKFile {
public:
    explicit CKTestCRCFile(CKContext *context) : CKFile(context) {}
    CKERROR ReadOpenedData() { return ReadFileData(&m_Parser); }
};

class CKFileCRCTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        ASSERT_NE(nullptr, Context);
    }
    static void TearDownTestSuite() { if (Context) CKCloseContext(Context); Context = nullptr; CKShutdown(); }

    void ExpectParsedManager(CKTestCRCFile &file) {
        ASSERT_EQ(1, file.m_ManagersData.Size());
        EXPECT_EQ(CKGUID(0x11223344, 0x55667788), file.m_ManagersData[0].Manager);
        CKStateChunk *chunk = file.m_ManagersData[0].data;
        ASSERT_NE(nullptr, chunk); EXPECT_EQ(CKCID_OBJECT, chunk->GetChunkClassID());
        EXPECT_EQ(7, chunk->GetChunkVersion()); EXPECT_EQ(9, chunk->GetDataVersion());
        EXPECT_EQ(8, chunk->GetDataSize()); chunk->StartRead();
        EXPECT_EQ(0x13572468, chunk->ReadInt()); EXPECT_EQ(0x24681357, chunk->ReadInt());
        EXPECT_EQ(0, file.m_FileObjects.Size()); EXPECT_EQ(0, file.m_IncludedFiles.Size());
    }

    static CKContext *Context;
};

CKContext *CKFileCRCTest::Context = nullptr;

TEST(CKDataCRCTest, KnownAsciiVectorsDistinguishDefaultZeroFromZlibInitialOne) {
    const char text[] = "123456789";
    EXPECT_EQ(0x091501DDu, CKComputeDataCRC(text, 9));
    EXPECT_EQ(0x091E01DEu, CKComputeDataCRC(text, 9, 1));
    EXPECT_EQ(OracleAdler(reinterpret_cast<const unsigned char *>(text), 9), CKComputeDataCRC(text, 9));
}

TEST(CKDataCRCTest, FullByteRangeMatchesIndependentGoldenAndOracle) {
    unsigned char bytes[256]; for (int i = 0; i < 256; ++i) bytes[i] = static_cast<unsigned char>(i);
    EXPECT_EQ(0xACF67F80u, CKComputeDataCRC(reinterpret_cast<const char *>(bytes), 256));
    EXPECT_EQ(OracleAdler(bytes, 256), CKComputeDataCRC(reinterpret_cast<const char *>(bytes), 256));
}

TEST(CKDataCRCTest, ArbitraryUnsignedSeedsRetainBothSixteenBitAccumulators) {
    const unsigned char bytes[] = {0, 1, 0x80, 0xFF, 17, 43};
    const CKDWORD seeds[] = {0, 1, 0x12345678u, 0x80000000u, 0xFFFFFFFFu};
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(OracleAdler(bytes, sizeof(bytes), seeds[i]),
                  CKComputeDataCRC(reinterpret_cast<const char *>(bytes), sizeof(bytes), seeds[i]));
    }
}

TEST(CKDataCRCTest, SegmentedUpdatesEqualIndependentContiguousChecksum) {
    const unsigned char bytes[] = {0, 1, 0x80, 0xFF, 17, 43, 67, 99, 127};
    const CKDWORD seeds[] = {0, 1, 0x12345678u, 0xFFFFFFFFu};
    for (int seed = 0; seed < 4; ++seed) {
        CKDWORD actual = seeds[seed];
        for (int i = 0; i < 9; ++i) actual = CKComputeDataCRC(reinterpret_cast<const char *>(bytes + i), 1, actual);
        EXPECT_EQ(OracleAdler(bytes, 9, seeds[seed]), actual);
    }
}

TEST(CKDataCRCTest, EmptyNonNullInputPreservesSeedWhileNullResetsToOne) {
    const char byte = 0;
    EXPECT_EQ(0u, CKComputeDataCRC(&byte, 0));
    EXPECT_EQ(0x12345678u, CKComputeDataCRC(&byte, 0, 0x12345678u));
    EXPECT_EQ(1u, CKComputeDataCRC(nullptr, 0));
    EXPECT_EQ(1u, CKComputeDataCRC(nullptr, 0, 0x12345678u));
}

TEST(CKDataCRCTest, LongInputCrossesAdlerReductionBoundaries) {
    unsigned char bytes[65536];
    for (int i = 0; i < 65536; ++i) bytes[i] = static_cast<unsigned char>((i * 37 + 11) & 255);
    const int lengths[] = {5551, 5552, 5553, 65536};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(OracleAdler(bytes, lengths[i]), CKComputeDataCRC(reinterpret_cast<const char *>(bytes), lengths[i]));
    }
}

TEST_F(CKFileCRCTest, PlainLiteralFramePassesChecksumAndParsesActualManagerChunk) {
    CKTestLiteralFrame frame(false); EXPECT_EQ(0x60440767u, GetWord(frame.Bytes, 8));
    CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_NE(nullptr, file.m_Parser); EXPECT_EQ(76, file.m_Parser->m_CursorPos);
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
    EXPECT_EQ(frame.Size, file.m_Parser->m_CursorPos);
}

TEST_F(CKFileCRCTest, PackedLiteralFramePassesChecksumAndParsesActualManagerChunk) {
    CKTestLiteralFrame frame(true); EXPECT_EQ(0xD7DB145Bu, GetWord(frame.Bytes, 8));
    CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_NE(nullptr, file.m_Parser); EXPECT_EQ(79, file.m_Parser->m_CursorPos);
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
    EXPECT_EQ(frame.Size, file.m_Parser->m_CursorPos);
}

TEST_F(CKFileCRCTest, ViewerFlagTwelveToEightCannotKeepTheOriginalChecksum) {
    CKTestLiteralFrame frame(true); ASSERT_EQ(12u, GetWord(frame.Bytes, 24));
    const CKDWORD original = GetWord(frame.Bytes, 8); PutWord(frame.Bytes, 24, 8);
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(original, GetWord(frame.Bytes, 8)); EXPECT_EQ(8u, GetWord(frame.Bytes, 24));
}

TEST_F(CKFileCRCTest, PartZeroVersionMutationIsCovered) {
    CKTestLiteralFrame frame(false); frame.Bytes[12] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, PartOneProductBuildMutationIsCovered) {
    CKTestLiteralFrame frame(false); frame.Bytes[56] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, PackedHeaderPayloadMutationIsRejectedBeforeDecompression) {
    CKTestLiteralFrame frame(true); frame.Bytes[64] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, PackedDataPayloadMutationIsRejectedBeforeDecompression) {
    CKTestLiteralFrame frame(true); frame.Bytes[64 + frame.HeaderBytes] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, PlainHeaderPayloadMutationIsCovered) {
    CKTestLiteralFrame frame(false); frame.Bytes[68] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, PlainDataPayloadMutationIsCovered) {
    CKTestLiteralFrame frame(false); frame.Bytes[64 + frame.HeaderBytes + 20] ^= 1;
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, ZeroStoredChecksumIsRejected) {
    CKTestLiteralFrame frame(false); PutWord(frame.Bytes, 8, 0);
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, CorruptedNonzeroStoredChecksumIsRejected) {
    CKTestLiteralFrame frame(true); frame.Bytes[8] ^= 1;
    ASSERT_NE(0u, GetWord(frame.Bytes, 8));
    CKTestCRCFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileCRCTest, DeclaredRegionsExcludeTrailingBytesFromChecksum) {
    for (int compressed = 0; compressed < 2; ++compressed) {
        CKTestLiteralFrame frame(compressed != 0); const int coveredSize = frame.Size;
        for (int i = 0; i < 8; ++i) frame.Bytes[frame.Size++] = static_cast<unsigned char>(0xA0 + i);
        CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
        ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
        EXPECT_EQ(coveredSize, file.m_Parser->m_CursorPos);
    }
}

TEST_F(CKFileCRCTest, LegitimatelyRecomputedViewerFlagChangeStillLoads) {
    CKTestLiteralFrame frame(true); PutWord(frame.Bytes, 24, 8); frame.UpdateChecksum();
    ASSERT_NE(0xD7DB145Bu, GetWord(frame.Bytes, 8));
    CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(8u, file.m_FileInfo.FileWriteMode);
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
}

TEST_F(CKFileCRCTest, HeaderVerificationDoesNotMutateCallerMemory) {
    CKTestLiteralFrame frame(true); unsigned char original[160]; std::memcpy(original, frame.Bytes, sizeof(original));
    CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
    EXPECT_EQ(0, std::memcmp(original, frame.Bytes, sizeof(original)));
}

TEST_F(CKFileCRCTest, ReopeningAfterCrcFailureRevalidatesEveryInput) {
    CKTestLiteralFrame valid(false), corrupt(false); corrupt.Bytes[56] ^= 1;
    CKTestCRCFile file(Context); ASSERT_EQ(CKERR_FILECRCERROR, file.OpenMemory(corrupt.Bytes, corrupt.Size));
    ASSERT_EQ(CK_OK, file.OpenMemory(valid.Bytes, valid.Size));
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
    EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(corrupt.Bytes, corrupt.Size));
}

TEST_F(CKFileCRCTest, ProductCompatibilityNormalizationDoesNotRewriteRawChecksumCoverage) {
    CKTestLiteralFrame frame(false); PutWord(frame.Bytes, 52, 12); PutWord(frame.Bytes, 56, 0x12345678); frame.UpdateChecksum();
    CKTestCRCFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(0u, file.m_FileInfo.ProductVersion); EXPECT_EQ(0x01010000u, file.m_FileInfo.ProductBuild);
    EXPECT_EQ(12u, GetWord(frame.Bytes, 52)); EXPECT_EQ(0x12345678u, GetWord(frame.Bytes, 56));
    ASSERT_EQ(CK_OK, file.ReadOpenedData()); ExpectParsedManager(file);
}
