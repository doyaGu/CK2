#include "CKParameterIn.h"


CKERROR CKParameterIn::SetDirectSource(CKParameter *param) {
    return 0;
}

CKERROR CKParameterIn::ShareSourceWith(CKParameterIn *param) {
    return 0;
}

void CKParameterIn::SetType(CKParameterType type, CKBOOL UpdateSource, CKSTRING NewName) {

}

void CKParameterIn::SetGUID(CKGUID guid, CKBOOL UpdateSource, CKSTRING NewName) {

}

CKParameterIn::CKParameterIn(CKContext *Context, CKSTRING name, int type) {

}

CKParameterIn::~CKParameterIn() {

}

CK_CLASSID CKParameterIn::GetClassID() {
    return CKObject::GetClassID();
}

void CKParameterIn::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);
}

CKStateChunk *CKParameterIn::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKParameterIn::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKParameterIn::PreDelete() {
    CKObject::PreDelete();
}

void CKParameterIn::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
}

int CKParameterIn::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

int CKParameterIn::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(o, cid);
}

CKERROR CKParameterIn::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKParameterIn::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKParameterIn::GetClassNameA() {
    return nullptr;
}

int CKParameterIn::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKParameterIn::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKParameterIn::Register() {

}

CKParameterIn *CKParameterIn::CreateInstance(CKContext *Context) {
    return nullptr;
}
