#include "CKObject.h"
#include "CKObjectManager.h"
#include "CKStateChunk.h"

CK_CLASSID CKObject::m_ClassID = CKCID_OBJECT;

void CKObject::SetName(CKSTRING Name, CKBOOL shared) {
    if ((m_ObjectFlags & CK_OBJECT_NAMESHARED) == 0)
        delete[] m_Name;
    m_Name = nullptr;
    if (shared) {
        m_ObjectFlags |= CK_OBJECT_NAMESHARED;
        m_Name = Name;
    } else {
        m_ObjectFlags &= ~CK_OBJECT_NAMESHARED;
        m_Name = CKStrdup(Name);
    }
}

void *CKObject::GetAppData() {
    return m_Context->m_ObjectManager->GetObjectAppData(m_ID);
}

void CKObject::SetAppData(void *Data) {
    m_Context->m_ObjectManager->SetObjectAppData(m_ID, Data);
}

void CKObject::Show(CK_OBJECT_SHOWOPTION show) {
    m_ObjectFlags &= ~(CK_OBJECT_VISIBLE | CK_OBJECT_HIERACHICALHIDE);
    if ((show & CKSHOW) != 0) {
        m_ObjectFlags |= CK_OBJECT_VISIBLE;
    } else if ((show & CKHIERARCHICALHIDE) != 0) {
        m_ObjectFlags |= CK_OBJECT_HIERACHICALHIDE;
    }
}

CKBOOL CKObject::IsHiddenByParent() {
    return FALSE;
}

int CKObject::CanBeHide() {
    return FALSE;
}

CKObject::CKObject(CKContext *Context, CKSTRING name) {
    if (name)
        SetName(name);
    else
        m_Name = nullptr;

    m_ID = 0;
    m_ObjectFlags = CK_OBJECT_VISIBLE;
    m_Context = Context;
    m_ID = Context->m_ObjectManager->RegisterObject(this);
}

CKObject::~CKObject() {
    m_Context->m_ObjectManager->UnRegisterObject(m_ID);
    if ((m_ObjectFlags & CK_OBJECT_NAMESHARED) == 0)
        delete[] m_Name;
}

CK_CLASSID CKObject::GetClassID() {
    return m_ClassID;
}

void CKObject::PreSave(CKFile *file, CKDWORD flags) { /* Empty */ }

CKStateChunk *CKObject::Save(CKFile *file, CKDWORD flags) {
    CKStateChunk *chunk = new CKStateChunk(CKCID_OBJECT, file);
    chunk->SetDynamic(IsDynamic());
    chunk->StartWrite();
    chunk->SetDataVersion(CHUNK_DEV_2_1);
    chunk->CheckSize(GetMemoryOccupation());
    if (m_ObjectFlags & CK_OBJECT_HIERACHICALHIDE)
        chunk->WriteIdentifier(CK_STATESAVE_OBJECTHIERAHIDDEN);
    else if (!(m_ObjectFlags & CK_OBJECT_VISIBLE))
        chunk->WriteIdentifier(CK_STATESAVE_OBJECTHIDDEN);
    return chunk;
}

CKERROR CKObject::Load(CKStateChunk *chunk, CKFile *file) {
    chunk->StartRead();
    if (chunk->SeekIdentifier(CK_STATESAVE_OBJECTHIDDEN)) {
        m_ObjectFlags &= ~(CK_OBJECT_VISIBLE | CK_OBJECT_HIERACHICALHIDE);
    } else {
        if (chunk->SeekIdentifier(CK_STATESAVE_OBJECTHIERAHIDDEN)) {
            m_ObjectFlags &= ~CK_OBJECT_VISIBLE;
            m_ObjectFlags |= CK_OBJECT_HIERACHICALHIDE;
        } else {
            m_ObjectFlags &= ~CK_OBJECT_HIERACHICALHIDE;
            m_ObjectFlags |= CK_OBJECT_VISIBLE;
        }
    }
    return CK_OK;
}

void CKObject::PostLoad() {
    /* Empty? */
}

void CKObject::PreDelete() {
    /* Empty? */
}

void CKObject::CheckPreDeletion() {
    /* Empty? */
}

void CKObject::CheckPostDeletion() {
    /* Empty? */
}

