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

    int count = 0;
    if (derived) {
        // Iterate through all class IDs to find derived classes
        for (CK_CLASSID currentCid = CKCID_OBJECT; currentCid < g_MaxClassID; ++currentCid) {
            if (!CKIsChildClassOf(currentCid, cid))
                continue;

            XObjectArray &objArray = m_ObjectLists[currentCid];
            const int classCount = objArray.Size();

            if (obj_ids) {
                memcpy(obj_ids + count, objArray.Begin(), classCount * sizeof(CK_ID));
            }

            count += classCount;
        }
    } else {
        // Direct class match
        XObjectArray &objArray = m_ObjectLists[cid];
        const int classCount = objArray.Size();

        if (obj_ids) {
            memcpy(obj_ids, objArray.Begin(), classCount * sizeof(CK_ID));
        }

        count = classCount;
    }

    return count;
}

int CKObjectManager::GetObjectsCount() {
    return m_ObjectsCount;
}

CKObject *CKObjectManager::GetObject(CK_ID id) {
    return m_Objects[id];
}

CKERROR CKObjectManager::DeleteAllObjects() {
    for (CK_CLASSID cid = 0; cid < g_MaxClassID; ++cid) {
        m_ObjectLists[cid].Clear();
    }

    if (m_Objects) {
        for (CK_ID id = 0; id < m_ObjectsCount; ++id) {
            CKObject *obj = m_Objects[id];
            if (obj)
                delete obj;
            m_Objects[id] = nullptr;
        }
    }

    m_ObjectsCount = 0;
    m_FreeObjectIDs.Clear();
    m_ObjectAppData.Clear();

    return CK_OK;
}

CKERROR CKObjectManager::ClearAllObjects() {
    // Bit array to track protected objects during cleanup
    XBitArray protectedObjects;
    protectedObjects.CheckSize(m_ObjectsCount);

    m_Context->m_InClearAll = TRUE;

    // Mark all objects for deletion
    for (CK_ID id = 0; id < m_ObjectsCount; ++id) {
        CKObject *obj = m_Objects[id];
        if (obj)
            obj->ModifyObjectFlags(CK_OBJECT_TOBEDELETED, 0);
    }

    // Protect render-related objects from immediate deletion
    if (m_Context->m_RenderManager) {
        const int rcCount = m_Context->m_RenderManager->GetRenderContextCount();
        for (int i = 0; i < rcCount; ++i) {
            CKRenderContext *dev = m_Context->m_RenderManager->GetRenderContext(i);
            if (!dev)
                continue;

            // Protect render context
            protectedObjects.Set(dev->GetID());
            dev->ModifyObjectFlags(0, CK_OBJECT_TOBEDELETED);

            // Protect viewpoint
            CK3dEntity *viewpoint = dev->GetViewpoint();
            if (viewpoint) {
                protectedObjects.Set(viewpoint->GetID());
                viewpoint->ModifyObjectFlags(0, CK_OBJECT_TOBEDELETED);
            }

            // Protect background material
            CKMaterial *bgMat = dev->GetBackgroundMaterial();
            if (bgMat) {
                bgMat->SetTexture0(nullptr);
                protectedObjects.Set(bgMat->GetID());
                bgMat->ModifyObjectFlags(0, CK_OBJECT_TOBEDELETED);
            }

            // Protect 2D roots
            CK2dEntity *bg2dRoot = dev->Get2dRoot(TRUE);
            if (bg2dRoot) {
                protectedObjects.Set(bg2dRoot->GetID());
                bg2dRoot->ModifyObjectFlags(0, CK_OBJECT_TOBEDELETED);
            }
            CK2dEntity *fg2dRoot = dev->Get2dRoot(FALSE);
            if (fg2dRoot) {
                protectedObjects.Set(fg2dRoot->GetID());
                fg2dRoot->ModifyObjectFlags(0, CK_OBJECT_TOBEDELETED);
            }
        }
    }

    // Destroy class lists and free arrays
    for (CK_CLASSID cid = 0; cid < g_MaxClassID; ++cid) {
        m_ObjectLists[cid].Clear();
    }
    m_FreeObjectIDs.Clear();
    m_DynamicObjects.Clear();

    // First pass: Delete non-protected behaviors
    for (CK_ID id = m_ObjectsCount; id > 0; --id) {
        if (protectedObjects.IsSet(id))
            continue;

        CKObject *obj = m_Objects[id];
        if (!obj)
            continue;

        if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
            CKBehavior *beh = (CKBehavior *) obj;
            beh->CallCallbackFunction(CKM_BEHAVIORDETACH);
            beh->~CKBehavior();
        }
    }

    // Second pass: Delete remaining non-protected objects
    for (CK_ID id = m_ObjectsCount; id > 0; --id) {
        if (protectedObjects.IsSet(id))
            continue;

        CKObject *obj = m_Objects[id];
        if (obj) {
            obj->~CKObject();
        }
        --m_ObjectsCount;
    }

    m_Context->m_InClearAll = FALSE;

    // Finalize protected objects
    for (CK_ID id = 0; id < m_ObjectsCount; ++id) {
        CKObject *obj = m_Objects[id];
        if (!obj)
            continue;

        // Render contexts need post-deletion
        CK_CLASSID cid = obj->GetClassID();
        if (cid == CKCID_RENDERCONTEXT)
            obj->CheckPostDeletion();

        // Re-add to class lists
        m_ObjectLists[cid].PushBack(id);
    }

    // Cleanup auxiliary data
    m_ObjectAppData.Clear();
    m_SingleObjectActivities.Clear();

    return CK_OK;
}

