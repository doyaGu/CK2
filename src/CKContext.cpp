#include "CKContext.h"
#include "CKLevel.h"
#include "CKBaseManager.h"
#include "CKObjectManager.h"
#include "CKParameterManager.h"
#include "CKAttributeManager.h"
#include "CKTimeManager.h"
#include "CKMessageManager.h"
#include "CKBehaviorManager.h"
#include "CKPathManager.h"
#include "CKDebugContext.h"
#include "CKBitmapReader.h"
#include "CKParameterOperation.h"
#include "CKFile.h"

CKObject *CKContext::CreateObject(CK_CLASSID cid, CKSTRING Name, CK_OBJECTCREATION_OPTIONS Options, CK_CREATIONMODE *Res) {
    return nullptr;
}

CKObject *CKContext::CopyObject(CKObject *src, CKDependencies *Dependencies, CKSTRING AppendName, CK_OBJECTCREATION_OPTIONS Options) {
    return nullptr;
}

const XObjectArray &CKContext::CopyObjects(const XObjectArray &SrcObjects, CKDependencies *Dependencies, CK_OBJECTCREATION_OPTIONS Options, CKSTRING AppendName) {
    return;
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
    if (!obj)
        return 0;

    return obj->GetMemoryOccupation();
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
    return;
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
    m_ObjectList.Clear();
    m_ObjectManager->GetObjectListByType(cid, m_ObjectList, derived);
    return m_ObjectList;
}

int CKContext::GetObjectsCountByClassID(CK_CLASSID cid) {
    return m_ObjectManager->GetObjectsCountByClassID(cid);
}

CK_ID *CKContext::GetObjectsListByClassID(CK_CLASSID cid) {
    return m_ObjectManager->GetObjectsListByClassID(cid);
}

CKERROR CKContext::Play() {
    return 0;
}

CKERROR CKContext::Pause() {
    return 0;
}

CKERROR CKContext::Reset() {
    return 0;
}

CKBOOL CKContext::IsPlaying() {
    return m_Playing;
}

CKBOOL CKContext::IsReseted() {
    return m_Reseted;
}

CKERROR CKContext::Process() {
    return 0;
}

CKERROR CKContext::ClearAll() {
    return 0;
}

CKBOOL CKContext::IsInClearAll() {
    return m_InClearAll;
}

