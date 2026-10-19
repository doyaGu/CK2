#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderMixedIndexData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviMixedIndexFile {
    std::filesystem::path Path;
    AviMixedIndexFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviMixedIndexFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviMixedIndexReader {
    CKMovieReader *Reader;
    AviMixedIndexReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviMixedIndexReader() { Reader->Release(); }
    CKERROR Open(const AviMixedIndexFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};
class AVIReaderMixedIndexTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(CK_OK,CKStartUp()); ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI mixed index",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader)); }
    static void TearDownTestSuite() { CKShutdown(); }
    void Gray(CKMovieProperties *p,unsigned value) {
        ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); ASSERT_EQ(2,p->m_Format.Width); ASSERT_EQ(2,p->m_Format.Height);
        const CKBYTE *bytes=static_cast<const CKBYTE*>(p->m_Data);
        for(int i=0;i<4;++i) { for(int c=0;c<3;++c) EXPECT_EQ(value,bytes[4*i+c]); EXPECT_EQ(255,bytes[4*i+3]); }
    }
    void Sample(unsigned mode) {
        AviMixedIndexData a; a.Make(mode); AviMixedIndexFile file(a); AviMixedIndexReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file)); const int count=mode==8||mode==9?70:mode==13?2:mode==29?4:3;
        EXPECT_EQ(count,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(count*40,reader.Reader->GetMovieLength());
        for(int frame=count-1;frame>=0;--frame) {
            unsigned source=mode==10||mode==28?2-frame:mode==12?(frame==1?0:2):mode==13?(frame?0:2):mode==29?(frame<2?0:frame-1):frame;
            Gray(reader.Frame(frame),(source%7+1)*32);
        }
    }
    void Avix(unsigned mode, unsigned codec=0) {
        AviContinuationData a; a.Make(codec,true);
        if(mode==0)a.Word(a.Super+32+16,0xfffffff0u);
        if(mode==1)a.Word(a.StdIndexes[1]+32+8,0xfffffff0u);
        if(mode==2)a.Word(a.Super+12,1);
        if(mode==3)a.Word(a.Super+32+16,a.StdIndexes[0]);
        if(mode==4)a.Word(a.Super+8,3,2);
        AviMixedIndexFile file(a); AviMixedIndexReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(6,reader.Reader->GetMovieFrameCount()); EXPECT_EQ(240,reader.Reader->GetMovieLength());
        const int order[]={5,0,4,2,1,3,4,0,5};
        for(int frame:order) {
            CKMovieProperties *p=reader.Frame(frame); ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data);
            const CKDWORD one=frame<2?0xffff0000u:frame<4?0xff00ff00u:0xff112141u;
            const CKDWORD other=frame<4?0xff0000ffu:0xffffff00u; const CKDWORD expected[]={one,codec?one:other,other,codec?other:one};
            EXPECT_EQ(0,std::memcmp(p->m_Data,expected,sizeof(expected)))<<"frame "<<frame;
        }
    }
};
TEST_F(AVIReaderMixedIndexTest, ValidClassicControl) { Sample(0); }
TEST_F(AVIReaderMixedIndexTest, CorruptMiddleClassicDoesNotDeleteSample) { Sample(1); }
TEST_F(AVIReaderMixedIndexTest, CorruptLastClassicDoesNotShortenTimeline) { Sample(2); }
TEST_F(AVIReaderMixedIndexTest, ShortClassicSizeFallsBackToWholeChunk) { Sample(3); }
TEST_F(AVIReaderMixedIndexTest, OversizedClassicSizeFallsBackToWholeChunk) { Sample(4); }
TEST_F(AVIReaderMixedIndexTest, DuplicateClassicEntryDoesNotHidePhysicalFrame) { Sample(5); }
TEST_F(AVIReaderMixedIndexTest, PartialClassicIndexRecoversMissingTail) { Sample(6); }
TEST_F(AVIReaderMixedIndexTest, TrailingClassicIndexByteDoesNotHideSamples) { Sample(7); }
TEST_F(AVIReaderMixedIndexTest, FakeMoviHeaderAfter64EntriesIsNotAFrame) { Sample(8); }
TEST_F(AVIReaderMixedIndexTest, MixedAbsoluteRelativeOffsetsAfter64StayCorrect) { Sample(9); }
TEST_F(AVIReaderMixedIndexTest, MustUseClassicPreservesReversedOrder) { Sample(10); }
TEST_F(AVIReaderMixedIndexTest, MustUseClassicPreservesIntentionalRepetition) { Sample(12); }
TEST_F(AVIReaderMixedIndexTest, MustUseClassicPreservesIntentionalSubset) { Sample(13); }
TEST_F(AVIReaderMixedIndexTest, ValidHeaderStandardControl) { Sample(20); }
TEST_F(AVIReaderMixedIndexTest, CorruptStandardEntryFallsBackToClassic) { Sample(21); }
TEST_F(AVIReaderMixedIndexTest, StandardCannotIndexFakeHeaderInsideJunk) { Sample(22); }
TEST_F(AVIReaderMixedIndexTest, ShortStandardSizeFallsBackToWholeChunk) { Sample(23); }
TEST_F(AVIReaderMixedIndexTest, PartialStandardDoesNotHideClassicTail) { Sample(24); }
TEST_F(AVIReaderMixedIndexTest, ShortHeaderIndexRecoversFromMovi) { Sample(25); }
TEST_F(AVIReaderMixedIndexTest, UnknownOptionalIndexTypeRecoversFrames) { Sample(26); }
TEST_F(AVIReaderMixedIndexTest, OverstatedStandardCountFallsBackToClassic) { Sample(27); }
TEST_F(AVIReaderMixedIndexTest, MustUseStandardPreservesReversedOrder) { Sample(28); }
TEST_F(AVIReaderMixedIndexTest, StandardVirtualDroppedSlotKeepsTime) { Sample(29); }
TEST_F(AVIReaderMixedIndexTest, BadAvixSuperReferenceDoesNotLoseSegment) { Avix(0); }
TEST_F(AVIReaderMixedIndexTest, BadAvixStandardEntryDoesNotLoseSlot) { Avix(1); }
TEST_F(AVIReaderMixedIndexTest, PartialAvixSuperIndexDoesNotHideTail) { Avix(2); }
TEST_F(AVIReaderMixedIndexTest, RepeatedAvixStandardReferenceDoesNotDuplicateFrames) { Avix(3); }
TEST_F(AVIReaderMixedIndexTest, MalformedAvixSuperHeaderFallsBackToMovieChunks) { Avix(4); }
TEST_F(AVIReaderMixedIndexTest, InvalidMustUseIndexFailsInsteadOfGuessingOrder) {
    AviMixedIndexData a; a.Make(11); AviMixedIndexFile file(a); AviMixedIndexReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderMixedIndexTest, MissingMustUseIndexFailsInsteadOfGuessingOrder) {
    AviMixedIndexData a; a.Make(14); AviMixedIndexFile file(a); AviMixedIndexReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderMixedIndexTest, OpenDmlAndClassicChooseIndependentlyPerStream) {
    AviMixedIndexData a; a.TwoStreams(); AviMixedIndexFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    ASSERT_EQ(2,demuxer.GetStreamCount()); for(int stream=0;stream<2;++stream) {
        EXPECT_EQ(3,demuxer.GetFrameCount(stream)); for(int f=2;f>=0;--f) { std::vector<uint8_t> bytes; ASSERT_TRUE(demuxer.ReadFrameData(stream,f,bytes)); ASSERT_EQ(16,bytes.size()); EXPECT_EQ((f+1)*32+stream*7,bytes[0]); }
    }
}
TEST_F(AVIReaderMixedIndexTest, WrongOwnerHeaderIndexCannotReturnAnotherStreamsPixels) {
    AviMixedIndexData a; a.TwoStreams(false,true); AviMixedIndexFile file(a); AviMixedIndexReader reader; ASSERT_EQ(CK_OK,reader.Open(file)); Gray(reader.Frame(2),96); Gray(reader.Frame(0),32);
}
TEST_F(AVIReaderMixedIndexTest, CorruptIndexDoesNotPoisonReopen) {
    AviMixedIndexData bad; bad.Make(11); AviMixedIndexFile badFile(bad); AviMixedIndexData good; good.Make(0); AviMixedIndexFile goodFile(good);
    AviMixedIndexReader reader; EXPECT_NE(CK_OK,reader.Open(badFile)); ASSERT_EQ(CK_OK,reader.Open(goodFile)); Gray(reader.Frame(2),96);
}
TEST_F(AVIReaderMixedIndexTest, BitmapConsumerRetainsRecoveredClassicSlots) {
    AviMixedIndexData a; a.Make(1); AviMixedIndexFile file(a); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str()))); EXPECT_EQ(3,bitmap.GetSlotCount());
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(0xff404040u,bitmap.GetPixel(0,0)); ASSERT_TRUE(bitmap.SetCurrentSlot(2)); EXPECT_EQ(0xff606060u,bitmap.GetPixel(1,1));
}

