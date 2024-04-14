#include "CKBehaviorIO.h"

CK_CLASSID CKBehaviorIO::m_ClassID = CKCID_BEHAVIORIO;

CKBehaviorIO::CKBehaviorIO(CKContext *Context, CKSTRING name) : CKObject(Context, name) {
    m_OwnerBehavior = NULL;
}

CKBehaviorIO::~CKBehaviorIO() {
    m_Links.Clear();
}

CK_CLASSID CKBehaviorIO::GetClassID() {
    return CKBehaviorIO::m_ClassID;
}

CKStateChunk *CKBehaviorIO::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKBehaviorIO::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKBehaviorIO::PreDelete() {
    CKObject::PreDelete();
}

int CKBehaviorIO::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

CKERROR CKBehaviorIO::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKBehaviorIO::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKBehaviorIO::GetClassNameA() {
    return nullptr;
}

int CKBehaviorIO::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKBehaviorIO::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKBehaviorIO::Register() {

}

CKBehaviorIO *CKBehaviorIO::CreateInstance(CKContext *Context) {
    return nullptr;
}

void CKBehaviorIO::SortLinks() {

}
