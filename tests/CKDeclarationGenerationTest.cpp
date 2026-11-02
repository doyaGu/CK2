#include <gtest/gtest.h>
#include "CKAll.h"

namespace {
const CKGUID guid(0x6b32de04, 0x1adf7821);
CKDWORD version = 1;
int Function(const CKBehaviorContext &) { return CKBR_OK; }
CKERROR Prototype(CKBehaviorPrototype **out) {
    *out = CreateCKBehaviorPrototype("Generation test");
    (*out)->SetFunction(Function);
    return CK_OK;
}
void Declarations(XObjectDeclarationArray *out) {
    auto *decl = CreateCKObjectDeclaration("Generation test");
    decl->SetGuid(guid);
    decl->SetVersion(version);
    decl->SetDescription("Metadata must survive replacement");
    decl->SetAuthorName("Test");
    decl->SetCategory("Tests");
    decl->SetCreationFunction(Prototype);
    CKStoreDeclaration(out, decl);
}
class TestPluginManager : public CKPluginManager {
public:
    using CKPluginManager::InitializeBehaviors;
};

TEST(CKDeclarationGenerationTest, RegistrationReplacementAndSameVersionReloadAreDistinct) {
    ASSERT_EQ(CK_OK, CKStartUp());
    {
        TestPluginManager manager;
        CKPluginEntry entry;
        entry.m_BehaviorsInfo = new CKPluginEntryBehaviorsData;
        entry.m_IndexInCategory = 0;
        auto *unregistered = CreateCKObjectDeclaration("Unregistered");
        EXPECT_EQ(0u, unregistered->GetGeneration());
        delete unregistered;
        version = 1;
        manager.InitializeBehaviors(Declarations, entry);
        auto *first = CKGetObjectDeclarationFromGuid(guid);
        ASSERT_NE(nullptr, first);
        const CKQWORD generation = first->GetGeneration();
        EXPECT_NE(0u, generation);
        ASSERT_NE(nullptr, CKGetPrototypeFromGuid(guid));
        manager.InitializeBehaviors(Declarations, entry);
        EXPECT_EQ(generation, CKGetObjectDeclarationFromGuid(guid)->GetGeneration());
        version = 2;
        manager.InitializeBehaviors(Declarations, entry);
        auto *replacement = CKGetObjectDeclarationFromGuid(guid);
        ASSERT_NE(nullptr, replacement);
        const CKQWORD replacementGeneration = replacement->GetGeneration();
        EXPECT_NE(generation, replacementGeneration);
        EXPECT_STREQ("Metadata must survive replacement", replacement->GetDescription());
        EXPECT_STREQ("Test", replacement->GetAuthorName());
        EXPECT_STREQ("Tests", replacement->GetCategory());
        ASSERT_NE(nullptr, CKGetPrototypeFromGuid(guid));
        ASSERT_EQ(CK_OK, CKRemovePrototypeDeclaration(replacement));
        EXPECT_EQ(nullptr, CKGetObjectDeclarationFromGuid(guid));
        manager.InitializeBehaviors(Declarations, entry);
        auto *reloaded = CKGetObjectDeclarationFromGuid(guid);
        ASSERT_NE(nullptr, reloaded);
        EXPECT_NE(replacementGeneration, reloaded->GetGeneration());
        EXPECT_NE(generation, reloaded->GetGeneration());
        EXPECT_EQ(CK_OK, CKRemovePrototypeDeclaration(reloaded));
    }
    CKShutdown();
}
} // namespace
