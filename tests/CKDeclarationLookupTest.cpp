#include <gtest/gtest.h>
#include "CKAll.h"

TEST(CKDeclarationLookupTest, MissingGuidsDoNotInsertDeclarationsOrBreakPrototypeLookup) {
    ASSERT_EQ(CK_OK, CKStartUp());
    const int before = CKGetPrototypeDeclarationCount();
    for (unsigned int i = 0; i < 16; ++i) {
        const CKGUID missing(0x71A80000 + i, 0x52B90000 + i);
        EXPECT_EQ(nullptr, CKGetObjectDeclarationFromGuid(missing));
        EXPECT_EQ(before, CKGetPrototypeDeclarationCount());
        // The old lookup inserted a null entry; querying its prototype then
        // dereferenced it. Keep the failing baseline observable without a crash.
        if (CKGetPrototypeDeclarationCount() != before) break;
        EXPECT_EQ(nullptr, CKGetPrototypeFromGuid(missing));
        EXPECT_EQ(before, CKGetPrototypeDeclarationCount());
    }
    CKShutdown();
}
