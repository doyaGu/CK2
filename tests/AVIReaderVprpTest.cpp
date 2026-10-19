#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderVprpData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviVprpFile {
    std::filesystem::path Path;
    AviVprpFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviVprpFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviVprpReader {
    CKMovieReader *Reader;
    AviVprpReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviVprpReader() { Reader->Release(); }
    CKERROR Open(const AviVprpFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};


class AVIReaderVprpTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI vprp",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Raw(bool topDown=false, bool early=false, unsigned fields=1, bool topFirst=true) {
        AviVprpData a; a.Raw(topDown,early,fields,topFirst); AviVprpFile file(a); AviVprpReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={1,0,1}; for(int f:order) { CKMovieProperties *p=reader.Frame(f); ASSERT_NE(nullptr,p);
            ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(4,p->m_Format.Height); EXPECT_EQ(16,p->m_Format.BytesPerLine);
            const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
            for(int y=0;y<4;++y) for(int x=0;x<4;++x) { int sourceY=3-y+(fields==1?1:2),v=sourceY*16+x+2;
                for(int c=0;c<3;++c) EXPECT_EQ(v,pixels[(y*4+x)*4+c]); EXPECT_EQ(255,pixels[(y*4+x)*4+3]); }
        }
    }
    void Jpeg(bool fields=true, bool topFirst=true, bool reverse=false, bool omit=false) {
        AviVprpData a; a.Jpeg(fields,topFirst,reverse,omit); AviVprpFile file(a); AviVprpReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file));
        const int order[]={1,0,1}; for(int f:order) { CKMovieProperties *p=reader.Frame(f); ASSERT_NE(nullptr,p);
            ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(fields?16:8,p->m_Format.Height);
            const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
            for(int y=0;y<p->m_Format.Height;++y) for(int x=0;x<4;++x) { int topY=p->m_Format.Height-1-y;
                int field=fields?((topY&1)==(topFirst?0:1)?0:1):0;
                int sourceY=(fields?topY/2:topY)+4,v=(field?160:32)+(sourceY>=8?64:0);
                for(int c=0;c<3;++c) EXPECT_NEAR(v,pixels[(y*4+x)*4+c],1); EXPECT_EQ(255,pixels[(y*4+x)*4+3]); }
        }
    }
};
TEST_F(AVIReaderVprpTest, BottomUpRawCropsBitmapCoordinates) { Raw(); }
TEST_F(AVIReaderVprpTest, TopDownRawCropsSameVisibleArea) { Raw(true); }
TEST_F(AVIReaderVprpTest, PropertiesBeforeFormatAreRetained) { Raw(false,true); }
TEST_F(AVIReaderVprpTest, RawFieldsCropWithinEachFieldWithoutReswappingRaster) { Raw(false,false,2); }
TEST_F(AVIReaderVprpTest, RawBottomFirstDescriptorsKeepRasterOrder) { Raw(false,false,2,false); }
TEST_F(AVIReaderVprpTest, ProgressiveJpegUsesValidRegion) { Jpeg(false); }
TEST_F(AVIReaderVprpTest, TopFirstJpegUsesVideoFieldLayout) { Jpeg(); }
TEST_F(AVIReaderVprpTest, BottomFirstJpegUsesVideoFieldLayout) { Jpeg(true,false); }
TEST_F(AVIReaderVprpTest, Avi1IdentitySelectsCropAfterReversedStorage) { Jpeg(true,true,true); }
TEST_F(AVIReaderVprpTest, CroppedFieldsMayOmitDht) { Jpeg(true,true,false,true); }
TEST_F(AVIReaderVprpTest, BitmapConsumerReceivesCroppedDimensionsAndPixels) {
    AviVprpData a; a.Raw(); AviVprpFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(4,bitmap.GetWidth()); EXPECT_EQ(4,bitmap.GetHeight()); EXPECT_EQ(0xff121212u,bitmap.GetPixel(0,0)); EXPECT_EQ(0xff454545u,bitmap.GetPixel(3,3));
}
TEST_F(AVIReaderVprpTest, SignalCoordinatesDoNotBecomeBitmapOffsets) {
    AviVprpData a; a.Raw(); a.Word(a.Vprp+44+24,122); a.Word(a.Vprp+44+28,283); AviVprpFile file(a); AviVprpReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p); ASSERT_EQ(4,p->m_Format.Width);
    EXPECT_EQ(0xff424242u,static_cast<const CKDWORD*>(p->m_Data)[0]);
}
TEST_F(AVIReaderVprpTest, ReopenWithoutPropertiesDropsPreviousCrop) {
    AviVprpData a; a.Raw(); AviVprpFile first(a); AviVprpReader reader; ASSERT_EQ(CK_OK,reader.Open(first));
    AviReaderTestData b; b.Start(24,0,8,8); uint8_t pixels[192]={}; b.Frame(pixels,sizeof(pixels)); b.Finish(); AviVprpFile second(b);
    ASSERT_EQ(CK_OK,reader.Open(second)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p); EXPECT_EQ(8,p->m_Format.Width); EXPECT_EQ(8,p->m_Format.Height);
}
TEST_F(AVIReaderVprpTest, RectangleOutsideCompressedBitmapIsRejected) {
    AviVprpData a; a.Raw(); a.Word(a.Vprp+44+16,7); AviVprpFile file(a); AviVprpReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderVprpTest, TruncatedFieldDescriptorsAreRejected) {
    AviVprpData a; a.Raw(); a.Word(a.Vprp+8+32,2); AviVprpFile file(a); AviVprpReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}

