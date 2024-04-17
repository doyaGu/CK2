#include "CKBehavior.h"

#include "CKBeObject.h"
#include "CKParameter.h"
#include "CKParameterIn.h"
#include "CKParameterOut.h"
#include "CKParameterLocal.h"
#include "CKParameterManager.h"

CK_CLASSID CKBehavior::m_ClassID;

CK_BEHAVIOR_TYPE CKBehavior::GetType() {
    if ((m_Flags & CKBEHAVIOR_BUILDINGBLOCK) != 0)
        return CKBEHAVIORTYPE_BASE;
    if ((m_Flags & CKBEHAVIOR_SCRIPT) != 0)
        return CKBEHAVIORTYPE_SCRIPT;
    else
        return CKBEHAVIORTYPE_BEHAVIOR;
}

void CKBehavior::SetType(CK_BEHAVIOR_TYPE type) {
    if (type == CKBEHAVIORTYPE_SCRIPT)
        m_Flags |= CKBEHAVIOR_SCRIPT;
}

void CKBehavior::SetFlags(CK_BEHAVIOR_FLAGS flags) {
    m_Flags = flags;
}

CK_BEHAVIOR_FLAGS CKBehavior::GetFlags() {
    return static_cast<CK_BEHAVIOR_FLAGS>(m_Flags);
}

CK_BEHAVIOR_FLAGS CKBehavior::ModifyFlags(CKDWORD Add, CKDWORD Remove) {
    m_Flags = (m_Flags | Add) & ~Remove;
    return static_cast<CK_BEHAVIOR_FLAGS>(m_Flags);
}

void CKBehavior::UseGraph() {
    if (m_GraphData) {
        m_GraphData = new BehaviorGraphData;
    }
    delete m_BlockData;
    m_BlockData = NULL;
    m_Flags = (m_Flags & ~0xFFFF) | ((unsigned short)m_Flags & 0x7FF7);
}

void CKBehavior::UseFunction() {
    if (!m_BlockData) {
        m_BlockData = new BehaviorBlockData;
    }
    delete m_GraphData;
    m_BlockData = NULL;
    m_Flags |= 0x8008;
}

int CKBehavior::IsUsingFunction() {
    return (m_Flags & CKBEHAVIOR_USEFUNCTION) != 0;
}

CKBOOL CKBehavior::IsTargetable() {
    return (m_Flags & CKBEHAVIOR_TARGETABLE) != 0;
}

CKBeObject *CKBehavior::GetTarget() {
    if (!m_InputTargetParam)
        return (CKBeObject *)m_Context->GetObject(m_Owner);

    CKParameterIn *pin = (CKParameterIn *)m_Context->GetObject(m_InputTargetParam);
    if (!pin)
        return NULL;

    CKParameter *param = pin->GetRealSource();
    CK_ID objId = NULL;
    param->GetValue(&objId);
    CKBeObject *obj = (CKBeObject *)m_Context->GetObject(objId);
    CKParameterManager *pm = m_Context->GetParameterManager();
    CK_CLASSID cid = pm->TypeToClassID(pin->GetType());
    if (!CKIsChildClassOf(obj, cid))
        return NULL;
    return obj;
}

CKERROR CKBehavior::UseTarget(CKBOOL Use) {
    return 0;
}

CKBOOL CKBehavior::IsUsingTarget() {
    return 0;
}

CKParameterIn *CKBehavior::GetTargetParameter() {
    return nullptr;
}

void CKBehavior::SetAsTargetable(CKBOOL target) {

}

CKParameterIn *CKBehavior::ReplaceTargetParameter(CKParameterIn *targetParam) {
    return nullptr;
}

CKParameterIn *CKBehavior::RemoveTargetParameter() {
    return nullptr;
}

CK_CLASSID CKBehavior::GetCompatibleClassID() {
    return 0;
}

void CKBehavior::SetCompatibleClassID(CK_CLASSID) {

}

void CKBehavior::SetFunction(CKBEHAVIORFCT fct) {

}

CKBEHAVIORFCT CKBehavior::GetFunction() {
    return nullptr;
}

void CKBehavior::SetCallbackFunction(CKBEHAVIORCALLBACKFCT fct) {

}

int CKBehavior::CallCallbackFunction(CKDWORD Message) {
    return 0;
}

int CKBehavior::CallSubBehaviorsCallbackFunction(CKDWORD Message, CKGUID *behguid) {
    return 0;
}

CKBOOL CKBehavior::IsActive() {
    return 0;
}

int CKBehavior::Execute(float deltat) {
    return 0;
}

CKBOOL CKBehavior::IsParentScriptActiveInScene(CKScene *scn) {
    return 0;
}

