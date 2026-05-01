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

void EmptyOperation(CKContext *, CKParameterOut *, CKParameterIn *, CKParameterIn *) {
}

void SetSampleValue(CKParameterOut *param, const CKGUID &guid, int seed) {
    if (guid == CKPGUID_INT) {
        int value = seed;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }
    if (guid == CKPGUID_FLOAT) {
        float value = static_cast<float>(seed) * 2.25f;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }
    if (guid == CKPGUID_BOOL) {
        CKBOOL value = (seed % 2) ? TRUE : FALSE;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }
    if (guid == CKPGUID_STRING) {
        char text[64] = {0};
        sprintf_s(text, "dest_value_%d", seed);
        ASSERT_EQ(CK_OK, param->SetStringValue(text));
        return;
    }
    FAIL() << "Unsupported guid";
}

void AssertSampleValue(CKParameterOut *param, const CKGUID &guid, int seed) {
    if (guid == CKPGUID_INT) {
        int value = 0;
        ASSERT_EQ(CK_OK, param->GetValue(&value, FALSE));
        EXPECT_EQ(seed, value);
        return;
    }
    if (guid == CKPGUID_FLOAT) {
        float value = 0.0f;
        ASSERT_EQ(CK_OK, param->GetValue(&value, FALSE));
        EXPECT_FLOAT_EQ(static_cast<float>(seed) * 2.25f, value);
        return;
    }
    if (guid == CKPGUID_BOOL) {
        CKBOOL value = FALSE;
        ASSERT_EQ(CK_OK, param->GetValue(&value, FALSE));
        EXPECT_EQ((seed % 2) ? TRUE : FALSE, value);
        return;
    }
    if (guid == CKPGUID_STRING) {
        char text[128] = {0};
        ASSERT_GT(param->GetStringValue(text, FALSE), 0);
        char expected[64] = {0};
        sprintf_s(expected, "dest_value_%d", seed);
        EXPECT_STREQ(expected, text);
        return;
    }
    FAIL() << "Unsupported guid";
}

} // namespace

TEST_F(CKRuntimeFixture, DataChangedPropagatesAcrossPrimaryTypeMatrix) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        CKParameterOut *source = context_->CreateCKParameterOut("source", tc.guid, TRUE);
        CKParameterOut *dest = context_->CreateCKParameterOut("dest", tc.guid, TRUE);

        ASSERT_NE(nullptr, source);
        ASSERT_NE(nullptr, dest);

        ASSERT_EQ(CK_OK, source->AddDestination(dest, TRUE)) << tc.name;
        EXPECT_EQ(1, source->GetDestinationCount()) << tc.name;

        SetSampleValue(source, tc.guid, 200 + i);
        AssertSampleValue(dest, tc.guid, 200 + i);
    }
}

TEST_F(CKRuntimeFixture, AddDestinationGuardsInvalidAlreadyPresentAndIncompatible) {
    CKParameterOut *intSource = context_->CreateCKParameterOut("intSource", CKPGUID_INT, TRUE);
    CKParameterOut *intDest = context_->CreateCKParameterOut("intDest", CKPGUID_INT, TRUE);
    CKParameterOut *strDest = context_->CreateCKParameterOut("strDest", CKPGUID_STRING, TRUE);

    ASSERT_NE(nullptr, intSource);
    ASSERT_NE(nullptr, intDest);
    ASSERT_NE(nullptr, strDest);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, intSource->AddDestination(nullptr, TRUE));
    EXPECT_EQ(CK_OK, intSource->AddDestination(intDest, TRUE));
    EXPECT_EQ(CKERR_ALREADYPRESENT, intSource->AddDestination(intDest, TRUE));
    EXPECT_EQ(CKERR_INCOMPATIBLEPARAMETERS, intSource->AddDestination(strDest, TRUE));
}

TEST_F(CKRuntimeFixture, AddDestinationGuardsSimpleCyclesBetweenOutputs) {
    CKParameterOut *outA = context_->CreateCKParameterOut("outA", CKPGUID_INT, TRUE);
    CKParameterOut *outB = context_->CreateCKParameterOut("outB", CKPGUID_INT, TRUE);

    ASSERT_NE(nullptr, outA);
    ASSERT_NE(nullptr, outB);

    ASSERT_EQ(CK_OK, outA->AddDestination(outB, TRUE));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, outB->AddDestination(outA, TRUE));
}

TEST_F(CKRuntimeFixture, RemoveDestinationAndClearUpdateCounts) {
    CKParameterOut *source = context_->CreateCKParameterOut("source", CKPGUID_INT, TRUE);
    CKParameterOut *dest1 = context_->CreateCKParameterOut("dest1", CKPGUID_INT, TRUE);
    CKParameterOut *dest2 = context_->CreateCKParameterOut("dest2", CKPGUID_INT, TRUE);

    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, dest1);
    ASSERT_NE(nullptr, dest2);

    ASSERT_EQ(CK_OK, source->AddDestination(dest1, TRUE));
    ASSERT_EQ(CK_OK, source->AddDestination(dest2, TRUE));
    EXPECT_EQ(2, source->GetDestinationCount());

    source->RemoveDestination(dest1);
    EXPECT_EQ(1, source->GetDestinationCount());

    source->RemoveAllDestinations();
    EXPECT_EQ(0, source->GetDestinationCount());
}

TEST_F(CKRuntimeFixture, DestroyingBehaviorOwnedOutputRemovesItFromNonScriptGraph) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "OutOwnerGraph", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    behavior->UseGraph();

    CKParameterOut *out = behavior->CreateOutputParameter("ownedOut", CKPGUID_INT);
    ASSERT_NE(nullptr, out);
    ASSERT_EQ(1, behavior->GetOutputParameterCount());
    EXPECT_EQ(out, behavior->GetOutputParameter(0));

    ASSERT_EQ(CK_OK, context_->DestroyObject(out));
    EXPECT_EQ(0, behavior->GetOutputParameterCount());
    EXPECT_EQ(nullptr, behavior->GetOutputParameter(0));
}

TEST_F(CKRuntimeFixture, DestroyingOperationOutputClearsOwnerPointerInNonScriptGraph) {
    CKBehavior *behavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "OpOwnerGraph", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, behavior);

    behavior->UseGraph();

    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x1F3A52B1u, 0x6C2D0E47u);
    pm->RegisterOperationType(opGuid, "ParameterOutCleanupOperation");

    CKGUID resGuid = CKPGUID_INT;
    CKGUID inGuid = CKPGUID_INT;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(opGuid, resGuid, inGuid, inGuid, EmptyOperation));

    CKParameterOperation *operation = context_->CreateCKParameterOperation(
        "CleanupOperation",
        opGuid,
        CKPGUID_INT,
        CKPGUID_INT,
        CKPGUID_INT);
    ASSERT_NE(nullptr, operation);
    ASSERT_EQ(CK_OK, behavior->AddParameterOperation(operation));

    CKParameterOut *out = operation->GetOutParameter();
    ASSERT_NE(nullptr, out);

    ASSERT_EQ(CK_OK, context_->DestroyObject(out));
    EXPECT_EQ(nullptr, operation->GetOutParameter());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
