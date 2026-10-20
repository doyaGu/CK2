#include <gtest/gtest.h>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderTestData.h"

CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviBoundaryFile {
    std::filesystem::path Path;
    explicit AviBoundaryFile(const AviReaderTestData &data) {
        static unsigned sequence = 0;
        char name[96]; std::snprintf(name, sizeof(name), "avi-boundary-%p-%u.avi", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
        FILE *file = std::fopen(Path.string().c_str(), "wb"); EXPECT_NE(nullptr, file);
        if (file) {
            EXPECT_EQ(data.Size, std::fwrite(data.Data, 1, data.Size, file));
            EXPECT_EQ(0, std::fclose(file));
        }
    }
    ~AviBoundaryFile() { std::error_code error; std::filesystem::remove(Path, error); }
};

struct AviBoundaryReader {
    CKMovieReader *Reader;
    AviBoundaryReader() {
        CKGet_AviReader_PluginInfo(0);
        Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0));
    }
    ~AviBoundaryReader() { Reader->Release(); }
    CKERROR Open(const AviBoundaryFile &file) {
        return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str()));
    }
};

static void MakeAviBoundaryRaw(AviReaderTestData &data, unsigned bpp = 24) {
    data.Start(bpp, 0, 2, 2);
    const uint8_t rgb24[] = {0,0,255, 0,0,255, 0,0, 0,0,255, 0,0,255, 0,0};
    const uint8_t rgb32[] = {0,0,255,0, 0,0,255,0, 0,0,255,0, 0,0,255,0};
    const uint8_t rgb16[] = {0,0x7c,0,0x7c, 0,0x7c,0,0x7c};
    const uint8_t rgb8[] = {1,1,0,0, 1,1,0,0};
    const uint8_t rgb4[] = {0x11,0,0,0, 0x11,0,0,0};
    const uint8_t rgb1[] = {0xc0,0,0,0, 0xc0,0,0,0};
    const uint8_t *pixels = bpp==32 ? rgb32 : bpp==16 ? rgb16 : bpp==8 ? rgb8 : bpp==4 ? rgb4 : bpp==1 ? rgb1 : rgb24;
    data.Frame(pixels, bpp >= 24 ? 16 : 8); data.Finish();
}

static void ExpectAviBoundarySolid(CKMovieReader *reader, unsigned width, unsigned height, CKDWORD color, int tolerance = 0) {
    CKMovieProperties *properties = nullptr;
    ASSERT_EQ(CK_OK, reader->ReadFrame(0, &properties)); ASSERT_NE(nullptr, properties);
    ASSERT_NE(nullptr, properties->m_Data);
    EXPECT_EQ(width, properties->m_Format.Width); EXPECT_EQ(height, properties->m_Format.Height);
    EXPECT_EQ(32, properties->m_Format.BitsPerPixel); EXPECT_EQ(width * 4, properties->m_Format.BytesPerLine);
    const CKDWORD *pixels = static_cast<const CKDWORD *>(properties->m_Data);
    for (unsigned i = 0; i < width * height; ++i) {
        EXPECT_EQ(255u, pixels[i] >> 24);
        for (unsigned shift = 0; shift < 24; shift += 8)
            EXPECT_NEAR(int((color >> shift) & 255), int((pixels[i] >> shift) & 255), tolerance);
    }
}

TEST(AVIReaderSupportBoundary, PluginMetadataAndFactoryKeepMovieContract) {
    EXPECT_EQ(1, CKGet_AviReader_PluginInfoCount());
    CKPluginInfo *info = CKGet_AviReader_PluginInfo(0); ASSERT_NE(nullptr, info);
    EXPECT_EQ(AVI_READER_GUID, info->m_GUID); EXPECT_EQ(1, info->m_Version);
    EXPECT_EQ(CKPLUGIN_MOVIE_READER, info->m_Type); EXPECT_STREQ("Avi", info->m_Extension);
    AviBoundaryReader reader;
    EXPECT_EQ(info, reader.Reader->GetReaderInfo()); EXPECT_EQ(CK_DATAREADER_FILELOAD, reader.Reader->GetFlags());
    EXPECT_EQ(0, reader.Reader->GetOptionsCount()); EXPECT_EQ(nullptr, reader.Reader->GetOptionDescription(0));
    EXPECT_EQ(nullptr, reader.Reader->GetOptionDescription(-1));
}

TEST(AVIReaderSupportBoundary, UnopenedReaderHasNoFrames) {
    AviBoundaryReader reader; EXPECT_EQ(0, reader.Reader->GetMovieFrameCount()); EXPECT_EQ(0, reader.Reader->GetMovieLength());
    CKMovieProperties *p = nullptr; EXPECT_EQ(CKMOVIEERROR_GENERIC, reader.Reader->ReadFrame(0, &p));
}

TEST(AVIReaderSupportBoundary, RawDepthsDecodePixelsThroughReader) {
    const unsigned depths[] = {1,4,8,16,24,32};
    for (unsigned bpp : depths) {
        SCOPED_TRACE(bpp); AviReaderTestData data; MakeAviBoundaryRaw(data, bpp);
        AviBoundaryFile file(data); AviBoundaryReader reader; ASSERT_EQ(CK_OK, reader.Open(file));
        ExpectAviBoundarySolid(reader.Reader, 2, 2, 0xffff0000u);
    }
}

