#include "CKLevel.h"

#include "CKFile.h"
#include "CKScene.h"
#include "CKPlace.h"
#include "CKCamera.h"
#include "CKBehavior.h"
#include "CKObjectArray.h"
#include "CKRenderContext.h"
#include "CKRenderManager.h"
#include "CKAttributeManager.h"

CK_CLASSID CKLevel::m_ClassID = CKCID_LEVEL;

CKERROR CKLevel::AddObject(CKObject *obj) {
    if (!CKIsChildClassOf(obj, CKCID_SCENEOBJECT) || CKIsChildClassOf(obj, CKCID_LEVEL))
        return CKERR_INVALIDPARAMETER;
    GetLevelScene()->AddObjectToScene((CKSceneObject *)obj);
    return CK_OK;
}

CKERROR CKLevel::RemoveObject(CKObject *obj) {
    if (!CKIsChildClassOf(obj, CKCID_SCENEOBJECT))
        return CKERR_INVALIDPARAMETER;
    GetLevelScene()->RemoveObjectFromScene((CKSceneObject *)obj);
    return CK_OK;
}

CKERROR CKLevel::RemoveObject(CK_ID objID) {
    return RemoveObject(m_Context->GetObject(objID));
}

void CKLevel::BeginAddSequence(CKBOOL Begin) {
    GetLevelScene()->BeginAddSequence(Begin);
}

void CKLevel::BeginRemoveSequence(CKBOOL Begin) {
    GetLevelScene()->BeginRemoveSequence(Begin);
}

const XObjectPointerArray &CKLevel::ComputeObjectList(CK_CLASSID cid, CKBOOL derived) {
    if (CKIsChildClassOf(cid, CKCID_SCENE))
        return m_SceneList;
    return GetLevelScene()->ComputeObjectList(cid, derived);
}

CKERROR CKLevel::AddPlace(CKPlace *pl) {
    if (!pl)
        return CKERR_INVALIDPARAMETER;
    pl->AddToScene(GetLevelScene());
    return CK_OK;
}

CKERROR CKLevel::RemovePlace(CKPlace *pl) {
    if (!pl)
        return CKERR_INVALIDPARAMETER;
    pl->RemoveFromScene(GetLevelScene());
    return CK_OK;
}

CKPlace *CKLevel::RemovePlace(int pos) {
    CKPlace *pl = GetPlace(pos);
    pl->RemoveFromScene(GetLevelScene());
    return pl;
}

CKPlace *CKLevel::GetPlace(int pos) {
    CKObjectArray *array = CreateCKObjectArray();
    GetLevelScene()->ComputeObjectList(array, CKCID_PLACE);
    CK_ID id = array->PositionFind(pos);
    CKPlace *pl = (CKPlace *)m_Context->GetObject(id);
    DeleteCKObjectArray(array);
    return pl;
}

int CKLevel::GetPlaceCount() {
    CKObjectArray *array = CreateCKObjectArray();
    GetLevelScene()->ComputeObjectList(array, CKCID_PLACE);
    int count = array->GetCount();
    DeleteCKObjectArray(array);
    return count;
}

CKERROR CKLevel::AddScene(CKScene *scn) {
    if (!scn)
        return CKERR_INVALIDPARAMETER;
    scn->AddObjectToScene(this);
    for (auto it = m_SceneList.Begin(); it != m_SceneList.End(); ++it) {
        if (*it == scn)
            return CK_OK;
    }
    m_SceneList.PushBack(scn);
    if (m_Context->m_UICallBackFct) {
        CKUICallbackStruct cbs;
        cbs.Reason = CKUIM_SCENEADDEDTOLEVEL;
        cbs.Param1 = scn->GetID();
        m_Context->m_UICallBackFct(cbs, m_Context->m_InterfaceModeData);
    }
    return CK_OK;
}

