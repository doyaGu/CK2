#include <gtest/gtest.h>

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

using CKStateChunkPtr = std::unique_ptr<CKStateChunk, CKStateChunkDeleter>;

} // namespace

TEST_F(CKRuntimeFixture, SetDirectSourceAndShareSourceRespectExclusivity) {
    CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceOut", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    CKParameterIn *sharedRoot = context_->CreateCKParameterIn("sharedRoot", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sharedRoot);
    ASSERT_EQ(CK_OK, sharedRoot->SetDirectSource(sourceOut));

    CKParameterIn *input = context_->CreateCKParameterIn("input", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, input);

    ASSERT_EQ(CK_OK, input->SetDirectSource(sourceOut));
    EXPECT_EQ(sourceOut, input->GetDirectSource());
    EXPECT_EQ(nullptr, input->GetSharedSource());
    EXPECT_EQ(sourceOut, input->GetRealSource());

    ASSERT_EQ(CK_OK, input->ShareSourceWith(sharedRoot));
    EXPECT_EQ(nullptr, input->GetDirectSource());
    EXPECT_EQ(sharedRoot, input->GetSharedSource());
    EXPECT_EQ(sourceOut, input->GetRealSource());
}

TEST_F(CKRuntimeFixture, SetDirectSourceSupportsAllPrimaryTypes) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        CKParameterIn *input = context_->CreateCKParameterIn("typedInput", tc.guid, TRUE);
        CKParameterOut *source = context_->CreateCKParameterOut("typedSource", tc.guid, TRUE);

        ASSERT_NE(nullptr, input);
        ASSERT_NE(nullptr, source);

        EXPECT_EQ(CK_OK, input->SetDirectSource(source)) << tc.name;
        EXPECT_EQ(source, input->GetDirectSource()) << tc.name;
        EXPECT_EQ(source, input->GetRealSource()) << tc.name;
        EXPECT_EQ(nullptr, input->GetSharedSource()) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, ShareSourceSupportsAllPrimaryTypes) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        CKParameterOut *source = context_->CreateCKParameterOut("typedSharedSource", tc.guid, TRUE);
        CKParameterIn *sharedRoot = context_->CreateCKParameterIn("typedSharedRoot", tc.guid, TRUE);
        CKParameterIn *input = context_->CreateCKParameterIn("typedSharedInput", tc.guid, TRUE);

        ASSERT_NE(nullptr, source);
        ASSERT_NE(nullptr, sharedRoot);
        ASSERT_NE(nullptr, input);

        ASSERT_EQ(CK_OK, sharedRoot->SetDirectSource(source)) << tc.name;
        EXPECT_EQ(CK_OK, input->ShareSourceWith(sharedRoot)) << tc.name;
        EXPECT_EQ(sharedRoot, input->GetSharedSource()) << tc.name;
        EXPECT_EQ(nullptr, input->GetDirectSource()) << tc.name;
        EXPECT_EQ(source, input->GetRealSource()) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, LoadDataSourceRestoresDirectSourceAndClearsSharedFlag) {
    CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceOutLoad", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    CKParameterIn *saved = context_->CreateCKParameterIn("savedDataSource", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, saved);
    ASSERT_EQ(CK_OK, saved->SetDirectSource(sourceOut));

    CKStateChunkPtr chunk(saved->Save(nullptr, 0xFFFFFFFFu));
    ASSERT_NE(nullptr, chunk.get());

    CKParameterIn *loaded = context_->CreateCKParameterIn("loadedDataSource", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(sourceOut, loaded->GetDirectSource());
    EXPECT_EQ(nullptr, loaded->GetSharedSource());
    EXPECT_EQ(sourceOut, loaded->GetRealSource());
}

TEST_F(CKRuntimeFixture, LoadDataSharedRestoresSharedSourceAndRealSource) {
    CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceOutShared", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    CKParameterIn *sharedRoot = context_->CreateCKParameterIn("sharedRootLoad", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sharedRoot);
    ASSERT_EQ(CK_OK, sharedRoot->SetDirectSource(sourceOut));

    CKParameterIn *saved = context_->CreateCKParameterIn("savedDataShared", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, saved);
    ASSERT_EQ(CK_OK, saved->ShareSourceWith(sharedRoot));

    CKStateChunkPtr chunk(saved->Save(nullptr, 0xFFFFFFFFu));
    ASSERT_NE(nullptr, chunk.get());

    CKParameterIn *loaded = context_->CreateCKParameterIn("loadedDataShared", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(sharedRoot, loaded->GetSharedSource());
    EXPECT_EQ(nullptr, loaded->GetDirectSource());
    EXPECT_EQ(sourceOut, loaded->GetRealSource());
}

TEST_F(CKRuntimeFixture, SaveLoadDataSourceRoundTripAcrossPrimaryTypes) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceOutTyped", tc.guid, TRUE);
        CKParameterIn *saved = context_->CreateCKParameterIn("savedTyped", tc.guid, TRUE);

        ASSERT_NE(nullptr, sourceOut);
        ASSERT_NE(nullptr, saved);
        ASSERT_EQ(CK_OK, saved->SetDirectSource(sourceOut)) << tc.name;

        CKStateChunkPtr chunk(saved->Save(nullptr, 0xFFFFFFFFu));
        ASSERT_NE(nullptr, chunk.get());

        CKParameterIn *loaded = context_->CreateCKParameterIn("loadedTyped", tc.guid, TRUE);
        ASSERT_NE(nullptr, loaded);
        ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr)) << tc.name;

        EXPECT_TRUE(loaded->GetGUID() == tc.guid) << tc.name;
        EXPECT_EQ(sourceOut, loaded->GetDirectSource()) << tc.name;
        EXPECT_EQ(nullptr, loaded->GetSharedSource()) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, SaveLoadDataSharedRoundTripAcrossPrimaryTypes) {
    for (int i = 0; i < static_cast<int>(sizeof(kPrimaryTypes) / sizeof(kPrimaryTypes[0])); ++i) {
        const ParameterTypeCase &tc = kPrimaryTypes[i];

        CKParameterOut *sourceOut = context_->CreateCKParameterOut("sourceSharedTyped", tc.guid, TRUE);
        CKParameterIn *sharedRoot = context_->CreateCKParameterIn("sharedRootTyped", tc.guid, TRUE);
        CKParameterIn *saved = context_->CreateCKParameterIn("savedSharedTyped", tc.guid, TRUE);

        ASSERT_NE(nullptr, sourceOut);
        ASSERT_NE(nullptr, sharedRoot);
        ASSERT_NE(nullptr, saved);

        ASSERT_EQ(CK_OK, sharedRoot->SetDirectSource(sourceOut)) << tc.name;
        ASSERT_EQ(CK_OK, saved->ShareSourceWith(sharedRoot)) << tc.name;

        CKStateChunkPtr chunk(saved->Save(nullptr, 0xFFFFFFFFu));
        ASSERT_NE(nullptr, chunk.get());

        CKParameterIn *loaded = context_->CreateCKParameterIn("loadedSharedTyped", tc.guid, TRUE);
        ASSERT_NE(nullptr, loaded);
        ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr)) << tc.name;

        EXPECT_TRUE(loaded->GetGUID() == tc.guid) << tc.name;
        EXPECT_EQ(sharedRoot, loaded->GetSharedSource()) << tc.name;
        EXPECT_EQ(nullptr, loaded->GetDirectSource()) << tc.name;
        EXPECT_EQ(sourceOut, loaded->GetRealSource()) << tc.name;
    }
}

