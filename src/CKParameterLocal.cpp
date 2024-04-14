#include "CKParameterLocal.h"

void CKParameterLocal::SetAsMyselfParameter(CKBOOL act) {

}

void CKParameterLocal::SetOwner(CKObject *o) {
    CKParameter::SetOwner(o);
}

CKERROR CKParameterLocal::SetValue(const void *buf, int size) {
    return CKParameter::SetValue(buf, size);
}

CKERROR CKParameterLocal::CopyValue(CKParameter *param, CKBOOL UpdateParam) {
    return CKParameter::CopyValue(param, UpdateParam);
}

void *CKParameterLocal::GetWriteDataPtr() {
    return CKParameter::GetWriteDataPtr();
}

CKERROR CKParameterLocal::SetStringValue(CKSTRING Value) {
    return CKParameter::SetStringValue(Value);
}

CKParameterLocal::CKParameterLocal(CKContext *Context, CKSTRING name) : CKParameter(Context, name) {

}

CKParameterLocal::~CKParameterLocal() {

}

CK_CLASSID CKParameterLocal::GetClassID() {
    return CKParameter::GetClassID();
}

void CKParameterLocal::PreDelete() {
    CKObject::PreDelete();
}

CKStateChunk *CKParameterLocal::Save(CKFile *file, CKDWORD flags) {
    return CKParameter::Save(file, flags);
}

CKERROR CKParameterLocal::Load(CKStateChunk *chunk, CKFile *file) {
    return CKParameter::Load(chunk, file);
}

int CKParameterLocal::GetMemoryOccupation() {
    return CKParameter::GetMemoryOccupation();
}

CKERROR CKParameterLocal::RemapDependencies(CKDependenciesContext &context) {
    return CKParameter::RemapDependencies(context);
}

CKERROR CKParameterLocal::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKParameter::Copy(o, context);
}

CKSTRING CKParameterLocal::GetClassNameA() {
    return nullptr;
}

int CKParameterLocal::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKParameterLocal::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKParameterLocal::Register() {

}

CKParameterLocal *CKParameterLocal::CreateInstance(CKContext *Context) {
    return nullptr;
}