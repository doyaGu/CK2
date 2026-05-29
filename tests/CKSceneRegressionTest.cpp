#include <gtest/gtest.h>

#include "CKAll.h"

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
    ++counter;
    XString name;
    name.Format("%s_%d", prefix, counter);
    return name;
}

TEST_F(CKRuntimeFixture, RemoveAllObjectsClearsSceneMembership) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKScene *scene = static_cast<CKScene *>(
        context_->CreateObject(CKCID_SCENE, MakeUniqueName("SceneRemoveAll").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, scene);

    CKSceneObject *obj = static_cast<CKSceneObject *>(
        context_->CreateObject(CKCID_SCENEOBJECT, MakeUniqueName("SceneObjectRemoveAll").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, obj);

    scene->AddObject(obj);
    ASSERT_TRUE(obj->IsInScene(scene));
    ASSERT_NE(nullptr, scene->GetSceneObjectDesc(obj));

    scene->RemoveAllObjects();

    EXPECT_EQ(0, scene->GetObjectCount());
    EXPECT_FALSE(obj->IsInScene(scene));
    EXPECT_EQ(nullptr, scene->GetSceneObjectDesc(obj));
}

TEST_F(CKRuntimeFixture, AddObjectDescUsesOwnerLevelDefaultSceneForLevelScripts) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKLevel *ownerLevel = static_cast<CKLevel *>(
        context_->CreateObject(CKCID_LEVEL, MakeUniqueName("OwnerLevel").CStr(), CK_OBJECTCREATION_DYNAMIC));
    CKLevel *currentLevel = static_cast<CKLevel *>(
        context_->CreateObject(CKCID_LEVEL, MakeUniqueName("CurrentLevel").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, ownerLevel);
    ASSERT_NE(nullptr, currentLevel);
    context_->SetCurrentLevel(currentLevel);

    CKBehavior *script = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, MakeUniqueName("LevelScript").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, script);
    script->SetType(CKBEHAVIORTYPE_SCRIPT);
    ASSERT_EQ(CK_OK, ownerLevel->AddScript(script));

    CKScene *targetScene = currentLevel->GetLevelScene();
    ASSERT_NE(nullptr, targetScene);

    CKSceneObjectDesc *desc = targetScene->AddObjectDesc(script);
    ASSERT_NE(nullptr, desc);
    EXPECT_EQ(0u, desc->m_Flags & CK_SCENEOBJECT_START_RESET);
}

TEST_F(CKRuntimeFixture, CopySelfKeepsSceneObjects) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKScene *scene = static_cast<CKScene *>(
        context_->CreateObject(CKCID_SCENE, MakeUniqueName("SceneCopySelf").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, scene);

    CKSceneObject *obj = static_cast<CKSceneObject *>(
        context_->CreateObject(CKCID_SCENEOBJECT, MakeUniqueName("SceneObjectCopySelf").CStr(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, obj);

    scene->AddObject(obj);
    ASSERT_EQ(1, scene->GetObjectCount());
    ASSERT_TRUE(obj->IsInScene(scene));
    ASSERT_NE(nullptr, scene->GetSceneObjectDesc(obj));

    CKDependenciesContext depsContext(context_);
    EXPECT_EQ(CK_OK, scene->Copy(*scene, depsContext));
    EXPECT_EQ(1, scene->GetObjectCount());
    EXPECT_TRUE(obj->IsInScene(scene));
    EXPECT_NE(nullptr, scene->GetSceneObjectDesc(obj));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
