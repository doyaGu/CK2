#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderNtscOriginData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviNtscOriginFile {
    std::filesystem::path Path;
    AviNtscOriginFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviNtscOriginFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviNtscOriginReader {
    CKMovieReader *Reader;
    AviNtscOriginReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviNtscOriginReader() { Reader->Release(); }
    CKERROR Open(const AviNtscOriginFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};



class AVIReaderNtscOriginTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI broadcast",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Pixels(CKMovieProperties *p, int height, int firstY, int secondY, bool jpeg=false, bool bottomFirst=false) {
        ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(height,p->m_Format.Height);
        ASSERT_EQ(16,p->m_Format.BytesPerLine); const CKBYTE *data=static_cast<const CKBYTE*>(p->m_Data);
        // Test fixtures contain two independent, literal field color sequences.
        const int rawA[]={34,66},rawB[]={50,82};
        for(int y=0;y<height;++y) for(int x=0;x<4;++x) {
            int value=0;
            for(int i=0;i<(jpeg?8:2);++i) {
                if(y==firstY+2*i) value=jpeg?(i<4?32:96):(bottomFirst?rawB[i]:rawA[i])+x;
                if(y==secondY+2*i) value=jpeg?(i<4?160:224):(bottomFirst?rawA[i]:rawB[i])+x;
            }
            const CKBYTE *pixel=data+((height-1-y)*4+x)*4;
            for(int c=0;c<3;++c) EXPECT_NEAR(value,pixel[c],jpeg?1:0)<<"topY="<<y<<" x="<<x;
            EXPECT_EQ(255,pixel[3]);
        }
    }
    void Read(AviNtscOriginData &a,int height,int firstY,int secondY,bool jpeg=false,bool bottomFirst=false) {
        AviNtscOriginFile file(a); AviNtscOriginReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
        EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={1,0,1}; for(int frame:order) Pixels(reader.Frame(frame),height,firstY,secondY,jpeg,bottomFirst);
    }
    void Raw(unsigned total,unsigned first,unsigned second,int height,int y0,int y1,bool bottomFirst=false,bool topDown=false) {
        AviNtscOriginData a; a.Make(total,first,second,height,false,topDown); Read(a,height,y0,y1,false,bottomFirst);
    }
    void Jpeg(unsigned total,unsigned first,unsigned second,int height,int y0,int y1,bool reverse=false) {
        AviNtscOriginData a; a.Make(total,first,second,height,true,false,reverse); Read(a,height,y0,y1,true);
    }
    void Full(unsigned first,unsigned second,unsigned height=480,bool topDown=false,bool odd=false) {
        AviNtscOriginData a; a.FullRaw(first,second,height,topDown,odd); AviNtscOriginFile file(a); AviNtscOriginReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={1,0,1}; for(int frame:order) {
            CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_EQ(8,p->m_Format.Width); ASSERT_EQ(int(height),p->m_Format.Height);
            const CKBYTE *data=static_cast<const CKBYTE*>(p->m_Data);
            for(unsigned y=0;y<height;++y) for(int x=0;x<8;++x) {
                const unsigned value=(y*7+x)%251+1;
                for(int c=0;c<3;++c) EXPECT_EQ(value,data[((height-1-y)*8+x)*4+c]);
                EXPECT_EQ(255,data[((height-1-y)*8+x)*4+3]);
            }
        }
    }
};
TEST_F(AVIReaderNtscOriginTest, EarlierTenLineOriginRetainsAllValidRows) { Raw(525,10,273,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, EarlierTwelveLineOriginRetainsAllValidRows) { Raw(525,12,275,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, EarlierNineteenLineOriginRetainsAllValidRows) { Raw(525,19,282,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, ModernTwentyLineOriginDoesNotMove) { Raw(525,20,283,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, LaterTwentyOneLineOriginRetainsLastRows) { Raw(525,21,284,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, LaterTwentyTwoLineOriginRetainsLastRows) { Raw(525,22,285,4,0,1); }
TEST_F(AVIReaderNtscOriginTest, ReversedEarlierDescriptorsKeepSourceParity) { Raw(525,273,10,4,1,0,true); }
TEST_F(AVIReaderNtscOriginTest, TopDownEarlierBitmapKeepsPixels) { Raw(525,10,273,4,0,1,false,true); }
TEST_F(AVIReaderNtscOriginTest, LaterShortCaptureKeepsExistingLeadingBlanking) { Raw(525,22,285,12,4,5); }
TEST_F(AVIReaderNtscOriginTest, EarlierShortCaptureRetainsDeclaredTrailingBlanking) { Raw(525,10,273,12,0,1); }
TEST_F(AVIReaderNtscOriginTest, IndependentFieldDelayIsPreserved) { Raw(525,10,275,8,0,5); }
TEST_F(AVIReaderNtscOriginTest, MinimalShiftPreservesLeadingOddFieldGap) { Raw(525,22,283,8,4,1); }
TEST_F(AVIReaderNtscOriginTest, NoEvenOriginRetainsCanonicalClipping) { Raw(525,22,283,6,4,1); }
TEST_F(AVIReaderNtscOriginTest, OversizedValidUnionRetainsCanonicalClipping) { Raw(525,19,282,2,-2,-1); }
TEST_F(AVIReaderNtscOriginTest, PalStillUsesCanonicalActiveOrigin) { Raw(625,22,335,12,-2,-1); }
TEST_F(AVIReaderNtscOriginTest, JpegEarlierOriginRetainsAllFieldBands) { Jpeg(525,10,273,16,0,1); }
TEST_F(AVIReaderNtscOriginTest, JpegLaterOriginRetainsAllFieldBands) { Jpeg(525,22,285,16,0,1); }
TEST_F(AVIReaderNtscOriginTest, ReversedAvi1EarlierFieldsRetainIdentity) { Jpeg(525,10,273,16,0,1,true); }
TEST_F(AVIReaderNtscOriginTest, BottomFirstJpegKeepsFieldPhase) { Jpeg(525,273,10,16,1,0); }
TEST_F(AVIReaderNtscOriginTest, Full480LineEarlierCaptureLosesNoBorderRows) { Full(10,273); }
TEST_F(AVIReaderNtscOriginTest, Full480LineLaterCaptureLosesNoBorderRows) { Full(22,285); }
TEST_F(AVIReaderNtscOriginTest, Full480LineModernCaptureDoesNotChange) { Full(20,283); }
TEST_F(AVIReaderNtscOriginTest, Full480LineTopDownEarlierCaptureLosesNoRows) { Full(10,273,480,true); }
TEST_F(AVIReaderNtscOriginTest, UnequalFieldHeightsFillDeclared485LineCanvas) { Full(21,284,485,false,true); }
TEST_F(AVIReaderNtscOriginTest, BitmapConsumerKeepsEarlierFirstAndLastValidRows) {
    AviNtscOriginData a; a.FullRaw(10,273); AviNtscOriginFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(8,bitmap.GetWidth()); EXPECT_EQ(480,bitmap.GetHeight()); EXPECT_EQ(0xff010101u,bitmap.GetPixel(0,0));
    EXPECT_EQ(0xff626262u,bitmap.GetPixel(7,479));
}
TEST_F(AVIReaderNtscOriginTest, EarlierOriginBlackRowsPreserveStrideCanaries) {
    AviNtscOriginData a; a.Make(525,10,273,12); AviNtscOriginFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    auto decoder=CreateFrameDecoder(*demuxer.GetStreamInfo(0)); ASSERT_NE(nullptr,decoder); uint8_t output[12*24]; std::memset(output,0xa5,sizeof(output));
    ASSERT_TRUE(decoder->Decode(a.Data+a.Index[0].Offset+8,a.Index[0].Size,4,12,output,24));
    for(int y=0;y<12;++y) { for(int i=16;i<24;++i) EXPECT_EQ(0xa5,output[y*24+i]);
        if(y<8) for(int x=0;x<4;++x) { for(int c=0;c<3;++c) EXPECT_EQ(0,output[y*24+x*4+c]); EXPECT_EQ(255,output[y*24+x*4+3]); } }
}
