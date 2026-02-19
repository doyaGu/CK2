#include <gtest/gtest.h>

#include <string>
#include <vector>

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

int gSourceExecCount = 0;
int gDestinationExecCount = 0;
int gRetryFrames = 0;
int gHighExecCount = 0;
int gLowExecCount = 0;
std::vector<int> gExecutionOrder;

int OneShotActivateOutput(const CKBehaviorContext &context) {
    ++gSourceExecCount;
    context.Behavior->ActivateOutput(0, TRUE);
    return CKBR_OK;
}

int ActivateOutputAndRetry(const CKBehaviorContext &context) {
    ++gSourceExecCount;
    context.Behavior->ActivateOutput(0, TRUE);
    return CKBR_ACTIVATENEXTFRAME;
}

int CountDestinationExec(const CKBehaviorContext &) {
    ++gDestinationExecCount;
    return CKBR_OK;
}

int ActivateTwoOutputs(const CKBehaviorContext &context) {
    ++gSourceExecCount;
    context.Behavior->ActivateOutput(0, TRUE);
    context.Behavior->ActivateOutput(1, TRUE);
    return CKBR_OK;
}

int CountHighExec(const CKBehaviorContext &) {
    ++gHighExecCount;
    gExecutionOrder.push_back(2);
    return CKBR_OK;
}

int CountLowExec(const CKBehaviorContext &) {
    ++gLowExecCount;
    gExecutionOrder.push_back(1);
    return CKBR_OK;
}

int RetryThenStop(const CKBehaviorContext &) {
    ++gSourceExecCount;
    if (gRetryFrames > 0) {
        --gRetryFrames;
        return CKBR_ACTIVATENEXTFRAME;
    }
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
    if (!link) {
        return nullptr;
    }

    if (link->SetInBehaviorIO(sourceOut) != CK_OK) {
        return nullptr;
    }
    if (link->SetOutBehaviorIO(targetIn) != CK_OK) {
        return nullptr;
    }

    link->SetInitialActivationDelay(initialDelay);
    link->SetActivationDelay(initialDelay);
    return link;
}

std::string MakeName(const char *prefix, int index) {
    return std::string(prefix) + std::to_string(index);
}

} // namespace

TEST_F(CKRuntimeFixture, DelayedLinkDoesNotDoubleTriggerWhilePending) {
    gSourceExecCount = 0;
    gDestinationExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentPending", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourcePending", ActivateOutputAndRetry);
    CKBehavior *destination = CreateFunctionBehavior(context_, "destinationPending", CountDestinationExec);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOut = source->CreateOutput("sourceOutPending");
    CKBehaviorIO *destinationIn = destination->CreateInput("destinationInPending");
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
    EXPECT_EQ(0, gDestinationExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(0, gDestinationExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(1, gDestinationExecCount);
    EXPECT_GE(gSourceExecCount, 2);
}

TEST_F(CKRuntimeFixture, FunctionBehaviorRetriesThenDeactivates) {
    gSourceExecCount = 0;

    CKBehavior *worker = CreateFunctionBehavior(context_, "workerRetry", RetryThenStop);
    ASSERT_NE(nullptr, worker);

    gRetryFrames = 1;
    worker->Activate(TRUE, FALSE);

    const int first = worker->Execute(0.016f);
    EXPECT_EQ(CKBR_ACTIVATENEXTFRAME, first);
    EXPECT_TRUE(worker->IsActive());

    const int second = worker->Execute(0.016f);
    EXPECT_EQ(CKBR_OK, second);
    EXPECT_FALSE(worker->IsActive());
    EXPECT_EQ(2, gSourceExecCount);
}

TEST_F(CKRuntimeFixture, ParentRemainsActiveUntilDelayedLinkResolves) {
    gSourceExecCount = 0;
    gDestinationExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentActivity", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourceActivity", OneShotActivateOutput);
    CKBehavior *destination = CreateFunctionBehavior(context_, "destinationActivity", CountDestinationExec);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);

    CKBehaviorIO *sourceOut = source->CreateOutput("sourceOutActivity");
    CKBehaviorIO *destinationIn = destination->CreateInput("destinationInActivity");
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
    EXPECT_TRUE(parent->IsActive());

    parent->Execute(0.016f);
    EXPECT_TRUE(parent->IsActive());

    parent->Execute(0.016f);
    EXPECT_EQ(1, gDestinationExecCount);

    for (int i = 0; i < 3; ++i) {
        parent->Execute(0.016f);
    }

    EXPECT_FALSE(parent->IsActive());
}

