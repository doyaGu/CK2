#include "CKBehaviorLink.h"

CK_CLASSID CKBehaviorLink::m_ClassID = CKCID_BEHAVIORLINK;

CKERROR CKBehaviorLink::SetOutBehaviorIO(CKBehaviorIO *ckbioin) {
    return 0;
}

CKERROR CKBehaviorLink::SetInBehaviorIO(CKBehaviorIO *ckbioout) {
    return 0;
}

CKBehaviorLink::CKBehaviorLink(CKContext *Context, CKSTRING name) : CKObject(Context, name) {

}

CKBehaviorLink::~CKBehaviorLink() {

}

CK_CLASSID CKBehaviorLink::GetClassID() {
    return CKObject::GetClassID();
}

CKStateChunk *CKBehaviorLink::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKBehaviorLink::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKBehaviorLink::PostLoad() {
    CKObject::PostLoad();
}

void CKBehaviorLink::PreDelete() {
    CKObject::PreDelete();
}

int CKBehaviorLink::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

CKBOOL CKBehaviorLink::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(obj, cid);
}

CKERROR CKBehaviorLink::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKBehaviorLink::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKBehaviorLink::GetClassNameA() {
    return nullptr;
}

int CKBehaviorLink::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKBehaviorLink::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKBehaviorLink::Register() {

}

CKBehaviorLink *CKBehaviorLink::CreateInstance(CKContext *Context) {
    return nullptr;
}