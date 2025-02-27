#include "CKContext.h"

#include "CKPluginManager.h"
#include "CKBaseManager.h"
#include "CKObjectManager.h"
#include "CKParameterManager.h"
#include "CKAttributeManager.h"
#include "CKTimeManager.h"
#include "CKMessageManager.h"
#include "CKBehaviorManager.h"
#include "CKRenderManager.h"
#include "CKPathManager.h"
#include "CKInterfaceManager.h"
#include "CKGlobals.h"
#include "CKFile.h"
#include "CKBitmapReader.h"
#include "CKDebugContext.h"
#include "CKLevel.h"
#include "CKRenderContext.h"
#include "CKBehavior.h"
#include "CKCharacter.h"
#include "CKSynchroObject.h"
#include "CKParameterOperation.h"

extern XClassInfoArray g_CKClassInfo;
extern CKPluginManager g_ThePluginManager;

CKObject *CKContext::CreateObject(CK_CLASSID cid, CKSTRING Name, CK_OBJECTCREATION_OPTIONS Options,
                                  CK_CREATIONMODE *Res) {
    CKObject *obj = nullptr;
    CK_CREATIONMODE mode = CKLOAD_OK;
    char *buffer = GetStringBuffer(512);
    unsigned int objectCreationOption = (Options & CK_OBJECTCREATION_FLAGSMASK);
    CKClassDesc &classDesc = g_CKClassInfo[cid];
    CKDWORD defaultOptions = classDesc.DefaultOptions;
    if (defaultOptions & CK_GENERALOPTIONS_NODUPLICATENAMECHECK)
        objectCreationOption = CK_OBJECTCREATION_NONAMECHECK;
    if (defaultOptions & CK_GENERALOPTIONS_AUTOMATICUSECURRENT)
        objectCreationOption = CK_OBJECTCREATION_USECURRENT;

    switch (objectCreationOption) {
    case CK_OBJECTCREATION_RENAME: {
        obj = GetObjectByNameAndClass(Name, cid);
        if (!obj)
            break;
        GetSecureName(buffer, Name, cid);
        mode = CKLOAD_RENAME;
        break;
    }
    case CK_OBJECTCREATION_USECURRENT: {
        obj = GetObjectByNameAndClass(Name, cid);
        if (!obj)
            break;
        mode = CKLOAD_USECURRENT;
        break;
    }
    case CK_OBJECTCREATION_ASK: {
        mode = LoadVerifyObjectUnicity(Name, cid, buffer, &obj);
        break;
    }
    case CK_OBJECTCREATION_NONAMECHECK:
    case CK_OBJECTCREATION_REPLACE:
    default:
        break;
    }

    if (mode != CKLOAD_USECURRENT) {
        if (classDesc.CreationFct) {
            m_InDynamicCreationMode = !!(Options & CK_OBJECTCREATION_ASK);
            obj = classDesc.CreationFct(this);
            m_InDynamicCreationMode = FALSE;
            if (Options & CK_OBJECTCREATION_DYNAMIC)
                obj->ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
            m_ObjectManager->FinishRegisterObject(obj);
        }
    }

    if (obj) {
        if (Name != nullptr) {
            if (mode & CKLOAD_RENAME)
                obj->SetName(buffer);
            else
                obj->SetName(Name);
        }
    }

    if (Res)
        *Res = mode;
    return obj;
}

CKObject *CKContext::CopyObject(CKObject *src, CKDependencies *Dependencies, CKSTRING AppendName,
                                CK_OBJECTCREATION_OPTIONS Options) {
    if (!src)
        return nullptr;

    CKDependenciesContext depContext(this);
    depContext.SetCreationMode(Options);
    depContext.SetOperationMode(CK_DEPENDENCIES_COPY);
    depContext.StartDependencies(Dependencies);
    depContext.AddObjects(&src->m_ID, 1);
    depContext.Copy(AppendName);
    depContext.StopDependencies();
    return depContext.Remap(src);
}

const XObjectArray &CKContext::CopyObjects(const XObjectArray &SrcObjects, CKDependencies *Dependencies,
                                           CK_OBJECTCREATION_OPTIONS Options, CKSTRING AppendName) {
    CKDependenciesContext depContext(this);
    depContext.SetCreationMode(Options);
    depContext.SetOperationMode(CK_DEPENDENCIES_COPY);
    depContext.StartDependencies(Dependencies);
    depContext.AddObjects(SrcObjects.Begin(), SrcObjects.Size());
    depContext.Copy(AppendName);
    m_CopyObjects.Clear();
    const int count = depContext.GetObjectsCount();
    for (int i = 0; i < count; i++) {
        CKObject *obj = depContext.GetObjects(i);
        CK_ID id = obj ? obj->GetID() : 0;
        m_CopyObjects.PushBack(id);
    }
    depContext.StopDependencies();
    return m_CopyObjects;
}

CKObject *CKContext::GetObject(CK_ID ObjID) {
    if (ObjID > m_ObjectManager->GetObjectsCount())
        return NULL;
    return m_ObjectManager->m_Objects[ObjID];
}

int CKContext::GetObjectCount() {
    return m_ObjectManager->GetObjectsCount();
}

int CKContext::GetObjectSize(CKObject *obj) {
    if (obj)
        return obj->GetMemoryOccupation();
    return 0;
}

CKERROR CKContext::DestroyObject(CKObject *obj, CKDWORD Flags, CKDependencies *depoptions) {
    if (!obj)
        return CKERR_INVALIDPARAMETER;
    return DestroyObjects(&obj->m_ID, 1, Flags, depoptions);
}

CKERROR CKContext::DestroyObject(CK_ID id, CKDWORD Flags, CKDependencies *depoptions) {
    if (id == 0)
        return CKERR_INVALIDPARAMETER;
    return DestroyObjects(&id, 1, Flags, depoptions);
}

CKERROR CKContext::DestroyObjects(CK_ID *obj_ids, int Count, CKDWORD Flags, CKDependencies *depoptions) {
    if (m_DeferDestroyObjects) {
        DeferredDestroyObjects(obj_ids, Count, depoptions, Flags);
    } else if (!PrepareDestroyObjects(obj_ids, Count, Flags, depoptions)) {
        FinishDestroyObjects(m_DestroyObjectFlag);
    }
    return CK_OK;
}

void CKContext::DestroyAllDynamicObjects() {
    m_ObjectManager->DeleteAllDynamicObjects();
}

void CKContext::ChangeObjectDynamic(CKObject *iObject, CKBOOL iSetDynamic) {
    if (iSetDynamic)
        m_ObjectManager->SetDynamic(iObject);
    else
        m_ObjectManager->UnSetDynamic(iObject);
}

