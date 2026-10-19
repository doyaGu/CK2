#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderInterlacedData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviInterlacedFile {
    std::filesystem::path Path;
    AviInterlacedFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviInterlacedFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviInterlacedReader {
    CKMovieReader *Reader;
    AviInterlacedReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviInterlacedReader() { Reader->Release(); }
    CKERROR Open(const AviInterlacedFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};

class AVIReaderInterlacedTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI interlaced",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Gray(CKMovieProperties *p, bool single=false, bool reversed=false) {
        ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); ASSERT_EQ(8,p->m_Format.Width); ASSERT_EQ(32,p->m_Format.Height);
        const CKBYTE *data=static_cast<const CKBYTE*>(p->m_Data);
        for(int y=0;y<32;++y) for(int x=0;x<8;++x) {
            int topY=31-y, band=(topY/2)/8;
            int expected=single?(band?96:32):((topY&1)?(reversed?160:32):(reversed?32:160))+band*64;
            for(int c=0;c<3;++c) EXPECT_NEAR(expected,data[(y*8+x)*4+c],1)<<"row "<<y;
            EXPECT_EQ(255,data[(y*8+x)*4+3]);
        }
    }
    void Pair(unsigned omit=0, unsigned padding=0, bool sizes=true, bool reversed=false, bool markers=true) {
        uint8_t bytes[2048]; unsigned n;
        if(markers) { n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),reversed?2:1,(omit&1)!=0,padding,sizes);
            n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),reversed?1:2,(omit&2)!=0); }
        else { n=sizeof(AviFieldA); std::memcpy(bytes,AviFieldA,n); std::memcpy(bytes+n,AviFieldB,sizeof(AviFieldB)); n+=sizeof(AviFieldB); }
        AviReaderTestData a; a.Start(24,0x47504a4du,8,32); a.Frame(bytes,n); a.Finish();
        AviInterlacedFile file(a); AviInterlacedReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
        EXPECT_EQ(1,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(40,reader.Reader->GetMovieLength()); Gray(reader.Frame(0),false,reversed);
    }
    void Index(bool super, bool avix=false, bool zero=false) {
        AviInterlacedData a; a.MakeIndex(super,avix,true,zero); AviInterlacedFile file(a); AviDemuxer demuxer;
        ASSERT_TRUE(demuxer.Open(file.Path.string().c_str())); EXPECT_EQ(2,demuxer.GetFrameCount(0));
        const avi::FrameIndexEntry *first=demuxer.GetFrameInfo(0,0), *second=demuxer.GetFrameInfo(0,1);
        ASSERT_NE(nullptr,first); ASSERT_NE(nullptr,second); EXPECT_EQ(a.Index[1].Offset+8,first->offset); EXPECT_EQ(48,first->size);
        EXPECT_TRUE(first->isKeyframe); EXPECT_FALSE(second->isKeyframe);
        AviInterlacedReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(80,reader.Reader->GetMovieLength());
        const int order[]={0,1,0,1}; for(int frame:order) { CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p);
            const CKBYTE *pixels=static_cast<const CKBYTE*>(p->m_Data);
            for(int i=0;i<16;++i) { for(int c=0;c<3;++c) EXPECT_EQ(frame?17:34,pixels[i*4+c]); EXPECT_EQ(255,pixels[i*4+3]); } }
    }
};
TEST_F(AVIReaderInterlacedTest, TwoFieldsWeaveBottomFirst) { Pair(); }
TEST_F(AVIReaderInterlacedTest, BothFieldsMayOmitDht) { Pair(3); }
TEST_F(AVIReaderInterlacedTest, OnlyFirstFieldOmitsDht) { Pair(1); }
TEST_F(AVIReaderInterlacedTest, OnlySecondFieldOmitsDht) { Pair(2); }
TEST_F(AVIReaderInterlacedTest, FieldPaddingAndBigEndianSizes) { Pair(3,7); }
TEST_F(AVIReaderInterlacedTest, LegacyZeroSizesScanBothFields) { Pair(0,5,false); }
TEST_F(AVIReaderInterlacedTest, FieldIdentitySurvivesReversedStorage) { Pair(0,0,true,true); }
TEST_F(AVIReaderInterlacedTest, LegacyJfifPairUsesAviBottomFirstOrder) { Pair(0,0,true,false,false); }
TEST_F(AVIReaderInterlacedTest, SingleCapturedFieldDuplicatesLines) {
    uint8_t bytes[1024]; unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1,true);
    AviReaderTestData a; a.Start(24,0x47504a4du,8,32); a.Frame(bytes,n); a.Finish();
    AviInterlacedFile file(a); AviInterlacedReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); Gray(reader.Frame(0),true);
}
TEST_F(AVIReaderInterlacedTest, HeaderFieldIndexHonorsOrderAndKeyFlag) { Index(false); }
TEST_F(AVIReaderInterlacedTest, HeaderFieldIndexAllowsZeroBase) { Index(false,false,true); }
TEST_F(AVIReaderInterlacedTest, SuperFieldIndexHonorsOrderAndKeyFlag) { Index(true); }
TEST_F(AVIReaderInterlacedTest, SuperFieldIndexRetainsAvixLogicalOrder) { Index(true,true); }
TEST_F(AVIReaderInterlacedTest, FieldIndexFramesAreNotCountedAsSeparateFields) {
    AviInterlacedData a; a.MakeIndex(false); AviInterlacedFile file(a); AviInterlacedReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(2,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(80,reader.Reader->GetMovieLength());
    Gray(reader.Frame(1)); Gray(reader.Frame(0));
}
TEST_F(AVIReaderInterlacedTest, BitmapConsumesFullInterlacedFrame) {
    AviInterlacedData a; a.MakeIndex(true,true); AviInterlacedFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); EXPECT_EQ(2,bitmap.GetSlotCount());
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(0xffa0a0a0u,bitmap.GetPixel(0,0)); EXPECT_EQ(0xff202020u,bitmap.GetPixel(0,1));
    EXPECT_EQ(0xffe0e0e0u,bitmap.GetPixel(0,30)); EXPECT_EQ(0xff606060u,bitmap.GetPixel(0,31));
}
TEST_F(AVIReaderInterlacedTest, IncompleteSecondFieldFailsThenNextFrameRecovers) {
    uint8_t bytes[2048]; unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1);
    unsigned split=n; n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),2,true);
    AviReaderTestData a; a.Start(24,0x47504a4du,8,32); a.Frame(bytes,n); a.Frame(bytes,split+10); a.Frame(bytes,n); a.Finish();
    AviInterlacedFile file(a); AviInterlacedReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p)); Gray(reader.Frame(2)); Gray(reader.Frame(0));
}
TEST_F(AVIReaderInterlacedTest, FieldIndexStrideMismatchIsRejected) {
    AviInterlacedData a; a.MakeIndex(false,true,true); a.Word(a.FieldIndex+8,2,2); AviInterlacedFile file(a); AviDemuxer demuxer;
    EXPECT_FALSE(demuxer.Open(file.Path.string().c_str()));
}
TEST_F(AVIReaderInterlacedTest, FieldIndexPastEndIsNotAcceptedAsValidIndex) {
    AviInterlacedData a; a.MakeIndex(false,false,true); a.Word(a.FieldIndex+40,a.Read(a.FieldIndex+32)+49);
    AviInterlacedFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    // Invalid first index entry is ignored; its neighbor remains one actual frame.
    EXPECT_EQ(1,demuxer.GetFrameCount(0)); EXPECT_EQ(a.Index[0].Offset+8,demuxer.GetFrameInfo(0,0)->offset);
}
TEST_F(AVIReaderInterlacedTest, JpegAppPayloadDoesNotCreateFalseFieldBoundary) {
    uint8_t bytes[2048]; unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1);
    const uint8_t app[]={255,0xe2,0,8,255,0xd9,255,0xd8,3,4};
    std::memmove(bytes+2+sizeof(app),bytes+2,n-2); std::memcpy(bytes+2,app,sizeof(app)); n+=sizeof(app);
    // The AVI1 size hints are now stale: marker scanning still has to find the real EOI.
    n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),2);
    AviReaderTestData a; a.Start(24,0x47504a4du,8,32); a.Frame(bytes,n); a.Finish();
    AviInterlacedFile file(a); AviInterlacedReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); Gray(reader.Frame(0));
}
TEST_F(AVIReaderInterlacedTest, DifferentSecondFieldDimensionsAreRejected) {
    uint8_t bytes[2048]; unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1), split=n;
    n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),2);
    for(unsigned p=split;p+9<n;++p) if(bytes[p]==255&&bytes[p+1]==0xc0) { bytes[p+8]=7; break; }
    AviReaderTestData a; a.Start(24,0x47504a4du,8,32); a.Frame(bytes,n); a.Finish();
    AviInterlacedFile file(a); AviInterlacedReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}

TEST_F(AVIReaderInterlacedTest, InterlacedOutputHonorsStrideAndLeavesPadding) {
    uint8_t bytes[2048], output[32*40]; std::memset(output,0xa5,sizeof(output));
    unsigned n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),1,true,3);
    n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),2,true);
    MjpegFrameDecoder decoder; ASSERT_TRUE(decoder.Decode(bytes,n,8,32,output,40));
    for(int y=0;y<32;++y) {
        int topY=31-y, expected=((topY&1)?32:160)+(topY/16)*64;
        for(int x=0;x<8;++x) { for(int c=0;c<3;++c) EXPECT_NEAR(expected,output[y*40+x*4+c],1); EXPECT_EQ(255,output[y*40+x*4+3]); }
        for(int i=32;i<40;++i) EXPECT_EQ(0xa5,output[y*40+i]);
    }
}