TEST_F(AVIReaderMixedIndexTest, RecoveredAvixRleRetainsDeltaHistoryAndPaletteTiming) { Avix(0,1); }
TEST_F(AVIReaderMixedIndexTest, RecoveredAvixRleRetainsDroppedSlotAfterBadStandardEntry) { Avix(1,1); }
TEST_F(AVIReaderMixedIndexTest, CorruptFirstStreamCanScanWhileSecondUsesClassic) {
    AviMixedIndexData a; a.TwoStreams(true); AviMixedIndexFile file(a); AviDemuxer demuxer; ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    for(int stream=0;stream<2;++stream) { EXPECT_EQ(3,demuxer.GetFrameCount(stream)); std::vector<uint8_t> bytes; ASSERT_TRUE(demuxer.ReadFrameData(stream,2,bytes)); EXPECT_EQ(96+stream*7,bytes[0]); }
}
TEST_F(AVIReaderMixedIndexTest, ActualBadPayloadRemainsAReadErrorWithoutDeletingTime) {
    AviReaderTestData a; a.Start(24,0); const uint8_t good[16]={32,32,32,32,32,32,0,0,32,32,32,32,32,32,0,0};
    a.Frame(good,16); a.Frame(good,8); a.Frame(good,16); a.Finish(); AviMixedIndexFile file(a); AviMixedIndexReader reader; ASSERT_EQ(CK_OK,reader.Open(file));
    EXPECT_EQ(3,reader.Reader->GetMovieFrameCount()); CKMovieProperties *p=nullptr; EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p)); Gray(reader.Frame(2),32); Gray(reader.Frame(0),32);
}
TEST_F(AVIReaderMixedIndexTest, DestroyedMovieChunkBoundaryFailsWithoutGuessingBytes) {
    AviMixedIndexData a; a.Make(0); a.Word(a.Payloads[1]-4,0xfffffff0u); AviMixedIndexFile file(a); AviMixedIndexReader reader; EXPECT_NE(CK_OK,reader.Open(file));
}

TEST_F(AVIReaderMixedIndexTest, MustUseAudioLengthCountsSamplesInsteadOfIndexedChunks) {
    AviMixedIndexData a; a.TwoStreams(false,false,true); a.Word(44,0x30); AviMixedIndexFile file(a); AviDemuxer demuxer;
    ASSERT_TRUE(demuxer.Open(file.Path.string().c_str())); EXPECT_EQ(1,demuxer.FindFirstAudioStream()); EXPECT_EQ(24,demuxer.GetStreamInfo(1)->totalFrames);
    EXPECT_EQ(3,demuxer.GetFrameCount(0)); EXPECT_EQ(3,demuxer.GetFrameCount(1));
    std::vector<uint8_t> bytes; ASSERT_TRUE(demuxer.ReadFrameData(1,2,bytes)); EXPECT_EQ(103,bytes[0]);
}
