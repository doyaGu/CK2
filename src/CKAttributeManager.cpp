#include "CKAttributeManager.h"

#include "CKContext.h"
#include "CKStateChunk.h"

CKAttributeType CKAttributeManager::RegisterNewAttributeType(CKSTRING Name, CKGUID ParameterType, CK_CLASSID CompatibleCid, CK_ATTRIBUT_FLAGS flags) {
    return 0;
}

void CKAttributeManager::UnRegisterAttribute(CKAttributeType AttribType) {
}

void CKAttributeManager::UnRegisterAttribute(CKSTRING atname) {
}

CKSTRING CKAttributeManager::GetAttributeNameByType(CKAttributeType AttribType) {
    return nullptr;
}

CKAttributeType CKAttributeManager::GetAttributeTypeByName(CKSTRING AttribName) {
    return 0;
}

void CKAttributeManager::SetAttributeNameByType(CKAttributeType AttribType, CKSTRING name) {
}

int CKAttributeManager::GetAttributeCount() {
    return 0;
}

CKGUID CKAttributeManager::GetAttributeParameterGUID(CKAttributeType AttribType) {
    return CKGUID();
}

CKParameterType CKAttributeManager::GetAttributeParameterType(CKAttributeType AttribType) {
    return 0;
}

CK_CLASSID CKAttributeManager::GetAttributeCompatibleClassId(CKAttributeType AttribType) {
    return 0;
}

CKBOOL CKAttributeManager::IsAttributeIndexValid(CKAttributeType index) {
    return 0;
}

CKBOOL CKAttributeManager::IsCategoryIndexValid(CKAttributeCategory index) {
    return 0;
}

CK_ATTRIBUT_FLAGS CKAttributeManager::GetAttributeFlags(CKAttributeType AttribType) {
    return CK_ATTRIBUT_DONOTCOPY;
}

void CKAttributeManager::SetAttributeCallbackFunction(CKAttributeType AttribType, CKATTRIBUTECALLBACK fct, void *arg) {
}

void CKAttributeManager::SetAttributeDefaultValue(CKAttributeType AttribType, CKSTRING DefaultVal) {
}

CKSTRING CKAttributeManager::GetAttributeDefaultValue(CKAttributeType AttribType) {
    return nullptr;
}

const XObjectPointerArray &CKAttributeManager::GetAttributeListPtr(CKAttributeType AttribType) {
    XObjectPointerArray a;
    return a;
}

const XObjectPointerArray &CKAttributeManager::GetGlobalAttributeListPtr(CKAttributeType AttribType) {
    XObjectPointerArray a;
    return a;
}

const XObjectPointerArray &CKAttributeManager::FillListByAttributes(CKAttributeType *ListAttrib, int AttribCount) {
    XObjectPointerArray a;
    return a;
}

const XObjectPointerArray &
CKAttributeManager::FillListByGlobalAttributes(CKAttributeType *ListAttrib, int AttribCount) {
    XObjectPointerArray a;
    return a;
}

int CKAttributeManager::GetCategoriesCount() {
    return 0;
}

CKSTRING CKAttributeManager::GetCategoryName(CKAttributeCategory index) {
    return nullptr;
}

CKAttributeCategory CKAttributeManager::GetCategoryByName(CKSTRING Name) {
    return 0;
}

void CKAttributeManager::SetCategoryName(CKAttributeCategory catType, CKSTRING name) {
}

CKAttributeCategory CKAttributeManager::AddCategory(CKSTRING Category, CKDWORD flags) {
    return 0;
}

void CKAttributeManager::RemoveCategory(CKSTRING Category) {
}

CKDWORD CKAttributeManager::GetCategoryFlags(CKAttributeCategory cat) {
    return 0;
}

CKDWORD CKAttributeManager::GetCategoryFlags(CKSTRING cat) {
    return 0;
}

void CKAttributeManager::SetAttributeCategory(CKAttributeType AttribType, CKSTRING Category) {
}

CKSTRING CKAttributeManager::GetAttributeCategory(CKAttributeType AttribType) {
    return nullptr;
}

CKAttributeCategory CKAttributeManager::GetAttributeCategoryIndex(CKAttributeType AttribType) {
    return 0;
}

void CKAttributeManager::AddAttributeToObject(CKAttributeType AttribType, CKBeObject *beo) {
}

void CKAttributeManager::RefreshList(CKObject *obj, CKScene *scene) {
}

CKPluginEntry *CKAttributeManager::GetCreatorDll(int pos) {
    if (pos >= 0 && pos < m_AttributeDescCount) {
        return m_AttributeInfos[pos]->CreatorDll;
    }
    return nullptr;
}

void CKAttributeManager::NewActiveScene(CKScene *scene) {
}

void CKAttributeManager::ObjectAddedToScene(CKBeObject *beo, CKScene *scene) {
}

void CKAttributeManager::RemoveAttributeFromObject(CKBeObject *beo) {
}

void CKAttributeManager::PatchRemapBeObjectFileChunk(CKStateChunk *chunk) {
    if (m_ConversionTable && m_ConversionTableCount > 0) {
        chunk->AttributePatch(1, m_ConversionTable, m_ConversionTableCount);
    }
}

void CKAttributeManager::PatchRemapBeObjectStateChunk(CKStateChunk *chunk) {
    if (m_ConversionTable && m_ConversionTableCount > 0) {
        chunk->AttributePatch(0, m_ConversionTable, m_ConversionTableCount);
    }
}

CKAttributeManager::CKAttributeManager(CKContext *Context): CKBaseManager(Context, ATTRIBUTE_MANAGER_GUID, "Attribute Manager") {
    m_AttributeDescCount = 0;
    m_AttributeInfos = nullptr;
    m_AttributeCategoryCount = 0;
    m_AttributeCategories = nullptr;
    m_ConversionTableCount = 0;
    m_ConversionTable = nullptr;
    m_Saving = FALSE;
    m_Context->RegisterNewManager(this);
}

CKAttributeManager::~CKAttributeManager() {
}

CKERROR CKAttributeManager::PreClearAll() {
    return CKBaseManager::PreClearAll();
}

CKERROR CKAttributeManager::PostLoad() {
    return CKBaseManager::PostLoad();
}

CKERROR CKAttributeManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile) {
    return CKBaseManager::LoadData(chunk, LoadedFile);
}

CKStateChunk *CKAttributeManager::SaveData(CKFile *SavedFile) {
    return CKBaseManager::SaveData(SavedFile);
}

CKERROR CKAttributeManager::SequenceAddedToScene(CKScene *scn, CK_ID *objid, int count) {
    return CKBaseManager::SequenceAddedToScene(scn, objid, count);
}

CKERROR CKAttributeManager::SequenceRemovedFromScene(CKScene *scn, CK_ID *objid, int count) {
    return CKBaseManager::SequenceRemovedFromScene(scn, objid, count);
}
