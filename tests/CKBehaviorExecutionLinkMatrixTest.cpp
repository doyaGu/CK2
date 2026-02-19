#include <gtest/gtest.h>

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

int gDstExecCount = 0;
int gMidExecCount = 0;

int SourceActivateOutput(const CKBehaviorContext &context) {
    context.Behavior->ActivateOutput(0, TRUE);
    return CKBR_OK;
}

int DestinationCounter(const CKBehaviorContext &) {
    ++gDstExecCount;
    return CKBR_OK;
}

int MidCounter(const CKBehaviorContext &) {
    ++gMidExecCount;
    return CKBR_OK;
}

CKBehavior *CreateFunctionBehavior(CKContext *ctx, const char *name, CKBEHAVIORFCT fct) {
    CKBehavior *behavior = static_cast<CKBehavior *>(ctx->CreateObject(CKCID_BEHAVIOR, const_cast<char *>(name), CK_OBJECTCREATION_DYNAMIC));
    if (behavior) {
        behavior->SetFunction(fct);
    }
    return behavior;
}

CKBehaviorLink *CreateLink(CKContext *ctx, CKBehaviorIO *sourceOut, CKBehaviorIO *targetIn, int initialDelay) {
    CKBehaviorLink *link = static_cast<CKBehaviorLink *>(ctx->CreateObject(CKCID_BEHAVIORLINK, const_cast<char *>("link"), CK_OBJECTCREATION_DYNAMIC));
    if (!link) return nullptr;

    if (link->SetInBehaviorIO(sourceOut) != CK_OK) return nullptr;
    if (link->SetOutBehaviorIO(targetIn) != CK_OK) return nullptr;

    link->SetInitialActivationDelay(initialDelay);
    link->SetActivationDelay(initialDelay);
    return link;
}

} // namespace

TEST_F(CKRuntimeFixture, ImmediateLinkActivatesAndExecutesDestinationSameFrame) {
    gDstExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parent", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "source", SourceActivateOutput);
    CKBehavior *destination = CreateFunctionBehavior(context_, "destination", DestinationCounter);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOut = source->CreateOutput("sourceOut");
    CKBehaviorIO *destinationIn = destination->CreateInput("destinationIn");
    ASSERT_NE(nullptr, sourceOut);
    ASSERT_NE(nullptr, destinationIn);

    CKBehaviorLink *link = CreateLink(context_, sourceOut, destinationIn, 0);
    ASSERT_NE(nullptr, link);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(link));

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);

    parent->Execute(0.016f);

    EXPECT_EQ(1, gDstExecCount);
}

TEST_F(CKRuntimeFixture, DelayedLinkExecutesDestinationAfterDelayFrames) {
    gDstExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentDelayed", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourceDelayed", SourceActivateOutput);
    CKBehavior *destination = CreateFunctionBehavior(context_, "destinationDelayed", DestinationCounter);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOut = source->CreateOutput("sourceOutDelayed");
    CKBehaviorIO *destinationIn = destination->CreateInput("destinationInDelayed");
    ASSERT_NE(nullptr, sourceOut);
    ASSERT_NE(nullptr, destinationIn);

    CKBehaviorLink *link = CreateLink(context_, sourceOut, destinationIn, 2);
    ASSERT_NE(nullptr, link);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(link));

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);

    parent->Execute(0.016f);
    EXPECT_EQ(0, gDstExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(0, gDstExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(1, gDstExecCount);
}

TEST_F(CKRuntimeFixture, GetShortestDelaySelectsShortestPath) {
    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentDelay", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourceShortest", SourceActivateOutput);
    CKBehavior *mid = CreateFunctionBehavior(context_, "midShortest", MidCounter);
    CKBehavior *destination = CreateFunctionBehavior(context_, "destShortest", DestinationCounter);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, mid);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOutA = source->CreateOutput("sourceOutA");
    CKBehaviorIO *sourceOutB = source->CreateOutput("sourceOutB");
    CKBehaviorIO *midIn = mid->CreateInput("midIn");
    CKBehaviorIO *midOut = mid->CreateOutput("midOut");
    CKBehaviorIO *destInA = destination->CreateInput("destInA");
    CKBehaviorIO *destInB = destination->CreateInput("destInB");

    ASSERT_NE(nullptr, sourceOutA);
    ASSERT_NE(nullptr, sourceOutB);
    ASSERT_NE(nullptr, midIn);
    ASSERT_NE(nullptr, midOut);
    ASSERT_NE(nullptr, destInA);
    ASSERT_NE(nullptr, destInB);

    CKBehaviorLink *shortPath1 = CreateLink(context_, sourceOutA, midIn, 1);
    CKBehaviorLink *shortPath2 = CreateLink(context_, midOut, destInA, 2);
    CKBehaviorLink *longPath = CreateLink(context_, sourceOutB, destInB, 6);

    ASSERT_NE(nullptr, shortPath1);
    ASSERT_NE(nullptr, shortPath2);
    ASSERT_NE(nullptr, longPath);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(mid));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));

    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(shortPath1));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(shortPath2));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(longPath));

    const int shortest = source->GetShortestDelay(destination);
    EXPECT_EQ(3, shortest);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
