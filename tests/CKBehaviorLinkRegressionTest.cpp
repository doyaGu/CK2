#include <gtest/gtest.h>

#include <memory>

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

struct CKStateChunkDeleter {
    void operator()(CKStateChunk *chunk) const {
        if (chunk) {
            DeleteCKStateChunk(chunk);
        }
    }
};

using CKStateChunkPtr = std::unique_ptr<CKStateChunk, CKStateChunkDeleter>;

int gSourceACount = 0;
int gSourceBCount = 0;
int gDestinationCount = 0;

int SourceAActivateOutput(const CKBehaviorContext &context) {
    ++gSourceACount;
    context.Behavior->ActivateOutput(0, TRUE);
    return CKBR_OK;
}

int SourceBActivateOutput(const CKBehaviorContext &context) {
    ++gSourceBCount;
    context.Behavior->ActivateOutput(0, TRUE);
    return CKBR_OK;
}

int DestinationCounter(const CKBehaviorContext &) {
    ++gDestinationCount;
    return CKBR_OK;
}

CKBehavior *CreateFunctionBehavior(CKContext *ctx, const char *name, CKBEHAVIORFCT fct) {
    CKBehavior *behavior = static_cast<CKBehavior *>(ctx->CreateObject(CKCID_BEHAVIOR, const_cast<char *>(name), CK_OBJECTCREATION_DYNAMIC));
    if (behavior) {
        behavior->SetFunction(fct);
    }
    return behavior;
}

} // namespace

TEST_F(CKRuntimeFixture, SetInOutBehaviorIOGuardsNullAndAssignsPointers) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "link", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "owner", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, link);
    ASSERT_NE(nullptr, owner);

    CKBehaviorIO *input = owner->CreateInput("in");
    CKBehaviorIO *output = owner->CreateOutput("out");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, link->SetInBehaviorIO(nullptr));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, link->SetOutBehaviorIO(nullptr));

    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(output));

    EXPECT_EQ(input, link->GetInBehaviorIO());
    EXPECT_EQ(output, link->GetOutBehaviorIO());
}

TEST_F(CKRuntimeFixture, ActivationDelayLifecycleWorks) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "delayLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, link);

    EXPECT_EQ(1, link->GetActivationDelay());
    EXPECT_EQ(1, link->GetInitialActivationDelay());

    link->SetInitialActivationDelay(9);
    link->SetActivationDelay(3);
    EXPECT_EQ(3, link->GetActivationDelay());
    EXPECT_EQ(9, link->GetInitialActivationDelay());

    link->ResetActivationDelay();
    EXPECT_EQ(9, link->GetActivationDelay());
}

TEST_F(CKRuntimeFixture, SaveLoadNewDataRoundTrip) {
    CKBehaviorLink *saved = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "savedLink", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "savedOwner", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, saved);
    ASSERT_NE(nullptr, owner);

    CKBehaviorIO *input = owner->CreateInput("in");
    CKBehaviorIO *output = owner->CreateOutput("out");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    ASSERT_EQ(CK_OK, saved->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, saved->SetOutBehaviorIO(output));
    saved->SetInitialActivationDelay(7);
    saved->SetActivationDelay(5);

    CKStateChunkPtr chunk(saved->Save(nullptr, CK_STATESAVE_BEHAV_LINKONLY));
    ASSERT_NE(nullptr, chunk.get());

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "loadedLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(input, loaded->GetInBehaviorIO());
    EXPECT_EQ(output, loaded->GetOutBehaviorIO());
    EXPECT_EQ(5, loaded->GetActivationDelay());
    EXPECT_EQ(7, loaded->GetInitialActivationDelay());
}

TEST_F(CKRuntimeFixture, LoadLegacyDataPathRoundTrip) {
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "legacyOwner", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, owner);

    CKBehaviorIO *input = owner->CreateInput("legacyIn");
    CKBehaviorIO *output = owner->CreateOutput("legacyOut");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_BEHAVIORLINK, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_CURDELAY);
    chunk->WriteInt(11);
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_IOS);
    chunk->WriteObject(input);
    chunk->WriteObject(output);
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_DELAY);
    chunk->WriteInt(13);
    chunk->CloseChunk();

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "legacyLoaded", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));
    EXPECT_EQ(input, loaded->GetInBehaviorIO());
    EXPECT_EQ(output, loaded->GetOutBehaviorIO());
    EXPECT_EQ(11, loaded->GetActivationDelay());
    EXPECT_EQ(13, loaded->GetInitialActivationDelay());
}

