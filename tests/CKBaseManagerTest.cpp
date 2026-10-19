#include <gtest/gtest.h>
#include <new>
#include <cstring>
#include <SDL3/SDL_timer.h>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBaseManager.cpp"

static CKGUID BaseManagerGuid() {
    static CKDWORD next = 0;
    return CKGUID(0x6d180000u, ++next);
}

struct ManagerEvents {
    int init = 0, end = 0, pre = 0, post = 0, beforeDelete = 0, afterDelete = 0, destroyed = 0;
};

class ManagerProbe : public CKBaseManager {
public:
    ManagerEvents *Events = nullptr;
    ManagerProbe(CKContext *context, CKGUID guid, CKSTRING name) : CKBaseManager(context, guid, name) {}
    ~ManagerProbe() override { if (Events) ++Events->destroyed; }
    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit | CKMANAGER_FUNC_OnCKEnd | CKMANAGER_FUNC_PreProcess |
            CKMANAGER_FUNC_PostProcess | CKMANAGER_FUNC_OnSequenceToBeDeleted | CKMANAGER_FUNC_OnSequenceDeleted;
    }
    CKERROR OnCKInit() override { if (Events) ++Events->init; return CK_OK; }
    CKERROR OnCKEnd() override { if (Events) ++Events->end; return CK_OK; }
    CKERROR PreProcess() override { if (Events) ++Events->pre; return CK_OK; }
    CKERROR PostProcess() override { if (Events) ++Events->post; return CK_OK; }
    CKERROR SequenceToBeDeleted(CK_ID *, int) override { if (Events) ++Events->beforeDelete; return CK_OK; }
    CKERROR SequenceDeleted(CK_ID *, int) override { if (Events) ++Events->afterDelete; return CK_OK; }
};

// Keep actual object storage until after the assertions. Remove any stale
// registration only afterwards so a failing destructor cannot corrupt teardown.
template<class T = CKBaseManager> class ManagerSlot {
public:
    ManagerSlot(CKContext *context, CKGUID guid, CKSTRING name = "audit manager") : Context(context), Guid(guid) {
        Object = new (Storage) T(context, guid, name); Base = Object;
    }
    ~ManagerSlot() {
        Destroy();
        if (Context) {
            if (Context->GetManagerByGuid(Guid) == Base) Context->m_ManagerTable.Remove(Guid);
            while (Context->m_InactiveManagers.Remove(Base)) {}
            Context->BuildSortedLists();
        }
    }
    T *Get() const { return Object; }
    CKBaseManager *Address() const { return Base; }
    void Destroy() { if (Alive) { Object->~T(); Alive = false; } }
private:
    alignas(T) unsigned char Storage[sizeof(T)];
    CKContext *Context;
    CKGUID Guid;
    T *Object;
    CKBaseManager *Base;
    bool Alive = true;
};

class ManagerTimerProbe : public VxTimeProfiler {
public:
    static Uint64 Start(const VxTimeProfiler &timer) {
        XDWORD (VxTimeProfiler::*words)[4] = &ManagerTimerProbe::Times;
        Uint64 start; std::memcpy(&start, timer.*words, sizeof(start)); return start;
    }
};

class CKBaseManagerTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    }
    void TearDown() override { if (Context) CKCloseContext(Context); CKShutdown(); }
    CKObject *Object() { return Context->CreateObject(CKCID_OBJECT, const_cast<char *>("manager object")); }
};

TEST_F(CKBaseManagerTest, ConstructorCopiesGuidAndNameAndBorrowsTheContext) {
    char name[] = "owned name"; CKGUID guid = BaseManagerGuid();
    ManagerSlot<> slot(Context, guid, name); CKBaseManager *manager = slot.Get();
    EXPECT_EQ(Context, manager->m_Context); EXPECT_EQ(guid, manager->GetGuid());
    EXPECT_STREQ("owned name", manager->GetName()); EXPECT_NE(name, manager->GetName());
    name[0] = 'X'; EXPECT_STREQ("owned name", manager->GetName()); EXPECT_FLOAT_EQ(0.0f, manager->m_ProcessingTime);
}

TEST_F(CKBaseManagerTest, NullAndEmptyNamesRemainDistinctStoredValues) {
    CKBaseManager unnamed(nullptr, BaseManagerGuid(), nullptr), empty(nullptr, BaseManagerGuid(), "");
    EXPECT_EQ(nullptr, unnamed.GetName()); ASSERT_NE(nullptr, empty.GetName()); EXPECT_STREQ("", empty.GetName());
}