TEST(AVIReaderSupportBoundary, Video1AliasesDecodeActualBlocks) {
    const uint32_t codecs[] = {0x4d415243u,0x6376736du,0x4356534du,0x4d414857u}; // CRAM, msvc, MSVC, WHAM
    for (uint32_t codec : codecs) for (unsigned bpp : {8u,16u}) {
        SCOPED_TRACE(codec);
        SCOPED_TRACE(bpp); AviReaderTestData data;
        MakeAviVideo1Sample(data,bpp,0,codec); AviBoundaryFile file(data); AviBoundaryReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); ExpectAviBoundarySolid(reader.Reader,4,4,0xffff0000u);
    }
}

TEST(AVIReaderSupportBoundary, MjpegAliasesDecodePixels) {
    for (unsigned lower = 0; lower < 2; ++lower) {
        AviReaderTestData data; MakeAviMjpegSample(data,lower!=0); AviBoundaryFile file(data); AviBoundaryReader reader;
        // Literal fixture was encoded from RGB(48,96,144), not from the production decoder.
        ASSERT_EQ(CK_OK,reader.Open(file)); ExpectAviBoundarySolid(reader.Reader,8,8,0xff306090u,2);
    }
}

TEST(AVIReaderSupportBoundary, PackedYuvLayoutsPreserveRows) {
    const uint32_t codecs[] = {0x32595559u,0x59565955u,0x55595659u}; // YUY2, UYVY, YVYU
    for (unsigned i = 0; i < 3; ++i) {
        AviReaderTestData data; MakeAviYuvSample(data,i==1);
        if (i==2) { data.Word(188,codecs[i]); } // strf.biCompression in this independent fixed header
        AviBoundaryFile file(data); AviBoundaryReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
        CKMovieProperties *p=nullptr; ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(0,&p)); ASSERT_NE(nullptr,p);
        const CKDWORD *pixels=static_cast<const CKDWORD*>(p->m_Data);
        EXPECT_EQ(0xffffffffu,pixels[0]); EXPECT_EQ(0xffffffffu,pixels[1]);
        EXPECT_EQ(0xff000000u,pixels[2]); EXPECT_EQ(0xff000000u,pixels[3]);
    }
}

TEST(AVIReaderSupportBoundary, UnknownCodecsFailAndReaderCanReopen) {
    const uint32_t codecs[] = {0x34363248u,0x64697663u,0x30323449u,0x3231564eu,0x58564944u,0x6b6e754au}; // H264,cvid,I420,NV12,DIVX,Junk
    AviReaderTestData good; MakeAviBoundaryRaw(good); AviBoundaryFile goodFile(good); AviBoundaryReader reader;
    for (uint32_t codec : codecs) {
        SCOPED_TRACE(codec); AviReaderTestData bad; bad.Start(24,codec);
        const uint8_t payload[16]={}; bad.Frame(payload,sizeof(payload)); bad.Finish(); AviBoundaryFile badFile(bad);
        EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE,reader.Open(badFile)); EXPECT_EQ(0,reader.Reader->GetMovieFrameCount());
        ASSERT_EQ(CK_OK,reader.Open(goodFile)); ExpectAviBoundarySolid(reader.Reader,2,2,0xffff0000u);
    }
}

TEST(AVIReaderSupportBoundary, UnsupportedCodecDepthsDoNotFallBackToRaw) {
    const uint32_t codecs[] = {0u,1u,2u,0x4d415243u};
    const unsigned depths[] = {2,16,8,24};
    for (unsigned i=0;i<4;++i) {
        AviReaderTestData data; data.Start(depths[i],codecs[i]);
        const uint8_t bytes[16]={}; data.Frame(bytes,sizeof(bytes)); data.Finish();
        AviBoundaryFile file(data); AviBoundaryReader reader; EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE,reader.Open(file));
    }
}

TEST(AVIReaderSupportBoundary, MemoryAndAsyncRemainUnsupportedWithoutChangingOpenMovie) {
    AviReaderTestData data; MakeAviBoundaryRaw(data); AviBoundaryFile file(data); AviBoundaryReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file));
    EXPECT_EQ(CKERR_NOTIMPLEMENTED,reader.Reader->OpenMemory(nullptr));
    EXPECT_EQ(CKERR_NOTIMPLEMENTED,reader.Reader->OpenAsynchronousFile(nullptr));
    EXPECT_EQ(1,reader.Reader->GetMovieFrameCount()); ExpectAviBoundarySolid(reader.Reader,2,2,0xffff0000u);
}

TEST(AVIReaderSupportBoundary, PackedWireHeadersHaveArchitectureIndependentLayout) {
    EXPECT_EQ(56u,sizeof(avi::MainAviHeader)); EXPECT_EQ(56u,sizeof(avi::AviStreamHeader));
    EXPECT_EQ(40u,sizeof(avi::BitmapInfoHeader)); EXPECT_EQ(18u,sizeof(avi::WaveFormatHeader));
    EXPECT_EQ(16u,sizeof(avi::AviOldIndexEntry)); EXPECT_EQ(24u,sizeof(avi::AviSuperIndexHeader));
    EXPECT_EQ(16u,sizeof(avi::AviSuperIndexEntry)); EXPECT_EQ(24u,sizeof(avi::AviStdIndexHeader));
    EXPECT_EQ(8u,sizeof(avi::AviStdIndexEntry)); EXPECT_EQ(32u,offsetof(avi::AviStreamHeader,dwLength));
    EXPECT_EQ(16u,offsetof(avi::BitmapInfoHeader,biCompression));
    EXPECT_EQ(1u,alignof(avi::AviStreamHeader)); EXPECT_EQ(1u,alignof(avi::AviStdIndexHeader));
}