TEST_F(CKRuntimeFixture, SaveLoadPreservesDisabledFlag) {
    CKParameterIn *saved = context_->CreateCKParameterIn("savedDisabled", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, saved);

    saved->Enable(FALSE);
    ASSERT_FALSE(saved->IsEnabled());

    CKStateChunkPtr chunk(saved->Save(nullptr, 0xFFFFFFFFu));
    ASSERT_NE(nullptr, chunk.get());

    CKParameterIn *loaded = context_->CreateCKParameterIn("loadedDisabled", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_FALSE(loaded->IsEnabled());
}

TEST_F(CKRuntimeFixture, LoadRejectsNullChunk) {
    CKParameterIn *input = context_->CreateCKParameterIn("nullChunkInput", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, input);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, input->Load(nullptr, nullptr));
}

TEST_F(CKRuntimeFixture, SetDirectSourceRejectsIncompatibleParameterType) {
    CKParameterIn *input = context_->CreateCKParameterIn("typedInput", CKPGUID_INT, TRUE);
    CKParameterOut *stringSource = context_->CreateCKParameterOut("stringSource", CKPGUID_STRING, TRUE);

    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, stringSource);

    EXPECT_EQ(CKERR_INCOMPATIBLEPARAMETERS, input->SetDirectSource(stringSource));
}

