#include "CKParameterOut.h"
#include "CKParameterOut.h"

CK_CLASSID CKParameterOut::m_ClassID = CKCID_PARAMETEROUT;

CKERROR CKParameterOut::GetValue(void *buf, CKBOOL update) {
    return CKParameter::GetValue(buf, update);
}

CKERROR CKParameterOut::SetValue(const void *buf, int size) {
    return CKParameter::SetValue(buf, size);
}

CKERROR CKParameterOut::CopyValue(CKParameter *param, CKBOOL UpdateParam) {
    return CKParameter::CopyValue(param, UpdateParam);
}

void *CKParameterOut::GetReadDataPtr(CKBOOL update) {
    return CKParameter::GetReadDataPtr(update);
}

int CKParameterOut::GetStringValue(CKSTRING Value, CKBOOL update) {
    return CKParameter::GetStringValue(Value, update);
}

void CKParameterOut::DataChanged() {

}

CKERROR CKParameterOut::AddDestination(CKParameter *param, CKBOOL CheckType) {
    return 0;
}

void CKParameterOut::RemoveDestination(CKParameter *param) {

}

int CKParameterOut::GetDestinationCount() {
    return 0;
}

CKParameter *CKParameterOut::GetDestination(int pos) {
    return nullptr;
}

void CKParameterOut::RemoveAllDestinations() {

}

CKParameterOut::CKParameterOut(CKContext *Context, CKSTRING name): CKParameter(Context, name) {

}

CKParameterOut::~CKParameterOut() {

}

CK_CLASSID CKParameterOut::GetClassID() {
    return CKParameter::GetClassID();
}

void CKParameterOut::PreSave(CKFile *file, CKDWORD flags) {
    CKParameter::PreSave(file, flags);
}

CKStateChunk *CKParameterOut::Save(CKFile *file, CKDWORD flags) {
    return CKParameter::Save(file, flags);
}

CKERROR CKParameterOut::Load(CKStateChunk *chunk, CKFile *file) {
    return CKParameter::Load(chunk, file);
}

void CKParameterOut::PreDelete() {
    CKObject::PreDelete();
}

void CKParameterOut::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
}

int CKParameterOut::GetMemoryOccupation() {
    return CKParameter::GetMemoryOccupation();
}

int CKParameterOut::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKParameter::IsObjectUsed(o, cid);
}

CKERROR CKParameterOut::RemapDependencies(CKDependenciesContext &context) {
    return CKParameter::RemapDependencies(context);
}

CKERROR CKParameterOut::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKParameter::Copy(o, context);
}

CKSTRING CKParameterOut::GetClassNameA() {
    return nullptr;
}

int CKParameterOut::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKParameterOut::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKParameterOut::Register() {

}

CKParameterOut *CKParameterOut::CreateInstance(CKContext *Context) {
    return nullptr;
}

void CKParameterOut::Update() {

}