const XObjectPointerArray &CKContext::CKFillObjectsUnused() {
    CKDependencies deps;
    deps.m_Flags = CK_DEPENDENCIES_FULL;
    CKDependenciesContext depContext(this);
    depContext.SetOperationMode(CK_DEPENDENCIES_BUILD);
    depContext.StartDependencies(&deps);

    CKRenderContext *dev = GetPlayerRenderContext();
    dev->PrepareDependencies(depContext);

    XObjectPointerArray objects;

    objects += GetObjectListByType(CKCID_DATAARRAY, FALSE);
    objects += GetObjectListByType(CKCID_GROUP, FALSE);
    objects += GetObjectListByType(CKCID_BEHAVIOR, FALSE);

    objects.Prepare(depContext);
    m_ObjectsUnused.Clear();

    CKScene *currentScene = GetCurrentScene();

    const int count = m_ObjectManager->GetObjectsCount();
    for (int i = 0; i < count; ++i) {
        CKObject *obj = m_ObjectManager->GetObject(i);
        if (!obj)
            continue;

        if (!depContext.IsDependenciesHere(obj->GetID())) {
            if (!CKIsChildClassOf(obj, CKCID_SCENEOBJECT) ||
                ((CKSceneObject *)obj)->IsInScene(currentScene)) {
                m_ObjectsUnused.PushBack(obj);
            }
        }
    }

    depContext.StopDependencies();
    return m_ObjectsUnused;
}

CKObject *CKContext::GetObjectByName(CKSTRING name, CKObject *previous) {
    return m_ObjectManager->GetObjectByName(name, previous);
}

CKObject *CKContext::GetObjectByNameAndClass(CKSTRING name, CK_CLASSID cid, CKObject *previous) {
    return m_ObjectManager->GetObjectByName(name, cid, previous);
}

CKObject *CKContext::GetObjectByNameAndParentClass(CKSTRING name, CK_CLASSID pcid, CKObject *previous) {
    return m_ObjectManager->GetObjectByNameAndParentClass(name, pcid, previous);
}

const XObjectPointerArray &CKContext::GetObjectListByType(CK_CLASSID cid, CKBOOL derived) {
    m_ObjectsUnused.Clear();
    m_ObjectManager->GetObjectListByType(cid, m_ObjectsUnused, derived);
    return m_ObjectsUnused;
}

int CKContext::GetObjectsCountByClassID(CK_CLASSID cid) {
    return m_ObjectManager->GetObjectsCountByClassID(cid);
}

CK_ID *CKContext::GetObjectsListByClassID(CK_CLASSID cid) {
    return m_ObjectManager->GetObjectsListByClassID(cid);
}

CKERROR CKContext::Play() {
    if (m_Playing)
        return CK_OK;

    CKLevel *currentLevel = nullptr;
    if (m_Reseted) {
        currentLevel = GetCurrentLevel();
        if (currentLevel) {
            if (!currentLevel->m_IsReseted)
                Reset();
            currentLevel->ActivateAllScript();
        }
    }

    m_Playing = TRUE;

    ExecuteManagersOnCKPlay();

    if (!currentLevel)
        return CKERR_NOCURRENTLEVEL;

    m_Reseted = FALSE;
    return CK_OK;
}

CKERROR CKContext::Pause() {
    if (m_Playing) {
        ExecuteManagersOnCKPause();
        m_Playing = FALSE;
    }
    return CK_OK;
}

CKERROR CKContext::Reset() {
    ExecuteManagersOnCKReset();

    m_Playing = FALSE;
    m_Reseted = TRUE;

    CKLevel *currentLevel = GetCurrentLevel();
    if (!currentLevel)
        return CKERR_NOCURRENTLEVEL;
    m_BehaviorContext.CurrentLevel = currentLevel;

    WarnAllBehaviors(CKM_BEHAVIORRESET);

    int characterCount = m_ObjectManager->GetObjectsCountByClassID(CKCID_CHARACTER);
    CK_ID *characterList = m_ObjectManager->GetObjectsListByClassID(CKCID_CHARACTER);
    for (int i = 0; i < characterCount; ++i) {
        CKCharacter *character = (CKCharacter *)m_ObjectManager->m_Objects[characterList[i]];
        if (character)
            character->FlushSecondaryAnimations();
    }

    int synchroCount = m_ObjectManager->GetObjectsCountByClassID(CKCID_SYNCHRO);
    CK_ID *synchroList = m_ObjectManager->GetObjectsListByClassID(CKCID_SYNCHRO);
    for (int i = 0; i < synchroCount; ++i) {
        CKSynchroObject *synchro = (CKSynchroObject *)m_ObjectManager->m_Objects[synchroList[i]];
        if (synchro)
            synchro->Reset();
    }

    int criticalSectionCount = m_ObjectManager->GetObjectsCountByClassID(CKCID_CRITICALSECTION);
    CK_ID *criticalSectionList = m_ObjectManager->GetObjectsListByClassID(CKCID_CRITICALSECTION);
    for (int i = 0; i < criticalSectionCount; ++i) {
        CKCriticalSectionObject *criticalSection = (CKCriticalSectionObject *)m_ObjectManager->m_Objects[
            criticalSectionList[i]];
        if (criticalSection)
            criticalSection->Reset();
    }

    int stateCount = m_ObjectManager->GetObjectsCountByClassID(CKCID_STATE);
    CK_ID *stateList = m_ObjectManager->GetObjectsListByClassID(CKCID_STATE);
    for (int i = 0; i < stateCount; ++i) {
        CKCriticalSectionObject *stateObject = (CKCriticalSectionObject *)m_ObjectManager->m_Objects[stateList[i]];
        if (stateObject)
            stateObject->Reset();
    }

    currentLevel->SetNextActiveScene(nullptr, CK_SCENEOBJECTACTIVITY_SCENEDEFAULT, CK_SCENEOBJECTRESET_RESET);
    currentLevel->Reset();

    CKScene *currentScene = currentLevel->GetCurrentScene();
    if (currentScene)
        currentLevel->LaunchScene(currentScene);

    ExecuteManagersOnCKPostReset();
    return CK_OK;
}

CKBOOL CKContext::IsPlaying() {
    return m_Playing;
}

CKBOOL CKContext::IsReseted() {
    return m_Reseted;
}

CKERROR CKContext::Process() {
    if (m_Playing) {
        VxTimeProfiler profiler;

        ExecuteManagersPreProcess();

        CKLevel *currentLevel = GetCurrentLevel();
        if (currentLevel) {
            currentLevel->PreProcess();
        }

        m_BehaviorManager->Execute(m_TimeManager->GetLastDeltaTime());

        ExecuteManagersPostProcess();

        m_Stats.ProcessTime = profiler.Current();
    }

    return CK_OK;
}

CKERROR CKContext::ClearAll() {
    m_RunTime = FALSE;
    m_InClearAll = TRUE;
    ExecuteManagersPreClearAll();
    m_CurrentLevel = 0;
    m_PVInformation = m_VirtoolsVersion;
    ExecuteManagersPostClearAll();

    XManagerArray managers = m_InactiveManagers;
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        CKBaseManager *manager = *it;
        ActivateManager(manager, TRUE);
    }
    return CK_OK;
}

CKBOOL CKContext::IsInClearAll() {
    return m_InClearAll;
}

CKLevel *CKContext::GetCurrentLevel() {
    return (CKLevel *)GetObject(m_CurrentLevel);
}

CKRenderContext *CKContext::GetPlayerRenderContext() {
    return m_BehaviorContext.CurrentRenderContext;
}

