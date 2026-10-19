#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderTimedPaletteData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING){ADD_FAILURE()<<"Unexpected serialization";return nullptr;}
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviTimedFile {
    std::filesystem::path Path;
    AviTimedFile(const AviReaderTestData &a){static unsigned sequence=0;char name[80];std::snprintf(name,sizeof(name),"avi-timed-%p-%u.avi",this,++sequence);Path=std::filesystem::temp_directory_path()/name;
        FILE *f=std::fopen(Path.string().c_str(),"wb");EXPECT_NE(nullptr,f);if(f){EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f));EXPECT_EQ(0,std::fclose(f));}}
    ~AviTimedFile(){std::error_code e;std::filesystem::remove(Path,e);}
};
struct AviTimedReader {
    CKMovieReader *Reader;
    AviTimedReader(){CKGet_AviReader_PluginInfo(0);Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0));}
    ~AviTimedReader(){Reader->Release();}
    CKERROR Open(const AviTimedFile &f){return Reader->OpenFile(const_cast<char*>(f.Path.string().c_str()));}
};
class AVIReaderTimedPaletteTest : public ::testing::Test {
protected:
    static void SetUpTestSuite(){ASSERT_EQ(CK_OK,CKStartUp());ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI timed palette",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));}
    static void TearDownTestSuite(){CKShutdown();}
    void Pixels(CKMovieReader *r,unsigned f,unsigned state,unsigned codec=0,unsigned bpp=8){
        CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,r->ReadFrame(f,&p));ASSERT_NE(nullptr,p);ASSERT_NE(nullptr,p->m_Data);ASSERT_EQ(2,p->m_Format.Width);ASSERT_EQ(2,p->m_Format.Height);
        const CKDWORD one[]={0xff00ff00u,0xff00ff00u,0xffff0000u,0xffff0000u};
        const CKDWORD two[]={0xff0000ffu,0xffffff00u,0xffffff00u,0xff0000ffu};
        CKDWORD a=one[state],b=bpp==1&&(state==0||state==3)?0xff000000u:two[state];
        const CKDWORD expected[]={a,codec?a:b,b,codec?b:a};EXPECT_EQ(0,std::memcmp(expected,p->m_Data,sizeof(expected)));
    }
    void Check(const AviTimedPaletteData &a,const unsigned *states,unsigned codec=0,unsigned bpp=8){
        AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));ASSERT_EQ(a.LogicalCount,r.Reader->GetMovieFrameCount());EXPECT_EQ(40*a.LogicalCount,r.Reader->GetMovieLength());
        for(unsigned turn=0;turn<2;++turn)for(unsigned i=0;i<a.LogicalCount;++i){unsigned f=turn?i:a.LogicalCount-1-i;Pixels(r.Reader,f,states[f],codec,bpp);}
    }
    void Sample(unsigned flags,unsigned codec=0,unsigned bpp=8){const unsigned durations[]={1,1,2},states[]={0,1,2,2};AviTimedPaletteData a;a.Make(flags|DmlVirtual,durations,codec,bpp);Check(a,states,codec,bpp);}
    void Recovery(unsigned bpp,unsigned codec){
        AviTimedPaletteData a;a.Recovery(bpp,codec);AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));
        EXPECT_EQ(4,r.Reader->GetMovieFrameCount());EXPECT_EQ(160,r.Reader->GetMovieLength());Pixels(r.Reader,3,2,codec,bpp);
        CKMovieProperties *p=nullptr;EXPECT_EQ(CKMOVIEERROR_READERROR,r.Reader->ReadFrame(1,&p));EXPECT_EQ(CKMOVIEERROR_READERROR,r.Reader->ReadFrame(2,&p));Pixels(r.Reader,0,0,codec,bpp);Pixels(r.Reader,3,2,codec,bpp);
    }
};
TEST_F(AVIReaderTimedPaletteTest,DirectDurationChangesPaletteOnVirtualSlot){Sample(0);}
TEST_F(AVIReaderTimedPaletteTest,PaletteBeforeVideoKeepsTimedClock){Sample(DmlPaletteFirst);}
TEST_F(AVIReaderTimedPaletteTest,SuperTimedPaletteKeepsVideoIndex){Sample(DmlPaletteSuper|DmlVideoSuper);}
TEST_F(AVIReaderTimedPaletteTest,SplitSuperClockContinuesAcrossChildren){Sample(DmlPaletteSuper|DmlSplit);}
TEST_F(AVIReaderTimedPaletteTest,SplitAvixUsesLogicalTicks){Sample(DmlPaletteSuper|DmlVideoSuper|DmlSplit|DmlAvix);}
TEST_F(AVIReaderTimedPaletteTest,NonzeroBaseDoesNotChangeEventTicks){Sample(DmlBase|DmlPaletteSuper);}
TEST_F(AVIReaderTimedPaletteTest,RleEightBitRecolorsHeldHistoryAtTimedTick){Sample(DmlPaletteSuper,1);}
TEST_F(AVIReaderTimedPaletteTest,RleFourBitRecolorsHeldHistoryAtTimedTick){Sample(0,2,4);}
TEST_F(AVIReaderTimedPaletteTest,RawFourBitTimedPalette){Sample(0,0,4);}
TEST_F(AVIReaderTimedPaletteTest,RawOneBitTimedPalette){Sample(0,0,1);}
TEST_F(AVIReaderTimedPaletteTest,DurationDoesNotFollowReorderedPictureOffsets){
    const unsigned d[]={1,1,1},s[]={0,1,2};AviTimedPaletteData a;a.Make(DmlReverse,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,ZeroDurationPatchesApplyInOrderAtSameTick){
    const unsigned d[]={0,0,3},s[]={2,2,2};AviTimedPaletteData a;a.Make(0,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,BoundaryWithZeroDurationPatchDoesNotAddVideoTime){
    const unsigned d[]={2,0,1},s[]={0,0,2};AviTimedPaletteData a;a.Make(DmlPaletteSuper|DmlSplit,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,PaletteOrderIsIndependentOfPhysicalOffsets){
    const unsigned d[]={1,1,1},s[]={3,0,1};AviTimedPaletteData a;a.Make(DmlPaletteSuper,d);a.Select(0,2);a.Select(1,0);a.Select(2,1);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,RepeatedPaletteReferenceRemainsAnExplicitEvent){
    const unsigned d[]={1,1,1},s[]={0,1,1};AviTimedPaletteData a;a.Make(0,d);a.Select(2,0);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,LargeDurationUsesStreamTicksWithoutExpandingFrames){
    const unsigned d[]={0xffffffffu,1,1},s[]={0,0,0};AviTimedPaletteData a;a.Make(DmlPaletteSuper|DmlSplit,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,DurationSumDoesNotWrapAt32Bits){
    const unsigned d[]={0xffffffffu,2,1},s[]={0,0,0};AviTimedPaletteData a;a.Make(0,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,CompletedTimedPaletteRemainsUntilNextChange){
    const unsigned d[]={0,1,0},s[]={1,2,2};AviTimedPaletteData a;a.Make(0,d);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,ScaleAndStartDoNotTurnTicksIntoMilliseconds){
    const unsigned d[]={2,0,1},s[]={0,0,2};AviTimedPaletteData a;a.Make(0,d);a.Word(128,2);a.Word(132,50);a.Word(136,17);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,ConsistentSuperDurationsAgreeWithChildren){
    const unsigned d[]={1,1,2},s[]={0,1,2,2};AviTimedPaletteData a;a.Make(DmlPaletteSuper|DmlSplit|DmlVirtual,d);a.Word(a.PaletteIndex+44,1);a.Word(a.PaletteIndex+60,3);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,BadOptionalTimedHeaderPreservesPhysicalPalette){
    const unsigned d[]={1,1,1},s[]={2,0,1};AviTimedPaletteData a;a.Make(DmlReverse,d);a.Word(a.Timed[0]+8,2,2);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,UnknownTimedSubtypeDoesNotDestroyVideo){
    const unsigned d[]={1,1,1},s[]={2,0,1};AviTimedPaletteData a;a.Make(DmlReverse,d);a.Word(a.Timed[0]+10,1,1);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,MixedTimedAndUntimedChildrenDoNotInventClock){
    const unsigned d[]={1,1,1},s[]={2,0,1};AviTimedPaletteData a;a.Make(DmlReverse|DmlPaletteSuper|DmlSplit,d);unsigned at=a.Timed[1];a.Word(at+8,2,2);a.Word(at+11,1,1);a.Word(at+40,a.Read(at+44));a.Word(at+44,a.Read(at+48));Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,InconsistentSuperDurationFallsBackWithoutDeletingFrames){
    const unsigned d[]={1,1,1},s[]={2,0,1};AviTimedPaletteData a;a.Make(DmlReverse|DmlPaletteSuper|DmlSplit,d);a.Word(a.PaletteIndex+44,9);Check(a,s);
}
TEST_F(AVIReaderTimedPaletteTest,BadMiddleTimedReferenceReportsReadErrorWithoutDeletingSlots){
    const unsigned d[]={1,1,2};AviTimedPaletteData a;a.Make(DmlVirtual,d);a.Word(a.Row(1),0xfffffff0u);AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));
    EXPECT_EQ(4,r.Reader->GetMovieFrameCount());EXPECT_EQ(160,r.Reader->GetMovieLength());CKMovieProperties *p=nullptr;EXPECT_EQ(CKMOVIEERROR_READERROR,r.Reader->ReadFrame(1,&p));EXPECT_EQ(CKMOVIEERROR_READERROR,r.Reader->ReadFrame(3,&p));Pixels(r.Reader,0,0);
}
TEST_F(AVIReaderTimedPaletteTest,BadFirstTimedReferenceFailsOpenRatherThanGuessing){
    const unsigned d[]={1,1,1};AviTimedPaletteData a;a.Make(0,d);a.Word(a.Row(0),0xfffffff0u);AviTimedFile file(a);AviTimedReader r;EXPECT_NE(CK_OK,r.Open(file));
}
TEST_F(AVIReaderTimedPaletteTest,OtherStreamTimedPaletteDoesNotLeakIntoFirstVideo){
    const unsigned first[]={1,1,1},second[]={2,0,1};AviTimedPaletteData a;a.TwoStreams(first,second);AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));Pixels(r.Reader,2,2);
    AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));uint32_t colors[256];ASSERT_TRUE(demuxer.GetFramePalette(1,1,colors));EXPECT_EQ(0xffffff00u,colors[1]);EXPECT_EQ(0xff0000ffu,colors[2]);
    ASSERT_TRUE(demuxer.GetFramePalette(1,2,colors));EXPECT_EQ(0xffff0000u,colors[1]);EXPECT_EQ(0xff00ff00u,colors[2]);
}
TEST_F(AVIReaderTimedPaletteTest,ReopenReleasesTimedPaletteEvents){
    const unsigned d[]={0,0,3};AviTimedPaletteData a;a.Make(0,d);AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));Pixels(r.Reader,0,2);
    AviOpenDmlPaletteData b;b.Make();AviTimedFile other(b);ASSERT_EQ(CK_OK,r.Open(other));Pixels(r.Reader,0,0);
}
TEST_F(AVIReaderTimedPaletteTest,BitmapConsumerSeesTimedPaletteOnHeldPicture){
    const unsigned d[]={1,1,2};AviTimedPaletteData a;a.Make(DmlVirtual,d,1);AviTimedFile file(a);CKBitmapData b;ASSERT_TRUE(b.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(4,b.GetSlotCount());ASSERT_TRUE(b.SetCurrentSlot(1));EXPECT_EQ(0xffffff00u,b.GetPixel(0,0));EXPECT_EQ(0xff00ff00u,b.GetPixel(0,1));
}
TEST_F(AVIReaderTimedPaletteTest,ZeroOffsetAtActualEmptyPayloadRemainsPhysical){
    AviOpenDmlPaletteData a;a.Make(DmlNoPalette);unsigned empty=a.Entries;a.Frame(nullptr,0);a.End(a.Movi);a.End(0);
    unsigned position=a.Index[empty].Offset+8;a.Word(a.VideoIndex+12,1);a.Word(a.VideoIndex+20,position);a.Word(a.VideoIndex+32,0);a.Word(a.VideoIndex+36,0);a.Word(a.FrameCountAt,1);a.Word(a.StreamCountAt,1);
    AviTimedFile file(a);AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));ASSERT_EQ(1,demuxer.GetFrameCount(0));
    EXPECT_EQ(position,demuxer.GetFrameInfo(0,0)->offset);uint32_t colors[256];ASSERT_TRUE(demuxer.GetFramePalette(0,0,colors));EXPECT_EQ(0xffff0000u,colors[1]);EXPECT_EQ(0xffffff00u,colors[2]);
}
TEST_F(AVIReaderTimedPaletteTest,Full256ColorTimedEventRestoresUnknownRleState){Recovery(8,1);}
TEST_F(AVIReaderTimedPaletteTest,Full16ColorTimedEventRestoresUnknownRleState){Recovery(4,2);}
TEST_F(AVIReaderTimedPaletteTest,FullTwoColorTimedEventRestoresUnknownState){Recovery(1,0);}
TEST_F(AVIReaderTimedPaletteTest,FailedOpenThenReopenClearsTimedSteps){
    const unsigned d[]={0,0,3};AviTimedPaletteData a;a.Make(0,d);a.Word(a.Row(0),0xfffffff0u);AviTimedFile file(a);AviTimedReader r;EXPECT_NE(CK_OK,r.Open(file));
    AviTimedPaletteData b;b.Make(0,d);AviTimedFile other(b);ASSERT_EQ(CK_OK,r.Open(other));Pixels(r.Reader,0,2);
}
TEST_F(AVIReaderTimedPaletteTest,OtherStreamBadTimedReferenceIsLocalReadError){
    const unsigned d[]={1,1,1};AviTimedPaletteData a;a.TwoStreams(d,d);unsigned second=a.PaletteIndex+(a.Movi-88)/2;
    a.Word(second+44,a.Read(a.PaletteIndex+32));AviTimedFile file(a);AviTimedReader r;ASSERT_EQ(CK_OK,r.Open(file));Pixels(r.Reader,1,1);
    AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));uint32_t colors[256];EXPECT_FALSE(demuxer.GetFramePalette(1,1,colors));EXPECT_TRUE(demuxer.GetFramePalette(0,1,colors));EXPECT_EQ(3,demuxer.GetFrameCount(1));
}
