#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderApertureData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviApertureFile {
    std::filesystem::path Path;
    AviApertureFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviApertureFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviApertureReader {
    CKMovieReader *Reader;
    AviApertureReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviApertureReader() { Reader->Release(); }
    CKERROR Open(const AviApertureFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};



class AVIReaderApertureTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI aperture",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Pixels(CKMovieProperties *p, const int *topRows, int width, int height) {
        ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); ASSERT_EQ(width,p->m_Format.Width); ASSERT_EQ(height,p->m_Format.Height);
        EXPECT_EQ(width*4,p->m_Format.BytesPerLine); const CKBYTE *data=static_cast<const CKBYTE*>(p->m_Data);
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            int expected=topRows[(height-1-y)*width+x];
            for(int c=0;c<3;++c) EXPECT_NEAR(expected,data[(y*width+x)*4+c],1)<<"x="<<x<<" bottomY="<<y;
            EXPECT_EQ(255,data[(y*width+x)*4+3]);
        }
    }
    void Raw(unsigned mode, const int *rows, int width, int height=4, bool topFirst=true, bool topDown=false) {
        AviApertureData a; a.Make(false,mode,topFirst,topDown); AviApertureFile file(a); AviApertureReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={1,0,1}; for(int frame:order) Pixels(reader.Frame(frame),rows,width,height);
    }
    void Jpeg(unsigned mode, bool topFirst=true, bool reverse=false) {
        AviApertureData a; a.Make(true,mode,topFirst,false,reverse); AviApertureFile file(a); AviApertureReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file));
        const int widths[]={3,4,5,7,4,3,4}; int width=widths[mode],height=mode==6?20:16; int rows[140]={};
        // Literal row colors/coverage: field A is 32 for four rows then 96;
        // field B's cropped eight rows are all224. Source x crop is independent.
        const int firstX=mode==2?2:0,secondX=mode==1||mode==6?2:mode==3?5:mode==4?1:0;
        for(int fieldRow=0;fieldRow<(mode==6?10:8);++fieldRow) {
            int firstY=fieldRow*2+(topFirst?0:1),secondY=fieldRow*2+(topFirst?1:0);
            if(fieldRow<(mode==6?4:8)) for(int x=0;x<3;++x) rows[firstY*width+firstX+x]=mode==6?32:fieldRow<4?32:96;
            for(int x=0;x<(mode>=4&&mode<=5?3:2);++x) rows[secondY*width+secondX+x]=mode==6?(fieldRow<8?160:224):224;
        }
        Pixels(reader.Frame(1),rows,width,height); Pixels(reader.Frame(0),rows,width,height);
    }
};
TEST_F(AVIReaderApertureTest, UnequalRawFieldWidthsShareOrigin) {
    const int rows[]={33,34,35,20,21,0,65,66,67,52,53,0}; Raw(0,rows,3);
}
TEST_F(AVIReaderApertureTest, RawSecondFieldShiftsRight) {
    const int rows[]={33,34,35,0,0,0,20,21,65,66,67,0,0,0,52,53}; Raw(1,rows,4);
}
TEST_F(AVIReaderApertureTest, RawFirstFieldShiftsRight) {
    const int rows[]={0,0,33,34,35,20,21,0,0,0,0,0,65,66,67,52,53,0,0,0}; Raw(2,rows,5);
}
TEST_F(AVIReaderApertureTest, DisjointRawAperturesKeepBlackGap) {
    const int rows[]={33,34,35,0,0,0,0,0,0,0,0,0,20,21,65,66,67,0,0,0,0,0,0,0,0,0,52,53}; Raw(3,rows,7);
}
TEST_F(AVIReaderApertureTest, EqualWidthsStillRetainSignalOffset) {
    const int rows[]={33,34,35,0,0,20,21,22,65,66,67,0,0,52,53,54}; Raw(4,rows,4);
}
TEST_F(AVIReaderApertureTest, DifferentBitmapCropOffsetsDoNotShiftDestination) {
    const int rows[]={33,34,35,20,21,22,65,66,67,52,53,54}; Raw(5,rows,3);
}
TEST_F(AVIReaderApertureTest, TopDownRawUsesSameVisibleUnion) {
    const int rows[]={33,34,35,0,0,0,20,21,65,66,67,0,0,0,52,53}; Raw(1,rows,4,4,true,true);
}
TEST_F(AVIReaderApertureTest, BottomFirstRawIsNotReswapped) {
    const int rows[]={0,0,4,5,49,50,51,0,0,0,36,37,81,82,83,0}; Raw(1,rows,4,4,false);
}
TEST_F(AVIReaderApertureTest, UnequalRawHeightsLeaveUncoveredFieldRowsBlack) {
    const int rows[]={1,2,3,0,0,0,20,21,0,0,0,0,0,0,52,53,0,0,0,0,0,0,84,85}; Raw(6,rows,4,6);
}
TEST_F(AVIReaderApertureTest, UnequalJpegFieldWidthsShareOrigin) { Jpeg(0); }
TEST_F(AVIReaderApertureTest, JpegSecondFieldShiftsRight) { Jpeg(1); }
TEST_F(AVIReaderApertureTest, JpegFirstFieldShiftsRight) { Jpeg(2); }
TEST_F(AVIReaderApertureTest, DisjointJpegAperturesKeepBlackGap) { Jpeg(3); }
TEST_F(AVIReaderApertureTest, EqualJpegWidthsRetainSignalOffset) { Jpeg(4); }
TEST_F(AVIReaderApertureTest, BottomFirstJpegUsesDescriptorDestination) { Jpeg(1,false); }
TEST_F(AVIReaderApertureTest, ReversedAvi1StorageRetainsDescriptorDestination) { Jpeg(1,true,true); }
TEST_F(AVIReaderApertureTest, UnequalJpegHeightsKeepMissingFieldRowsBlack) { Jpeg(6); }
TEST_F(AVIReaderApertureTest, BitmapConsumerSeesUnionAndOpaqueBlackMargins) {
    AviApertureData a; a.Make(); AviApertureFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(4,bitmap.GetWidth()); EXPECT_EQ(4,bitmap.GetHeight()); EXPECT_EQ(0xff212121u,bitmap.GetPixel(0,0));
    EXPECT_EQ(0xff000000u,bitmap.GetPixel(0,1)); EXPECT_EQ(0xff353535u,bitmap.GetPixel(3,3));
}
TEST_F(AVIReaderApertureTest, DecoderWritesBlackMarginsButPreservesStridePadding) {
    AviApertureData a; a.Make(true,1); AviApertureFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    auto decoder=CreateFrameDecoder(*demuxer.GetStreamInfo(0)); ASSERT_NE(nullptr,decoder); uint8_t output[16*24]; std::memset(output,0xa5,sizeof(output));
    ASSERT_TRUE(decoder->Decode(a.Data+a.Index[0].Offset+8,a.Index[0].Size,4,16,output,24));
    for(int y=0;y<16;++y) { int margin=((15-y)&1)?0:3; const uint8_t *p=output+y*24+margin*4;
        EXPECT_EQ(0,p[0]); EXPECT_EQ(0,p[1]); EXPECT_EQ(0,p[2]); EXPECT_EQ(255,p[3]); for(int i=16;i<24;++i) EXPECT_EQ(0xa5,output[y*24+i]); }
}
TEST_F(AVIReaderApertureTest, LargeCommonSignalOriginDoesNotCreateHugeImage) {
    AviApertureData a; a.Make(); a.Word(a.Vprp+44+24,0xfffffff0u); a.Word(a.Vprp+76+24,0xfffffff2u); AviApertureFile file(a); AviApertureReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); const int rows[]={33,34,35,0,0,0,20,21,65,66,67,0,0,0,52,53}; Pixels(reader.Frame(0),rows,4,4);
}
TEST_F(AVIReaderApertureTest, UnrepresentableSignalSpanDoesNotWrapWidth) {
    AviApertureData a; a.Make(); a.Word(a.Vprp+44+24,0); a.Word(a.Vprp+76+24,0xffffffffu); AviApertureFile file(a); AviApertureReader reader;
    EXPECT_NE(CK_OK,reader.Open(file));
}

