#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderOpenDmlPaletteData.h"
#include "../src/CKBitmapData.cpp"
CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization";return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();
struct AviDmlPaletteFile {
    std::filesystem::path Path;
    AviDmlPaletteFile(const AviReaderTestData &a) {
        static unsigned sequence=0;char name[80];std::snprintf(name,sizeof(name),"avi-dml-palette-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name;FILE *f=std::fopen(Path.string().c_str(),"wb");EXPECT_NE(nullptr,f);
        if(f){EXPECT_EQ(a.Size,std::fwrite(a.Data,1,a.Size,f));EXPECT_EQ(0,std::fclose(f));}
    }
    ~AviDmlPaletteFile(){std::error_code e;std::filesystem::remove(Path,e);}
};
struct AviDmlPaletteReader {
    CKMovieReader *Reader;
    AviDmlPaletteReader(){CKGet_AviReader_PluginInfo(0);Reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0));}
    ~AviDmlPaletteReader(){Reader->Release();}
    CKERROR Open(const AviDmlPaletteFile &f){return Reader->OpenFile(const_cast<char*>(f.Path.string().c_str()));}
};
class AVIReaderOpenDmlPaletteTest : public ::testing::Test {
protected:
    static void SetUpTestSuite(){ASSERT_EQ(CK_OK,CKStartUp());ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI DML palette",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));}
    static void TearDownTestSuite(){CKShutdown();}
    void Pixels(CKMovieReader *r,unsigned frame,unsigned state,unsigned codec=0,unsigned bpp=8){
        CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,r->ReadFrame(frame,&p));ASSERT_NE(nullptr,p);ASSERT_NE(nullptr,p->m_Data);
        ASSERT_EQ(2,p->m_Format.Width);ASSERT_EQ(2,p->m_Format.Height);
        CKDWORD one=state==2?0xffff0000u:0xff00ff00u;
        CKDWORD two=state?0xffffff00u:bpp==1?0xff000000u:0xff0000ffu;
        const CKDWORD expected[]={one,codec?one:two,two,codec?two:one};EXPECT_EQ(0,std::memcmp(expected,p->m_Data,sizeof(expected)));
    }
    void Sample(unsigned options,unsigned codec=0,unsigned bpp=8){
        AviOpenDmlPaletteData a;a.Make(options,codec,bpp);AviDmlPaletteFile file(a);AviDmlPaletteReader r;ASSERT_EQ(CK_OK,r.Open(file));
        ASSERT_EQ(a.LogicalCount,r.Reader->GetMovieFrameCount());EXPECT_EQ(40*a.LogicalCount,r.Reader->GetMovieLength());
        const unsigned normal[]={0,1,2},reverse[]={2,0,1},drops[]={0,0,1,2};
        const unsigned *states=(options&DmlVirtual)?drops:(options&DmlReverse)?reverse:normal;
        for(unsigned turn=0;turn<2;++turn)for(unsigned i=0;i<a.LogicalCount;++i){unsigned f=turn?i:a.LogicalCount-1-i;Pixels(r.Reader,f,states[f],codec,bpp);}
    }
};
TEST_F(AVIReaderOpenDmlPaletteTest,SeparateDirectPaletteAfterVideo){Sample(0);}
TEST_F(AVIReaderOpenDmlPaletteTest,SeparateDirectPaletteBeforeVideo){Sample(DmlPaletteFirst);}
TEST_F(AVIReaderOpenDmlPaletteTest,SeparatePaletteDoesNotDestroyEditedVideoOrder){Sample(DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,PaletteFirstPreservesEditedVideoOrder){Sample(DmlPaletteFirst|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,VideoSuperWithDirectPalette){Sample(DmlVideoSuper);}
TEST_F(AVIReaderOpenDmlPaletteTest,VideoDirectWithPaletteSuper){Sample(DmlPaletteSuper);}
TEST_F(AVIReaderOpenDmlPaletteTest,IndependentSuperIndexChains){Sample(DmlVideoSuper|DmlPaletteSuper);}
TEST_F(AVIReaderOpenDmlPaletteTest,PaletteSuperBeforeVideoSuper){Sample(DmlPaletteFirst|DmlVideoSuper|DmlPaletteSuper|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,SplitPaletteIndexSpansAvix){Sample(DmlAvix|DmlVideoSuper|DmlPaletteSuper|DmlSplit);}
TEST_F(AVIReaderOpenDmlPaletteTest,SplitPaletteAndReorderedVideoAcrossAvix){Sample(DmlAvix|DmlVideoSuper|DmlPaletteSuper|DmlSplit|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,DirectIndexesUseNonzeroBase){Sample(DmlBase);}
TEST_F(AVIReaderOpenDmlPaletteTest,SuperChildrenUseNonzeroBase){Sample(DmlBase|DmlVideoSuper|DmlPaletteSuper);}
TEST_F(AVIReaderOpenDmlPaletteTest,RawVirtualDropKeepsUntimedPaletteAnchor){Sample(DmlVirtual);}
TEST_F(AVIReaderOpenDmlPaletteTest,SuperVirtualDropKeepsUntimedPaletteAnchor){Sample(DmlVirtual|DmlVideoSuper|DmlPaletteSuper);}
TEST_F(AVIReaderOpenDmlPaletteTest,RleEightBitPaletteIndexRecolorsHistory){Sample(DmlPaletteSuper|DmlVideoSuper,1);}
TEST_F(AVIReaderOpenDmlPaletteTest,RleFourBitPaletteIndexRecolorsHistory){Sample(DmlPaletteFirst,2,4);}
TEST_F(AVIReaderOpenDmlPaletteTest,RawFourBitDedicatedPalette){Sample(DmlPaletteSuper,0,4);}
TEST_F(AVIReaderOpenDmlPaletteTest,RawOneBitDedicatedPalette){Sample(DmlPaletteSuper,0,1);}
TEST_F(AVIReaderOpenDmlPaletteTest,RleVirtualDropWithDedicatedPalette){Sample(DmlVirtual|DmlPaletteSuper|DmlVideoSuper,1);}
TEST_F(AVIReaderOpenDmlPaletteTest,BadOptionalPaletteReferenceDoesNotEraseVideo){Sample(DmlBadReference|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,BadPaletteChildDoesNotEraseVideo){Sample(DmlBadChild|DmlPaletteSuper|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,DuplicateOptionalPaletteReferencesRecoverPhysicalEvents){Sample(DmlRepeated|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,WrongOwnerPaletteDoesNotEraseVideo){Sample(DmlBadOwner|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,FieldPaletteIndexDoesNotEraseVideo){Sample(DmlFieldPalette|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,EmptyOptionalPaletteIndexDoesNotEraseVideo){Sample(DmlEmptyPalette|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,UnknownPaletteIndexTypeDoesNotEraseVideo){Sample(DmlWrongType|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,TruncatedPaletteIndexDoesNotEraseVideo){Sample(DmlTruncated|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,NoPaletteIndexControl){Sample(DmlNoPalette|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,PaletteIndexDoesNotBecomeVideoTimeline){Sample(DmlNoVideo);}
TEST_F(AVIReaderOpenDmlPaletteTest,ClassicPaletteTimelineDoesNotOverwriteSelectedOpenDml){Sample(DmlClassic|DmlReverse);}
TEST_F(AVIReaderOpenDmlPaletteTest,RequiredBrokenVideoIsNotRepairedByPaletteIndex){
    AviOpenDmlPaletteData a;a.Make(DmlBadVideo);AviDmlPaletteFile file(a);AviDmlPaletteReader r;EXPECT_NE(CK_OK,r.Open(file));
}
TEST_F(AVIReaderOpenDmlPaletteTest,ReopenClearsDedicatedPaletteState){
    AviOpenDmlPaletteData a;a.Make(DmlPaletteSuper|DmlVideoSuper|DmlReverse);AviDmlPaletteFile file(a);
    AviOpenDmlPaletteData b;b.Make(DmlNoPalette);AviDmlPaletteFile other(b);AviDmlPaletteReader r;
    ASSERT_EQ(CK_OK,r.Open(file));Pixels(r.Reader,0,2);ASSERT_EQ(CK_OK,r.Open(other));Pixels(r.Reader,0,0);
}
TEST_F(AVIReaderOpenDmlPaletteTest,FailedOpenThenReopenClearsPendingPaletteIndexes){
    AviOpenDmlPaletteData a;a.Make(DmlPaletteFirst|DmlBadVideo);AviDmlPaletteFile file(a);
    AviOpenDmlPaletteData b;b.Make(DmlPaletteSuper);AviDmlPaletteFile other(b);AviDmlPaletteReader r;
    EXPECT_NE(CK_OK,r.Open(file));ASSERT_EQ(CK_OK,r.Open(other));Pixels(r.Reader,2,2);
}
TEST_F(AVIReaderOpenDmlPaletteTest,BitmapConsumerReadsEditedPaletteFrames){
    AviOpenDmlPaletteData a;a.Make(DmlPaletteSuper|DmlVideoSuper|DmlReverse);AviDmlPaletteFile file(a);CKBitmapData b;
    ASSERT_TRUE(b.LoadMovieFile(XString(file.Path.string().c_str())));EXPECT_EQ(3,b.GetSlotCount());
    ASSERT_TRUE(b.SetCurrentSlot(0));EXPECT_EQ(0xffffff00u,b.GetPixel(0,0));EXPECT_EQ(0xffff0000u,b.GetPixel(0,1));
    ASSERT_TRUE(b.SetCurrentSlot(1));EXPECT_EQ(0xff0000ffu,b.GetPixel(0,0));EXPECT_EQ(0xff00ff00u,b.GetPixel(0,1));
}
TEST_F(AVIReaderOpenDmlPaletteTest,SeparateStreamsKeepTheirOwnPaletteLists){
    AviOpenDmlPaletteData a;a.TwoStreams();AviDmlPaletteFile file(a);AviDmlPaletteReader r;ASSERT_EQ(CK_OK,r.Open(file));
    Pixels(r.Reader,2,2);Pixels(r.Reader,0,0);AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    EXPECT_EQ(3,demuxer.GetFrameCount(0));EXPECT_EQ(3,demuxer.GetFrameCount(1));uint32_t colors[256];
    ASSERT_TRUE(demuxer.GetFramePalette(1,0,colors));EXPECT_EQ(0xffffff00u,colors[1]);EXPECT_EQ(0xff0000ffu,colors[2]);
    ASSERT_TRUE(demuxer.GetFramePalette(1,2,colors));EXPECT_EQ(0xffff0000u,colors[1]);EXPECT_EQ(0xff00ff00u,colors[2]);
}
TEST_F(AVIReaderOpenDmlPaletteTest,OtherStreamBadPaletteReferenceRemainsLocal){
    AviOpenDmlPaletteData a;a.TwoStreams();unsigned clone=a.Movi-88;clone/=2;
    a.Word(a.PaletteIndex+clone+32,a.Read(a.PaletteIndex+32));AviDmlPaletteFile file(a);AviDmlPaletteReader r;ASSERT_EQ(CK_OK,r.Open(file));
    Pixels(r.Reader,1,1);AviDemuxer demuxer;ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));uint32_t colors[256];
    ASSERT_TRUE(demuxer.GetFramePalette(1,1,colors));EXPECT_EQ(0xffffff00u,colors[1]);EXPECT_EQ(0xff00ff00u,colors[2]);
}
TEST_F(AVIReaderOpenDmlPaletteTest,BadFinalPaletteSegmentKeepsEntireVideoTimeline){
    AviOpenDmlPaletteData a;a.Make(DmlPaletteSuper|DmlVideoSuper|DmlSplit|DmlAvix|DmlReverse);
    unsigned last=a.Read(a.PaletteIndex+48);a.Word(last+32,a.Read(last+32)+1);AviDmlPaletteFile file(a);AviDmlPaletteReader r;
    ASSERT_EQ(CK_OK,r.Open(file));EXPECT_EQ(3,r.Reader->GetMovieFrameCount());EXPECT_EQ(120,r.Reader->GetMovieLength());
    Pixels(r.Reader,0,2);Pixels(r.Reader,2,1);Pixels(r.Reader,1,0);
}
TEST_F(AVIReaderOpenDmlPaletteTest,DedicatedIndexDoesNotHideMalformedActualPalette){
    AviOpenDmlPaletteData a;a.Make(DmlPaletteSuper);a.Data[a.Index[a.Changes[0]].Offset+9]=255;
    AviDmlPaletteFile file(a);AviDmlPaletteReader r;EXPECT_NE(CK_OK,r.Open(file));
}
