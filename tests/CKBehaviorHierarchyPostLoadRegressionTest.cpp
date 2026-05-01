#include <gtest/gtest.h>

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

void EmptyOperation(CKContext *, CKParameterOut *, CKParameterIn *, CKParameterIn *) {
}

} // namespace

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

TEST_F(CKRuntimeFixture, ReparentingRemovesChildFromPreviousParent) {
    CKBehavior *firstParent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "FirstParentGraph", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *secondParent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "SecondParentGraph", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *child = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "SharedChildGraph", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, firstParent);
    ASSERT_NE(nullptr, secondParent);
    ASSERT_NE(nullptr, child);

    firstParent->UseGraph();
    secondParent->UseGraph();
    child->UseGraph();

    ASSERT_EQ(CK_OK, firstParent->AddSubBehavior(child));
    EXPECT_EQ(1, firstParent->GetSubBehaviorCount());
    EXPECT_EQ(child, firstParent->GetSubBehavior(0));
    EXPECT_EQ(firstParent, child->GetParent());

    ASSERT_EQ(CK_OK, secondParent->AddSubBehavior(child));
    EXPECT_EQ(0, firstParent->GetSubBehaviorCount());
    EXPECT_EQ(nullptr, firstParent->GetSubBehavior(0));
    EXPECT_EQ(1, secondParent->GetSubBehaviorCount());
    EXPECT_EQ(child, secondParent->GetSubBehavior(0));
    EXPECT_EQ(secondParent, child->GetParent());
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