CKScene *CKContext::GetCurrentScene() {
    CKLevel *level = GetCurrentLevel();
    if (!level)
        return NULL;
    return level->GetCurrentScene();
}

void CKContext::SetCurrentLevel(CKLevel *level) {
    m_CurrentLevel = (level != NULL) ? level->GetID() : 0;
    m_BehaviorContext.CurrentLevel = level;
}

CKParameterIn *CKContext::CreateCKParameterIn(CKSTRING Name, CKParameterType type, CKBOOL Dynamic) {
    CKParameterIn *pIn = NULL;
    if (m_ParameterManager->CheckParamTypeValidity(type)) {
        pIn = (CKParameterIn *)CreateObject(
            CKCID_PARAMETERIN, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
        pIn->SetType(type);
    }
    return pIn;
}

CKParameterIn *CKContext::CreateCKParameterIn(CKSTRING Name, CKGUID guid, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterGuidToType(guid);
    return CreateCKParameterIn(Name, type, Dynamic);
}

CKParameterIn *CKContext::CreateCKParameterIn(CKSTRING Name, CKSTRING TypeName, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterNameToType(TypeName);
    return CreateCKParameterIn(Name, type, Dynamic);
}

CKParameterOut *CKContext::CreateCKParameterOut(CKSTRING Name, CKParameterType type, CKBOOL Dynamic) {
    if (!m_ParameterManager->CheckParamTypeValidity(type)) {
        return (CKParameterOut *)CreateObject(
            CKCID_PARAMETEROUT, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
    }

    CKParameterOut *pOut = (CKParameterOut *)CreateObject(
        CKCID_PARAMETEROUT, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
    pOut->SetType(type);
    return pOut;
}

CKParameterOut *CKContext::CreateCKParameterOut(CKSTRING Name, CKGUID guid, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterGuidToType(guid);
    return CreateCKParameterOut(Name, type, Dynamic);
}

CKParameterOut *CKContext::CreateCKParameterOut(CKSTRING Name, CKSTRING TypeName, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterNameToType(TypeName);
    return CreateCKParameterOut(Name, type, Dynamic);
}

CKParameterLocal *CKContext::CreateCKParameterLocal(CKSTRING Name, CKParameterType type, CKBOOL Dynamic) {
    CKParameterLocal *pLocal = NULL;
    if (m_ParameterManager->CheckParamTypeValidity(type)) {
        pLocal = (CKParameterLocal *)CreateObject(
            CKCID_PARAMETERLOCAL, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
        pLocal->SetType(type);
    }
    return pLocal;
}

CKParameterLocal *CKContext::CreateCKParameterLocal(CKSTRING Name, CKGUID guid, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterGuidToType(guid);
    return CreateCKParameterLocal(Name, type, Dynamic);
}

CKParameterLocal *CKContext::CreateCKParameterLocal(CKSTRING Name, CKSTRING TypeName, CKBOOL Dynamic) {
    CKParameterType type = m_ParameterManager->ParameterNameToType(TypeName);
    return CreateCKParameterLocal(Name, type, Dynamic);
}

CKParameterOperation *
CKContext::CreateCKParameterOperation(CKSTRING Name, CKGUID opguid, CKGUID ResGuid, CKGUID p1Guid, CKGUID p2Guid) {
    CKParameterOperation *pOperation = (CKParameterOperation *)CreateObject(CKCID_PARAMETEROPERATION, Name);
    pOperation->Reconstruct(Name, opguid, ResGuid, p1Guid, p2Guid);
    return pOperation;
}

CKFile *CKContext::CreateCKFile() {
    return new CKFile(this);
}

CKERROR CKContext::DeleteCKFile(CKFile *file) {
    if (!file)
        return CKERR_INVALIDPARAMETER;
    delete file;
    return CK_OK;
}

void CKContext::SetInterfaceMode(CKBOOL mode, CKUICALLBACKFCT CallBackFct, void *data) {
    m_InterfaceMode = mode;
    m_UICallBackFct = CallBackFct;
    m_InterfaceModeData = data;
}

CKBOOL CKContext::IsInInterfaceMode() {
    return m_InterfaceMode;
}

CKERROR CKContext::OutputToConsole(CKSTRING str, CKBOOL bBeep) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_OUTTOCONSOLE;
    cbs.DoBeep = bBeep;
    cbs.ConsoleString = str;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKERROR CKContext::OutputToConsoleEx(CKSTRING format, ...) {
    if (!m_UICallBackFct)
        return CK_OK;

    char *buf = GetStringBuffer(256);
    if (!buf)
        return CKERR_INVALIDPARAMETER;

    va_list args;
    va_start(args, format);
    vsnprintf(buf, 256, format, args);
    va_end(args);
    return OutputToConsole(buf, FALSE);
}

CKERROR CKContext::OutputToConsoleExBeep(CKSTRING format, ...) {
    if (!m_UICallBackFct)
        return CK_OK;

    char *buf = GetStringBuffer(256);
    if (!buf)
        return CKERR_INVALIDPARAMETER;

    va_list args;
    va_start(args, format);
    vsnprintf(buf, 256, format, args);
    va_end(args);
    return OutputToConsole(buf, TRUE);
}

CKERROR CKContext::OutputToInfo(CKSTRING format, ...) {
    if (!m_UICallBackFct)
        return CK_OK;

    char *buf = GetStringBuffer(256);
    if (!buf)
        return CKERR_INVALIDPARAMETER;

    va_list args;
    va_start(args, format);
    vsnprintf(buf, 256, format, args);
    va_end(args);

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_OUTTOINFOBAR;
    cbs.Param1 = 0;
    cbs.ConsoleString = buf;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKERROR CKContext::RefreshBuildingBlocks(const XArray<CKGUID> &iGuids) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_REFRESHBUILDINGBLOCKS;
    cbs.Param1 = (CKDWORD)iGuids.Begin();
    cbs.Param2 = iGuids.Size();
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKERROR CKContext::ShowSetup(CK_ID id) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_SHOWSETUP;
    cbs.ObjectID = id;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CK_ID CKContext::ChooseObject(void *dialogParentWnd) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_CHOOSEOBJECT;
    cbs.Param1 = (CKDWORD)dialogParentWnd;
    m_UICallBackFct(cbs, m_InterfaceModeData);
    return cbs.ObjectID;
}

CKERROR CKContext::Select(const XObjectArray &o, CKBOOL clearSelection) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_SELECT;
    cbs.Objects = (XObjectArray *)&o;
    cbs.ClearSelection = clearSelection;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKDWORD CKContext::SendInterfaceMessage(CKDWORD reason, CKDWORD param1, CKDWORD param2) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = reason;
    cbs.Param1 = param1;
    cbs.Param2 = param2;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKERROR CKContext::UICopyObjects(const XObjectArray &iObjects, CKBOOL iClearClipboard) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_COPYOBJECTS;
    cbs.Objects = (XObjectArray *)&iObjects;
    cbs.ClearSelection = iClearClipboard;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKERROR CKContext::UIPasteObjects(const XObjectArray &oObjects) {
    if (!m_UICallBackFct)
        return CK_OK;

    CKUICallbackStruct cbs;
    cbs.Reason = CKUIM_PASTEOBJECTS;
    cbs.Objects = (XObjectArray *)&oObjects;
    return m_UICallBackFct(cbs, m_InterfaceModeData);
}

CKRenderManager *CKContext::GetRenderManager() {
    if (!m_RenderManager)
        m_RenderManager = (CKRenderManager *)GetManagerByGuid(RENDER_MANAGER_GUID);
    return m_RenderManager;
}

CKBehaviorManager *CKContext::GetBehaviorManager() {
    return m_BehaviorManager;
}

CKParameterManager *CKContext::GetParameterManager() {
    return m_ParameterManager;
}

CKMessageManager *CKContext::GetMessageManager() {
    return m_MessageManager;
}

CKTimeManager *CKContext::GetTimeManager() {
    return m_TimeManager;
}

CKAttributeManager *CKContext::GetAttributeManager() {
    return m_AttributeManager;
}

CKPathManager *CKContext::GetPathManager() {
    return m_PathManager;
}

XManagerHashTableIt CKContext::GetManagers() {
    return m_ManagerTable.Begin();
}

CKBaseManager *CKContext::GetManagerByGuid(CKGUID guid) {
    XManagerHashTableIt it = m_ManagerTable.Find(guid);
    if (it == m_ManagerTable.End())
        return NULL;
    return *it;
}

CKBaseManager *CKContext::GetManagerByName(CKSTRING ManagerName) {
    for (XManagerHashTableIt it = m_ManagerTable.Begin(); it != m_ManagerTable.End(); ++it) {
        CKBaseManager *manager = *it;
        if (!strcmp(manager->GetName(), ManagerName))
            return manager;
    }
    for (int i = 0; i < m_InactiveManagers.Size(); ++i) {
        CKBaseManager *manager = m_InactiveManagers[i];
        if (!strcmp(manager->GetName(), ManagerName))
            return manager;
    }
    return NULL;
}

int CKContext::GetManagerCount() {
    return m_ManagerTable.Size();
}

CKBaseManager *CKContext::GetManager(int index) {
    int i = 0;
    for (XManagerHashTableIt it = m_ManagerTable.Begin(); it != m_ManagerTable.End(); ++it) {
        if (i++ == index)
            return *it;
        if (i > index)
            break;
    }
    return NULL;
}

CKBOOL CKContext::IsManagerActive(CKBaseManager *manager) {
    return manager && manager == GetManagerByGuid(manager->GetGuid());
}

CKBOOL CKContext::HasManagerDuplicates(CKBaseManager *manager) {
    if (m_InactiveManagers.Size() == 0)
        return FALSE;
    for (auto it = m_InactiveManagers.Begin(); it != m_InactiveManagers.End(); ++it) {
        if (*it == manager)
            return TRUE;
    }
    return FALSE;
}

void CKContext::ActivateManager(CKBaseManager *manager, CKBOOL activate) {
    if (!manager)
        return;

    CKGUID managerGuid = manager->m_ManagerGuid;
    CKBaseManager *existingManager = GetManagerByGuid(managerGuid);

    if (activate) {
        if (manager == existingManager)
            return;

        if (existingManager) {
            existingManager->OnCKEnd();
            m_ManagerTable.Remove(managerGuid);
            m_InactiveManagers.PushBack(existingManager);
        }

        m_ManagerTable.Insert(managerGuid, manager);
        m_InactiveManagers.Remove(manager);
        manager->OnCKInit();
    } else {
        if (existingManager != manager)
            return;

        manager->OnCKEnd();
        m_ManagerTable.Remove(managerGuid);
        m_InactiveManagers.PushBack(manager);
    }

    BuildSortedLists();
}

int CKContext::GetInactiveManagerCount() {
    return m_InactiveManagers.Size();
}

CKBaseManager *CKContext::GetInactiveManager(int index) {
    return m_InactiveManagers[index];
}

CKERROR CKContext::RegisterNewManager(CKBaseManager *manager) {
    if (!manager)
        return CKERR_INVALIDPARAMETER;

    XManagerHashTablePair pair = m_ManagerTable.TestInsert(manager->GetGuid(), manager);
    if (!pair.m_New) {
        m_InactiveManagers.PushBack(manager);
        return CKERR_ALREADYPRESENT;
    }

    BuildSortedLists();
    if (m_InitManagerOnRegister)
        manager->OnCKInit();
    return CK_OK;
}

void CKContext::EnableProfiling(CKBOOL enable) {
    m_ProfilingEnabled = enable;
}

CKBOOL CKContext::IsProfilingEnable() {
    return m_ProfilingEnabled;
}

void CKContext::GetProfileStats(CKStats *stats) {
    if (stats)
        *stats = m_ProfileStats;
}

void CKContext::UserProfileStart(CKDWORD UserSlot) {
    if (UserSlot < 8) {
        m_UserProfileTimers[UserSlot].Reset();
    }
}

float CKContext::UserProfileEnd(CKDWORD UserSlot) {
    if (UserSlot < 8) {
        float time = m_UserProfileTime[UserSlot] + m_UserProfileTimers[UserSlot].Current();
        m_UserProfileTime[UserSlot] = time;
        m_Stats.UserProfiles[UserSlot] = time;
        return time;
    }
    return 0.0f;
}

float CKContext::GetLastUserProfileTime(CKDWORD UserSlot) {
    if (UserSlot < 8)
        return m_UserProfileTime[UserSlot];
    return 0.0f;
}

CKSTRING CKContext::GetStringBuffer(int size) {
    if (m_StringBuffer.Length() < size) {
        m_StringBuffer.Resize(size);
    }
    return m_StringBuffer.Str();
}

CKGUID CKContext::GetSecureGuid() {
    CKGUID guid;

    do {
        guid.d1 = (rand() & 0xFFFF) | ((rand() & 0xFFFF) << 16);
        guid.d2 = (rand() & 0xFFFF) | ((rand() & 0xFFFF) << 16);
    } while (!CKGetObjectDeclarationFromGuid(guid) &&
        (m_ParameterManager->OperationGuidToCode(guid) >= 0 ||
            m_ParameterManager->ParameterGuidToType(guid) >= 0 ||
            GetManagerByGuid(guid) != NULL));


    return guid;
}

CKDWORD CKContext::GetStartOptions() {
    return m_StartOptions;
}

WIN_HANDLE CKContext::GetMainWindow() {
    return m_MainWindow;
}

int CKContext::GetSelectedRenderEngine() {
    return m_SelectedRenderEngine;
}

void CKContext::SetCompressionLevel(int level) {
    if (level > 0 && level < 10)
        m_CompressionLevel = level;
}

int CKContext::GetCompressionLevel() {
    return m_CompressionLevel;
}

void CKContext::SetFileWriteMode(CK_FILE_WRITEMODE mode) {
    m_FileWriteMode = mode;
}

CK_FILE_WRITEMODE CKContext::GetFileWriteMode() {
    return m_FileWriteMode;
}

CK_TEXTURE_SAVEOPTIONS CKContext::GetGlobalImagesSaveOptions() {
    return m_GlobalImagesSaveOptions;
}

void CKContext::SetGlobalImagesSaveOptions(CK_TEXTURE_SAVEOPTIONS Options) {
    if (Options != CKTEXTURE_USEGLOBAL)
        m_GlobalImagesSaveOptions = Options;
}

CKBitmapProperties *CKContext::GetGlobalImagesSaveFormat() {
    return m_GlobalImagesSaveFormat;
}

void CKContext::SetGlobalImagesSaveFormat(CKBitmapProperties *Format) {
    if (Format && Format->m_Size != 0) {
        CKDeletePointer(m_GlobalImagesSaveFormat);
        m_GlobalImagesSaveFormat = CKCopyBitmapProperties(Format);
    }
}

CK_SOUND_SAVEOPTIONS CKContext::GetGlobalSoundsSaveOptions() {
    return m_GlobalSoundsSaveOptions;
}

void CKContext::SetGlobalSoundsSaveOptions(CK_SOUND_SAVEOPTIONS Options) {
    if (Options != CKSOUND_USEGLOBAL)
        m_GlobalSoundsSaveOptions = Options;
}

CKERROR CKContext::Load(CKSTRING FileName, CKObjectArray *liste, CK_LOAD_FLAGS LoadFlags, CKGUID *ReaderGuid) {
    if (!FileName)
        return CKERR_INVALIDPARAMETER;
    m_LastFileLoaded = FileName;
    return g_ThePluginManager.Load(this, FileName, liste, LoadFlags, NULL, ReaderGuid);
}

CKERROR CKContext::Load(int BufferSize, void *MemoryBuffer, CKObjectArray *ckarray, CK_LOAD_FLAGS LoadFlags) {
    if (!MemoryBuffer || !ckarray)
        return CKERR_INVALIDPARAMETER;

    CKFile *file = CreateCKFile();
    if (!file)
        return CKERR_INVALIDFILE;
    CKERROR err = file->Load(MemoryBuffer, BufferSize, ckarray, LoadFlags);
    file->UpdateAndApplyAnimationsTo(NULL);
    DeleteCKFile(file);
    return err;
}

CKSTRING CKContext::GetLastFileLoaded() {
    return (CKSTRING)m_LastFileLoaded.CStr();
}

CKSTRING CKContext::GetLastCmoLoaded() {
    return (CKSTRING)m_LastCmoLoaded.CStr();
}

void CKContext::SetLastCmoLoaded(CKSTRING str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 4 && !strcmp(&str[len - 4], ".cmo"))
        m_LastCmoLoaded = str;
}

CKERROR CKContext::GetFileInfo(CKSTRING FileName, CKFileInfo *FileInfo) {
    if (!FileInfo)
        return CKERR_INVALIDPARAMETER;
    if (!FileName)
        return CKERR_INVALIDFILE;

    VxMemoryMappedFile file(FileName);
    if (!file.IsValid())
        return CKERR_INVALIDFILE;

    return GetFileInfo(file.GetFileSize(), file.GetBase(), FileInfo);
}

CKERROR CKContext::GetFileInfo(int BufferSize, void *MemoryBuffer, CKFileInfo *FileInfo) {
    if (!MemoryBuffer || !FileInfo)
        return CKERR_INVALIDPARAMETER;

    if (BufferSize < 64)
        return CKERR_INVALIDFILE;

    int header1[8];
    int header2[8];

    memcpy(header1, MemoryBuffer, sizeof(header1));
    memcpy(header2, (char *)MemoryBuffer + 32, sizeof(header2));

    if (header1[5]) {
        memset(header1, 0, sizeof(header1));
        memset(header2, 0, sizeof(header2));
    }

    if (header1[4] < 5)
        memset(header2, 0, sizeof(header2));

    FileInfo->ProductVersion = header2[5];
    FileInfo->ProductBuild = header2[6];
    FileInfo->FileWriteMode = header1[6];
    FileInfo->CKVersion = header1[3];
    FileInfo->FileVersion = header1[4];
    FileInfo->FileSize = BufferSize;
    FileInfo->ManagerCount = header2[2];
    FileInfo->ObjectCount = header2[3];
    FileInfo->MaxIDSaved = header2[4];
    FileInfo->Hdr1PackSize = header1[7];
    FileInfo->Hdr1UnPackSize = header2[1];
    FileInfo->DataUnPackSize = header2[1];
    FileInfo->DataPackSize = header2[0];
    FileInfo->Crc = header1[2];
    return CK_OK;
}

CKERROR CKContext::Save(CKSTRING FileName, CKObjectArray *liste, CKDWORD SaveFlags, CKDependencies *dependencies,
                        CKGUID *ReaderGuid) {
    CKObjectArray *saveList = nullptr;
    CKObjectArray *tempObjectArray = nullptr;

    if (dependencies) {
        CKDependenciesContext depContext(this);
        depContext.SetOperationMode(CK_DEPENDENCIES_SAVE);

        liste->Reset();
        while (!liste->EndOfList()) {
            CK_ID objectId = liste->GetDataId();
            depContext.AddObjects(&objectId, 1);
            liste->Next();
        }

        depContext.StartDependencies(dependencies);

        depContext.m_Objects.Prepare(depContext);

        tempObjectArray = CreateCKObjectArray();

        // TODO: Need fix
        for (auto it = depContext.m_MapID.Begin(); it != depContext.m_MapID.End(); ++it) {
            tempObjectArray->InsertRear(it.GetKey());
        }

        saveList = tempObjectArray;
    } else {
        saveList = liste;
    }

    CKERROR result = g_ThePluginManager.Save(this, FileName, saveList, SaveFlags, ReaderGuid);

    if (tempObjectArray)
        DeleteCKObjectArray(tempObjectArray);

    return result;
}

CKERROR CKContext::LoadAnimationOnCharacter(CKSTRING FileName, CKObjectArray *liste, CKCharacter *carac,
                                            CKGUID *ReaderGuid, CKBOOL AsDynamicObjects) {
    CKDWORD flags = CK_LOAD_ANIMATION;
    if (AsDynamicObjects)
        flags |= CK_LOAD_AS_DYNAMIC_OBJECT;
    return g_ThePluginManager.Load(this, FileName, liste, (CK_LOAD_FLAGS)flags, carac, ReaderGuid);
}

CKERROR CKContext::LoadAnimationOnCharacter(int BufferSize, void *MemoryBuffer, CKObjectArray *ckarray,
                                            CKCharacter *carac, CKBOOL AsDynamicObjects) {
    if (!MemoryBuffer || !ckarray)
        return CKERR_INVALIDPARAMETER;
    CKFile *file = CreateCKFile();
    if (!file)
        return CKERR_INVALIDFILE;

    CKDWORD flags = CK_LOAD_ANIMATION;
    if (AsDynamicObjects)
        flags |= CK_LOAD_AS_DYNAMIC_OBJECT;
    CKERROR err = file->Load(MemoryBuffer, BufferSize, ckarray, (CK_LOAD_FLAGS)flags);
    file->UpdateAndApplyAnimationsTo(carac);
    DeleteCKFile(file);
    return err;
}

void CKContext::SetAutomaticLoadMode(CK_LOADMODE GeneralMode, CK_LOADMODE _3DObjectsMode, CK_LOADMODE MeshMode,
                                     CK_LOADMODE MatTexturesMode) {
    m_GeneralLoadMode = GeneralMode;
    m_3DObjectsLoadMode = _3DObjectsMode;
    m_MeshLoadMode = MeshMode;
    m_MatTexturesLoadMode = MatTexturesMode;
}

void CKContext::SetUserLoadCallback(CK_USERLOADCALLBACK fct, void *Arg) {
    m_UserLoadCallBack = fct;
    m_UserLoadCallBackArgs = Arg;
}

CK_LOADMODE CKContext::LoadVerifyObjectUnicity(CKSTRING OldName, CK_CLASSID Cid, const CKSTRING NewName,
                                               CKObject **newobj) {
    if (!OldName)
        return CKLOAD_OK;

    CKClassDesc &classInfo = g_CKClassInfo[Cid];

    if (classInfo.DefaultOptions & CK_GENERALOPTIONS_NODUPLICATENAMECHECK)
        return CKLOAD_OK;

    CKObject *existingObj = GetObjectByNameAndClass(OldName, Cid);
    if (!existingObj)
        return CKLOAD_OK;

    if (newobj)
        *newobj = existingObj;

    if (classInfo.DefaultOptions & CK_GENERALOPTIONS_AUTOMATICUSECURRENT)
        return CKLOAD_USECURRENT;

    if (m_UserLoadCallBack)
        return m_UserLoadCallBack(Cid, OldName, NewName, newobj, m_UserLoadCallBackArgs);

    if (!(classInfo.DefaultOptions & CK_GENERALOPTIONS_CANUSECURRENTOBJECT)) {
        if (CKIsChildClassOf(Cid, CKCID_3DENTITY)) {
            if (m_3DObjectsLoadMode != CKLOAD_INVALID) {
                if (m_3DObjectsLoadMode == CKLOAD_RENAME)
                    GetSecureName(NewName, OldName, Cid);
                return m_3DObjectsLoadMode;
            }
        } else {
            if (m_GeneralLoadMode != CKLOAD_INVALID) {
                if (m_GeneralLoadMode == CKLOAD_RENAME)
                    GetSecureName(NewName, OldName, Cid);
                return m_GeneralLoadMode;
            }
        }

        if (m_GeneralRenameOption == CKLOAD_REPLACE)
            return CKLOAD_OK;
        if (m_GeneralRenameOption != CKLOAD_OK) {
            if (m_GeneralRenameOption == CKLOAD_RENAME) {
                GetSecureName(NewName, OldName, Cid);
                if (newobj)
                    *newobj = NULL;
                return CKLOAD_RENAME;
            } else if (m_GeneralRenameOption == CKLOAD_USECURRENT) {
                if (newobj)
                    *newobj = existingObj;
                return CKLOAD_USECURRENT;
            } else {
                return CKLOAD_OK;
            }
        }

        int rename = 0;

        CKInterfaceManager *im = (CKInterfaceManager *)GetManagerByGuid(INTERFACE_MANAGER_GUID);
        if (im) {
            rename = (CK_LOADMODE)im->DoRenameDialog(OldName, Cid);
        }

        switch (rename) {
        case 0:
            return CKLOAD_OK;
        case 1:
            m_GeneralRenameOption = CKLOAD_REPLACE;
            return CKLOAD_OK;
        case 2:
            GetSecureName(NewName, OldName, Cid);
            if (newobj)
                *newobj = NULL;
            return CKLOAD_RENAME;
        case 3:
            m_GeneralRenameOption = CKLOAD_RENAME;
            GetSecureName(NewName, OldName, Cid);
            if (newobj)
                *newobj = NULL;
            return CKLOAD_RENAME;
        case 4:
            if (newobj)
                *newobj = existingObj;
            return CKLOAD_USECURRENT;
        case 5:
            m_GeneralRenameOption = CKLOAD_USECURRENT;
            if (newobj)
                *newobj = existingObj;
            return CKLOAD_USECURRENT;
        default:
            break;
        }
    } else {
        if (CKIsChildClassOf(Cid, CKCID_MESH)) {
            if (m_MeshLoadMode != CKLOAD_INVALID) {
                if (m_MeshLoadMode == CKLOAD_RENAME)
                    GetSecureName(NewName, OldName, Cid);
                return m_MeshLoadMode;
            }
        } else if (m_MatTexturesLoadMode != CKLOAD_INVALID) {
            if (m_MatTexturesLoadMode == CKLOAD_RENAME)
                GetSecureName(NewName, OldName, Cid);
            return m_MatTexturesLoadMode;
        }

        int rename = 0;

        CKInterfaceManager *im = (CKInterfaceManager *)GetManagerByGuid(INTERFACE_MANAGER_GUID);
        if (im) {
            rename = (CK_LOADMODE)im->DoRenameDialog(OldName, Cid);
        }

        switch (rename) {
        case 0:
            return CKLOAD_OK;
        case 1:
            m_MatTexturesRenameOption = CKLOAD_REPLACE;
            return CKLOAD_OK;
        case 2:
            GetSecureName(NewName, OldName, Cid);
            if (newobj)
                *newobj = NULL;
            return CKLOAD_RENAME;
        case 3:
            m_MatTexturesRenameOption = CKLOAD_RENAME;
            GetSecureName(NewName, OldName, Cid);
            if (newobj)
                *newobj = NULL;
            return CKLOAD_RENAME;
        case 4:
            if (newobj)
                *newobj = existingObj;
            return CKLOAD_USECURRENT;
        case 5:
            m_MatTexturesRenameOption = CKLOAD_USECURRENT;
            if (newobj)
                *newobj = existingObj;
            return CKLOAD_USECURRENT;
        default:
            break;
        }
    }

    return CKLOAD_OK;
}

CKBOOL CKContext::IsInLoad() {
    return m_InLoad;
}

CKBOOL CKContext::IsInSave() {
    return m_Saving;
}

CKBOOL CKContext::IsRunTime() {
    return m_RunTime;
}

void CKContext::ClearManagersData() {
    CKRenderManager *renderManager = GetRenderManager();
    if (renderManager) {
        int count = renderManager->GetRenderContextCount();
        for (int i = 0; i < count; ++i) {
            CKRenderContext *renderContext = renderManager->GetRenderContext(i);
            renderManager->RemoveRenderContext(renderContext);
        }
    }
    m_ObjectManager->DeleteAllObjects();
    ExecuteManagersOnCKEnd();

    // TODO: Maybe need to clear m_ManagerTable and m_InactiveManagers
    for (auto it = m_ManagerTable.Begin(); it != m_ManagerTable.End(); ++it) {
        CKBaseManager *manager = *it;
        delete manager;
    }
    for (auto it = m_InactiveManagers.Begin(); it != m_InactiveManagers.End(); ++it) {
        CKBaseManager *manager = *it;
        delete manager;
    }
}

void CKContext::ExecuteManagersOnCKInit() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKInit];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKInit();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnCKEnd() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKEnd];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKEnd();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPreProcess() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PreProcess];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PreProcess();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPostProcess() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PostProcess];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PostProcess();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPreClearAll() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PreClearAll];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PreClearAll();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPostClearAll() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PostClearAll];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PostClearAll();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnSequenceDeleted(CK_ID *objids, int count) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnSequenceDeleted];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->SequenceDeleted(objids, count);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnSequenceToBeDeleted(CK_ID *objids, int count) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnSequenceToBeDeleted];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->SequenceToBeDeleted(objids, count);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnCKReset() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKReset];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKReset();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnCKPostReset() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKPostReset];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKPostReset();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnCKPause() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKPause];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKPause();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnCKPlay() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnCKPlay];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnCKPlay();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPreLaunchScene(CKScene *OldScene, CKScene *NewScene) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PreLaunchScene];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PreLaunchScene(OldScene, NewScene);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPostLaunchScene(CKScene *OldScene, CKScene *NewScene) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PostLaunchScene];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PostLaunchScene(OldScene, NewScene);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPreLoad() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PreLoad];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PreLoad();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPostLoad() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PostLoad];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PostLoad();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPreSave() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PreSave];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PreSave();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersPostSave() {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_PostSave];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->PostSave();
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnSequenceAddedToScene(CKScene *scn, CK_ID *objids, int coun) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnSequenceAddedToScene];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->SequenceAddedToScene(scn, objids, coun);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnSequenceRemovedFromScene(CKScene *scn, CK_ID *objids, int coun) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnSequenceRemovedFromScene];
    for (auto it = managers.Begin(); it != managers.End(); ++
         it) {
        m_CurrentManager = *it;
        m_CurrentManager->SequenceRemovedFromScene(scn, objids, coun);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnPreCopy(CKDependenciesContext *context) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnPreCopy];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnPreCopy(*context);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnPostCopy(CKDependenciesContext *context) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnPostCopy];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnPostCopy(*context);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnPreRender(CKRenderContext *dev) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnPreRender];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnPreRender(dev);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnPostRender(CKRenderContext *dev) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnPostRender];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnPostRender(dev);
    }
    m_CurrentManager = nullptr;
}