TEST_F(CKRuntimeFixture, CyclicImmediateLinksReturnInfiniteLoopCode) {
    gSourceExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentCycle", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *a = CreateFunctionBehavior(context_, "cycleA", OneShotActivateOutput);
    CKBehavior *b = CreateFunctionBehavior(context_, "cycleB", OneShotActivateOutput);
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    CKBehaviorIO *aOut = a->CreateOutput("aOut");
    CKBehaviorIO *aIn = a->CreateInput("aIn");
    CKBehaviorIO *bOut = b->CreateOutput("bOut");
    CKBehaviorIO *bIn = b->CreateInput("bIn");
    ASSERT_NE(nullptr, aOut);
    ASSERT_NE(nullptr, aIn);
    ASSERT_NE(nullptr, bOut);
    ASSERT_NE(nullptr, bIn);

    CKBehaviorLink *aToB = CreateLink(context_, aOut, bIn, 0);
    CKBehaviorLink *bToA = CreateLink(context_, bOut, aIn, 0);
    ASSERT_NE(nullptr, aToB);
    ASSERT_NE(nullptr, bToA);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(a));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(b));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(aToB));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(bToA));

    CKBehaviorManager *manager = context_->GetBehaviorManager();
    ASSERT_NE(nullptr, manager);
    const int oldLimit = manager->GetBehaviorMaxIteration();
    manager->SetBehaviorMaxIteration(4);

    a->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);
    const int result = parent->Execute(0.016f);

    manager->SetBehaviorMaxIteration(oldLimit);

    EXPECT_EQ(CKBR_INFINITELOOP, result);
}

TEST_F(CKRuntimeFixture, ImmediateMultiDestinationSchedulingFollowsPriority) {
    gSourceExecCount = 0;
    gHighExecCount = 0;
    gLowExecCount = 0;
    gExecutionOrder.clear();

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentPriority", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourcePriority", ActivateTwoOutputs);
    CKBehavior *highPriorityDst = CreateFunctionBehavior(context_, "highPriorityDst", CountHighExec);
    CKBehavior *lowPriorityDst = CreateFunctionBehavior(context_, "lowPriorityDst", CountLowExec);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, highPriorityDst);
    ASSERT_NE(nullptr, lowPriorityDst);

    highPriorityDst->SetPriority(100);
    lowPriorityDst->SetPriority(10);

    CKBehaviorIO *sourceOut0 = source->CreateOutput("sourceOut0Priority");
    CKBehaviorIO *sourceOut1 = source->CreateOutput("sourceOut1Priority");
    CKBehaviorIO *highIn = highPriorityDst->CreateInput("highInPriority");
    CKBehaviorIO *lowIn = lowPriorityDst->CreateInput("lowInPriority");
    ASSERT_NE(nullptr, sourceOut0);
    ASSERT_NE(nullptr, sourceOut1);
    ASSERT_NE(nullptr, highIn);
    ASSERT_NE(nullptr, lowIn);

    CKBehaviorLink *toHigh = CreateLink(context_, sourceOut0, highIn, 0);
    CKBehaviorLink *toLow = CreateLink(context_, sourceOut1, lowIn, 0);
    ASSERT_NE(nullptr, toHigh);
    ASSERT_NE(nullptr, toLow);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(highPriorityDst));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(lowPriorityDst));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(toHigh));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(toLow));

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);

    parent->Execute(0.016f);

    ASSERT_EQ(1, gHighExecCount);
    ASSERT_EQ(1, gLowExecCount);
    ASSERT_EQ(2u, gExecutionOrder.size());
    EXPECT_EQ(2, gExecutionOrder[0]);
    EXPECT_EQ(1, gExecutionOrder[1]);
}

