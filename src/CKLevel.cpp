#include "CKLevel.h"

#include "CKFile.h"
#include "CKScene.h"
#include "CKPlace.h"
#include "CKObjectArray.h"

CK_CLASSID CKLevel::m_ClassID = CKCID_LEVEL;

CKERROR CKLevel::AddObject(CKObject *obj) {
    if (!CKIsChildClassOf(obj, CKCID_SCENEOBJECT) || CKIsChildClassOf(obj, CKCID_LEVEL))
        return CKERR_INVALIDPARAMETER;
    GetLevelScene()->AddObjectToScene((CKSceneObject *) obj);
    return CK_OK;
}

CKERROR CKLevel::RemoveObject(CKObject *obj) {
    if (!CKIsChildClassOf(obj, CKCID_SCENEOBJECT))
        return CKERR_INVALIDPARAMETER;
	GetLevelScene()->RemoveObjectFromScene((CKSceneObject*)obj);
    return CK_OK;
}

CKERROR CKLevel::RemoveObject(CK_ID objID) {
    return RemoveObject(m_Context->GetObject(objID));
}

void CKLevel::BeginAddSequence(CKBOOL Begin) {
    GetLevelScene()->BeginAddSequence(Begin);
}

void CKLevel::BeginRemoveSequence(CKBOOL Begin) {
    GetLevelScene()->BeginRemoveSequence(Begin);
}

const XObjectPointerArray &CKLevel::ComputeObjectList(CK_CLASSID cid, CKBOOL derived) {
    if (CKIsChildClassOf(cid, CKCID_SCENE))
        return m_SceneList;
    return GetLevelScene()->ComputeObjectList(cid, derived);
}

CKERROR CKLevel::AddPlace(CKPlace *pl) {
    if (!pl)
        return CKERR_INVALIDPARAMETER;
    pl->AddToScene(GetLevelScene());
    return CK_OK;
}

CKERROR CKLevel::RemovePlace(CKPlace *pl) {
    if (!pl)
        return CKERR_INVALIDPARAMETER;
    pl->RemoveFromScene(GetLevelScene());
    return CK_OK;
}

CKPlace *CKLevel::RemovePlace(int pos) {
    CKPlace *pl = GetPlace(pos);
    pl->RemoveFromScene(GetLevelScene());
    return pl;
}

CKPlace *CKLevel::GetPlace(int pos) {
    CKObjectArray *array = CreateCKObjectArray();
    GetLevelScene()->ComputeObjectList(array, CKCID_PLACE);
    CK_ID id = array->PositionFind(pos);
    CKPlace *pl = (CKPlace *)m_Context->GetObject(id);
    DeleteCKObjectArray(array);
    return pl;
}

int CKLevel::GetPlaceCount() {
    CKObjectArray *array = CreateCKObjectArray();
    GetLevelScene()->ComputeObjectList(array, CKCID_PLACE);
    int count = array->GetCount();
    DeleteCKObjectArray(array);
    return count;
}

CKERROR CKLevel::AddScene(CKScene *scn) {
    return 0;
}

CKERROR CKLevel::RemoveScene(CKScene *scn) {
    return 0;
}

CKScene *CKLevel::RemoveScene(int pos) {
    return nullptr;
}

CKScene *CKLevel::GetScene(int pos) {
    return nullptr;
}

int CKLevel::GetSceneCount() {
    return 0;
}

CKERROR CKLevel::SetNextActiveScene(CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS Active, CK_SCENEOBJECTRESET_FLAGS Reset) {
    m_NextActiveScene = scene ? scene->GetID() : 0;
    m_NextSceneActivityFlags = Active;
    m_NextSceneResetFlags = Reset;
    return CK_OK;
}

CKERROR CKLevel::LaunchScene(CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS Active, CK_SCENEOBJECTRESET_FLAGS Reset) {
    return 0;
}

