#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"

// SDK does not export CKJustFile. This dependency adapter handles only fixture
// ASCII paths; it does not constitute an audit of the CKFile helper.
CKSTRING CKJustFile(CKSTRING path) {
    if (!path) return nullptr;
    CKSTRING name = path;
    for (CKSTRING p = path; *p; ++p) if (*p == '/' || *p == '\\') name = p + 1;
    return name;
}
static CKDWORD Ids[5] = {0x1001,0x1002,0x1003,0x1004,0x1005};
static const CKDWORD Pixels[4] = {0x10203040,0x50607080,0x90a0b0c0,0xd0e0f010};
static const CKBYTE Encoded[5] = {0x31,0x32,0x33,0x34,0x35};
struct BitmapWire {
    int Words[2048] = {}, Count = 0, Last = -1;
    CKDWORD Masks[2048] = {};
    void Int(int x) { ASSERT_LT(Count,2048); Masks[Count]=0xffffffffu; Words[Count++] = x; }
    void Bytes(const void *p, int n) { ASSERT_LE(Count+(n+3)/4,2048); memcpy(Words+Count,p,n); memset(reinterpret_cast<CKBYTE *>(Masks+Count),255,n); Count+=(n+3)/4; }
    void Buffer(const void *p,int n) { Int(n); if(n) Bytes(p,n); }
    void Id(int id) { if(Last>=0) Words[Last+1]=Count; Last=Count; Int(id); Int(0); }
    void String(const char *p) { if(!p) Int(0); else Buffer(p,static_cast<int>(strlen(p))+1); }
    void Raw() {
        Int(32); Int(2); Int(2); Int(A_MASK); Int(R_MASK); Int(G_MASK); Int(B_MASK); Int(0);
        const CKBYTE b[]={0xc0,0x10,0x40,0x80},g[]={0xb0,0xf0,0x30,0x70},r[]={0xa0,0xe0,0x20,0x60},a[]={0x90,0xd0,0x10,0x50};
        Buffer(b,4); Buffer(g,4); Buffer(r,4); Buffer(a,4);
    }
    void Reader(bool alpha=false) { Int(alpha?2:1); Int(0x00686371); Int(0x18710001); Int(1); Buffer(Encoded,5); if(alpha){Int(1);Int(0x22);} }
    void Legacy() { const CKBYTE bytes[]={'C','K','T','G','A',0x31,0x32,0x33,0x34,0x35}; Int(10); Buffer(bytes,10); }
    void Load(CKStateChunk &c) { int envelope[2050]={}; envelope[0]=(7<<16)|(CKCID_TEXTURE<<8); envelope[1]=Count; memcpy(envelope+2,Words,Count*4); ASSERT_TRUE(c.ConvertFromBuffer(envelope)); c.StartRead(); }
    void Expect(CKStateChunk &c) { c.CloseChunk(); ASSERT_EQ(Count*4,c.GetDataSize()); c.StartRead(); for(int i=0;i<Count;++i) EXPECT_EQ(static_cast<CKDWORD>(Words[i])&Masks[i],static_cast<CKDWORD>(c.ReadInt())&Masks[i])<<"word "<<i; }
};
struct ChunkOptions : CKBitmapProperties {
    int Marker=0;
    ChunkOptions() { m_Size=sizeof(ChunkOptions); m_Ext="qch"; m_ReaderGuid=CKGUID(0x18710001,1); }
};
static CKPluginInfo Plugins[4];
static struct ChunkProbe {
    int Created,Released,Saves,Reads,FileReads,Marker,FileWidth;
    bool Unavailable,FailFirstRead,FailFile,SawSlots,SawOptions;
    CKBitmapData *DyingOwner; CKDWORD Saved[4];
    void Reset() { Created=Released=Saves=Reads=FileReads=0;Marker=-1;FileWidth=2;Unavailable=FailFirstRead=FailFile=SawSlots=SawOptions=false;DyingOwner=nullptr;memset(Saved,0,16); }
} Probe;
class ChunkReader : public CKBitmapReader {
public:
    ChunkOptions Props;
    void Release() override { ++Probe.Released; delete this; }
    CKPluginInfo *GetReaderInfo() override { return &Plugins[0]; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    CKBOOL IsAlphaSaved(CKBitmapProperties *) override { return TRUE; }
    void GetBitmapDefaultProperties(CKBitmapProperties **p) override { *p=&Props; }
    void SetBitmapDefaultProperties(CKBitmapProperties *) override {}
    void Image(int width) {
        Props.m_Data=new CKBYTE[16]; memcpy(Props.m_Data,Pixels,16);
        Props.m_Format.Width=Props.m_Format.Height=width; Props.m_Format.BytesPerLine=width*4;Props.m_Format.BitsPerPixel=32;
        Props.m_Format.RedMask=R_MASK;Props.m_Format.GreenMask=G_MASK;Props.m_Format.BlueMask=B_MASK;Props.m_Format.AlphaMask=A_MASK;
    }
    int ReadMemory(void *p,int n,CKBitmapProperties **out) override {
        ++Probe.Reads; EXPECT_EQ(5,n);if(n==5) EXPECT_EQ(0,memcmp(p,Encoded,5));
        if(Probe.FailFirstRead&&Probe.Reads==1)return CKBITMAPERROR_READERROR; Image(2);*out=&Props;return 0;
    }
    int ReadFile(CKSTRING,CKBitmapProperties **out) override {
        ++Probe.FileReads;if(Probe.FailFile)return CKBITMAPERROR_READERROR;Image(Probe.FileWidth);
        for(int i=0;i<4;++i)static_cast<CKDWORD *>(Props.m_Data)[i]=0xff00aa11;*out=&Props;return 0;
    }
    int ReadASynchronousFile(CKSTRING,CKBitmapProperties **) override { ADD_FAILURE();return 1; }
    int SaveFile(CKSTRING,CKBitmapProperties *) override { ADD_FAILURE();return 0; }
    int SaveMemory(void **out,CKBitmapProperties *p) override { ++Probe.Saves;Probe.Marker=static_cast<ChunkOptions *>(p)->Marker;memcpy(Probe.Saved,p->m_Data,16);*out=new CKBYTE[5];memcpy(*out,Encoded,5);return 5; }
    void ReleaseMemory(void *p) override { delete[] static_cast<CKBYTE *>(p); }
};
class ChunkMovieReader : public CKMovieReader {
public:
    CKMovieProperties Props; CKDWORD Pixel=0xff123456;
    void Release() override {
        if(Probe.DyingOwner){Probe.SawSlots=Probe.DyingOwner->m_Slots.Size()==1;CKBitmapProperties *p=Probe.DyingOwner->GetSaveFormat();Probe.SawOptions=p&&static_cast<ChunkOptions *>(p)->Marker==99;}
        ++Probe.Released;delete this;
    }
    CKPluginInfo *GetReaderInfo() override{return &Plugins[3];}
    int GetOptionsCount() override{return 0;}
    CKSTRING GetOptionDescription(int) override{return nullptr;}
    int GetMovieFrameCount() override{return 2;}
    int GetMovieLength() override{return 100;}
    CKERROR OpenFile(CKSTRING) override{return CK_OK;}
    CKERROR OpenMemory(CKSTRING) override{return CKERR_NOTIMPLEMENTED;}
    CKERROR OpenAsynchronousFile(CKSTRING) override{return CKERR_NOTIMPLEMENTED;}
    CKERROR ReadFrame(int,CKMovieProperties **out) override {
        Props.m_Format.Width=Props.m_Format.Height=1;Props.m_Format.BytesPerLine=4;Props.m_Format.BitsPerPixel=32;
        Props.m_Format.RedMask=R_MASK;Props.m_Format.GreenMask=G_MASK;Props.m_Format.BlueMask=B_MASK;Props.m_Format.AlphaMask=A_MASK;Props.m_Data=&Pixel;*out=&Props;return CK_OK;
    }
};
static int PluginCount(){return 4;}
static CKPluginInfo *PluginInfo(int i){return &Plugins[i];}
static CKDataReader *ReaderFactory(int i){if(Probe.Unavailable&&i==0)return nullptr;++Probe.Created;return i==3?static_cast<CKDataReader *>(new ChunkMovieReader):static_cast<CKDataReader *>(new ChunkReader);}
class CKBitmapDataChunkTest : public ::testing::Test {
protected:
    CKContext *Context=nullptr;std::filesystem::path Directory;
    static void SetUpTestSuite(){ASSERT_EQ(CK_OK,CKStartUp());const char *ext[]={"qch","bmp","tga","qmv"};for(int i=0;i<4;++i){Plugins[i].m_GUID=CKGUID(0x18710001,i+1);Plugins[i].m_Extension=ext[i];Plugins[i].m_Type=i==3?CKPLUGIN_MOVIE_READER:CKPLUGIN_BITMAP_READER;}ASSERT_EQ(CK_OK,CKGetPluginManager()->RegisterStaticPlugin("Bitmap chunk fixture",PluginCount,PluginInfo,ReaderFactory));}
    static void TearDownTestSuite(){CKShutdown();}
    void SetUp() override {
        Probe.Reset();ASSERT_EQ(CK_OK,CKCreateContext(&Context,nullptr,0,0));Probe.Reset();
#ifdef _WIN32
        const int pid=_getpid();
#else
        const int pid=getpid();
#endif
        Directory=std::filesystem::temp_directory_path()/("CKBitmapChunk-"+std::to_string(pid))/::testing::UnitTest::GetInstance()->current_test_info()->name();std::filesystem::create_directories(Directory);
    }
    void TearDown() override{CKCloseContext(Context);std::filesystem::remove_all(Directory);}
    XString File(const char *name){XString path((Directory/name).string().c_str());FILE *f=fopen(path.CStr(),"wb");EXPECT_NE(nullptr,f);if(f){fputc(1,f);fclose(f);}return path;}
    void MakePixels(CKBitmapData &b,int n=1){for(int i=0;i<n;++i){ASSERT_TRUE(b.CreateImage(2,2,32,i));for(int p=0;p<4;++p)ASSERT_TRUE(b.SetPixel(p%2,p/2,Pixels[p],i));}}
    void ExpectPixels(CKBitmapData &b,int slot=0){for(int p=0;p<4;++p)EXPECT_EQ(Pixels[p],b.GetPixel(p%2,p/2,slot));}
};
TEST_F(CKBitmapDataChunkTest, DefaultsInitializeAllBitmapAndSavingState){
    CKBitmapData b;EXPECT_EQ(nullptr,b.m_MovieInfo);EXPECT_EQ(0,b.m_Slots.Size());EXPECT_EQ(-1,b.m_Width);EXPECT_EQ(-1,b.m_Height);EXPECT_EQ(0,b.m_CurrentSlot);EXPECT_EQ(0,b.m_PickThreshold);EXPECT_EQ(CKBITMAPDATA_INVALID,b.m_BitmapFlags);EXPECT_EQ(0u,b.m_TransColor);EXPECT_EQ(nullptr,b.GetSaveFormat());EXPECT_EQ(CKTEXTURE_USEGLOBAL,b.GetSaveOptions());
}
TEST_F(CKBitmapDataChunkTest, SaveOptionsStoreExactValuesWithoutChangingOtherState){CKBitmapData b;for(int i=0;i<5;++i){b.SetSaveOptions(static_cast<CK_BITMAP_SAVEOPTIONS>(i));EXPECT_EQ(i,b.GetSaveOptions());}EXPECT_EQ(CKBITMAPDATA_INVALID,b.m_BitmapFlags);EXPECT_EQ(nullptr,b.GetSaveFormat());}
TEST_F(CKBitmapDataChunkTest, SaveFormatCopiesExtendedOptionsAndDetachesTransientPointers){
    CKBitmapData b;ChunkOptions p;p.Marker=1234;CKBYTE pixels[16]={},palette[8]={};p.m_Data=pixels;p.m_Format.Image=pixels;p.m_Format.ColorMap=palette;p.m_Format.ColorMapEntries=2;p.m_Format.BytesPerColorEntry=4;b.SetSaveFormat(&p);
    ASSERT_NE(nullptr,b.GetSaveFormat());EXPECT_NE(&p,b.GetSaveFormat());EXPECT_EQ(1234,static_cast<ChunkOptions *>(b.GetSaveFormat())->Marker);EXPECT_EQ(nullptr,b.GetSaveFormat()->m_Data);EXPECT_EQ(nullptr,b.GetSaveFormat()->m_Format.Image);EXPECT_EQ(nullptr,b.GetSaveFormat()->m_Format.ColorMap);EXPECT_EQ(0,b.GetSaveFormat()->m_Format.ColorMapEntries);EXPECT_EQ(0,b.GetSaveFormat()->m_Format.BytesPerColorEntry);EXPECT_EQ(pixels,p.m_Data);EXPECT_EQ(palette,p.m_Format.ColorMap);p.Marker=5678;EXPECT_EQ(1234,static_cast<ChunkOptions *>(b.GetSaveFormat())->Marker);
}
TEST_F(CKBitmapDataChunkTest, SaveFormatSelfAssignmentPreservesLiveOptions){CKBitmapData b;ChunkOptions p;p.Marker=54321;b.SetSaveFormat(&p);b.SetSaveFormat(b.GetSaveFormat());ASSERT_NE(nullptr,b.GetSaveFormat());EXPECT_EQ(sizeof(ChunkOptions),b.GetSaveFormat()->m_Size);EXPECT_EQ(54321,static_cast<ChunkOptions *>(b.GetSaveFormat())->Marker);}
TEST_F(CKBitmapDataChunkTest, ReplacingAndClearingOptionsDoesNotOwnCallerBuffers){CKBYTE *p=new CKBYTE[16]{0x33};{CKBitmapData b;ChunkOptions o;o.m_Data=p;o.m_Format.Image=p;b.SetSaveFormat(&o);ChunkOptions q;q.Marker=8;b.SetSaveFormat(&q);EXPECT_EQ(0x33,p[0]);b.SetSaveFormat(nullptr);EXPECT_EQ(nullptr,b.GetSaveFormat());b.SetSaveFormat(&o);}EXPECT_EQ(0x33,p[0]);delete[] p;}
TEST_F(CKBitmapDataChunkTest, DestructorReleasesMovieBeforeSlotsAndOptions){ {CKBitmapData b;MakePixels(b);ChunkOptions p;p.Marker=99;b.SetSaveFormat(&p);CKMovieInfo *m=new CKMovieInfo(XString("owner.qmv"));m->m_MovieReader=static_cast<CKMovieReader *>(ReaderFactory(3));b.SetMovieInfo(m);Probe.DyingOwner=&b;}EXPECT_EQ(1,Probe.Released);EXPECT_TRUE(Probe.SawSlots);EXPECT_TRUE(Probe.SawOptions);Probe.DyingOwner=nullptr;}
TEST_F(CKBitmapDataChunkTest, ReaderDumpUsesLocalOptionsAndRestoresBorrowedState){CKBitmapData b;MakePixels(b);ChunkOptions p;p.Marker=777;p.m_Format.Width=19;b.SetSaveFormat(&p);b.SetSaveOptions(CKTEXTURE_IMAGEFORMAT);CKFile f(Context);CKStateChunk c(CKCID_TEXTURE,nullptr);c.StartWrite();ASSERT_TRUE(b.DumpToChunk(&c,Context,&f,Ids));EXPECT_EQ(777,Probe.Marker);EXPECT_EQ(0,memcmp(Pixels,Probe.Saved,16));EXPECT_EQ(nullptr,b.GetSaveFormat()->m_Data);EXPECT_EQ(nullptr,b.GetSaveFormat()->m_Format.Image);EXPECT_EQ(19,b.GetSaveFormat()->m_Format.Width);BitmapWire w;w.Id(Ids[1]);w.Int(1);w.Int(2);w.Int(2);w.Int(32);w.Reader();w.Id(Ids[3]);w.Int(1);w.String("");w.Expect(c);}
TEST_F(CKBitmapDataChunkTest, GlobalReaderOptionsAreChosenAndRestored){CKBitmapData b;MakePixels(b);ChunkOptions local,global;local.Marker=9;global.Marker=888;global.m_Format.Width=23;CKBYTE previous[16]={};global.m_Data=previous;global.m_Format.Image=previous;b.SetSaveFormat(&local);Context->m_GlobalImagesSaveOptions=CKTEXTURE_IMAGEFORMAT;CKBitmapProperties *old=Context->m_GlobalImagesSaveFormat;Context->m_GlobalImagesSaveFormat=&global;CKFile f(Context);CKStateChunk c(CKCID_TEXTURE,nullptr);c.StartWrite();CKBOOL result=b.DumpToChunk(&c,Context,&f,Ids);Context->m_GlobalImagesSaveFormat=old;EXPECT_TRUE(result);EXPECT_EQ(888,Probe.Marker);EXPECT_EQ(previous,global.m_Data);EXPECT_EQ(previous,global.m_Format.Image);EXPECT_EQ(23,global.m_Format.Width);}