CKLevel *CKContext::GetCurrentLevel() {
    return (CKLevel *) GetObject(m_CurrentLevel);
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
        pIn = (CKParameterIn *) CreateObject(CKCID_PARAMETERIN, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
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
        return (CKParameterOut *) CreateObject(CKCID_PARAMETEROUT, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
    }

    CKParameterOut *pOut = (CKParameterOut *) CreateObject(CKCID_PARAMETEROUT, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
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
        pLocal = (CKParameterLocal *) CreateObject(CKCID_PARAMETERLOCAL, Name, Dynamic ? CK_OBJECTCREATION_DYNAMIC : CK_OBJECTCREATION_NONAMECHECK);
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
    CKParameterOperation *pOperation = (CKParameterOperation *) CreateObject(CKCID_PARAMETEROPERATION, Name);
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
    return 0;
}

CKERROR CKContext::OutputToConsoleEx(CKSTRING format, ...) {
    return 0;
}

CKERROR CKContext::OutputToConsoleExBeep(CKSTRING format, ...) {
    return 0;
}

CKERROR CKContext::OutputToInfo(CKSTRING format, ...) {
    return 0;
}

CKERROR CKContext::RefreshBuildingBlocks(const XArray<CKGUID> &iGuids) {
    return 0;
}

CKERROR CKContext::ShowSetup(CK_ID) {
    return 0;
}

CK_ID CKContext::ChooseObject(void *dialogParentWnd) {
    return 0;
}

CKERROR CKContext::Select(const XObjectArray &o, CKBOOL clearSelection) {
    return 0;
}

CKDWORD CKContext::SendInterfaceMessage(CKDWORD reason, CKDWORD param1, CKDWORD param2) {
    return 0;
}

CKERROR CKContext::UICopyObjects(const XObjectArray &iObjects, CKBOOL iClearClipboard) {
    return 0;
}

CKERROR CKContext::UIPasteObjects(const XObjectArray &oObjects) {
    return 0;
}

CKRenderManager *CKContext::GetRenderManager() {
    if (!m_RenderManager)
        m_RenderManager = (CKRenderManager *) GetManagerByGuid(RENDER_MANAGER_GUID);
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
    return nullptr;
}

int CKContext::GetManagerCount() {
    return m_ManagerTable.Size();
}

CKBaseManager *CKContext::GetManager(int index) {
    return nullptr;
}

CKBOOL CKContext::IsManagerActive(CKBaseManager *bm) {
    return bm && bm == GetManagerByGuid(bm->GetGuid());
}

CKBOOL CKContext::HasManagerDuplicates(CKBaseManager *bm) {
    return 0;
}

void CKContext::ActivateManager(CKBaseManager *bm, CKBOOL active) {

}

int CKContext::GetInactiveManagerCount() {
    return m_InactiveManagers.Size();
}

CKBaseManager *CKContext::GetInactiveManager(int index) {
    return m_InactiveManagers[index];
}

CKERROR CKContext::RegisterNewManager(CKBaseManager *manager) {
    return 0;
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

}

float CKContext::UserProfileEnd(CKDWORD UserSlot) {
    return 0;
}

float CKContext::GetLastUserProfileTime(CKDWORD UserSlot) {
    return 0;
}

CKSTRING CKContext::GetStringBuffer(int size) {

}

CKGUID CKContext::GetSecureGuid() {
    return CKGUID();
}

CKDWORD CKContext::GetStartOptions() {
    return 0;
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
    return CKFILE_UNCOMPRESSED;
}

CK_TEXTURE_SAVEOPTIONS CKContext::GetGlobalImagesSaveOptions() {
    return CKTEXTURE_EXTERNAL;
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
    return CKSOUND_EXTERNAL;
}

void CKContext::SetGlobalSoundsSaveOptions(CK_SOUND_SAVEOPTIONS Options) {
    if (Options != CKSOUND_USEGLOBAL)
        m_GlobalSoundsSaveOptions = Options;
}

CKERROR CKContext::Load(CKSTRING FileName, CKObjectArray *liste, CK_LOAD_FLAGS LoadFlags, CKGUID *ReaderGuid) {
    return 0;
}

CKERROR CKContext::Load(int BufferSize, void *MemoryBuffer, CKObjectArray *ckarray, CK_LOAD_FLAGS LoadFlags) {
    return 0;
}

CKSTRING CKContext::GetLastFileLoaded() {
    return (CKSTRING) m_LastFileLoaded.CStr();
}

CKSTRING CKContext::GetLastCmoLoaded() {
    return (CKSTRING) m_LastCmoLoaded.CStr();
}

void CKContext::SetLastCmoLoaded(CKSTRING str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 4 && !strcmp(&str[len - 4], ".cmo"))
        m_LastCmoLoaded = str;
}

CKERROR CKContext::GetFileInfo(CKSTRING FileName, CKFileInfo *FileInfo) {
    return 0;
}

CKERROR CKContext::GetFileInfo(int BufferSize, void *MemoryBuffer, CKFileInfo *FileInfo) {
    return 0;
}

CKERROR CKContext::Save(CKSTRING FileName, CKObjectArray *liste, CKDWORD SaveFlags, CKDependencies *dependencies, CKGUID *ReaderGuid) {
    return 0;
}

CKERROR CKContext::LoadAnimationOnCharacter(CKSTRING FileName, CKObjectArray *liste, CKCharacter *carac, CKGUID *ReaderGuid, CKBOOL AsDynamicObjects) {
    return 0;
}

CKERROR CKContext::LoadAnimationOnCharacter(int BufferSize, void *MemoryBuffer, CKObjectArray *ckarray, CKCharacter *carac, CKBOOL AsDynamicObjects) {
    return 0;
}

void CKContext::SetAutomaticLoadMode(CK_LOADMODE GeneralMode, CK_LOADMODE _3DObjectsMode, CK_LOADMODE MeshMode, CK_LOADMODE MatTexturesMode) {
    m_GeneralLoadMode = GeneralMode;
    m_3DObjectsLoadMode = _3DObjectsMode;
    m_MeshLoadMode = MeshMode;
    m_MatTexturesLoadMode = MatTexturesMode;
}

void CKContext::SetUserLoadCallback(CK_USERLOADCALLBACK fct, void *Arg) {
    m_UserLoadCallBack = fct;
    m_UserLoadCallBackArgs = Arg;
}

CK_LOADMODE CKContext::LoadVerifyObjectUnicity(CKSTRING OldName, CK_CLASSID Cid, const CKSTRING NewName, CKObject **newobj) {
    return CKLOAD_USECURRENT;
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

}

void CKContext::ExecuteManagersOnCKInit() {

}

void CKContext::ExecuteManagersOnCKEnd() {

}

void CKContext::ExecuteManagersPreProcess() {

}

void CKContext::ExecuteManagersPostProcess() {

}

void CKContext::ExecuteManagersPreClearAll() {

}

void CKContext::ExecuteManagersPostClearAll() {

}

void CKContext::ExecuteManagersOnSequenceDeleted(CK_ID *objids, int count) {

}

void CKContext::ExecuteManagersOnSequenceToBeDeleted(CK_ID *objids, int count) {

}

void CKContext::ExecuteManagersOnCKReset() {

}

void CKContext::ExecuteManagersOnCKPostReset() {

}

void CKContext::ExecuteManagersOnCKPause() {

}

void CKContext::ExecuteManagersOnCKPlay() {

}

void CKContext::ExecuteManagersPreLaunchScene(CKScene *OldScene, CKScene *NewScene) {

}

void CKContext::ExecuteManagersPostLaunchScene(CKScene *OldScene, CKScene *NewScene) {

}

void CKContext::ExecuteManagersPreLoad() {

}

void CKContext::ExecuteManagersPostLoad() {

}

void CKContext::ExecuteManagersPreSave() {

}

void CKContext::ExecuteManagersPostSave() {

}

void CKContext::ExecuteManagersOnSequenceAddedToScene(CKScene *scn, CK_ID *objids, int coun) {

}

void CKContext::ExecuteManagersOnSequenceRemovedFromScene(CKScene *scn, CK_ID *objids, int coun) {

}

void CKContext::ExecuteManagersOnPreCopy(CKDependenciesContext *context) {

}

void CKContext::ExecuteManagersOnPostCopy(CKDependenciesContext *context) {

}

void CKContext::ExecuteManagersOnPreRender(CKRenderContext *dev) {

}

void CKContext::ExecuteManagersOnPostRender(CKRenderContext *dev) {

}

void CKContext::ExecuteManagersOnPostSpriteRender(CKRenderContext *dev) {

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
    return 0;
}

CKBOOL CKContext::ProcessDebugStep() {
    return 0;
}

CKERROR CKContext::ProcessDebugEnd() {
    return 0;
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
    return 0;
}

void CKContext::GetSecureName(CKSTRING secureName, const char *name, CK_CLASSID cid) {

}

void CKContext::GetObjectSecureName(XString &secureName, CKSTRING name, CK_CLASSID cid) {

}

int CKContext::PrepareDestroyObjects(CK_ID *obj_ids, int Count, CKDWORD Flags, CKDependencies *Dependencies) {
    return 0;
}

int CKContext::FinishDestroyObjects(CKDWORD Flags) {
    return 0;
}

void CKContext::BuildSortedLists() {

}

void CKContext::DeferredDestroyObjects(CK_ID *obj_ids, int Count, CKDependencies *Dependencies, CKDWORD Flags) {

}

CKContext::CKContext(WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags)
    : m_DependenciesContext(this) {
    field_498 = 0;
    field_494 = NULL;
    field_4A0 = 0;
    field_49C = NULL;

    m_MainWindow = iWin;
    m_InitManagerOnRegister = 0;
    m_RunTime = 0;
    m_StartOptions = Flags;
    m_InterfaceMode = 0;
    m_VirtoolsBuild = 0x2010001;
    m_VirtoolsVersion = 0;
    m_UICallBackFct = NULL;
    m_InterfaceModeData = NULL;
    m_DeferDestroyObjects = 0;
    m_Playing = 0;
    m_Reseted = 1;
    m_InLoad = 0;
    m_Saving = 0;
    m_InClearAll = 0;
    m_Init = 0;
    m_InDynamicCreationMode = 0;
    m_GlobalImagesSaveOptions = CKTEXTURE_RAWDATA;
    m_GlobalSoundsSaveOptions = CKSOUND_EXTERNAL;
    m_FileWriteMode = 0;
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
    field_45C = 0;
    field_460 = 0;

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

    m_ProfilingEnabled = 0;
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