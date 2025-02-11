#include "CKBehavior.h"

#include "CKBehaviorIO.h"
#include "CKBehaviorLink.h"
#include "CKBehaviorPrototype.h"
#include "CKBeObject.h"
#include "CKFile.h"
#include "CKParameter.h"
#include "CKParameterIn.h"
#include "CKParameterOut.h"
#include "CKParameterLocal.h"
#include "CKParameterManager.h"
#include "CKStateChunk.h"

CK_CLASSID CKBehavior::m_ClassID = CKCID_BEHAVIOR;

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
    return (CK_BEHAVIOR_FLAGS)m_Flags;
}

CK_BEHAVIOR_FLAGS CKBehavior::ModifyFlags(CKDWORD Add, CKDWORD Remove) {
    m_Flags = (m_Flags | Add) & ~Remove;
    return (CK_BEHAVIOR_FLAGS)m_Flags;
}

void CKBehavior::UseGraph() {
    if (!m_GraphData) {
        m_GraphData = new BehaviorGraphData;
    }
    delete m_BlockData;
    m_BlockData = nullptr;
    m_Flags &= ~(CKBEHAVIOR_BUILDINGBLOCK | CKBEHAVIOR_USEFUNCTION);
}

void CKBehavior::UseFunction() {
    if (!m_BlockData) {
        m_BlockData = new BehaviorBlockData;
    }
    delete m_GraphData;
    m_GraphData = nullptr;
    m_Flags |= CKBEHAVIOR_BUILDINGBLOCK | CKBEHAVIOR_USEFUNCTION;
}

CKBOOL CKBehavior::IsUsingFunction() {
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
        return nullptr;

    CKParameter *param = pin->GetRealSource();
    CK_ID objId = 0;
    param->GetValue(&objId);
    CKBeObject *obj = (CKBeObject *)m_Context->GetObject(objId);
    CKParameterManager *pm = m_Context->GetParameterManager();
    CK_CLASSID cid = pm->TypeToClassID(pin->GetType());
    if (!CKIsChildClassOf(obj, cid))
        return nullptr;
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
    if (target) {
        m_Flags |= CKBEHAVIOR_TARGETABLE;
    } else {
        UseTarget(FALSE);
        m_Flags &= ~CKBEHAVIOR_TARGETABLE;
    }
}

CKParameterIn *CKBehavior::ReplaceTargetParameter(CKParameterIn *targetParam) {
    return nullptr;
}

CKParameterIn *CKBehavior::RemoveTargetParameter() {
    return nullptr;
}

CK_CLASSID CKBehavior::GetCompatibleClassID() {
    return m_CompatibleClassID;
}

void CKBehavior::SetCompatibleClassID(CK_CLASSID cid) {
    m_CompatibleClassID = cid;
}

void CKBehavior::SetFunction(CKBEHAVIORFCT fct) {
    if (m_BlockData)
        m_BlockData->m_Function = fct;
}

CKBEHAVIORFCT CKBehavior::GetFunction() {
    if (!m_BlockData)
        return nullptr;
    return m_BlockData->m_Function;
}

void CKBehavior::SetCallbackFunction(CKBEHAVIORCALLBACKFCT fct) {
    if (m_BlockData)
        m_BlockData->m_Callback = fct;
}

int CKBehavior::CallCallbackFunction(CKDWORD Message) {
    return 0;
}

int CKBehavior::CallSubBehaviorsCallbackFunction(CKDWORD Message, CKGUID *behguid) {
    return 0;
}

CKBOOL CKBehavior::IsActive() {
    return (m_Flags & CKBEHAVIOR_ACTIVE) != 0;
}

int CKBehavior::Execute(float delta) {
    return 0;
}

CKBOOL CKBehavior::IsParentScriptActiveInScene(CKScene *scn) {
    return 0;
}

int CKBehavior::GetShortestDelay(CKBehavior *beh) {
    XObjectPointerArray behParsed;
    return InternalGetShortestDelay(beh, behParsed);
}

CKBeObject *CKBehavior::GetOwner() {
    return (CKBeObject *)m_Context->GetObject(m_Owner);
}

CKBehavior *CKBehavior::GetParent() {
    return (CKBehavior *)m_Context->GetObject(m_BehParent);
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
    if (!m_BlockData)
        return CKGUID();
    return m_BlockData->m_Guid;
}

CKBehaviorPrototype *CKBehavior::GetPrototype() {
    if (!m_BlockData)
        return nullptr;
    return CKGetPrototypeFromGuid(m_BlockData->m_Guid);
}

CKSTRING CKBehavior::GetPrototypeName() {
    CKBehaviorPrototype *proto = GetPrototype();
    if (!proto)
        return nullptr;
    return proto->GetName();
}

CKDWORD CKBehavior::GetVersion() {
    if (!m_BlockData)
        return 0;
    return m_BlockData->m_Version;
}

void CKBehavior::SetVersion(CKDWORD version) {
    if (m_BlockData)
        m_BlockData->m_Version = version;
}

