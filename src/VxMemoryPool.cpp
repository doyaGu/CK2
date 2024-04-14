#include "VxMemoryPool.h"

#include <stdlib.h>

void *VxNewAligned(int size, int align) {
    return _aligned_malloc(size, align);
}

void VxDeleteAligned(void *ptr) {
    _aligned_free(ptr);
}