TEST_F(AVIReaderApertureTest, DeltaHistoryAndDroppedFramesKeepBlackApertureMargins) {
    AviApertureData a; a.Start(8,1,8,8); a.Properties(2,8,4,4,2,2,1); a.Geometry(false,1);
    uint8_t full[34]; for(int i=0;i<8;++i) { full[4*i]=8; full[4*i+1]=1; full[4*i+2]=0; full[4*i+3]=0; } full[32]=0; full[33]=1;
    const uint8_t delta[]={0,2,4,6,1,2,0,1}; a.Frame(full,sizeof(full)); a.Frame(delta,sizeof(delta)); a.Frame(nullptr,0); a.Finish();
    AviApertureFile file(a); AviApertureReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); const int order[]={1,0,2,1};
    const int coverage[]={1,1,1,0,0,0,1,1,1,1,1,0,0,0,1,1};
    for(int frame:order) { CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(4,p->m_Format.Height);
        const CKDWORD *data=static_cast<const CKDWORD*>(p->m_Data);
        for(int y=0;y<4;++y) for(int x=0;x<4;++x) { CKDWORD expected=coverage[(3-y)*4+x]?0xffff0000u:0xff000000u;
            if(frame&&y==2&&x==2) expected=0xff0000ffu; EXPECT_EQ(expected,data[y*4+x]); }
    }
}
