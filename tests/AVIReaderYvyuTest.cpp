#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "../../Plugins/AVIReader/FrameDecoder.h"
#include "AVIReaderYvyuData.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviYvyuFile {
    std::filesystem::path Path;
    explicit AviYvyuFile(const AviReaderTestData &data) {
        static unsigned sequence = 0; char name[96];
        std::snprintf(name, sizeof(name), "avi-yvyu-%p-%u.avi", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
        FILE *file = std::fopen(Path.string().c_str(), "wb"); EXPECT_NE(nullptr, file);
        if (file) {
            EXPECT_EQ(data.Size, std::fwrite(data.Data, 1, data.Size, file));
            EXPECT_EQ(0, std::fclose(file));
        }
    }
    ~AviYvyuFile() { std::error_code error; std::filesystem::remove(Path, error); }
};
struct AviYvyuReader {
    CKMovieReader *Reader;
    AviYvyuReader() { CKGet_AviReader_PluginInfo(0); Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0)); }
    ~AviYvyuReader() { Reader->Release(); }
    CKERROR Open(const AviYvyuFile &file) { return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str())); }
};

class AVIReaderYvyuTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("AVI YVYU",
            CKGet_AviReader_PluginInfoCount, CKGet_AviReader_PluginInfo, CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Check(int width = 4, int signedHeight = 2, uint32_t codec = 0x55595659u,
               bool drop = false, bool indexed = true, bool records = false,
               uint32_t handler = 0x55595659u) {
        AviReaderTestData data; MakeAviYvyu(data, width, signedHeight, codec, drop, indexed, records, handler);
        AviYvyuFile file(data); AviYvyuReader reader;
        ASSERT_EQ(CK_OK, reader.Open(file));
        EXPECT_EQ(3, reader.Reader->GetMovieFrameCount()); EXPECT_EQ(120, reader.Reader->GetMovieLength());
        const int order[] = {2, 0, 1, 2, 1, 0};
        const int height = signedHeight < 0 ? -signedHeight : signedHeight;
        for (int frame : order) {
            CKMovieProperties *p = nullptr;
            ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(frame, &p)); ASSERT_NE(nullptr, p); ASSERT_NE(nullptr, p->m_Data);
            EXPECT_EQ(width, p->m_Format.Width); EXPECT_EQ(height, p->m_Format.Height);
            EXPECT_EQ(width * 4, p->m_Format.BytesPerLine); EXPECT_EQ(32, p->m_Format.BitsPerPixel);
            const CKDWORD *pixels = static_cast<const CKDWORD *>(p->m_Data);
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                const char *pattern = (height - 1 - y + (frame == 2)) % 2 ? "wkbb" : "kwrr";
                const char color = pattern[x % 4];
                const CKDWORD expected = color == 'k' ? 0xff000000u : color == 'w' ? 0xffffffffu : color == 'r' ? 0xffff0000u : 0xff0000ffu;
                EXPECT_EQ(expected, pixels[y * width + x]) << "frame " << frame << " x " << x << " y " << y;
            }
        }
    }
};

TEST_F(AVIReaderYvyuTest, Yuy2Control) { Check(4, 2, 0x32595559u); }
TEST_F(AVIReaderYvyuTest, UyvyControl) { Check(4, 2, 0x59565955u); }
TEST_F(AVIReaderYvyuTest, YvyuHasDistinctRedBlueAndTopDownRows) { Check(); }
TEST_F(AVIReaderYvyuTest, TwoPixelWidth) { Check(2, 2); }
TEST_F(AVIReaderYvyuTest, SixPixelWidthHasNoExtraSourceRowPadding) { Check(6, 2); }
TEST_F(AVIReaderYvyuTest, OddHeightKeepsLastRow) { Check(4, 3); }
TEST_F(AVIReaderYvyuTest, SingleRow) { Check(4, 1); }
TEST_F(AVIReaderYvyuTest, NegativeBitmapHeightStillUsesYuvTopDownStorage) { Check(4, -2); }
TEST_F(AVIReaderYvyuTest, BitmapCompressionChoosesLayoutWithDibHandler) { Check(4, 2, 0x55595659u, false, true, false, 0x20424944u); }
TEST_F(AVIReaderYvyuTest, IndexlessVideo) { Check(4, 2, 0x55595659u, false, false); }
TEST_F(AVIReaderYvyuTest, RecordChunks) { Check(4, 2, 0x55595659u, false, true, true); }
TEST_F(AVIReaderYvyuTest, DroppedFrameRetainsTimelineAndEarlierImage) { Check(4, 2, 0x55595659u, true); }

