#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#include "CKAll.h"
#include "VxWindowFunctions.h"

static const size_t kLegacyWindowsPathLimit = 260;

class CKRuntimeFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context_, nullptr, 0, 0));
        ASSERT_NE(nullptr, context_);
    }

    static void TearDownTestSuite() {
        if (context_) {
            CKCloseContext(context_);
            context_ = nullptr;
        }
        CKShutdown();
    }

    static CKContext *context_;
};

CKContext *CKRuntimeFixture::context_ = nullptr;

static XString MakeUniqueName(const char *prefix) {
    static int counter = 0;
    char buffer[128] = {};
    ++counter;
    snprintf(buffer, sizeof(buffer), "%s_%d", prefix, counter);
    return XString(buffer);
}

static void ExpectReadableFile(const XString &path) {
    FILE *file = fopen(path.CStr(), "rb");
    ASSERT_NE(nullptr, file);
    fclose(file);
}

static void WriteTestFile(const char *path) {
    FILE *file = fopen(path, "wb");
    ASSERT_NE(nullptr, file);
    fputs("ok", file);
    fclose(file);
}

static XString MakeTestPath(const char *directory, const char *fileName) {
    const size_t directoryLength = directory ? strlen(directory) : 0u;
    const size_t fileNameLength = fileName ? strlen(fileName) : 0u;
    char *buffer = new char[directoryLength + fileNameLength + 2u];
    memset(buffer, 0, directoryLength + fileNameLength + 2u);
    if (!VxMakePath(buffer, directoryLength + fileNameLength + 2u, directory, fileName)) {
        delete[] buffer;
        ADD_FAILURE() << "VxMakePath failed for test path";
        return "";
    }
    XString result(buffer);
    delete[] buffer;
    return result;
}

static XString GetCurrentDirectoryForTest() {
    XString currentDirectory = VxGetCurrentDirectory();
    if (currentDirectory.IsEmpty())
        ADD_FAILURE() << "VxGetCurrentDirectory returned an empty path";
    return currentDirectory;
}

static XString ToLongPath(const XString &path) {
#if defined(_WIN32)
    if (path.Length() >= 4 && strncmp(path.CStr(), "\\\\?\\", 4) == 0)
        return path;

    XString nativePath = path;
    for (int i = 0; i < nativePath.Length(); ++i) {
        if (nativePath[i] == '/')
            nativePath[i] = '\\';
    }

    XString longPath = "\\\\?\\";
    longPath << nativePath.CStr();
    return longPath;
#else
    return path;
#endif
}

static XString BuildLongDirectory(const XString &root, const char *leafName) {
    XString dir = root;
    int index = 0;
    while (MakeTestPath(dir.CStr(), leafName).Length() <= kLegacyWindowsPathLimit + 32) {
        XString segment;
        segment.Format("segment %d_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", index++);
        dir = MakeTestPath(dir.CStr(), segment.CStr());
    }
    return dir;
}

static void CreateTestFile(const XString &path) {
    ASSERT_TRUE(VxCreateFileTree(path.CStr()));
    WriteTestFile(path.CStr());
}

struct ScopedDirectoryCleanup {
    explicit ScopedDirectoryCleanup(const XString &path) : Path(path) {}
    ~ScopedDirectoryCleanup() {
        VxDeleteDirectory(Path.CStr());
    }

    XString Path;
};

TEST_F(CKRuntimeFixture, RenameCategoryAllowsDuplicateCategoryName) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString categoryA = MakeUniqueName("PathManagerCategoryA");
    XString categoryB = MakeUniqueName("PathManagerCategoryB");

    const int categoryAIdx = pathManager->AddCategory(categoryA);
    ASSERT_GE(categoryAIdx, 0);
    const int categoryBIdx = pathManager->AddCategory(categoryB);
    ASSERT_GE(categoryBIdx, 0);

    EXPECT_EQ(CK_OK, pathManager->RenameCategory(categoryBIdx, categoryA));

    XString categoryBName;
    ASSERT_EQ(CK_OK, pathManager->GetCategoryName(categoryBIdx, categoryBName));
    EXPECT_TRUE(categoryBName == categoryA);

    if (categoryAIdx > categoryBIdx) {
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryAIdx));
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryBIdx));
    } else {
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryBIdx));
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryAIdx));
    }
}

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsFileSchemeWithoutExistenceCheck) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString currentDirectory = GetCurrentDirectoryForTest();
    ASSERT_FALSE(currentDirectory.IsEmpty());

    const XString uniqueName = MakeUniqueName("CKPathManagerFileUri");
    XString absoluteFilePath = MakeTestPath(currentDirectory.CStr(), (uniqueName + ".tmp").Str());
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString fileUri = "file://";
    fileUri << absoluteFilePath.CStr();

    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(fileUri, DATA_PATH_IDX, -1));
    XString expectedFileUri = "file://";
    expectedFileUri << absoluteFilePath.CStr();
    EXPECT_TRUE(fileUri == expectedFileUri);

    const int removeResult = remove(absoluteFilePath.CStr());
    EXPECT_EQ(0, removeResult);

    XString missingFileUri = "file://";
    missingFileUri << absoluteFilePath.CStr();
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(missingFileUri, DATA_PATH_IDX, -1));
}

