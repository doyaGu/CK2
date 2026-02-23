#include <gtest/gtest.h>

#include <cstdint>

#include "CKAll.h"

namespace {

class CKRuntimeFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context_, nullptr, 0, 0));
        ASSERT_NE(nullptr, context_);
    }

    static void TearDownTestSuite() {
        if (context_) {
            CKCloseContext(context_);
            context_ = nullptr;
        }
        CKShutdown();
    }

    static CKContext *context_;
};

CKContext *CKRuntimeFixture::context_ = nullptr;

} // namespace

TEST_F(CKRuntimeFixture, SetElementValueFromParameterClearsUpperBitsForObjectColumn) {
    if (sizeof(CKUINTPTR) <= sizeof(CKDWORD)) {
        GTEST_SKIP() << "64-bit specific regression";
    }

    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_OBJECT, "ObjectRef");
    array->AddRow();

    CKUINTPTR *element = array->GetElement(0, 0);
    ASSERT_NE(nullptr, element);
#if UINTPTR_MAX > UINT32_MAX
    *element = (static_cast<CKUINTPTR>(0x12345678u) << 32) | static_cast<CKUINTPTR>(0xFFFFFFFFu);
#else
    *element = static_cast<CKUINTPTR>(0xFFFFFFFFu);
#endif

    CKObject *target = context_->CreateObject(CKCID_OBJECT, "TargetObject", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, target);
    const CK_ID targetId = target->GetID();

    CKParameterOut *objectParam = context_->CreateCKParameterOut("ObjectParam", CKPGUID_OBJECT, TRUE);
    ASSERT_NE(nullptr, objectParam);
    ASSERT_EQ(CK_OK, objectParam->SetValue((void *)&targetId));

    ASSERT_TRUE(array->SetElementValueFromParameter(0, 0, objectParam));

    EXPECT_EQ(static_cast<CKUINTPTR>(targetId), *element);
    EXPECT_EQ(target, array->GetElementObject(0, 0));
    EXPECT_TRUE(array->IsObjectUsed(target, CKCID_OBJECT));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
