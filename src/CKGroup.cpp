#include "CKGroup.h"

CK_CLASSID CKGroup::m_ClassID = CKCID_GROUP;

CKERROR CKGroup::AddObject(CKBeObject *o) {
    return 0;
}

CKERROR CKGroup::AddObjectFront(CKBeObject *o) {
    return 0;
}

CKERROR CKGroup::InsertObjectAt(CKBeObject *o, int pos) {
    return 0;
}

CKBeObject *CKGroup::RemoveObject(int pos) {
    return nullptr;
}

void CKGroup::RemoveObject(CKBeObject *obj) {
    if (m_ObjectArray.Remove(obj)) {
        m_Groups.Unset(m_GroupIndex);
        m_ClassIdUpdated = FALSE;
    }
}

void CKGroup::Clear() {

}

void CKGroup::MoveObjectUp(CKBeObject *o) {

}

void CKGroup::MoveObjectDown(CKBeObject *o) {

}

CKBeObject *CKGroup::GetObject(int pos) {
return nullptr;
}

int CKGroup::GetObjectCount() {
    return 0;
}

CK_CLASSID CKGroup::GetCommonClassID() {
    return 0;
}

int CKGroup::CanBeHide() {
    return CKObject::CanBeHide();
}

void CKGroup::Show(CK_OBJECT_SHOWOPTION Show) {
    CKObject::Show(Show);
}

CKGroup::CKGroup(CKContext *Context, CKSTRING Name) : CKBeObject(Context, Name) {

}

CKGroup::~CKGroup() {

}

CK_CLASSID CKGroup::GetClassID() {
    return m_ClassID;
}

void CKGroup::AddToScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::AddToScene(scene, dependencies);
}

void CKGroup::RemoveFromScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::RemoveFromScene(scene, dependencies);
}

void CKGroup::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
}

CKStateChunk *CKGroup::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKGroup::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKGroup::PostLoad() {
    CKObject::PostLoad();
}

void CKGroup::PreDelete() {
    CKBeObject::PreDelete();
}

void CKGroup::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
}

int CKGroup::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

int CKGroup::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKGroup::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKERROR CKGroup::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKGroup::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

CKSTRING CKGroup::GetClassName() {
    return nullptr;
}

int CKGroup::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKGroup::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKGroup::Register() {

}

CKGroup *CKGroup::CreateInstance(CKContext *Context) {
    return nullptr;
}

void CKGroup::ComputeClassID() {

}