CKBOOL CKObjectManager::IsObjectSafe(CKObject *iObject) {
    if (!iObject)
        return FALSE;

    for (CK_ID id = 0; id < m_ObjectsCount; ++id) {
        CKObject *obj = m_Objects[id];
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

    XBitArray protectedClasses;
    for (int i = 0; i < Count; ++i) {
        CK_ID id = obj_ids[i];
        CKObject *obj = m_Objects[id];
        if (obj) {
            protectedClasses.Set(obj->GetClassID());
            obj->ModifyObjectFlags(flags, 0);
        }
    }

    const int classCount = m_ObjectLists.Size();
    for (CK_CLASSID classId = 0; classId < classCount; ++classId) {
        if (classId < protectedClasses.Size() && protectedClasses.IsSet(classId)) {
            XObjectArray &objects = m_ObjectLists[classId];
            for (int i = 0; i < objects.Size(); ++i) {
                const int newSize = CheckIDArrayPredeleted(objects.Begin(), objects.Size());
                objects.Resize(newSize);
            }
        }
    }

    if (!(Flags & CK_DESTROY_NONOTIFY)) {
        m_Context->ExecuteManagersOnSequenceToBeDeleted(obj_ids, Count);
        if (classCount > CKCID_OBJECT) {
            for (CK_CLASSID classId = classCount - 1; classId > 0; --classId) {
                if (protectedClasses.CheckCommon(g_CKClassInfo[classId].CommonToBeNotify)) {
                    XObjectArray &objects = m_ObjectLists[classId];
                    for (int i = 0; i < objects.Size(); ++i) {
                        CKObject *obj = m_Objects[objects[i]];
                        if (obj) {
                            obj->CheckPreDeletion();
                        }
                    }
                }
            }
        }
    }

    for (int i = Count; i > 0; --i) {
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
    if (!(Flags & CK_DESTROY_NONOTIFY) && classCount > CKCID_OBJECT) {
        for (CK_CLASSID classId = classCount - 1; classId > 0; --classId) {
            if (protectedClasses.CheckCommon(g_CKClassInfo[classId].CommonToBeNotify)) {
                XObjectArray &objects = m_ObjectLists[classId];
                for (int i = 0; i < objects.Size(); ++i) {
                    CKObject *obj = m_Objects[objects[i]];
                    if (obj) {
                        obj->CheckPostDeletion();
                    }
                }
            }
        }
    }

    if (!(Flags & CK_DESTROY_NONOTIFY)) {
        m_Context->ExecuteManagersOnSequenceDeleted(obj_ids, Count);
    }

    return CK_OK;
}

CKERROR CKObjectManager::GetRootEntities(XObjectPointerArray &array) {
    if (g_MaxClassID == 0)
        return CK_OK;

    for (CK_CLASSID cid = CKCID_OBJECT; cid < g_MaxClassID; ++cid) {
        if (CKIsChildClassOf(cid, CKCID_3DENTITY)) {
            XObjectArray &objects = m_ObjectLists[cid];
            for (CK_ID *it = objects.Begin(); it != objects.End(); ++it) {
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

    // Determine start position (after previous object)
    CK_ID startId = 1;
    if (previous) {
        startId = previous->GetID() + 1;
    }

    // Search through all objects sequentially
    for (CK_ID id = startId; id <= m_ObjectsCount; ++id) {
        CKObject* obj = m_Objects[id];
        if (!obj || !obj->GetName())
            continue;

        if (strcmp(obj->GetName(), name) == 0) {
            return obj;
        }
    }

    return nullptr;
}

CKObject *CKObjectManager::GetObjectByName(CKSTRING name, CK_CLASSID cid, CKObject *previous) {
    if (!name)
        return nullptr;

    // Get the object array for the requested class
    XObjectArray &objects = m_ObjectLists[cid];
    const int count = objects.Size();
    if (count == 0)
        return nullptr;

    // Find starting index (after previous object)
    int startIndex = -1;
    if (previous) {
        CK_ID prevId = previous->GetID();
        for (int i = 0; i < count; ++i) {
            if (objects[i] == prevId) {
                startIndex = i; // Found previous object
                break;
            }
        }
    }

    // Search from next position
    for (int i = startIndex + 1; i < count; ++i) {
        CK_ID objId = objects[i];
        CKObject *obj = m_Objects[objId];

        // Skip invalid objects or those without names
        if (!obj || !obj->GetName())
            continue;

        // Check for name match
        if (strcmp(obj->GetName(), name) == 0) {
            return obj;
        }
    }

    return nullptr;
}

CKObject *CKObjectManager::GetObjectByNameAndParentClass(CKSTRING name, CK_CLASSID pcid, CKObject *previous) {
    if (!name)
        return nullptr;

    // Find starting position (class and index)
    int startClassId = 0;
    int startIndex = -1;

    if (previous) {
        CK_ID prevId = previous->GetID();

        // Locate previous object in class hierarchy
        for (CK_CLASSID cid = CKCID_OBJECT; cid < g_MaxClassID; ++cid) {
            if (!CKIsChildClassOf(cid, pcid))
                continue;

            XObjectArray &objects = m_ObjectLists[cid];
            for (int i = 0; i < objects.Size(); ++i) {
                if (objects[i] == prevId) {
                    startClassId = cid;
                    startIndex = i;
                    break;
                }
            }
            if (startIndex != -1)
                break;
        }
    }

    // Search through derived classes
    for (CK_CLASSID cid = startClassId; cid < g_MaxClassID; ++cid) {
        if (!CKIsChildClassOf(cid, pcid))
            continue;

        XObjectArray &objects = m_ObjectLists[cid];
        const int count = objects.Size();

        // Determine start index for current class
        int start = 0;
        if (cid == startClassId) {
            start = startIndex + 1;
            if (start >= count)
                continue;
        }

        // Search within class
        for (int i = start; i < count; ++i) {
            CKObject *obj = m_Objects[objects[i]];
            if (!obj || !obj->GetName())
                continue;

            if (strcmp(obj->GetName(), name) == 0) {
                return obj;
            }
        }
    }

    return nullptr;
}

CKERROR CKObjectManager::GetObjectListByType(CK_CLASSID cid, XObjectPointerArray &array, CKBOOL derived) {
    if (derived) {
        // Iterate through all classes
        for (CK_CLASSID classId = 0; classId < g_MaxClassID; ++classId) {
            // Skip non-derived classes
            if (!CKIsChildClassOf(classId, cid))
                continue;

            // Add all objects in this class
            XObjectArray &objects = m_ObjectLists[classId];
            for (int i = 0; i < objects.Size(); ++i) {
                CKObject *obj = m_Objects[objects[i]];
                if (obj) {
                    array.PushBack(obj);
                }
            }
        }
    } else {
        // Direct class match
        XObjectArray &objects = m_ObjectLists[cid];
        for (int i = 0; i < objects.Size(); ++i) {
            CKObject *obj = m_Objects[objects[i]];
            if (obj) {
                array.PushBack(obj);
            }
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
        m_LoadSession[ObjectID] = iObject ? iObject->GetID() : 0;
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
    int validCount = 0;
    int currentIndex = 0;
    CKObject **objects = m_Objects; // Local copy for efficiency

    // First segment: consecutive valid objects
    while (currentIndex < Count) {
        CK_ID id = obj_ids[currentIndex];
        if (!objects[id]) break; // Stop at first invalid object

        validCount++;
        currentIndex++;
    }

    // Second segment: process remaining objects after first invalid
    int remaining = Count - currentIndex - 1;
    if (remaining > 0) {
        CK_ID *remainingIds = &obj_ids[currentIndex + 1];

        for (int i = 0; i < remaining; i++) {
            CK_ID id = remainingIds[i];
            if (objects[id]) {
                obj_ids[validCount++] = id;
            }
        }
    }

    return validCount;
}

int CKObjectManager::CheckIDArrayPredeleted(CK_ID *obj_ids, int Count) {
    int validCount = 0;
    int currentIndex = 0;

    // First segment: consecutive valid, non-predeleted objects
    while (currentIndex < Count) {
        CK_ID id = obj_ids[currentIndex];
        CKObject *obj = m_Objects[id];

        // Stop at first invalid or predeleted object
        if (!obj || obj->IsToBeDeleted())
            break;

        validCount++;
        currentIndex++;
    }

    // Second segment: process remaining objects
    int remaining = Count - currentIndex - 1;
    if (remaining > 0) {
        CK_ID *remainingIds = &obj_ids[currentIndex + 1];

        for (int i = 0; i < remaining; i++) {
            CK_ID id = remainingIds[i];
            CKObject *obj = m_Objects[id];

            if (obj && !obj->IsToBeDeleted()) {
                obj_ids[validCount++] = id;
            }
        }
    }

    return validCount;
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
    // Process deferred deletions
    for (CKDWORD flag = 0; flag < 4; ++flag) {
        XArray<CKDeferredDeletion *> &deletions = m_DeferredDeletions[flag];

        if (!deletions.IsEmpty()) {
            for (int i = 0; i < deletions.Size(); ++i) {
                CKDeferredDeletion *deletion = deletions[i];
                if (deletion) {
                    m_Context->PrepareDestroyObjects(deletion->m_Dependencies.Begin(),
                                                     deletion->m_Dependencies.Size(),
                                                     flag,
                                                     deletion->m_DependenciesPtr);
                }
            }

            m_Context->FinishDestroyObjects(flag);

            for (int i = 0; i < deletions.Size(); ++i) {
                CKDeferredDeletion *deletion = deletions[i];
                if (deletion) {
                    delete deletion->m_DependenciesPtr;
                    delete deletion;
                }
            }
            deletions.Clear();
        }
    }

    // Process deletion of all dynamic objects if requested
    if (m_NeedDeleteAllDynamicObjects) {
        if (!m_DynamicObjects.IsEmpty()) {
            m_Context->DestroyObjects(m_DynamicObjects.Begin(), m_DynamicObjects.Size());
            m_DynamicObjects.Clear();
        }
        m_NeedDeleteAllDynamicObjects = FALSE;
    }

    return CK_OK;
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
}

CKObjectManager::CKObjectManager(CKContext *Context) : CKBaseManager(Context, OBJECT_MANAGER_GUID, "Object Manager") {
    m_MaxObjectID = 0;
    m_ObjectsCount = 0;
    m_Count = 1000;
    m_Objects = new CKObject *[1000];
    memset(m_Objects, 0, m_Count * sizeof(CKObject *));
    m_LoadSession = nullptr;
    m_InLoadSession = FALSE;
    m_NeedDeleteAllDynamicObjects = FALSE;
    m_ObjectLists.Resize(g_MaxClassID);
    m_Context->RegisterNewManager(this);
}

CKDWORD CKObjectManager::GetGroupGlobalIndex() {
    int index = m_GroupGlobalIndex.GetUnsetBitPosition(0);
    m_GroupGlobalIndex.Set(index);
    return index;
}

void CKObjectManager::ReleaseGroupGlobalIndex(CKDWORD index) {
    m_GroupGlobalIndex.Unset(index);
}

int CKObjectManager::GetSceneGlobalIndex() {
    int index = m_SceneGlobalIndex.GetUnsetBitPosition(0);
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
