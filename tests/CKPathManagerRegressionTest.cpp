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
    sprintf_s(buffer, "%s_%d", prefix, counter);
    return XString(buffer);
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

    FILE *created = fopen(absoluteFilePath, "wb");
    ASSERT_NE(nullptr, created);
    fputs("ok", created);
    fclose(created);

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
    ASSERT_TRUE(VxMakePath(directoryPath, VxGetTempPath().Str(), uniqueName.Str()));
    ASSERT_TRUE(VxMakeDirectory(directoryPath));

    XString fileName = uniqueName + ".tmp";
    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, directoryPath, fileName.Str()));

    FILE *created = fopen(absoluteFilePath, "wb");
    ASSERT_NE(nullptr, created);
    fputs("ok", created);
    fclose(created);

    XString categoryName = uniqueName + "Category";
    const int categoryIdx = pathManager->AddCategory(categoryName);
    ASSERT_GE(categoryIdx, 0);

    XString categoryPath = directoryPath;
    ASSERT_GE(pathManager->AddPath(categoryIdx, categoryPath), 0);

    XString resolvedFile = fileName;
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(resolvedFile, categoryIdx, -1));
    EXPECT_TRUE(resolvedFile == XString(absoluteFilePath));

    EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryIdx));
    EXPECT_TRUE(VxDeleteDirectory(directoryPath));
}

#ifndef _WIN32
TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsWindowsStyleRelativeSubdirectories) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    const XString uniqueName = MakeUniqueName("CKPathManagerWindowsStyleSubdir");
    char rootPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(rootPath, VxGetTempPath().Str(), uniqueName.Str()));
    ASSERT_TRUE(VxMakeDirectory(rootPath));

    char nestedPath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(nestedPath, rootPath, "3D Entities"));
    ASSERT_TRUE(VxMakeDirectory(nestedPath));

    char absoluteFilePath[_MAX_PATH] = {};
    ASSERT_TRUE(VxMakePath(absoluteFilePath, nestedPath, "Menu.nmo"));

    FILE *created = fopen(absoluteFilePath, "wb");
    ASSERT_NE(nullptr, created);
    fputs("ok", created);
    fclose(created);

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
#endif

TEST_F(CKRuntimeFixture, ResolveFileNameAcceptsUncPathWithoutExistenceCheck) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString missingUncPath = "\\\\";
    EXPECT_EQ(CK_OK, pathManager->ResolveFileName(missingUncPath, DATA_PATH_IDX, -1));
}