int CKBehavior::GetShortestDelay(CKBehavior *beh) {
    return 0;
}

CKBeObject *CKBehavior::GetOwner() {
    return nullptr;
}

CKBehavior *CKBehavior::GetParent() {
    return nullptr;
}

CKBehavior *CKBehavior::GetOwnerScript() {
    return nullptr;
}

CKERROR CKBehavior::InitFromPrototype(CKBehaviorPrototype *proto) {
    return 0;
}

CKERROR CKBehavior::InitFromGuid(CKGUID Guid) {
    return 0;
}

CKERROR CKBehavior::InitFctPtrFromGuid(CKGUID Guid) {
    return 0;
}

CKERROR CKBehavior::InitFctPtrFromPrototype(CKBehaviorPrototype *proto) {
    return 0;
}

CKGUID CKBehavior::GetPrototypeGuid() {
    return CKGUID();
}

CKBehaviorPrototype *CKBehavior::GetPrototype() {
    return nullptr;
}

CKSTRING CKBehavior::GetPrototypeName() {
    return nullptr;
}

CKDWORD CKBehavior::GetVersion() {
    return 0;
}

void CKBehavior::SetVersion(CKDWORD version) {

}

void CKBehavior::ActivateOutput(int pos, CKBOOL active) {

}

CKBOOL CKBehavior::IsOutputActive(int pos) {
    return 0;
}

CKBehaviorIO *CKBehavior::RemoveOutput(int pos) {
    return nullptr;
}

CKERROR CKBehavior::DeleteOutput(int pos) {
    return 0;
}

CKBehaviorIO *CKBehavior::GetOutput(int pos) {
    return nullptr;
}

int CKBehavior::GetOutputCount() {
    return 0;
}

int CKBehavior::GetOutputPosition(CKBehaviorIO *pbio) {
    return 0;
}

int CKBehavior::AddOutput(CKSTRING name) {
    return 0;
}

CKBehaviorIO *CKBehavior::ReplaceOutput(int pos, CKBehaviorIO *io) {
    return nullptr;
}

CKBehaviorIO *CKBehavior::CreateOutput(CKSTRING name) {
    return nullptr;
}

void CKBehavior::ActivateInput(int pos, CKBOOL active) {

}

CKBOOL CKBehavior::IsInputActive(int pos) {
    return 0;
}

CKBehaviorIO *CKBehavior::RemoveInput(int pos) {
    return nullptr;
}

CKERROR CKBehavior::DeleteInput(int pos) {
    return 0;
}

CKBehaviorIO *CKBehavior::GetInput(int pos) {
    return nullptr;
}

int CKBehavior::GetInputCount() {
    return 0;
}

int CKBehavior::GetInputPosition(CKBehaviorIO *pbio) {
    return 0;
}

int CKBehavior::AddInput(CKSTRING name) {
    return 0;
}

CKBehaviorIO *CKBehavior::ReplaceInput(int pos, CKBehaviorIO *io) {
    return nullptr;
}

CKBehaviorIO *CKBehavior::CreateInput(CKSTRING name) {
    return nullptr;
}

CKERROR CKBehavior::ExportInputParameter(CKParameterIn *p) {
    return 0;
}

CKParameterIn *CKBehavior::CreateInputParameter(CKSTRING name, CKParameterType type) {
    return nullptr;
}

CKParameterIn *CKBehavior::CreateInputParameter(CKSTRING name, CKGUID guid) {
    return nullptr;
}

void CKBehavior::AddInputParameter(CKParameterIn *in) {

}

int CKBehavior::GetInputParameterPosition(CKParameterIn *) {
    return 0;
}

CKParameterIn *CKBehavior::GetInputParameter(int pos) {
    return nullptr;
}

CKParameterIn *CKBehavior::RemoveInputParameter(int pos) {
    return nullptr;
}

CKParameterIn *CKBehavior::ReplaceInputParameter(int pos, CKParameterIn *param) {
    return nullptr;
}

int CKBehavior::GetInputParameterCount() {
    return 0;
}

CKERROR CKBehavior::GetInputParameterValue(int pos, void *buf) {
    return 0;
}

void *CKBehavior::GetInputParameterReadDataPtr(int pos) {
    return nullptr;
}

CKObject *CKBehavior::GetInputParameterObject(int pos) {
    return nullptr;
}

CKBOOL CKBehavior::IsInputParameterEnabled(int pos) {
    return 0;
}

void CKBehavior::EnableInputParameter(int pos, CKBOOL enable) {

}

CKERROR CKBehavior::ExportOutputParameter(CKParameterOut *p) {
    return 0;
}