TEST_F(CKRuntimeFixture, IsObjectUsedTracksInputAndOutputIo) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "usedLink", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "usedOwner", CK_OBJECTCREATION_DYNAMIC));

    ASSERT_NE(nullptr, link);
    ASSERT_NE(nullptr, owner);

    CKBehaviorIO *input = owner->CreateInput("usedIn");
    CKBehaviorIO *output = owner->CreateOutput("usedOut");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(output));

    EXPECT_TRUE(link->IsObjectUsed(input, CKCID_BEHAVIORIO));
    EXPECT_TRUE(link->IsObjectUsed(output, CKCID_BEHAVIORIO));

    CKObject *other = context_->CreateObject(CKCID_OBJECT, "otherObj", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, other);
    EXPECT_FALSE(link->IsObjectUsed(other, CKCID_OBJECT));
}

TEST_F(CKRuntimeFixture, LoadRejectsNullChunk) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "nullChunkLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, link);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, link->Load(nullptr, nullptr));
}

TEST_F(CKRuntimeFixture, LoadLegacyCurrentDelayOnlyKeepsOtherDefaults) {
    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_BEHAVIORLINK, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_CURDELAY);
    chunk->WriteInt(23);
    chunk->CloseChunk();

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "legacyCurDelayOnly", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));
    EXPECT_EQ(23, loaded->GetActivationDelay());
    EXPECT_EQ(1, loaded->GetInitialActivationDelay());
    EXPECT_EQ(nullptr, loaded->GetInBehaviorIO());
    EXPECT_EQ(nullptr, loaded->GetOutBehaviorIO());
}

TEST_F(CKRuntimeFixture, LoadLegacyInitialDelayOnlyKeepsCurrentDefault) {
    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_BEHAVIORLINK, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_DELAY);
    chunk->WriteInt(29);
    chunk->CloseChunk();

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "legacyInitDelayOnly", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));
    EXPECT_EQ(1, loaded->GetActivationDelay());
    EXPECT_EQ(29, loaded->GetInitialActivationDelay());
    EXPECT_EQ(nullptr, loaded->GetInBehaviorIO());
    EXPECT_EQ(nullptr, loaded->GetOutBehaviorIO());
}

TEST_F(CKRuntimeFixture, LoadLegacyIoOnlyKeepsDelayDefaults) {
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "legacyIoOwner", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, owner);

    CKBehaviorIO *input = owner->CreateInput("legacyIoIn");
    CKBehaviorIO *output = owner->CreateOutput("legacyIoOut");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_BEHAVIORLINK, nullptr));
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_IOS);
    chunk->WriteObject(input);
    chunk->WriteObject(output);
    chunk->CloseChunk();

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "legacyIoOnly", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));
    EXPECT_EQ(1, loaded->GetActivationDelay());
    EXPECT_EQ(1, loaded->GetInitialActivationDelay());
    EXPECT_EQ(input, loaded->GetInBehaviorIO());
    EXPECT_EQ(output, loaded->GetOutBehaviorIO());
}

TEST_F(CKRuntimeFixture, AddSubBehaviorLinkRequiresGraphAndValidPointer) {
    CKBehavior *functionBehavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "functionBehavior", CK_OBJECTCREATION_DYNAMIC));
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "graphGuardLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, functionBehavior);
    ASSERT_NE(nullptr, link);

    EXPECT_EQ(CKERR_INVALIDPARAMETER, functionBehavior->AddSubBehaviorLink(link));

    CKBehavior *graphBehavior = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "graphBehavior", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, graphBehavior);
    graphBehavior->UseGraph();
    EXPECT_EQ(CKERR_INVALIDPARAMETER, graphBehavior->AddSubBehaviorLink(nullptr));
}

TEST_F(CKRuntimeFixture, RemoveSubBehaviorLinkByPointerAndIndexUpdatesGraph) {
    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "removeParent", CK_OBJECTCREATION_DYNAMIC));
    CKBehavior *owner = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "removeOwner", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    ASSERT_NE(nullptr, owner);
    parent->UseGraph();

    CKBehaviorIO *input = owner->CreateInput("removeIn");
    CKBehaviorIO *output = owner->CreateOutput("removeOut");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    CKBehaviorLink *linkA = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "removeLinkA", CK_OBJECTCREATION_DYNAMIC));
    CKBehaviorLink *linkB = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "removeLinkB", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, linkA);
    ASSERT_NE(nullptr, linkB);

    ASSERT_EQ(CK_OK, linkA->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, linkA->SetOutBehaviorIO(output));
    ASSERT_EQ(CK_OK, linkB->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, linkB->SetOutBehaviorIO(output));

    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(linkA));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(linkB));
    EXPECT_EQ(2, parent->GetSubBehaviorLinkCount());

    EXPECT_EQ(linkA, parent->RemoveSubBehaviorLink(linkA));
    EXPECT_EQ(1, parent->GetSubBehaviorLinkCount());

    EXPECT_EQ(linkB, parent->RemoveSubBehaviorLink(0));
    EXPECT_EQ(0, parent->GetSubBehaviorLinkCount());
    EXPECT_EQ(nullptr, parent->GetSubBehaviorLink(0));
}

