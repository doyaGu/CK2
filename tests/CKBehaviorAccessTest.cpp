#include <gtest/gtest.h>
#include "CKAll.h"

#include <vector>

namespace {
std::vector<CK_ID> executions;
int Record(const CKBehaviorContext &ctx) {
    executions.push_back(ctx.Behavior->GetID());
    return CKBR_OK;
}
int Trigger(const CKBehaviorContext &ctx) {
    ctx.Behavior->ActivateOutput(0);
    return CKBR_OK;
}
class CKBehaviorAccessTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        executions.clear();
    }
    void TearDown() override { CKCloseContext(Context); CKShutdown(); }
    CKBehavior *Node(int priority = 0) {
        auto *node = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
        node->SetFunction(Record);
        node->SetPriority(priority);
        return node;
    }
    CKBehavior *Graph() {
        auto *graph = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
        graph->UseGraph();
        return graph;
    }
    CKBehaviorLink *Link(CKBehaviorIO *from, CKBehaviorIO *to) {
        auto *link = static_cast<CKBehaviorLink *>(Context->CreateObject(CKCID_BEHAVIORLINK));
        EXPECT_EQ(CK_OK, link->SetInBehaviorIO(from));
        EXPECT_EQ(CK_OK, link->SetOutBehaviorIO(to));
        return link;
    }
};

TEST_F(CKBehaviorAccessTest, ChildMovesPreservePriorityOwnershipAndExecutionOrder) {
    CKBehavior *graph = Graph();
    for (int i = 0; i < 4; ++i) ASSERT_EQ(CK_OK, graph->AddSubBehavior(Node(i == 0 ? 10 : 0)));
    CKBehavior *first = graph->GetSubBehavior(1);
    CKBehavior *second = graph->GetSubBehavior(2);
    CKBehavior *third = graph->GetSubBehavior(3);
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(1, 3));
    EXPECT_EQ(second, graph->GetSubBehavior(1));
    EXPECT_EQ(third, graph->GetSubBehavior(2));
    EXPECT_EQ(first, graph->GetSubBehavior(3));
    EXPECT_EQ(graph, first->GetParent());
    EXPECT_EQ(0, first->GetPriority());
    EXPECT_EQ(CKERR_INVALIDPARAMETER, graph->MoveSubBehavior(0, 1));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, graph->MoveSubBehavior(-1, 0));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, graph->MoveSubBehavior(1, 4));
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(3, 1));
    EXPECT_EQ(first, graph->GetSubBehavior(1));
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(2, 2));
    std::vector<CK_ID> expected;
    for (int i = 0; i < graph->GetSubBehaviorCount(); ++i) {
        CKBehavior *node = graph->GetSubBehavior(i);
        expected.push_back(node->GetID());
        node->Activate();
    }
    graph->Activate();
    graph->Execute(.016f);
    EXPECT_EQ(expected, executions);
    EXPECT_EQ(CKERR_INVALIDPARAMETER, first->MoveSubBehavior(0, 0));
}

TEST_F(CKBehaviorAccessTest, LinkEnumerationAndMovesKeepEndpointsAndMembership) {
    CKBehavior *graph = Graph();
    CKBehaviorIO *source = graph->CreateInput("source");
    CKBehavior *destination = Node();
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(destination));
    CKBehaviorIO *sink = destination->CreateInput("sink");
    CKBehaviorLink *a = Link(source, sink), *b = Link(source, sink), *c = Link(source, sink);
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(a));
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(b));
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(c));
    EXPECT_EQ(3, source->GetLinkCount());
    EXPECT_EQ(nullptr, source->GetLink(-1));
    EXPECT_EQ(nullptr, source->GetLink(3));
    ASSERT_EQ(CK_OK, source->MoveLink(0, 2));
    EXPECT_EQ(b, source->GetLink(0)); EXPECT_EQ(c, source->GetLink(1)); EXPECT_EQ(a, source->GetLink(2));
    EXPECT_EQ(a, graph->GetSubBehaviorLink(0));
    EXPECT_EQ(source, a->GetInBehaviorIO()); EXPECT_EQ(sink, a->GetOutBehaviorIO());
    EXPECT_EQ(CKERR_INVALIDPARAMETER, source->MoveLink(-1, 0));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, source->MoveLink(0, 3));
    ASSERT_EQ(CK_OK, source->MoveLink(2, 0));
    EXPECT_EQ(a, source->GetLink(0)); EXPECT_EQ(b, source->GetLink(1)); EXPECT_EQ(c, source->GetLink(2));
    EXPECT_EQ(0, sink->GetLinkCount());
}

TEST_F(CKBehaviorAccessTest, PendingActivationTracksQueueCompletionAndRemoval) {
    CKBehavior *graph = Graph(), *source = Node(), *destination = Node();
    source->SetFunction(Trigger);
    CKBehaviorLink *link = Link(source->CreateOutput("out"), destination->CreateInput("in"));
    link->SetInitialActivationDelay(2); link->SetActivationDelay(2);
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(source));
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(destination));
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(link));
    EXPECT_FALSE(link->IsActivationPending());
    source->Activate(); graph->Activate();
    graph->Execute(.016f);
    EXPECT_TRUE(link->IsActivationPending());
    graph->Execute(.016f);
    EXPECT_FALSE(link->IsActivationPending());
    graph->Execute(.016f);
    EXPECT_FALSE(link->IsActivationPending());
    EXPECT_EQ(1u, executions.size());
    source->Activate(); graph->Activate(); graph->Execute(.016f);
    ASSERT_TRUE(link->IsActivationPending());
    EXPECT_EQ(link, graph->RemoveSubBehaviorLink(link));
    EXPECT_FALSE(link->IsActivationPending());
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(link));
    link->ResetActivationDelay();
    source->Activate(); graph->Activate(); graph->Execute(.016f);
    ASSERT_TRUE(link->IsActivationPending());
    EXPECT_EQ(link, graph->RemoveSubBehaviorLink(0));
    EXPECT_FALSE(link->IsActivationPending());
}
} // namespace