TEST_F(AVIReaderYvyuTest, ReopenReplacesDimensionsAndOldImage) {
    AviReaderTestData first, second; MakeAviYvyu(first); MakeAviYvyu(second, 2, 3);
    AviYvyuFile a(first), b(second); AviYvyuReader reader;
    ASSERT_EQ(CK_OK, reader.Open(a)); ASSERT_EQ(CK_OK, reader.Open(b));
    CKMovieProperties *p = nullptr;
    ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(2, &p)); ASSERT_NE(nullptr, p);
    EXPECT_EQ(2, p->m_Format.Width); EXPECT_EQ(3, p->m_Format.Height);
    EXPECT_EQ(0xffffffffu, static_cast<CKDWORD *>(p->m_Data)[0]);
}
TEST_F(AVIReaderYvyuTest, TruncatedLaterPayloadFailsAndEarlierFrameRemainsReadable) {
    AviReaderTestData data; MakeAviYvyu(data); data.Index[1].Size -= 1;
    // A 15-byte payload plus one RIFF padding byte keeps the next boundary.
    data.Word(data.Index[1].Offset + 4, data.Index[1].Size);
    const unsigned idx = data.Size - 8 - data.Entries * 16;
    data.Word(idx + 8 + 16 + 12, 15);
    AviYvyuFile file(data); AviYvyuReader reader; ASSERT_EQ(CK_OK, reader.Open(file));
    CKMovieProperties *p = nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR, reader.Reader->ReadFrame(1, &p)); EXPECT_EQ(nullptr, p);
    ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(0, &p)); ASSERT_NE(nullptr, p);
    EXPECT_EQ(0xffffffffu, static_cast<CKDWORD *>(p->m_Data)[0]);
}
TEST_F(AVIReaderYvyuTest, TruncatedFirstPayloadCannotOpen) {
    AviReaderTestData data; MakeAviYvyu(data);
    data.Word(data.Index[0].Offset + 4, 15);
    const unsigned idx = data.Size - 8 - data.Entries * 16; data.Word(idx + 8 + 12, 15);
    AviYvyuFile file(data); AviYvyuReader reader;
    EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE, reader.Open(file));
}
TEST_F(AVIReaderYvyuTest, PaddedOutputStrideKeepsPaddingAndGuards) {
    avi::AviStreamInfo info; info.width = 4; info.height = 2; info.bitsPerPixel = 16;
    info.codec = 0x55595659u; info.videoTopDown = true;
    auto decoder = CreateFrameDecoder(info); ASSERT_NE(nullptr, decoder);
    const uint8_t bytes[] = {16,128,235,128,81,240,81,90,235,128,16,128,41,110,41,240};
    uint8_t output[8 + 40 + 8]; std::memset(output, 0xcd, sizeof(output));
    ASSERT_TRUE(decoder->Decode(bytes, sizeof(bytes), 4, 2, output + 8, 20));
    const CKDWORD expected[] = {0xffffffffu,0xff000000u,0xff0000ffu,0xff0000ffu,0xff000000u,0xffffffffu,0xffff0000u,0xffff0000u};
    for (int y = 0; y < 2; ++y) {
        EXPECT_EQ(0, std::memcmp(output + 8 + y * 20, expected + y * 4, 16));
        for (int i = 0; i < 4; ++i) EXPECT_EQ(0xcd, output[8 + y * 20 + 16 + i]);
    }
    for (int i = 0; i < 8; ++i) { EXPECT_EQ(0xcd, output[i]); EXPECT_EQ(0xcd, output[48 + i]); }
    std::memset(output, 0xcd, sizeof(output));
    EXPECT_FALSE(decoder->Decode(bytes, sizeof(bytes) - 1, 4, 2, output + 8, 20));
    for (uint8_t value : output) EXPECT_EQ(0xcd, value);
}
TEST_F(AVIReaderYvyuTest, UnconfirmedYuyvAliasRemainsUnsupported) {
    AviReaderTestData data; MakeAviYvyu(data, 4, 2, 0x56595559u);
    AviYvyuFile file(data); AviYvyuReader reader;
    EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE, reader.Open(file));
}
TEST_F(AVIReaderYvyuTest, BitmapConsumerSeesLogicalRowsAndCanReturnToEarlierSlot) {
    AviReaderTestData data; MakeAviYvyu(data); AviYvyuFile file(data); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0)); EXPECT_EQ(0xffff0000u, bitmap.GetPixel(2, 0));
    EXPECT_EQ(0xff0000ffu, bitmap.GetPixel(2, 1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(2)); EXPECT_EQ(0xff0000ffu, bitmap.GetPixel(2, 0));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xffff0000u, bitmap.GetPixel(2, 0));
}