void CKContext::ExecuteManagersOnPostSpriteRender(CKRenderContext *dev) {
    XArray<CKBaseManager *> &managers = m_ManagerList[CKMANAGER_INDEX_OnPostSpriteRender];
    for (auto it = managers.Begin(); it != managers.End(); ++it) {
        m_CurrentManager = *it;
        m_CurrentManager->OnPostSpriteRender(dev);
    }
    m_CurrentManager = nullptr;
}

void CKContext::AddProfileTime(CK_PROFILE_CATEGORY cat, float time) {
    switch (cat) {
    case CK_PROFILE_RENDERTIME:
        m_Stats.RenderTime += time;
        break;
    case CK_PROFILE_IKTIME:
        m_Stats.IKManagement += time;
        break;
    case CK_PROFILE_ANIMATIONTIME:
        m_Stats.AnimationManagement += time;
        break;
    }
}

CKERROR CKContext::ProcessDebugStart(float delta_time) {
    ExecuteManagersPreProcess();
    CKLevel *level = GetCurrentLevel();
    if (level)
        level->PreProcess();
    return m_BehaviorManager->ExecuteDebugStart(delta_time);
}

CKBOOL CKContext::ProcessDebugStep() {
    if (m_Playing)
        return m_DebugContext->DebugStep();
    else
        return FALSE;
}

