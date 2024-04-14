#include "CKDependencies.h"

#include "CKContext.h"
#include "CKObject.h"

void CKDependenciesContext::AddObjects(CK_ID *ids, int count) {
    m_Objects.Resize(m_Objects.Size() + count);
    for (int i = 0; i < count; ++i) {
        if (m_CKContext->GetObject(ids[i]))
            m_Objects.PushBack(ids[i]);
    }
}

int CKDependenciesContext::GetObjectsCount() {
    return m_Objects.Size();
}

CKObject *CKDependenciesContext::GetObjects(int i) {
    return m_CKContext->GetObject(m_Objects[i]);
}

CK_ID CKDependenciesContext::RemapID(CK_ID &id) {
    XHashItID it = m_MapID.Find(id);
    id = (*it) ? (*it) : 0;
    return id;
}

CKObject *CKDependenciesContext::Remap(const CKObject *o) {
    if (!o)
        return nullptr;

    CK_ID id = o->m_ID;
    XHashItID it =  m_MapID.Find(id);
    id = (*it) ? (*it) : 0;
    return m_CKContext->GetObject(id);
}

XObjectArray CKDependenciesContext::FillDependencies() {
    return XObjectArray();
}

XObjectArray CKDependenciesContext::FillRemappedDependencies() {
    return XObjectArray();
}

CKDWORD CKDependenciesContext::GetClassDependencies(int c) {
    return 0;
}

void CKDependenciesContext::Copy(CKSTRING appendstring) {

}

CKBOOL CKDependenciesContext::ContainClassID(CK_CLASSID Cid) {
    return 0;
}

CKBOOL CKDependenciesContext::AddDependencies(CK_ID id) {
    return 0;
}

void CKDependenciesContext::Clear() {
    m_DynamicObjects.Clear();
    m_MapID.Clear();
    m_Objects.Clear();
    m_Scripts.Clear();
    m_ObjectsClassMask.Clear();
}

void CKDependenciesContext::FinishPrepareDependencies(CKObject *iMySelf, CK_ID Cid) {

}