CKParameterOut *CKBehavior::CreateOutputParameter(CKSTRING name, CKParameterType type) {
    return nullptr;
}

CKParameterOut *CKBehavior::CreateOutputParameter(CKSTRING name, CKGUID guid) {
    return nullptr;
}

CKParameterOut *CKBehavior::GetOutputParameter(int pos) {
    return nullptr;
}

int CKBehavior::GetOutputParameterPosition(CKParameterOut *) {
    return 0;
}

CKParameterOut *CKBehavior::ReplaceOutputParameter(int pos, CKParameterOut *p) {
    return nullptr;
}

CKParameterOut *CKBehavior::RemoveOutputParameter(int pos) {
    return nullptr;
}

void CKBehavior::AddOutputParameter(CKParameterOut *out) {

}

int CKBehavior::GetOutputParameterCount() {
    return 0;
}

CKERROR CKBehavior::GetOutputParameterValue(int pos, void *buf) {
    return 0;
}

CKERROR CKBehavior::SetOutputParameterValue(int pos, const void *buf, int size) {
    return 0;
}

void *CKBehavior::GetOutputParameterWriteDataPtr(int pos) {
    return nullptr;
}

CKERROR CKBehavior::SetOutputParameterObject(int pos, CKObject *obj) {
    return 0;
}

CKObject *CKBehavior::GetOutputParameterObject(int pos) {
    return nullptr;
}

CKBOOL CKBehavior::IsOutputParameterEnabled(int pos) {
    return 0;
}

void CKBehavior::EnableOutputParameter(int pos, CKBOOL enable) {

}

void CKBehavior::SetInputParameterDefaultValue(CKParameterIn *pin, CKParameter *plink) {

}

CKParameterLocal *CKBehavior::CreateLocalParameter(CKSTRING name, CKParameterType type) {
    return nullptr;
}

CKParameterLocal *CKBehavior::CreateLocalParameter(CKSTRING name, CKGUID guid) {
    return nullptr;
}

CKParameterLocal *CKBehavior::GetLocalParameter(int pos) {
    return nullptr;
}

CKParameterLocal *CKBehavior::RemoveLocalParameter(int pos) {
    return nullptr;
}

void CKBehavior::AddLocalParameter(CKParameterLocal *loc) {

}

int CKBehavior::GetLocalParameterPosition(CKParameterLocal *) {
    return 0;
}

int CKBehavior::GetLocalParameterCount() {
    return 0;
}

CKERROR CKBehavior::GetLocalParameterValue(int pos, void *buf) {
    return 0;
}

CKERROR CKBehavior::SetLocalParameterValue(int pos, const void *buf, int size) {
    return 0;
}

void *CKBehavior::GetLocalParameterWriteDataPtr(int pos) {
    return nullptr;
}

void *CKBehavior::GetLocalParameterReadDataPtr(int pos) {
    return nullptr;
}

CKObject *CKBehavior::GetLocalParameterObject(int pos) {
    return nullptr;
}

CKERROR CKBehavior::SetLocalParameterObject(int pos, CKObject *obj) {
    return 0;
}

CKBOOL CKBehavior::IsLocalParameterSetting(int pos) {
    return 0;
}

void CKBehavior::Activate(CKBOOL Active, CKBOOL breset) {

}

CKERROR CKBehavior::AddSubBehavior(CKBehavior *cbk) {
    return 0;
}

CKBehavior *CKBehavior::RemoveSubBehavior(CKBehavior *cbk) {
    return nullptr;
}

CKBehavior *CKBehavior::RemoveSubBehavior(int pos) {
    return nullptr;
}

CKBehavior *CKBehavior::GetSubBehavior(int pos) {
    return nullptr;
}

int CKBehavior::GetSubBehaviorCount() {
    return 0;
}

CKERROR CKBehavior::AddSubBehaviorLink(CKBehaviorLink *cbkl) {
    return 0;
}

CKBehaviorLink *CKBehavior::RemoveSubBehaviorLink(CKBehaviorLink *cbkl) {
    return nullptr;
}

CKBehaviorLink *CKBehavior::RemoveSubBehaviorLink(int pos) {
    return nullptr;
}

CKBehaviorLink *CKBehavior::GetSubBehaviorLink(int pos) {
    return nullptr;
}

int CKBehavior::GetSubBehaviorLinkCount() {
    return 0;
}

CKERROR CKBehavior::AddParameterOperation(CKParameterOperation *op) {
    return 0;
}

CKParameterOperation *CKBehavior::GetParameterOperation(int pos) {
    return nullptr;
}

CKParameterOperation *CKBehavior::RemoveParameterOperation(int pos) {
    return nullptr;
}