CKERROR CKContext::ProcessDebugEnd() {
    if (!m_Playing)
        return CKERR_PAUSED;
    if (!m_DebugContext->InDebug)
        return CKERR_PAUSED;
    m_DebugContext->Clear();
    ExecuteManagersPostProcess();
    return CK_OK;
}

CKDebugContext *CKContext::GetDebugContext() {
    return m_DebugContext;
}

void CKContext::SetVirtoolsVersion(CK_VIRTOOLS_VERSION ver, CKDWORD Build) {
    m_VirtoolsVersion = ver;
    m_VirtoolsBuild = Build;
    m_PVInformation = ver;
}

int CKContext::GetPVInformation() {
    return m_PVInformation;
}

CKBOOL CKContext::IsInDynamicCreationMode() {
    return m_InDynamicCreationMode;
}

int CKContext::WarnAllBehaviors(CKDWORD Message) {
    const int behaviorCount = m_ObjectManager->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *behaviorList = m_ObjectManager->GetObjectsListByClassID(CKCID_BEHAVIOR);

    int result = 0;
    for (int i = 0; i < behaviorCount; ++i) {
        CKBehavior *beh = (CKBehavior *)m_ObjectManager->GetObject(behaviorList[i]);
        if (!beh)
            continue;

        CKObject *owner = beh->GetOwner();
        if (!owner)
            continue;

        result = beh->CallCallbackFunction(Message);
    }
    return result;
}