CKERROR CKLevel::RemoveScene(CKScene *scn) {
    if (!scn)
        return CKERR_INVALIDPARAMETER;
    if (!m_SceneList.Remove(scn))
        return CKERR_NOTFOUND;
    scn->SetLevel(nullptr);
    scn->RemoveObjectFromScene(this);
    return CK_OK;
}

CKScene *CKLevel::RemoveScene(int pos) {
    if (pos >= 0 && pos < m_SceneList.Size()) {
        CKScene *scene = (CKScene *)m_SceneList[pos];
        m_SceneList.RemoveAt(pos);
        return scene;
    }
    return nullptr;
}

CKScene *CKLevel::GetScene(int pos) {
    if (pos >= 0 && pos < m_SceneList.Size())
        return (CKScene *)m_SceneList[pos];
    return nullptr;
}

int CKLevel::GetSceneCount() {
    return m_SceneList.Size();
}

CKERROR CKLevel::SetNextActiveScene(CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS Active,
                                    CK_SCENEOBJECTRESET_FLAGS Reset) {
    m_NextActiveScene = scene ? scene->GetID() : 0;
    m_NextSceneActivityFlags = Active;
    m_NextSceneResetFlags = Reset;
    return CK_OK;
}

CKERROR CKLevel::LaunchScene(CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS Active, CK_SCENEOBJECTRESET_FLAGS Reset) {
    CKScene *sceneToLaunch = scene ? scene : (CKScene *)m_Context->GetObject(m_DefaultScene);
    if (!sceneToLaunch)
        return CKERR_INVALIDPARAMETER;

    CKRenderContext *renderContext = GetRenderContext(0);
    CKCamera *previousCamera = renderContext ? renderContext->GetAttachedCamera() : nullptr;
    CKScene *previousScene = GetCurrentScene();

    m_Context->ExecuteManagersPreLaunchScene(previousScene, sceneToLaunch);
    m_Context->m_BehaviorManager->RemoveAllObjects();

    if (previousScene)
        previousScene->Stop(m_RenderContextList, previousScene != sceneToLaunch);

    m_NextActiveScene = 0;
    m_CurrentScene = sceneToLaunch ? sceneToLaunch->m_ID : 0;

    if (GetScriptCount() > 0)
        m_Context->m_BehaviorManager->AddObject(this);

    if (sceneToLaunch)
        sceneToLaunch->Init(m_RenderContextList, Active, Reset);

    m_Context->m_AttributeManager->NewActiveScene(sceneToLaunch);

    if (m_Context->m_RenderManager)
        m_Context->m_RenderManager->FlushTextures();

    m_Context->m_BehaviorContext.CurrentScene = sceneToLaunch;
    m_Context->m_BehaviorContext.PreviousScene = previousScene;

    m_Context->WarnAllBehaviors(CKM_BEHAVIORNEWSCENE);

    if (renderContext && !renderContext->GetAttachedCamera() && sceneToLaunch) {
        CKCamera *startingCamera = sceneToLaunch->GetStartingCamera();
        if (startingCamera) {
            renderContext->AttachViewpointToCamera(startingCamera);
        } else if (previousCamera && previousCamera->IsInScene(sceneToLaunch)) {
            renderContext->AttachViewpointToCamera(previousCamera);
        }
    }

    m_Context->ExecuteManagersPostLaunchScene(previousScene, sceneToLaunch);

    if (sceneToLaunch->EnvironmentSettings())
        sceneToLaunch->ApplyEnvironmentSettings(&m_RenderContextList);

    return CK_OK;
}

CKScene *CKLevel::GetCurrentScene() {
    return (CKScene *)m_Context->GetObject(m_CurrentScene);
}

