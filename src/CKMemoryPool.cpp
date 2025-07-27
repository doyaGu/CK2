#include "CKMemoryPool.h"

CKMemoryPool::CKMemoryPool(CKContext *Context, int ByteCount) {
    m_Context = Context;
    m_Index = -1;
    if (Context) {
        m_Memory = m_Context->GetMemoryPoolGlobalIndex(ByteCount, m_Index);
    } else {
        m_Memory = nullptr;
    }
}

CKMemoryPool::~CKMemoryPool() {
    if (m_Context) {
        m_Context->ReleaseMemoryPoolGlobalIndex(m_Index);
    }
}

void *CKMemoryPool::Mem() const {
    return m_Memory;
}
