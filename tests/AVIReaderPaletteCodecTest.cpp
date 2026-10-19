#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderTestData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviPaletteFile {
    std::filesystem::path Path;
    AviPaletteFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-palette-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb");
        EXPECT_NE(nullptr,f); if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviPaletteFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviPaletteReader {
    CKMovieReader *Reader;
    AviPaletteReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviPaletteReader() { Reader->Release(); }
    CKERROR Open(const AviPaletteFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) {
        CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p;
    }
};
class AVIReaderPaletteCodecTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI palette codec",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Palette(unsigned bpp, unsigned codec, bool changes=true, bool rec=false, bool indexed=true) {
        AviReaderTestData a; MakeAviPaletteSample(a,bpp,codec,changes,rec,indexed); AviPaletteFile file(a); AviPaletteReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(4,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(160,reader.Reader->GetMovieLength());
        const int order[]={3,0,2,1,2,3,1,0};
        for(int f:order) {
            CKMovieProperties *p=reader.Frame(f); ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data);
            const CKDWORD one=changes&&(f==1||f==2)?0xff00ff00u:0xffff0000u;
            const CKDWORD other=changes&&f==2?0xffffff00u:bpp==1?0xff000000u:0xff0000ffu;
            const CKDWORD expected[4]={one,codec?one:other,codec?other:other,codec?other:one};
            EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected))) << "frame " << f;
        }
    }
    void Video1(unsigned bpp, unsigned mode, uint32_t codec=0x4d415243u) {
        AviReaderTestData a; MakeAviVideo1Sample(a,bpp,mode,codec); AviPaletteFile file(a); AviPaletteReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(3,reader.Reader->GetMovieFrameCount());
        const int order[]={2,0,1,2,0};
        for(int f:order) {
            CKMovieProperties *p=reader.Frame(f); ASSERT_NE(nullptr,p); const CKDWORD *pixels=static_cast<CKDWORD*>(p->m_Data); ASSERT_NE(nullptr,pixels);
            const CKDWORD red=bpp==8&&f>0?0xff00ff00u:0xffff0000u,blue=0xff0000ffu;
            // Literal rows encode two-color flags and the four distinct 2x2 quadrant pairs.
            const char *pattern=mode==0?"rrrrrrrrrrrrrrrr":mode==1?"brbrbrbrrbrbrbrb":bpp==8?"brrbbrrbrbbrrbrb":"brrbbrrbrbbrrbbr";
            for(int i=0;i<16;++i) EXPECT_EQ(pattern[i]=='r'?red:blue,pixels[i]) << "frame " << f << " pixel " << i;
        }
    }
    void Yuv(bool uyvy, bool negative) {
        AviReaderTestData a; MakeAviYuvSample(a,uyvy,negative); AviPaletteFile file(a); AviPaletteReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
        const CKDWORD expected[]={0xffffffffu,0xffffffffu,0xff000000u,0xff000000u};
        EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)));
    }
    void Mjpeg(bool lower, bool omitHuffman) {
        AviReaderTestData a; MakeAviMjpegSample(a,lower,omitHuffman); AviPaletteFile file(a); AviPaletteReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
        const CKBYTE *pixels=static_cast<CKBYTE*>(p->m_Data);
        for(int i=0;i<64;++i) { EXPECT_NEAR(144,pixels[i*4],2); EXPECT_NEAR(96,pixels[i*4+1],2); EXPECT_NEAR(48,pixels[i*4+2],2); EXPECT_EQ(255,pixels[i*4+3]); }
    }
    void RleBackground(unsigned codec) {
        AviReaderTestData a; a.Start(codec==1?8:4,codec); const uint8_t orange[]={128,64,32},eob[]={0,1};
        a.Palette(0,1,orange); a.Frame(eob,2); a.Finish(); AviPaletteFile file(a); AviPaletteReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
        const CKDWORD expected[]={0xff804020u,0xff804020u,0xff804020u,0xff804020u}; EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)));
    }
    void Video1ZeroFlags(unsigned bpp) {
        AviReaderTestData a; MakeAviVideo1Sample(a,bpp,1); a.Word(a.Index[0].Offset+8,0,2);
        AviPaletteFile file(a); AviPaletteReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
        CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
        for(int i=0;i<16;++i) EXPECT_EQ(0xff0000ffu,static_cast<CKDWORD*>(p->m_Data)[i]);
    }
};
TEST_F(AVIReaderPaletteCodecTest, Raw8StaticPaletteControl) { Palette(8,0,false); }
TEST_F(AVIReaderPaletteCodecTest, Raw1PaletteChangesAndDroppedFrame) { Palette(1,0); }
TEST_F(AVIReaderPaletteCodecTest, Raw4PaletteChangesAndBackwardSeek) { Palette(4,0); }
TEST_F(AVIReaderPaletteCodecTest, Raw8PaletteChangesAndRestore) { Palette(8,0); }
TEST_F(AVIReaderPaletteCodecTest, Raw8IndexlessPaletteEvents) { Palette(8,0,true,false,false); }
TEST_F(AVIReaderPaletteCodecTest, Raw4RecordPaletteEvents) { Palette(4,0,true,true); }
TEST_F(AVIReaderPaletteCodecTest, Rle8StaticDeltaControl) { Palette(8,1,false); }
TEST_F(AVIReaderPaletteCodecTest, Rle4StaticDeltaControl) { Palette(4,2,false); }
TEST_F(AVIReaderPaletteCodecTest, Rle8PaletteChangesRecolorUnwrittenHistory) { Palette(8,1); }
TEST_F(AVIReaderPaletteCodecTest, Rle4PaletteChangesRecolorUnwrittenHistory) { Palette(4,2); }
TEST_F(AVIReaderPaletteCodecTest, Rle8IndexlessRecordPaletteAndBackwardReplay) { Palette(8,1,true,true,false); }
TEST_F(AVIReaderPaletteCodecTest, Video1EightBitSolidAndPaletteReplay) { Video1(8,0); }
TEST_F(AVIReaderPaletteCodecTest, Video1EightBitTwoColorBlocks) { Video1(8,1); }
TEST_F(AVIReaderPaletteCodecTest, Video1EightBitEightColorBlocks) { Video1(8,2); }
TEST_F(AVIReaderPaletteCodecTest, Video1SixteenBitSolidControl) { Video1(16,0); }
TEST_F(AVIReaderPaletteCodecTest, Video1SixteenBitTwoColorControl) { Video1(16,1); }
TEST_F(AVIReaderPaletteCodecTest, Video1SixteenBitEightColorControl) { Video1(16,2); }
TEST_F(AVIReaderPaletteCodecTest, Video1EightBitZeroFlagsAreARealTwoColorBlock) { Video1ZeroFlags(8); }
TEST_F(AVIReaderPaletteCodecTest, Video1SixteenBitZeroFlagsAreARealTwoColorBlock) { Video1ZeroFlags(16); }
TEST_F(AVIReaderPaletteCodecTest, Video1UppercaseMsvcHandler) { Video1(16,0,0x4356534du); }
TEST_F(AVIReaderPaletteCodecTest, Video1LowercaseMsvcHandler) { Video1(16,0,0x6376736du); }
TEST_F(AVIReaderPaletteCodecTest, RawDibHandlerDoesNotOverrideUncompressedBitmapFormat) {
    AviReaderTestData a; a.Start(24,0,2,2,false,0x20424944); const uint8_t raw[16]={1,2,3,4,5,6,0,0,7,8,9,10,11,12,0,0}; a.Frame(raw,16); a.Finish();
    AviPaletteFile file(a); AviPaletteReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
    const CKDWORD expected[]={0xff030201u,0xff060504u,0xff090807u,0xff0c0b0au}; EXPECT_EQ(0,std::memcmp(expected,p->m_Data,sizeof(expected)));
}
TEST_F(AVIReaderPaletteCodecTest, BitmapConsumerSeesRecoloredDroppedSlotAndOwnsItsSurface) {
    AviReaderTestData a; MakeAviPaletteSample(a,8,1); AviPaletteFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); ASSERT_TRUE(bitmap.SetCurrentSlot(2));
    EXPECT_EQ(0xffffff00u,bitmap.GetPixel(0,0)); EXPECT_EQ(0xff00ff00u,bitmap.GetPixel(0,1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0)); EXPECT_EQ(0xffff0000u,bitmap.GetPixel(0,1));
}
TEST_F(AVIReaderPaletteCodecTest, Yuy2PositiveHeightIsTopDownVideo) { Yuv(false,false); }
TEST_F(AVIReaderPaletteCodecTest, Yuy2NegativeHeightIsTopDownVideo) { Yuv(false,true); }
TEST_F(AVIReaderPaletteCodecTest, UyvyPositiveHeightIsTopDownVideo) { Yuv(true,false); }
TEST_F(AVIReaderPaletteCodecTest, UyvyNegativeHeightIsTopDownVideo) { Yuv(true,true); }
TEST_F(AVIReaderPaletteCodecTest, MjpegUppercaseDecodesActualJpegPixels) { Mjpeg(false,false); }
TEST_F(AVIReaderPaletteCodecTest, MjpegLowercaseDecodesActualJpegPixels) { Mjpeg(true,false); }
TEST_F(AVIReaderPaletteCodecTest, Rle8InitialUnwrittenPixelsUsePaletteZero) { RleBackground(1); }
TEST_F(AVIReaderPaletteCodecTest, Rle4InitialUnwrittenPixelsUsePaletteZero) { RleBackground(2); }
TEST_F(AVIReaderPaletteCodecTest, PaletteCountZeroUpdatesAll256EntriesInRgbOrder) {
    AviReaderTestData a; a.Start(8,0); const uint8_t frame[]={1,255,0,0,255,1,0,0}; a.Frame(frame,8);
    uint8_t rgb[768]={}; rgb[3]=128; rgb[4]=64; rgb[5]=32; rgb[765]=7; rgb[766]=19; rgb[767]=233;
    a.Palette(0,256,rgb); a.Frame(frame,8); a.Finish(); AviPaletteFile file(a); AviPaletteReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(1); ASSERT_NE(nullptr,p);
    const CKDWORD expected[]={0xff804020u,0xff0713e9u,0xff0713e9u,0xff804020u};
    EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected))); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount());
}
TEST_F(AVIReaderPaletteCodecTest, DuplicatePaletteColorsRetainSeparateIndicesAcrossDeltaReplay) {
    AviReaderTestData a; a.Start(8,1); const uint8_t same[]={255,0,0,255,0,0}; a.Palette(1,2,same);
    const uint8_t frame[]={2,1,0,0,2,2,0,1},eob[]={0,1},blue[]={0,0,255};
    a.Frame(frame,8); a.Palette(2,1,blue); a.Frame(eob,2); a.Finish(); AviPaletteFile file(a); AviPaletteReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *initial=reader.Frame(0); ASSERT_NE(nullptr,initial);
    const CKDWORD initialExpected[]={0xffff0000u,0xffff0000u,0xffff0000u,0xffff0000u};
    EXPECT_EQ(0,std::memcmp(initial->m_Data,initialExpected,sizeof(initialExpected)));
    CKMovieProperties *p=reader.Frame(1); ASSERT_NE(nullptr,p);
    const CKDWORD expected[]={0xffff0000u,0xffff0000u,0xff0000ffu,0xff0000ffu}; EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)));
}
TEST_F(AVIReaderPaletteCodecTest, OtherStreamPaletteEventsDoNotChangeTheVideo) {
    AviReaderTestData a; a.Start(8,0); const uint8_t frame[]={1,1,0,0,1,1,0,0},green[]={0,255,0};
    a.Frame(frame,8); a.Palette(1,1,green,"01pc"); a.Frame(frame,8); a.Finish(); AviPaletteFile file(a); AviPaletteReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(1); ASSERT_NE(nullptr,p); EXPECT_EQ(0xffff0000u,static_cast<CKDWORD*>(p->m_Data)[0]);
}
TEST_F(AVIReaderPaletteCodecTest, TruncatedPaletteChangeDoesNotOpenAsAValidMovie) {
    AviReaderTestData a; a.Start(8,0); const uint8_t frame[]={1,1,0,0,1,1,0,0},shortChange[]={1,2,0,0,255,0,0,0};
    a.Frame(frame,8); a.Add("00pc",shortChange,8,0x100); a.Frame(frame,8); a.Finish(); AviPaletteFile file(a); AviPaletteReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderPaletteCodecTest, ReopeningClearsThePreviousMoviesPaletteEvents) {
    AviReaderTestData first,second; MakeAviPaletteSample(first,8,0); MakeAviPaletteSample(second,8,0,false);
    AviPaletteFile f(first),g(second); AviPaletteReader reader; ASSERT_EQ(CK_OK,reader.Open(f));
    CKMovieProperties *p=reader.Frame(1); ASSERT_NE(nullptr,p); EXPECT_EQ(0xff00ff00u,static_cast<CKDWORD*>(p->m_Data)[0]);
    ASSERT_EQ(CK_OK,reader.Open(g)); p=reader.Frame(1); ASSERT_NE(nullptr,p); EXPECT_EQ(0xffff0000u,static_cast<CKDWORD*>(p->m_Data)[0]);
}