TEST_F(CKRuntimeFixture, SimultaneousDelayedLinksExpireAndExecuteInSameCycle) {
    gSourceExecCount = 0;
    gHighExecCount = 0;
    gLowExecCount = 0;

    CKBehavior *parent = static_cast<CKBehavior *>(context_->CreateObject(CKCID_BEHAVIOR, "parentSimDelay", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, parent);
    parent->UseGraph();

    CKBehavior *source = CreateFunctionBehavior(context_, "sourceSimDelay", ActivateTwoOutputs);
    CKBehavior *dstA = CreateFunctionBehavior(context_, "dstASimDelay", CountHighExec);
    CKBehavior *dstB = CreateFunctionBehavior(context_, "dstBSimDelay", CountLowExec);
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, dstA);
    ASSERT_NE(nullptr, dstB);

    CKBehaviorIO *sourceOutA = source->CreateOutput("sourceOutASimDelay");
    CKBehaviorIO *sourceOutB = source->CreateOutput("sourceOutBSimDelay");
    CKBehaviorIO *dstInA = dstA->CreateInput("dstInASimDelay");
    CKBehaviorIO *dstInB = dstB->CreateInput("dstInBSimDelay");
    ASSERT_NE(nullptr, sourceOutA);
    ASSERT_NE(nullptr, sourceOutB);
    ASSERT_NE(nullptr, dstInA);
    ASSERT_NE(nullptr, dstInB);

    CKBehaviorLink *linkA = CreateLink(context_, sourceOutA, dstInA, 2);
    CKBehaviorLink *linkB = CreateLink(context_, sourceOutB, dstInB, 2);
    ASSERT_NE(nullptr, linkA);
    ASSERT_NE(nullptr, linkB);

    ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(dstA));
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(dstB));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(linkA));
    ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(linkB));

    source->Activate(TRUE, FALSE);
    parent->Activate(TRUE, FALSE);

    parent->Execute(0.016f);
    EXPECT_EQ(0, gHighExecCount);
    EXPECT_EQ(0, gLowExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(0, gHighExecCount);
    EXPECT_EQ(0, gLowExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(1, gHighExecCount);
    EXPECT_EQ(1, gLowExecCount);

    parent->Execute(0.016f);
    EXPECT_EQ(1, gHighExecCount);
    EXPECT_EQ(1, gLowExecCount);
}

TEST_F(CKRuntimeFixture, ImmediatePropagationStress100Rounds) {
    for (int round = 0; round < 100; ++round) {
        gSourceExecCount = 0;
        gDestinationExecCount = 0;

        CKBehavior *parent = static_cast<CKBehavior *>(
            context_->CreateObject(CKCID_BEHAVIOR, const_cast<char *>(MakeName("stressParentImmediate_", round).c_str()), CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, parent);
        parent->UseGraph();

        CKBehavior *source = CreateFunctionBehavior(context_, MakeName("stressSourceImmediate_", round).c_str(), OneShotActivateOutput);
        CKBehavior *destination = CreateFunctionBehavior(context_, MakeName("stressDestinationImmediate_", round).c_str(), CountDestinationExec);
        ASSERT_NE(nullptr, source);
        ASSERT_NE(nullptr, destination);

        CKBehaviorIO *sourceOut = source->CreateOutput(const_cast<char *>(MakeName("outImmediate_", round).c_str()));
        CKBehaviorIO *destinationIn = destination->CreateInput(const_cast<char *>(MakeName("inImmediate_", round).c_str()));
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
        EXPECT_EQ(1, gDestinationExecCount) << "round=" << round;

        parent->Execute(0.016f);
        EXPECT_EQ(1, gDestinationExecCount) << "round=" << round;
    }
}

TEST_F(CKRuntimeFixture, DelayedPropagationStress100Rounds) {
    for (int round = 0; round < 100; ++round) {
        gSourceExecCount = 0;
        gDestinationExecCount = 0;

        const int delay = (round % 5) + 1;

        CKBehavior *parent = static_cast<CKBehavior *>(
            context_->CreateObject(CKCID_BEHAVIOR, const_cast<char *>(MakeName("stressParentDelayed_", round).c_str()), CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, parent);
        parent->UseGraph();

        CKBehavior *source = CreateFunctionBehavior(context_, MakeName("stressSourceDelayed_", round).c_str(), OneShotActivateOutput);
        CKBehavior *destination = CreateFunctionBehavior(context_, MakeName("stressDestinationDelayed_", round).c_str(), CountDestinationExec);
        ASSERT_NE(nullptr, source);
        ASSERT_NE(nullptr, destination);

        CKBehaviorIO *sourceOut = source->CreateOutput(const_cast<char *>(MakeName("outDelayed_", round).c_str()));
        CKBehaviorIO *destinationIn = destination->CreateInput(const_cast<char *>(MakeName("inDelayed_", round).c_str()));
        ASSERT_NE(nullptr, sourceOut);
        ASSERT_NE(nullptr, destinationIn);

        CKBehaviorLink *link = CreateLink(context_, sourceOut, destinationIn, delay);
        ASSERT_NE(nullptr, link);

        ASSERT_EQ(CK_OK, parent->AddSubBehavior(source));
        ASSERT_EQ(CK_OK, parent->AddSubBehavior(destination));
        ASSERT_EQ(CK_OK, parent->AddSubBehaviorLink(link));

        source->Activate(TRUE, FALSE);
        parent->Activate(TRUE, FALSE);

        for (int frame = 0; frame < delay; ++frame) {
            parent->Execute(0.016f);
            EXPECT_EQ(0, gDestinationExecCount) << "round=" << round << ",frame=" << frame;
        }

        parent->Execute(0.016f);
        EXPECT_EQ(1, gDestinationExecCount) << "round=" << round;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}