void CKLevel::AddRenderContext(CKRenderContext *dev, CKBOOL Main) {
    if (!dev)
        return;

    for (int i = 0; i < m_RenderContextList.Size(); ++i) {
        if (m_RenderContextList[i] == dev)
            return;
    }

    if (Main) {
        m_RenderContextList.PushFront(dev);
        dev->ChangeCurrentRenderOptions(CK_RENDER_PLAYERCONTEXT, 0);
        m_Context->m_BehaviorContext.CurrentRenderContext = dev;
    } else {
        dev->ChangeCurrentRenderOptions(0, CK_RENDER_PLAYERCONTEXT);
        m_RenderContextList.PushBack(dev);
    }

    CKScene *scene = GetCurrentScene();
    if (!scene)
        scene = GetLevelScene();
    CKSceneObjectIterator it = scene->GetObjectIterator();
    while (!it.End()) {
        CKSceneObject *obj = (CKSceneObject *)m_Context->GetObject(it.GetObjectID());
        if (CKIsChildClassOf(obj, CKCID_RENDEROBJECT))
            dev->AddObject((CKRenderObject *)obj);
        it++;
    }
    if (scene->EnvironmentSettings()) {
        scene->ApplyEnvironmentSettings(&m_RenderContextList);
    }
}

void CKLevel::RemoveRenderContext(CKRenderContext *dev) {
    m_RenderContextList.Remove(dev);
    dev->ChangeCurrentRenderOptions(0, CK_RENDER_PLAYERCONTEXT);
    if (m_Context->m_BehaviorContext.CurrentRenderContext == dev) {
        if (m_RenderContextList.Size() > 0) {
            m_Context->m_BehaviorContext.CurrentRenderContext = (CKRenderContext *)m_RenderContextList[0];
        } else {
            m_Context->m_BehaviorContext.CurrentRenderContext = nullptr;
        }
    }
}

int CKLevel::GetRenderContextCount() {
    return m_RenderContextList.Size();
}

CKRenderContext *CKLevel::GetRenderContext(int count) {
    if (count >= m_RenderContextList.Size())
        return nullptr;
    return (CKRenderContext *)m_RenderContextList[count];
}

CKScene *CKLevel::GetLevelScene() {
    return (CKScene *)m_Context->GetObject(m_DefaultScene);
}

CKERROR CKLevel::Merge(CKLevel *mergedLevel, CKBOOL asScene) {
    if (!mergedLevel)
        return CKERR_INVALIDPARAMETER;

    CKScene *scene = GetLevelScene();
    if (asScene) {
        CKMoveAllScripts(mergedLevel, scene);
        CKCopyAllAttributes(mergedLevel, scene);

        if (!scene->m_Name)
            scene->SetName("Merged Scene");

        AddScene(scene);
        mergedLevel->m_DefaultScene = 0;
        CKRemapObjectParameterValue(m_Context, mergedLevel->m_ID, m_ID, CKCID_LEVEL, TRUE);
        CKRemapObjectParameterValue(m_Context, mergedLevel->m_ID, scene->m_ID, CKCID_OBJECT, TRUE);
    } else {
        CKMoveAllScripts(mergedLevel, this);
        CKScene *mergedScene = mergedLevel->GetLevelScene();
        CKScene *currentScene = GetLevelScene();
        currentScene->Merge(mergedScene, nullptr);
        CKCopyAllAttributes(mergedLevel, this);
        CKRemapObjectParameterValue(m_Context, mergedLevel->m_ID, m_ID, CKCID_OBJECT, TRUE);
    }

    for (auto it = mergedLevel->m_SceneList.Begin(); it != mergedLevel->m_SceneList.End(); ++it) {
        AddScene((CKScene *)*it);
    }

    mergedLevel->m_SceneList.Clear();
    return CK_OK;
}

