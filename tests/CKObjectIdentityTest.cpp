#include <gtest/gtest.h>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include "CKAll.h"

static_assert(sizeof(CK_ID) == 4, "File object identifiers remain 32 bits");
static_assert(std::is_standard_layout<CKObjectHandle>::value, "Handle is a value");
static_assert(std::is_trivially_copyable<CKObjectHandle>::value, "Handle owns no resources");

namespace {
bool Same(CKObjectHandle a, CKObjectHandle b) { return a.Id == b.Id && a.Lifetime == b.Lifetime; }
class ReusedObject : public CKObject {
public:
    explicit ReusedObject(CKContext *context) : CKObject(context, "Reused") {}
    static void *operator new(std::size_t);
    static void operator delete(void *) noexcept {}
};
alignas(ReusedObject) unsigned char storage[sizeof(ReusedObject)];
void *ReusedObject::operator new(std::size_t) { return storage; }
class ThrowingObject : public CKObject {
public:
    explicit ThrowingObject(CKContext *context) : CKObject(context, "Throwing") {}
    void PreDelete() override { throw std::runtime_error("PreDelete"); }
};
class Observer : public CKBaseManager {
public:
    explicit Observer(CKContext *context) : CKBaseManager(context, CKGUID(0x3c919e60, 0x6e80540c), "Identity observer") {}
    int before = 0, after = 0;
    CKObjectHandle expected{};
    bool beforeMatches = false, afterInvalid = false;
    CKDWORD GetValidFunctionsMask() override { return CKMANAGER_FUNC_OnSequenceToBeDeleted | CKMANAGER_FUNC_OnSequenceDeleted; }
    CKERROR SequenceToBeDeleted(CK_ID *, int) override {
        ++before;
        beforeMatches = m_Context->GetObjectByHandle(expected) == m_Context->GetObject(expected.Id);
        return CK_OK;
    }
    CKERROR SequenceDeleted(CK_ID *, int) override { ++after; afterInvalid = !m_Context->GetObjectByHandle(expected); return CK_OK; }
};
class CKObjectIdentityTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override { ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0)); }
    void TearDown() override { if (Context) CKCloseContext(Context); CKShutdown(); }
};

