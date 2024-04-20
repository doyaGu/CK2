#ifndef CKMEMORYPOOL_H
#define CKMEMORYPOOL_H

#include "CKContext.h"
#include "VxMemoryPool.h"

class DLL_EXPORT CKMemoryPool
{
public:
    CKMemoryPool(CKContext *Context, int ByteCount = 0);
    ~CKMemoryPool();

    //CKObject &operator=(const CKObject &);

    VxMemoryPool *Mem() const;

protected:
    CKContext *m_Context;
    VxMemoryPool *m_Memory;
    int m_Index;
};

#endif // CKMEMORYPOOL_H