TEST_F(AVIReaderVprpTest, EachJpegFieldKeepsItsOwnCropRectangle) {
    AviVprpData a; a.Jpeg(); a.Word(a.Vprp+44+20,0); a.Word(a.Vprp+76+20,8);
    AviVprpFile file(a); AviVprpReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(1); ASSERT_NE(nullptr,p);
    ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(16,p->m_Format.Height); const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
    for(int y=0;y<16;++y) for(int x=0;x<4;++x) { int v=((15-y)&1)?224:32;
        for(int c=0;c<3;++c) EXPECT_NEAR(v,pixels[(y*4+x)*4+c],1); }
}
TEST_F(AVIReaderVprpTest, UnequalCompressedFieldHeightsUseIndependentDescriptors) {
    AviVprpData a; a.Start(24,0x47504a4du,8,32); a.Properties(2,8,16,4,8,2,4,true);
    a.Word(a.Vprp+76,8); a.Word(a.Vprp+76+20,0);
    uint8_t bytes[2048]; unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1,true);
    n+=AviFieldBytes(bytes+n,AviPaletteJpeg,sizeof(AviPaletteJpeg),2,true); a.Frame(bytes,n); a.Finish();
    AviVprpFile file(a); AviVprpReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p);
    ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(16,p->m_Format.Height); const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
    const int bgr[]={144,96,48};
    for(int y=0;y<16;++y) for(int x=0;x<4;++x) { int topY=15-y,v=topY/2+4<8?32:96;
        for(int c=0;c<3;++c) EXPECT_NEAR((topY&1)?bgr[c]:v,pixels[(y*4+x)*4+c],2); EXPECT_EQ(255,pixels[(y*4+x)*4+3]); }
}
TEST_F(AVIReaderVprpTest, OddVisibleHeightRetainsTheExtraTopFieldLine) {
    AviVprpData a; a.Jpeg(); a.Word(a.Vprp+44+8,9); AviVprpFile file(a); AviVprpReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
    CKMovieProperties *p=reader.Frame(0); ASSERT_NE(nullptr,p); ASSERT_EQ(17,p->m_Format.Height); ASSERT_EQ(4,p->m_Format.Width);
    const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
    for(int y=0;y<17;++y) for(int x=0;x<4;++x) { int topY=16-y,v=((topY&1)?160:32)+(topY/2+4>=8?64:0);
        for(int c=0;c<3;++c) EXPECT_NEAR(v,pixels[(y*4+x)*4+c],1); }
}
TEST_F(AVIReaderVprpTest, CroppingPreservesDeltaHistoryAndDroppedFrames) {
    AviVprpData a; a.Start(8,1,8,8); a.Properties(1,8,8,4,4,2,1);
    uint8_t full[34]; for(int i=0;i<8;++i) { full[i*4]=8; full[i*4+1]=1; full[i*4+2]=0; full[i*4+3]=0; }
    full[32]=0; full[33]=1; const uint8_t delta[]={0,2,2,6,1,2,0,1};
    a.Frame(full,sizeof(full)); a.Frame(delta,sizeof(delta)); a.Frame(nullptr,0); a.Finish();
    AviVprpFile file(a); AviVprpReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); const int order[]={1,0,2,1};
    for(int frame:order) { CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_EQ(4,p->m_Format.Width); ASSERT_EQ(4,p->m_Format.Height);
        const CKDWORD *pixels=static_cast<const CKDWORD*>(p->m_Data);
        for(int y=0;y<4;++y) for(int x=0;x<4;++x) EXPECT_EQ(frame&&y==3&&x==0?0xff0000ffu:0xffff0000u,pixels[y*4+x]); }
}
TEST_F(AVIReaderVprpTest, CroppedDecoderHonorsOutputStridePadding) {
    AviVprpData a; a.Jpeg(false); AviVprpFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    auto decoder=CreateFrameDecoder(*demuxer.GetStreamInfo(0)); ASSERT_NE(nullptr,decoder);
    uint8_t output[8*24]; std::memset(output,0xa5,sizeof(output));
    ASSERT_TRUE(decoder->Decode(a.Data+a.Index[0].Offset+8,a.Index[0].Size,4,8,output,24));
    for(int y=0;y<8;++y) { int v=y<4?96:32;
        for(int x=0;x<4;++x) { for(int c=0;c<3;++c) EXPECT_NEAR(v,output[y*24+x*4+c],1); EXPECT_EQ(255,output[y*24+x*4+3]); }
        for(int i=16;i<24;++i) EXPECT_EQ(0xa5,output[y*24+i]); }
}
