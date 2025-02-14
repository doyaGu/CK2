#include "CKBehaviorLink.h"

#include "CKBehavior.h"
#include "CKBehaviorIO.h"
#include "CKStateChunk.h"

CK_CLASSID CKBehaviorLink::m_ClassID = CKCID_BEHAVIORLINK;

CKERROR CKBehaviorLink::SetInBehaviorIO(CKBehaviorIO *ckbioin) {
    if (!ckbioin)
        return CKERR_INVALIDPARAMETER;

    if (m_InIO) {
        XSObjectPointerArray &links = m_InIO->m_Links;
        for (auto it = links.Begin(); it != links.End(); ++it) {
            if (*it == this) {
                links.Remove(*it);
                break;
            }
        }
    }
    m_InIO = ckbioin;
    m_InIO->m_Links.AddIfNotHere(this);
    return CK_OK;
}

CKERROR CKBehaviorLink::SetOutBehaviorIO(CKBehaviorIO *ckbioout) {
    if (!ckbioout)
        return CKERR_INVALIDPARAMETER;

    m_InIO = ckbioout;
    return CK_OK;
}

CKBehaviorLink::CKBehaviorLink(CKContext *Context, CKSTRING name) : CKObject(Context, name) {
    m_OldFlags = 0;
    m_InIO = nullptr;
    m_OutIO = nullptr;
    m_ActivationDelay = 1;
    m_InitialActivationDelay = 1;
}

CKBehaviorLink::~CKBehaviorLink() {
}

CK_CLASSID CKBehaviorLink::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKBehaviorLink::Save(CKFile *file, CKDWORD flags) {
    CKStateChunk *baseChunk = CKObject::Save(file, flags);
    if (file || (flags & CK_STATESAVE_BEHAV_LINKONLY) != 0) {
        CKStateChunk *chunk = new CKStateChunk(CKCID_BEHAVIORLINK, file);
        chunk->StartWrite();
        chunk->AddChunkAndDelete(baseChunk);
        chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_NEWDATA);
        chunk->WriteInt(m_ActivationDelay);
        chunk->WriteObject(m_InIO);
        chunk->WriteObject(m_OutIO);

        if (GetClassID() == CKCID_BEHAVIORLINK) {
            chunk->CloseChunk();
        } else {
            chunk->UpdateDataSize();
        }

        return chunk;
    }
    return baseChunk;
}

CKERROR CKBehaviorLink::Load(CKStateChunk *chunk, CKFile *file) {
    if (!chunk)
        return CKERR_INVALIDPARAMETER;

    CKObject::Load(chunk, file);

    if (m_InIO) {
        for (auto it = m_InIO->m_Links.Begin(); it != m_InIO->m_Links.End(); ++it) {
            if (*it == this) {
                m_InIO->m_Links.Remove(*it);
                break;
            }
        }
    }

    if (chunk->SeekIdentifier(CK_STATESAVE_BEHAV_LINK_NEWDATA)) {
        // New format loading
        m_ActivationDelay = chunk->ReadInt();
        m_InIO = (CKBehaviorIO *)chunk->ReadObject(m_Context);
        m_OutIO = (CKBehaviorIO *)chunk->ReadObject(m_Context);
    } else {
        // Legacy format support
        if (chunk->SeekIdentifier(CK_STATESAVE_BEHAV_LINK_CURDELAY)) {
            m_ActivationDelay = chunk->ReadInt();
        }
        if (chunk->SeekIdentifier(CK_STATESAVE_BEHAV_LINK_IOS)) {
            m_InIO = (CKBehaviorIO *)chunk->ReadObject(m_Context);
            m_OutIO = (CKBehaviorIO *)chunk->ReadObject(m_Context);
        }
        if (chunk->SeekIdentifier(CK_STATESAVE_BEHAV_LINK_DELAY)) {
            m_InitialActivationDelay = chunk->ReadInt();
        }
    }

    return CK_OK;
}

void CKBehaviorLink::PostLoad() {
    if (m_InIO)
        m_InIO->m_Links.AddIfNotHere(this);
    CKObject::PostLoad();
}

void CKBehaviorLink::PreDelete() {
    if (m_InIO) {
        if (m_InIO->IsToBeDeleted()) {
            XSObjectPointerArray &links = m_InIO->m_Links;
            for (auto it = links.Begin(); it != links.End(); ++it) {
                if (*it == this) {
                    links.Remove(*it);
                    break;
                }
            }
        }
        CKBehavior *owner = m_InIO->GetOwner();
        if (owner && owner->IsToBeDeleted()) {
            if (owner->m_GraphData) {
                owner->m_GraphData->m_SubBehaviorLinks.Remove(this);
            }
        }
    }
}

int CKBehaviorLink::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation() + 16;
}

CKBOOL CKBehaviorLink::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    if (obj == m_InIO || obj == m_OutIO)
        return TRUE;
    return CKObject::IsObjectUsed(obj, cid);
}

CKERROR CKBehaviorLink::RemapDependencies(CKDependenciesContext &context) {
    CKERROR err = CKObject::RemapDependencies(context);
    if (err != CK_OK)
        return err;

    m_InIO = (CKBehaviorIO *)context.Remap(m_InIO);
    m_OutIO = (CKBehaviorIO *)context.Remap(m_OutIO);
    if (m_InIO)
        m_InIO->m_Links.AddIfNotHere(this);

    return CK_OK;
}

CKERROR CKBehaviorLink::Copy(CKObject &o, CKDependenciesContext &context) {
    CKERROR err = CKObject::Copy(o, context);
    if (err != CK_OK)
        return err;

    CKBehaviorLink *link = (CKBehaviorLink *)&o;
    m_OldFlags = link->m_OldFlags & ~1;
    m_ActivationDelay = link->m_ActivationDelay;
    m_InitialActivationDelay = link->m_InitialActivationDelay;
    m_InIO = link->m_InIO;
    m_OutIO = link->m_OutIO;
    return CK_OK;
}

CKSTRING CKBehaviorLink::GetClassName() {
    return "Behavior Link";
}

int CKBehaviorLink::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKBehaviorLink::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKBehaviorLink::Register() {
    CKClassRegisterDefaultOptions(m_ClassID, 1);
}

CKBehaviorLink *CKBehaviorLink::CreateInstance(CKContext *Context) {
    return new CKBehaviorLink(Context, nullptr);
}
