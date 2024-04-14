#include "CKObjectManager.h"

#include "CKContext.h"
#include "CKSceneObject.h"

extern int g_MaxClassID;

int CKObjectManager::ObjectsByClass(CK_CLASSID cid, CKBOOL derived, CK_ID *obj_ids) {
    if (cid < 0 || cid >= g_MaxClassID)
        return 0;

    if (derived) {
        int count = 0;
        if (g_MaxClassID > CKCID_OBJECT) {
            for (CK_CLASSID i = CKCID_OBJECT; i < g_MaxClassID; ++i) {
                if (CKIsChildClassOf(i, cid)) {
                    XObjectArray &array = m_ObjectLists[i];
                    const int size = array.Size();
                    count += size;
                    if (obj_ids) {
                        memcpy(obj_ids, array.Begin(), sizeof(CK_ID) * size);
                        obj_ids += size;
                    }
                }
            }
        }
        return count;
    } else {
        XObjectArray &array = m_ObjectLists[cid];
        const int size = array.Size();
        if (obj_ids) {
            memcpy(obj_ids, array.Begin(), sizeof(CK_ID) * size);
        }
        return size;
    }
}

int CKObjectManager::GetObjectsCount() {
    return m_ObjectsCount;
}

CKERROR CKObjectManager::DeleteAllObjects() {
    for (CK_CLASSID i = CKCID_OBJECT; i < g_MaxClassID; ++i) {
        m_ObjectLists[i].Clear();
    }

    if (m_Objects) {
        for (int i = 0; i < m_ObjectsCount; ++i) {
            CKObject *obj = m_Objects[i];
            if (obj)
                delete obj;
            m_Objects[i] = nullptr;
        }
    }

    m_ObjectsCount = 0;
    m_Array.Clear();
    m_ObjectAppData.Clear();

    return CK_OK;
}

CKERROR CKObjectManager::ClearAllObjects() {
    return 0;
}

CKBOOL CKObjectManager::IsObjectSafe(CKObject *iObject) {
    if (!iObject)
        return FALSE;

    for (int i = 0; i < m_ObjectsCount; ++i) {
        CKObject *obj = m_Objects[i];
        if (obj == iObject)
            return TRUE;
    }

    return FALSE;
}

CKERROR CKObjectManager::DeleteObjects(CK_ID *obj_ids, int Count, CK_CLASSID cid, CKDWORD Flags) {
    return 0;
}

CKERROR CKObjectManager::GetRootEntities(XObjectPointerArray &array) {
    return 0;
}

int CKObjectManager::GetObjectsCountByClassID(CK_CLASSID cid) {
    if (cid < 0 || cid >= g_MaxClassID)
        return 0;

    return m_ObjectLists[cid].Size();
}

CK_ID *CKObjectManager::GetObjectsListByClassID(CK_CLASSID cid) {
    if (cid < 0 || cid >= g_MaxClassID)
        return nullptr;

    return m_ObjectLists[cid].Begin();
}

CK_ID CKObjectManager::RegisterObject(CKObject *iObject) {
    return 0;
}

void CKObjectManager::FinishRegisterObject(CKObject *iObject) {

}

void CKObjectManager::UnRegisterObject(CK_ID id) {

}

CKObject *CKObjectManager::GetObjectByName(CKSTRING name, CKObject *previous) {
    return nullptr;
}

CKObject *CKObjectManager::GetObjectByName(CKSTRING name, CK_CLASSID cid, CKObject *previous) {
    return nullptr;
}

CKObject *CKObjectManager::GetObjectByNameAndParentClass(CKSTRING name, CK_CLASSID pcid, CKObject *previous) {
    return nullptr;
}

CKERROR CKObjectManager::GetObjectListByType(CK_CLASSID cid, XObjectPointerArray &array, CKBOOL derived) {
    return 0;
}

CKBOOL CKObjectManager::InLoadSession() {
    return m_InLoadSession;
}

void CKObjectManager::StartLoadSession(int MaxObjectID) {
    if (!m_InLoadSession)
    {
        if (m_LoadSession)
            delete[] m_LoadSession;
        m_MaxObjectID = MaxObjectID;
        m_LoadSession = new CK_ID[MaxObjectID];
        memset(m_LoadSession, 0, sizeof(CK_ID) * MaxObjectID);
        m_InLoadSession = TRUE;
    }
}

void CKObjectManager::EndLoadSession() {
    delete[] m_LoadSession;
    m_LoadSession = nullptr;
    m_InLoadSession = FALSE;
}

void CKObjectManager::RegisterLoadObject(CKObject *iObject, int ObjectID) {
    if ( m_LoadSession && ObjectID >= 0 && ObjectID < m_MaxObjectID) {
        m_LoadSession[ObjectID] = (iObject) ? iObject->GetID() : 0;
    }
}

CK_ID CKObjectManager::RealId(CK_ID id) {
    if (!m_LoadSession)
        return id;
    else if (id < m_MaxObjectID)
        return m_LoadSession[id];
    else
        return 0;
}

int CKObjectManager::CheckIDArray(CK_ID *obj_ids, int Count) {
    int i = 0;
    for (; i < Count; ++i)
    {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (!obj)
            break;
    }

    for (int j = i + 1; j < Count; ++j) {
        CK_ID id = obj_ids[j];
        CKObject *obj = m_Objects[id];
        if (obj) {
            obj_ids[i++] = id;
        }
    }
    return i;
}

