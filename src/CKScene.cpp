#include "CKScene.h"

#include "CKStateChunk.h"
#include "CKLevel.h"
#include "CKBehavior.h"
#include "CKRenderContext.h"
#include "CKTexture.h"
#include "CKMaterial.h"
#include "CKCamera.h"

CK_CLASSID CKScene::m_ClassID = CKCID_SCENE;

void CKScene::AddObjectToScene(CKSceneObject *o, CKBOOL dependencies) {
    if (!o)
        return;
    if (CKIsChildClassOf(o, CKCID_BEOBJECT)) {
        CKBeObject *beo = (CKBeObject *)o;
        beo->AddToScene(this, dependencies);
    } else {
        AddObject(o);
    }
}

void CKScene::RemoveObjectFromScene(CKSceneObject *o, CKBOOL dependencies) {
    if (!o)
        return;
    if (CKIsChildClassOf(o, CKCID_BEOBJECT)) {
        CKBeObject *beo = (CKBeObject *)o;
        beo->RemoveFromScene(this, dependencies);
    } else {
        RemoveObject(o);
    }
}

CKBOOL CKScene::IsObjectHere(CKObject *o) {
    if (!o)
        return FALSE;
    if (CKIsChildClassOf(o, CKCID_BEOBJECT)) {
        CKBeObject *beo = (CKBeObject *)o;
        return beo->IsInScene(this);
    }
    return FALSE;
}

void CKScene::BeginAddSequence(CKBOOL Begin) {
    if (Begin) {
        if (m_AddObjectCount == 0) {
            m_AddObjectList.Resize(0);
        }
        ++m_AddObjectCount;
    } else if (--m_AddObjectCount <= 0) {
        const int objectCount = m_AddObjectList.Size();
        if (objectCount > 0) {
            m_Context->ExecuteManagersOnSequenceAddedToScene(this, m_AddObjectList.Begin(), objectCount);
        }

        m_AddObjectList.Resize(0);
        m_AddObjectCount = 0;
    }
}

void CKScene::BeginRemoveSequence(CKBOOL Begin) {
    if (Begin) {
        if (m_RemoveObjectCount == 0) {
            m_RemoveObjectList.Resize(0);
        }
        ++m_RemoveObjectCount;
    } else if (--m_RemoveObjectCount <= 0) {
        const int objectCount = m_RemoveObjectList.Size();
        if (objectCount > 0) {
            m_Context->ExecuteManagersOnSequenceRemovedFromScene(this, m_RemoveObjectList.Begin(), objectCount);
        }

        m_RemoveObjectList.Resize(0);
        m_RemoveObjectCount = 0;
    }
}

int CKScene::GetObjectCount() {
    return m_SceneObjects.Size();
}

const XObjectPointerArray &CKScene::ComputeObjectList(CK_CLASSID cid, CKBOOL derived) {
    static XObjectPointerArray array;
    return array;
}

CKSceneObjectIterator CKScene::GetObjectIterator() {
    return m_SceneObjects.Begin();
}

CKSceneObjectDesc *CKScene::GetSceneObjectDesc(CKSceneObject *o) {
    if (!o || !o->IsInScene(this))
        return nullptr;

    auto it = m_SceneObjects.Find(o->GetID());
    if (it == m_SceneObjects.End())
        return nullptr;
    return it;
}

void CKScene::Activate(CKSceneObject *o, CKBOOL Reset) {
    if (CKIsChildClassOf(o, CKCID_BEOBJECT)) {
        ActivateBeObject((CKBeObject *)o, TRUE, Reset);
    } else if (CKIsChildClassOf(o, CKCID_BEHAVIOR)) {
        ActivateScript((CKBehavior *)o, TRUE, Reset);
    }
}

void CKScene::DeActivate(CKSceneObject *o) {
    if (CKIsChildClassOf(o, CKCID_BEOBJECT)) {
        ActivateBeObject((CKBeObject *)o, FALSE, FALSE);
    } else if (CKIsChildClassOf(o, CKCID_BEHAVIOR)) {
        ActivateScript((CKBehavior *)o, FALSE, FALSE);
    }
}

void CKScene::ActivateBeObject(CKBeObject *beo, CKBOOL activate, CKBOOL reset) {
}

void CKScene::ActivateScript(CKBehavior *beh, CKBOOL activate, CKBOOL reset) {
}

void CKScene::SetObjectFlags(CKSceneObject *o, CK_SCENEOBJECT_FLAGS flags) {
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (desc)
        desc->m_Flags = flags;
}

CK_SCENEOBJECT_FLAGS CKScene::GetObjectFlags(CKSceneObject *o) {
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (desc)
        return (CK_SCENEOBJECT_FLAGS)desc->m_Flags;
    return (CK_SCENEOBJECT_FLAGS)0;
}

