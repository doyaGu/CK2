#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "CKAll.h"

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

size_t BoundedStringLength(const char *text, size_t capacity) {
    if (!text) {
        return 0;
    }

    size_t length = 0;
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

std::string MakeUniqueName(const char *prefix) {
    static int counter = 0;
    char buffer[128] = {};
    ++counter;
    sprintf_s(buffer, "%s_%d", prefix, counter);
    return std::string(buffer);
}

CKGUID MakeUniqueGuid() {
    static CKDWORD counter = 0;
    ++counter;
    return CKGUID(0x7C0FFEE0u, 0x42000000u + counter);
}

class ScopedAutomaticLoadModeReset {
public:
    explicit ScopedAutomaticLoadModeReset(CKContext *context) : context_(context) {}

    ~ScopedAutomaticLoadModeReset() {
        if (context_) {
            context_->SetAutomaticLoadMode(
                CKLOAD_INVALID,
                CKLOAD_INVALID,
                CKLOAD_INVALID,
                CKLOAD_INVALID);
        }
    }

private:
    CKContext *context_;
};

class CountingManager : public CKBaseManager {
public:
    CountingManager(CKContext *context, CKGUID guid, CKSTRING name)
        : CKBaseManager(context, guid, name), on_init_calls(0), on_end_calls(0) {}

    CKERROR OnCKInit() override {
        ++on_init_calls;
        return CK_OK;
    }

    CKERROR OnCKEnd() override {
        ++on_end_calls;
        return CK_OK;
    }

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit | CKMANAGER_FUNC_OnCKEnd;
    }

    int on_init_calls;
    int on_end_calls;
};

} // namespace

TEST_F(CKRuntimeFixture, GetSecureNameRespectsCallerProvidedBufferSize) {
    const std::string baseName = MakeUniqueName("SecureNameBoundaryObject");
    CKObject *existing = context_->CreateObject(
        CKCID_OBJECT,
        baseName.c_str(),
        CK_OBJECTCREATION_NONAMECHECK);
    ASSERT_NE(nullptr, existing);

    struct BufferWithGuard {
        char buffer[8];
        char guard[4];
    } output = {};

    memset(&output, 'X', sizeof(output));
    memset(output.guard, 'G', sizeof(output.guard));

    context_->GetSecureName(output.buffer, baseName.c_str(), CKCID_OBJECT, sizeof(output.buffer));

    EXPECT_LT(BoundedStringLength(output.buffer, sizeof(output.buffer)), sizeof(output.buffer));
    EXPECT_EQ('G', output.guard[0]);
    EXPECT_EQ('G', output.guard[1]);
    EXPECT_EQ('G', output.guard[2]);
    EXPECT_EQ('G', output.guard[3]);
}

TEST_F(CKRuntimeFixture, LoadVerifyObjectUnicityRenameHonorsBufferSizeParameter) {
    ScopedAutomaticLoadModeReset reset(context_);
    context_->SetAutomaticLoadMode(
        CKLOAD_RENAME,
        CKLOAD_INVALID,
        CKLOAD_INVALID,
        CKLOAD_INVALID);

    const std::string baseName = MakeUniqueName("LoadRenameBoundaryObject");
    CKObject *existing = context_->CreateObject(
        CKCID_OBJECT,
        baseName.c_str(),
        CK_OBJECTCREATION_NONAMECHECK);
    ASSERT_NE(nullptr, existing);

    struct BufferWithGuard {
        char buffer[9];
        unsigned char guard;
    } output = {};

    memset(&output, 0xAB, sizeof(output));

    CKObject *resolvedObject = nullptr;
    const CK_LOADMODE mode = context_->LoadVerifyObjectUnicity(
        baseName.c_str(),
        CKCID_OBJECT,
        output.buffer,
        &resolvedObject,
        sizeof(output.buffer));

    ASSERT_EQ(CKLOAD_RENAME, mode);
    EXPECT_EQ(existing, resolvedObject);
    EXPECT_LT(BoundedStringLength(output.buffer, sizeof(output.buffer)), sizeof(output.buffer));
    EXPECT_EQ(static_cast<unsigned char>(0xAB), output.guard);
}

