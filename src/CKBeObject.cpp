#include "CKBeObject.h"
#include "CKFile.h"
#include "CKAttributeManager.h"
#include "CKBehaviorManager.h"
#include "CKBehavior.h"
#include "CKGroup.h"
#include "CKParameterOut.h"

CK_CLASSID CKBeObject::m_ClassID;

void CKBeObject::ExecuteBehaviors(float delta) {
    if (m_Context->IsProfilingEnable())
    {
        ++m_Context->m_Stats.ActiveObjectsExecuted;
        ResetExecutionTime();
    }

    if (!m_ScriptArray && GetClassID() != CKCID_CHARACTER)
    {
        m_Context->GetBehaviorManager()->RemoveObjectNextFrame(this);
        return;
    }

    CKBOOL executed = FALSE;
    int count = m_ScriptArray->Size();
    for (int i = 0; i < count; ++i)
    {
        CKBehavior *beh = (CKBehavior *)m_ScriptArray->GetObject(i);
        if (beh->IsActive())
        {
            executed = TRUE;
            beh->Execute(delta);
            m_LastExecutionTime += beh->GetLastExecutionTime();
        }
    }

    if (!executed && GetClassID() != CKCID_CHARACTER)
    {
        m_Context->GetBehaviorManager()->RemoveObjectNextFrame(this);
        return;
    }
}

CKBOOL CKBeObject::IsInGroup(CKGroup *group) {
    if (!group)
        return FALSE;

    if (group->m_GroupIndex < m_Groups.Size())
        return FALSE;

    return m_Groups.IsSet(group->m_GroupIndex);
}

CKBOOL CKBeObject::HasAttribute(CKAttributeType AttribType) {
    return m_Attributes.IsHere(AttribType);
}

CKBOOL CKBeObject::SetAttribute(CKAttributeType AttribType, CK_ID parameter) {
    if (AttribType < 0)
        return FALSE;

    if (HasAttribute(AttribType))
        return FALSE;

    CKAttributeManager *am = m_Context->GetAttributeManager();

    CK_CLASSID cid = am->GetAttributeCompatibleClassId(AttribType);
    if (!CKIsChildClassOf(GetClassID(), cid))
        return FALSE;

    CKGUID guid = am->GetAttributeParameterGUID(AttribType);
    if (guid.IsValid() && !m_Context->GetObject(parameter))
    {
        CKParameterOut *pout = m_Context->CreateCKParameterOut(NULL, guid, IsDynamic());
        if (pout)
        {
            CKSTRING defValue = am->GetAttributeDefaultValue(AttribType);
            if (defValue)
            {
                pout->SetValue(defValue);
            }
            pout->SetOwner(this);

            parameter = pout->GetID();
        }
        else
        {
            parameter = 0;
        }
    }

    CKAttributeVal attrVal = {AttribType, parameter};
    int count = m_Attributes.Size();
    m_Attributes.Insert(parameter, attrVal);
    am->AddAttributeToObject(AttribType, this);
    return count != m_Attributes.Size();
}

CKBOOL CKBeObject::RemoveAttribute(CKAttributeType AttribType) {
    XAttributeList::Iterator it = m_Attributes.Find(AttribType);
    if (it == m_Attributes.End())
        return FALSE;

    CKAttributeVal val = *it;

    m_Context->GetAttributeManager()->RemoveAttributeFromObject(this);
    m_Attributes.Remove(AttribType);
    m_Context->DestroyObject(val.Parameter);
    return TRUE;
}

CKParameterOut *CKBeObject::GetAttributeParameter(CKAttributeType AttribType) {
    XAttributeList::Iterator it = m_Attributes.Find(AttribType);
    if (it == m_Attributes.End())
        return nullptr;

    return (CKParameterOut *)m_Context->GetObject((*it).Parameter);
}

int CKBeObject::GetAttributeCount() {
    return m_Attributes.Size();
}

int CKBeObject::GetAttributeType(int index) {
    return 0;
}

CKParameterOut *CKBeObject::GetAttributeParameterByIndex(int index) {
    return nullptr;
}

void CKBeObject::GetAttributeList(CKAttributeVal *liste) {

}

void CKBeObject::RemoveAllAttributes() {

}

CKERROR CKBeObject::AddScript(CKBehavior *ckb) {
    return 0;
}

