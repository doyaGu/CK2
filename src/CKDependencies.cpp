#include "CKDependencies.h"

#include "CKContext.h"
#include "CKObject.h"
#include "CKBehavior.h"
#include "CKLevel.h"
#include "CKScene.h"

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
    XHashItID it = m_MapID.Find(id);
    id = (*it) ? (*it) : 0;
    return m_CKContext->GetObject(id);
}

XObjectArray CKDependenciesContext::FillDependencies() {
    XObjectArray objects;
    objects.Reserve(m_MapID.Size());
    for (XHashID::Iterator it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
        objects.PushBack(it.GetKey());
    }
    return objects;
}

XObjectArray CKDependenciesContext::FillRemappedDependencies() {
    XObjectArray objects;
    objects.Reserve(m_MapID.Size());
    for (XHashID::Iterator it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
        objects.PushBack(*it);
    }
    return objects;
}

CKDWORD CKDependenciesContext::GetClassDependencies(int c) {
    if (m_Dependencies) {
        if (m_Dependencies->m_Flags & CK_DEPENDENCIES_NONE)
            return 0;
        if (m_Dependencies->m_Flags & CK_DEPENDENCIES_FULL)
            return 0xFFFF;
        return (*m_Dependencies)[c];
    }

    CKDependencies *deps = CKGetDefaultClassDependencies((CK_DEPENDENCIES_OPMODE)m_Mode);
    if (!deps)
        return 0;
    return (*deps)[c];
}

void CKDependenciesContext::Copy(CKSTRING appendstring) {
    m_CopyAppendString = appendstring;
    m_Mode = CK_DEPENDENCIES_COPY;

    m_Objects.Prepare(*this);
    m_ObjectsClassMask.Clear();
    m_Scripts.Clear();

    for (XHashItID it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
        CK_ID originalId = it.GetKey();
        CKObject *original = m_CKContext->GetObject(originalId);

        m_ObjectsClassMask.Set(original->GetClassID());

        if (CKIsChildClassOf(original, CKCID_BEHAVIOR)) {
            CKBehavior *beh = (CKBehavior *)original;
            if (beh->GetType() & CKBEHAVIOR_SCRIPT) {
                m_Scripts.PushBack(beh->GetID());
            }
        }
    }

    m_CKContext->ExecuteManagersOnPreCopy(this);

    for (XHashItID it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
        CK_ID originalId = it.GetKey();
        CKObject *original = m_CKContext->GetObject(originalId);
        CKObject *copy = m_CKContext->CreateObject(original->GetClassID(), nullptr);

        *it = copy->GetID();

        copy->Copy(*original, *this);
    }

    for (XHashItID it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
        CKObject *copy = m_CKContext->GetObject(*it);
        if (copy) {
            copy->RemapDependencies(*this);
        }
    }

    CKLevel *currentLevel = m_CKContext->GetCurrentLevel();
    if (currentLevel) {
        CKScene *levelScene = currentLevel->GetLevelScene();

        int count = m_CKContext->m_ObjectManager->GetObjectsCountByClassID(CKCID_SCENE);
        CK_ID *ids = m_CKContext->m_ObjectManager->GetObjectsListByClassID(CKCID_SCENE);

        for (int i = 0; i < count; ++i) {
            CKScene *scene = (CKScene *)m_CKContext->GetObject(ids[i]);
            if (scene == levelScene || m_CreationMode & CK_OBJECTCREATION_ACTIVATE) {
                scene->BeginAddSequence(TRUE);
                for (XHashItID it = m_MapID.Begin(); it != m_MapID.End(); ++it) {
                    CKObject *original = m_CKContext->GetObject(it.GetKey());
                    CKObject *copy = m_CKContext->GetObject(*it);
                    if (CKIsChildClassOf(copy, CKCID_SCENE)) {
                        currentLevel->AddToScene((CKScene *)copy, TRUE);
                    } else if (CKIsChildClassOf(copy, CKCID_SCENEOBJECT)) {
                        CKSceneObject *sceneObjOrig = (CKSceneObject *)original;
                        CKSceneObject *sceneObjCopy = (CKSceneObject *)copy;
                        if (sceneObjOrig->IsInScene(scene)) {
                            scene->AddObject(sceneObjCopy);
                            CKDWORD origFlags = scene->GetObjectFlags(sceneObjOrig);
                            scene->SetObjectFlags(sceneObjCopy, (CK_SCENEOBJECT_FLAGS)(origFlags & ~CK_SCENEOBJECT_ACTIVE));
                            if (m_CreationMode & CK_OBJECTCREATION_ACTIVATE && CKIsChildClassOf(
                                sceneObjCopy, CKCID_BEOBJECT) || sceneObjOrig->IsActiveInScene(scene)) {
                                scene->Activate(sceneObjCopy, TRUE);
                            }
                        }
                    }
                }
                scene->BeginAddSequence(FALSE);
            }
        }
    }

    m_Scripts.Remap(*this);

    m_CKContext->ExecuteManagersOnPostCopy(this);

    m_Objects.Remap(*this);
}

CKBOOL CKDependenciesContext::ContainClassID(CK_CLASSID Cid) {
    return m_ObjectsClassMask.IsSet(Cid);
}

CKBOOL CKDependenciesContext::AddDependencies(CK_ID id) {
    return m_MapID.Insert(id, 0, FALSE);
}

void CKDependenciesContext::Clear() {
    m_DynamicObjects.Clear();
    m_MapID.Clear();
    m_Objects.Clear();
    m_Scripts.Clear();
    m_ObjectsClassMask.Clear();
}

CKERROR CKDependenciesContext::FinishPrepareDependencies(CKObject *iMySelf, CK_CLASSID Cid) {
    if (iMySelf->GetClassID() == Cid && m_Dependencies) {
        CKObject *obj = m_DynamicObjects.Back();
        if (obj)
            m_Dependencies->Remove(obj->GetID());
    }
    return CK_OK;
}
