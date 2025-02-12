#include "CKScene.h"

CK_CLASSID CKScene::m_ClassID = CKCID_SCENE;

void CKScene::AddObjectToScene(CKSceneObject *o, CKBOOL dependencies) {

}

void CKScene::RemoveObjectFromScene(CKSceneObject *o, CKBOOL dependencies) {

}

CKBOOL CKScene::IsObjectHere(CKObject *o) {
    return 0;
}

void CKScene::BeginAddSequence(CKBOOL Begin) {

}

void CKScene::BeginRemoveSequence(CKBOOL Begin) {

}

int CKScene::GetObjectCount() {
    return 0;
}

const XObjectPointerArray &CKScene::ComputeObjectList(CK_CLASSID cid, CKBOOL derived) {
    static XObjectPointerArray array;
    return array;
}

CKSceneObjectIterator CKScene::GetObjectIterator() {
    return CKSceneObjectIterator(CKSODHashIt());
}

CKSceneObjectDesc *CKScene::GetSceneObjectDesc(CKSceneObject *o) {
    return nullptr;
}

void CKScene::Activate(CKSceneObject *o, CKBOOL Reset) {

}

void CKScene::DeActivate(CKSceneObject *o) {

}

void CKScene::SetObjectFlags(CKSceneObject *o, CK_SCENEOBJECT_FLAGS flags) {

}

CK_SCENEOBJECT_FLAGS CKScene::GetObjectFlags(CKSceneObject *o) {
    return CK_SCENEOBJECT_INTERNAL_IC;
}

CK_SCENEOBJECT_FLAGS CKScene::ModifyObjectFlags(CKSceneObject *o, CKDWORD Add, CKDWORD Remove) {
    return CK_SCENEOBJECT_INTERNAL_IC;
}

CKBOOL CKScene::SetObjectInitialValue(CKSceneObject *o, CKStateChunk *chunk) {
    return 0;
}

CKStateChunk *CKScene::GetObjectInitialValue(CKSceneObject *o) {
    return nullptr;
}

CKBOOL CKScene::IsObjectActive(CKSceneObject *o) {
    return 0;
}

void CKScene::ApplyEnvironmentSettings(XObjectPointerArray *renderlist) {

}

void CKScene::UseEnvironmentSettings(CKBOOL use) {

}

CKBOOL CKScene::EnvironmentSettings() {
    return 0;
}

void CKScene::SetAmbientLight(CKDWORD Color) {

}

CKDWORD CKScene::GetAmbientLight() {
    return 0;
}

void CKScene::SetFogMode(VXFOG_MODE Mode) {

}

void CKScene::SetFogStart(float Start) {

}

void CKScene::SetFogEnd(float End) {

}

void CKScene::SetFogDensity(float Density) {

}

void CKScene::SetFogColor(CKDWORD Color) {

}

VXFOG_MODE CKScene::GetFogMode() {
    return VXFOG_NONE;
}

float CKScene::GetFogStart() {
    return 0;
}

float CKScene::GetFogEnd() {
    return 0;
}

float CKScene::GetFogDensity() {
    return 0;
}

CKDWORD CKScene::GetFogColor() {
    return 0;
}

void CKScene::SetBackgroundColor(CKDWORD Color) {

}

CKDWORD CKScene::GetBackgroundColor() {
    return 0;
}

void CKScene::SetBackgroundTexture(CKTexture *texture) {

}

CKTexture *CKScene::GetBackgroundTexture() {
    return nullptr;
}

void CKScene::SetStartingCamera(CKCamera *camera) {

}

CKCamera *CKScene::GetStartingCamera() {
    return nullptr;
}

CKLevel *CKScene::GetLevel() {
    return nullptr;
}

CKERROR CKScene::Merge(CKScene *mergedScene, CKLevel *fromLevel) {
    return 0;
}

CKScene::CKScene(CKContext *Context, CKSTRING name) : CKBeObject(Context, name) {

}

CKScene::~CKScene() {

}

CK_CLASSID CKScene::GetClassID() {
    return m_ClassID;
}

void CKScene::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
}

CKStateChunk *CKScene::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKScene::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKScene::PreDelete() {
    CKBeObject::PreDelete();
}

void CKScene::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKScene::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

int CKScene::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKScene::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKERROR CKScene::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKScene::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

CKSTRING CKScene::GetClassName() {
    return nullptr;
}

int CKScene::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKScene::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKScene::Register() {

}

CKScene *CKScene::CreateInstance(CKContext *Context) {
    return nullptr;
}

CKERROR CKScene::ComputeObjectList(CKObjectArray *array, CK_CLASSID cid, CKBOOL derived) {
    return 0;
}

void CKScene::AddObject(CKSceneObject *o) {

}

void CKScene::RemoveObject(CKSceneObject *o) {

}