CKScene *CKLevel::GetCurrentScene() {
    return (CKScene *) m_Context->GetObject(m_CurrentScene);
}

void CKLevel::AddRenderContext(CKRenderContext *dev, CKBOOL Main) {

}

void CKLevel::RemoveRenderContext(CKRenderContext *dev) {

}

int CKLevel::GetRenderContextCount() {
    return m_RenderContextList.Size();
}

CKRenderContext *CKLevel::GetRenderContext(int count) {
    if (count >= m_RenderContextList.Size())
        return nullptr;
    return (CKRenderContext *) m_RenderContextList[count];
}

CKScene *CKLevel::GetLevelScene() {
    return (CKScene *) m_Context->GetObject(m_DefaultScene);
}

CKERROR CKLevel::Merge(CKLevel *mergedLevel, CKBOOL asScene) {
    return 0;
}

void CKLevel::ApplyPatchForOlderVersion(int NbObject, CKFileObject *FileObjects) {
    CKBeObject::ApplyPatchForOlderVersion(NbObject, FileObjects);
}

CKLevel::CKLevel(CKContext *Context, CKSTRING name) : CKBeObject(Context, name) {
    m_CurrentScene = 0;
    m_DefaultScene = 0;
    m_Priority = 32000;
    m_NextActiveScene = 0;
    m_IsReseted = 0;
    CreateLevelScene();
}

CKLevel::~CKLevel() {
    if (m_ID == m_Context->m_CurrentLevel)
        m_Context->m_CurrentLevel = 0;
}

CK_CLASSID CKLevel::GetClassID() {
    return m_ClassID;
}

void CKLevel::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
    file->SaveObjects(m_SceneList.Begin(), m_SceneList.Size());
    GetLevelScene()->PreSave(file, flags);
}

CKStateChunk *CKLevel::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKLevel::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKLevel::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
    m_SceneList.Check();
    m_RenderContextList.Check();
}

void CKLevel::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
    if (!m_Context->GetObject(m_DefaultScene))
        m_DefaultScene = 0;
    if (!m_Context->GetObject(m_CurrentScene))
        m_CurrentScene = 0;
    if (!m_Context->GetObject(m_NextActiveScene))
        m_NextActiveScene = 0;
}

int CKLevel::GetMemoryOccupation() {
    int size = CKBeObject::GetMemoryOccupation() + 48 + 4;
    size += m_RenderContextList.GetMemoryOccupation();
    size += m_SceneList.GetMemoryOccupation();
    return size;
}

int CKLevel::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    if (o->GetClassID() == CKCID_RENDERCONTEXT) {
        for (int i = 0; i < m_RenderContextList.Size(); ++i) {
            if (m_RenderContextList[i] == o)
                return TRUE;
        }
    } else if (o->GetClassID() == CKCID_SCENE) {
        for (int i = 0; i < m_SceneList.Size(); ++i) {
            if (m_SceneList[i] == o)
                return TRUE;
        }
    }
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKLevel::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKSTRING CKLevel::GetClassName() {
    return "Level";
}

int CKLevel::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKLevel::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKLevel::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKScene::m_ClassID);
    CKClassNeedNotificationFrom(m_ClassID, CKCID_PLACE);
    CKClassNeedNotificationFrom(m_ClassID, CKCID_RENDERCONTEXT);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_LEVEL);
    CKClassRegisterDefaultOptions(m_ClassID, 1);
}

CKLevel *CKLevel::CreateInstance(CKContext *Context) {
    return new CKLevel(Context, nullptr);
}

void CKLevel::AddToScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::AddToScene(scene, dependencies);
}

void CKLevel::RemoveFromScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::RemoveFromScene(scene, dependencies);
}

void CKLevel::Reset() {
    m_IsReseted = TRUE;
}

void CKLevel::ActivateAllScript() {

}

void CKLevel::CheckForNextScene() {

}

void CKLevel::CreateLevelScene() {

}

void CKLevel::PreProcess() {

}