TEST_F(CKBaseManagerTest, ConstructionDoesNotRegisterOrRunLifecycleCallbacks) {
    int count = Context->GetManagerCount(); ManagerEvents events; CKGUID guid = BaseManagerGuid();
    ManagerSlot<ManagerProbe> slot(Context, guid); slot.Get()->Events = &events;
    EXPECT_EQ(nullptr, Context->GetManagerByGuid(guid)); EXPECT_EQ(count, Context->GetManagerCount());
    EXPECT_EQ(0, events.init); EXPECT_EQ(0, events.end);
}

TEST_F(CKBaseManagerTest, LayoutRetainsNativeWin32StorageAndNaturalPointerSizedX64Offsets) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "layout");
    const void *fields[] = {&manager.m_ManagerGuid, &manager.m_ManagerName, &manager.m_Context, &manager.m_ProcessingTime, &manager.m_Timer};
    const size_t x86[] = {4,12,16,20,24}, x64[] = {8,16,24,32,36};
    for (int i = 0; i < 5; ++i) EXPECT_EQ(sizeof(void *) == 4 ? x86[i] : x64[i], static_cast<const char *>(fields[i]) - reinterpret_cast<const char *>(&manager));
    EXPECT_EQ(sizeof(void *) == 4 ? 40u : 56u, sizeof(CKBaseManager));
    EXPECT_EQ(sizeof(void *), alignof(CKBaseManager)); EXPECT_EQ(16u, sizeof(VxTimeProfiler));
    EXPECT_TRUE((bool)__is_polymorphic(CKBaseManager)); EXPECT_FALSE((bool)__is_base_of(CKObject, CKBaseManager));
}

TEST_F(CKBaseManagerTest, GuidAndNameGettersExposeTheStoredIdentity) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "original");
    CKGUID changed = BaseManagerGuid(); manager.m_ManagerGuid = changed;
    EXPECT_EQ(changed, manager.GetGuid()); EXPECT_EQ(manager.m_ManagerName, manager.GetName());
    manager.m_ManagerName[0] = 'O'; EXPECT_STREQ("Original", manager.GetName());
}

TEST_F(CKBaseManagerTest, DefaultNoArgumentCallbacksSucceedWithoutChangingManagerState) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "defaults");
    CKERROR (CKBaseManager::*calls[])() = {&CKBaseManager::PreClearAll,&CKBaseManager::PostClearAll,
        &CKBaseManager::PreProcess,&CKBaseManager::PostProcess,&CKBaseManager::OnCKInit,&CKBaseManager::OnCKEnd,
        &CKBaseManager::OnCKReset,&CKBaseManager::OnCKPostReset,&CKBaseManager::OnCKPause,&CKBaseManager::OnCKPlay,
        &CKBaseManager::PreLoad,&CKBaseManager::PostLoad,&CKBaseManager::PreSave,&CKBaseManager::PostSave};
    CKGUID guid = manager.GetGuid(); manager.m_ProcessingTime = 17.5f;
    for (int i = 0; i < 14; ++i) {
        SCOPED_TRACE(i); EXPECT_EQ(CK_OK, (manager.*calls[i])());
        EXPECT_EQ(guid, manager.GetGuid()); EXPECT_STREQ("defaults", manager.GetName()); EXPECT_FLOAT_EQ(17.5f, manager.m_ProcessingTime);
    }
}

TEST_F(CKBaseManagerTest, DefaultSaveAndLoadLeaveCallerChunkStateUnchanged) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "serialization");
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteIdentifier(0x1234); chunk.WriteInt(789); chunk.CloseChunk();
    chunk.StartRead(); ASSERT_TRUE(chunk.SeekIdentifier(0x1234)); int position = chunk.GetCurrentPos();
    EXPECT_EQ(nullptr, manager.SaveData(nullptr)); EXPECT_EQ(CK_OK, manager.LoadData(&chunk, nullptr));
    EXPECT_EQ(position, chunk.GetCurrentPos()); EXPECT_EQ(789, chunk.ReadInt()); EXPECT_EQ(CK_OK, manager.LoadData(nullptr, nullptr));
}

