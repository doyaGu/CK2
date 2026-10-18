#include "AnsiPathTest.h"

class CKFileAnsiPathTest : public CKAnsiPathTest {};

TEST_F(CKFileAnsiPathTest, SaveAndReopenChineseFilenamePreservesObjectNameBytes) {
    if (!PrepareDirectory()) return;
    CreateContext(); ASSERT_NE(nullptr, Context);
    const char name[] = "\xD6\xD0\xCE\xC4";
    CKObject *object = Context->CreateObject(CKCID_BEOBJECT, (char *)name);
    ASSERT_NE(nullptr, object);
    XString native = NativeDirectory; native << u8"/\u7EC4\u5408.cmo";
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    {
        CKFile file(Context);
        ASSERT_EQ(CK_OK, file.StartSave(filename.Str()));
        EXPECT_STREQ(filename.CStr(), file.m_FileName);
        file.SaveObject(object);
        ASSERT_EQ(CK_OK, file.EndSave());
        EXPECT_FALSE(Context->m_Saving);
    }
    SDL_PathInfo info;
    ASSERT_TRUE(SDL_GetPathInfo(native.CStr(), &info)); EXPECT_GT(info.size, 64u);
    CloseContext(); CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); CKObjectArray objects;
        ASSERT_EQ(CK_OK, file.Load(filename.Str(), &objects));
        ASSERT_EQ(1, objects.GetCount()); objects.Reset();
        CKObject *loaded = objects.GetData(Context); ASSERT_NE(nullptr, loaded);
        EXPECT_STREQ(name, loaded->GetName());
    }
}

static void SetFileWord(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = (unsigned char)(value >> (8 * i));
}

TEST_F(CKFileAnsiPathTest, LiteralVersionSixCompositionLoadsFromChineseFilename) {
    if (!PrepareDirectory()) return;
    // Original v6 record with a raw CP936 name; authored independently of the writer.
    unsigned char bytes[112] = {};
    memcpy(bytes, "Nemo Fi", 7);
    SetFileWord(bytes, 12, 0x13022002); SetFileWord(bytes, 16, 6);
    SetFileWord(bytes, 32, 48); SetFileWord(bytes, 36, 48);
    SetFileWord(bytes, 44, 1); SetFileWord(bytes, 48, 41); SetFileWord(bytes, 56, 0x01010000);
    SetFileWord(bytes, 64, 41); SetFileWord(bytes, 68, 1); SetFileWord(bytes, 72, 41); SetFileWord(bytes, 76, 32);
    SetFileWord(bytes, 80, (7u << 16) | (CKCID_BEOBJECT << 8) | 9u); SetFileWord(bytes, 84, 6);
    SetFileWord(bytes, 88, 1); SetFileWord(bytes, 96, 5);
    const char name[] = "\xD6\xD0\xCE\xC4";
    memcpy(bytes + 100, name, sizeof(name));
    unsigned int a = 0, b = 0;
    for (int i = 64; i < sizeof(bytes); ++i) { a = (a + bytes[i]) % 65521; b = (b + a) % 65521; }
    SetFileWord(bytes, 8, a | (b << 16));
    XString native = NativeDirectory; native << u8"/\u65E7\u7EC4\u5408.cmo";
    WriteNative(native.CStr(), bytes, sizeof(bytes));
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    CreateContext(); ASSERT_NE(nullptr, Context);
    CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(filename.Str(), &objects));
    ASSERT_EQ(1, objects.GetCount()); objects.Reset();
    ASSERT_NE(nullptr, objects.GetData(Context));
    EXPECT_STREQ(name, objects.GetData(Context)->GetName());
}

#ifdef CK_LIB
// CKBufferParser has no DLL exports; the public load test below also reaches it.
TEST_F(CKFileAnsiPathTest, ExtractFileWritesExactBytesToChineseFilename) {
    if (!PrepareDirectory()) return;
    unsigned char payload[] = {0, 0x81, 0xFF, 0x5C, 17};
    XString native = NativeDirectory; native << u8"/\u8D44\u6E90.bin";
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    CKBufferParser parser(payload, sizeof(payload));
    ASSERT_TRUE(parser.ExtractFile(filename.Str(), sizeof(payload)));
    EXPECT_EQ(sizeof(payload), parser.CursorPos());
    ExpectNativeBytes(native.CStr(), payload, sizeof(payload));
}
#endif

