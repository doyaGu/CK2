#include "CKBehaviorPrototype.h"

int CKBehaviorPrototype::DeclareInput(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::DeclareOutput(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::DeclareInParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval) {
    return 0;
}

int CKBehaviorPrototype::DeclareInParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize) {
    return 0;
}

int CKBehaviorPrototype::DeclareOutParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval) {
    return 0;
}

int CKBehaviorPrototype::DeclareOutParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize) {
    return 0;
}

int CKBehaviorPrototype::DeclareLocalParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval) {
    return 0;
}

int CKBehaviorPrototype::DeclareLocalParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize) {
    return 0;
}

int CKBehaviorPrototype::DeclareSetting(CKSTRING name, CKGUID guid_type, CKSTRING defaultval) {
    return 0;
}

int CKBehaviorPrototype::DeclareSetting(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize) {
    return 0;
}

void CKBehaviorPrototype::SetGuid(CKGUID guid) {
    m_Guid = guid;
}

CKGUID CKBehaviorPrototype::GetGuid() {
    return m_Guid;
}

void CKBehaviorPrototype::SetFlags(CK_BEHAVIORPROTOTYPE_FLAGS flags) {
    m_bFlags = flags;
}

CK_BEHAVIORPROTOTYPE_FLAGS CKBehaviorPrototype::GetFlags() {
    return m_bFlags;
}

void CKBehaviorPrototype::SetApplyToClassID(CK_CLASSID cid) {

}

CK_CLASSID CKBehaviorPrototype::GetApplyToClassID() {
    return 0;
}

void CKBehaviorPrototype::SetFunction(CKBEHAVIORFCT fct) {
    m_FctPtr = fct;
}

CKBEHAVIORFCT CKBehaviorPrototype::GetFunction() {
    return m_FctPtr;
}

void CKBehaviorPrototype::SetBehaviorCallbackFct(CKBEHAVIORCALLBACKFCT fct, CKDWORD CallbackMask, void *param) {
    m_CallbackFctPtr = fct;
    m_CallBackMask = CallbackMask;
    m_CallBackParam = param;
}

CKBEHAVIORCALLBACKFCT CKBehaviorPrototype::GetBehaviorCallbackFct() {
    return m_CallbackFctPtr;
}

void CKBehaviorPrototype::SetBehaviorFlags(CK_BEHAVIOR_FLAGS flags) {

}

CK_BEHAVIOR_FLAGS CKBehaviorPrototype::GetBehaviorFlags() {
    return CKBEHAVIOR_ACTIVE;
}

CKObjectDeclaration *CKBehaviorPrototype::GetSoureObjectDeclaration() {
    return m_SourceObjectDeclaration;
}

int CKBehaviorPrototype::GetInIOIndex(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::GetOutIOIndex(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::GetInParamIndex(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::GetOutParamIndex(CKSTRING name) {
    return 0;
}

int CKBehaviorPrototype::GetLocalParamIndex(CKSTRING name) {
    return 0;
}

CKSTRING CKBehaviorPrototype::GetInIOName(int idx) {
    return nullptr;
}

CKSTRING CKBehaviorPrototype::GetOutIOIndex(int idx) {
    return nullptr;
}

CKSTRING CKBehaviorPrototype::GetInParamIndex(int idx) {
    return nullptr;
}

CKSTRING CKBehaviorPrototype::GetOutParamIndex(int idx) {
    return nullptr;
}

CKSTRING CKBehaviorPrototype::GetLocalParamIndex(int idx) {
    return nullptr;
}

CKBehaviorPrototype::~CKBehaviorPrototype() {

}

CKBehaviorPrototype::CKBehaviorPrototype(CKSTRING Name) {

}