void CKBehavior::ActivateOutput(int pos, CKBOOL active) {
    if (pos < 0 || pos >= GetOutputCount())
        return;

    CKBehaviorIO *io = (CKBehaviorIO *)m_OutputArray[pos];
    if (!io)
        return;

    if (active) {
        io->m_ObjectFlags |= CK_BEHAVIORIO_ACTIVE;
    } else {
        io->m_ObjectFlags &= ~CK_BEHAVIORIO_ACTIVE;
    }
}

CKBOOL CKBehavior::IsOutputActive(int pos) {
    if (pos < 0 || pos >= GetOutputCount())
        return FALSE;

    CKBehaviorIO *io = (CKBehaviorIO *)m_OutputArray[pos];
    return io ? (io->m_ObjectFlags & CK_BEHAVIORIO_ACTIVE) != 0 : FALSE;
}

CKBehaviorIO *CKBehavior::RemoveOutput(int pos) {
    if (pos < 0 || pos >= m_OutputArray.Size()) return nullptr;

    CKBehaviorIO *io = (CKBehaviorIO *)m_OutputArray[pos];
    m_OutputArray.RemoveAt(pos);
    return io;
}

CKERROR CKBehavior::DeleteOutput(int pos) {
    CKBehaviorIO *io = RemoveOutput(pos);
    if (!io)
        return CKERR_INVALIDPARAMETER;
    return m_Context->DestroyObject(io);
}

CKBehaviorIO *CKBehavior::GetOutput(int pos) {
    if (pos < 0 || pos >= m_OutputArray.Size())
        return nullptr;
    return (CKBehaviorIO *)m_OutputArray[pos];
}

int CKBehavior::GetOutputCount() {
    return m_OutputArray.Size();
}

int CKBehavior::GetOutputPosition(CKBehaviorIO *pbio) {
    return 0;
}

int CKBehavior::AddOutput(CKSTRING name) {
    CKBehaviorIO *io = CreateOutput(name);
    return io ? m_OutputArray.Size() - 1 : -1;
}

CKBehaviorIO *CKBehavior::ReplaceOutput(int pos, CKBehaviorIO *io) {
    if (pos < 0 || pos >= m_OutputArray.Size())
        return nullptr;

    CKBehaviorIO *oldIO = (CKBehaviorIO *)m_OutputArray[pos];
    m_OutputArray[pos] = io;
    return oldIO;
}

CKBehaviorIO *CKBehavior::CreateOutput(CKSTRING name) {
    CKBehaviorIO *io = (CKBehaviorIO *)m_Context->CreateObject(CKCID_BEHAVIORIO, name, CK_OBJECTCREATION_SameDynamic);
    if (io) {
        m_OutputArray.PushBack(io);
        io->m_OwnerBehavior = this;
        io->ModifyObjectFlags(CK_BEHAVIORIO_OUT, CK_OBJECT_IOTYPEMASK);
    }
    return io;
}

void CKBehavior::ActivateInput(int pos, CKBOOL active) {
    if (pos < 0 || pos >= m_InputArray.Size())
        return;

    CKBehaviorIO *io = (CKBehaviorIO *)m_InputArray[pos];
    if (io) {
        if (active)
            io->ModifyObjectFlags(CK_BEHAVIORIO_ACTIVE, 0);
        else
            io->ModifyObjectFlags(0, CK_BEHAVIORIO_ACTIVE);
    }
}

CKBOOL CKBehavior::IsInputActive(int pos) {
    if (pos < 0 || pos >= m_InputArray.Size())
        return FALSE;

    CKBehaviorIO *io = (CKBehaviorIO *)m_InputArray[pos];
    return io ? io->IsActive() : FALSE;
}

CKBehaviorIO *CKBehavior::RemoveInput(int pos) {
    if (pos < 0 || pos >= m_InputArray.Size())
        return nullptr;

    CKBehaviorIO *io = (CKBehaviorIO *)m_InputArray[pos];
    m_InputArray.RemoveAt(pos);
    return io;
}

CKERROR CKBehavior::DeleteInput(int pos) {
    CKBehaviorIO *io = RemoveInput(pos);
    if (!io)
        return CKERR_INVALIDPARAMETER;
    return m_Context->DestroyObject(io);
}

CKBehaviorIO *CKBehavior::GetInput(int pos) {
    if (pos < 0 || pos >= m_InputArray.Size())
        return nullptr;
    return (CKBehaviorIO *)m_InputArray[pos];
}

int CKBehavior::GetInputCount() {
    return m_InputArray.Size();
}

int CKBehavior::GetInputPosition(CKBehaviorIO *io) {
    if (!io) return -1;

    for (int i = 0; i < m_InputArray.Size(); ++i) {
        if (m_InputArray[i] == io) {
            return i;
        }
    }
    return -1;
}

int CKBehavior::AddInput(CKSTRING name) {
    CKBehaviorIO *io = CreateInput(name);
    return io ? m_InputArray.Size() - 1 : -1;
}

CKBehaviorIO *CKBehavior::ReplaceInput(int pos, CKBehaviorIO *io) {
    if (pos < 0 || pos >= m_InputArray.Size())
        return nullptr;

    CKBehaviorIO *oldIO = (CKBehaviorIO *)m_InputArray[pos];
    m_InputArray[pos] = io;
    return oldIO;
}

