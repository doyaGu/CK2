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

class TestDependenciesContext : public CKDependenciesContext {
public:
    explicit TestDependenciesContext(CKContext *context) : CKDependenciesContext(context) {}

    void SetMapping(CK_ID source, CK_ID target) {
        m_MapID.Insert(source, target, TRUE);
    }
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

TEST_F(CKRuntimeFixture, DependenciesContextRemapIdMappedToZeroClearsId) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKObject *obj = context_->CreateObject(
        CKCID_OBJECT,
        MakeUniqueName("DepsRemapIdZero").c_str(),
        CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, obj);

    TestDependenciesContext depsContext(context_);
    CK_ID id = obj->GetID();
    depsContext.SetMapping(id, 0);

    EXPECT_EQ((CK_ID)0, depsContext.RemapID(id));
    EXPECT_EQ((CK_ID)0, id);
}

TEST_F(CKRuntimeFixture, DependenciesContextRemapMappedToZeroReturnsNull) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKObject *obj = context_->CreateObject(
        CKCID_OBJECT,
        MakeUniqueName("DepsRemapObjZero").c_str(),
        CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, obj);

    TestDependenciesContext depsContext(context_);
    depsContext.SetMapping(obj->GetID(), 0);

    EXPECT_EQ(nullptr, depsContext.Remap(obj));
}

TEST_F(CKRuntimeFixture, DataArrayRemapClearsMappedToZeroEntries) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, MakeUniqueName("DepsRemapArray").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_OBJECT, "ObjCol");
    array->InsertColumn(-1, CKARRAYTYPE_PARAMETER, "ParamCol", CKPGUID_INT);
    array->AddRow();

    CKObject *obj = context_->CreateObject(
        CKCID_OBJECT,
        MakeUniqueName("DepsRemapArrayObj").c_str(),
        CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, obj);
    ASSERT_TRUE(array->SetElementObject(0, 0, obj));

    CKUINTPTR *paramCell = array->GetElement(0, 1);
    ASSERT_NE(nullptr, paramCell);
    CKObject *paramObject = reinterpret_cast<CKObject *>(*paramCell);
    ASSERT_NE(nullptr, paramObject);

    TestDependenciesContext depsContext(context_);
    depsContext.SetOperationMode(CK_DEPENDENCIES_COPY);

    CKDependencies fullDeps;
    fullDeps.m_Flags = CK_DEPENDENCIES_FULL;
    depsContext.StartDependencies(&fullDeps);
    depsContext.SetMapping(obj->GetID(), 0);
    depsContext.SetMapping(paramObject->GetID(), 0);

    EXPECT_EQ(CK_OK, array->RemapDependencies(depsContext));
    depsContext.StopDependencies();

    CK_ID objectCellId = -1;
    EXPECT_TRUE(array->GetElementValue(0, 0, &objectCellId));
    EXPECT_EQ((CK_ID)0, objectCellId);

    paramCell = array->GetElement(0, 1);
    ASSERT_NE(nullptr, paramCell);
    EXPECT_EQ((CKUINTPTR)0, *paramCell);
}

TEST_F(CKRuntimeFixture, BehaviorRemapClearsSceneLinksWhenOwnerMappedToZero) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKScene *scene = static_cast<CKScene *>(
        context_->CreateObject(CKCID_SCENE, MakeUniqueName("BehaviorRemapScene").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, scene);

    CKBeObject *owner = static_cast<CKBeObject *>(
        context_->CreateObject(CKCID_BEOBJECT, MakeUniqueName("BehaviorRemapOwner").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, owner);
    scene->AddObject(owner);

    CKBehavior *script = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, MakeUniqueName("BehaviorRemapScript").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, script);
    script->SetType(CKBEHAVIORTYPE_SCRIPT);
    ASSERT_EQ(CK_OK, owner->AddScript(script));
    ASSERT_EQ(owner, script->GetOwner());
    ASSERT_TRUE(script->IsInScene(scene));

    TestDependenciesContext depsContext(context_);
    depsContext.SetMapping(owner->GetID(), 0);

    EXPECT_EQ(CK_OK, script->RemapDependencies(depsContext));
    EXPECT_EQ(nullptr, script->GetOwner());
    EXPECT_FALSE(script->IsInScene(scene));
}

TEST_F(CKRuntimeFixture, ParameterRemapClearsObjectValueWhenMappedToZero) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKParameter *param = static_cast<CKParameter *>(
        context_->CreateObject(CKCID_PARAMETER, MakeUniqueName("ParamRemapObjectValue").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, param);

    CKObject *obj = context_->CreateObject(
        CKCID_OBJECT,
        MakeUniqueName("ParamRemapTarget").c_str(),
        CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, obj);

    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);
    CKParameterType objectType = pm->ParameterGuidToType(CKPGUID_OBJECT);
    ASSERT_GE(objectType, 0);
    param->SetType(objectType);

    CK_ID objectId = obj->GetID();
    ASSERT_EQ(CK_OK, param->SetValue(&objectId, sizeof(objectId)));

    TestDependenciesContext depsContext(context_);
    depsContext.SetMapping(obj->GetID(), 0);

    EXPECT_EQ(CK_OK, param->RemapDependencies(depsContext));

    CK_ID *storedId = reinterpret_cast<CK_ID *>(param->GetReadDataPtr());
    ASSERT_NE(nullptr, storedId);
    EXPECT_EQ((CK_ID)0, *storedId);
}