CK_SCENEOBJECT_FLAGS CKScene::ModifyObjectFlags(CKSceneObject *o, CKDWORD Add, CKDWORD Remove) {
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (!desc)
        return (CK_SCENEOBJECT_FLAGS)0;
    CKDWORD flags = desc->m_Flags;
    flags |= Add;
    flags &= ~Remove;
    desc->m_Flags = flags;
    return (CK_SCENEOBJECT_FLAGS)flags;
}

CKBOOL CKScene::SetObjectInitialValue(CKSceneObject *o, CKStateChunk *chunk) {
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (!desc)
        return FALSE;

    if (desc->m_InitialValue) {
        delete desc->m_InitialValue;
    }
    if (chunk) {
        chunk->CloseChunk();
    }
    desc->m_InitialValue = chunk;
    return TRUE;
}

CKStateChunk *CKScene::GetObjectInitialValue(CKSceneObject *o) {
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (!desc)
        return nullptr;
    return desc->m_InitialValue;
}

CKBOOL CKScene::IsObjectActive(CKSceneObject *o) {
    if (o == this)
        return TRUE;
    CKSceneObjectDesc *desc = GetSceneObjectDesc(o);
    if (!desc)
        return FALSE;
    return desc->IsActive();
}

void CKScene::ApplyEnvironmentSettings(XObjectPointerArray *renderContexts) {
    XObjectPointerArray *tempArray = NULL;
    bool cleanupNeeded = false;

    // If no render contexts provided, get all from level
    if (!renderContexts) {
        CKLevel *level = m_Context->GetCurrentLevel();
        if (!level)
            return;

        tempArray = new XObjectPointerArray();
        if (!tempArray)
            return;

        const int count = level->GetRenderContextCount();
        for (int i = 0; i < count; ++i) {
            CKRenderContext *rc = level->GetRenderContext(i);
            if (rc) {
                tempArray->PushBack(rc);
            }
        }

        renderContexts = tempArray;
        cleanupNeeded = true;
    }

    for (XObjectPointerArray::Iterator it = renderContexts->Begin(); it != renderContexts->End(); ++it) {
        CKRenderContext *rc = (CKRenderContext *)*it;
        if (!rc)
            continue;

        // 1. Attach viewpoint to starting camera
        CKCamera *startCam = GetStartingCamera();
        if (startCam) {
            rc->AttachViewpointToCamera(startCam);
        }

        // 2. Set background properties
        CKMaterial *bgMaterial = rc->GetBackgroundMaterial();
        if (bgMaterial) {
            // Set background color
            VxColor bgColor(m_BackgroundColor);
            bgMaterial->SetDiffuse(bgColor);

            // Set background texture
            CKTexture *bgTexture = GetBackgroundTexture();
            bgMaterial->SetTexture(bgTexture);
        }

        // 3. Configure fog settings
        rc->SetFogColor(m_FogColor);
        rc->SetFogMode(m_FogMode);
        rc->SetFogStart(m_FogStart);
        rc->SetFogEnd(m_FogEnd);
        rc->SetFogDensity(m_FogDensity);

        // 4. Set ambient lighting
        rc->SetAmbientLight(m_AmbientLightColor);
    }

    // Cleanup temporary array if created
    if (cleanupNeeded && tempArray) {
        delete tempArray;
    }
}

void CKScene::UseEnvironmentSettings(CKBOOL use) {
    if (use)
        m_EnvironmentSettings |= CK_SCENE_USEENVIRONMENTSETTINGS;
    else
        m_EnvironmentSettings &= ~CK_SCENE_USEENVIRONMENTSETTINGS;
}

CKBOOL CKScene::EnvironmentSettings() {
    return m_EnvironmentSettings & CK_SCENE_USEENVIRONMENTSETTINGS;
}

void CKScene::SetAmbientLight(CKDWORD Color) {
    m_AmbientLightColor = Color;
}

CKDWORD CKScene::GetAmbientLight() {
    return m_AmbientLightColor;
}

void CKScene::SetFogMode(VXFOG_MODE Mode) {
    m_FogMode = Mode;
}

void CKScene::SetFogStart(float Start) {
    m_FogStart = Start;
}

void CKScene::SetFogEnd(float End) {
    m_FogEnd = End;
}

void CKScene::SetFogDensity(float Density) {
    m_FogDensity = Density;
}

void CKScene::SetFogColor(CKDWORD Color) {
    m_FogColor = Color;
}

VXFOG_MODE CKScene::GetFogMode() {
    return m_FogMode;
}

float CKScene::GetFogStart() {
    return m_FogStart;
}

float CKScene::GetFogEnd() {
    return m_FogEnd;
}

float CKScene::GetFogDensity() {
    return m_FogDensity;
}

CKDWORD CKScene::GetFogColor() {
    return m_FogColor;
}

void CKScene::SetBackgroundColor(CKDWORD Color) {
    m_BackgroundColor = Color;
}

CKDWORD CKScene::GetBackgroundColor() {
    return m_BackgroundColor;
}

void CKScene::SetBackgroundTexture(CKTexture *texture) {
    m_BackgroundTexture = texture ? texture->GetID() : 0;
}

