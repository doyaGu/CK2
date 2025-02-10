#include "CKParameterManager.h"
#include "CKGlobals.h"

CKERROR CKParameterManager::RegisterParameterType(CKParameterTypeDesc *param_type) {
    return 0;
}

CKERROR CKParameterManager::UnRegisterParameterType(CKGUID guid) {
    return 0;
}

CKParameterTypeDesc *CKParameterManager::GetParameterTypeDescription(int type) {
    if (type < 0 || type > m_RegistredTypes.Size())
        return nullptr;
    return m_RegistredTypes[type];
}

CKParameterTypeDesc *CKParameterManager::GetParameterTypeDescription(CKGUID guid) {
    auto type = ParameterGuidToType(guid);
    if (type == -1)
        return nullptr;
    return m_RegistredTypes[type];
}

int CKParameterManager::GetParameterSize(CKParameterType type) {
    return 0;
}

int CKParameterManager::GetParameterTypesCount() {
    return 0;
}

CKParameterType CKParameterManager::ParameterGuidToType(CKGUID guid) {
    auto* pType = m_ParamGuids.FindPtr(guid);
    if (pType)
        return *pType;
    return -1;
}

CKSTRING CKParameterManager::ParameterGuidToName(CKGUID guid) {
    return nullptr;
}

CKGUID CKParameterManager::ParameterTypeToGuid(CKParameterType type) {
    return CKGUID();
}

CKSTRING CKParameterManager::ParameterTypeToName(CKParameterType type) {
    return nullptr;
}

CKGUID CKParameterManager::ParameterNameToGuid(CKSTRING name) {
    return CKGUID();
}

CKParameterType CKParameterManager::ParameterNameToType(CKSTRING name) {
    return 0;
}

CKBOOL CKParameterManager::IsDerivedFrom(CKGUID guid1, CKGUID parent) {
    return 0;
}

CKBOOL CKParameterManager::IsDerivedFrom(CKParameterType child, CKParameterType parent) {
    return 0;
}

CKBOOL CKParameterManager::IsTypeCompatible(CKGUID guid1, CKGUID guid2) {
    return 0;
}

CKBOOL CKParameterManager::IsTypeCompatible(CKParameterType Ptype1, CKParameterType Ptype2) {
    return 0;
}

CK_CLASSID CKParameterManager::TypeToClassID(CKParameterType type) {
    return 0;
}

CK_CLASSID CKParameterManager::GuidToClassID(CKGUID guid) {
    return 0;
}

CKParameterType CKParameterManager::ClassIDToType(CK_CLASSID cid) {
    return 0;
}

CKGUID CKParameterManager::ClassIDToGuid(CK_CLASSID cid) {
    return CKGUID();
}

CKERROR CKParameterManager::RegisterNewFlags(CKGUID FlagsGuid, CKSTRING FlagsName, CKSTRING FlagsData) {
    return 0;
}

CKERROR CKParameterManager::RegisterNewEnum(CKGUID EnumGuid, CKSTRING EnumName, CKSTRING EnumData) {
    if (!EnumName || !EnumData)
        return CKERR_INVALIDPARAMETER;
    if (ParameterGuidToType(EnumGuid) >= 0)
        return CKERR_INVALIDGUID;
    CKParameterTypeDesc desc;
    desc.Guid = EnumGuid;
    desc.TypeName = EnumName;
    desc.DerivedFrom = CKPGUID_INT;
    desc.dwParam = m_NbEnumsDefined;
    desc.DefaultSize = sizeof(int);
    desc.StringFunction = nullptr;//(CK_PARAMETERSTRINGFUNCTION)CKEnumStringFunc;
    desc.Valid = 1;
    desc.CreateDefaultFunction = nullptr;
    desc.DeleteFunction = nullptr;
    desc.SaveLoadFunction = nullptr;
    desc.Cid = 0;
    desc.CopyFunction = nullptr;
    desc.dwFlags = CKPARAMETERTYPE_ENUMS;
    desc.UICreatorFunction = nullptr;
    RegisterParameterType(&desc);
    CKEnumStruct* enums = new CKEnumStruct[m_NbEnumsDefined + 1];
    if (m_Enums) {
        memcpy(enums, m_Enums, sizeof(CKEnumStruct) * m_NbEnumsDefined);
        delete[] m_Enums;
    }
    m_Enums = enums;
    // TODO
    return CK_OK;
}

CKERROR CKParameterManager::ChangeEnumDeclaration(CKGUID EnumGuid, CKSTRING EnumData) {
    return 0;
}

CKERROR CKParameterManager::ChangeFlagsDeclaration(CKGUID FlagsGuid, CKSTRING FlagsData) {
    return 0;
}

