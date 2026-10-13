#include <gtest/gtest.h>
#include <cstring>
#include "CKAll.h"

namespace {

void PutLegacyWord(unsigned char *data, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i)
        data[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

unsigned int LegacyAdler(const unsigned char *data, int size) {
    unsigned int a = 0, b = 0;
    for (int i = 0; i < size; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return a | (b << 16);
}

// Independently authored wire data. File format versions are distinct from
// the authoring product: the reference consumer is Virtools 2.1.0.14.
struct LegacyFrame {
    unsigned char Bytes[256];
    int Version, HeaderSize, DataSize, Size;
    explicit LegacyFrame(int version, bool object = true)
        : Version(version), HeaderSize(version < 5 ? 32 : 64), DataSize(8), Size(0) {
        std::memset(Bytes, 0, sizeof(Bytes));
        std::memcpy(Bytes, "Nemo Fi", 7);
        PutLegacyWord(Bytes, 12, 0x13022002);
        PutLegacyWord(Bytes, 16, version);
        PutLegacyWord(Bytes, 24, CKFILE_UNCOMPRESSED);
        PutLegacyWord(Bytes, HeaderSize, 73);
        PutLegacyWord(Bytes, HeaderSize + 4, object ? 1 : 0);
        if (object) {
            const int record = HeaderSize + 8;
            PutLegacyWord(Bytes, record, 73);
            if (version < 4) {
                PutLegacyWord(Bytes, record + 4, 16);
                PutLegacyWord(Bytes, record + 8, CKCID_BEOBJECT);
                PutLegacyWord(Bytes, record + 12, 0x1234);
                PutLegacyWord(Bytes, record + 16, 16);
                DataSize += 20;
            } else {
                PutLegacyWord(Bytes, record + 4, 40);
                PutLegacyWord(Bytes, record + 8, 4u << 16);
                PutLegacyWord(Bytes, record + 12, CKCID_BEOBJECT);
                PutLegacyWord(Bytes, record + 16, 4);
                DataSize += 32;
            }
            PutLegacyWord(Bytes, HeaderSize + DataSize, CKCID_OBJECT);
            PutLegacyWord(Bytes, HeaderSize + DataSize + 8, 4);
            std::memcpy(Bytes + HeaderSize + DataSize + 12, "raw", 4);
            DataSize += 16;
        }
        Refresh();
    }
    void Refresh() {
        Size = HeaderSize + DataSize;
        if (Version >= 5) {
            PutLegacyWord(Bytes, 32, DataSize);
            PutLegacyWord(Bytes, 36, DataSize);
        }
        PutLegacyWord(Bytes, 8, LegacyAdler(Bytes + HeaderSize, DataSize));
    }
};

class LegacyFile : public CKFile {
public:
    explicit LegacyFile(CKContext *context) : CKFile(context) {}
    CKERROR ReadData() { return ReadFileData(&m_Parser); }
};

class CKFileLegacyFormatTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        ASSERT_NE(nullptr, Context);
    }
    void TearDown() override {
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
};

class CKFileLegacyVersionsTest : public CKFileLegacyFormatTest,
    public ::testing::WithParamInterface<int> {};

TEST_P(CKFileLegacyVersionsTest, LiteralRecordRestoresSourceIdClassNameAndPayload) {
    LegacyFrame frame(GetParam());
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_NE(nullptr, file.m_Parser);
    EXPECT_EQ(frame.HeaderSize, file.m_Parser->m_CursorPos);
    EXPECT_EQ(static_cast<unsigned int>(GetParam()), file.m_FileInfo.FileVersion);
    ASSERT_EQ(CK_OK, file.ReadData());
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_EQ(73u, file.m_SaveIDMax);
    EXPECT_EQ(1u, file.m_FileInfo.ObjectCount);
    CKFileObject &object = file.m_FileObjects[0];
    EXPECT_EQ(73u, object.Object);
    EXPECT_EQ(CKCID_BEOBJECT, object.ObjectCid);
    ASSERT_NE(nullptr, object.Name);
    EXPECT_STREQ("raw", object.Name);
    ASSERT_NE(nullptr, object.Data);
    EXPECT_EQ(16, object.Data->GetDataSize());
    EXPECT_EQ(GetParam() < 4 ? 0 : 4, object.Data->GetChunkVersion());
    object.Data->StartRead();
    ASSERT_TRUE(object.Data->SeekIdentifier(CKCID_OBJECT));
    EXPECT_EQ(4, object.Data->ReadInt());
    EXPECT_EQ(0x776172, object.Data->ReadInt());
    if (GetParam() < 4) EXPECT_EQ(0x1234u, object.SaveFlags);
}

TEST_P(CKFileLegacyVersionsTest, EmptyFileUsesDataPreambleWithoutObjectHeaders) {
    LegacyFrame frame(GetParam(), false);
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadData());
    EXPECT_EQ(73u, file.m_SaveIDMax);
    EXPECT_EQ(0u, file.m_FileInfo.ObjectCount);
    EXPECT_EQ(0, file.m_FileObjects.Size());
}

TEST_P(CKFileLegacyVersionsTest, DependencyPreloadConsumesLegacyDataAndPreservesRecords) {
    LegacyFrame frame(GetParam());
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_TRUE(file.m_ReadFileDataDone);
    EXPECT_EQ(nullptr, file.m_Parser);
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_STREQ("raw", file.m_FileObjects[0].Name);
    ASSERT_EQ(2, file.m_PluginsDep.Size());
    EXPECT_EQ(CKPLUGIN_BEHAVIOR_DLL, file.m_PluginsDep[0].m_PluginCategory);
    EXPECT_EQ(CKPLUGIN_MANAGER_DLL, file.m_PluginsDep[1].m_PluginCategory);
}

INSTANTIATE_TEST_SUITE_P(FileFormatsZeroThroughSix, CKFileLegacyVersionsTest,
    ::testing::Values(0, 1, 2, 3, 4, 5, 6));

TEST_F(CKFileLegacyFormatTest, VersionsTwoThroughSixRejectPayloadCrcMismatch) {
    for (int version = 2; version <= 6; ++version) {
        SCOPED_TRACE(version);
        LegacyFrame frame(version);
        frame.Bytes[frame.Size - 2] ^= 1;
        LegacyFile file(Context);
        ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
        EXPECT_EQ(CKERR_FILECRCERROR, file.ReadData());
        EXPECT_EQ(0, file.m_FileObjects.Size());
    }
}

TEST_F(CKFileLegacyFormatTest, RawRecordRequiresNonzeroClassId) {
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 16, 0);
    frame.Refresh();
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE, file.ReadData());
}

