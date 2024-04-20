#include "XObjectArray.h"

#include "CKStateChunk.h"

CK_ID XObjectPointerArray::GetObjectID(unsigned int i) const {
    if (i >= Size())
        return 0;
    return m_Begin[i]->m_ID;
}

CKBOOL XObjectPointerArray::Check() {
    CKObject **it = m_Begin;
    int size = Size();
    int i;
    for (i = 0; i < size; ++i) {
        CKObject *obj = *it++;
        if (obj->m_ObjectFlags & CK_OBJECT_TOBEDELETED)
            break;
    }

    if (i + 1 < size) {
        for (int j = size - 1; j > 0; --j) {
            if (((*it)->m_ObjectFlags & CK_OBJECT_TOBEDELETED) == 0)
                m_Begin[i++] = *it;
            ++it;
        }
    }

    m_End = &m_Begin[i];
    return i != size;
}

void XObjectPointerArray::Load(CKContext *Context, CKStateChunk *chunk) {
    *this = chunk->ReadXObjectArray(Context);
}

void XObjectPointerArray::Save(CKStateChunk *chunk) const {
    chunk->StartObjectIDSequence(Size());
    for (CKObject **it = m_Begin; it != m_End; ++it) {
       chunk->WriteObjectSequence(*it);
    }
}

void XObjectPointerArray::Prepare(CKDependenciesContext &context) const {
    for (CKObject **it = m_Begin; it != m_End; ++it) {
        CKObject *obj = *it;
        if (obj)
            obj->PrepareDependencies(context);
    }
}

void XObjectPointerArray::Remap(CKDependenciesContext &context) {
    for (CKObject **it = m_Begin; it != m_End; ++it) {
        if (*it)
            *it = context.Remap(*it);
    }
}

CKBOOL XSObjectArray::Check(CKContext* Context)
{
    return CKBOOL();
}

CKBOOL XSObjectArray::RemoveObject(CKObject* obj)
{
    return CKBOOL();
}

void XSObjectArray::Save(CKStateChunk* chunk, CKContext* ctx) const
{
}

CKBOOL XObjectArray::Check(CKContext* Context)
{
    return CKBOOL();
}

CKObject* XObjectArray::GetObject(CKContext* Context, unsigned int i) const
{
    return nullptr;
}

CKBOOL XObjectArray::RemoveObject(CKObject* obj)
{
    return CKBOOL();
}

CKBOOL XObjectArray::FindObject(CKObject* obj) const
{
    return CKBOOL();
}

void XObjectArray::Load(CKStateChunk* chunk)
{
}

void XObjectArray::Save(CKStateChunk* chunk, CKContext* ctx) const
{
}

void XObjectArray::Prepare(CKDependenciesContext& context) const
{
}

void XObjectArray::Remap(CKDependenciesContext& context)
{
}

CKBOOL XSObjectPointerArray::Check()
{
    return CKBOOL();
}

void XSObjectPointerArray::Load(CKContext* Context, CKStateChunk* chunk)
{
}

void XSObjectPointerArray::Save(CKStateChunk* chunk) const
{
}

void XSObjectPointerArray::Prepare(CKDependenciesContext& context) const
{
}

void XSObjectPointerArray::Remap(CKDependenciesContext& context)
{
}