TEST_F(CKRuntimeFixture, PreDeleteRemovesLinkFromOwningGraph) {
    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "preDeleteParent", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehaviorIO *input = parent->CreateInput("preDeleteIn");
    CKBehaviorIO *output = parent->CreateOutput("preDeleteOut");
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, output);

    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "preDeleteLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, link);
    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(input));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(output));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(link));
    EXPECT_EQ(1, parent->GetSubBehaviorLinkCount());

    context_->DestroyObject(link);
    EXPECT_EQ(0, parent->GetSubBehaviorLinkCount());
}

TEST_F(CKRuntimeFixture, RebindingInputIoChangesPropagationSource) {
    gSourceACount = 0;
    gSourceBCount = 0;
    gDestinationCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "rebindParent", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *sourceA = CreateFunctionBehavior(context_, "rebindSourceA", SourceAActivateOutput);
    CKBehavior *sourceB = CreateFunctionBehavior(context_, "rebindSourceB", SourceBActivateOutput);
    CKBehavior *destination = CreateFunctionBehavior(context_, "rebindDestination", DestinationCounter);
    ASSERT_NE(nullptr, sourceA);
    ASSERT_NE(nullptr, sourceB);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *aOut = sourceA->CreateOutput("aOut");
    CKBehaviorIO *bOut = sourceB->CreateOutput("bOut");
    CKBehaviorIO *dstIn = destination->CreateInput("dstIn");
    ASSERT_NE(nullptr, aOut);
    ASSERT_NE(nullptr, bOut);
    ASSERT_NE(nullptr, dstIn);

    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "rebindLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, link);
    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(aOut));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(dstIn));
    link->SetInitialActivationDelay(0);
    link->SetActivationDelay(0);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(sourceA));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(sourceB));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(link));

    sourceA->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    parent->Execute(0.016f);
    EXPECT_EQ(1, gDestinationCount);

    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(bOut));

    sourceA->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    parent->Execute(0.016f);
    EXPECT_EQ(1, gDestinationCount);

    sourceB->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    parent->Execute(0.016f);
    EXPECT_EQ(2, gDestinationCount);
}

TEST_F(CKRuntimeFixture, LoadedLinkNeedsPostLoadToParticipateInOutputTraversal) {
    gSourceACount = 0;
    gDestinationCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(
        context_->CreateObject(CKCID_BEHAVIOR, "postLoadParent", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "postLoadSource", SourceAActivateOutput);
    CKBehavior *destination = CreateFunctionBehavior(context_, "postLoadDestination", DestinationCounter);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOut = source->CreateOutput("postLoadOut");
    CKBehaviorIO *destinationIn = destination->CreateInput("postLoadIn");
    ASSERT_NE(nullptr, sourceOut);
    ASSERT_NE(nullptr, destinationIn);

    CKBehaviorLink *saved = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "postLoadSaved", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, saved);
    ASSERT_EQ(CK_OK, saved->SetInBehaviorIO(sourceOut));
    ASSERT_EQ(CK_OK, saved->SetOutBehaviorIO(destinationIn));

    CKStateChunkPtr chunk(saved->Save(nullptr, CK_STATESAVE_BEHAV_LINKONLY));
    ASSERT_NE(nullptr, chunk.get());

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "postLoadLoaded", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(loaded));

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    parent->Execute(0.016f);
    EXPECT_EQ(0, gDestinationCount);

    loaded->PostLoad();

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    parent->Execute(0.016f);
    EXPECT_EQ(1, gDestinationCount);
}

TEST_F(CKRuntimeFixture, SetFlagsIsMaskedByBehaviorLinkMask) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "maskLink", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, link);

    link->SetFlags(0xFFFFFFFFu);
    EXPECT_EQ(CK_OBJECT_BEHAVIORLINKMASK, link->GetFlags());
}