TEST_F(CKFileLegacyFormatTest, RawRecordRejectsMissingMetadataAndPayload) {
    const int sizes[] = {16, 20, 24, 28, 32, 36, 40};
    for (int size : sizes) {
        SCOPED_TRACE(size);
        LegacyFrame frame(3);
        frame.DataSize = size;
        frame.Refresh();
        LegacyFile file(Context);
        ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
        EXPECT_EQ(CKERR_INVALIDFILE, file.ReadData());
    }
}

TEST_F(CKFileLegacyFormatTest, RawRecordRejectsNegativeStoredSize) {
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 24, 0xffffffffu);
    frame.Refresh();
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE, file.ReadData());
}

TEST_F(CKFileLegacyFormatTest, RawRecordRejectsMisalignedDecodedPayload) {
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 12, 15);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 24, 15);
    frame.DataSize = 43;
    frame.Refresh();
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE, file.ReadData());
}

TEST_F(CKFileLegacyFormatTest, FailedChunkDecompressionDoesNotReportSuccessfulDataLoad) {
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, 24, CKFILE_CHUNKCOMPRESSED_OLD);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 24, 12);
    frame.DataSize = 40;
    frame.Refresh();
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE, file.ReadData());
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_EQ(nullptr, file.m_FileObjects[0].Data);
}

TEST_F(CKFileLegacyFormatTest, LegacyChunkCompressionFlagStillAllowsUncompressedRawRecord) {
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, 24, CKFILE_CHUNKCOMPRESSED_OLD);
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadData());
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_STREQ("raw", file.m_FileObjects[0].Name);
}

TEST_F(CKFileLegacyFormatTest, RawChunkDecompressionRestoresDecodedNameAndWordCount) {
    // Standard zlib stream for the independently authored 16-byte raw payload.
    const unsigned char packed[] = {0x78,0x9c,0x63,0x64,0x80,0x00,0x16,0x20,
        0x2e,0x4a,0x2c,0x67,0x00,0x00,0x04,0x19,0x01,0x50};
    LegacyFrame frame(3);
    PutLegacyWord(frame.Bytes, 24, CKFILE_CHUNKCOMPRESSED_OLD);
    PutLegacyWord(frame.Bytes, frame.HeaderSize + 24, sizeof(packed));
    std::memcpy(frame.Bytes + frame.HeaderSize + 28, packed, sizeof(packed));
    frame.DataSize = 28 + sizeof(packed);
    frame.Refresh();
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadData());
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_STREQ("raw", file.m_FileObjects[0].Name);
    ASSERT_NE(nullptr, file.m_FileObjects[0].Data);
    EXPECT_EQ(16, file.m_FileObjects[0].Data->GetDataSize());
}

TEST_F(CKFileLegacyFormatTest, WholeCompressedVersionFiveChecksDecodedPayloadCrc) {
    const unsigned char packed[] = {0x78,0x9c,0xf3,0x64,0x80,0x00,0x00,0x02,0x50,0x00,0x4a};
    LegacyFrame frame(5, false);
    PutLegacyWord(frame.Bytes, 24, CKFILE_WHOLECOMPRESSED);
    PutLegacyWord(frame.Bytes, 32, sizeof(packed));
    std::memcpy(frame.Bytes + 64, packed, sizeof(packed));
    frame.Size = 64 + sizeof(packed);
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadData());
    EXPECT_EQ(73u, file.m_SaveIDMax);
    EXPECT_EQ(0, file.m_FileObjects.Size());
}

TEST_F(CKFileLegacyFormatTest, ExtendedHeaderCarriesProductMetadataWithoutAuthoringVersionInference) {
    LegacyFrame frame(5, false);
    PutLegacyWord(frame.Bytes, 52, 11);
    PutLegacyWord(frame.Bytes, 56, 0x02050030);
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(5u, file.m_FileInfo.FileVersion);
    EXPECT_EQ(11u, file.m_FileInfo.ProductVersion);
    EXPECT_EQ(0x02050030u, file.m_FileInfo.ProductBuild);
    ASSERT_EQ(CK_OK, file.ReadData());
}

TEST_F(CKFileLegacyFormatTest, LegacyVersion2MarkerSelectsVersionZeroHeader) {
    LegacyFrame frame(0, false);
    PutLegacyWord(frame.Bytes, 16, 9);
    PutLegacyWord(frame.Bytes, 20, 1);
    LegacyFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(0u, file.m_FileInfo.FileVersion);
    EXPECT_EQ(32, file.m_Parser->m_CursorPos);
    ASSERT_EQ(CK_OK, file.ReadData());
}

} // namespace