void CKContext::GetSecureName(CKSTRING secureName, const char *name, CK_CLASSID cid) {
    XString baseName(name);
    int highestNumber = 0;

    int objCount = GetObjectsCountByClassID(cid);
    CK_ID *objList = GetObjectsListByClassID(cid);

    // Find the highest existing number suffix
    for (int i = 0; i < objCount; ++i) {
        CKObject *obj = GetObject(objList[i]);
        if (!obj || !obj->GetName())
            continue;

        const char *objName = obj->GetName();
        size_t baseLen = baseName.Length();

        // Check if name starts with our base name
        if (strncmp(objName, name, baseLen) == 0) {
            // Check for numbered suffix
            if (objName[baseLen] != '\0') {
                const char *numStr = objName + baseLen;
                int currentNum = atoi(numStr);
                if (currentNum > highestNumber) {
                    highestNumber = currentNum;
                }
            } else {
                // Exact match with no number
                highestNumber = XMax(highestNumber, 1);
            }
        }
    }

    sprintf(secureName, "%s%03d", name, highestNumber + 1);
}

void CKContext::GetObjectSecureName(XString &secureName, CKSTRING name, CK_CLASSID cid) {
    XString processedName(name);
    int highestNumber = -1;

    // Check if base name ends with 3 digits
    if (processedName.Length() >= 3) {
        const char *strEnd = processedName.Str() + processedName.Length() - 3;
        if (isdigit(strEnd[0]) && isdigit(strEnd[1]) && isdigit(strEnd[2])) {
            // Extract and remove the numeric suffix
            highestNumber = atoi(strEnd);
            processedName.Crop(0, processedName.Length() - 3);
        }
    }

    // Get all objects of specified class
    int objCount = GetObjectsCountByClassID(cid);
    CK_ID *objList = GetObjectsListByClassID(cid);

    // Find the highest existing number suffix
    for (int i = 0; i < objCount; ++i) {
        CKObject *obj = GetObject(objList[i]);
        if (!obj || !obj->GetName()) continue;

        const char *objName = obj->GetName();
        size_t baseLen = processedName.Length();

        // Check name prefix match
        if (strncmp(objName, processedName.Str(), baseLen) == 0) {
            const char *numPart = objName + baseLen;

            if (*numPart != '\0') {
                int currentNum = atoi(numPart);
                if (currentNum > highestNumber) {
                    highestNumber = currentNum;
                }
            } else if (highestNumber < 0) {
                highestNumber = 0;
            }
        }
    }

    // Build final secure name
    secureName = processedName;
    if (highestNumber != -1) {
        char numBuffer[4];
        sprintf(numBuffer, "%03d", highestNumber + 1);
        secureName << numBuffer;
    }
}

