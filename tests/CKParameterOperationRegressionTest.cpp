#include <gtest/gtest.h>

#include <cstdio>
#include <memory>

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

struct CKStateChunkDeleter {
    void operator()(CKStateChunk *chunk) const {
        if (chunk) {
            DeleteCKStateChunk(chunk);
        }
    }
};

struct CKFileDeleter {
    explicit CKFileDeleter(CKContext *context) : context(context) {}

    void operator()(CKFile *file) const {
        if (file && context) {
            context->DeleteCKFile(file);
        }
    }

    CKContext *context;
};

using CKStateChunkPtr = std::unique_ptr<CKStateChunk, CKStateChunkDeleter>;
using CKFilePtr = std::unique_ptr<CKFile, CKFileDeleter>;

int gOperationInvocationCount = 0;

void NoInputOperation(CKContext *, CKParameterOut *, CKParameterIn *, CKParameterIn *) {
    ++gOperationInvocationCount;
}

void SwapAwareOperation(CKContext *, CKParameterOut *, CKParameterIn *, CKParameterIn *) {
    ++gOperationInvocationCount;
}

void SetSampleValue(CKParameterOut *param, const CKGUID &guid, int seed) {
    if (guid == CKPGUID_INT) {
        int value = seed;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_FLOAT) {
        float value = static_cast<float>(seed) * 1.5f;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_BOOL) {
        CKBOOL value = (seed % 2) ? TRUE : FALSE;
        ASSERT_EQ(CK_OK, param->SetValue(&value));
        return;
    }

    if (guid == CKPGUID_STRING) {
        char text[64];
        sprintf_s(text, "typed_value_%d", seed);
        ASSERT_EQ(CK_OK, param->SetStringValue(text));
        return;
    }

    FAIL() << "Unsupported test guid";
}

void AssertOutputEqualsSample(CKParameterOut *param, const CKGUID &guid, int seed) {
    if (guid == CKPGUID_INT) {
        int value = 0;
        ASSERT_EQ(CK_OK, param->GetValue(&value, FALSE));
        EXPECT_EQ(seed, value);
        return;
    }

    if (guid == CKPGUID_FLOAT) {
        float value = 0.0f;
        ASSERT_EQ(CK_OK, param->GetValue(&value, FALSE));
        EXPECT_FLOAT_EQ(static_cast<float>(seed) * 1.5f, value);
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
        char expected[64];
        sprintf_s(expected, "typed_value_%d", seed);
        EXPECT_STREQ(expected, text);
        return;
    }

    FAIL() << "Unsupported test guid";
}

} // namespace

TEST_F(CKRuntimeFixture, LoadWithNullFileInvokesUpdateAndResolvesOperationFunction) {
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0C1u);

    pm->RegisterOperationType(opGuid, "LoadUpdateNoFile");

    CKGUID resGuid = CKPGUID_NONE;
    CKGUID p1Guid = CKPGUID_NONE;
    CKGUID p2Guid = CKPGUID_NONE;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(
        opGuid,
        resGuid,
        p1Guid,
        p2Guid,
        NoInputOperation));

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETEROPERATION, nullptr));
    ASSERT_NE(nullptr, chunk.get());
    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONOP);
    chunk->WriteGuid(opGuid);
    chunk->CloseChunk();

    CKParameterOperation *loaded = static_cast<CKParameterOperation *>(
        context_->CreateObject(CKCID_PARAMETEROPERATION, "LoadNoFileOperation", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));
    EXPECT_NE(nullptr, loaded->GetOperationFunction());
}

TEST_F(CKRuntimeFixture, LoadWithFileInvokesUpdateAndResolvesOperationFunction) {
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0C2u);

    pm->RegisterOperationType(opGuid, "LoadUpdateWithFile");

    CKGUID resGuid = CKPGUID_NONE;
    CKGUID p1Guid = CKPGUID_NONE;
    CKGUID p2Guid = CKPGUID_NONE;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(
        opGuid,
        resGuid,
        p1Guid,
        p2Guid,
        NoInputOperation));

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETEROPERATION, nullptr));
    ASSERT_NE(nullptr, chunk.get());
    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONOP);
    chunk->WriteGuid(opGuid);
    chunk->CloseChunk();

    CKFilePtr file(context_->CreateCKFile(), CKFileDeleter(context_));
    ASSERT_NE(nullptr, file.get());

    CKParameterOperation *loaded = static_cast<CKParameterOperation *>(
        context_->CreateObject(CKCID_PARAMETEROPERATION, "LoadWithFileOperation", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), file.get()));
    EXPECT_TRUE(loaded->GetOperationGuid() == opGuid);
}