CKERROR CKParameterManager::RegisterNewStructure(CKGUID StructGuid, CKSTRING StructName, CKSTRING Structdata, ...) {
    return 0;
}

CKERROR CKParameterManager::RegisterNewStructure(CKGUID StructGuid, CKSTRING StructName, CKSTRING StructData,
                                                 XArray<CKGUID> &ListGuid) {
    return 0;
}

int CKParameterManager::GetNbFlagDefined() {
    return 0;
}

int CKParameterManager::GetNbEnumDefined() {
    return 0;
}

int CKParameterManager::GetNbStructDefined() {
    return 0;
}

CKFlagsStruct *CKParameterManager::GetFlagsDescByType(CKParameterType pType) {
    return nullptr;
}

CKEnumStruct *CKParameterManager::GetEnumDescByType(CKParameterType pType) {
    return nullptr;
}

CKStructStruct *CKParameterManager::GetStructDescByType(CKParameterType pType) {
    return nullptr;
}

CKOperationType CKParameterManager::RegisterOperationType(CKGUID OpCode, CKSTRING name) {
    return 0;
}

CKERROR CKParameterManager::UnRegisterOperationType(CKGUID opguid) {
    return 0;
}

CKERROR CKParameterManager::UnRegisterOperationType(CKOperationType opcode) {
    return 0;
}

CKERROR CKParameterManager::RegisterOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                                      CKGUID &type_param2, CK_PARAMETEROPERATION op) {
    return 0;
}

CK_PARAMETEROPERATION
CKParameterManager::GetOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                         CKGUID &type_param2) {
    return nullptr;
}

CKERROR CKParameterManager::UnRegisterOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                                        CKGUID &type_param2) {
    return 0;
}

CKGUID CKParameterManager::OperationCodeToGuid(CKOperationType type) {
    return CKGUID();
}

CKSTRING CKParameterManager::OperationCodeToName(CKOperationType type) {
    return nullptr;
}

CKOperationType CKParameterManager::OperationGuidToCode(CKGUID guid) {
    return 0;
}

CKSTRING CKParameterManager::OperationGuidToName(CKGUID guid) {
    return nullptr;
}

CKGUID CKParameterManager::OperationNameToGuid(CKSTRING name) {
    return CKGUID();
}

CKOperationType CKParameterManager::OperationNameToCode(CKSTRING name) {
    return 0;
}

int CKParameterManager::GetAvailableOperationsDesc(const CKGUID &opGuid, CKParameterOut *res, CKParameterIn *p1,
                                                   CKParameterIn *p2, CKOperationDesc *list) {
    return 0;
}

int CKParameterManager::GetParameterOperationCount() {
    return 0;
}

CKBOOL CKParameterManager::IsParameterTypeToBeShown(CKParameterType type) {
    return 0;
}

CKBOOL CKParameterManager::IsParameterTypeToBeShown(CKGUID guid) {
    return 0;
}

CKParameterManager::CKParameterManager(CKContext *Context) : CKBaseManager(Context, PARAMETER_MANAGER_GUID, "Parameter Manager") {

}

CKParameterManager::~CKParameterManager() {

}

CKBOOL CKParameterManager::CheckParamTypeValidity(CKParameterType type) {
    return 0;
}

CKBOOL CKParameterManager::CheckOpCodeValidity(CKOperationType type) {
    return 0;
}

CKBOOL CKParameterManager::GetParameterGuidParentGuid(CKGUID child, CKGUID &parent) {
    return 0;
}

void CKParameterManager::UpdateDerivationTables() {

}

void CKParameterManager::RecurseDeleteParam(TreeCell *cell, CKGUID param) {

}

int CKParameterManager::DichotomicSearch(int start, int end, TreeCell *tab, CKGUID key) {
    return 0;
}

void CKParameterManager::RecurseDelete(TreeCell *cell) {

}

CKBOOL CKParameterManager::IsDerivedFromIntern(int child, int parent) {
    return 0;
}

CKBOOL CKParameterManager::RemoveAllParameterTypes() {
    return 0;
}

CKBOOL CKParameterManager::RemoveAllOperations() {
    return 0;
}

CKStructHelper::CKStructHelper(CKParameter* Param)
{
}

CKStructHelper::CKStructHelper(CKContext* ctx, CKGUID PGuid, CK_ID* Data)
{
}

CKStructHelper::CKStructHelper(CKContext* ctx, CKParameterType PType, CK_ID* Data)
{
}

char* CKStructHelper::GetMemberName(int Pos)
{
    return nullptr;
}

CKGUID CKStructHelper::GetMemberGUID(int Pos)
{
    return CKGUID();
}

int CKStructHelper::GetMemberCount()
{
    return 0;
}
