#include <gtest/gtest.h>

#include "CKAll.h"

TEST(CKGuidComparisonTest, EqualityAndInequality) {
    const CKGUID left(0x11223344u, 0x55667788u);
    const CKGUID same(0x11223344u, 0x55667788u);
    const CKGUID different(0x11223344u, 0x55667789u);

    EXPECT_TRUE(left == same);
    EXPECT_FALSE(left != same);

    EXPECT_TRUE(left != different);
    EXPECT_FALSE(left == different);
}

TEST(CKGuidComparisonTest, OrdersByFirstDwordBeforeSecondDword) {
    const CKGUID smallerFirst(0x00000010u, 0xFFFFFFFFu);
    const CKGUID largerFirst(0x00000020u, 0x00000000u);

    EXPECT_TRUE(smallerFirst < largerFirst);
    EXPECT_TRUE(largerFirst > smallerFirst);
    EXPECT_TRUE(smallerFirst <= largerFirst);
    EXPECT_TRUE(largerFirst >= smallerFirst);
}

TEST(CKGuidComparisonTest, OrdersBySecondDwordWhenFirstIsEqual) {
    const CKGUID smallerSecond(0xAABBCCDDu, 0x00000001u);
    const CKGUID largerSecond(0xAABBCCDDu, 0x00000002u);

    EXPECT_TRUE(smallerSecond < largerSecond);
    EXPECT_TRUE(smallerSecond <= largerSecond);
    EXPECT_TRUE(largerSecond > smallerSecond);
    EXPECT_TRUE(largerSecond >= smallerSecond);
}

TEST(CKGuidComparisonTest, IsValidReflectsNonZeroContent) {
    const CKGUID zeroGuid(0u, 0u);
    const CKGUID firstNonZero(1u, 0u);
    const CKGUID secondNonZero(0u, 1u);

    EXPECT_FALSE(zeroGuid.IsValid());
    EXPECT_TRUE(firstNonZero.IsValid());
    EXPECT_TRUE(secondNonZero.IsValid());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