TEST_F(CKBaseManagerTest, DefaultSceneAndDeletionCallbacksLeaveIdsAndObjectsUntouched) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "sequences");
    CKObject *first = Object(), *second = Object(); ASSERT_NE(nullptr, first); ASSERT_NE(nullptr, second);
    CK_ID ids[] = {first->GetID(), second->GetID()};
    CKScene *scene = static_cast<CKScene *>(Context->CreateObject(CKCID_SCENE, "scene")); ASSERT_NE(nullptr, scene);
    EXPECT_EQ(CK_OK, manager.SequenceAddedToScene(scene, ids, 2)); EXPECT_EQ(CK_OK, manager.SequenceRemovedFromScene(scene, ids, 2));
    EXPECT_EQ(CK_OK, manager.SequenceToBeDeleted(ids, 2)); EXPECT_EQ(CK_OK, manager.SequenceDeleted(ids, 2));
    EXPECT_EQ(CK_OK, manager.PreLaunchScene(scene, nullptr)); EXPECT_EQ(CK_OK, manager.PostLaunchScene(nullptr, scene));
    EXPECT_EQ(first->GetID(), ids[0]); EXPECT_EQ(second->GetID(), ids[1]);
    EXPECT_EQ(first, Context->GetObject(ids[0])); EXPECT_EQ(second, Context->GetObject(ids[1]));
}

TEST_F(CKBaseManagerTest, DefaultCopyAndRenderCallbacksDoNotChangeDependenciesOrManagerState) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "copy/render"); CKDependenciesContext dependencies(Context);
    CKObject *object = Object(); ASSERT_NE(nullptr, object); CK_ID id = object->GetID(); dependencies.AddObjects(&id, 1);
    EXPECT_EQ(CK_OK, manager.OnPreCopy(dependencies)); EXPECT_EQ(CK_OK, manager.OnPostCopy(dependencies));
    EXPECT_EQ(CK_OK, manager.OnPreRender(nullptr)); EXPECT_EQ(CK_OK, manager.OnPostRender(nullptr)); EXPECT_EQ(CK_OK, manager.OnPostSpriteRender(nullptr));
    ASSERT_EQ(1, dependencies.GetObjectsCount()); EXPECT_EQ(object, dependencies.GetObjects(0)); EXPECT_FLOAT_EQ(0.0f, manager.m_ProcessingTime);
}

TEST_F(CKBaseManagerTest, DefaultMaskAndEveryCallbackPriorityAreZero) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "mask"); EXPECT_EQ(0u, manager.GetValidFunctionsMask());
    for (int i = 0; i < 25; ++i) EXPECT_EQ(0, manager.GetFunctionPriority(static_cast<CKMANAGER_FUNCTIONS>(1u << i)));
}

TEST_F(CKBaseManagerTest, FixedSdkContextDispatchesDerivedVirtualCallbacksThroughTheBaseInterface) {
    ManagerEvents events; ManagerSlot<ManagerProbe> slot(Context, BaseManagerGuid()); slot.Get()->Events = &events;
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get()));
    Context->ExecuteManagersOnCKInit(); Context->ExecuteManagersPreProcess(); Context->ExecuteManagersPostProcess(); Context->ExecuteManagersOnCKEnd();
    EXPECT_GE(events.init, 1); EXPECT_EQ(1, events.pre); EXPECT_EQ(1, events.post); EXPECT_EQ(1, events.end);
}

TEST_F(CKBaseManagerTest, VirtualDeletionRunsTheDerivedDestructor) {
    ManagerEvents events; ManagerProbe *probe = new ManagerProbe(nullptr, BaseManagerGuid(), "virtual"); probe->Events = &events;
    CKBaseManager *base = probe; delete base; EXPECT_EQ(1, events.destroyed);
}

TEST_F(CKBaseManagerTest, DestroyingAnActiveManagerRemovesItsGuidAndDispatchEntries) {
    ManagerEvents events; CKGUID guid = BaseManagerGuid(); ManagerSlot<ManagerProbe> slot(Context, guid); slot.Get()->Events = &events;
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get())); ASSERT_TRUE(Context->m_ManagerList[CKMANAGER_INDEX_PreProcess].IsHere(slot.Address()));
    slot.Destroy();
    EXPECT_EQ(nullptr, Context->GetManagerByGuid(guid)); EXPECT_FALSE(Context->m_InactiveManagers.IsHere(slot.Address()));
    EXPECT_FALSE(Context->m_ManagerList[CKMANAGER_INDEX_PreProcess].IsHere(slot.Address()));
}

TEST_F(CKBaseManagerTest, DestroyingAnInactiveManagerRemovesItsInactiveEntry) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> slot(Context, guid);
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get())); Context->ActivateManager(slot.Get(), FALSE);
    ASSERT_TRUE(Context->m_InactiveManagers.IsHere(slot.Address())); int before = Context->GetInactiveManagerCount();
    slot.Destroy(); EXPECT_EQ(before - 1, Context->GetInactiveManagerCount()); EXPECT_FALSE(Context->m_InactiveManagers.IsHere(slot.Address()));
    EXPECT_EQ(nullptr, Context->GetManagerByGuid(guid));
}