void CKLevel::ApplyPatchForOlderVersion(int NbObject, CKFileObject *FileObjects) {
    int scriptCount = GetScriptCount();
    for (int i = 0; i < scriptCount; ++i) {
        CKBehavior *script = GetScript(i);
        if (!script)
            continue;

        int sceneInCount = GetSceneInCount();
        for (int s = 0; s < sceneInCount; ++s) {
            CKScene *scene = GetSceneIn(s);
            if (!scene->GetSceneObjectDesc(script)) {
                scene->AddObjectDesc(script);
            }
        }
    }

    CKScene *scene = GetLevelScene();
    XObjectArray scriptsToRemove;
    CKSceneObjectIterator it = scene->GetObjectIterator();
    while (!it.End()) {
        CKSceneObject *obj = (CKSceneObject *)m_Context->GetObject(it.GetObjectID());
        if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
            CKBehavior *beh = (CKBehavior *)obj;
            CKBeObject *owner = beh->GetOwner();
            if (owner && beh->GetFlags() & CKBEHAVIOR_SCRIPT) {
                bool found = false;
                for (int i = 0; i < owner->GetScriptCount(); ++i) {
                    if (owner->GetScript(i) == beh) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    scriptsToRemove.PushBack(beh->GetID());
                }
            } else {
                scriptsToRemove.PushBack(beh->GetID());
            }
        }
    }

    if (scriptsToRemove.Size() > 0) {
        m_Context->DestroyObjects(scriptsToRemove.Begin(), scriptsToRemove.Size());
    }
}

CKLevel::CKLevel(CKContext *Context, CKSTRING name) : CKBeObject(Context, name) {
    m_CurrentScene = 0;
    m_DefaultScene = 0;
    m_Priority = 32000;
    m_NextActiveScene = 0;
    m_IsReseted = 0;
    CreateLevelScene();
}

CKLevel::~CKLevel() {
    if (m_ID == m_Context->m_CurrentLevel)
        m_Context->m_CurrentLevel = 0;
}

CK_CLASSID CKLevel::GetClassID() {
    return m_ClassID;
}

void CKLevel::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
    file->SaveObjects(m_SceneList.Begin(), m_SceneList.Size());
    GetLevelScene()->PreSave(file, flags);
}

CKStateChunk *CKLevel::Save(CKFile *file, CKDWORD flags) {
    CKStateChunk *baseChunk = CKBeObject::Save(file, flags);
    if (!file)
        return baseChunk;

    CKStateChunk *chunk = new CKStateChunk(CKCID_LEVEL, file);

    chunk->StartWrite();
    chunk->AddChunkAndDelete(baseChunk);

    chunk->WriteIdentifier(CK_STATESAVE_LEVELDEFAULTDATA);
    chunk->WriteObjectArray(nullptr, nullptr);

    XObjectPointerArray objects;
    objects.Save(chunk);
    m_SceneList.Save(chunk);

    chunk->WriteIdentifier(CK_STATESAVE_LEVELSCENE);

    CKObject *currentScene = GetCurrentScene();
    chunk->WriteObject(currentScene);

    CKObject *defaultScene = GetLevelScene();
    CKStateChunk *defaultSceneChunk = nullptr;
    if (defaultScene)
        defaultSceneChunk = defaultScene->Save(file, -1);
    chunk->WriteObject(defaultScene);
    chunk->WriteSubChunk(defaultSceneChunk);
    if (defaultSceneChunk) {
        delete defaultSceneChunk;
    }

    const int inactiveManagerCount = m_Context->GetInactiveManagerCount();
    if (inactiveManagerCount > 0) {
        chunk->WriteIdentifier(CK_STATESAVE_LEVELINACTIVEMAN);
        for (int i = 0; i < inactiveManagerCount; ++i) {
            CKBaseManager *inactiveManager = m_Context->GetInactiveManager(i);
            if (!m_Context->GetManagerByGuid(inactiveManager->m_ManagerGuid))
                chunk->WriteGuid(inactiveManager->m_ManagerGuid);
        }

        chunk->WriteIdentifier(CK_STATESAVE_LEVELDUPLICATEMAN);
        int managerCount = m_Context->GetManagerCount();
        for (int i = 0; i < managerCount; ++i) {
            CKBaseManager *manager = m_Context->GetManager(i);
            if (m_Context->HasManagerDuplicates(manager))
                chunk->WriteString(manager->m_ManagerName);
            chunk->WriteString(nullptr);
        }
    }

    chunk->CloseChunk();
    return chunk;
}

