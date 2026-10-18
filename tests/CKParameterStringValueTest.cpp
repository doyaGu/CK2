#include <gtest/gtest.h>
#include "CKAll.h"
#include <cstring>

class CKParameterStringValueTest : public ::testing::Test {
protected:
    CKContext *context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    void TearDown() override {
        if (context) CKCloseContext(context);
        CKShutdown();
    }
};

TEST_F(CKParameterStringValueTest, VectorAndColorTextPreservesEveryComponent) {
    const CKGUID types[] = {CKPGUID_2DVECTOR, CKPGUID_VECTOR, CKPGUID_COLOR};
    const char *texts[] = {"1.25,-2.5", "1.25,-2.5,3.75", "255,128,0,64"};
    const float expected[][4] = {{1.25f, -2.5f}, {1.25f, -2.5f, 3.75f},
        {255 * 0.0039215689f, 128 * 0.0039215689f, 0, 64 * 0.0039215689f}};
    for (int i = 0; i < 3; ++i) {
        CKParameterOut *parameter = context->CreateCKParameterOut("Components", types[i]);
        ASSERT_NE(nullptr, parameter);
        ASSERT_EQ(CK_OK, parameter->SetStringValue(const_cast<char *>(texts[i])));
        float values[4] = {};
        ASSERT_EQ(CK_OK, parameter->GetValue(values, FALSE));
        EXPECT_EQ(0, std::memcmp(values, expected[i], parameter->GetDataSize()));
        char text[64] = {};
        ASSERT_GT(parameter->GetStringValue(text, FALSE), 0);
        EXPECT_STREQ(texts[i], text);
        ASSERT_EQ(CK_OK, parameter->SetStringValue(text));
        float roundTrip[4] = {};
        ASSERT_EQ(CK_OK, parameter->GetValue(roundTrip, FALSE));
        EXPECT_EQ(0, std::memcmp(values, roundTrip, parameter->GetDataSize()));
    }
}

TEST_F(CKParameterStringValueTest, OversizedComponentStorageDoesNotOverwriteTextOutput) {
    const CKGUID types[] = {CKPGUID_2DVECTOR, CKPGUID_VECTOR, CKPGUID_COLOR};
    float oversized[8] = {};
    for (int i = 0; i < 3; ++i) {
        CKParameterOut *parameter = context->CreateCKParameterOut("Oversized", types[i]);
        ASSERT_NE(nullptr, parameter);
        ASSERT_EQ(CK_OK, parameter->SetValue(oversized, sizeof(oversized)));
        char text[] = "unchanged";
        EXPECT_EQ(0, parameter->GetStringValue(text, FALSE));
        EXPECT_STREQ("unchanged", text);
    }
}
