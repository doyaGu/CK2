#include <gtest/gtest.h>

#include <cstdio>

#include "CKAll.h"
#include "VxWindowFunctions.h"

namespace {

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

XString MakeUniqueName(const char *prefix) {
    static int counter = 0;
    char buffer[128] = {};
    ++counter;
    snprintf(buffer, sizeof(buffer), "%s_%d", prefix, counter);
    return XString(buffer);
}

void ExpectReadableFile(const XString &path) {
    FILE *file = fopen(path.CStr(), "rb");
    ASSERT_NE(nullptr, file);
    fclose(file);
}

void WriteTestFile(const char *path) {
    FILE *file = fopen(path, "wb");
    ASSERT_NE(nullptr, file);
    fputs("ok", file);
    fclose(file);
}

} // namespace

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

    char currentDirectory[_MAX_PATH] = {};
    ASSERT_TRUE(VxGetCurrentDirectory(currentDirectory));

    const XString uniqueName = MakeUniqueName("CKPathManagerFileUri");
    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, currentDirectory, (uniqueName + ".tmp").Str()));

    WriteTestFile(absoluteFilePath);

    XString fileUri = "file://";
    fileUri << absoluteFilePath;

    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(fileUri, DATA_PATH_IDX, -1));
    XString expectedFileUri = "file://";
    expectedFileUri << absoluteFilePath;
    EXPECT_TRUE(fileUri == expectedFileUri);

    const int removeResult = remove(absoluteFilePath);
    EXPECT_EQ(0, removeResult);

    XString missingFileUri = "file://";
    missingFileUri << absoluteFilePath;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(missingFileUri, DATA_PATH_IDX, -1));
}

TEST_F(CKRuntimeFixture, ResolveFileNameSearchesAbsoluteCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerAbsoluteCategory");
    char directoryPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(directoryPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(directoryPath));

    XString fileName = uniqueName + ".tmp";
    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, directoryPath, fileName.Str()));

    WriteTestFile(absoluteFilePath);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = directoryPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == XString(absoluteFilePath));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_TRUE(VxDeleteDirectory(directoryPath));
}

#ifndef _WIN32
TEST_F(CKRuntimeFixture, ResolveFileNameMatchesCaseInsensitiveAbsolutePath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerCaseInsensitive");
    char rootPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(rootPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(rootPath));

    char texturesPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(texturesPath, rootPath, "Textures"));
    ASSERT_TRUE(VxMakeDirectory(texturesPath));

    char skyPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(skyPath, texturesPath, "Sky"));
    ASSERT_TRUE(VxMakeDirectory(skyPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, skyPath, "Sky_C_Back.bmp"));

    WriteTestFile(absoluteFilePath);

    char requestedFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(requestedFilePath, rootPath, "textures/sky/Sky_C_Back.bmp"));
    XString resolvedFile = requestedFilePath;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, BITMAP_PATH_IDX, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(0, remove(absoluteFilePath));
    EXPECT_TRUE(VxDeleteDirectory(skyPath));
    EXPECT_TRUE(VxDeleteDirectory(texturesPath));
    EXPECT_TRUE(VxDeleteDirectory(rootPath));
}

TEST_F(CKRuntimeFixture, ResolveFileNameMatchesCaseInsensitiveCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerCaseInsensitiveCategory");
    char directoryPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(directoryPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(directoryPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, directoryPath, "Floor_Top_Checkpoint.bmp"));

    WriteTestFile(absoluteFilePath);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = directoryPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "floor_top_Checkpoint.bmp";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath));
    EXPECT_TRUE(VxDeleteDirectory(directoryPath));
}

TEST_F(CKRuntimeFixture, ResolveFileNameMatchesUppercaseLevelExtensionInSubdirectory) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerLevelExtension");
    char rootPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(rootPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(rootPath));

    char levelPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(levelPath, rootPath, "Level"));
    ASSERT_TRUE(VxMakeDirectory(levelPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, levelPath, "Level_01.NMO"));

    WriteTestFile(absoluteFilePath);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = rootPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "Level\\Level_01.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath));
    EXPECT_TRUE(VxDeleteDirectory(levelPath));
    EXPECT_TRUE(VxDeleteDirectory(rootPath));
}

TEST_F(CKRuntimeFixture, ResolveFileNameResolvesCaseFromFileSchemeCategoryPath) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerFileSchemeCase");
    char rootPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(rootPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(rootPath));

    char levelPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(levelPath, rootPath, "Level"));
    ASSERT_TRUE(VxMakeDirectory(levelPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, levelPath, "Level_01.NMO"));

    WriteTestFile(absoluteFilePath);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = "file://";
    categoryPath << rootPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "level/level_01.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == XString(absoluteFilePath));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath));
    EXPECT_TRUE(VxDeleteDirectory(levelPath));
    EXPECT_TRUE(VxDeleteDirectory(rootPath));
}
#endif

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsWindowsStyleRelativeSubdirectories) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerWindowsStyleSubdir");
    char rootPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(rootPath, VxGetTempPath().Str(), uniqueName.CStr()));
    ASSERT_TRUE(VxMakeDirectory(rootPath));

    char nestedPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(nestedPath, rootPath, "3D Entities"));
    ASSERT_TRUE(VxMakeDirectory(nestedPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, nestedPath, "Menu.nmo"));

    WriteTestFile(absoluteFilePath);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = rootPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = "3D Entities\\Menu.nmo";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == XString(absoluteFilePath));

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_EQ(0, remove(absoluteFilePath));
    EXPECT_TRUE(VxDeleteDirectory(nestedPath));
    EXPECT_TRUE(VxDeleteDirectory(rootPath));
}

TEST_F(CKRuntimeFixture, ResolveFileNameFindsFileInCurrentDirectory) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    char currentDirectory[_MAX_PATH] = {};
    ASSERT_TRUE(VxGetCurrentDirectory(currentDirectory));

    const XString uniqueName = MakeUniqueName("CKPathManagerCurrentDirectory");
    XString fileName = uniqueName + ".tmp";
    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, currentDirectory, fileName.Str()));

    WriteTestFile(absoluteFilePath);

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, DATA_PATH_IDX, -1));
    ExpectReadableFile(resolvedFile);

    EXPECT_EQ(0, remove(absoluteFilePath));
}

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsUncPathWithoutExistenceCheck) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString missingUncPath = "\\\\";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(missingUncPath, DATA_PATH_IDX, -1));
}