CKBehaviorIO *CKBehavior::CreateInput(CKSTRING name) {
    CKBehaviorIO *io = (CKBehaviorIO *)m_Context->CreateObject(CKCID_BEHAVIORIO, name, CK_OBJECTCREATION_SameDynamic);
    if (io) {
        m_InputArray.PushBack(io);
        io->m_OwnerBehavior = this;
        io->ModifyObjectFlags(CK_BEHAVIORIO_IN, CK_OBJECT_IOTYPEMASK); // Clear OUT, set IN
    }
    return io;
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

CKParameterIn *CKBehavior::InsertInputParameter(int pos, CKSTRING name, CKParameterType type) {
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

CKParameterOut *CKBehavior::InsertOutputParameter(int pos, CKSTRING name, CKParameterType type) {
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
    if (breset || m_Flags == CKBEHAVIOR_NONE)
        Reset();
    if (Active)
        m_Flags |= CKBEHAVIOR_ACTIVE;
    else
        m_Flags &= ~CKBEHAVIOR_ACTIVE;
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
    if (!m_GraphData)
        return 0;
    return m_GraphData->m_Operations.Size();
}

int CKBehavior::GetPriority() {
    return m_Priority;
}

void CKBehavior::SetPriority(int priority) {
    m_Priority = priority;

    // Check if this behavior has a parent
    if (m_BehParent) {
        // Get the parent behavior object
        CKBehavior *parentBehavior = (CKBehavior *)m_Context->GetObjectA(m_BehParent);

        // If the parent behavior object exists, sort its sub-behaviors
        if (parentBehavior)
            parentBehavior->SortSubs(); // Calling a class method with an object instance
    } else {
        // Check if this behavior has an owner
        if (m_Owner) {
            // Check if the 'm_Flags' bitmask has the second bit set
            if ((m_Flags & 2) != 0) {
                // Get the owner object
                CKBeObject *ownerObject = (CKBeObject *)m_Context->GetObjectA(m_Owner);

                // If the owner object exists, sort its scripts
                //if (ownerObject)
                //  ownerObject->SortScripts(); // Calling a class method with an object instance
            }
        }
    }
}

float CKBehavior::GetLastExecutionTime() {
    return m_LastExecutionTime;
}

CKERROR CKBehavior::SetOwner(CKBeObject *, CKBOOL callback) {
    return 0;
}

CKERROR CKBehavior::SetSubBehaviorOwner(CKBeObject *o, CKBOOL callback) {
    return 0;
}

void CKBehavior::NotifyEdition() {
    CallCallbackFunction(CKM_BEHAVIOREDITED);
}

void CKBehavior::NotifySettingsEdition() {
    CallCallbackFunction(CKM_BEHAVIORSETTINGSEDITED);
}

CKStateChunk *CKBehavior::GetInterfaceChunk() {
    return m_InterfaceChunk;
}

void CKBehavior::SetInterfaceChunk(CKStateChunk *state) {
    if (m_InterfaceChunk)
        delete m_InterfaceChunk;
    m_InterfaceChunk = state;
}

CKBehavior::CKBehavior(CKContext *Context, CKSTRING name) : CKSceneObject(Context, name) {
}

CKBehavior::~CKBehavior() {
}

CK_CLASSID CKBehavior::GetClassID() {
    return m_ClassID;
}

void CKBehavior::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);

    if (m_GraphData) {
        file->SaveObjects(m_GraphData->m_SubBehaviors.Begin(), m_GraphData->m_SubBehaviors.Size());
        file->SaveObjects(m_GraphData->m_SubBehaviorLinks.Begin(), m_GraphData->m_SubBehaviorLinks.Size());
        file->SaveObjects(m_GraphData->m_Operations.Begin(), m_GraphData->m_Operations.Size());
    }

    file->SaveObjects(m_InputArray.Begin(), m_InputArray.Size());
    file->SaveObjects(m_OutputArray.Begin(), m_OutputArray.Size());
    file->SaveObjects(m_InParameter.Begin(), m_InParameter.Size());
    file->SaveObjects(m_OutParameter.Begin(), m_OutParameter.Size());
    file->SaveObjects(m_LocalParameter.Begin(), m_LocalParameter.Size());

    CKObject *targetObj = m_Context->GetObject(m_InputTargetParam);
    file->SaveObject(targetObj);
}

CKStateChunk *CKBehavior::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKBehavior::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKBehavior::PostLoad() {
    if ((m_Flags & (CKBEHAVIOR_SCRIPT | CKBEHAVIOR_TOPMOST)) != 0)
        HierarchyPostLoad();
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

CKERROR CKBehavior::PrepareDependencies(CKDependenciesContext &context) {
    return CKObject::PrepareDependencies(context);
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
    return new CKBehavior(Context, nullptr);
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
    if (m_GraphData)
        m_GraphData->m_SubBehaviors.Sort(CKBehavior::BehaviorPrioritySort);
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
