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

static void EmptyOperation(CKContext *, CKParameterOut *, CKParameterIn *, CKParameterIn *) {
}

TEST_F(CKRuntimeFixture, PostLoadRebindsParameterOperationOwnerAndFunction) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "PostLoadScript", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    behavior->UseGraph();
    behavior->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);

    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0D1u);
    pm->RegisterOperationType(opGuid, "HierarchyPostLoadOperation");

    CKGUID resGuid = CKPGUID_NONE;
    CKGUID p1Guid = CKPGUID_NONE;
    CKGUID p2Guid = CKPGUID_NONE;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(opGuid, resGuid, p1Guid, p2Guid, EmptyOperation));

    CKParameterOperation *operation = context_->CreateCKParameterOperation(
        "OperationBeforePostLoad",
        opGuid,
        CKPGUID_NONE,
        CKPGUID_NONE,
        CKPGUID_NONE);
    ASSERT_NE(nullptr, operation);

    ASSERT_EQ(CK_OK, behavior->AddParameterOperation(operation));

    operation->SetOwner(nullptr);
    operation->SetName("ChangedOperationName", TRUE);

    ASSERT_EQ(nullptr, operation->GetOwner());
    ASSERT_STREQ("ChangedOperationName", operation->GetName());

    behavior->PostLoad();

    EXPECT_EQ(behavior, operation->GetOwner());
    EXPECT_NE(nullptr, operation->GetOperationFunction());
    ASSERT_NE(nullptr, operation->GetName());
    EXPECT_STREQ("HierarchyPostLoadOperation", operation->GetName());
}

TEST_F(CKRuntimeFixture, PostLoadClearsTopmostFlagOnSubBehavior) {
    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "ParentGraph", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *child = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "ChildGraph", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, parent);
    ASSERT_NE(nullptr, child);

    parent->UseGraph();
    parent->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);

    child->UseGraph();
    child->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);
    ASSERT_NE(0u, child->GetFlags() & CKBEHAVIOR_TOPMOST);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(child));

    parent->PostLoad();

    EXPECT_EQ(0u, child->GetFlags() & CKBEHAVIOR_TOPMOST);
}

TEST_F(CKRuntimeFixture, PostLoadDoesNotRenameSharedUpstreamSource) {
    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "SharedSourceParent", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *child = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "SharedSourceChild", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, parent);
    ASSERT_NE(nullptr, child);

    parent->UseGraph();
    parent->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);
    child->UseGraph();
    child->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);

    CKParameterOut *sharedSource = parent->CreateOutputParameter("SharedSource", CKPGUID_INT);
    CKParameterIn *childInput = child->CreateInputParameter("ChildInput", CKPGUID_INT);
    ASSERT_NE(nullptr, sharedSource);
    ASSERT_NE(nullptr, childInput);
    ASSERT_EQ(CK_OK, childInput->SetDirectSource(sharedSource));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(child));

    parent->PostLoad();

    ASSERT_NE(nullptr, sharedSource->GetName());
    ASSERT_NE(nullptr, childInput->GetName());
    EXPECT_STREQ("SharedSource", sharedSource->GetName());
    EXPECT_STREQ("ChildInput", childInput->GetName());
}

static const CKGUID PostLoadNamesGuid(0x791ab607, 0x18a54d32);

static CKERROR CreatePostLoadNamesPrototype(CKBehaviorPrototype **result) {
    CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("PostLoad names");
    proto->DeclareInput("Default input");
    proto->DeclareOutput("Default output");
    proto->DeclareInParameter("Default argument", CKPGUID_INT);
    proto->DeclareOutParameter("Default result", CKPGUID_INT);
    proto->DeclareLocalParameter("Default local", CKPGUID_INT);
    *result = proto;
    return CK_OK;
}

static void DeclarePostLoadNames(XObjectDeclarationArray *declarations) {
    CKObjectDeclaration *declaration = CreateCKObjectDeclaration("PostLoad names");
    declaration->SetGuid(PostLoadNamesGuid);
    declaration->SetVersion(0x10000);
    declaration->SetCreationFunction(CreatePostLoadNamesPrototype);
    CKStoreDeclaration(declarations, declaration);
}

static CKPluginInfo *PostLoadNamesPluginInfo(int index) {
    if (index != 0) return nullptr;
    static CKPluginInfo info;
    info.m_GUID = CKGUID(0x791ab607, 0x18a54d33);
    info.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    return &info;
}

TEST_F(CKRuntimeFixture, PostLoadPreservesSavedPortNamesAndUnnamedLocalParameter) {
    ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin(
        "PostLoad names fixture", nullptr, PostLoadNamesPluginInfo, nullptr, DeclarePostLoadNames));

    CKBehavior *root = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR));
    CKBehavior *child = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR));
    ASSERT_NE(nullptr, root);
    ASSERT_NE(nullptr, child);
    root->UseGraph();
    root->ModifyFlags(CKBEHAVIOR_TOPMOST, 0);
    ASSERT_EQ(CK_OK, child->InitFromGuid(PostLoadNamesGuid));
    ASSERT_EQ(CK_OK, root->AddSubBehavior(child));
    child->GetInput(0)->SetName("Saved input");
    child->GetOutput(0)->SetName("Saved output");
    child->GetInputParameter(0)->SetName("Saved argument");
    child->GetOutputParameter(0)->SetName("Saved result");
    child->GetLocalParameter(0)->SetName(nullptr);

    root->PostLoad();

    EXPECT_STREQ("Saved input", child->GetInput(0)->GetName());
    EXPECT_STREQ("Saved output", child->GetOutput(0)->GetName());
    EXPECT_STREQ("Saved argument", child->GetInputParameter(0)->GetName());
    EXPECT_STREQ("Saved result", child->GetOutputParameter(0)->GetName());
    EXPECT_EQ(nullptr, child->GetLocalParameter(0)->GetName());
    EXPECT_EQ(child, child->GetInput(0)->GetOwner());
    EXPECT_EQ(child, child->GetInputParameter(0)->GetOwner());
    context_->DestroyObject(root);
    CKRemovePrototypeDeclaration(CKGetObjectDeclarationFromGuid(PostLoadNamesGuid));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
