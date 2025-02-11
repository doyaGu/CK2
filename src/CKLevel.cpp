#include "CKLevel.h"

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

CKERROR CKLevel::SetNextActiveScene(CKScene *, CK_SCENEOBJECTACTIVITY_FLAGS Active, CK_SCENEOBJECTRESET_FLAGS Reset) {
    return 0;
}

CKERROR CKLevel::LaunchScene(CKScene *, CK_SCENEOBJECTACTIVITY_FLAGS Active, CK_SCENEOBJECTRESET_FLAGS Reset) {
    return 0;
}

CKScene *CKLevel::GetCurrentScene() {
    return (CKScene *) m_Context->GetObject(m_CurrentScene);
}

void CKLevel::AddRenderContext(CKRenderContext *, CKBOOL Main) {

}

void CKLevel::RemoveRenderContext(CKRenderContext *) {

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

}

CKLevel::~CKLevel() {

}

CK_CLASSID CKLevel::GetClassID() {
    return m_ClassID;
}

void CKLevel::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
}

CKStateChunk *CKLevel::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKLevel::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKLevel::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
}

void CKLevel::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKLevel::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

int CKLevel::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKLevel::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKSTRING CKLevel::GetClassNameA() {
    return nullptr;
}

int CKLevel::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKLevel::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKLevel::Register() {

}

CKLevel *CKLevel::CreateInstance(CKContext *Context) {
    return nullptr;
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