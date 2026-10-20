#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "../../Plugins/AVIReader/FrameDecoder.h"
#include "AVIReaderVideo1DimensionsData.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviVideo1DimensionsFile {
    std::filesystem::path Path;
    explicit AviVideo1DimensionsFile(const AviReaderTestData &data) {
        static unsigned sequence = 0;
        char name[96];
        std::snprintf(name, sizeof(name), "avi-video1-dimensions-%p-%u.avi", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
        FILE *file = std::fopen(Path.string().c_str(), "wb");
        EXPECT_NE(nullptr, file);
        if (file) {
            EXPECT_EQ(data.Size, std::fwrite(data.Data, 1, data.Size, file));
            EXPECT_EQ(0, std::fclose(file));
        }
    }
    ~AviVideo1DimensionsFile() { std::error_code error; std::filesystem::remove(Path, error); }
};

struct AviVideo1DimensionsReader {
    CKMovieReader *Reader;
    AviVideo1DimensionsReader() {
        CKGet_AviReader_PluginInfo(0);
        Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0));
    }
    ~AviVideo1DimensionsReader() { Reader->Release(); }
    CKERROR Open(const AviVideo1DimensionsFile &file) {
        return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str()));
    }
};

class AVIReaderVideo1DimensionsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("AVI Video1 dimensions",
            CKGet_AviReader_PluginInfoCount, CKGet_AviReader_PluginInfo, CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }

    void Check(int width, int height, unsigned mode = 0, bool skip = false,
               bool indexed = true, bool palette = false, uint32_t codec = 0x4d415243u) {
        AviReaderTestData data;
        MakeAviVideo1Dimensions(data, width, height, 8, mode, skip, indexed, palette, codec);
        AviVideo1DimensionsFile file(data);
        AviVideo1DimensionsReader reader;
        ASSERT_EQ(CK_OK, reader.Open(file));
        EXPECT_EQ(3, reader.Reader->GetMovieFrameCount());
        EXPECT_EQ(120, reader.Reader->GetMovieLength());
        const int order[] = {2, 0, 1, 2, 1, 0};
        for (int frame : order) {
            CKMovieProperties *properties = nullptr;
            ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(frame, &properties));
            ASSERT_NE(nullptr, properties);
            ASSERT_NE(nullptr, properties->m_Data);
            EXPECT_EQ(width, properties->m_Format.Width);
            EXPECT_EQ(height, properties->m_Format.Height);
            EXPECT_EQ(width * 4, properties->m_Format.BytesPerLine);
            const CKDWORD *pixels = static_cast<const CKDWORD *>(properties->m_Data);
            const CKDWORD background = palette && frame ? 0xffffff00u : 0xff204080u;
            const CKDWORD red = palette && frame ? 0xff00ff00u : 0xffff0000u;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                CKDWORD expected = background;
                if (x < width / 4 * 4 && y < height / 4 * 4) {
                    const int block = y / 4 * (width / 4) + x / 4;
                    if (frame == 2 && block == 0) expected = 0xff0000ffu;
                    else if (!skip) {
                        const char *pattern = mode == 1 ? "brbrbrbrrbrbrbrb" : "brrbbrrbrbbrrbrb";
                        const bool blue = mode == 0 ? block % 2 != 0 : pattern[y % 4 * 4 + x % 4] == 'b';
                        expected = blue ? 0xff0000ffu : red;
                    }
                }
                EXPECT_EQ(expected, pixels[y * width + x]) << "frame " << frame << " x " << x << " y " << y;
            }
        }
    }
};

TEST_F(AVIReaderVideo1DimensionsTest, AlignedEightBitControl) { Check(8, 8); }
TEST_F(AVIReaderVideo1DimensionsTest, RightRemainderUsesPaletteZero) { Check(5, 4); }
TEST_F(AVIReaderVideo1DimensionsTest, TopRemainderUsesPaletteZero) { Check(4, 5); }
TEST_F(AVIReaderVideo1DimensionsTest, BothRemaindersAndDeltaReplay) { Check(6, 6); }
TEST_F(AVIReaderVideo1DimensionsTest, MultipleBlockRowsKeepBottomUpOrder) { Check(9, 9); }
TEST_F(AVIReaderVideo1DimensionsTest, ThreePixelRemainders) { Check(11, 11); }
TEST_F(AVIReaderVideo1DimensionsTest, SmallerThanOneBlockIsPaletteZero) { Check(1, 1); }
TEST_F(AVIReaderVideo1DimensionsTest, NoCompleteBlockColumn) { Check(3, 8); }
TEST_F(AVIReaderVideo1DimensionsTest, NoCompleteBlockRow) { Check(8, 3); }
TEST_F(AVIReaderVideo1DimensionsTest, OddTwoColorBlocks) { Check(9, 5, 1); }
TEST_F(AVIReaderVideo1DimensionsTest, OddEightColorBlocks) { Check(5, 9, 2); }
TEST_F(AVIReaderVideo1DimensionsTest, InitialSkippedBlocksUsePaletteZero) { Check(9, 9, 0, true); }
TEST_F(AVIReaderVideo1DimensionsTest, AlignedInitialSkippedBlocksUsePaletteZero) { Check(8, 8, 0, true); }
TEST_F(AVIReaderVideo1DimensionsTest, IndexlessDeltaReplay) { Check(9, 9, 0, false, false); }
TEST_F(AVIReaderVideo1DimensionsTest, PaletteChangeRecolorsRemaindersAndSkippedBlocks) { Check(9, 5, 0, false, true, true); }
TEST_F(AVIReaderVideo1DimensionsTest, UppercaseMsvcUsesIndexedDimensions) { Check(5, 5, 0, false, true, false, 0x4356534du); }

