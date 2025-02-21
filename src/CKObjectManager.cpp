#include "CKObjectManager.h"

#include "CKContext.h"
#include "CKSceneObject.h"
#include "CKRenderManager.h"
#include "CKRenderContext.h"
#include "CKMaterial.h"
#include "CK2dEntity.h"
#include "CK3dEntity.h"
#include "CKBehavior.h"

extern int g_MaxClassID;
extern XClassInfoArray g_CKClassInfo;

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

CKObject *CKObjectManager::GetObjectA(CK_ID id) {
    return m_Objects[id];
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
    m_FreeObjectIDs.Clear();
    m_ObjectAppData.Clear();

    return CK_OK;
}

CKERROR CKObjectManager::ClearAllObjects() {
    XBitArray objs;
    objs.CheckSize(m_ObjectsCount);

    m_Context->m_InClearAll = TRUE;

    for (int i = 0; i < m_ObjectsCount; ++i) {
        CKObject *obj = m_Objects[i];
        obj->m_ObjectFlags |= CK_OBJECT_TOBEDELETED;
    }

    CKRenderManager *rm = m_Context->m_RenderManager;
    if (rm) {
        int rcCount = rm->GetRenderContextCount();
        for (int i = 0; i < rcCount; ++i) {
            CKRenderContext *rc = rm->GetRenderContext(i);
            if (rc) {
                objs.Set(rc->GetID());
                rc->m_ObjectFlags &= ~CK_OBJECT_TOBEDELETED;

                CK3dEntity *ent = rc->GetViewpoint();
                if (ent) {
                    objs.Set(ent->GetID());
                    ent->m_ObjectFlags &= ~CK_OBJECT_TOBEDELETED;
                }

                CKMaterial *mat = rc->GetBackgroundMaterial();
                if (mat) {
                    mat->SetTexture(nullptr);
                    objs.Set(mat->GetID());
                    mat->m_ObjectFlags &= ~CK_OBJECT_TOBEDELETED;
                }

                CK2dEntity *backEnt = rc->Get2dRoot(TRUE);
                if (backEnt) {
                    objs.Set(backEnt->GetID());
                    backEnt->m_ObjectFlags &= ~CK_OBJECT_TOBEDELETED;
                }

                CK2dEntity *foreEnt = rc->Get2dRoot(FALSE);
                if (foreEnt) {
                    objs.Set(foreEnt->GetID());
                    foreEnt->m_ObjectFlags &= ~CK_OBJECT_TOBEDELETED;
                }
            }
        }
    }

    for (CK_CLASSID cid = 0; cid < g_MaxClassID; ++cid) {
        m_ObjectLists[cid].Clear();
    }

    m_FreeObjectIDs.Clear();
    m_DynamicObjects.Clear();

    for (int i = m_ObjectsCount; i > 0; --i) {
        if (i >= objs.Size() || objs.IsSet(i)) {
            CKObject *obj = m_Objects[i];
            if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
                CKBehavior *beh = (CKBehavior *) obj;
                beh->CallCallbackFunction(CKM_BEHAVIORDETACH);
                beh->~CKBehavior();
            }
        }
    }

    for (int i = m_ObjectsCount; i > 0; --i) {
        if (i >= objs.Size() || objs.IsSet(i)) {
            CKObject *obj = m_Objects[i];
            obj->~CKObject();
            --m_ObjectsCount;
        }
    }

    m_Context->m_InClearAll = FALSE;

    for (int i = 0; i < m_ObjectsCount; ++i) {
        CKObject *obj = m_Objects[i];
        if (obj && obj->GetClassID() == CKCID_RENDERCONTEXT) {
            obj->CheckPostDeletion();
        }
        m_ObjectLists.PushBack(i);
    }

    m_ObjectAppData.Clear();
    m_SingleObjectActivities.Clear();

    return CK_OK;
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
    if (m_Context->m_InClearAll)
        return CK_OK;

    if (Count == 0)
        return CKERR_INVALIDPARAMETER;

    CKDWORD flags = CK_OBJECT_TOBEDELETED;
    if (Flags & CK_DESTROY_FREEID)
        flags |= CK_OBJECT_FREEID;

    XBitArray classIDs;
    for (int i = 0; i < Count; ++i) {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (obj) {
            obj->m_ObjectFlags |= flags;
            classIDs.Set(obj->GetClassID());
        }
    }

    const int classCount = m_ObjectLists.Size();
    for (int i = 0; i < classCount; ++i) {
        if (i < classIDs.Size() && classIDs.IsSet(i)) {
            XObjectArray &array = m_ObjectLists[i];
            for (int j = 0; j < array.Size(); ++j) {
                int newSize = CheckIDArrayPredeleted(array.Begin(), array.Size());
                array.Resize(newSize);
            }
        }
    }

    if ((Flags & CK_DESTROY_NONOTIFY) == 0) {
        m_Context->ExecuteManagersOnSequenceToBeDeleted(obj_ids, Count);
        if (classCount > 1) {
            for (int i = classCount; i > 0; --i) {
                if (classIDs.CheckCommon(g_CKClassInfo[i].CommonToBeNotify)) {
                    XObjectArray &array = m_ObjectLists[i];
                    for (int j = 0; j < array.Size(); ++j) {
                        CKObject *obj = m_Objects[array[j]];
                        if (obj) {
                            obj->CheckPreDeletion();
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < Count; ++i) {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (obj) {
            obj->PreDelete();
        }
    }

    for (int i = 0; i < Count; ++i) {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (obj) {
            obj->~CKObject();
        }
    }

    m_DynamicObjects.Check(m_Context);
    if ((Flags & CK_DESTROY_NONOTIFY) == 0 && classCount > 1) {
        for (int i = 0; i < classCount; ++i) {
            if (classIDs.CheckCommon(g_CKClassInfo[i].CommonToBeNotify)) {
                XObjectArray &array = m_ObjectLists[i];
                for (int j = 0; j < array.Size(); ++j) {
                    CKObject *obj = m_Objects[array[j]];
                    if (obj) {
                        obj->CheckPostDeletion();
                    }
                }
            }
        }
    }

    if ((Flags & CK_DESTROY_NONOTIFY) == 0) {
        m_Context->ExecuteManagersOnSequenceDeleted(obj_ids, Count);
    }

    return CK_OK;
}

CKERROR CKObjectManager::GetRootEntities(XObjectPointerArray &array) {
    if (g_MaxClassID == 0)
        return CK_OK;

    for (CK_CLASSID cid = 0; cid < g_MaxClassID; ++cid) {
        if (CKIsChildClassOf(cid, CKCID_3DENTITY)) {
            auto &objs = m_ObjectLists[cid];
            for (auto it = objs.Begin(); it != objs.End(); ++it) {
                CK3dEntity *ent = (CK3dEntity *) m_Objects[*it];
                if (ent && ent->GetParent() == nullptr)
                    array.PushBack(ent);
            }
        }
    }

    return CK_OK;
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
    if (m_FreeObjectIDs.Size() != 0) {
        CK_ID id = m_FreeObjectIDs.PopBack();
        m_Objects[id] = iObject;
        return id;
    }

    ++m_ObjectsCount;
    if (m_ObjectsCount >= m_Count) {
        m_Count += 100;
        CKObject **objs = new CKObject *[m_Count];
        memset(objs, 0, sizeof(CKObject *) * m_Count);
        memcpy(objs, m_Objects, sizeof(CKObject *) * (m_Count - 100));
        delete[] m_Objects;
        m_Objects = objs;
    }
    m_Objects[m_ObjectsCount] = iObject;
    return m_ObjectsCount;
}

void CKObjectManager::FinishRegisterObject(CKObject *iObject) {
    if (iObject->IsDynamic())
        m_DynamicObjects.PushBack(iObject->GetID());
    else
        m_ObjectLists[iObject->GetClassID()].PushBack(iObject->GetID());
}

void CKObjectManager::UnRegisterObject(CK_ID id) {
    if (id < m_Count && id != 0) {
        if (m_Objects[id]->GetObjectFlags() & CK_OBJECT_FREEID && !m_Context->m_InClearAll) {
            m_FreeObjectIDs.PushBack(id);
        }
        m_Objects[id] = nullptr;
        if (m_ObjectAppData.Size() != 0) {
            m_ObjectAppData.Remove(id);
        }
        if (m_SingleObjectActivities.Size() != 0) {
            m_SingleObjectActivities.Remove(id);
        }
    }
}

CKObject *CKObjectManager::GetObjectByName(CKSTRING name, CKObject *previous) {
    if (!name)
        return nullptr;

    CK_ID id = 0;
    if (previous)
        id = previous->GetID();
    for (CK_ID i = id; i < m_ObjectsCount; ++i) {
        CKObject *obj = m_Objects[i];
        if (obj && strcmp(obj->GetName(), name) == 0) {
            return obj;
        }
    }

    return nullptr;
}

CKObject *CKObjectManager::GetObjectByName(CKSTRING name, CK_CLASSID cid, CKObject *previous) {
    if (!name)
        return nullptr;

    CK_ID id = 0;
    if (previous)
        id = previous->GetID();

    int i = 0;
    const int count = m_ObjectLists[cid].Size();
    if (id != 0) {
        while (i < count) {
            if (id == m_ObjectLists[cid][i])
                break;
            ++i;
        }
    }

    for (int j = i + 1; j < count; ++j) {
        id = m_ObjectLists[cid][j];
        CKObject *obj = m_Objects[id];
        if (obj && strcmp(obj->GetName(), name) == 0) {
            return obj;
        }
    }

    return nullptr;
}

CKObject *CKObjectManager::GetObjectByNameAndParentClass(CKSTRING name, CK_CLASSID pcid, CKObject *previous) {
    if (!name)
        return nullptr;

    CK_ID id = 0;
    if (previous)
        id = previous->GetID();

    int i = 0;
    int count = 0;
    CK_CLASSID cid = CKCID_OBJECT;
    if (id != 0 && g_MaxClassID > CKCID_OBJECT) {
        while (cid < g_MaxClassID) {
            auto &ci = g_CKClassInfo[cid];
            if (ci.Parent == pcid && ci.Children.IsSet(cid)) {
                count = m_ObjectLists[cid].Size();
                while (i < count) {
                    if (id == m_ObjectLists[cid][i])
                        break;
                    ++i;
                }
                break;
            }
            ++cid;
        }
    }

    if (cid >= g_MaxClassID)
        return nullptr;

    CK_CLASSID c = cid;
    while (c < g_MaxClassID) {
        auto &ci = g_CKClassInfo[c];
        if (c < ci.Children.Size() && ci.Children.IsSet(c)) {
            if (++i < m_ObjectLists[c].Size())
                break;
        }
        i = -1;
        ++c;
    }

    if (c >= g_MaxClassID)
        return nullptr;

    count = m_ObjectLists[c].Size();
    for (int j = i; j < count; ++j) {
        id = m_ObjectLists[c][j];
        CKObject *obj = m_Objects[id];
        if (obj && strcmp(obj->GetName(), name) == 0) {
            return obj;
        }
    }

    return nullptr;
}

CKERROR CKObjectManager::GetObjectListByType(CK_CLASSID cid, XObjectPointerArray &array, CKBOOL derived) {
    if (derived) {
        const int count = m_ObjectLists.Size();
        for (int i = 0; i < count; ++i) {
            if (CKIsChildClassOf(i, cid)) {
                auto &objs = m_ObjectLists[i];
                for (auto it = objs.Begin(); it != objs.End(); ++it) {
                    CKObject *obj = m_Context->GetObject(*it);
                    array.PushBack(obj);
                }
            }
        }
    } else {
        auto &objs = m_ObjectLists[cid];
        for (auto it = objs.Begin(); it != objs.End(); ++it) {
            CKObject *obj = m_Context->GetObject(*it);
            array.PushBack(obj);
        }
    }

    return CK_OK;
}

CKBOOL CKObjectManager::InLoadSession() {
    return m_InLoadSession;
}

void CKObjectManager::StartLoadSession(int MaxObjectID) {
    if (!m_InLoadSession) {
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
    if (m_LoadSession && ObjectID >= 0 && ObjectID < m_MaxObjectID) {
        m_LoadSession[ObjectID] = (iObject) ? iObject->GetID() : 0;
    }
}

CK_ID CKObjectManager::RealId(CK_ID id) {
    if (!m_LoadSession)
        return id;
    if (id < m_MaxObjectID)
        return m_LoadSession[id];
    return 0;
}

int CKObjectManager::CheckIDArray(CK_ID *obj_ids, int Count) {
    int i = 0;
    for (; i < Count; ++i) {
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
    for (; i < Count; ++i) {
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
        if ((*it)->m_DependenciesPtr == depoptions)
            return *it;
    }
    return nullptr;
}

void CKObjectManager::RegisterDeletion(CKDeferredDeletion *deletion) {
    if (deletion) {
        m_DeferredDeletions[deletion->m_Flags].PushBack(deletion);
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
    if (!m_DynamicObjects.IsEmpty()) {
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

    m_FreeObjectIDs.Clear();
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