TEST_F(CKRuntimeFixture, GroupCopyReplacesMembershipLinks) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKGroup *srcGroup = static_cast<CKGroup *>(
        context_->CreateObject(CKCID_GROUP, MakeUniqueName("SrcGroupCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKGroup *dstGroup = static_cast<CKGroup *>(
        context_->CreateObject(CKCID_GROUP, MakeUniqueName("DstGroupCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, srcGroup);
    ASSERT_NE(nullptr, dstGroup);

    CKBeObject *srcMember = static_cast<CKBeObject *>(
        context_->CreateObject(CKCID_BEOBJECT, MakeUniqueName("SrcGroupMember").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKBeObject *dstMember = static_cast<CKBeObject *>(
        context_->CreateObject(CKCID_BEOBJECT, MakeUniqueName("DstGroupMember").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, srcMember);
    ASSERT_NE(nullptr, dstMember);

    ASSERT_EQ(CK_OK, srcGroup->AddObject(srcMember));
    ASSERT_EQ(CK_OK, dstGroup->AddObject(dstMember));
    ASSERT_TRUE(srcMember->IsInGroup(srcGroup));
    ASSERT_TRUE(dstMember->IsInGroup(dstGroup));

    CKDependenciesContext depsContext(context_);
    EXPECT_EQ(CK_OK, dstGroup->Copy(*srcGroup, depsContext));

    EXPECT_EQ(1, dstGroup->GetObjectCount());
    EXPECT_EQ(srcMember, dstGroup->GetObject(0));
    EXPECT_TRUE(srcMember->IsInGroup(dstGroup));
    EXPECT_FALSE(dstMember->IsInGroup(dstGroup));
}

TEST_F(CKRuntimeFixture, DataArrayCopyClearsDestinationBeforeClone) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKDataArray *src = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, MakeUniqueName("SrcArrayCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKDataArray *dst = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, MakeUniqueName("DstArrayCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, src);
    ASSERT_NE(nullptr, dst);

    src->InsertColumn(-1, CKARRAYTYPE_INT, "SrcCol");
    src->AddRow();
    int srcValue = 42;
    ASSERT_TRUE(src->SetElementValue(0, 0, &srcValue, sizeof(srcValue)));

    dst->InsertColumn(-1, CKARRAYTYPE_INT, "DstCol");
    dst->AddRow();
    dst->AddRow();
    int dstValue = 7;
    ASSERT_TRUE(dst->SetElementValue(0, 0, &dstValue, sizeof(dstValue)));

    CKDependenciesContext depsContext(context_);
    EXPECT_EQ(CK_OK, dst->Copy(*src, depsContext));

    EXPECT_EQ(src->GetColumnCount(), dst->GetColumnCount());
    EXPECT_EQ(src->GetRowCount(), dst->GetRowCount());

    int copiedValue = 0;
    ASSERT_TRUE(dst->GetElementValue(0, 0, &copiedValue));
    EXPECT_EQ(srcValue, copiedValue);
}

TEST_F(CKRuntimeFixture, ParameterOutCopyReplacesDestinationList) {
    ASSERT_EQ(CK_OK, context_->ClearAll());

    CKParameterOut *srcOut = static_cast<CKParameterOut *>(
        context_->CreateObject(CKCID_PARAMETEROUT, MakeUniqueName("SrcParameterOutCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKParameterOut *dstOut = static_cast<CKParameterOut *>(
        context_->CreateObject(CKCID_PARAMETEROUT, MakeUniqueName("DstParameterOutCopy").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKParameter *srcDest = static_cast<CKParameter *>(
        context_->CreateObject(CKCID_PARAMETER, MakeUniqueName("SrcParameterDest").c_str(), CK_OBJECTCREATION_DYNAMIC));
    CKParameter *dstDest = static_cast<CKParameter *>(
        context_->CreateObject(CKCID_PARAMETER, MakeUniqueName("DstParameterDest").c_str(), CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, srcOut);
    ASSERT_NE(nullptr, dstOut);
    ASSERT_NE(nullptr, srcDest);
    ASSERT_NE(nullptr, dstDest);

    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);
    CKParameterType intType = pm->ParameterGuidToType(CKPGUID_INT);
    ASSERT_GE(intType, 0);
    srcOut->SetType(intType);
    dstOut->SetType(intType);
    srcDest->SetType(intType);
    dstDest->SetType(intType);

    ASSERT_EQ(CK_OK, srcOut->AddDestination(srcDest, TRUE));
    ASSERT_EQ(CK_OK, dstOut->AddDestination(dstDest, TRUE));

    CKDependenciesContext depsContext(context_);
    EXPECT_EQ(CK_OK, dstOut->Copy(*srcOut, depsContext));

    EXPECT_EQ(1, dstOut->GetDestinationCount());
    EXPECT_EQ(srcDest, dstOut->GetDestination(0));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
