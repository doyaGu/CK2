#include <gtest/gtest.h>

#include <cstdio>

#include "CKAll.h"

namespace {

struct ParameterTypeCase {
    const char *name;
    CKGUID guid;
};

const ParameterTypeCase kPrimaryTypes[] = {
    {"int", CKPGUID_INT},
    {"float", CKPGUID_FLOAT},
    {"bool", CKPGUID_BOOL},
    {"string", CKPGUID_STRING},
};

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

void SetOutputSampleValue(CKBehavior *behavior, int index, const CKGUID &guid, int seed) {
    CKParameterOut *out = behavior->GetOutputParameter(index);
    ASSERT_NE(nullptr, out);

    if (guid == CKPGUID_INT) {
        int value = seed;
        ASSERT_EQ(CK_OK, out->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_FLOAT) {
        float value = static_cast<float>(seed) * 1.25f;
        ASSERT_EQ(CK_OK, out->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_BOOL) {
        CKBOOL value = (seed % 2) ? TRUE : FALSE;
        ASSERT_EQ(CK_OK, out->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_STRING) {
        char text[64] = {0};
        sprintf_s(text, "behavior_value_%d", seed);
        ASSERT_EQ(CK_OK, out->SetStringValue(text));
        return;
    }

    FAIL() << "Unsupported guid";
}

void AssertInputSampleValue(CKBehavior *behavior, int index, const CKGUID &guid, int seed) {
    CKParameterIn *in = behavior->GetInputParameter(index);
    ASSERT_NE(nullptr, in);
    CKParameter *src = in->GetRealSource();
    ASSERT_NE(nullptr, src);

    if (guid == CKPGUID_INT) {
        int value = 0;
        ASSERT_EQ(CK_OK, behavior->GetInputParameterValue(index, &value));
        EXPECT_EQ(seed, value);
        return;
    }

    if (guid == CKPGUID_FLOAT) {
        float value = 0.0f;
        ASSERT_EQ(CK_OK, behavior->GetInputParameterValue(index, &value));
        EXPECT_FLOAT_EQ(static_cast<float>(seed) * 1.25f, value);
        return;
    }

    if (guid == CKPGUID_BOOL) {
        CKBOOL value = FALSE;
        ASSERT_EQ(CK_OK, behavior->GetInputParameterValue(index, &value));
        EXPECT_EQ((seed % 2) ? TRUE : FALSE, value);
        return;
    }

    if (guid == CKPGUID_STRING) {
        char text[128] = {0};
        ASSERT_GT(src->GetStringValue(text, TRUE), 0);
        char expected[64] = {0};
        sprintf_s(expected, "behavior_value_%d", seed);
        EXPECT_STREQ(expected, text);
        return;
    }

    FAIL() << "Unsupported guid";
}

} // namespace

TEST_F(CKRuntimeFixture, IoAndParameterCreationMatrixAcrossTypes) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "MatrixBehavior", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        const int inputIoIndex = behavior->AddInput(const_cast<char *>(tc.name));
        const int outputIoIndex = behavior->AddOutput(const_cast<char *>(tc.name));
        ASSERT_GE(inputIoIndex, 0);
        ASSERT_GE(outputIoIndex, 0);

        CKParameterIn *in = behavior->CreateInputParameter("in", tc.guid);
        CKParameterOut *out = behavior->CreateOutputParameter("out", tc.guid);

        ASSERT_NE(nullptr, in) << tc.name;
        ASSERT_NE(nullptr, out) << tc.name;
        EXPECT_EQ(behavior, in->GetOwner()) << tc.name;
        EXPECT_EQ(behavior, out->GetOwner()) << tc.name;

        ASSERT_EQ(CK_OK, in->SetDirectSource(out)) << tc.name;
        SetOutputSampleValue(behavior, i, tc.guid, 1000 + i);
        AssertInputSampleValue(behavior, i, tc.guid, 1000 + i);

        EXPECT_EQ(i, behavior->GetInputParameterPosition(in));
        EXPECT_EQ(i, behavior->GetOutputParameterPosition(out));
    }

    EXPECT_EQ(static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])), behavior->GetInputCount());
    EXPECT_EQ(static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])), behavior->GetOutputCount());
    EXPECT_EQ(static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])), behavior->GetInputParameterCount());
    EXPECT_EQ(static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])), behavior->GetOutputParameterCount());
}

TEST_F(CKRuntimeFixture, EnableDisableParameterMatrixAcrossTypes) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "EnableBehavior", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];
        ASSERT_NE(nullptr, behavior->CreateInputParameter("inEnabled", tc.guid));
        ASSERT_NE(nullptr, behavior->CreateOutputParameter("outEnabled", tc.guid));

        behavior->EnableInputParameter(i, FALSE);
        behavior->EnableOutputParameter(i, FALSE);
        EXPECT_FALSE(behavior->IsInputParameterEnabled(i)) << tc.name;
        EXPECT_FALSE(behavior->IsOutputParameterEnabled(i)) << tc.name;

        behavior->EnableInputParameter(i, TRUE);
        behavior->EnableOutputParameter(i, TRUE);
        EXPECT_TRUE(behavior->IsInputParameterEnabled(i)) << tc.name;
        EXPECT_TRUE(behavior->IsOutputParameterEnabled(i)) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, InvalidIndexGuardsForIoAndParameters) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "GuardBehavior", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    int intValue = 0;
    EXPECT_EQ(CKERR_INVALIDPARAMETER, behavior->GetInputParameterValue(-1, &intValue));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, behavior->GetOutputParameterValue(0, &intValue));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, behavior->SetOutputParameterValue(0, &intValue, sizeof(intValue)));

    EXPECT_EQ(nullptr, behavior->GetInput(-1));
    EXPECT_EQ(nullptr, behavior->GetOutput(-1));
    EXPECT_EQ(nullptr, behavior->RemoveInput(-1));
    EXPECT_EQ(nullptr, behavior->RemoveOutput(-1));
}

TEST_F(CKRuntimeFixture, OutputObjectBridgeRoundTrip) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "ObjectBridgeBehavior", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    CKParameterOut *objectOut = behavior->CreateOutputParameter("objectOut", CKPGUID_OBJECT);
    ASSERT_NE(nullptr, objectOut);

    CKObject *target = context_->CreateObject(CKCID_OBJECT, "TargetObj", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, target);

    ASSERT_EQ(CK_OK, behavior->SetOutputParameterObject(0, target));
    EXPECT_EQ(target, behavior->GetOutputParameterObject(0));

    ASSERT_EQ(CK_OK, behavior->SetOutputParameterObject(0, nullptr));
    EXPECT_EQ(nullptr, behavior->GetOutputParameterObject(0));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
