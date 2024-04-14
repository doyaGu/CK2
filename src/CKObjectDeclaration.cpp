#include "CKObjectDeclaration.h"

void CKObjectDeclaration::SetDescription(CKSTRING Description) {
    CKDeletePointer(m_Description);
    m_Description = CKStrdup(Description);
}

CKSTRING CKObjectDeclaration::GetDescription() {
    return m_Description;
}

void CKObjectDeclaration::SetGuid(CKGUID guid) {
    m_Guid = guid;
}

CKGUID CKObjectDeclaration::GetGuid() {
    return m_Guid;
}

void CKObjectDeclaration::SetType(int type) {
    m_Type = type;
}

int CKObjectDeclaration::GetType() {
    return m_Type;
}

void CKObjectDeclaration::NeedManager(CKGUID Manager) {
    CKGUID *it;
    for (it = m_ManagersGuid.Begin(); it != m_ManagersGuid.End(); ++it) {
        if (*it == Manager)
            break;
    }
    if (it == m_ManagersGuid.End())
        m_ManagersGuid.PushBack(Manager);
}

void CKObjectDeclaration::SetCreationFunction(CKDLL_CREATEPROTOFUNCTION f) {
    m_CreationFunction = f;
}

CKDLL_CREATEPROTOFUNCTION CKObjectDeclaration::GetCreationFunction() {
    return m_CreationFunction;
}

void CKObjectDeclaration::SetAuthorGuid(CKGUID guid) {
    m_AuthorGuid = guid;
}

CKGUID CKObjectDeclaration::GetAuthorGuid() {
    return m_AuthorGuid;
}

void CKObjectDeclaration::SetAuthorName(CKSTRING Name) {
    CKDeletePointer(m_AuthorName);
    m_AuthorName = CKStrdup(Name);
}

CKSTRING CKObjectDeclaration::GetAuthorName() {
    return m_AuthorName;
}

void CKObjectDeclaration::SetVersion(CKDWORD verion) {
    m_Version = verion;
}

CKDWORD CKObjectDeclaration::GetVersion() {
    return m_Version;
}

void CKObjectDeclaration::SetCompatibleClassId(CK_CLASSID id) {
    m_CompatibleClassID = id;
}

CK_CLASSID CKObjectDeclaration::GetCompatibleClassId() {
    return m_CompatibleClassID;
}

void CKObjectDeclaration::SetCategory(CKSTRING cat) {
    CKDeletePointer(m_Category);
    m_Category = CKStrdup(cat);
}

CKSTRING CKObjectDeclaration::GetCategory() {
    return m_Category;
}

CKObjectDeclaration::CKObjectDeclaration(CKSTRING Name) {
    m_Version = -1;
    m_Description = NULL;
    m_Guid = CKGUID();
    m_Type = CKDLL_BEHAVIORPROTOTYPE;
    m_CreationFunction = NULL;
    m_AuthorGuid = CKGUID();
    m_AuthorName = NULL;
    m_Proto = NULL;
    m_CompatibleClassID = 1;
    m_Category = NULL;
    m_Name = Name;
    m_PluginIndex = -1;
}

CKObjectDeclaration::~CKObjectDeclaration() {
    CKDeletePointer(m_Description);
    CKDeletePointer(m_AuthorName);
    CKDeletePointer(m_Category);
}