TEST_F(CKBaseManagerTest, DestroyingAnInactiveDuplicatePreservesTheActiveManagerWithTheSameGuid) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> active(Context, guid), duplicate(Context, guid);
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(active.Get())); ASSERT_EQ(CKERR_ALREADYPRESENT, Context->RegisterNewManager(duplicate.Get()));
    duplicate.Destroy(); EXPECT_EQ(active.Get(), Context->GetManagerByGuid(guid)); EXPECT_FALSE(Context->m_InactiveManagers.IsHere(duplicate.Address()));
    // Restore the still-live peer after observations if the old destructor erased it.
    if (!Context->GetManagerByGuid(guid)) Context->m_ManagerTable.Insert(guid, active.Get());
}

TEST_F(CKBaseManagerTest, DestroyingAPromotedDuplicateKeepsTheDisplacedManagerInactive) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> first(Context, guid), second(Context, guid);
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(first.Get())); ASSERT_EQ(CKERR_ALREADYPRESENT, Context->RegisterNewManager(second.Get()));
    Context->ActivateManager(second.Get(), TRUE); ASSERT_EQ(second.Get(), Context->GetManagerByGuid(guid));
    second.Destroy(); EXPECT_EQ(nullptr, Context->GetManagerByGuid(guid)); EXPECT_TRUE(Context->m_InactiveManagers.IsHere(first.Address()));
}

TEST_F(CKBaseManagerTest, DestroyingADisplacedInactiveManagerKeepsItsActiveReplacement) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> first(Context, guid), second(Context, guid);
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(first.Get())); ASSERT_EQ(CKERR_ALREADYPRESENT, Context->RegisterNewManager(second.Get()));
    Context->ActivateManager(second.Get(), TRUE); first.Destroy();
    EXPECT_EQ(second.Get(), Context->GetManagerByGuid(guid)); EXPECT_FALSE(Context->m_InactiveManagers.IsHere(first.Address()));
    if (!Context->GetManagerByGuid(guid)) Context->m_ManagerTable.Insert(guid, second.Get());
}

TEST_F(CKBaseManagerTest, ContextShutdownModeSkipsRegistryMutation) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> slot(Context, guid); ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get()));
    CKBOOL previous = Context->m_Init; Context->m_Init = TRUE; slot.Destroy(); Context->m_Init = previous;
    EXPECT_EQ(slot.Address(), Context->GetManagerByGuid(guid));
}

TEST_F(CKBaseManagerTest, DestroyingAnUnregisteredUniqueManagerKeepsOtherRegisteredManagers) {
    CKGUID guid = BaseManagerGuid(); ManagerSlot<> active(Context, guid), detached(Context, BaseManagerGuid());
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(active.Get())); int count = Context->GetManagerCount();
    detached.Destroy(); EXPECT_EQ(active.Get(), Context->GetManagerByGuid(guid)); EXPECT_EQ(count, Context->GetManagerCount());
}

TEST_F(CKBaseManagerTest, DestructionDoesNotImplicitlyCallTheEndHook) {
    ManagerEvents events; ManagerSlot<ManagerProbe> slot(Context, BaseManagerGuid()); slot.Get()->Events = &events;
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get())); slot.Destroy(); EXPECT_EQ(0, events.end); EXPECT_EQ(1, events.destroyed);
}

TEST_F(CKBaseManagerTest, StartProfileResetsTheTimerWithoutResettingAccumulatedTime) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "profile"); manager.m_ProcessingTime = 19.25f;
    Uint64 before = ManagerTimerProbe::Start(manager.m_Timer); SDL_Delay(2); manager.StartProfile();
    EXPECT_GT(ManagerTimerProbe::Start(manager.m_Timer), before); EXPECT_FLOAT_EQ(19.25f, manager.m_ProcessingTime);
}

TEST_F(CKBaseManagerTest, StopProfileAddsElapsedMillisecondsWithoutRestartingTheTimer) {
    CKBaseManager manager(nullptr, BaseManagerGuid(), "profile"); manager.m_ProcessingTime = 17.5f; manager.StartProfile(); SDL_Delay(2);
    Uint64 start = ManagerTimerProbe::Start(manager.m_Timer);
    for (int i = 0; i < 2; ++i) {
        float accumulated = manager.m_ProcessingTime, before = manager.m_Timer.Current(); manager.StopProfile(); float after = manager.m_Timer.Current();
        EXPECT_GE(manager.m_ProcessingTime, accumulated + before - .001f); EXPECT_LE(manager.m_ProcessingTime, accumulated + after + .001f);
        EXPECT_EQ(start, ManagerTimerProbe::Start(manager.m_Timer));
    }
}

