#include "CKGlobals.h"
#include "CKBitmapReader.h"

CKBitmapProperties *CKCopyBitmapProperties(CKBitmapProperties *bp) {
    if (!bp)
        return nullptr;
    if (bp->m_Size <= 0 || bp->m_Size >= 256)
        return nullptr;
    CKBitmapProperties *nbp = (CKBitmapProperties *) new CKBYTE[bp->m_Size];
    memcpy(nbp, bp, bp->m_Size);
    return nbp;
}

void CKDeletePointer(void *ptr) {
    delete[] static_cast<CKBYTE *>(ptr);
}
