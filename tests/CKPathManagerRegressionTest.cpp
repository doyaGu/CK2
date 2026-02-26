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

TEST_F(CKRuntimeFixture, ResolveFileNameRejectsStartIndexBelowMinusOne) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString fileName = "missing_file.dat";
    EXPECT_EQ(CKERR_INVALIDPARAMETER, pathManager->ResolveFileName(fileName, DATA_PATH_IDX, -2));
}

TEST_F(CKRuntimeFixture, RenameCategoryRejectsDuplicateCategoryName) {
    CKPathManager *pathManager = context_->GetPathManager();
    ASSERT_NE(nullptr, pathManager);

    XString categoryA = MakeUniqueName("PathManagerCategoryA");
    XString categoryB = MakeUniqueName("PathManagerCategoryB");

    const int categoryAIdx = pathManager->AddCategory(categoryA);
    ASSERT_GE(categoryAIdx, 0);
    const int categoryBIdx = pathManager->AddCategory(categoryB);
    ASSERT_GE(categoryBIdx, 0);

    EXPECT_EQ(CKERR_ALREADYPRESENT, pathManager->RenameCategory(categoryBIdx, categoryA));

    XString categoryBName;
    ASSERT_EQ(CK_OK, pathManager->GetCategoryName(categoryBIdx, categoryBName));
    EXPECT_TRUE(categoryBName == categoryB);

    if (categoryAIdx > categoryBIdx) {
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryAIdx));
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryBIdx));
    } else {
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryBIdx));
        EXPECT_EQ(CK_OK, pathManager->RemoveCategory(categoryAIdx));
    }
}

TEST_F(CKRuntimeFixture, ResolveFileNameValidatesFileSchemeTargetExists) {
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
    EXPECT_TRUE(fileUri == XString(absoluteFilePath));

    const int removeResult = remove(absoluteFilePath);
    EXPECT_EQ(0, removeResult);

    XString missingFileUri = "file://";
    missingFileUri << absoluteFilePath;
    EXPECT_EQ(CKERR_NOTFOUND, pathManager->ResolveFileName(missingFileUri, DATA_PATH_IDX, -1));
}