CKERROR CKLevel::Load(CKStateChunk *chunk, CKFile *file) {
    if (file)
        return CK_OK;
    if (!chunk)
        return CKERR_INVALIDPARAMETER;

    CKBeObject::Load(chunk, file);

    if (chunk->SeekIdentifier(CK_STATESAVE_LEVELDEFAULTDATA)) {
        m_SceneList.Clear();

        CKObjectArray *objectArray = chunk->ReadObjectArray();
        if (objectArray)
            DeleteCKObjectArray(objectArray);

        objectArray = chunk->ReadObjectArray();
        if (objectArray)
            DeleteCKObjectArray(objectArray);

        CKObjectArray *sceneArray = chunk->ReadObjectArray();
        if (sceneArray) {
            sceneArray->Reset();
            while (!sceneArray->EndOfList()) {
                CKScene *scene = (CKScene *)sceneArray->GetData(m_Context);
                if (scene)
                    m_SceneList.AddIfNotHere(scene);
                sceneArray->Next();
            }
            DeleteCKObjectArray(sceneArray);
        }
    }

    if (chunk->SeekIdentifier(CK_STATESAVE_LEVELSCENE)) {
        m_CurrentScene = chunk->ReadObjectID();
        chunk->ReadObjectID();

        CKScene *levelScene = GetLevelScene();
        if (!levelScene)
            CreateLevelScene();

        CKStateChunk *subChunk = chunk->ReadSubChunk();
        if (subChunk) {
            levelScene->Load(subChunk, file);
            delete subChunk;
        }
    }

    const int inactiveManagerSize = chunk->SeekIdentifierAndReturnSize(CK_STATESAVE_LEVELINACTIVEMAN);
    if (inactiveManagerSize != -1) {
        const int inactiveManagerCount = inactiveManagerSize / (int)sizeof(CKGUID);
        for (int i = inactiveManagerCount; i >= 0; --i) {
            CKGUID managerGuid = chunk->ReadGuid();
            CKBaseManager *manager = m_Context->GetManagerByGuid(managerGuid);
            m_Context->ActivateManager(manager, FALSE);
        }

        if (chunk->SeekIdentifier(CK_STATESAVE_LEVELDUPLICATEMAN)) {
            char *managerName = nullptr;
            while (true) {
                chunk->ReadString(&managerName);
                if (!managerName)
                    break;
                CKBaseManager *manager = m_Context->GetManagerByName(managerName);
                m_Context->ActivateManager(manager, TRUE);
                delete[] managerName;
            }
        }
    }
    return CK_OK;
}

void CKLevel::CheckPreDeletion() {
    CKObject::CheckPreDeletion();
    m_SceneList.Check();
    m_RenderContextList.Check();
}

void CKLevel::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
    if (!m_Context->GetObject(m_DefaultScene))
        m_DefaultScene = 0;
    if (!m_Context->GetObject(m_CurrentScene))
        m_CurrentScene = 0;
    if (!m_Context->GetObject(m_NextActiveScene))
        m_NextActiveScene = 0;
}

int CKLevel::GetMemoryOccupation() {
    int size = CKBeObject::GetMemoryOccupation() + 48 + 4;
    size += m_RenderContextList.GetMemoryOccupation();
    size += m_SceneList.GetMemoryOccupation();
    return size;
}

int CKLevel::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    if (o->GetClassID() == CKCID_RENDERCONTEXT) {
        for (int i = 0; i < m_RenderContextList.Size(); ++i) {
            if (m_RenderContextList[i] == o)
                return TRUE;
        }
    } else if (o->GetClassID() == CKCID_SCENE) {
        for (int i = 0; i < m_SceneList.Size(); ++i) {
            if (m_SceneList[i] == o)
                return TRUE;
        }
    }
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKLevel::PrepareDependencies(CKDependenciesContext &context) {
    CKERROR err = CKBeObject::PrepareDependencies(context);
    if (err != CK_OK)
        return err;

    if (!context.IsInMode(CK_DEPENDENCIES_BUILD)) {
        context.GetClassDependencies(m_ClassID);
        m_SceneList.Prepare(context);

        if (!context.IsInMode(CK_DEPENDENCIES_SAVE)) {
            m_RenderContextList.Prepare(context);
        }

        CKScene *defaultScene = GetLevelScene();
        if (defaultScene) {
            defaultScene->PrepareDependencies(context);
        }
    }

    return context.FinishPrepareDependencies(this, m_ClassID);
}

