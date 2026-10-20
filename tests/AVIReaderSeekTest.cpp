#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderSeekData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviSeekFile {
    std::filesystem::path Path;
    explicit AviSeekFile(const AviReaderTestData &a) {
        static unsigned sequence = 0; char name[96]; std::snprintf(name,sizeof(name),"avi-seek-%p-%u.avi",this,++sequence);
        Path = std::filesystem::temp_directory_path()/name; FILE *f = std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if (f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviSeekFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviSeekReader {
    CKMovieReader *Reader;
    AviSeekReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviSeekReader() { Reader->Release(); }
    CKERROR Open(const AviSeekFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
};
class AVIReaderSeekTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI keyframe seek",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Expect(AviSeekReader &reader,int frame,CKDWORD color,bool partial=false) {
        CKMovieProperties *p=nullptr; ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(frame,&p)); ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data);
        EXPECT_EQ(4,p->m_Format.Width); EXPECT_EQ(4,p->m_Format.Height);
        const CKDWORD *pixels=static_cast<const CKDWORD*>(p->m_Data);
        for(int i=0;i<16;++i) EXPECT_EQ(partial&&i==0?0xff0000ffu:color,pixels[i]) << "frame " << frame << " pixel " << i;
    }
    void Check(unsigned codec,unsigned options=SeekBadDelta,bool firstError=false) {
        AviReaderTestData data; MakeAviSeek(data,codec,options); AviSeekFile file(data); AviSeekReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(4,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(160,reader.Reader->GetMovieLength());
        if(firstError) { CKMovieProperties *p=nullptr; EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p)); EXPECT_EQ(nullptr,p); }
        const int order[]={3,0,2,3,2,0,3};
        for(int f:order) Expect(reader,f,f==0?0xffff0000u:(options&SeekPalette)&&f==3?0xffffff00u:0xff0000ffu);
    }
    void Partial(unsigned codec,bool bad) {
        AviReaderTestData data; MakeAviSeek(data,codec,SeekPartialKey|(bad?SeekBadDelta:0)); AviSeekFile file(data); AviSeekReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file));
        if(bad) { CKMovieProperties *p=nullptr; EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(3,&p)); EXPECT_EQ(nullptr,p); Expect(reader,0,0xffff0000u); }
        else { Expect(reader,3,0xff00ff00u,codec>=2); Expect(reader,0,0xffff0000u); Expect(reader,2,0xff00ff00u,codec>=2); }
    }
};
TEST_F(AVIReaderSeekTest, Video1Rgb555CanSeekBeyondDamagedDelta) { Check(0); }
TEST_F(AVIReaderSeekTest, Video1IndexedCanSeekBeyondDamagedDelta) { Check(1); }
TEST_F(AVIReaderSeekTest, Rle8CanSeekBeyondDamagedDelta) { Check(2); }
TEST_F(AVIReaderSeekTest, Rle4CanSeekBeyondDamagedDelta) { Check(3); }
TEST_F(AVIReaderSeekTest, Video1CanRecoverAfterExplicitReadError) { Check(0,SeekBadDelta,true); }
TEST_F(AVIReaderSeekTest, Rle8CanRecoverAfterExplicitReadError) { Check(2,SeekBadDelta,true); }
TEST_F(AVIReaderSeekTest, Video1FalseKeyStillUsesReferenceHistory) { Partial(0,false); }
TEST_F(AVIReaderSeekTest, Video1IndexedFalseKeyStillUsesReferenceHistory) { Partial(1,false); }
TEST_F(AVIReaderSeekTest, Rle8PartialKeyStillUsesReferenceHistory) { Partial(2,false); }
TEST_F(AVIReaderSeekTest, Rle4PartialKeyStillUsesReferenceHistory) { Partial(3,false); }
TEST_F(AVIReaderSeekTest, Video1FalseKeyCannotHideMissingReference) { Partial(0,true); }
TEST_F(AVIReaderSeekTest, Rle8PartialKeyCannotHideMissingReference) { Partial(2,true); }
TEST_F(AVIReaderSeekTest, UnmarkedFullPictureDoesNotAuthorizeSkippingDamagedHistory) {
    AviReaderTestData data; MakeAviSeek(data,0,SeekBadDelta|SeekNoKey); AviSeekFile file(data); AviSeekReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(3,&p)); EXPECT_EQ(nullptr,p);
}
TEST_F(AVIReaderSeekTest, IndexlessVideoRetainsSequentialFailure) {
    AviReaderTestData data; MakeAviSeek(data,0,SeekBadDelta|SeekNoIndex); AviSeekFile file(data); AviSeekReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(3,&p)); EXPECT_EQ(nullptr,p);
}
TEST_F(AVIReaderSeekTest, StandardVideo1IndexProvidesKeyframe) { Check(0,SeekBadDelta|SeekStandardIndex); }
TEST_F(AVIReaderSeekTest, StandardRle8IndexProvidesKeyframe) { Check(2,SeekBadDelta|SeekStandardIndex); }
TEST_F(AVIReaderSeekTest, PaletteOnDroppedTargetRecolorsEarlierKeyframe) { Check(1,SeekBadDelta|SeekPalette|SeekDrop); }
TEST_F(AVIReaderSeekTest, PaletteOnSkippedTargetRecolorsEarlierKeyframe) { Check(2,SeekBadDelta|SeekPalette); }
TEST_F(AVIReaderSeekTest, LayoutWrapperPreservesIndependentFrameResult) { Check(0,SeekBadDelta|SeekLayout); }
TEST_F(AVIReaderSeekTest, FalseLastKeyFallsBackToActualIndependentKey) { Check(0,SeekBadDelta|SeekFalseLastKey); }
TEST_F(AVIReaderSeekTest, Rle8AbsoluteKeyCountsWrittenPixels) { Check(2,SeekBadDelta|SeekAbsoluteRle); }
TEST_F(AVIReaderSeekTest, Rle4AbsoluteKeyCountsWrittenPixels) { Check(3,SeekBadDelta|SeekAbsoluteRle); }
TEST_F(AVIReaderSeekTest, BitmapConsumerCanSelectKeyframeBeyondDamage) {
    AviReaderTestData data; MakeAviSeek(data,1); AviSeekFile file(data); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    ASSERT_TRUE(bitmap.SetCurrentSlot(3)); EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xffff0000u,bitmap.GetPixel(0,0));
}
TEST_F(AVIReaderSeekTest, BrokenFirstPictureStillCannotOpen) {
    AviReaderTestData data; MakeAviSeek(data,0); data.Data[data.Index[0].Offset+8]=0; data.Data[data.Index[0].Offset+9]=0;
    AviSeekFile file(data); AviSeekReader reader; EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE,reader.Open(file));
}
