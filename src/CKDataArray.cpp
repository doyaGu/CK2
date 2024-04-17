#include "CKDataArray.h"

CK_ID CKDataArray::m_ClassID;

void CKDataArray::InsertColumn(int cdest, CK_ARRAYTYPE type, char *name, CKGUID paramguid) {

}

void CKDataArray::MoveColumn(int csrc, int cdest) {

}

void CKDataArray::RemoveColumn(int c) {

}

void CKDataArray::SetColumnName(int c, char *name) {

}

char *CKDataArray::GetColumnName(int c) {
    return nullptr;
}

void CKDataArray::SetColumnType(int c, CK_ARRAYTYPE type, CKGUID paramguid) {

}

CK_ARRAYTYPE CKDataArray::GetColumnType(int c) {
    return CKARRAYTYPE_OBJECT;
}

CKGUID CKDataArray::GetColumnParameterGuid(int c) {
    return CKGUID();
}

int CKDataArray::GetKeyColumn() {
    return 0;
}

void CKDataArray::SetKeyColumn(int c) {

}

int CKDataArray::GetColumnCount() {
    return 0;
}

CKDWORD *CKDataArray::GetElement(int i, int c) {
    return nullptr;
}

CKBOOL CKDataArray::GetElementValue(int i, int c, void *value) {
    return 0;
}

CKObject *CKDataArray::GetElementObject(int i, int c) {
    return nullptr;
}

CKBOOL CKDataArray::SetElementValue(int i, int c, void *value, int size) {
    return 0;
}

CKBOOL CKDataArray::SetElementValueFromParameter(int i, int c, CKParameter *pout) {
    return 0;
}

CKBOOL CKDataArray::SetElementObject(int i, int c, CKObject *object) {
    return 0;
}

CKBOOL CKDataArray::PasteShortcut(int i, int c, CKParameter *pout) {
    return 0;
}

CKParameterOut *CKDataArray::RemoveShortcut(int i, int c) {
    return nullptr;
}

CKBOOL CKDataArray::SetElementStringValue(int i, int c, char *svalue) {
    return 0;
}

int CKDataArray::GetStringValue(CKDWORD key, int c, char *svalue) {
    return 0;
}

int CKDataArray::GetElementStringValue(int i, int c, char *svalue) {
    return 0;
}

CKBOOL CKDataArray::LoadElements(CKSTRING string, CKBOOL append, int column) {
    return 0;
}

CKBOOL CKDataArray::WriteElements(CKSTRING string, int column, int number, CKBOOL iAppend) {
    return 0;
}

int CKDataArray::GetRowCount() {
    return 0;
}

CKDataRow *CKDataArray::GetRow(int n) {
    return nullptr;
}

void CKDataArray::AddRow() {

}

CKDataRow *CKDataArray::InsertRow(int n) {
    return nullptr;
}

CKBOOL CKDataArray::TestRow(int row, int c, CK_COMPOPERATOR op, CKDWORD key, int size) {
    return 0;
}

int CKDataArray::FindRowIndex(int c, CK_COMPOPERATOR op, CKDWORD key, int size, int startingindex) {
    return 0;
}

CKDataRow *CKDataArray::FindRow(int c, CK_COMPOPERATOR op, CKDWORD key, int size, int startindex) {
    return nullptr;
}

void CKDataArray::RemoveRow(int n) {

}

void CKDataArray::MoveRow(int rsrc, int rdst) {

}

void CKDataArray::SwapRows(int i1, int i2) {

}

void CKDataArray::Clear(CKBOOL Params) {

}

CKBOOL CKDataArray::GetHighest(int c, int &row) {
    return 0;
}

CKBOOL CKDataArray::GetLowest(int c, int &row) {
    return 0;
}

CKBOOL CKDataArray::GetNearest(int c, void *value, int &row) {
    return 0;
}

void CKDataArray::ColumnTransform(int c, CK_BINARYOPERATOR op, CKDWORD value) {

}

void CKDataArray::ColumnsOperate(int c1, CK_BINARYOPERATOR op, int c2, int cr) {

}

void CKDataArray::Sort(int c, CKBOOL ascending) {

}

void CKDataArray::Unique(int c) {

}

void CKDataArray::RandomShuffle() {

}

void CKDataArray::Reverse() {

}

CKDWORD CKDataArray::Sum(int c) {
    return 0;
}

CKDWORD CKDataArray::Product(int c) {
    return 0;
}

int CKDataArray::GetCount(int c, CK_COMPOPERATOR op, CKDWORD key, int size) {
    return 0;
}

void CKDataArray::CreateGroup(int mc, CK_COMPOPERATOR op, CKDWORD key, int size, CKGroup *group, int ec) {

}

CKDataArray::CKDataArray(CKContext *Context, CKSTRING Name) : CKBeObject(Context, Name) {

}

CKDataArray::~CKDataArray() {

}

CK_CLASSID CKDataArray::GetClassID() {
    return CKBeObject::GetClassID();
}

void CKDataArray::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
}

CKStateChunk *CKDataArray::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKDataArray::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKDataArray::PostLoad() {
    CKObject::PostLoad();
}

void CKDataArray::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
}

void CKDataArray::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKDataArray::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

int CKDataArray::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKDataArray::PrepareDependencies(CKDependenciesContext &context, CKBOOL iCaller) {
    return CKBeObject::PrepareDependencies(context, iCaller);
}

CKERROR CKDataArray::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKDataArray::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

CKSTRING CKDataArray::GetClassNameA() {
    return nullptr;
}

int CKDataArray::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKDataArray::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKDataArray::Register() {

}

CKDataArray *CKDataArray::CreateInstance(CKContext *Context) {
    return nullptr;
}