TEST_F(CKRuntimeFixture, SetSharedSourceRejectsIncompatibleParameterType) {
    CKParameterIn *left = context_->CreateCKParameterIn("leftInput", CKPGUID_INT, TRUE);
    CKParameterIn *right = context_->CreateCKParameterIn("rightInput", CKPGUID_STRING, TRUE);

    ASSERT_NE(nullptr, left);
    ASSERT_NE(nullptr, right);

    EXPECT_EQ(CKERR_INCOMPATIBLEPARAMETERS, left->ShareSourceWith(right));
}

TEST_F(CKRuntimeFixture, LegacyLoadInSharedSetsSharedFlagAndSource) {
    CKParameterOut *sourceOut = context_->CreateCKParameterOut("legacyOut", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETERIN, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_PARAMETERIN_INSHARED);
    chunk->WriteObject(sourceOut);
    chunk->SetDataVersion(0);
    chunk->CloseChunk();

    CKParameterIn *loaded = context_->CreateCKParameterIn("legacyLoadedShared", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(nullptr, loaded->GetDirectSource());
    EXPECT_NE(nullptr, loaded->GetSharedSource());
}

TEST_F(CKRuntimeFixture, LegacyLoadOutSourceAssignsDirectSourceWhenNotShared) {
    CKParameterOut *sourceOut = context_->CreateCKParameterOut("legacyOutSource", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, sourceOut);

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETERIN, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_PARAMETERIN_OUTSOURCE);
    chunk->WriteObject(sourceOut);
    chunk->SetDataVersion(0);
    chunk->CloseChunk();

    CKParameterIn *loaded = context_->CreateCKParameterIn("legacyLoadedOutSource", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(sourceOut, loaded->GetDirectSource());
    EXPECT_EQ(sourceOut, loaded->GetRealSource());
}

TEST_F(CKRuntimeFixture, LegacyGuidConversionMapsOldMessageToCurrentType) {
    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETERIN, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_PARAMETERIN_DATASOURCE);
    chunk->WriteGuid(CKPGUID_OLDMESSAGE);
    chunk->WriteObject(nullptr);
    chunk->SetDataVersion(1);
    chunk->CloseChunk();

    CKParameterIn *loaded = context_->CreateCKParameterIn("legacyGuidLoaded", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_TRUE(loaded->GetGUID() == CKPGUID_MESSAGE);
}

TEST_F(CKRuntimeFixture, LegacyGuidConversionMapsOldAttributeAndTimeToCurrentTypes) {
    struct GuidPair {
        CKGUID legacy;
        CKGUID expected;
    } pairs[] = {
        {CKPGUID_OLDATTRIBUTE, CKPGUID_ATTRIBUTE},
        {CKPGUID_OLDTIME, CKPGUID_TIME},
    };

    for (int i = 0; i < static_cast<int>(sizeof(pairs) / sizeof(pairs[0])); ++i) {
        CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_PARAMETERIN, nullptr));
        ASSERT_NE(nullptr, chunk.get());

        chunk->StartWrite();
        chunk->WriteIdentifier(CK_STATESAVE_PARAMETERIN_DATASOURCE);
        chunk->WriteGuid(pairs[i].legacy);
        chunk->WriteObject(nullptr);
        chunk->SetDataVersion(1);
        chunk->CloseChunk();

        CKParameterIn *loaded = context_->CreateCKParameterIn("legacyGuidLoaded2", CKPGUID_INT, TRUE);
        ASSERT_NE(nullptr, loaded);
        ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

        EXPECT_TRUE(loaded->GetGUID() == pairs[i].expected);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