CKParameterOperation *CKBehavior::RemoveParameterOperation(CKParameterOperation *op) {
    return nullptr;
}

int CKBehavior::GetParameterOperationCount() {
    return 0;
}

int CKBehavior::GetPriority() {
    return 0;
}

void CKBehavior::SetPriority(int priority) {
    m_Priority = priority;

    // Check if this behavior has a parent
    if (m_BehParent) {
        // Get the parent behavior object
        CKBehavior *parentBehavior = (CKBehavior *) m_Context->GetObjectA(m_BehParent);

        // If the parent behavior object exists, sort its sub-behaviors
        if (parentBehavior)
            parentBehavior->SortSubs(); // Calling a class method with an object instance
    } else {
        // Check if this behavior has an owner
        if (m_Owner) {
            // Check if the 'm_Flags' bitmask has the second bit set
            if ((m_Flags & 2) != 0) {
                // Get the owner object
                CKBeObject *ownerObject = (CKBeObject *) m_Context->GetObjectA(m_Owner);

                // If the owner object exists, sort its scripts
                //if (ownerObject)
                  //  ownerObject->SortScripts(); // Calling a class method with an object instance
            }
        }
    }
}

float CKBehavior::GetLastExecutionTime() {
    return 0;
}

CKERROR CKBehavior::SetOwner(CKBeObject *, CKBOOL callback) {
    return 0;
}

CKERROR CKBehavior::SetSubBehaviorOwner(CKBeObject *o, CKBOOL callback) {
    return 0;
}

void CKBehavior::NotifyEdition() {

}

void CKBehavior::NotifySettingsEdition() {

}

CKStateChunk *CKBehavior::GetInterfaceChunk() {
    return nullptr;
}

void CKBehavior::SetInterfaceChunk(CKStateChunk *state) {

}

//CKBehavior::CKBehavior(CKContext *Context, CKSTRING name) : CKObject(Context, name) {
//
//}

CKBehavior::~CKBehavior() {

}

CK_CLASSID CKBehavior::GetClassID() {
    return CKSceneObject::GetClassID();
}

void CKBehavior::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);
}

CKStateChunk *CKBehavior::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKBehavior::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKBehavior::PostLoad() {
    CKObject::PostLoad();
}

void CKBehavior::PreDelete() {
    CKObject::PreDelete();
}

int CKBehavior::GetMemoryOccupation() {
    return CKSceneObject::GetMemoryOccupation();
}

CKBOOL CKBehavior::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(obj, cid);
}

CKERROR CKBehavior::PrepareDependencies(CKDependenciesContext &context, CKBOOL iCaller) {
    return CKObject::PrepareDependencies(context, iCaller);
}

CKERROR CKBehavior::RemapDependencies(CKDependenciesContext &context) {
    return CKObject::RemapDependencies(context);
}

CKERROR CKBehavior::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKObject::Copy(o, context);
}

CKSTRING CKBehavior::GetClassNameA() {
    return nullptr;
}

int CKBehavior::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKBehavior::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKBehavior::Register() {

}

CKBehavior *CKBehavior::CreateInstance(CKContext *Context) {
    return nullptr;
}

void CKBehavior::Reset() {

}

void CKBehavior::ErrorMessage(CKSTRING Error, CKSTRING Context, CKBOOL ShowOwner, CKBOOL ShowScript) {

}

void CKBehavior::ErrorMessage(CKSTRING Error, CKDWORD Context, CKBOOL ShowOwner, CKBOOL ShowScript) {

}

void CKBehavior::SetPrototypeGuid(CKGUID ckguid) {

}

void CKBehavior::SetParent(CKBehavior *parent) {

}

void CKBehavior::SortSubs() {

}

void CKBehavior::ResetExecutionTime() {

}

void CKBehavior::ExecuteStepStart() {

}

int CKBehavior::ExecuteStep(float delta, CKDebugContext *Context) {
    return 0;
}

void CKBehavior::WarnInfiniteLoop() {

}

int CKBehavior::InternalGetShortestDelay(CKBehavior *beh, XObjectPointerArray &behparsed) {
    return 0;
}

void CKBehavior::CheckIOsActivation() {

}

void CKBehavior::CheckBehaviorActivity() {

}

int CKBehavior::ExecuteFunction() {
    return 0;
}

void CKBehavior::FindNextBehaviorsToExecute(CKBehavior *beh) {

}

void CKBehavior::HierarchyPostLoad() {

}

int CKBehavior::BehaviorPrioritySort(CKObject *o1, CKObject *o2) {
    return 0;
}

int CKBehavior::BehaviorPrioritySort(const void *elem1, const void *elem2) {
    return 0;
}

void CKBehavior::ApplyPatchLoad() {

}
