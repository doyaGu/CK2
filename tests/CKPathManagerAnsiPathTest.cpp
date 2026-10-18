#include "AnsiPathTest.h"

class CKPathManagerAnsiPathTest : public CKAnsiPathTest {};

TEST_F(CKPathManagerAnsiPathTest, WindowsStyleSeparatorsDoNotSplitDbcsCharacters) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory; native << u8"/\u4E57/Sub";
    ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    native << u8"/\u8D44\u6E90.BIN";
    const unsigned char payload[] = {0, 0x81, 0xFF, 0x5C};
    WriteNative(native.CStr(), payload, sizeof(payload));
    CreateContext(); ASSERT_NE(nullptr, Context);
    ASSERT_GE(Context->GetPathManager()->AddPath(DATA_PATH_IDX, Directory), 0);
    XString filename;
    ASSERT_TRUE(EncodeAnsiTestPath(u8"\u4E57\\Sub\\\u8D44\u6E90.BIN", filename));
    ASSERT_EQ(CK_OK, Context->GetPathManager()->ResolveFileName(filename, DATA_PATH_IDX, 0));
    VxMemoryMappedFile mapped(filename.CStr());
    ASSERT_TRUE(mapped.IsValid()); ASSERT_EQ(sizeof(payload), mapped.GetFileSize());
    EXPECT_EQ(0, memcmp(payload, mapped.GetBase(), sizeof(payload)));
}

TEST_F(CKPathManagerAnsiPathTest, CaseInsensitiveSearchDoesNotFoldDbcsTrailBytes) {
    if (!PrepareDirectory()) return;
    XString native = NativeDirectory; native << u8"/\u4E64.BIN";
    const unsigned char payload[] = {0x51};
    WriteNative(native.CStr(), payload, sizeof(payload));
    CreateContext(); ASSERT_NE(nullptr, Context);
    ASSERT_GE(Context->GetPathManager()->AddPath(DATA_PATH_IDX, Directory), 0);
    XString missing;
    // U+4E04 and U+4E64 are CP936 0x8141 and 0x8161, not a case pair.
    ASSERT_TRUE(EncodeAnsiTestPath(u8"\u4E04.bin", missing));
    EXPECT_EQ(CKERR_NOTFOUND, Context->GetPathManager()->ResolveFileName(missing, DATA_PATH_IDX, 0));
    XString requested; ASSERT_TRUE(EncodeAnsiTestPath(u8"\u4E64.bin", requested));
    EXPECT_EQ(CK_OK, Context->GetPathManager()->ResolveFileName(requested, DATA_PATH_IDX, 0));
}

TEST_F(CKPathManagerAnsiPathTest, IncludedFilesWithDifferentDbcsNamesAreNotDuplicates) {
    if (!PrepareDirectory()) return;
    CreateContext(); ASSERT_NE(nullptr, Context);
    XString output; ASSERT_TRUE(VxMakePath(output, Directory.CStr(), "include.cmo"));
    CKFile file(Context); ASSERT_EQ(CK_OK, file.StartSave(output.Str()));
    XString first = NativeDirectory, second = NativeDirectory;
    first << u8"/\u4E04.bin"; second << u8"/\u4E64.bin";
    const unsigned char a = 17, b = 34;
    WriteNative(first.CStr(), &a, 1); WriteNative(second.CStr(), &b, 1);
    XString left, right;
    ASSERT_TRUE(EncodeAnsiTestPath(first.CStr(), left)); ASSERT_TRUE(EncodeAnsiTestPath(second.CStr(), right));
    ASSERT_TRUE(file.IncludeFile(left.Str())); ASSERT_TRUE(file.IncludeFile(right.Str()));
    ASSERT_EQ(2, file.m_IncludedFiles.Size());
    EXPECT_EQ(left, file.m_IncludedFiles[0]); EXPECT_EQ(right, file.m_IncludedFiles[1]);
    EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKPathManagerAnsiPathTest, PluginFilenameCanEndWithADbcsBackslashByte) {
    XString name;
#ifdef _WIN32
    const char *native = u8"\u4E57.dll";
#elif defined(__APPLE__)
    const char *native = u8"\u4E57.dylib";
#else
    const char *native = u8"\u4E57.so";
#endif
    if (!EncodeAnsiTestPath(native, name)) return;
    EXPECT_TRUE(CKPluginManager::IsPluginLibraryFileName(name.CStr()));
}