TEST_F(CKRuntimeFixture, ResolveFileNameSearchesAbsoluteCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerAbsoluteCategory");
    XString directoryPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(directoryPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(directoryPath.CStr()));

    XString fileName = uniqueName + ".tmp";
    XString absoluteFilePath = MakeTestPath(directoryPath.CStr(), fileName.Str());
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = directoryPath.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == absoluteFilePath);
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_TRUE(VxDeleteDirectory(directoryPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameSearchesLongAbsoluteCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerLongCategory");
    const XString tempPath = VxGetTempPath();
    const XString root = ToLongPath(MakeTestPath(tempPath.CStr(), uniqueName.CStr()));
    ScopedDirectoryCleanup cleanup(root);

    const char *fileName = "LongResource.nmo";
    const XString longDirectory = BuildLongDirectory(root, fileName);
    const XString longFile = MakeTestPath(longDirectory.CStr(), fileName);

    CreateTestFile(longFile);

    const XString categoryPathText = longDirectory;
    const XString expectedPathText = longFile;
    ASSERT_GT(expectedPathText.Length(), kLegacyWindowsPathLimit);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = categoryPathText.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == expectedPathText);
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
}

TEST_F(CKRuntimeFixture, ResolveFileNameSearchesLongFileSchemeCategoryPathWithSpaces) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerLongFileScheme");
    const XString tempPath = VxGetTempPath();
    const XString root = ToLongPath(MakeTestPath(tempPath.CStr(), uniqueName.CStr()));
    ScopedDirectoryCleanup cleanup(root);

    const char *relativeFile = "3D Entities/Menu File.nmo";
    const XString longDirectory = BuildLongDirectory(root, relativeFile);
    const XString entityDirectory = MakeTestPath(longDirectory.CStr(), "3D Entities");
    const XString longFile = MakeTestPath(entityDirectory.CStr(), "Menu File.nmo");

    CreateTestFile(longFile);

    const XString categoryPathText = longDirectory;
    const XString expectedPathText = longFile;
    ASSERT_GT(expectedPathText.Length(), kLegacyWindowsPathLimit);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = "file://";
    categoryPath << categoryPathText.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "3D Entities\\Menu File.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == expectedPathText);
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
}