CKTexture *CKScene::GetBackgroundTexture() {
    return (CKTexture *)m_Context->GetObject(m_BackgroundTexture);
}

void CKScene::SetStartingCamera(CKCamera *camera) {
    if (camera && camera->IsInScene(this))
        m_StartingCamera = camera->GetID();
    else
        m_StartingCamera = 0;
}

CKCamera *CKScene::GetStartingCamera() {
    return (CKCamera *)m_Context->GetObject(m_StartingCamera);
}

CKLevel *CKScene::GetLevel() {
    return (CKLevel *)m_Context->GetObject(m_Level);
}

CKERROR CKScene::Merge(CKScene *mergedScene, CKLevel *fromLevel) {
    if (!mergedScene)
        return CKERR_INVALIDPARAMETER;

    CKMoveAllScripts(mergedScene, this);
    CKCopyAllAttributes(mergedScene, this);

    for (CKSODHashIt it = mergedScene->m_SceneObjects.Begin(); it != mergedScene->m_SceneObjects.End(); ++it) {
        CKSceneObjectDesc &desc = *it;
        CKSceneObject *obj = (CKSceneObject *)m_Context->GetObject(it.GetKey());

        if (obj && !obj->IsInScene(this)) {
            CKSceneObjectDesc *newDesc = AddObjectDesc(obj);
            *newDesc = desc;

            if (fromLevel) {
                CKLevel *currentLevel = m_Context->GetCurrentLevel();
                if (currentLevel && currentLevel != fromLevel) {
                    newDesc->m_InitialValue->RemapObject(fromLevel->GetID(), currentLevel->GetID());
                }
            }

            if (newDesc->m_InitialValue) {
                newDesc->m_InitialValue->RemapObject(
                    mergedScene->GetID(),
                    GetID()
                );
            }

            m_AddObjectList.PushBack(newDesc->m_Object);
        }
    }

    CKRemapObjectParameterValue(m_Context, mergedScene->GetID(), GetID());

    if (m_AddObjectCount == 0) {
        m_Context->ExecuteManagersOnSequenceAddedToScene(
            this,
            m_AddObjectList.Begin(),
            m_AddObjectList.Size()
        );
    }

    m_Context->DestroyObject(mergedScene);
    return CK_OK;
}

CKScene::CKScene(CKContext *Context, CKSTRING name) : CKBeObject(Context, name) {
    m_Level = 0;
    m_Priority = 30000;
    m_EnvironmentSettings = CK_SCENE_USEENVIRONMENTSETTINGS;
    m_SceneGlobalIndex = m_Context->m_ObjectManager->GetSceneGlobalIndex();
    m_Scenes.Set(m_SceneGlobalIndex);
    m_BackgroundColor = 0;
    m_BackgroundTexture = 0;
    m_StartingCamera = 0;
    m_FogStart = 1.0;
    m_FogMode = VXFOG_NONE;
    m_FogColor = 0;
    m_FogDensity = 1.0;
    m_AddObjectCount = 0;
    m_RemoveObjectCount = 0;
    m_AmbientLightColor = 0xF0F0F;
    m_FogEnd = 100.0;
}

CKScene::~CKScene() {
    RemoveAllObjects();
    m_Context->m_ObjectManager->ReleaseSceneGlobalIndex(m_SceneGlobalIndex);
    if (m_Context->m_BehaviorContext.CurrentScene)
        m_Context->m_BehaviorContext.CurrentScene = NULL;
    if (m_Context->m_BehaviorContext.PreviousScene)
        m_Context->m_BehaviorContext.PreviousScene = NULL;
}

CK_CLASSID CKScene::GetClassID() {
    return m_ClassID;
}

void CKScene::PreSave(CKFile *file, CKDWORD flags) {
    CKBeObject::PreSave(file, flags);
}

CKStateChunk *CKScene::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKScene::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

void CKScene::PreDelete() {
    CKBeObject::PreDelete();
}

void CKScene::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKScene::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

int CKScene::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKBeObject::IsObjectUsed(o, cid);
}

CKERROR CKScene::PrepareDependencies(CKDependenciesContext &context) {
    return CKBeObject::PrepareDependencies(context);
}

CKERROR CKScene::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKScene::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

CKSTRING CKScene::GetClassName() {
    return "Scene";
}

int CKScene::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKScene::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKScene::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKSceneObject::m_ClassID);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_SCENE);
}

CKScene *CKScene::CreateInstance(CKContext *Context) {
    return new CKScene(Context, nullptr);
}

CKERROR CKScene::ComputeObjectList(CKObjectArray *array, CK_CLASSID cid, CKBOOL derived) {
    return 0;
}

void CKScene::AddObject(CKSceneObject *o) {
}

void CKScene::RemoveObject(CKSceneObject *o) {
}

void CKScene::RemoveAllObjects() {
}

CKSceneObjectDesc *CKScene::AddObjectDesc(CKSceneObject *o) {
    return NULL;
}
