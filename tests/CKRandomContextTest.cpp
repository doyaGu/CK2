#include <gtest/gtest.h>
#include "CKAll.h"
#include <cstdlib>
#include <thread>
#include <vector>

namespace {
class CKRandomContextTest : public ::testing::Test {
protected:
    CKContext *First = nullptr, *Second = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&First, nullptr, 0, 0));
        ASSERT_EQ(CK_OK, CKCreateContext(&Second, nullptr, 0, 0));
    }
    void TearDown() override {
        if (First) CKCloseContext(First);
        if (Second) CKCloseContext(Second);
        CKShutdown();
    }
};

TEST_F(CKRandomContextTest, SeedOneMatchesTheLegacyWindowsSequence) {
    const int expected[] = {41, 18467, 6334, 26500, 19169, 15724, 11478, 29358};
    First->SetRandomSeed(1);
    for (int value : expected) EXPECT_EQ(value, First->Random());
    First->SetRandomSeed(1);
    for (int value : expected) EXPECT_EQ(value, First->Random());
    EXPECT_EQ(32767, CK_RANDOM_MAX);
}

TEST_F(CKRandomContextTest, InterleavingAndReseedingNeverAdvanceAnotherContext) {
    First->SetRandomSeed(1); Second->SetRandomSeed(1);
    EXPECT_EQ(41, First->Random());
    for (int i = 0; i < 100; ++i) Second->Random();
    EXPECT_EQ(18467, First->Random());
    Second->SetRandomSeed(10);
    EXPECT_EQ(6334, First->Random());
    First->Reset();
    EXPECT_EQ(26500, First->Random());
}

TEST_F(CKRandomContextTest, IndependentThreadsReplaySeedsIncludingZeroAndAllBitsSet) {
    const CKDWORD seeds[] = {0, 1, 10, 0xffffffffu};
    for (CKDWORD seed : seeds) {
        First->SetRandomSeed(seed); Second->SetRandomSeed(seed);
        std::vector<int> expected(1024), first(1024), second(1024);
        for (int &value : expected) value = First->Random();
        First->SetRandomSeed(seed);
        std::thread a([&] { for (int &value : first) value = First->Random(); });
        std::thread b([&] { for (int &value : second) value = Second->Random(); });
        a.join(); b.join();
        EXPECT_EQ(expected, first); EXPECT_EQ(expected, second);
        for (int value : first) { EXPECT_GE(value, 0); EXPECT_LE(value, CK_RANDOM_MAX); }
    }
}

TEST_F(CKRandomContextTest, ShuffleRetainsTheLegacySwapAlgorithmAndDrawCount) {
    auto *array = static_cast<CKDataArray *>(First->CreateObject(CKCID_DATAARRAY, (CKSTRING)"random rows"));
    ASSERT_NE(nullptr, array);
    array->InsertColumn(-1, CKARRAYTYPE_INT, (CKSTRING)"value");
    for (int i = 0; i < 5; ++i) { array->AddRow(); array->SetElementValue(i, 0, &i); }
    First->SetRandomSeed(1); Second->SetRandomSeed(1);
    array->RandomShuffle();
    const int expected[] = {3, 2, 4, 1, 0};
    for (int i = 0; i < 5; ++i) {
        int value = -1;
        ASSERT_TRUE(array->GetElementValue(i, 0, &value));
        EXPECT_EQ(expected[i], value);
    }
    EXPECT_EQ(15724, First->Random());
    EXPECT_EQ(41, Second->Random());
}

TEST_F(CKRandomContextTest, GuidGenerationConsumesOnlyItsOwnersSequence) {
    First->SetRandomSeed(1); Second->SetRandomSeed(1);
    EXPECT_NE(CKGUID(), First->GetSecureGuid());
    EXPECT_EQ(19169, First->Random());
    EXPECT_EQ(41, Second->Random());
}

TEST_F(CKRandomContextTest, EqualDefaultSeedsStillHaveDistinctScratchDirectories) {
    EXPECT_NE(First->GetPathManager()->GetVirtoolsTemporaryFolder(),
              Second->GetPathManager()->GetVirtoolsTemporaryFolder());
}

TEST(CKRandomHostTest, StartupContextCreationAndSeededDrawsLeaveTheHostCrtStreamAlone) {
    std::srand(8971);
    const int expected = std::rand();
    std::srand(8971);
    ASSERT_EQ(CK_OK, CKStartUp());
    CKContext *context = nullptr;
    ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    context->SetRandomSeed(10);
    for (int i = 0; i < 50; ++i) context->Random();
    CKCloseContext(context);
    CKShutdown();
    EXPECT_EQ(expected, std::rand());
}
} // namespace