int CKContext::PrepareDestroyObjects(CK_ID *obj_ids, int Count, CKDWORD Flags, CKDependencies *Dependencies) {
    if (Count <= 0)
        return CKERR_INVALIDPARAMETER;

    const int count = m_DependenciesContext.m_MapID.Size();
    if (count != 0) {
        m_DestroyObjectFlag &= Flags;
    } else {
        m_DestroyObjectFlag = Flags;
    }

    m_DependenciesContext.SetOperationMode(CK_DEPENDENCIES_DELETE);

    m_DependenciesContext.StartDependencies(Dependencies);

    for (int i = 0; i < Count; ++i) {
        if (CKObject* obj = GetObject(obj_ids[i])) {
            obj->PrepareDependencies(m_DependenciesContext);
        }
    }

    m_DependenciesContext.StopDependencies();

    return CK_OK;
}

int CKContext::FinishDestroyObjects(CKDWORD Flags) {
    XHashID& depMap = m_DependenciesContext.m_MapID;
    XObjectArray objectsToDelete;
    objectsToDelete.Resize(depMap.Size());
    for (auto it = depMap.Begin(); it != depMap.End(); ++it) {
        objectsToDelete.PushBack(it.GetKey());
    }

    m_DependenciesContext.Clear();

    m_ObjectManager->DeleteObjects(objectsToDelete.Begin(), objectsToDelete.Size(), NULL, Flags);

    return CK_OK;
}