TEST_F(CKObjectIdentityTest, EmptyAndInvalidHandlesFailWithoutChangingLegacyLookup) {
    EXPECT_EQ(nullptr, Context->GetObjectByHandle({}));
    EXPECT_TRUE(Same({}, Context->GetObjectHandle(0)));
    EXPECT_TRUE(Same({}, Context->GetObjectHandle(0xFFFFFFFF)));
    auto *object = Context->CreateObject(CKCID_OBJECT, "Valid"); ASSERT_NE(nullptr, object);
    const auto handle = Context->GetObjectHandle(object->GetID());
    EXPECT_EQ(object->GetID(), handle.Id); EXPECT_NE(0u, handle.Lifetime);
    EXPECT_EQ(object, Context->GetObjectByHandle(handle));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle({handle.Id, 0}));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle({handle.Id, handle.Lifetime + 1}));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle({0xFFFFFFFF, handle.Lifetime}));
    EXPECT_EQ(object, Context->GetObject(handle.Id));
}
TEST_F(CKObjectIdentityTest, SameIdAndAddressNeverReviveNormalOrSilentReferences) {
    for (CKDWORD flags : {CKDWORD(CK_DESTROY_FREEID), CKDWORD(CK_DESTROY_FREEID | CK_DESTROY_NONOTIFY)}) {
        auto *object = new ReusedObject(Context); const auto id = object->GetID();
        const auto old = Context->GetObjectHandle(id);
        ASSERT_EQ(CK_OK, Context->DestroyObject(id, flags));
        EXPECT_EQ(nullptr, Context->GetObjectByHandle(old));
        auto *replacement = new ReusedObject(Context);
        ASSERT_EQ(object, replacement); ASSERT_EQ(id, replacement->GetID());
        EXPECT_EQ(replacement, Context->GetObject(id)); EXPECT_EQ(nullptr, Context->GetObjectByHandle(old));
        EXPECT_FALSE(Same(old, Context->GetObjectHandle(id)));
        ASSERT_EQ(CK_OK, Context->DestroyObject(id, CK_DESTROY_FREEID | CK_DESTROY_NONOTIFY));
    }
}
TEST_F(CKObjectIdentityTest, DirectDestructionAlsoInvalidatesTheSlot) {
    auto *object = new ReusedObject(Context); const auto id = object->GetID();
    const auto old = Context->GetObjectHandle(id);
    object->ModifyObjectFlags(CK_OBJECT_FREEID, 0); delete object;
    auto *replacement = new ReusedObject(Context);
    ASSERT_EQ(object, replacement); ASSERT_EQ(id, replacement->GetID());
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(old));
    EXPECT_EQ(replacement, Context->GetObjectByHandle(Context->GetObjectHandle(id)));
}
TEST_F(CKObjectIdentityTest, NotificationsKeepTheirFlagsAndLookupTiming) {
    auto *observer = new Observer(Context); ASSERT_EQ(CK_OK, Context->RegisterNewManager(observer));
    auto *object = Context->CreateObject(CKCID_OBJECT, "Notified"); ASSERT_NE(nullptr, object);
    observer->expected = Context->GetObjectHandle(object->GetID());
    ASSERT_EQ(CK_OK, Context->DestroyObject(object));
    EXPECT_EQ(1, observer->before); EXPECT_EQ(1, observer->after);
    EXPECT_TRUE(observer->beforeMatches); EXPECT_TRUE(observer->afterInvalid);
    object = Context->CreateObject(CKCID_OBJECT, "Silent"); ASSERT_NE(nullptr, object);
    const auto silent = Context->GetObjectHandle(object->GetID());
    ASSERT_EQ(CK_OK, Context->DestroyObject(object, CK_DESTROY_NONOTIFY));
    EXPECT_EQ(1, observer->before); EXPECT_EQ(1, observer->after);
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(silent));
    Context->ActivateManager(observer, FALSE);
    object = Context->CreateObject(CKCID_OBJECT, "Inactive observer");
    const auto inactive = Context->GetObjectHandle(object->GetID());
    ASSERT_EQ(CK_OK, Context->DestroyObject(object, CK_DESTROY_FREEID));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(inactive));
    EXPECT_EQ(1, observer->before); EXPECT_EQ(1, observer->after);
}
TEST_F(CKObjectIdentityTest, DeferredRequestDoesNotEndTheLifetime) {
    auto *object = Context->CreateObject(CKCID_OBJECT, "Deferred"); ASSERT_NE(nullptr, object);
    const auto handle = Context->GetObjectHandle(object->GetID());
    Context->m_DeferDestroyObjects = TRUE;
    EXPECT_EQ(CK_OK, Context->DestroyObject(handle.Id, CK_DESTROY_NONOTIFY | CK_DESTROY_FREEID));
    EXPECT_TRUE(Same(handle, Context->GetObjectHandle(handle.Id)));
    EXPECT_EQ(object, Context->GetObjectByHandle(handle));
    Context->m_DeferDestroyObjects = FALSE; Context->m_ObjectManager->PostProcess();
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(handle));
}
TEST_F(CKObjectIdentityTest, FailedDeletionStillResolvesExactlyAsLegacyLookup) {
    auto *object = new ThrowingObject(Context);
    const auto handle = Context->GetObjectHandle(object->GetID());
    EXPECT_THROW(Context->DestroyObject(object), std::runtime_error);
    EXPECT_TRUE(object->IsToBeDeleted()); EXPECT_EQ(object, Context->GetObject(handle.Id));
    EXPECT_TRUE(Same(handle, Context->GetObjectHandle(handle.Id)));
    EXPECT_EQ(object, Context->GetObjectByHandle(handle));
}
TEST_F(CKObjectIdentityTest, ClearAllAndDeleteAllDoNotResetLifetimeIssuance) {
    auto *object = Context->CreateObject(CKCID_OBJECT, "Before clear");
    const auto before = Context->GetObjectHandle(object->GetID());
    ASSERT_EQ(CK_OK, Context->ClearAll());
    object = Context->CreateObject(CKCID_OBJECT, "After clear"); ASSERT_NE(nullptr, object);
    EXPECT_EQ(before.Id, object->GetID()); EXPECT_EQ(nullptr, Context->GetObjectByHandle(before));
    const auto after = Context->GetObjectHandle(object->GetID());
    EXPECT_NE(before.Lifetime, after.Lifetime);
    ASSERT_EQ(CK_OK, Context->m_ObjectManager->DeleteAllObjects());
    object = Context->CreateObject(CKCID_OBJECT, "After delete all"); ASSERT_NE(nullptr, object);
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(after));
    EXPECT_NE(after.Lifetime, Context->GetObjectHandle(object->GetID()).Lifetime);
}
TEST_F(CKObjectIdentityTest, LifetimesDistinguishContextsAndContextRecreation) {
    auto *a = Context->CreateObject(CKCID_OBJECT, "A"); const auto old = Context->GetObjectHandle(a->GetID());
    CKContext *other = nullptr; ASSERT_EQ(CK_OK, CKCreateContext(&other, nullptr, 0, 0));
    auto *b = other->CreateObject(CKCID_OBJECT, "B"); ASSERT_NE(nullptr, b);
    ASSERT_EQ(old.Id, b->GetID());
    EXPECT_EQ(nullptr, other->GetObjectByHandle(old));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(other->GetObjectHandle(b->GetID())));
    CKCloseContext(other); CKCloseContext(Context); Context = nullptr;
    ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    a = Context->CreateObject(CKCID_OBJECT, "New context"); ASSERT_NE(nullptr, a);
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(old));
}
TEST_F(CKObjectIdentityTest, CopyGetsANewLifetimeAndUseCurrentKeepsTheExistingOne) {
    auto *object = Context->CreateObject(CKCID_BEOBJECT, "Source"); ASSERT_NE(nullptr, object);
    const auto source = Context->GetObjectHandle(object->GetID());
    auto *copy = Context->CopyObject(object); ASSERT_NE(nullptr, copy);
    EXPECT_NE(source.Lifetime, Context->GetObjectHandle(copy->GetID()).Lifetime);
    CK_CREATIONMODE mode = CKLOAD_INVALID;
    EXPECT_EQ(object, Context->CreateObject(CKCID_BEOBJECT, "Source", CK_OBJECTCREATION_USECURRENT, &mode));
    EXPECT_EQ(CKLOAD_USECURRENT, mode); EXPECT_TRUE(Same(source, Context->GetObjectHandle(object->GetID())));
}
TEST_F(CKObjectIdentityTest, GrowingTheTablePreservesAllExistingHandles) {
    auto *object = Context->CreateObject(CKCID_OBJECT, "First"); ASSERT_NE(nullptr, object);
    const auto first = Context->GetObjectHandle(object->GetID());
    for (int i = 0; i < 1200; ++i) ASSERT_NE(nullptr, Context->CreateObject(CKCID_OBJECT));
    EXPECT_TRUE(Same(first, Context->GetObjectHandle(first.Id)));
    EXPECT_EQ(object, Context->GetObjectByHandle(first));
}