CKBehavior *CKBeObject::RemoveScript(CK_ID id) {
    return nullptr;
}

CKBehavior *CKBeObject::RemoveScript(int pos) {
    return nullptr;
}

CKERROR CKBeObject::RemoveAllScripts() {
    return 0;
}

CKBehavior *CKBeObject::GetScript(int pos) {
    return nullptr;
}

int CKBeObject::GetScriptCount() {
    return 0;
}

int CKBeObject::GetPriority() {
    return 0;
}

void CKBeObject::SetPriority(int priority) {

}

int CKBeObject::GetLastFrameMessageCount() {
    return 0;
}

CKMessage *CKBeObject::GetLastFrameMessage(int pos) {
    return nullptr;
}

void CKBeObject::SetAsWaitingForMessages(CKBOOL wait) {

}

CKBOOL CKBeObject::IsWaitingForMessages() {
    return 0;
}

int CKBeObject::CallBehaviorCallbackFunction(CKDWORD Message, CKGUID *behguid) {
    return 0;
}

float CKBeObject::GetLastExecutionTime() {
    return m_LastExecutionTime;
}

void CKBeObject::ApplyPatchForOlderVersion(int NbObject, CKFileObject *FileObjects) {

}

CKBeObject::CKBeObject(CKContext *Context, CKSTRING name) : CKSceneObject(Context, name) {

}

CKBeObject::~CKBeObject() {

}

CK_CLASSID CKBeObject::GetClassID() {
    return CKSceneObject::GetClassID();
}

void CKBeObject::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);
    if (m_ScriptArray)
        file->SaveObjects(m_ScriptArray->Begin(), m_ScriptArray->Size());

    for (XAttributeList::Iterator it = m_Attributes.Begin(); it != m_Attributes.End(); ++it)
    {
        if ((m_Context->GetAttributeManager()->GetAttributeFlags((*it).AttribType) & CK_ATTRIBUT_DONOTSAVE) == 0)
            file->SaveObject(m_Context->GetObject((*it).Parameter));
    }
}

CKStateChunk *CKBeObject::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKBeObject::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKBeObject::PreDelete() {
    CKObject::PreDelete();
}

int CKBeObject::GetMemoryOccupation() {
    return CKSceneObject::GetMemoryOccupation();
}

CKBOOL CKBeObject::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(obj, cid);
}

CKERROR CKBeObject::PrepareDependencies(CKDependenciesContext &context, CKBOOL iCaller) {
    return CKObject::PrepareDependencies(context, iCaller);
}

CKERROR CKBeObject::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKBeObject::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

void CKBeObject::AddToScene(CKScene *scene, CKBOOL dependencies) {
    CKSceneObject::AddToScene(scene, dependencies);
}

void CKBeObject::RemoveFromScene(CKScene *scene, CKBOOL dependencies) {
    CKSceneObject::RemoveFromScene(scene, dependencies);
}

CKSTRING CKBeObject::GetClassName() {
    return nullptr;
}

int CKBeObject::GetDependenciesCount(int mode) {
    if (mode == 1 || mode == 4)
        return 3;
    else
        return 0;
}

CKSTRING CKBeObject::GetDependencies(int i, int mode) {
    switch (i)
    {
        case 0:
            return "Scripts";
        case 1:
            return "Attributes";
        case 2:
            return "Groups Belonging";
        default:
            return nullptr;
    }
}

void CKBeObject::Register() {
    CKPARAMETERFROMCLASS(CKObject, CKPGUID_BEOBJECT);
    CKCLASSDEFAULTCOPYDEPENDENCIES(CKObject, 1);
}

CKBeObject *CKBeObject::CreateInstance(CKContext *Context) {
    return new CKBeObject(Context);
}

void CKBeObject::AddToSelfScenes(CKSceneObject *o) {

}

void CKBeObject::RemoveFromSelfScenes(CKSceneObject *o) {

}

void CKBeObject::SortScripts() {

}

void CKBeObject::RemoveFromAllGroups() {

}

CKERROR CKBeObject::AddLastFrameMessage(CKMessage *msg) {
    return 0;
}

CKERROR CKBeObject::RemoveAllLastFrameMessage() {
    return 0;
}

void CKBeObject::ApplyOwner() {

}

void CKBeObject::ResetExecutionTime() {

}

int CKBeObject::BeObjectPrioritySort(const void *o1, const void *o2) {
    return 0;
}