TEST_F(CKRuntimeFixture, ClearAllReactivatesInactiveManager) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    const std::string managerName = MakeUniqueName("ClearAllSingleManager");
    CountingManager *manager = new CountingManager(context_, MakeUniqueGuid(), managerName.c_str());
    ASSERT_NE(nullptr, manager);
    ASSERT_EQ(CK_OK, context_->RegisterNewManager(manager));
    EXPECT_EQ(1, manager->on_init_calls);
    EXPECT_TRUE(context_->IsManagerActive(manager));

    context_->ActivateManager(manager, FALSE);
    EXPECT_FALSE(context_->IsManagerActive(manager));
    EXPECT_EQ(1, manager->on_end_calls);

    ASSERT_EQ(CK_OK, context_->ClearAll());
    EXPECT_TRUE(context_->IsManagerActive(manager));
    EXPECT_EQ(2, manager->on_init_calls);
}

TEST_F(CKRuntimeFixture, ClearAllReactivatesAllInactiveManagers) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    std::vector<CountingManager *> managers;
    const int managerCount = 3;
    for (int i = 0; i < managerCount; ++i) {
        const std::string managerName = MakeUniqueName("ClearAllMultiManager");
        CountingManager *manager = new CountingManager(context_, MakeUniqueGuid(), managerName.c_str());
        ASSERT_NE(nullptr, manager);
        ASSERT_EQ(CK_OK, context_->RegisterNewManager(manager));
        context_->ActivateManager(manager, FALSE);
        managers.push_back(manager);
    }

    EXPECT_EQ(managerCount, context_->GetInactiveManagerCount());

    ASSERT_EQ(CK_OK, context_->ClearAll());
    EXPECT_EQ(0, context_->GetInactiveManagerCount());
    for (int i = 0; i < managerCount; ++i) {
        EXPECT_TRUE(context_->IsManagerActive(managers[i]));
        EXPECT_EQ(2, managers[i]->on_init_calls);
        EXPECT_EQ(1, managers[i]->on_end_calls);
    }
}

TEST_F(CKRuntimeFixture, RemoveSceneByIndexRejectsInvalidIndices) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    const std::string levelName = MakeUniqueName("RemoveSceneInvalidIndexLevel");
    CKLevel *level = static_cast<CKLevel *>(
        context_->CreateObject(CKCID_LEVEL, levelName.c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, level);

    const std::string sceneName = MakeUniqueName("RemoveSceneInvalidIndexScene");
    CKScene *scene = static_cast<CKScene *>(
        context_->CreateObject(CKCID_SCENE, sceneName.c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, scene);
    ASSERT_EQ(CK_OK, level->AddScene(scene));

    ASSERT_EQ(1, level->GetSceneCount());
    EXPECT_EQ(nullptr, level->RemoveScene(-1));
    EXPECT_EQ(nullptr, level->RemoveScene(1));
    EXPECT_EQ(1, level->GetSceneCount());
    EXPECT_EQ(level, scene->GetLevel());
    EXPECT_TRUE(scene->IsObjectHere(level));
}

TEST_F(CKRuntimeFixture, RemoveSceneByIndexClearsSceneLinks) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    const std::string levelName = MakeUniqueName("RemoveSceneByIndexLevel");
    CKLevel *level = static_cast<CKLevel *>(
        context_->CreateObject(CKCID_LEVEL, levelName.c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, level);

    const std::string sceneName = MakeUniqueName("RemoveSceneByIndexScene");
    CKScene *scene = static_cast<CKScene *>(
        context_->CreateObject(CKCID_SCENE, sceneName.c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, scene);
    ASSERT_EQ(CK_OK, level->AddScene(scene));
    ASSERT_EQ(1, level->GetSceneCount());

    CKScene *removed = level->RemoveScene(0);
    ASSERT_EQ(scene, removed);
    EXPECT_EQ(0, level->GetSceneCount());
    EXPECT_EQ(nullptr, scene->GetLevel());
    EXPECT_FALSE(scene->IsObjectHere(level));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