TEST_F(CKBaseManagerTest, PointerDestroyHelperDestroysTheRealObjectAndReturnsContextErrors) {
    ManagerSlot<> slot(Context, BaseManagerGuid()); CKObject *object = Object(); ASSERT_NE(nullptr, object); CK_ID id = object->GetID();
    EXPECT_EQ(CK_OK, slot.Get()->CKDestroyObject(object)); EXPECT_EQ(nullptr, Context->GetObject(id));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, slot.Get()->CKDestroyObject(static_cast<CKObject *>(nullptr)));
}

TEST_F(CKBaseManagerTest, IdDestroyHelperDestroysTheRealObjectAndReturnsContextErrors) {
    ManagerSlot<> slot(Context, BaseManagerGuid()); CKObject *object = Object(); ASSERT_NE(nullptr, object); CK_ID id = object->GetID();
    EXPECT_EQ(CK_OK, slot.Get()->CKDestroyObject(id)); EXPECT_EQ(nullptr, Context->GetObject(id));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, slot.Get()->CKDestroyObject(static_cast<CK_ID>(0)));
}

TEST_F(CKBaseManagerTest, BatchDestroyHelperForwardsTheArrayAndCount) {
    ManagerSlot<> slot(Context, BaseManagerGuid()); CKObject *first = Object(), *second = Object(), *retained = Object();
    ASSERT_NE(nullptr, first); ASSERT_NE(nullptr, second); ASSERT_NE(nullptr, retained);
    CK_ID ids[] = {first->GetID(), second->GetID(), retained->GetID()};
    EXPECT_EQ(CK_OK, slot.Get()->CKDestroyObjects(ids, 2)); EXPECT_EQ(nullptr, Context->GetObject(ids[0])); EXPECT_EQ(nullptr, Context->GetObject(ids[1]));
    EXPECT_EQ(retained, Context->GetObject(ids[2]));
}

TEST_F(CKBaseManagerTest, ObjectLookupUsesTheBorrowedContextAndReturnsNullForDeletedIds) {
    ManagerSlot<> slot(Context, BaseManagerGuid()); CKObject *object = Object(); ASSERT_NE(nullptr, object); CK_ID id = object->GetID();
    EXPECT_EQ(object, slot.Get()->CKGetObject(id)); EXPECT_EQ(nullptr, slot.Get()->CKGetObject(0));
    CKDestroyObject(object); EXPECT_EQ(nullptr, slot.Get()->CKGetObject(id));
}

TEST_F(CKBaseManagerTest, DestroyHelperForwardsTheNoNotificationFlagToTheRealContext) {
    ManagerEvents events; ManagerSlot<ManagerProbe> slot(Context, BaseManagerGuid()); slot.Get()->Events = &events;
    ASSERT_EQ(CK_OK, Context->RegisterNewManager(slot.Get())); CKObject *object = Object(); ASSERT_NE(nullptr, object);
    EXPECT_EQ(CK_OK, slot.Get()->CKDestroyObject(object, CK_DESTROY_NONOTIFY)); EXPECT_EQ(0, events.beforeDelete); EXPECT_EQ(0, events.afterDelete);
    object = Object(); ASSERT_NE(nullptr, object); EXPECT_EQ(CK_OK, slot.Get()->CKDestroyObject(object));
    EXPECT_EQ(1, events.beforeDelete); EXPECT_EQ(1, events.afterDelete);
}

TEST_F(CKBaseManagerTest, CompilerCopiesPreserveDetachedNullNameFieldsWithoutRegistering) {
    CKBaseManager source(nullptr, BaseManagerGuid(), nullptr); source.m_ProcessingTime = 9.5f; source.m_Timer.Current();
    CKBaseManager copied(source), assigned(nullptr, BaseManagerGuid(), nullptr); assigned = source;
    EXPECT_EQ(source.GetGuid(), copied.GetGuid()); EXPECT_EQ(source.GetGuid(), assigned.GetGuid());
    EXPECT_EQ(nullptr, copied.GetName()); EXPECT_EQ(nullptr, assigned.GetName()); EXPECT_EQ(nullptr, copied.m_Context); EXPECT_EQ(nullptr, assigned.m_Context);
    EXPECT_FLOAT_EQ(9.5f, copied.m_ProcessingTime); EXPECT_FLOAT_EQ(9.5f, assigned.m_ProcessingTime);
    EXPECT_EQ(ManagerTimerProbe::Start(source.m_Timer), ManagerTimerProbe::Start(copied.m_Timer));
    EXPECT_EQ(ManagerTimerProbe::Start(source.m_Timer), ManagerTimerProbe::Start(assigned.m_Timer));
}
