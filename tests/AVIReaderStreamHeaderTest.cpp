#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderStreamHeaderData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviHeaderFile {
    std::filesystem::path Path;
    explicit AviHeaderFile(const AviReaderTestData &a) {
        static unsigned sequence = 0; char name[96]; std::snprintf(name,sizeof(name),"avi-stream-header-%p-%u.avi",this,++sequence);
        Path = std::filesystem::temp_directory_path()/name; FILE *f = std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if (f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviHeaderFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviHeaderReader {
    CKMovieReader *Reader;
    AviHeaderReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviHeaderReader() { Reader->Release(); }
    CKERROR Open(const AviHeaderFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
};
class AVIReaderStreamHeaderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI stream header",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void ExpectRaw(AviHeaderReader &reader) {
        EXPECT_EQ(3,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(120,reader.Reader->GetMovieLength());
        const CKDWORD first[] = {0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu};
        const CKDWORD last[] = {0xff0000ffu,0xffffffffu,0xffff0000u,0xff00ff00u};
        const int order[] = {2,0,1,2,1,0};
        for (int f:order) {
            CKMovieProperties *p=nullptr; ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(f,&p)); ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data);
            EXPECT_EQ(2,p->m_Format.Width); EXPECT_EQ(2,p->m_Format.Height); EXPECT_EQ(8,p->m_Format.BytesPerLine);
            EXPECT_EQ(0,std::memcmp(p->m_Data,f==2?last:first,sizeof(first))) << "frame " << f;
        }
    }
    void Raw(unsigned size,bool indexed=true,bool records=false) {
        AviReaderTestData a; MakeAviStreamHeader(a,size,indexed,records); AviHeaderFile file(a); AviHeaderReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); ExpectRaw(reader);
    }
};
TEST_F(AVIReaderStreamHeaderTest, Full56ByteHeaderControl) { Raw(56); }
TEST_F(AVIReaderStreamHeaderTest, HeaderWithoutRectangle48Bytes) { Raw(48); }
TEST_F(AVIReaderStreamHeaderTest, CompleteTimingPrefix36Bytes) { Raw(36); }
TEST_F(AVIReaderStreamHeaderTest, HeaderWithBufferHint40Bytes) { Raw(40); }
TEST_F(AVIReaderStreamHeaderTest, HeaderWithQualityHint44Bytes) { Raw(44); }
TEST_F(AVIReaderStreamHeaderTest, OddOptionalTailDoesNotConsumeRiffPadding) { Raw(37); Raw(55); }
TEST_F(AVIReaderStreamHeaderTest, ExtendedHeadersSkipUnknownTail) { Raw(64); Raw(72); }
TEST_F(AVIReaderStreamHeaderTest, ShortIndexedHeaderPreservesPaletteAndDroppedFrame) {
    AviReaderTestData a; MakeAviPaletteSample(a,8,0); ResizeAviStreamHeader(a,36); AviHeaderFile file(a); AviHeaderReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(4,reader.Reader->GetMovieFrameCount());
    CKMovieProperties *p=nullptr; ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(2,&p)); ASSERT_NE(nullptr,p);
    const CKDWORD expected[] = {0xff00ff00u,0xffffff00u,0xffffff00u,0xff00ff00u};
    EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)));
}
TEST_F(AVIReaderStreamHeaderTest, ShortVideo1HeaderPreservesIndependentKeySeek) {
    AviReaderTestData a; MakeAviSeek(a); ResizeAviStreamHeader(a,36); AviHeaderFile file(a); AviHeaderReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(4,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(160,reader.Reader->GetMovieLength());
    CKMovieProperties *p=nullptr; ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(3,&p)); ASSERT_NE(nullptr,p);
    for(int i=0;i<16;++i) EXPECT_EQ(0xff0000ffu,static_cast<CKDWORD*>(p->m_Data)[i]);
}
TEST_F(AVIReaderStreamHeaderTest, ShortYvyuHeaderPreservesFormatAndRowOrder) {
    AviReaderTestData a; MakeAviYvyu(a); ResizeAviStreamHeader(a,48); AviHeaderFile file(a); AviHeaderReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); CKMovieProperties *p=nullptr;
    ASSERT_EQ(CK_OK,reader.Reader->ReadFrame(0,&p)); ASSERT_NE(nullptr,p);
    EXPECT_EQ(0xffffffffu,static_cast<CKDWORD*>(p->m_Data)[0]); EXPECT_EQ(0xffff0000u,static_cast<CKDWORD*>(p->m_Data)[6]);
}
TEST_F(AVIReaderStreamHeaderTest, IndexlessShortHeaderKeepsTimeline) { Raw(48,false); }
TEST_F(AVIReaderStreamHeaderTest, ShortHeaderWithRecordLists) { Raw(36,false,true); }
TEST_F(AVIReaderStreamHeaderTest, AudioBeforeVideoKeepsPhysicalStreamNumbers) {
    AviReaderTestData a; MakeAviStreamHeader(a,48); PrependAviAudioStream(a,36); AviHeaderFile file(a); AviHeaderReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); ExpectRaw(reader);
}
TEST_F(AVIReaderStreamHeaderTest, BothAudioAndVideoMayUseCompletePrefix) {
    AviReaderTestData a; MakeAviStreamHeader(a,36); PrependAviAudioStream(a,36); AviHeaderFile file(a); AviHeaderReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); ExpectRaw(reader);
}
TEST_F(AVIReaderStreamHeaderTest, CompleteClockAndStartFieldsRemainIntact) {
    AviReaderTestData a; MakeAviStreamHeader(a,36); const unsigned at=AviHeaderFind(a,"strh");
    a.Word(at+28,2); a.Word(at+32,50); a.Word(at+36,17);
    AviHeaderFile file(a); AviHeaderReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); ExpectRaw(reader);
}
TEST_F(AVIReaderStreamHeaderTest, IncompleteLengthFieldIsNotAnOptionalTail) {
    AviReaderTestData a; MakeAviStreamHeader(a,35); AviHeaderFile file(a); AviHeaderReader reader;
    EXPECT_EQ(CKMOVIEERROR_UNSUPPORTEDFILE,reader.Open(file));
}
TEST_F(AVIReaderStreamHeaderTest, ReopenAcrossHeaderSizesReplacesParserState) {
    AviHeaderReader reader; const unsigned sizes[]={56,36,72};
    for (unsigned size:sizes) { AviReaderTestData a; MakeAviStreamHeader(a,size); AviHeaderFile file(a); ASSERT_EQ(CK_OK,reader.Open(file)); ExpectRaw(reader); }
}
TEST_F(AVIReaderStreamHeaderTest, BitmapConsumerReceivesShortHeaderImages) {
    AviReaderTestData a; MakeAviStreamHeader(a,36); AviHeaderFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0)); ASSERT_TRUE(bitmap.SetCurrentSlot(2)); EXPECT_EQ(0xffff0000u,bitmap.GetPixel(0,0));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0));
}
