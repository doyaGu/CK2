#include "CKParameter.h"

CK_CLASSID CKParameter::m_ClassID;

CKObject *CKParameter::GetValueObject(CKBOOL update) {
    return nullptr;
}

CKERROR CKParameter::GetValue(void *buf, CKBOOL update) {
    return 0;
}

CKERROR CKParameter::SetValue(const void *buf, int size) {
    return 0;
}

CKERROR CKParameter::CopyValue(CKParameter *param, CKBOOL UpdateParam) {
    return 0;
}

CKBOOL CKParameter::IsCompatibleWith(CKParameter *param) {
    return 0;
}

int CKParameter::GetDataSize() {
    return 0;
}

void *CKParameter::GetReadDataPtr(CKBOOL update) {
    return nullptr;
}

void *CKParameter::GetWriteDataPtr() {
    return nullptr;
}

CKERROR CKParameter::SetStringValue(CKSTRING Value) {
    return 0;
}

int CKParameter::GetStringValue(CKSTRING Value, CKBOOL update) {
    return 0;
}

CKParameterType CKParameter::GetType() {
    return 0;
}

void CKParameter::SetType(CKParameterType type) {

}

CKGUID CKParameter::GetGUID() {
    return CKGUID();
}

void CKParameter::SetGUID(CKGUID guid) {

}

CK_CLASSID CKParameter::GetParameterClassID() {
    return 0;
}

void CKParameter::SetOwner(CKObject *o) {

}

CKObject *CKParameter::GetOwner() {
    return nullptr;
}

void CKParameter::Enable(CKBOOL act) {

}

CKBOOL CKParameter::IsEnabled() {
    return 0;
}

CKParameter::CKParameter(CKContext *Context, CKSTRING name) : CKObject(Context, name) {

}

CKParameter::~CKParameter() {

}

CK_CLASSID CKParameter::GetClassID() {
    return CKObject::GetClassID();
}

void CKParameter::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);
}

CKStateChunk *CKParameter::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKParameter::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKParameter::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKParameter::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

int CKParameter::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(o, cid);
}

CKERROR CKParameter::PrepareDependencies(CKDependenciesContext &context, CKBOOL iCaller) {
    return CKObject::PrepareDependencies(context, iCaller);
}

CKERROR CKParameter::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKParameter::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKParameter::GetClassNameA() {
    return nullptr;
}

int CKParameter::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKParameter::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKParameter::Register() {

}

CKParameter *CKParameter::CreateInstance(CKContext *Context) {
    return nullptr;
}

CKERROR CKParameter::CreateDefaultValue() {
    return 0;
}

void CKParameter::MessageDeleteAfterUse(CKBOOL act) {

}

CKBOOL CKParameter::IsCandidateForFixedSize(CKParameterTypeDesc *iType) {
    return 0;
}