TEST_F(AVIReaderVideo1DimensionsTest, SixteenBitAlignedControl) {
    AviReaderTestData data; MakeAviVideo1Dimensions(data, 8, 8, 16);
    AviVideo1DimensionsFile file(data); AviVideo1DimensionsReader reader;
    ASSERT_EQ(CK_OK, reader.Open(file));
    CKMovieProperties *p = nullptr;
    ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(0, &p)); ASSERT_NE(nullptr, p);
    const CKDWORD *pixels = static_cast<const CKDWORD *>(p->m_Data);
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
        EXPECT_EQ(x < 4 ? 0xffff0000u : 0xff0000ffu, pixels[y * 8 + x]);
}

TEST_F(AVIReaderVideo1DimensionsTest, SixteenBitRemaindersRemainUnsupported) {
    const int dimensions[][2] = {{5, 4}, {4, 5}, {6, 6}, {1, 1}};
    for (const auto &size : dimensions) {
        AviReaderTestData data; MakeAviVideo1Dimensions(data, size[0], size[1], 16);
        AviVideo1DimensionsFile file(data); AviVideo1DimensionsReader reader;
        EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE, reader.Open(file));
    }
}

TEST_F(AVIReaderVideo1DimensionsTest, DecoderKeepsPaddedStrideAndBufferGuards) {
    avi::AviStreamInfo info; info.width = info.height = 5; info.bitsPerPixel = 8;
    info.palette.push_back(0xff204080u); info.palette.push_back(0xffff0000u);
    Msvideo1FrameDecoder decoder(info);
    alignas(4) uint8_t output[8 + 24 * 5 + 8];
    std::memset(output, 0xcd, sizeof(output));
    const uint8_t block[] = {1, 0x80};
    ASSERT_TRUE(decoder.Decode(block, sizeof(block), 5, 5, output + 8, 24));
    for (int i = 0; i < 8; ++i) { EXPECT_EQ(0xcd, output[i]); EXPECT_EQ(0xcd, output[128 + i]); }
    for (int y = 0; y < 5; ++y) for (int x = 0; x < 5; ++x) {
        CKDWORD pixel; std::memcpy(&pixel, output + 8 + y * 24 + x * 4, 4);
        EXPECT_EQ(x < 4 && y < 4 ? 0xffff0000u : 0xff204080u, pixel);
    }
}

TEST_F(AVIReaderVideo1DimensionsTest, TruncatedOddDeltaFailsAndEarlierFrameCanReplay) {
    AviReaderTestData data; MakeAviVideo1Dimensions(data, 9, 9);
    // A two-color command without its two palette indices, at frame 1.
    const unsigned at = data.Index[1].Offset + 8;
    data.Data[at] = data.Data[at + 1] = 0;
    AviVideo1DimensionsFile file(data); AviVideo1DimensionsReader reader;
    ASSERT_EQ(CK_OK, reader.Open(file));
    CKMovieProperties *p = nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR, reader.Reader->ReadFrame(1, &p)); EXPECT_EQ(nullptr, p);
    ASSERT_EQ(CK_OK, reader.Reader->ReadFrame(0, &p)); ASSERT_NE(nullptr, p);
    EXPECT_EQ(0xffff0000u, static_cast<CKDWORD *>(p->m_Data)[0]);
    EXPECT_EQ(0xff204080u, static_cast<CKDWORD *>(p->m_Data)[80]);
}

TEST_F(AVIReaderVideo1DimensionsTest, BitmapConsumerOwnsOddSurfaceAndUsesLogicalTopRow) {
    AviReaderTestData data; MakeAviVideo1Dimensions(data, 5, 5);
    AviVideo1DimensionsFile file(data); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(0xff204080u, bitmap.GetPixel(0, 0));
    EXPECT_EQ(0xff204080u, bitmap.GetPixel(4, 4));
    EXPECT_EQ(0xffff0000u, bitmap.GetPixel(0, 4));
    ASSERT_TRUE(bitmap.SetCurrentSlot(2)); EXPECT_EQ(0xff0000ffu, bitmap.GetPixel(0, 4));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xffff0000u, bitmap.GetPixel(0, 4));
}