void CKContext::BuildSortedLists() {
    for (int i = 0; i < 32; ++i) {
        m_ManagerList[i].Clear();
    }

    for (int i = 0; i < 32; ++i) {
        for (auto it = m_ManagerTable.Begin(); it != m_ManagerTable.End(); ++it) {
            CKBaseManager *manager = *it;
            if (manager->GetValidFunctionsMask() & (1 << i)) {
                m_ManagerList[i].PushBack(manager);
            }
        }
    }
}

void CKContext::DeferredDestroyObjects(CK_ID *obj_ids, int Count, CKDependencies *Dependencies, CKDWORD Flags) {
    CKDeferredDeletion* deferred = m_ObjectManager->MatchDeletion(Dependencies, Flags);
    if (!deferred) {
        deferred = new CKDeferredDeletion();
        deferred->m_DependenciesPtr = NULL;
        deferred->m_Flags = Flags;
        m_ObjectManager->RegisterDeletion(deferred);
    }

    CKDependencies &dependencies = deferred->m_Dependencies;
    for(int i = 0; i < Count; ++i) {
        dependencies.PushBack(obj_ids[i]);
    }

    if (Dependencies) {
        CKDependencies* dep = new CKDependencies();
        deferred->m_DependenciesPtr = dep;
        deferred->m_Dependencies = *Dependencies;
    }
}

VxMemoryPool *CKContext::GetMemoryPoolGlobalIndex(int count, int &index) {
    int newIndex = m_MemoryPoolMask.GetUnsetBitPosition(0);
    while (m_MemoryPools.Size() < index) {
        m_MemoryPools.PushBack(new VxMemoryPool());
    }

    VxMemoryPool *pool = m_MemoryPools[newIndex];
    if (pool) {
        pool->Allocate(count);
        m_MemoryPoolMask.Set(newIndex);
        index = newIndex;
    } else {
        index = -1;
    }
    return pool;
}

void CKContext::ReleaseMemoryPoolGlobalIndex(int index) {
    if (index >= 0)
        m_MemoryPoolMask.Unset(index);
}

CKContext::CKContext(WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags) : m_DependenciesContext(this) {
    field_498 = 0;
    field_494 = NULL;
    field_4A0 = 0;
    field_49C = NULL;

    m_MainWindow = iWin;
    m_InitManagerOnRegister = FALSE;
    m_RunTime = FALSE;
    m_StartOptions = Flags;
    m_InterfaceMode = 0;
    m_VirtoolsBuild = 0x2010001;
    m_VirtoolsVersion = 0;
    m_UICallBackFct = NULL;
    m_InterfaceModeData = NULL;
    m_DeferDestroyObjects = 0;
    m_Playing = FALSE;
    m_Reseted = TRUE;
    m_InLoad = FALSE;
    m_Saving = FALSE;
    m_InClearAll = FALSE;
    m_Init = FALSE;
    m_InDynamicCreationMode = FALSE;
    m_GlobalImagesSaveOptions = CKTEXTURE_RAWDATA;
    m_GlobalSoundsSaveOptions = CKSOUND_EXTERNAL;
    m_FileWriteMode = CKFILE_UNCOMPRESSED;
    m_GlobalImagesSaveFormat = new CKBitmapProperties;
    memset(m_GlobalImagesSaveFormat, 0, sizeof(CKBitmapProperties));
    m_GlobalImagesSaveFormat->m_Ext = CKFileExtension("bmp");
    m_GeneralLoadMode = CKLOAD_INVALID;
    m_3DObjectsLoadMode = CKLOAD_INVALID;
    m_MeshLoadMode = CKLOAD_INVALID;
    m_MatTexturesLoadMode = CKLOAD_INVALID;
    m_UserLoadCallBack = NULL;
    m_UserLoadCallBackArgs = NULL;

    //    field_3C8 = (DWORD)operator new(0x104u);
    //    field_3CC = (DWORD)operator new(0x104u);
    m_GeneralRenameOption = 0;
    m_MatTexturesRenameOption = 0;

    m_CurrentLevel = 0;
    m_CompressionLevel = 5;
    m_CurrentManager = NULL;

    m_ObjectManager = new CKObjectManager(this);
    m_ParameterManager = new CKParameterManager(this);
    m_AttributeManager = new CKAttributeManager(this);
    m_TimeManager = new CKTimeManager(this);
    m_MessageManager = new CKMessageManager(this);
    m_BehaviorManager = new CKBehaviorManager(this);
    m_PathManager = new CKPathManager(this);
    m_DebugContext = new CKDebugContext(this);

    m_BehaviorContext.AttributeManager = m_AttributeManager;
    m_BehaviorContext.MessageManager = m_MessageManager;
    m_BehaviorContext.ParameterManager = m_ParameterManager;
    m_BehaviorContext.TimeManager = m_TimeManager;
    m_BehaviorContext.Context = this;

    m_ProfilingEnabled = FALSE;
    m_SelectedRenderEngine = iRenderEngine;
    m_RenderManager = NULL;

    m_MemoryPools.Resize(8);
    for (XArray<VxMemoryPool *>::Iterator it = m_MemoryPools.Begin();
         it != m_MemoryPools.End(); ++it) {
        (*it) = new VxMemoryPool;
    }

    memset(&m_Stats, 0, sizeof(CKStats));
    memset(&m_ProfileStats, 0, sizeof(CKStats));
}