TEST_F(CKFileAnsiPathTest, IncludedResourceExtractsUnderChineseTempAndIsRemovedOnClose) {
    if (!PrepareDirectory()) return;
    AnsiTestEnvironment tmp("TMP", NativeDirectory.CStr());
    AnsiTestEnvironment temp("TEMP", NativeDirectory.CStr());
    AnsiTestEnvironment tmpdir("TMPDIR", NativeDirectory.CStr());
    const unsigned char payload[] = {0, 0x81, 0xFF, 0x5C, 17};
    XString resource = NativeDirectory; resource << u8"/\u8D44\u6E90.bin";
    WriteNative(resource.CStr(), payload, sizeof(payload));
    XString native = NativeDirectory; native << u8"/\u5305.cmo";
    XString source, filename, name;
    ASSERT_TRUE(EncodeAnsiTestPath(resource.CStr(), source));
    ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    ASSERT_TRUE(EncodeAnsiTestPath(u8"\u8D44\u6E90.bin", name));
    CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); ASSERT_EQ(CK_OK, file.StartSave(filename.Str()));
        ASSERT_TRUE(file.IncludeFile(source.Str())); ASSERT_EQ(CK_OK, file.EndSave());
    }
    // Independent native inspection of the uncompressed included-file trailer.
    size_t count = 0; unsigned char *bytes = (unsigned char *)SDL_LoadFile(native.CStr(), &count);
    ASSERT_NE(nullptr, bytes);
    ASSERT_GE(count, 64u);
    unsigned int end = 64;
    for (int i = 0; i < 4; ++i) end += ((unsigned int)bytes[28 + i] + bytes[32 + i]) << (8 * i);
    EXPECT_EQ(end + 4 + name.Length() + 4 + sizeof(payload), count);
    if (end + 4 + name.Length() <= count) EXPECT_EQ(0, memcmp(bytes + end + 4, name.CStr(), name.Length()));
    SDL_free(bytes);
    CloseContext(); CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); ASSERT_EQ(CK_OK, file.Load(filename.Str(), NULL));
    }
    XString folder = Context->GetPathManager()->GetVirtoolsTemporaryFolder();
    XString path; ASSERT_TRUE(VxMakePath(path, folder.CStr(), name.CStr()));
    VxMemoryMappedFile mapped(path.CStr());
    ASSERT_TRUE(mapped.IsValid()); ASSERT_EQ(sizeof(payload), mapped.GetFileSize());
    EXPECT_EQ(0, memcmp(payload, mapped.GetBase(), sizeof(payload)));
    // A returned resource path keeps platform bytes and is rooted under TEMP.
    EXPECT_EQ(0, strncmp(folder.CStr(), Directory.CStr(), Directory.Length()));
    XString nativeFolder = NativeDirectory;
    CKPathSplitter splitter(folder.CStr()); nativeFolder << '/' << splitter.GetName();
    XString nativeResource = nativeFolder; nativeResource << u8"/\u8D44\u6E90.bin";
    ExpectNativeBytes(nativeResource.CStr(), payload, sizeof(payload));
    CloseContext();
    SDL_PathInfo info; EXPECT_FALSE(SDL_GetPathInfo(nativeFolder.CStr(), &info));
}

TEST_F(CKFileAnsiPathTest, AsciiFilenamePreservesWesternAndChineseObjectNameBytes) {
    ASSERT_TRUE(PrepareDirectory("ascii"));
    CreateContext(); ASSERT_NE(nullptr, Context);
    const char *names[] = {"caf\xE9", "\xD6\xD0\xCE\xC4"};
    XString filename; ASSERT_TRUE(VxMakePath(filename, Directory.CStr(), "names.cmo"));
    {
        CKFile file(Context); ASSERT_EQ(CK_OK, file.StartSave(filename.Str()));
        for (int i = 0; i < 2; ++i) {
            CKObject *object = Context->CreateObject(CKCID_BEOBJECT, (char *)names[i]);
            ASSERT_NE(nullptr, object);
            file.SaveObject(object);
        }
        ASSERT_EQ(CK_OK, file.EndSave());
    }
    CloseContext(); CreateContext(); ASSERT_NE(nullptr, Context);
    CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(filename.Str(), &objects));
    ASSERT_EQ(2, objects.GetCount());
    for (int i = 0; i < 2; ++i) {
        CKObject *object = Context->GetObjectByName((char *)names[i]);
        ASSERT_NE(nullptr, object);
        EXPECT_STREQ(names[i], object->GetName());
    }
}

#ifndef _WIN32
TEST_F(CKFileAnsiPathTest, NativeWesternPathsSaveReopenAndExtractResources) {
    ASSERT_TRUE(PrepareDirectory(u8"Fran\u00E7ois"));
    XString native = NativeDirectory; native << u8"/caf\u00E9.cmo";
    XString resource = NativeDirectory; resource << u8"/Fran\u00E7ois.bin";
    const unsigned char payload[] = {0, 0xE9, 0x81, 0x5C, 0xFF};
    WriteNative(resource.CStr(), payload, sizeof(payload));
    AnsiTestEnvironment tmp("TMP", NativeDirectory.CStr()), temp("TEMP", NativeDirectory.CStr()), tmpdir("TMPDIR", NativeDirectory.CStr());
    CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); ASSERT_EQ(CK_OK, file.StartSave(native.Str()));
        CKObject *object = Context->CreateObject(CKCID_BEOBJECT, (char *)"caf\xE9");
        ASSERT_NE(nullptr, object); file.SaveObject(object);
        ASSERT_TRUE(file.IncludeFile(resource.Str()));
        ASSERT_EQ(CK_OK, file.EndSave());
    }
    ASSERT_TRUE(SDL_RemovePath(resource.CStr()));
    CloseContext(); CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); CKObjectArray objects;
        ASSERT_EQ(CK_OK, file.Load(native.Str(), &objects));
        ASSERT_EQ(1, objects.GetCount()); objects.Reset();
        ASSERT_NE(nullptr, objects.GetData(Context));
        EXPECT_STREQ("caf\xE9", objects.GetData(Context)->GetName());
    }
    XString folder = Context->GetPathManager()->GetVirtoolsTemporaryFolder();
    ASSERT_EQ(0, strncmp(folder.CStr(), NativeDirectory.CStr(), NativeDirectory.Length()));
    XString extracted; ASSERT_TRUE(VxMakePath(extracted, folder.CStr(), u8"Fran\u00E7ois.bin"));
    ExpectNativeBytes(extracted.CStr(), payload, sizeof(payload));
    CloseContext();
    EXPECT_FALSE(VxDirectoryExists(folder.CStr()));
}
#endif
