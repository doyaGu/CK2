#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderPaletteAuthorityData.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE()<<"Unexpected serialization";return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviAuthorityFile {
    std::filesystem::path Path;
    AviAuthorityFile(const AviReaderTestData &data) {
        static unsigned sequence=0;char name[80];
        std::snprintf(name,sizeof(name),"avi-authority-%p-%u.avi",this,++sequence);
        Path=std::filesystem::temp_directory_path()/name;
        FILE *file=std::fopen(Path.string().c_str(),"wb");EXPECT_NE(nullptr,file);
        if(file){EXPECT_EQ(data.Size,std::fwrite(data.Data,1,data.Size,file));EXPECT_EQ(0,std::fclose(file));}
    }
    ~AviAuthorityFile(){std::error_code ignored;std::filesystem::remove(Path,ignored);}
};
struct AviAuthorityReader {
    CKMovieReader *Reader;
    AviAuthorityReader(){CKGet_AviReader_PluginInfo(0);Reader=static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0));}
    ~AviAuthorityReader(){Reader->Release();}
    CKERROR Open(const AviAuthorityFile &file){return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str()));}
};
class AVIReaderPaletteAuthorityTest : public ::testing::Test {
protected:
    static void SetUpTestSuite(){
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("AVI palette authority",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite(){CKShutdown();}
    void Pixels(CKMovieReader *reader,unsigned frame,unsigned picture,unsigned color){
        const unsigned patterns[3][4]={{1,2,2,1},{2,1,1,2},{1,1,2,2}};
        const CKDWORD colors[]={0xffff0000u,0xff00ff00u,0xffffff00u};
        CKMovieProperties *properties=nullptr;ASSERT_EQ(CK_OK,reader->ReadFrame(frame,&properties));
        ASSERT_NE(nullptr,properties);ASSERT_NE(nullptr,properties->m_Data);
        const CKDWORD *pixels=static_cast<const CKDWORD *>(properties->m_Data);
        for(unsigned i=0;i<4;++i)EXPECT_EQ(patterns[picture][i]==1?colors[color]:0xff0000ffu,pixels[i]);
    }
    void Check(unsigned options,bool classic){
        AviPaletteAuthorityData data;data.Make(options);AviAuthorityFile file(data);AviAuthorityReader reader;
        ASSERT_EQ(CK_OK,reader.Open(file));EXPECT_EQ(3,reader.Reader->GetMovieFrameCount());EXPECT_EQ(120,reader.Reader->GetMovieLength());
        for(unsigned turn=0;turn<2;++turn)for(unsigned n=0;n<3;++n){
            unsigned frame=turn?n:2-n,picture=frame;
            if(options&AuthorityReverseDml){const unsigned order[]={2,0,1};picture=order[frame];}
            unsigned color=classic?frame:(frame==0?1:2);
            if(!classic&&(options&AuthorityAlternate))color=frame==0?2:1;
            if(!classic&&(options&AuthoritySecond))color=color==1?2:1;
            Pixels(reader.Reader,frame,picture,color);
        }
    }
};
TEST_F(AVIReaderPaletteAuthorityTest,RequiredClassicPaletteSurvivesIdenticalDirectVideoIndex){Check(AuthorityClassic|AuthorityRequired,true);}
TEST_F(AVIReaderPaletteAuthorityTest,RequiredClassicPaletteSurvivesAlternateTimedPalette){Check(AuthorityClassic|AuthorityRequired|AuthorityAlternate,true);}
TEST_F(AVIReaderPaletteAuthorityTest,RequiredClassicPaletteSurvivesIdenticalSuperVideoIndex){Check(AuthorityClassic|AuthorityRequired|AuthoritySuper,true);}
TEST_F(AVIReaderPaletteAuthorityTest,PaletteHeaderBeforeVideoDoesNotChangeRequiredClassicState){Check(AuthorityClassic|AuthorityRequired|AuthorityPaletteFirst,true);}
TEST_F(AVIReaderPaletteAuthorityTest,TwoTimedListsDoNotOverwriteRequiredClassicEvents){Check(AuthorityClassic|AuthorityRequired|AuthoritySecond,true);}
TEST_F(AVIReaderPaletteAuthorityTest,ReversedTimedHeaderOrderDoesNotOverwriteRequiredClassicEvents){Check(AuthorityClassic|AuthorityRequired|AuthoritySecond|AuthorityAlternate,true);}
TEST_F(AVIReaderPaletteAuthorityTest,NoClassicIndexRetainsExistingSingleTimedBehavior){Check(0,false);}
TEST_F(AVIReaderPaletteAuthorityTest,OptionalClassicIndexDoesNotCreateARequiredPaletteAuthority){Check(AuthorityClassic,false);}
TEST_F(AVIReaderPaletteAuthorityTest,InvalidClassicVideoCannotBindItsPaletteToSelectedDml){Check(AuthorityClassic|AuthorityRequired|AuthorityBadClassic,false);}
TEST_F(AVIReaderPaletteAuthorityTest,PartialClassicTimelineCannotBindItsPaletteToLongerDml){Check(AuthorityClassic|AuthorityRequired|AuthorityShortClassic,false);}
TEST_F(AVIReaderPaletteAuthorityTest,EqualCountsWithDifferentPicturesDoNotSharePaletteSteps){Check(AuthorityClassic|AuthorityRequired|AuthorityDifferentClassic,false);}
TEST_F(AVIReaderPaletteAuthorityTest,DifferentDmlPresentationOrderRetainsItsExistingPaletteClock){Check(AuthorityClassic|AuthorityRequired|AuthorityReverseDml,false);}
TEST_F(AVIReaderPaletteAuthorityTest,ClassicIndexWithoutPaletteEventsDoesNotOverrideTimedList){Check(AuthorityClassic|AuthorityRequired|AuthorityNoClassicPalette,false);}
TEST_F(AVIReaderPaletteAuthorityTest,SuccessfulReopenReplacesThePaletteAuthority){
    AviPaletteAuthorityData first;first.Make(AuthorityClassic|AuthorityRequired);AviAuthorityFile file(first);
    AviPaletteAuthorityData second;second.Make(AuthorityAlternate);AviAuthorityFile other(second);AviAuthorityReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file));Pixels(reader.Reader,0,0,0);
    ASSERT_EQ(CK_OK,reader.Open(other));Pixels(reader.Reader,0,0,2);
}
TEST_F(AVIReaderPaletteAuthorityTest,BitmapConsumerReceivesRequiredClassicPalettePixels){
    AviPaletteAuthorityData data;data.Make(AuthorityClassic|AuthorityRequired|AuthoritySecond);AviAuthorityFile file(data);
    CKBitmapData bitmap;ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));EXPECT_EQ(3,bitmap.GetSlotCount());
    ASSERT_TRUE(bitmap.SetCurrentSlot(0));EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0));EXPECT_EQ(0xffff0000u,bitmap.GetPixel(0,1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(1));EXPECT_EQ(0xff00ff00u,bitmap.GetPixel(0,0));EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,1));
    ASSERT_TRUE(bitmap.SetCurrentSlot(2));EXPECT_EQ(0xff0000ffu,bitmap.GetPixel(0,0));EXPECT_EQ(0xffffff00u,bitmap.GetPixel(0,1));
}