int CKObject::GetMemoryOccupation() {
    if (m_Name && (m_ObjectFlags & CK_OBJECT_VISIBLE) == 0)
        return strlen(m_Name) + 21;
    else
        return 20;
}

CKBOOL CKObject::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return FALSE;
}

CKERROR CKObject::PrepareDependencies(CKDependenciesContext &context) {
    if (context.m_Mode == CK_DEPENDENCIES_SAVE && (m_ObjectFlags & CK_OBJECT_NOTTOBESAVED) != 0)
        return CKERR_INCOMPATIBLEPARAMETERS;

    CKObject *obj = (!context.m_DynamicObjects.IsEmpty()) ? context.m_DynamicObjects.Back() : nullptr;

    if (obj && obj->IsDynamic() && !IsDynamic())
        return CKERR_INCOMPATIBLEPARAMETERS;

    if (!context.AddDependencies(m_ID))
        return CKERR_ALREADYPRESENT;

    context.m_DynamicObjects.PushBack(this);

    return CK_OK;
}

CKERROR CKObject::RemapDependencies(CKDependenciesContext &context) {
    return CK_OK;
}

CKERROR CKObject::Copy(CKObject &o, CKDependenciesContext &context) {
    // TODO: May need fix
    m_ObjectFlags &= ~(CK_OBJECT_VISIBLE | CK_OBJECT_HIERACHICALHIDE);
    m_ObjectFlags |= o.m_ObjectFlags & (CK_OBJECT_VISIBLE | CK_OBJECT_HIERACHICALHIDE);

    CKDWORD dependencies = context.GetClassDependencies(CKObject::m_ClassID);
    CK_CLASSID cid = GetClassID();
    if (!(dependencies & CK_DEPENDENCIES_NONE)) {
        if (!context.m_CopyAppendString.Empty() && CKIsChildClassOf(cid, CKCID_BEOBJECT)) {
            XString name(o.GetName());
            name << context.m_CopyAppendString;
            SetName(name.Str());
        } else if (context.m_Dependencies && (context.m_Dependencies->m_Flags & CK_DEPENDENCIES_NONE)) {
            SetName(o.GetName());
        } else if ((m_ObjectFlags & CK_OBJECT_NAMESHARED) || cid == CKCID_PARAMETEROUT || cid == CKCID_PARAMETERIN) {
            SetName(o.GetName(), (o.m_ObjectFlags & CK_OBJECT_NAMESHARED) != 0);
        }
    } else {
        if ((CKGetClassDesc(cid)->DefaultOptions & 5) || !(dependencies & CK_DEPENDENCIES_FULL)) {
            if (!context.m_CopyAppendString.Empty() && CKIsChildClassOf(cid, CKCID_BEOBJECT)) {
                XString name(o.GetName());
                name << context.m_CopyAppendString;
                SetName(name.Str());
            } else {
                SetName(o.GetName(), (o.m_ObjectFlags & CK_OBJECT_NAMESHARED) != 0);
            }
        } else {
            XString secureName;
            if (!context.m_CopyAppendString.Empty()) {
                XString name(o.GetName());
                name << context.m_CopyAppendString;
                m_Context->GetObjectSecureName(secureName, name.Str(), cid);
            } else {
                m_Context->GetObjectSecureName(secureName, o.GetName(), cid);
            }
            SetName(secureName.Str());
        }
    }

    void *AppData = o.GetAppData();
    SetAppData(AppData);
    return CK_OK;
}

CKSTRING CKObject::GetClassName() {
    return "Basic Object";
}

int CKObject::GetDependenciesCount(int mode) {
    if (mode == CK_DEPENDENCIES_COPY)
        return 2;
    else if (mode == CK_DEPENDENCIES_SAVE)
        return 1;
    else
        return 0;
}

CKSTRING CKObject::GetDependencies(int i, int mode) {
    if (!i)
        return "Name";
    if (mode == 1 && i == 1)
        return "Unique Name";
    return nullptr;
}

void CKObject::Register() {
    CKPARAMETERFROMCLASS(CKObject, CKPGUID_OBJECT);
    CKCLASSDEFAULTCOPYDEPENDENCIES(CKObject, CK_DEPENDENCIES_COPY_OBJECT_NAME);
}

CKObject *CKObject::CreateInstance(CKContext *Context) {
    return new CKObject(Context);
}