TEST_F(CKRuntimeFixture, NegativeDelayValuesRoundTripAsStoredState) {
    CKBehaviorLink *saved = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "negativeSaved", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, saved);

    saved->SetActivationDelay(-3);
    saved->SetInitialActivationDelay(-7);

    CKStateChunkPtr chunk(saved->Save(nullptr, CK_STATESAVE_BEHAV_LINKONLY));
    ASSERT_NE(nullptr, chunk.get());

    CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
        context_->CreateObject(CKCID_BEHAVIORLINK, "negativeLoaded", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);
    ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

    EXPECT_EQ(-3, loaded->GetActivationDelay());
    EXPECT_EQ(-7, loaded->GetInitialActivationDelay());
}

TEST_F(CKRuntimeFixture, NewFormatSaveLoadStress100Rounds) {
    for (int round = 0; round < 100; ++round) {
        CKBehavior *owner = static_cast<CKBehavior *>(
            context_->CreateObject(CKCID_BEHAVIOR, "newFormatStressOwner", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, owner);

        CKBehaviorIO *input = owner->CreateInput("newFormatStressIn");
        CKBehaviorIO *output = owner->CreateOutput("newFormatStressOut");
        ASSERT_NE(nullptr, input);
        ASSERT_NE(nullptr, output);

        CKBehaviorLink *saved = static_cast<CKBehaviorLink *>(
            context_->CreateObject(CKCID_BEHAVIORLINK, "newFormatStressSaved", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, saved);

        const int currentDelay = (round % 41) - 20;
        const int initialDelay = (round % 23) - 11;

        ASSERT_EQ(CK_OK, saved->SetInBehaviorIO(input));
        ASSERT_EQ(CK_OK, saved->SetOutBehaviorIO(output));
        saved->SetActivationDelay(currentDelay);
        saved->SetInitialActivationDelay(initialDelay);

        CKStateChunkPtr chunk(saved->Save(nullptr, CK_STATESAVE_BEHAV_LINKONLY));
        ASSERT_NE(nullptr, chunk.get());

        CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
            context_->CreateObject(CKCID_BEHAVIORLINK, "newFormatStressLoaded", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, loaded);
        ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

        EXPECT_EQ(currentDelay, loaded->GetActivationDelay()) << "round=" << round;
        EXPECT_EQ(initialDelay, loaded->GetInitialActivationDelay()) << "round=" << round;
        EXPECT_EQ(input, loaded->GetInBehaviorIO()) << "round=" << round;
        EXPECT_EQ(output, loaded->GetOutBehaviorIO()) << "round=" << round;
    }
}

TEST_F(CKRuntimeFixture, LegacyPartialLoadStress100Rounds) {
    for (int round = 0; round < 100; ++round) {
        CKBehavior *owner = static_cast<CKBehavior *>(
            context_->CreateObject(CKCID_BEHAVIOR, "legacyStressOwner", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, owner);

        CKBehaviorIO *input = owner->CreateInput("legacyStressIn");
        CKBehaviorIO *output = owner->CreateOutput("legacyStressOut");
        ASSERT_NE(nullptr, input);
        ASSERT_NE(nullptr, output);

        const bool writeCurDelay = (round % 2) == 0;
        const bool writeIoPair = (round % 3) != 0;
        const bool writeInitDelay = (round % 5) != 0;

        CKStateChunkPtr chunk(CreateCKStateChunk(CKCID_BEHAVIORLINK, nullptr));
        ASSERT_NE(nullptr, chunk.get());
        chunk->StartWrite();

        const int curDelay = round - 50;
        const int initDelay = 70 - round;

        if (writeCurDelay) {
            chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_CURDELAY);
            chunk->WriteInt(curDelay);
        }
        if (writeIoPair) {
            chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_IOS);
            chunk->WriteObject(input);
            chunk->WriteObject(output);
        }
        if (writeInitDelay) {
            chunk->WriteIdentifier(CK_STATESAVE_BEHAV_LINK_DELAY);
            chunk->WriteInt(initDelay);
        }

        chunk->CloseChunk();

        CKBehaviorLink *loaded = static_cast<CKBehaviorLink *>(
            context_->CreateObject(CKCID_BEHAVIORLINK, "legacyStressLoaded", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, loaded);
        ASSERT_EQ(CK_OK, loaded->Load(chunk.get(), nullptr));

        EXPECT_EQ(writeCurDelay ? curDelay : 1, loaded->GetActivationDelay()) << "round=" << round;
        EXPECT_EQ(writeInitDelay ? initDelay : 1, loaded->GetInitialActivationDelay()) << "round=" << round;
        EXPECT_EQ(writeIoPair ? input : nullptr, loaded->GetInBehaviorIO()) << "round=" << round;
        EXPECT_EQ(writeIoPair ? output : nullptr, loaded->GetOutBehaviorIO()) << "round=" << round;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