class RestoreAnsiTestCurrentDirectory {
public:
    explicit RestoreAnsiTestCurrentDirectory(const char *path) : Previous(VxGetCurrentDirectory()) {
        EXPECT_FALSE(Previous.Empty()); EXPECT_TRUE(VxSetCurrentDirectory(path));
    }
    ~RestoreAnsiTestCurrentDirectory() { EXPECT_TRUE(VxSetCurrentDirectory(Previous.CStr())); }
private:
    XString Previous;
};

static SDL_EnumerationResult SDLCALL CountRelativeTempFolders(void *data, const char *, const char *name) {
    if (strncmp(name, "VTmp", 4) == 0) ++*(int *)data;
    return SDL_ENUM_CONTINUE;
}

#ifndef _WIN32
TEST_F(CKPathManagerAnsiPathTest, NativeTempDoesNotDependOnALegacyCodePage) {
    XString native = Root; native << u8"\U0001F642";
    ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    XString root; ASSERT_TRUE(EncodeAnsiTestPath(Root.CStr(), root));
    RestoreAnsiTestCurrentDirectory cwd(root.CStr());
    AnsiTestEnvironment tmp("TMP", native.CStr()), temp("TEMP", native.CStr()), tmpdir("TMPDIR", native.CStr());
    CreateContext(); ASSERT_NE(nullptr, Context);
    XString folder = Context->GetPathManager()->GetVirtoolsTemporaryFolder();
    ASSERT_FALSE(folder.Empty());
    EXPECT_EQ(0, strncmp(folder.CStr(), native.CStr(), native.Length()));
    EXPECT_TRUE(VxDirectoryExists(folder.CStr()));
    CloseContext();
    EXPECT_FALSE(VxDirectoryExists(folder.CStr()));
    int count = 0; ASSERT_TRUE(SDL_EnumerateDirectory(Root.CStr(), CountRelativeTempFolders, &count));
    EXPECT_EQ(0, count);
}
#endif

TEST_F(CKPathManagerAnsiPathTest, FailedTempCreationCanBeRetriedWithoutClaimingOwnership) {
    ASSERT_TRUE(PrepareDirectory("ascii"));
    XString native = NativeDirectory; native << "/blocked";
    const unsigned char byte = 17; WriteNative(native.CStr(), &byte, 1);
    AnsiTestEnvironment tmp("TMP", native.CStr()), temp("TEMP", native.CStr()), tmpdir("TMPDIR", native.CStr());
    CreateContext(); ASSERT_NE(nullptr, Context);
    EXPECT_TRUE(Context->GetPathManager()->GetVirtoolsTemporaryFolder().Empty());
    ASSERT_TRUE(SDL_RemovePath(native.CStr())); ASSERT_TRUE(SDL_CreateDirectory(native.CStr()));
    XString folder = Context->GetPathManager()->GetVirtoolsTemporaryFolder();
    EXPECT_FALSE(folder.Empty()); EXPECT_TRUE(VxDirectoryExists(folder.CStr()));
}

TEST_F(CKPathManagerAnsiPathTest, MissingTempSkipsIncludedExtractionWithoutWritingInCurrentDirectory) {
    ASSERT_TRUE(PrepareDirectory("ascii"));
    XString native = Root; native << "blocked";
    const unsigned char blocker = 23;
    WriteNative(native.CStr(), &blocker, 1);
    XString root; ASSERT_TRUE(EncodeAnsiTestPath(Root.CStr(), root));
    RestoreAnsiTestCurrentDirectory cwd(root.CStr());
    XString source = NativeDirectory; source << "/payload.bin";
    const unsigned char payload[] = {0, 0xFF, 17}; WriteNative(source.CStr(), payload, sizeof(payload));
    XString filename, resource;
    ASSERT_TRUE(VxMakePath(filename, Directory.CStr(), "bundle.cmo"));
    ASSERT_TRUE(EncodeAnsiTestPath(source.CStr(), resource));
    CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); ASSERT_EQ(CK_OK, file.StartSave(filename.Str()));
        ASSERT_TRUE(file.IncludeFile(resource.Str())); ASSERT_EQ(CK_OK, file.EndSave());
    }
    CloseContext();
    AnsiTestEnvironment tmp("TMP", native.CStr()), temp("TEMP", native.CStr()), tmpdir("TMPDIR", native.CStr());
    CreateContext(); ASSERT_NE(nullptr, Context);
    {
        CKFile file(Context); EXPECT_EQ(CK_OK, file.Load(filename.Str(), NULL));
    }
    int count = 0; ASSERT_TRUE(SDL_EnumerateDirectory(Root.CStr(), CountRelativeTempFolders, &count));
    EXPECT_EQ(0, count);
    SDL_PathInfo info; XString relative = Root; relative << "payload.bin";
    EXPECT_FALSE(SDL_GetPathInfo(relative.CStr(), &info));
}
