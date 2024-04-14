#include "CKParameterOperation.h"

CKERROR CKParameterOperation::DoOperation() {
    return 0;
}

CKGUID CKParameterOperation::GetOperationGuid() {
    return CKGUID();
}

void CKParameterOperation::Reconstruct(CKSTRING Name, CKGUID opguid, CKGUID ResGuid, CKGUID p1Guid, CKGUID p2Guid) {

}

CK_PARAMETEROPERATION CKParameterOperation::GetOperationFunction() {
    return nullptr;
}

CKParameterOperation::CKParameterOperation(CKContext *Context, CKSTRING name) : CKObject(Context, name) {

}

CKParameterOperation::CKParameterOperation(CKContext *Context, CKSTRING name, CKGUID OpGuid, CKGUID ResGuid,
                                           CKGUID P1Guid, CKGUID P2Guid) {

}

CKParameterOperation::~CKParameterOperation() {

}

CK_CLASSID CKParameterOperation::GetClassID() {
    return CKObject::GetClassID();
}

void CKParameterOperation::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);
}

CKStateChunk *CKParameterOperation::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKParameterOperation::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKParameterOperation::PostLoad() {
    CKObject::PostLoad();
}

void CKParameterOperation::PreDelete() {
    CKObject::PreDelete();
}

int CKParameterOperation::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

int CKParameterOperation::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(o, cid);
}

CKERROR CKParameterOperation::PrepareDependencies(CKDependenciesContext &context, CKBOOL iCaller) {
    return CKObject::PrepareDependencies(context, iCaller);
}

CKERROR CKParameterOperation::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKParameterOperation::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKParameterOperation::GetClassNameA() {
    return nullptr;
}

int CKParameterOperation::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKParameterOperation::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKParameterOperation::Register() {

}

CKParameterOperation *CKParameterOperation::CreateInstance(CKContext *Context) {
    return nullptr;
}