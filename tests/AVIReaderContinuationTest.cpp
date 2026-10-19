#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderContinuationData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviContinuationFile {
    std::filesystem::path Path;
    AviContinuationFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviContinuationFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviContinuationReader {
    CKMovieReader *Reader;
    AviContinuationReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviContinuationReader() { Reader->Release(); }
    CKERROR Open(const AviContinuationFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};
class AVIReaderContinuationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI continuation",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Segments(unsigned codec, bool rec=false, int indexMode=0) {
        AviContinuationData a; a.Make(codec,rec,indexMode); AviContinuationFile file(a); AviContinuationReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(6,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(240,reader.Reader->GetMovieLength());
        const int order[]={5,0,4,2,1,3,4,0,5};
        for(int frame:order) {
            CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data);
            const CKDWORD one=frame<2?0xffff0000u:frame<4?0xff00ff00u:0xff112141u;
            const CKDWORD other=frame<4?0xff0000ffu:0xffffff00u;
            if(codec==0x4d415243u) { for(int i=0;i<16;++i) EXPECT_EQ(one,static_cast<CKDWORD*>(p->m_Data)[i]); }
            else { const CKDWORD expected[]={one,codec?one:other,other,codec?other:one};
                EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)))<<"frame "<<frame; }
        }
    }
    void ExpectJpeg(CKMovieProperties *p, int gray=-1) {
        ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); EXPECT_EQ(8,p->m_Format.Width); EXPECT_EQ(8,p->m_Format.Height);
        const CKBYTE *pixels=static_cast<CKBYTE*>(p->m_Data);
        for(int i=0;i<64;++i) { EXPECT_NEAR(gray<0?144:gray,pixels[4*i],2); EXPECT_NEAR(gray<0?96:gray,pixels[4*i+1],2);
            EXPECT_NEAR(gray<0?48:gray,pixels[4*i+2],2); EXPECT_EQ(255,pixels[4*i+3]); }
    }
    void Jpeg(unsigned mask, bool avi1=false) {
        uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,mask,avi1);
        AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
        AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); ExpectJpeg(reader.Frame(0));
    }
    void Gray(bool omitAc) {
        uint8_t bytes[1024]; unsigned n=AviCustomGrayJpeg(bytes,omitAc);
        AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
        AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); ExpectJpeg(reader.Frame(0),128);
    }
};
TEST_F(AVIReaderContinuationTest, AvixRawPaletteFollowsAllThreeSegments) { Segments(0); }
TEST_F(AVIReaderContinuationTest, AvixRawPaletteInsideRecordLists) { Segments(0,true); }
TEST_F(AVIReaderContinuationTest, AvixRleHistoryRecolorsAcrossSegments) { Segments(1); }
TEST_F(AVIReaderContinuationTest, AvixVideo1HistoryRecolorsAcrossSegments) { Segments(0x4d415243u); }
TEST_F(AVIReaderContinuationTest, IndexlessAvixRetainsEveryPhysicalSample) { Segments(0,false,1); }
TEST_F(AVIReaderContinuationTest, IndexlessAvixRecordsPreserveRleDependencies) { Segments(1,true,1); }
TEST_F(AVIReaderContinuationTest, InitialClassicIndexDoesNotHideLaterAvixFrames) { Segments(0,false,2); }
TEST_F(AVIReaderContinuationTest, BitmapConsumerReadsRecoloredDroppedAvixSlot) {
    AviContinuationData a; a.Make(1); AviContinuationFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); EXPECT_EQ(6,bitmap.GetSlotCount());
    ASSERT_TRUE(bitmap.SetCurrentSlot(4)); EXPECT_EQ(0xffffff00u,bitmap.GetPixel(0,0)); EXPECT_EQ(0xff112141u,bitmap.GetPixel(0,1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0)); EXPECT_EQ(0xffff0000u,bitmap.GetPixel(0,1));
}
TEST_F(AVIReaderContinuationTest, MissingDhtWithAvi1ApplicationMarker) { Jpeg(0,true); }
TEST_F(AVIReaderContinuationTest, ExplicitCustomDcTableControl) { Gray(false); }
TEST_F(AVIReaderContinuationTest, MissingAcTablePreservesExplicitCustomDcTable) { Gray(true); }
TEST_F(AVIReaderContinuationTest, LaterAvixJpegMayOmitHuffmanTables) {
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(AviPaletteJpeg,sizeof(AviPaletteJpeg)); a.Finish(false);
    unsigned root=a.Begin("RIFF"); a.Bytes("AVIX",4); unsigned movi=a.List("movi");
    uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,0); a.Frame(bytes,n); a.End(movi); a.End(root); a.Word(a.StreamCountAt,2);
    AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount());
    ExpectJpeg(reader.Frame(1)); ExpectJpeg(reader.Frame(0));
}
TEST_F(AVIReaderContinuationTest, MalformedExplicitDhtIsNotReplacedByDefaults) {
    uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,15);
    for(unsigned p=0;p+5<n;++p) if(bytes[p]==255&&bytes[p+1]==0xc4) { bytes[p+2]=0; bytes[p+3]=1; break; }
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderContinuationTest, MissingNonstandardTableTwoFailsInsteadOfGuessing) {
    uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,0);
    for(unsigned p=0;p+7<n;++p) if(bytes[p]==255&&bytes[p+1]==0xda) { bytes[p+6]=0x22; break; }
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderContinuationTest, BadJpegDoesNotPoisonLaterMissingDhtFrame) {
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(AviPaletteJpeg,sizeof(AviPaletteJpeg));
    const uint8_t bad[]={255,0xd8,255,0xc4,0,1,255,0xd9}; a.Frame(bad,sizeof(bad));
    uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,0); a.Frame(bytes,n); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p)); ExpectJpeg(reader.Frame(2)); ExpectJpeg(reader.Frame(0));
}
TEST_F(AVIReaderContinuationTest, ExplicitNonstandardTableTwoRemainsSupported) {
    uint8_t bytes[1024]; unsigned n=AviCustomGrayJpeg(bytes,false);
    for(unsigned p=0;p+7<n;++p) {
        if(bytes[p]==255&&bytes[p+1]==0xc4&&bytes[p+4]==0) bytes[p+4]=2;
        if(bytes[p]==255&&bytes[p+1]==0xda) bytes[p+6]=0x20;
    }
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); ExpectJpeg(reader.Frame(0),128);
}
TEST_F(AVIReaderContinuationTest, ShortBaselineScanHeaderFailsWithoutReadingPastItsPayload) {
    uint8_t bytes[sizeof(AviPaletteJpeg)]; unsigned n=AviJpegVariant(bytes,15);
    for(unsigned p=0;p+7<n;++p) if(bytes[p]==255&&bytes[p+1]==0xda) { bytes[p+2]=0; bytes[p+3]=2; n=p+4; break; }
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(bytes,n); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderContinuationTest, ProgressiveJpegRetainsTablesDefinedInLaterScans) {
    AviReaderTestData a; a.Start(24,0x47504a4du,8,8); a.Frame(AviContinuationProgressiveJpeg,sizeof(AviContinuationProgressiveJpeg)); a.Finish();
    AviContinuationFile file(a); AviContinuationReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); ExpectJpeg(reader.Frame(0));
}

TEST_F(AVIReaderContinuationTest, HuffmanTableMask0) { Jpeg(0); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask1) { Jpeg(1); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask2) { Jpeg(2); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask3) { Jpeg(3); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask4) { Jpeg(4); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask5) { Jpeg(5); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask6) { Jpeg(6); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask7) { Jpeg(7); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask8) { Jpeg(8); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask9) { Jpeg(9); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask10) { Jpeg(10); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask11) { Jpeg(11); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask12) { Jpeg(12); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask13) { Jpeg(13); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask14) { Jpeg(14); }
TEST_F(AVIReaderContinuationTest, HuffmanTableMask15) { Jpeg(15); }
