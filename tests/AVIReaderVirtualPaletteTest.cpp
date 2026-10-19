#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderVirtualPaletteData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviVirtualPaletteFile {
    std::filesystem::path Path;
    AviVirtualPaletteFile(const AviReaderTestData &a) {
        static unsigned sequence=0; char name[80]; std::snprintf(name,sizeof(name),"avi-continuation-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name; FILE *f=std::fopen(Path.string().c_str(),"wb"); EXPECT_NE(nullptr,f);
        if(f) { EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f)); EXPECT_EQ(0,std::fclose(f)); }
    }
    ~AviVirtualPaletteFile() { std::error_code error; std::filesystem::remove(Path,error); }
};
struct AviVirtualPaletteReader {
    CKMovieReader *Reader;
    AviVirtualPaletteReader() { CKGet_AviReader_PluginInfo(0); Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0)); }
    ~AviVirtualPaletteReader() { Reader->Release(); }
    CKERROR Open(const AviVirtualPaletteFile &file) { return Reader->OpenFile(const_cast<char*>(file.Path.string().c_str())); }
    CKMovieProperties *Frame(int frame) { CKMovieProperties *p=nullptr; EXPECT_EQ(CK_OK,Reader->ReadFrame(frame,&p)); return p; }
};
class AVIReaderVirtualPaletteTest : public ::testing::Test {
protected:
    static void SetUpTestSuite(){ASSERT_EQ(CK_OK,CKStartUp());ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI virtual palette",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));}
    static void TearDownTestSuite(){CKShutdown();}
    void Pixels(CKMovieProperties *p,CKDWORD one,CKDWORD two,bool run=false){
        ASSERT_NE(nullptr,p);ASSERT_NE(nullptr,p->m_Data);ASSERT_EQ(2,p->m_Format.Width);ASSERT_EQ(2,p->m_Format.Height);
        const CKDWORD expected[]={one,run?one:two,two,run?two:one};EXPECT_EQ(0,std::memcmp(expected,p->m_Data,sizeof(expected)));
    }
    void Sample(unsigned mode,const unsigned *states,unsigned count,unsigned codec=0,unsigned bpp=8,bool rec=false){
        const CKDWORD first[]={0xff00ff00u,0xff00ff00u,0xffff0000u,0xff808080u,0xffff0000u,0xff112141u,0xff000000u,0xff112141u};
        const CKDWORD second[]={0xff0000ffu,0xffffff00u,0xffffff00u,0xff808080u,0xff808080u,0xff112141u,0xff000000u,0xffffff00u};
        AviVirtualPaletteData a;a.Make(mode,codec,bpp,rec);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));
        ASSERT_EQ(count,reader.Reader->GetMovieFrameCount());EXPECT_EQ(count*40,reader.Reader->GetMovieLength());
        for(unsigned turn=0;turn<2;++turn)for(unsigned i=0;i<count;++i){unsigned f=turn?i:count-1-i;unsigned st=states[f];CKDWORD other=bpp==1&&st==0?0xff000000u:second[st];Pixels(reader.Frame(f),first[st],other,codec!=0);}
    }
};
TEST_F(AVIReaderVirtualPaletteTest,OpenDmlDropHoldsPreviousPalette){const unsigned s[]={0,0,1};Sample(0,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,OpenDmlLeadingDropUsesFirstPhysicalPalette){const unsigned s[]={0,0,0,1};Sample(1,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,OpenDmlConsecutiveAndTrailingDropsKeepState){const unsigned s[]={0,0,0,1,1};Sample(2,s,5);}
TEST_F(AVIReaderVirtualPaletteTest,PhysicalDropUpdatesPaletteBeforeFollowingVirtualDrop){const unsigned s[]={0,1,1,1};Sample(3,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,ReorderedOpenDmlDropHoldsLogicalPredecessorPalette){const unsigned s[]={1,1,0,0};Sample(4,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,OnlyVirtualDropsDisplayOpaqueBlack){const unsigned s[]={6,6};Sample(5,s,2);}
TEST_F(AVIReaderVirtualPaletteTest,AvixGapDoesNotLeakFuturePaletteIntoVirtualDrop){const unsigned s[]={0,0,1,1};Sample(6,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,DuplicatePaletteColorsKeepOriginalIndices){const unsigned s[]={3,3,4};Sample(7,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,PhysicalDropControlUpdatesAtRealOffset){const unsigned s[]={0,1,1};Sample(8,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,MultipleLeadingDropsUseFirstPalette){const unsigned s[]={0,0,0,0,0,1};Sample(9,s,6);}
TEST_F(AVIReaderVirtualPaletteTest,ClassicPaletteEventAppliesBeforeVirtualDrop){const unsigned s[]={0,1,1};Sample(10,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,ClassicLeadingVirtualDropHonorsPaletteEvent){const unsigned s[]={0,0,1,1};Sample(11,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,ClassicConsecutiveDropsKeepEventPalette){const unsigned s[]={0,1,1,1};Sample(12,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,PalettePatchesFollowLogicalIndexOrder){const unsigned s[]={1,2,2};Sample(13,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,ConsecutiveDropsMayHaveDifferentPaletteEvents){const unsigned s[]={0,1,2,2};Sample(14,s,4);}
TEST_F(AVIReaderVirtualPaletteTest,BackwardPaletteOffsetsRetainOtherPartialUpdates){const unsigned s[]={2,1,1};Sample(15,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,PhysicalAndVirtualDropsShareExplicitPaletteTimeline){const unsigned s[]={0,1,1,2,2};Sample(16,s,5);}
TEST_F(AVIReaderVirtualPaletteTest,ClassicWithoutPaletteEntriesHoldsPreviousState){const unsigned s[]={0,0,1};Sample(17,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,ClassicOnlyVirtualDropsDisplayBlack){const unsigned s[]={6,6};Sample(18,s,2);}
TEST_F(AVIReaderVirtualPaletteTest,EditedSubsetRetainsPaletteEventsForRepeatedPictures){const unsigned s[]={2,2,2,1,1};Sample(20,s,5);}
TEST_F(AVIReaderVirtualPaletteTest,Full256PaletteThenPartialUpdate){const unsigned s[]={5,7,7};Sample(21,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,RepeatedPaletteEventConsumesNoTime){const unsigned s[]={0,1,1};Sample(23,s,3);}
TEST_F(AVIReaderVirtualPaletteTest,RleVirtualDropReplaysIndicesUnderHeldPalette){const unsigned s[]={0,0,1};Sample(0,s,3,1,8,false);}
TEST_F(AVIReaderVirtualPaletteTest,RleExplicitPaletteDropRecolorsHistory){const unsigned s[]={0,1,1};Sample(10,s,3,1,8,false);}
TEST_F(AVIReaderVirtualPaletteTest,RleAliasColorsRemainDistinctAfterPalettePatch){const unsigned s[]={3,3,4};Sample(7,s,3,1,8,false);}
TEST_F(AVIReaderVirtualPaletteTest,RleRecordListsPreserveLogicalPaletteEvents){const unsigned s[]={0,1,2,2};Sample(14,s,4,1,8,true);}
TEST_F(AVIReaderVirtualPaletteTest,RawFourBitVirtualDropKeepsPalette){const unsigned s[]={0,0,1};Sample(0,s,3,0,4,false);}
TEST_F(AVIReaderVirtualPaletteTest,RawFourBitClassicDropUsesLogicalPalette){const unsigned s[]={0,1,1};Sample(10,s,3,0,4,false);}
TEST_F(AVIReaderVirtualPaletteTest,RawOneBitVirtualDropKeepsPalette){const unsigned s[]={0,0,1};Sample(0,s,3,0,1,false);}
TEST_F(AVIReaderVirtualPaletteTest,RawOneBitClassicDropUsesLogicalPalette){const unsigned s[]={0,1,1};Sample(10,s,3,0,1,false);}
TEST_F(AVIReaderVirtualPaletteTest,MalformedPalettePayloadIsNotHiddenByVirtualSlots){
    AviVirtualPaletteData a;a.Make(22);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderVirtualPaletteTest,BadRequiredPaletteReferenceFailsInsteadOfGuessing){
    AviVirtualPaletteData a;a.Make(19);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;EXPECT_NE(CK_OK,reader.Open(file));
}
TEST_F(AVIReaderVirtualPaletteTest,BitmapConsumerSeesPaletteOnlyChangeOnVirtualSlot){
    AviVirtualPaletteData a;a.Make(10,1);AviVirtualPaletteFile file(a);CKBitmapData bitmap;ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(3,bitmap.GetSlotCount());ASSERT_TRUE(bitmap.SetCurrentSlot(1));EXPECT_EQ(0xffffff00u,bitmap.GetPixel(0,0));EXPECT_EQ(0xff00ff00u,bitmap.GetPixel(0,1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(0));EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0));
}
TEST_F(AVIReaderVirtualPaletteTest,ReopenReleasesLogicalPaletteSteps){
    AviVirtualPaletteData a;a.Make(15);AviVirtualPaletteFile file(a);AviVirtualPaletteData b;b.Make(0);AviVirtualPaletteFile other(b);
    AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));Pixels(reader.Frame(1),0xff00ff00u,0xffffff00u);ASSERT_EQ(CK_OK,reader.Open(other));Pixels(reader.Frame(1),0xff00ff00u,0xff0000ffu);
}

TEST_F(AVIReaderVirtualPaletteTest,OtherStreamPaletteDoesNotAffectVirtualDrop){
    AviVirtualPaletteData a;a.TwoStreams();AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));
    EXPECT_EQ(3,reader.Reader->GetMovieFrameCount());Pixels(reader.Frame(1),0xff00ff00u,0xffffff00u);Pixels(reader.Frame(0),0xff00ff00u,0xff0000ffu);
    AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));uint32_t colors[256];ASSERT_TRUE(demuxer.GetFramePalette(1,1,colors));EXPECT_EQ(0xffff0000u,colors[1]);EXPECT_EQ(0xff0000ffu,colors[2]);
}
TEST_F(AVIReaderVirtualPaletteTest,FailedPaletteReferenceThenReopenDoesNotRetainSteps){
    AviVirtualPaletteData a;a.Make(19);AviVirtualPaletteFile file(a);AviVirtualPaletteData b;b.Make(15);AviVirtualPaletteFile other(b);
    AviVirtualPaletteReader reader;EXPECT_NE(CK_OK,reader.Open(file));ASSERT_EQ(CK_OK,reader.Open(other));Pixels(reader.Frame(0),0xffff0000u,0xffffff00u);Pixels(reader.Frame(1),0xff00ff00u,0xffffff00u);
}
TEST_F(AVIReaderVirtualPaletteTest,LeadingVirtualUsesFirstPhysicalDropPalette){
    AviVirtualPaletteData a;a.Make(3);a.Word(a.HeaderIndex+32,0);a.Word(a.HeaderIndex+36,0);a.Word(a.HeaderIndex+48,a.Logical[0]);a.Word(a.HeaderIndex+52,a.LogicalSize[0]);a.Word(44,0x30);
    AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));EXPECT_EQ(4,reader.Reader->GetMovieFrameCount());
    const int order[]={0,3,1,2,0};for(int f:order)Pixels(reader.Frame(f),0xff00ff00u,f==2?0xff0000ffu:0xffffff00u);
}

TEST_F(AVIReaderVirtualPaletteTest,BadPaletteReferenceDoesNotDeleteVirtualTimeSlots){
    AviVirtualPaletteData a;a.Make(10);a.Word(a.Classic+48,0xfffffff0u);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));
    EXPECT_EQ(3,reader.Reader->GetMovieFrameCount());EXPECT_EQ(120,reader.Reader->GetMovieLength());CKMovieProperties *p=nullptr;
    EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p));EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(2,&p));Pixels(reader.Frame(0),0xff00ff00u,0xff0000ffu);
}
TEST_F(AVIReaderVirtualPaletteTest,BadOtherStreamPaletteDoesNotPoisonFirstVideo){
    AviVirtualPaletteData a;a.TwoStreams();a.Word(a.Classic+96,0xfffffff0u);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));Pixels(reader.Frame(1),0xff00ff00u,0xffffff00u);
    AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));uint32_t colors[256];EXPECT_FALSE(demuxer.GetFramePalette(1,1,colors));EXPECT_EQ(3,demuxer.GetFrameCount(1));
}
TEST_F(AVIReaderVirtualPaletteTest,FullPaletteAfterBrokenReferenceRestoresRleAndVirtualTimeline){
    AviVirtualPaletteData a;a.Recovery(1);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));EXPECT_EQ(4,reader.Reader->GetMovieFrameCount());EXPECT_EQ(160,reader.Reader->GetMovieLength());
    Pixels(reader.Frame(3),0xffffff00u,0xffff0000u,true);CKMovieProperties *p=nullptr;EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p));
    Pixels(reader.Frame(0),0xff00ff00u,0xff0000ffu,true);EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(2,&p));Pixels(reader.Frame(3),0xffffff00u,0xffff0000u,true);
}

TEST_F(AVIReaderVirtualPaletteTest,Full16ColorPaletteRestoresFourBitVirtualTimeline){
    AviVirtualPaletteData a;a.Recovery(0,4);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));EXPECT_EQ(4,reader.Reader->GetMovieFrameCount());
    CKMovieProperties *p=nullptr;EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p));Pixels(reader.Frame(3),0xffffff00u,0xffff0000u);Pixels(reader.Frame(0),0xff00ff00u,0xff0000ffu);
}
TEST_F(AVIReaderVirtualPaletteTest,FullTwoColorPaletteRestoresOneBitVirtualTimeline){
    AviVirtualPaletteData a;a.Recovery(0,1);AviVirtualPaletteFile file(a);AviVirtualPaletteReader reader;ASSERT_EQ(CK_OK,reader.Open(file));EXPECT_EQ(4,reader.Reader->GetMovieFrameCount());
    CKMovieProperties *p=nullptr;EXPECT_EQ(CKMOVIEERROR_READERROR,reader.Reader->ReadFrame(1,&p));Pixels(reader.Frame(3),0xffffff00u,0xff000000u);Pixels(reader.Frame(0),0xff00ff00u,0xff000000u);
}
