#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderBroadcastData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviBroadcastFile {
    std::filesystem::path Path;
    AviBroadcastFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviBroadcastFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviBroadcastReader {
    CKMovieReader *Reader;
    AviBroadcastReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviBroadcastReader() { Reader->Release(); }
    CKERROR Open(const AviBroadcastFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};



class AVIReaderBroadcastTest : public ::testing::Test {
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
    void Read(AviBroadcastData &a,int height,int firstY,int secondY,bool jpeg=false,bool bottomFirst=false) {
        AviBroadcastFile file(a); AviBroadcastReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
        EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={1,0,1}; for(int frame:order) Pixels(reader.Frame(frame),height,firstY,secondY,jpeg,bottomFirst);
    }
    void Raw(unsigned total,unsigned first,unsigned second,int height,int y0,int y1,bool bottomFirst=false,bool topDown=false) {
        AviBroadcastData a; a.Make(total,first,second,height,false,topDown); Read(a,height,y0,y1,false,bottomFirst);
    }
    void Jpeg(unsigned total,unsigned first,unsigned second,int height,int y0,int y1,bool reverse=false) {
        AviBroadcastData a; a.Make(total,first,second,height,true,false,reverse); Read(a,height,y0,y1,true);
    }
};
TEST_F(AVIReaderBroadcastTest, NtscHalfLinePlacesFieldsOnAdjacentRows) { Raw(525,20,283,12,0,1); }
TEST_F(AVIReaderBroadcastTest, PalHalfLinePlacesFieldsOnAdjacentRows) { Raw(625,23,336,12,0,1); }
TEST_F(AVIReaderBroadcastTest, NtscLateCaptureKeepsLeadingAndTrailingBlackRows) { Raw(525,22,285,12,4,5); }
TEST_F(AVIReaderBroadcastTest, PalLateCaptureKeepsLeadingAndTrailingBlackRows) { Raw(625,25,338,12,4,5); }
TEST_F(AVIReaderBroadcastTest, IndependentFieldDelaysLeaveInterleavedBlackRows) { Raw(525,20,285,12,0,5); }
TEST_F(AVIReaderBroadcastTest, EarlyNtscFieldRowsAreClippedAtActiveOrigin) { Raw(525,19,282,12,-2,-1); }
TEST_F(AVIReaderBroadcastTest, EarlyPalFieldRowsAreClippedAtActiveOrigin) { Raw(625,22,335,12,-2,-1); }
TEST_F(AVIReaderBroadcastTest, ReversedNtscDescriptorsRetainSourceFieldIdentity) { Raw(525,283,20,12,1,0,true); }
TEST_F(AVIReaderBroadcastTest, ReversedPalDescriptorsRetainSourceFieldIdentity) { Raw(625,336,23,12,1,0,true); }
TEST_F(AVIReaderBroadcastTest, NtscFieldTwoWrapsThroughLinesOneToThree) { Raw(525,2,20,492,489,0,true); }
TEST_F(AVIReaderBroadcastTest, NtscDeclared485LineCanvasIsNotReducedToValidUnion) { Raw(525,20,283,485,0,1); }
TEST_F(AVIReaderBroadcastTest, PalDeclared576LineCanvasIsNotReducedToValidUnion) { Raw(625,23,336,576,0,1); }
TEST_F(AVIReaderBroadcastTest, TopDownRawRetainsBroadcastPlacement) { Raw(525,22,285,12,4,5,false,true); }
TEST_F(AVIReaderBroadcastTest, JpegFieldsUseBroadcastCanvas) { Jpeg(525,20,283,24,0,1); }
TEST_F(AVIReaderBroadcastTest, JpegFieldsKeepDelayedBlankRows) { Jpeg(625,25,338,24,4,5); }
TEST_F(AVIReaderBroadcastTest, JpegNegativeOffsetClipsBeforeSampling) { Jpeg(525,19,282,12,-2,-1); }
TEST_F(AVIReaderBroadcastTest, ReversedAvi1StorageRetainsBroadcastPlacement) { Jpeg(525,22,285,24,4,5,true); }
TEST_F(AVIReaderBroadcastTest, BottomFirstJpegDescriptorsRetainIdentity) { Jpeg(525,285,22,24,5,4); }
TEST_F(AVIReaderBroadcastTest, BitmapConsumerSeesDeclaredCanvasAndBlackMargins) {
    AviBroadcastData a; a.Make(525,22,285,12); AviBroadcastFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(4,bitmap.GetWidth()); EXPECT_EQ(12,bitmap.GetHeight()); EXPECT_EQ(0xff000000u,bitmap.GetPixel(0,0));
    EXPECT_EQ(0xff222222u,bitmap.GetPixel(0,4)); EXPECT_EQ(0xff555555u,bitmap.GetPixel(3,7)); EXPECT_EQ(0xff000000u,bitmap.GetPixel(0,11));
}
TEST_F(AVIReaderBroadcastTest, DecoderPreservesStridePaddingAcrossBlankRows) {
    AviBroadcastData a; a.Make(525,22,285,12); AviBroadcastFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    auto decoder=CreateFrameDecoder(*demuxer.GetStreamInfo(0)); ASSERT_NE(nullptr,decoder); uint8_t output[12*24]; std::memset(output,0xa5,sizeof(output));
    ASSERT_TRUE(decoder->Decode(a.Data+a.Index[0].Offset+8,a.Index[0].Size,4,12,output,24));
    for(int y=0;y<12;++y) { for(int i=16;i<24;++i) EXPECT_EQ(0xa5,output[y*24+i]);
        if(y<4||y>=8) for(int x=0;x<4;++x) { for(int c=0;c<3;++c) EXPECT_EQ(0,output[y*24+x*4+c]); EXPECT_EQ(255,output[y*24+x*4+3]); } }
}
TEST_F(AVIReaderBroadcastTest, ZeroBasedFieldNumbersKeepLegacyUnion) { Raw(525,0,1,4,0,1); }
TEST_F(AVIReaderBroadcastTest, UnknownSignalTotalKeepsLegacyUnion) { Raw(600,20,283,4,0,1); }
TEST_F(AVIReaderBroadcastTest, SameSignalFieldDoesNotInventBroadcastOrigin) { Raw(525,20,21,4,0,1); }
TEST_F(AVIReaderBroadcastTest, MissingCanvasHeightKeepsLegacyUnion) {
    AviBroadcastData a; a.Make(525,20,283,0); Read(a,4,0,1);
}
TEST_F(AVIReaderBroadcastTest, OversizedDeclaredBroadcastCanvasIsRejected) {
    AviBroadcastData a; a.Make(525,20,283,0xffffffffu); AviBroadcastFile file(a); AviBroadcastReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderBroadcastTest, RleDeltaHistorySurvivesPlacementAndDroppedFrames) {
    AviBroadcastData a; a.Start(8,1,8,8); a.Properties(2,8,4,4,2,2,1); a.Timing(525,22,285,12);
    uint8_t full[34]; for(int i=0;i<8;++i) { full[4*i]=8; full[4*i+1]=1; full[4*i+2]=0; full[4*i+3]=0; } full[32]=0; full[33]=1;
    const uint8_t delta[]={0,2,4,5,1,2,0,1}; a.Frame(full,sizeof(full)); a.Frame(delta,sizeof(delta)); a.Frame(nullptr,0); a.Finish();
    AviBroadcastFile file(a); AviBroadcastReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); const int order[]={1,0,2,1};
    for(int frame:order) { CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(12,p->m_Format.Height);
        const CKDWORD *data=static_cast<const CKDWORD*>(p->m_Data);
        for(int y=0;y<12;++y) for(int x=0;x<4;++x) { CKDWORD expected=y>=4&&y<8?0xffff0000u:0xff000000u;
            if(frame&&y==4&&x==2) expected=0xff0000ffu; EXPECT_EQ(expected,data[(11-y)*4+x]); }
    }
}
