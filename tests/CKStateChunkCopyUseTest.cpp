#include <gtest/gtest.h>
#include "CKAll.h"
#include <type_traits>

static_assert(!std::is_copy_constructible<CKStateChunk>::value, "Chunks require an explicit deep copy");
static_assert(!std::is_copy_assignable<CKStateChunk>::value, "Chunks require an explicit deep copy");

class CKStateChunkCopyUseTest : public ::testing::Test {
protected:
    CKContext *context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    void TearDown() override {
        if (context) CKCloseContext(context);
        CKShutdown();
    }
    CKStateChunk *Values() {
        CKStateChunk *chunk = CreateCKStateChunk(CKCID_BEHAVIOR, nullptr);
        chunk->StartWrite(); chunk->WriteInt(41); chunk->WriteInt(43); chunk->CloseChunk(); chunk->StartRead();
        return chunk;
    }
    void ExpectValues(CKStateChunk *chunk) {
        ASSERT_NE(nullptr, chunk); EXPECT_EQ(8, chunk->GetDataSize());
        chunk->StartRead(); EXPECT_EQ(41, chunk->ReadInt()); EXPECT_EQ(43, chunk->ReadInt());
    }
};

TEST_F(CKStateChunkCopyUseTest, BehaviorInterfaceCopySurvivesSourceDestruction) {
    CKBehavior *source = static_cast<CKBehavior *>(context->CreateObject(CKCID_BEHAVIOR, "Source"));
    CKBehavior *copy = static_cast<CKBehavior *>(context->CreateObject(CKCID_BEHAVIOR, "Copy"));
    ASSERT_NE(nullptr, source); ASSERT_NE(nullptr, copy);
    source->SetInterfaceChunk(Values());
    CKDependenciesContext dependencies(context);
    ASSERT_EQ(CK_OK, copy->Copy(*source, dependencies));
    ASSERT_NE(source->GetInterfaceChunk(), copy->GetInterfaceChunk());
    EXPECT_NE(source->GetInterfaceChunk()->LockReadBuffer(), copy->GetInterfaceChunk()->LockReadBuffer());
    context->DestroyObject(source);
    ExpectValues(copy->GetInterfaceChunk());
}

TEST_F(CKStateChunkCopyUseTest, SceneInitialValueCopySurvivesSourceDestruction) {
    CKScene *source = static_cast<CKScene *>(context->CreateObject(CKCID_SCENE, "Source Scene"));
    CKScene *copy = static_cast<CKScene *>(context->CreateObject(CKCID_SCENE, "Copy Scene"));
    CKSceneObject *object = static_cast<CKSceneObject *>(context->CreateObject(CKCID_SCENEOBJECT, "Object"));
    ASSERT_NE(nullptr, source); ASSERT_NE(nullptr, copy); ASSERT_NE(nullptr, object);
    source->AddObject(object);
    CKSceneObjectDesc *original = source->GetSceneObjectDesc(object); ASSERT_NE(nullptr, original);
    original->m_InitialValue = Values();
    CKDependenciesContext dependencies(context);
    ASSERT_EQ(CK_OK, copy->Copy(*source, dependencies));
    CKSceneObjectDesc *copied = copy->GetSceneObjectDesc(object); ASSERT_NE(nullptr, copied);
    ASSERT_NE(nullptr, copied->m_InitialValue);
    EXPECT_NE(original->m_InitialValue->LockReadBuffer(), copied->m_InitialValue->LockReadBuffer());
    context->DestroyObject(source);
    ExpectValues(copied->m_InitialValue);
}

TEST_F(CKStateChunkCopyUseTest, MovingLevelScriptRetainsIndependentSceneInitialValue) {
    CKLevel *source = static_cast<CKLevel *>(context->CreateObject(CKCID_LEVEL, "Source Level"));
    CKLevel *target = static_cast<CKLevel *>(context->CreateObject(CKCID_LEVEL, "Target Level"));
    CKBehavior *script = static_cast<CKBehavior *>(context->CreateObject(CKCID_BEHAVIOR, "Script"));
    ASSERT_NE(nullptr, source); ASSERT_NE(nullptr, target); ASSERT_NE(nullptr, script);
    context->SetCurrentLevel(source);
    script->SetType(CKBEHAVIORTYPE_SCRIPT); ASSERT_EQ(CK_OK, source->AddScript(script));
    CKSceneObjectDesc *original = source->GetLevelScene()->GetSceneObjectDesc(script);
    ASSERT_NE(nullptr, original);
    DeleteCKStateChunk(original->m_InitialValue); original->m_InitialValue = Values();
    const void *originalData = original->m_InitialValue->LockReadBuffer();
    ASSERT_EQ(CK_OK, CKMoveScript(source, target, script));
    EXPECT_EQ(target, script->GetOwner()); EXPECT_EQ(0, source->GetScriptCount());
    CKSceneObjectDesc *moved = target->GetLevelScene()->GetSceneObjectDesc(script);
    ASSERT_NE(nullptr, moved); ASSERT_NE(nullptr, moved->m_InitialValue);
    EXPECT_NE(originalData, moved->m_InitialValue->LockReadBuffer());
    ExpectValues(moved->m_InitialValue);
}
