#include "CKBehaviorIO.h"

#include "CKBehavior.h"
#include "CKBehaviorLink.h"
#include "CKStateChunk.h"

CK_CLASSID CKBehaviorIO::m_ClassID = CKCID_BEHAVIORIO;

CKBehaviorIO::CKBehaviorIO(CKContext *Context, CKSTRING name) : CKObject(Context, name) {
    m_OwnerBehavior = nullptr;
}

CKBehaviorIO::~CKBehaviorIO() {
    m_Links.Clear();
}

CK_CLASSID CKBehaviorIO::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKBehaviorIO::Save(CKFile *file, CKDWORD flags) {
    CKStateChunk *chunk = CKObject::Save(file, flags);
    if (file || (flags & CK_STATESAVE_BEHAVIOONLY) != 0) {
        CKStateChunk *ioChunk = new CKStateChunk(CKCID_BEHAVIORIO, file);
        ioChunk->StartWrite();
        ioChunk->AddChunkAndDelete(chunk);

        ioChunk->WriteIdentifier(CK_STATESAVE_BEHAV_IOFLAGS);
        ioChunk->WriteDword(GetOldFlags());

        if (GetClassID() == CKCID_BEHAVIORIO) {
            ioChunk->CloseChunk();
        } else {
            ioChunk->UpdateDataSize();
        }
        return ioChunk;
    }
    return chunk;
}

CKERROR CKBehaviorIO::Load(CKStateChunk *chunk, CKFile *file) {
    if (!chunk)
        return CKERR_INVALIDPARAMETER;
    CKObject::Load(chunk, file);

    if (chunk->SeekIdentifier(CK_STATESAVE_BEHAV_IOFLAGS)) {
        SetOldFlags(chunk->ReadInt());
    }
    return CK_OK;
}

void CKBehaviorIO::PreDelete() {
    CKBehavior *owner = m_OwnerBehavior;
    if (owner && !owner->IsToBeDeleted()) {
        XSObjectPointerArray &array = (m_ObjectFlags & CK_BEHAVIORIO_IN) ? owner->m_InputArray : owner->m_OutputArray;

        for (int i = 0; i < array.Size(); ++i) {
            if (array[i] == this) {
                array.RemoveAt(i);
                break;
            }
        }

        BehaviorGraphData *graph = owner->m_GraphData;
        if (graph) {
            for (int i = 0; i < graph->m_SubBehaviorLinks.Size(); ++i) {
                CKBehaviorLink *link = (CKBehaviorLink *)graph->m_SubBehaviorLinks[i];
                if (link) {
                    if (link->m_InIO == this) link->m_InIO = NULL;
                    if (link->m_OutIO == this) link->m_OutIO = NULL;
                }
            }
        }

        CKBehavior *parent = owner->GetParent();
        if (parent) {
            BehaviorGraphData *parentGraph = parent->m_GraphData;
            if (parentGraph) {
                for (int i = 0; i < parentGraph->m_SubBehaviorLinks.Size(); ++i) {
                    CKBehaviorLink *link = (CKBehaviorLink *)parentGraph->m_SubBehaviorLinks[i];
                    if (link) {
                        if (link->m_InIO == this) link->m_InIO = NULL;
                        if (link->m_OutIO == this) link->m_OutIO = NULL;
                    }
                }
            }
        }
    }
}

int CKBehaviorIO::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation() + 12;
}

CKERROR CKBehaviorIO::RemapDependencies(CKDependenciesContext &context) {
    CKERROR err = CKObject::RemapDependencies(context);
    if (err != CK_OK)
        return err;

    m_OwnerBehavior = (CKBehavior *)context.Remap(m_OwnerBehavior);
    return CK_OK;
}

CKERROR CKBehaviorIO::Copy(CKObject &o, CKDependenciesContext &context) {
    CKERROR err = CKObject::Copy(o, context);
    if (err != CK_OK)
        return err;

    m_ObjectFlags = (m_ObjectFlags & ~CK_OBJECT_IOMASK) | (o.m_ObjectFlags & CK_OBJECT_IOMASK);

    CKBehaviorIO *io = (CKBehaviorIO *)&o;
    m_OwnerBehavior = io->m_OwnerBehavior;
    return CK_OK;
}

CKSTRING CKBehaviorIO::GetClassName() {
    return "Behavior IO";
}

int CKBehaviorIO::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKBehaviorIO::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKBehaviorIO::Register() {
    CKClassRegisterDefaultOptions(m_ClassID, 1);
}

CKBehaviorIO *CKBehaviorIO::CreateInstance(CKContext *Context) {
    return new CKBehaviorIO(Context);
}

void CKBehaviorIO::SetOldFlags(CKDWORD flags) {
    m_ObjectFlags &= ~CK_OBJECT_IOMASK;
    if (flags & 0x01) m_ObjectFlags |= CK_BEHAVIORIO_IN;
    if (flags & 0x02) m_ObjectFlags |= CK_BEHAVIORIO_OUT;
    if (flags & 0x100) m_ObjectFlags |= CK_BEHAVIORIO_ACTIVE;
}

CKDWORD CKBehaviorIO::GetOldFlags() {
    CKDWORD flags = 0;
    if (m_ObjectFlags & CK_BEHAVIORIO_IN) flags |= 0x01;
    if (m_ObjectFlags & CK_BEHAVIORIO_OUT) flags |= 0x02;
    return flags;
}
