#include "CKDataArray.h"

CK_CLASSID CKDataArray::m_ClassID = CKCID_DATAARRAY;

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
    if (c < 0 || c >= m_FormatArray.Size())
        return CK_ARRAYTYPE(0);
    return m_FormatArray[c]->m_Type;
}

CKGUID CKDataArray::GetColumnParameterGuid(int c) {
    return CKGUID();
}

int CKDataArray::GetKeyColumn() {
    return m_KeyColumn;
}

void CKDataArray::SetKeyColumn(int c) {
    if (c < 0 || c >= m_FormatArray.Size())
        return;
    m_KeyColumn = c;
}

int CKDataArray::GetColumnCount() {
    return m_FormatArray.Size();
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
    return m_DataMatrix.Size();
}

CKDataRow *CKDataArray::GetRow(int n) {
    if (n < 0 || n >= m_DataMatrix.Size())
        return nullptr;
    return m_DataMatrix[n];
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

void CKDataArray::DataDelete(CKBOOL Params) {
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
    m_KeyColumn = -1;
    m_Order = FALSE;
    m_ColumnIndex = 0;
}

CKDataArray::~CKDataArray() {
    DataDelete();
}

CK_CLASSID CKDataArray::GetClassID() {
    return m_ClassID;
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

CKERROR CKDataArray::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKERROR CKDataArray::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKDataArray::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

CKSTRING CKDataArray::GetClassName() {
    return "Array";
}

int CKDataArray::GetDependenciesCount(int mode) {
    if (mode == 1) {
        return 2;
    }
    return mode == 2 ? 1 : 0;
}

CKSTRING CKDataArray::GetDependencies(int i, int mode) {
    if (i == 0) {
        return "Objects";
    }
    if (mode == 1 && i == 1) {
        return "Data";
    }
    return nullptr;
}

void CKDataArray::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKObject::m_ClassID);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_DATAARRAY);
    CKClassRegisterDefaultDependencies(m_ClassID, 2, 1);
}

CKDataArray *CKDataArray::CreateInstance(CKContext *Context) {
    return new CKDataArray(Context);
}