int CKObjectManager::CheckIDArrayPredeleted(CK_ID *obj_ids, int Count) {
    int i = 0;
    for (; i < Count; ++i)
    {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (!obj)
            break;
        if (obj->IsToBeDeleted())
            break;
    }

    for (int j = i + 1; j < Count; ++j) {
        CK_ID id = obj_ids[j];
        CKObject *obj = m_Objects[id];
        if (obj && !obj->IsToBeDeleted()) {
            obj_ids[i++] = id;
        }
    }
    return i;
}

CKDeferredDeletion *CKObjectManager::MatchDeletion(CKDependencies *depoptions, CKDWORD Flags) {
    XArray<CKDeferredDeletion *> &deletions = m_DeferredDeletions[Flags];
    for (XArray<CKDeferredDeletion *>::Iterator it = deletions.Begin(); it != deletions.End(); ++it) {
        if ((*it)->m_Dependencies == depoptions)
            return (*it);
    }
    return nullptr;
}

void CKObjectManager::RegisterDeletion(CKDeferredDeletion *deletion) {
    if (deletion) {
        m_DeferredDeletions[deletion->m_Flag].PushBack(deletion);
    }
}

int CKObjectManager::GetDynamicIDCount() {
    return m_DynamicObjects.Size();
}

CK_ID CKObjectManager::GetDynamicID(int index) {
    return m_DynamicObjects[index];
}

void CKObjectManager::DeleteAllDynamicObjects() {
    m_NeedDeleteAllDynamicObjects = TRUE;
}

void CKObjectManager::SetDynamic(CKObject *iObject) {
    if (iObject) {
        if (!iObject->IsDynamic()) {
            iObject->ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
            m_DynamicObjects.PushBack(iObject->GetID());
        }
    }
}

void CKObjectManager::UnSetDynamic(CKObject *iObject) {
    if (iObject) {
        if (iObject->IsDynamic()) {
            iObject->ModifyObjectFlags(0, CK_OBJECT_DYNAMIC);
            m_DynamicObjects.RemoveObject(iObject);
        }
    }
}

CKERROR CKObjectManager::PostProcess() {
    return CKBaseManager::PostProcess();
}

CKERROR CKObjectManager::OnCKReset() {
    if (!m_DynamicObjects.IsEmpty())
    {
        m_Context->DestroyObjects(m_DynamicObjects.Begin(), m_DynamicObjects.Size());
        m_DynamicObjects.Clear();
    }

    return CK_OK;
}

CKObjectManager::~CKObjectManager() {
    delete[] m_Objects;
    delete[] m_LoadSession;
    m_GroupGlobalIndex.Clear();
    m_SceneGlobalIndex.Clear();
    m_DynamicObjects.Clear();

    for (int i = 0; i < 4; ++i)
        m_DeferredDeletions[i].Clear();

    m_Array.Clear();
    m_SingleObjectActivities.Clear();
    m_ObjectAppData.Clear();
    m_ObjectLists.Clear();
}

CKObjectManager::CKObjectManager(CKContext *Context) : CKBaseManager(Context, OBJECT_MANAGER_GUID, "Object Manager") {
    m_MaxObjectID = 0;
    m_ObjectsCount = 0;
    m_Count = 1000;
    m_Objects = new CKObject *[1000];
    memset(m_Objects, 0, m_Count * sizeof(CKObject *));
    m_LoadSession = nullptr;
    m_InLoadSession = 0;
    m_NeedDeleteAllDynamicObjects = 0;
    m_ObjectLists.Reserve(g_MaxClassID);
    m_Context->RegisterNewManager(this);
}

CKDWORD CKObjectManager::GetGroupGlobalIndex() {
    int index = m_GroupGlobalIndex.GetSetBitPosition(0);
    m_GroupGlobalIndex.Set(index);
    return index;
}

void CKObjectManager::ReleaseGroupGlobalIndex(CKDWORD index) {
    m_GroupGlobalIndex.Unset(index);
}

int CKObjectManager::GetSceneGlobalIndex() {
    int index = m_SceneGlobalIndex.GetSetBitPosition(0);
    m_SceneGlobalIndex.Set(index);
    return index;
}

void CKObjectManager::ReleaseSceneGlobalIndex(int index) {
    m_SceneGlobalIndex.Unset(index);
}

void *CKObjectManager::GetObjectAppData(CK_ID id) {
    XObjectAppDataTable::Iterator it = m_ObjectAppData.Find(id);
    if (it == m_ObjectAppData.End()) {
        return nullptr;
    }
    return *it;
}

void CKObjectManager::SetObjectAppData(CK_ID id, void *arg) {
    m_ObjectAppData[id] = arg;
}

void CKObjectManager::AddSingleObjectActivity(CKSceneObject *o, CK_ID id) {
    if (o) {
        m_SingleObjectActivities[o->GetID()] = id;
    }
}

int CKObjectManager::GetSingleObjectActivity(CKSceneObject *o, CK_ID &id) {
    if (!o) {
        return 0;
    }

    XHashItID it = m_SingleObjectActivities.Find(o->GetID());
    if (it == m_SingleObjectActivities.End()) {
        return 0;
    }

    id = *it;
    return 1;
}