CKSTRING CKLevel::GetClassName() {
    return "Level";
}

int CKLevel::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKLevel::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKLevel::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKScene::m_ClassID);
    CKClassNeedNotificationFrom(m_ClassID, CKCID_PLACE);
    CKClassNeedNotificationFrom(m_ClassID, CKCID_RENDERCONTEXT);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_LEVEL);
    CKClassRegisterDefaultOptions(m_ClassID, 1);
}

CKLevel *CKLevel::CreateInstance(CKContext *Context) {
    return new CKLevel(Context);
}

void CKLevel::AddToScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::AddToScene(scene, dependencies);
}

void CKLevel::RemoveFromScene(CKScene *scene, CKBOOL dependencies) {
    CKBeObject::RemoveFromScene(scene, dependencies);
}

void CKLevel::Reset() {
    m_IsReseted = TRUE;
}

void CKLevel::ActivateAllScript() {
    CKScene *levelScene = GetLevelScene();
    if (!levelScene)
        return;

    CKSceneObjectIterator oit = levelScene->GetObjectIterator();
    while (!oit.End()) {
        CKObject *obj = m_Context->GetObject(oit.GetObjectID());
        if (CKIsChildClassOf(obj, CKCID_BEOBJECT)) {
            CKBeObject *beo = (CKBeObject *)obj;
            const int scriptCount = beo->GetScriptCount();
            for (int i = 0; i < scriptCount; ++i) {
                CKBehavior *script = beo->GetScript(i);
                if (script && script->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    script->Activate(TRUE, TRUE);
                }
            }
        }
    }

    // 2. Activate scripts in all sub-scenes
    for (auto it = m_SceneList.Begin(); it != m_SceneList.End(); ++it) {
        CKScene *scene = (CKScene *)*it;
        if (scene) {
            const int scriptCount = scene->GetScriptCount();
            for (int i = 0; i < scriptCount; ++i) {
                CKBehavior *script = scene->GetScript(i);
                if (script && script->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    script->Activate(TRUE, TRUE);
                }
            }
        }
    }

    if (m_ScriptArray) {
        XObjectPointerArray &levelScripts = *m_ScriptArray;
        for (XObjectPointerArray::Iterator it = levelScripts.Begin(); it != levelScripts.End(); ++it) {
            CKBehavior *behavior = (CKBehavior *)*it;
            if (behavior && behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                behavior->Activate(true, true);
            }
        }
    }

    m_IsReseted = FALSE;
    LaunchScene(GetCurrentScene(), CK_SCENEOBJECTACTIVITY_SCENEDEFAULT, CK_SCENEOBJECTRESET_RESET);
}

void CKLevel::CreateLevelScene() {
    if (!m_Context->GetObject(m_DefaultScene)) {
        CKScene *scene = (CKScene *)m_Context->CreateObject(
            CKCID_SCENE, m_Name, CK_OBJECTCREATION_Dynamic(m_Context->IsInDynamicCreationMode()), nullptr);
        if (scene) {
            scene->SetLevel(this);
            scene->AddObjectToScene(this, TRUE);
            m_DefaultScene = scene->GetID();
            m_CurrentScene = m_DefaultScene;
        }
    }
}

void CKLevel::PreProcess() {
    CKScene *scene = (CKScene *)m_Context->GetObject(m_NextActiveScene);
    if (scene)
        LaunchScene(scene, m_NextSceneActivityFlags, m_NextSceneResetFlags);
}