TEST_F(CKRuntimeFixture, DoOperationSkipsExecutionWhenRequiredInputSourceIsMissing) {
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0C3u);

    pm->RegisterOperationType(opGuid, "GuardNullSource");

    CKGUID resGuid = CKPGUID_INT;
    CKGUID p1Guid = CKPGUID_INT;
    CKGUID p2Guid = CKPGUID_NONE;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(
        opGuid,
        resGuid,
        p1Guid,
        p2Guid,
        NoInputOperation));

    CKParameterOperation *operation = context_->CreateCKParameterOperation(
        "GuardOperation",
        opGuid,
        CKPGUID_INT,
        CKPGUID_INT,
        CKPGUID_NONE);
    ASSERT_NE(nullptr, operation);

    gOperationInvocationCount = 0;

    const int initialValue = 77;
    ASSERT_EQ(CK_OK, operation->GetOutParameter()->SetValue(&initialValue));

    const CKERROR result = operation->DoOperation();

    EXPECT_EQ(CKERR_NOTINITIALIZED, result);
    EXPECT_EQ(0, gOperationInvocationCount);

    int afterValue = 0;
    ASSERT_EQ(CK_OK, operation->GetOutParameter()->GetValue(&afterValue, FALSE));
    EXPECT_EQ(initialValue, afterValue);
}

TEST_F(CKRuntimeFixture, LoadRejectsNullChunk) {
    CKParameterOperation *operation = static_cast<CKParameterOperation *>(
        context_->CreateObject(CKCID_PARAMETEROPERATION, "NullChunkOp", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, operation);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, operation->Load(nullptr, nullptr));
}

TEST_F(CKRuntimeFixture, DoOperationWithoutFunctionCopiesInputToOutput) {
    CKGUID unknownGuid(0x6E505F10u, 0x11B2A0D0u);

    CKParameterOperation *operation = context_->CreateCKParameterOperation(
        "NoFunctionCopy",
        unknownGuid,
        CKPGUID_INT,
        CKPGUID_INT,
        CKPGUID_NONE);
    ASSERT_NE(nullptr, operation);

    CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceForCopy", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    const int sourceValue = 2468;
    ASSERT_EQ(CK_OK, sourceOut->SetValue(&sourceValue));
    ASSERT_EQ(CK_OK, operation->GetInParameter1()->SetDirectSource(sourceOut));

    const int initialOut = -1;
    ASSERT_EQ(CK_OK, operation->GetOutParameter()->SetValue(&initialOut));

    EXPECT_EQ(CKERR_NOTINITIALIZED, operation->DoOperation());

    int copiedValue = 0;
    ASSERT_EQ(CK_OK, operation->GetOutParameter()->GetValue(&copiedValue, FALSE));
    EXPECT_EQ(sourceValue, copiedValue);
}

TEST_F(CKRuntimeFixture, DoOperationWithoutFunctionCopiesAcrossPrimaryTypes) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];
        CKGUID unknownGuid(0x6E505F10u, 0x11B2A0F0u + i);

        CKParameterOperation *operation = context_->CreateCKParameterOperation(
            "NoFunctionCopyTyped",
            unknownGuid,
            tc.guid,
            tc.guid,
            CKPGUID_NONE);
        ASSERT_NE(nullptr, operation) << tc.name;

        CKParameterOut *sourceOut = context_->CreateCKParameterOut("typedSourceForCopy", tc.guid, TRUE);
        ASSERT_NE(nullptr, sourceOut) << tc.name;

        SetSampleValue(sourceOut, tc.guid, 100 + i);
        ASSERT_EQ(CK_OK, operation->GetInParameter1()->SetDirectSource(sourceOut)) << tc.name;

        EXPECT_EQ(CKERR_NOTINITIALIZED, operation->DoOperation()) << tc.name;
        AssertOutputEqualsSample(operation->GetOutParameter(), tc.guid, 100 + i);
    }
}