TEST_F(CKObjectIdentityTest, ConstructorFailureUnregistersTheIssuedLifetime) {
    class FailingObject : public CKObject {
    public:
        FailingObject(CKContext *context, CKObjectHandle &issued) : CKObject(context, "Failing construction") {
            issued = context->GetObjectHandle(GetID());
            throw std::runtime_error("constructor");
        }
    };
    CKObjectHandle issued{};
    EXPECT_THROW(new FailingObject(Context, issued), std::runtime_error);
    ASSERT_NE(0u, issued.Lifetime);
    EXPECT_EQ(nullptr, Context->GetObject(issued.Id));
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(issued));
}
TEST_F(CKObjectIdentityTest, DynamicModeChangesKeepIdentityUntilActualDestruction) {
    auto *object = Context->CreateObject(CKCID_OBJECT, "Dynamic transition"); ASSERT_NE(nullptr, object);
    const auto handle = Context->GetObjectHandle(object->GetID());
    Context->ChangeObjectDynamic(object, TRUE);
    ASSERT_TRUE(object->IsDynamic());
    EXPECT_TRUE(Same(handle, Context->GetObjectHandle(handle.Id)));
    Context->ChangeObjectDynamic(object, FALSE);
    ASSERT_FALSE(object->IsDynamic());
    EXPECT_TRUE(Same(handle, Context->GetObjectHandle(handle.Id)));
    Context->ChangeObjectDynamic(object, TRUE);
    Context->DestroyAllDynamicObjects();
    EXPECT_EQ(object, Context->GetObjectByHandle(handle));
    Context->m_ObjectManager->PostProcess();
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(handle));
}
TEST_F(CKObjectIdentityTest, RuntimeRestartDoesNotReissueAnOldLifetime) {
    auto *object = Context->CreateObject(CKCID_OBJECT, "Before shutdown"); ASSERT_NE(nullptr, object);
    const auto handle = Context->GetObjectHandle(object->GetID());
    CKCloseContext(Context); Context = nullptr;
    CKShutdown();
    ASSERT_EQ(CK_OK, CKStartUp());
    ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    object = Context->CreateObject(CKCID_OBJECT, "After startup"); ASSERT_NE(nullptr, object);
    EXPECT_EQ(handle.Id, object->GetID());
    EXPECT_EQ(nullptr, Context->GetObjectByHandle(handle));
    EXPECT_NE(handle.Lifetime, Context->GetObjectHandle(handle.Id).Lifetime);
}
} // namespace
