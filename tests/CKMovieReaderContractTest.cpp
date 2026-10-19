#include <gtest/gtest.h>
#include <cstddef>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <type_traits>
#include "CKAll.h"
#include "CKMovieReader.h"
#include "../../Plugins/AVIReader/AVIReader.h"
// Exercise the current host consumer, not the copy inside the fixed SDK.
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization path"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct MovieReaderOwner {
    CKMovieReader *Reader;
    MovieReaderOwner() { CKGet_AviReader_PluginInfo(0); Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0)); }
    ~MovieReaderOwner() { if (Reader) Reader->Release(); }
};
struct MovieContractFile {
    std::filesystem::path Path;
    MovieContractFile() {
        static unsigned sequence = 0; char name[80]; std::snprintf(name,sizeof(name),"ck-movie-contract-%p-%u.avi",this,++sequence);
        Path = std::filesystem::temp_directory_path() / name;
    }
    ~MovieContractFile() { std::error_code ignored; std::filesystem::remove(Path,ignored); }
    void Write(const CKBYTE *data, size_t size) const {
        FILE *f = std::fopen(Path.string().c_str(),"wb"); ASSERT_NE(nullptr,f);
        EXPECT_EQ(size,std::fwrite(data,1,size,f)); EXPECT_EQ(0,std::fclose(f));
    }
};
static void MovieWord(CKBYTE *bytes, int offset, unsigned value, int count = 4) {
    for (int i = 0; i < count; ++i) bytes[offset + i] = CKBYTE(value >> (i * 8));
}
static void MovieMakeAvi(CKBYTE (&bytes)[312]) {
    // Two independent 2x2 BGR24 frames at25fps, bottom-up rows with padding.
    // Hand-authored RIFF/AVI/index bytes; no production writer or parser builds expected data.
    memset(bytes,0,sizeof(bytes)); memcpy(bytes,"RIFF",4); MovieWord(bytes,4,304); memcpy(bytes+8,"AVI ",4);
    memcpy(bytes+12,"LIST",4); MovieWord(bytes,16,192); memcpy(bytes+20,"hdrl",4);
    memcpy(bytes+24,"avih",4); MovieWord(bytes,28,56); MovieWord(bytes,32,40000); MovieWord(bytes,44,0x10);
    MovieWord(bytes,48,2); MovieWord(bytes,56,1); MovieWord(bytes,64,2); MovieWord(bytes,68,2);
    memcpy(bytes+88,"LIST",4); MovieWord(bytes,92,116); memcpy(bytes+96,"strl",4);
    memcpy(bytes+100,"strh",4); MovieWord(bytes,104,56); memcpy(bytes+108,"vids",4);
    MovieWord(bytes,128,1); MovieWord(bytes,132,25); MovieWord(bytes,140,2); MovieWord(bytes,144,16);
    MovieWord(bytes,160,2,2); MovieWord(bytes,162,2,2);
    memcpy(bytes+164,"strf",4); MovieWord(bytes,168,40); MovieWord(bytes,172,40); MovieWord(bytes,176,2); MovieWord(bytes,180,2);
    MovieWord(bytes,184,1,2); MovieWord(bytes,186,24,2); MovieWord(bytes,192,16);
    memcpy(bytes+212,"LIST",4); MovieWord(bytes,216,52); memcpy(bytes+220,"movi",4);
    memcpy(bytes+224,"00db",4); MovieWord(bytes,228,16);
    const CKBYTE first[] = {0x11,0x22,0x33,0x44,0x55,0x66,0xee,0xee,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xdd};
    memcpy(bytes+232,first,16); memcpy(bytes+248,"00db",4); MovieWord(bytes,252,16);
    const CKBYTE second[] = {1,2,3,4,5,6,0xee,0xee,7,8,9,10,11,12,0xdd,0xdd}; memcpy(bytes+256,second,16);
    memcpy(bytes+272,"idx1",4); MovieWord(bytes,276,32);
    memcpy(bytes+280,"00db",4); MovieWord(bytes,284,0x10); MovieWord(bytes,288,4); MovieWord(bytes,292,16);
    memcpy(bytes+296,"00db",4); MovieWord(bytes,300,0x10); MovieWord(bytes,304,28); MovieWord(bytes,308,16);
}
static CKERROR MovieOpen(CKMovieReader *reader, const MovieContractFile &file) {
    return reader->OpenFile(const_cast<char *>(file.Path.string().c_str()));
}
static void MovieExpected(CKMovieProperties *p, int frame) {
    ASSERT_NE(nullptr,p); ASSERT_NE(nullptr,p->m_Data); EXPECT_GE(p->m_Size,int(sizeof(CKMovieProperties)));
    EXPECT_EQ(CKGUID(0x67541bfe,0x75e510c0),p->m_ReaderGuid); EXPECT_STREQ("avi",p->m_Ext.m_Data);
    EXPECT_EQ(sizeof(VxImageDescEx),p->m_Format.Size); EXPECT_EQ(2,p->m_Format.Width); EXPECT_EQ(2,p->m_Format.Height);
    EXPECT_EQ(32,p->m_Format.BitsPerPixel); EXPECT_EQ(8,p->m_Format.BytesPerLine);
    const CKBYTE expected[][16] = {{0x11,0x22,0x33,255,0x44,0x55,0x66,255,0x77,0x88,0x99,255,0xaa,0xbb,0xcc,255},
                                  {1,2,3,255,4,5,6,255,7,8,9,255,10,11,12,255}};
    EXPECT_EQ(0,memcmp(expected[frame],p->m_Data,16));
}
class CKMovieReaderContractTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK,CKStartUp());
        ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("Movie contract actual AVI",CKGet_AviReader_PluginInfoCount,CKGet_AviReader_PluginInfo,CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
};
TEST_F(CKMovieReaderContractTest, PropertiesLayoutMatchesNativeFieldsAndPortablePointerSize) {
    EXPECT_TRUE(std::is_standard_layout<CKMovieProperties>::value); EXPECT_FALSE(std::is_polymorphic<CKMovieProperties>::value);
    EXPECT_EQ(0u,offsetof(CKMovieProperties,m_Size)); EXPECT_EQ(4u,offsetof(CKMovieProperties,m_ReaderGuid));
    EXPECT_EQ(12u,offsetof(CKMovieProperties,m_Ext)); EXPECT_EQ(16u,offsetof(CKMovieProperties,m_Format));
    EXPECT_EQ(sizeof(void*)==4?68u:80u,offsetof(CKMovieProperties,m_Data)); EXPECT_EQ(sizeof(void*)==4?72u:88u,sizeof(CKMovieProperties));
    EXPECT_EQ(sizeof(void*),alignof(CKMovieProperties));
}
TEST_F(CKMovieReaderContractTest, PropertiesConstructorInitializesSemanticFieldsOverDirtyStorage) {
    alignas(CKMovieProperties) CKBYTE storage[sizeof(CKMovieProperties)]; memset(storage,0xa5,sizeof(storage));
    CKMovieProperties *p = new(storage) CKMovieProperties;
    EXPECT_EQ(sizeof(CKMovieProperties),p->m_Size); EXPECT_EQ(nullptr,p->m_Data); EXPECT_EQ(CKGUID(),p->m_ReaderGuid);
    EXPECT_EQ(0,p->m_Ext.m_Data[0]); EXPECT_EQ(sizeof(VxImageDescEx),p->m_Format.Size); EXPECT_EQ(0u,p->m_Format.Flags);
    EXPECT_EQ(0,p->m_Format.Width); EXPECT_EQ(0,p->m_Format.Height); EXPECT_EQ(0,p->m_Format.BytesPerLine); EXPECT_EQ(0,p->m_Format.BitsPerPixel);
    EXPECT_EQ(0u,p->m_Format.RedMask); EXPECT_EQ(0u,p->m_Format.GreenMask); EXPECT_EQ(0u,p->m_Format.BlueMask); EXPECT_EQ(0u,p->m_Format.AlphaMask);
    EXPECT_EQ(0,p->m_Format.BytesPerColorEntry); EXPECT_EQ(0,p->m_Format.ColorMapEntries); EXPECT_EQ(nullptr,p->m_Format.ColorMap); EXPECT_EQ(nullptr,p->m_Format.Image);
    p->~CKMovieProperties();
}
TEST_F(CKMovieReaderContractTest, PropertiesCopyValuesAndBorrowPointersWithoutOwnership) {
    EXPECT_TRUE(std::is_trivially_copyable<CKMovieProperties>::value); EXPECT_TRUE(std::is_trivially_destructible<CKMovieProperties>::value);
    CKBYTE pixels[16] = {17}, view[16] = {34}, palette[8] = {51};
    { CKMovieProperties p; p.m_ReaderGuid=CKGUID(1,2);p.m_Ext="avi";p.m_Data=pixels;p.m_Format.Image=view;p.m_Format.ColorMap=palette;p.m_Format.Width=2;
      CKMovieProperties copy(p),assigned;assigned=p;EXPECT_EQ(pixels,copy.m_Data);EXPECT_EQ(view,assigned.m_Format.Image);EXPECT_EQ(palette,copy.m_Format.ColorMap);
      copy.m_Format.Width=9;assigned.m_Ext="xyz";EXPECT_EQ(2,p.m_Format.Width);EXPECT_STREQ("avi",p.m_Ext.m_Data); }
    EXPECT_EQ(17,pixels[0]);EXPECT_EQ(34,view[0]);EXPECT_EQ(51,palette[0]);
}
TEST_F(CKMovieReaderContractTest, DerivedPropertiesMustReportTheirActualOptionBlockSize) {
    struct Options : CKMovieProperties { CKDWORD Extra=0x12345678; Options(){m_Size=sizeof(Options);} } options;
    EXPECT_EQ(sizeof(options),options.m_Size);EXPECT_GT(sizeof(options),sizeof(CKMovieProperties));EXPECT_EQ(0x12345678u,options.Extra);EXPECT_EQ(nullptr,options.m_Data);
}
TEST_F(CKMovieReaderContractTest, ReaderSingleBaseLayoutAndAllPublicMethodSignaturesRemainCompatible) {
    EXPECT_TRUE((std::is_base_of<CKDataReader,CKMovieReader>::value));EXPECT_TRUE(std::is_abstract<CKMovieReader>::value);
    EXPECT_TRUE(std::has_virtual_destructor<CKMovieReader>::value);EXPECT_EQ(sizeof(void*),sizeof(CKMovieReader));EXPECT_EQ(alignof(void*),alignof(CKMovieReader));
    static_assert(std::is_same<decltype(&CKMovieReader::Release),void(CKMovieReader::*)()>::value,"Release ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::GetFlags),CK_DATAREADER_FLAGS(CKMovieReader::*)()>::value,"Flags ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::GetMovieFrameCount),int(CKMovieReader::*)()>::value,"Frame count ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::GetMovieLength),int(CKMovieReader::*)()>::value,"Length ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::OpenFile),CKERROR(CKMovieReader::*)(CKSTRING)>::value,"OpenFile ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::OpenMemory),CKERROR(CKMovieReader::*)(CKSTRING)>::value,"OpenMemory ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::OpenAsynchronousFile),CKERROR(CKMovieReader::*)(CKSTRING)>::value,"Async ABI");
    static_assert(std::is_same<decltype(&CKMovieReader::ReadFrame),CKERROR(CKMovieReader::*)(int,CKMovieProperties**)>::value,"ReadFrame ABI");
    MovieReaderOwner owner;CKDataReader *base=owner.Reader;EXPECT_EQ(static_cast<void*>(base),static_cast<void*>(owner.Reader));EXPECT_EQ(owner.Reader,dynamic_cast<CKMovieReader*>(base));
}
TEST_F(CKMovieReaderContractTest, BaseFlagsAndConcreteFileOnlyCapabilitiesAreDistinguished) {
    MovieReaderOwner owner;EXPECT_EQ(5,owner.Reader->CKMovieReader::GetFlags());CKDataReader *base=owner.Reader;EXPECT_EQ(CK_DATAREADER_FILELOAD,base->GetFlags());
    EXPECT_EQ(CKPLUGIN_MOVIE_READER,base->GetReaderInfo()->m_Type);EXPECT_EQ(0,base->GetOptionsCount());EXPECT_EQ(nullptr,base->GetOptionDescription(0));
}
TEST_F(CKMovieReaderContractTest, FileOpenPublishesFrameCountAndMillisecondLength) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;
    ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));EXPECT_EQ(2,owner.Reader->GetMovieFrameCount());EXPECT_EQ(80,owner.Reader->GetMovieLength());
}
TEST_F(CKMovieReaderContractTest, FrameZeroAndFinalFrameUseZeroBasedIndicesAndRealDecodedPixels) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(0,&p));MovieExpected(p,0);ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(1,&p));MovieExpected(p,1);
}
TEST_F(CKMovieReaderContractTest, OutOfOrderAndRepeatedReadsKeepFrameIdentity) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    const int order[]={1,0,1,1,0};for(int frame:order){CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(frame,&p));MovieExpected(p,frame);}
}
TEST_F(CKMovieReaderContractTest, InvalidFrameIndicesFailAndDoNotPreventALaterValidRead) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    for(int frame:{INT_MIN,-1,2,INT_MAX}){CKMovieProperties *p=nullptr;EXPECT_NE(CK_OK,owner.Reader->ReadFrame(frame,&p));}
    CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(1,&p));MovieExpected(p,1);
}
TEST_F(CKMovieReaderContractTest, ReturnedPropertiesAndPixelsAreReaderOwnedBorrowedViews) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    CKMovieProperties *first=nullptr,*next=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(0,&first));MovieExpected(first,0);
    CKMovieProperties copied(*first);ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(1,&next));MovieExpected(next,1);
    EXPECT_EQ(first,next);EXPECT_EQ(copied.m_Data,next->m_Data);EXPECT_EQ(2,copied.m_Format.Width);EXPECT_EQ(nullptr,next->m_Format.Image);
}
TEST_F(CKMovieReaderContractTest, MemoryOpenIsUnsupportedWithoutDestroyingAnOpenMovie) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    EXPECT_EQ(CKERR_NOTIMPLEMENTED,owner.Reader->OpenMemory("unused.avi"));EXPECT_EQ(2,owner.Reader->GetMovieFrameCount());
    CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(1,&p));MovieExpected(p,1);
}
TEST_F(CKMovieReaderContractTest, AsyncOpenIsUnsupportedWithoutDestroyingAnOpenMovie) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));MovieReaderOwner owner;ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));
    EXPECT_EQ(CKERR_NOTIMPLEMENTED,owner.Reader->OpenAsynchronousFile("unused.avi"));EXPECT_EQ(80,owner.Reader->GetMovieLength());
    CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(0,&p));MovieExpected(p,0);
}
TEST_F(CKMovieReaderContractTest, MalformedOpenCanBeFollowedBySuccessfulReopen) {
    const CKBYTE bad[]={1,2,3,4};MovieContractFile file;file.Write(bad,sizeof(bad));MovieReaderOwner owner;EXPECT_NE(CK_OK,MovieOpen(owner.Reader,file));
    CKBYTE bytes[312];MovieMakeAvi(bytes);file.Write(bytes,sizeof(bytes));ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,file));CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(0,&p));MovieExpected(p,0);
}
TEST_F(CKMovieReaderContractTest, SuccessfulReopenReplacesThePreviousMovieState) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile first,second;first.Write(bytes,sizeof(bytes));bytes[232]=0x55;second.Write(bytes,sizeof(bytes));MovieReaderOwner owner;
    ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,first));ASSERT_EQ(CK_OK,MovieOpen(owner.Reader,second));CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,owner.Reader->ReadFrame(0,&p));ASSERT_NE(nullptr,p);EXPECT_EQ(0x55,static_cast<CKBYTE*>(p->m_Data)[0]);EXPECT_EQ(80,owner.Reader->GetMovieLength());
}
TEST_F(CKMovieReaderContractTest, ReleaseThroughTheBaseInterfaceClosesTheActualFile) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));
    CKMovieReader *reader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0));ASSERT_EQ(CK_OK,MovieOpen(reader,file));CKDataReader *base=reader;base->Release();
    EXPECT_TRUE(std::filesystem::remove(file.Path));
}
TEST_F(CKMovieReaderContractTest, MovieInfoOwnsTheRealReaderAndReleasesItsOpenFile) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));
    {CKMovieInfo info(XString(file.Path.string().c_str()));info.m_MovieReader=static_cast<CKMovieReader*>(CKGet_AviReader_Reader(0));ASSERT_EQ(CK_OK,MovieOpen(info.m_MovieReader,file));}
    EXPECT_TRUE(std::filesystem::remove(file.Path));
}
TEST_F(CKMovieReaderContractTest, BitmapConsumerLoadsAndSelectsFramesFromTheRealMovieReader) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));EXPECT_EQ(2,bitmap.GetSlotCount());EXPECT_EQ(2,bitmap.GetWidth());EXPECT_EQ(2,bitmap.GetHeight());
    EXPECT_EQ(0xff998877u,bitmap.GetPixel(0,0));EXPECT_EQ(0xff332211u,bitmap.GetPixel(0,1));ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(0xff090807u,bitmap.GetPixel(0,0));EXPECT_EQ(0xff030201u,bitmap.GetPixel(0,1));
}
TEST_F(CKMovieReaderContractTest, BitmapKeepsItsCopiedSurfaceWhenTheBorrowedReaderFrameChanges) {
    CKBYTE bytes[312];MovieMakeAvi(bytes);MovieContractFile file;file.Write(bytes,sizeof(bytes));CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));CKMovieReader *reader=bitmap.GetMovieReader();ASSERT_NE(nullptr,reader);
    CKMovieProperties *p=nullptr;ASSERT_EQ(CK_OK,reader->ReadFrame(1,&p));MovieExpected(p,1);EXPECT_NE(bitmap.LockSurfacePtr(0),p->m_Data);
    EXPECT_EQ(0xff998877u,bitmap.GetPixel(0,0));EXPECT_EQ(0xff332211u,bitmap.GetPixel(0,1));
}
TEST_F(CKMovieReaderContractTest, ReaderErrorEnumKeepsTheNativeNumericContract) {
    EXPECT_EQ(1,CKMOVIEERROR_GENERIC);EXPECT_EQ(2,CKMOVIEERROR_READERROR);EXPECT_EQ(3,CKMOVIEERROR_UNSUPPORTEDFILE);
    EXPECT_EQ(4,CKMOVIEERROR_FILECORRUPTED);EXPECT_EQ(5,CKMOVIEERROR_SAVEERROR);EXPECT_EQ(6,CKMOVIEERROR_UNSUPPORTEDSAVEFORMAT);EXPECT_EQ(-7,CKERR_NOTIMPLEMENTED);
}