#ifndef _WIN32
TEST_F(CKRuntimeFixture, ResolveFileNameMatchesCaseInsensitiveAbsolutePath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerCaseInsensitive");
    XString rootPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(rootPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(rootPath.CStr()));

    XString texturesPath = MakeTestPath(rootPath.CStr(), "Textures");
    ASSERT_FALSE(texturesPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(texturesPath.CStr()));

    XString skyPath = MakeTestPath(texturesPath.CStr(), "Sky");
    ASSERT_FALSE(skyPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(skyPath.CStr()));

    XString absoluteFilePath = MakeTestPath(skyPath.CStr(), "Sky_C_Back.bmp");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString requestedFilePath = MakeTestPath(rootPath.CStr(), "textures/sky/Sky_C_Back.bmp");
    ASSERT_FALSE(requestedFilePath.IsEmpty());
    XString resolvedFile = requestedFilePath.CStr();
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, BITMAP_PATH_IDX, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(skyPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(texturesPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(rootPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameMatchesCaseInsensitiveCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerCaseInsensitiveCategory");
    XString directoryPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(directoryPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(directoryPath.CStr()));

    XString absoluteFilePath = MakeTestPath(directoryPath.CStr(), "Floor_Top_Checkpoint.bmp");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = directoryPath.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "floor_top_Checkpoint.bmp";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(directoryPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameMatchesCaseInsensitiveAbsoluteCategoryRoot) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerCaseInsensitiveRoot");
    XString rootPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(rootPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(rootPath.CStr()));

    XString levelPath = MakeTestPath(rootPath.CStr(), "Level");
    ASSERT_FALSE(levelPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(levelPath.CStr()));

    XString absoluteFilePath = MakeTestPath(levelPath.CStr(), "Level_01.NMO");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = rootPath;
    categoryPath.ToLower();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "level/level_01.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == absoluteFilePath);
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(levelPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(rootPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameMatchesUppercaseLevelExtensionInSubdirectory) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerLevelExtension");
    XString rootPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(rootPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(rootPath.CStr()));

    XString levelPath = MakeTestPath(rootPath.CStr(), "Level");
    ASSERT_FALSE(levelPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(levelPath.CStr()));

    XString absoluteFilePath = MakeTestPath(levelPath.CStr(), "Level_01.NMO");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = rootPath.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "Level\\Level_01.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(levelPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(rootPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameResolvesCaseFromFileSchemeCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerFileSchemeCase");
    XString rootPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(rootPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(rootPath.CStr()));

    XString levelPath = MakeTestPath(rootPath.CStr(), "Level");
    ASSERT_FALSE(levelPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(levelPath.CStr()));

    XString absoluteFilePath = MakeTestPath(levelPath.CStr(), "Level_01.NMO");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = "file://";
    categoryPath << rootPath.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "level/level_01.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == absoluteFilePath);
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(levelPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(rootPath.CStr()));
}
#endif

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsWindowsStyleRelativeSubdirectories) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerWindowsStyleSubdir");
    XString rootPath = MakeTestPath(VxGetTempPath().Str(), uniqueName.CStr());
    ASSERT_FALSE(rootPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(rootPath.CStr()));

    XString nestedPath = MakeTestPath(rootPath.CStr(), "3D Entities");
    ASSERT_FALSE(nestedPath.IsEmpty());
    ASSERT_TRUE(VxMakeDirectory(nestedPath.CStr()));

    XString absoluteFilePath = MakeTestPath(nestedPath.CStr(), "Menu.nmo");
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = rootPath.CStr();
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "3D Entities\\Menu.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == absoluteFilePath);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(nestedPath.CStr()));
    EXPECT_TRUE(VxDeleteDirectory(rootPath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameFindsFileInCurrentDirectory) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString currentDirectory = GetCurrentDirectoryForTest();
    ASSERT_FALSE(currentDirectory.IsEmpty());

    const XString uniqueName = MakeUniqueName("CKPathManagerCurrentDirectory");
    XString fileName = uniqueName + ".tmp";
    XString absoluteFilePath = MakeTestPath(currentDirectory.CStr(), fileName.Str());
    ASSERT_FALSE(absoluteFilePath.IsEmpty());

    WriteTestFile(absoluteFilePath.CStr());

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, DATA_PATH_IDX, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(0, remove(absoluteFilePath.CStr()));
}

TEST_F(CKRuntimeFixture, ResolveFileNameFindsFileInLongCurrentDirectory) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString root = ToLongPath(MakeTestPath(VxGetTempPath().CStr(), "CKPathManagerLongCurrentDirectory"));
    ScopedDirectoryCleanup cleanup(root);

    const XString uniqueName = MakeUniqueName("CKPathManagerLongCurrentDirectory");
    XString fileName = uniqueName + ".tmp";
    const XString longDir = BuildLongDirectory(root, fileName.CStr());
    const XString filePath = MakeTestPath(longDir.CStr(), fileName.CStr());
    CreateTestFile(filePath);

    const XString previousDirectory = VxGetCurrentDirectory();
    if (!VxSetCurrentDirectory(longDir.CStr()))
        GTEST_SKIP() << "Long current directory is not supported by this process";

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, DATA_PATH_IDX, -1));
    VxSetCurrentDirectory(previousDirectory.CStr());

    EXPECT_TRUE(resolvedFile == filePath);
    ExpectReadableFile(resolvedFile);
}

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsUncPathWithoutExistenceCheck) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString missingUncPath = "\\\\";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(missingUncPath, DATA_PATH_IDX, -1));
}