TEST_F(CKRuntimeFixture, DoOperationGuardsMissingInputSourceAcrossPrimaryTypes) {
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];
        CKGUID opGuid(0x6E505F20u, 0x11B2A000u + i);

        char opName[64] = {0};
        sprintf_s(opName, "GuardTypedSource_%s", tc.name);

        pm->RegisterOperationType(opGuid, opName);

        CKGUID resGuid = tc.guid;
        CKGUID p1Guid = tc.guid;
        CKGUID p2Guid = CKPGUID_NONE;
        ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(opGuid, resGuid, p1Guid, p2Guid, NoInputOperation)) << tc.name;

        CKParameterOperation *operation = context_->CreateCKParameterOperation(
            "GuardTypedOperation",
            opGuid,
            tc.guid,
            tc.guid,
            CKPGUID_NONE);
        ASSERT_NE(nullptr, operation) << tc.name;

        gOperationInvocationCount = 0;
        EXPECT_EQ(CKERR_NOTINITIALIZED, operation->DoOperation()) << tc.name;
        EXPECT_EQ(0, gOperationInvocationCount) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, LoadWithFileOldFormatReadsOwnerOutputAndInputs) {
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "OperationOwner", CK_OBJECTCREATION_DYNAMIC));
    CKParameterIn *in1 = context_->CreateCKParameterIn("in1", CKPGUID_INT, TRUE);
    CKParameterIn *in2 = context_->CreateCKParameterIn("in2", CKPGUID_INT, TRUE);
    CKParameterOut *out = context_->CreateCKParameterOut("out", CKPGUID_INT, TRUE);

    ASSERT_NE(nullptr, owner);
    ASSERT_NE(nullptr, in1);
    ASSERT_NE(nullptr, in2);
    ASSERT_NE(nullptr, out);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0E1u);
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);
    pm->RegisterOperationType(opGuid, "LoadWithFileOldFormat");

    CKGUID resGuid = CKPGUID_INT;
    CKGUID p1Guid = CKPGUID_INT;
    CKGUID p2Guid = CKPGUID_INT;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(opGuid, resGuid, p1Guid, p2Guid, NoInputOperation));

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETEROPERATION, nullptr));
    ASSERT_NE(nullptr, chunk.get());
    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONOP);
    chunk->WriteGuid(opGuid);
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONDEFAULTDATA);
    chunk->WriteObject(owner);
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONOUTPUT);
    chunk->WriteObject(out);
    chunk->WriteIdentifier(CK_STATESAVE_OPERATIONINPUTS);
    chunk->WriteObject(in1);
    chunk->WriteObject(in2);
    chunk->CloseChunk();

    CKFilePtr file(context_->CreateCKFile(), CKFileDeleter(context_));
    ASSERT_NE(nullptr, file.get());

    CKParameterOperation *loaded = static_cast<CKParameterOperation *>(
        context_->CreateObject(CKCID_PARAMETEROPERATION, "LoadedOldFormat", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), file.get()));
    EXPECT_EQ(owner, loaded->GetOwner());
    EXPECT_EQ(in1, loaded->GetInParameter1());
    EXPECT_EQ(in2, loaded->GetInParameter2());
    EXPECT_EQ(out, loaded->GetOutParameter());
    EXPECT_TRUE(loaded->GetOperationGuid() == opGuid);
}

TEST_F(CKRuntimeFixture, UpdateFindsSwappedOperationSignatureAndExecutes) {
    CKParameterManager *pm = context_->GetParameterManager();
    ASSERT_NE(nullptr, pm);

    CKGUID opGuid(0x6E505F10u, 0x11B2A0E2u);
    pm->RegisterOperationType(opGuid, "SwapSignature");

    CKGUID resGuid = CKPGUID_INT;
    CKGUID p1Guid = CKPGUID_INT;
    CKGUID p2Guid = CKPGUID_NONE;
    ASSERT_EQ(CK_OK, pm->RegisterOperationFunction(opGuid, resGuid, p2Guid, p1Guid, SwapAwareOperation));

    CKParameterOperation *operation = context_->CreateCKParameterOperation(
        "SwapOperation",
        opGuid,
        CKPGUID_INT,
        CKPGUID_INT,
        CKPGUID_NONE);
    ASSERT_NE(nullptr, operation);

    CKParameterOut *sourceOut = context_->CreateCKParameterOut("swapSource", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    const int sourceValue = 41;
    ASSERT_EQ(CK_OK, sourceOut->SetValue(&sourceValue));
    ASSERT_EQ(CK_OK, operation->GetInParameter1()->SetDirectSource(sourceOut));

    gOperationInvocationCount = 0;
    EXPECT_EQ(CK_OK, operation->DoOperation());
    EXPECT_EQ(1, gOperationInvocationCount);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
