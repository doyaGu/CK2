#include <gtest/gtest.h>
#include <cstring>
#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKMessage.cpp"
#ifdef SendMessage
#undef SendMessage
#endif

// This internal helper is not exported by the fixed SDK. Bridge only its
// native 0x240089DF flag contract; messages and parameter objects remain real.
void CKParameter::MessageDeleteAfterUse(CKBOOL value) {
    if (value) ModifyObjectFlags(CK_PARAMETEROUT_DELETEAFTERUSE, 0);
    else ModifyObjectFlags(0, CK_PARAMETEROUT_DELETEAFTERUSE);
}

class MessageProbe : public CKMessage {
public:
    explicit MessageProbe(CKContext *context) : CKMessage(context) {}
    static int References(const CKMessage &message) {
        int CKMessage::*field = &MessageProbe::m_RefCount; return message.*field;
    }
    CKContext *Context() const { return m_Context; }
    CK_ID SenderID() const { return m_Sender; }
    CK_ID RecipientID() const { return m_Recipient; }
    CK_CLASSID BroadcastClass() const { return m_BroadcastCid; }
    const XObjectArray *Parameters() const { return m_Parameters; }
    void AppendRaw(CK_ID id) {
        if (!m_Parameters) m_Parameters = new XObjectArray;
        m_Parameters->PushBack(id);
    }
    size_t Offset(int index) const {
        const void *fields[] = {&m_RefCount,&m_MessageType,&m_Parameters,&m_SendingType,
            &m_Sender,&m_BroadcastCid,&m_Recipient,&m_Context};
        return (const char *)fields[index] - (const char *)this;
    }
};

class CKMessageContractTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    }
    void TearDown() override { if (Context) CKCloseContext(Context); CKShutdown(); }
    CKParameterLocal *Parameter(const char *name = "message parameter") {
        return Context->CreateCKParameterLocal(const_cast<char *>(name), CKPGUID_INT);
    }
    CKBeObject *Object(const char *name = "message object") {
        return static_cast<CKBeObject *>(Context->CreateObject(CKCID_BEOBJECT, const_cast<char *>(name)));
    }
};

TEST_F(CKMessageContractTest, ConstructorInitializesNativeDefaultsAndKeepsTheBorrowedContext) {
    MessageProbe message(Context);
    EXPECT_EQ(1, MessageProbe::References(message)); EXPECT_EQ(-1, message.GetMsgType());
    EXPECT_EQ(CK_MESSAGE_SINGLE, message.GetSendingType()); EXPECT_EQ(0u, message.SenderID());
    EXPECT_EQ(0u, message.RecipientID()); EXPECT_EQ(nullptr, message.Parameters()); EXPECT_EQ(Context, message.Context());
    EXPECT_EQ(nullptr, message.GetSender()); EXPECT_EQ(nullptr, message.GetRecipient()); EXPECT_EQ(0, message.GetParameterCount());
}

TEST_F(CKMessageContractTest, LayoutMatchesNativeWin32AndPointerSizedX64Fields) {
    MessageProbe message(Context); const size_t x86[] = {0,4,8,12,16,20,20,24}, x64[] = {0,4,8,16,20,24,24,32};
    for (int i = 0; i < 8; ++i) EXPECT_EQ(sizeof(void *) == 4 ? x86[i] : x64[i], message.Offset(i));
    EXPECT_EQ(sizeof(void *) == 4 ? 28u : 40u, sizeof(CKMessage)); EXPECT_EQ(sizeof(void *), alignof(CKMessage));
    EXPECT_FALSE((bool)__is_polymorphic(CKMessage)); EXPECT_FALSE((bool)__is_base_of(CKObject, CKMessage));
}

TEST_F(CKMessageContractTest, MessageTypeAndSendingModeAreIndependentStoredValues) {
    CKMessage message(Context); message.SetMsgType(123); message.SetSendingType(CK_MESSAGE_GROUP);
    EXPECT_EQ(123, message.GetMsgType()); EXPECT_EQ(CK_MESSAGE_GROUP, message.GetSendingType());
    message.SetSendingType(CK_MESSAGE_BROADCAST); EXPECT_EQ(123, message.GetMsgType());
    message.SetMsgType(-7); EXPECT_EQ(-7, message.GetMsgType()); EXPECT_EQ(CK_MESSAGE_BROADCAST, message.GetSendingType());
}

TEST_F(CKMessageContractTest, SenderAndRecipientResolveIdsWithoutOwningTheObjects) {
    CKBeObject *sender = Object("sender"), *recipient = Object("recipient"); ASSERT_NE(nullptr, sender); ASSERT_NE(nullptr, recipient);
    CK_ID senderID = sender->GetID(), recipientID = recipient->GetID();
    { MessageProbe message(Context); message.SetSender(sender); message.SetRecipient(recipient);
      EXPECT_EQ(senderID, message.SenderID()); EXPECT_EQ(recipientID, message.RecipientID());
      EXPECT_EQ(sender, message.GetSender()); EXPECT_EQ(recipient, message.GetRecipient()); }
    EXPECT_EQ(sender, Context->GetObject(senderID)); EXPECT_EQ(recipient, Context->GetObject(recipientID));
}

TEST_F(CKMessageContractTest, SenderAndRecipientCanBeClearedAndDeletedIdsResolveToNull) {
    CKBeObject *sender = Object("sender"), *recipient = Object("recipient"); ASSERT_NE(nullptr, sender); ASSERT_NE(nullptr, recipient);
    CKMessage message(Context); message.SetSender(sender); message.SetRecipient(recipient);
    CKDestroyObject(sender); CKDestroyObject(recipient); EXPECT_EQ(nullptr, message.GetSender()); EXPECT_EQ(nullptr, message.GetRecipient());
    message.SetSender(nullptr); message.SetRecipient(nullptr); EXPECT_EQ(nullptr, message.GetSender()); EXPECT_EQ(nullptr, message.GetRecipient());
}

TEST_F(CKMessageContractTest, BroadcastTypeAcceptsBeObjectHierarchyAndSharesTheRecipientUnion) {
    MessageProbe message(Context); message.SetSendingType(CK_MESSAGE_BROADCAST);
    EXPECT_EQ(CK_OK, message.SetBroadcastObjectType()); EXPECT_EQ(CKCID_BEOBJECT, message.BroadcastClass());
    EXPECT_EQ(static_cast<CK_ID>(CKCID_BEOBJECT), message.RecipientID());
    EXPECT_EQ(CK_OK, message.SetBroadcastObjectType(CKCID_GROUP)); EXPECT_EQ(CKCID_GROUP, message.BroadcastClass());
    CKBeObject *recipient = Object(); ASSERT_NE(nullptr, recipient); message.SetRecipient(recipient);
    EXPECT_EQ(static_cast<CK_CLASSID>(recipient->GetID()), message.BroadcastClass());
}

TEST_F(CKMessageContractTest, InvalidBroadcastClassPreservesThePreviousUnionAndMode) {
    MessageProbe message(Context); ASSERT_EQ(CK_OK, message.SetBroadcastObjectType(CKCID_GROUP));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, message.SetBroadcastObjectType(CKCID_PARAMETER));
    EXPECT_EQ(CKCID_GROUP, message.BroadcastClass()); EXPECT_EQ(CK_MESSAGE_SINGLE, message.GetSendingType());
}

TEST_F(CKMessageContractTest, NullParameterOperationsDoNotAllocateOrChangeMessageState) {
    MessageProbe message(Context); EXPECT_EQ(CKERR_INVALIDPARAMETER, message.AddParameter(nullptr));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, message.RemoveParameter(nullptr)); EXPECT_EQ(nullptr, message.Parameters());
    EXPECT_EQ(0, message.GetParameterCount()); EXPECT_EQ(nullptr, message.GetParameter(0));
}

TEST_F(CKMessageContractTest, ParametersKeepInsertionOrderAndPreserveDuplicateSlots) {
    CKParameterLocal *a = Parameter("a"), *b = Parameter("b"); ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    CKMessage message(Context); ASSERT_EQ(CK_OK, message.AddParameter(a)); ASSERT_EQ(CK_OK, message.AddParameter(b));
    ASSERT_EQ(CK_OK, message.AddParameter(a)); ASSERT_EQ(3, message.GetParameterCount());
    EXPECT_EQ(a, message.GetParameter(0)); EXPECT_EQ(b, message.GetParameter(1)); EXPECT_EQ(a, message.GetParameter(2));
}

TEST_F(CKMessageContractTest, AddParameterSetsOrClearsTheParametersGlobalDeleteAfterUseFlag) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter);
    CKMessage message(Context); ASSERT_EQ(CK_OK, message.AddParameter(parameter, TRUE));
    EXPECT_NE(0u, parameter->GetObjectFlags() & CK_PARAMETEROUT_DELETEAFTERUSE);
    ASSERT_EQ(CK_OK, message.AddParameter(parameter, FALSE));
    EXPECT_EQ(0u, parameter->GetObjectFlags() & CK_PARAMETEROUT_DELETEAFTERUSE); EXPECT_EQ(2, message.GetParameterCount());
}

TEST_F(CKMessageContractTest, RemoveParameterRemovesOnlyTheFirstMatchingDuplicate) {
    CKParameterLocal *a = Parameter("a"), *b = Parameter("b"); ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    CKMessage message(Context); message.AddParameter(a); message.AddParameter(b); message.AddParameter(a);
    EXPECT_EQ(CK_OK, message.RemoveParameter(a)); ASSERT_EQ(2, message.GetParameterCount());
    EXPECT_EQ(b, message.GetParameter(0)); EXPECT_EQ(a, message.GetParameter(1));
}

TEST_F(CKMessageContractTest, RemovingAnAbsentParameterFromANonemptyListStillSucceeds) {
    CKParameterLocal *a = Parameter("a"), *b = Parameter("b"); ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    CKMessage message(Context); EXPECT_EQ(CKERR_INVALIDPARAMETER, message.RemoveParameter(a));
    message.AddParameter(a); EXPECT_EQ(CK_OK, message.RemoveParameter(b)); ASSERT_EQ(1, message.GetParameterCount()); EXPECT_EQ(a, message.GetParameter(0));
}

TEST_F(CKMessageContractTest, RemovingTheLastParameterReleasesTheListButDoesNotDestroyTheParameter) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    MessageProbe message(Context); message.AddParameter(parameter, TRUE);
    EXPECT_EQ(CK_OK, message.RemoveParameter(parameter)); EXPECT_EQ(0, message.GetParameterCount()); EXPECT_EQ(nullptr, message.Parameters());
    EXPECT_EQ(parameter, Context->GetObject(id)); EXPECT_NE(0u, parameter->GetObjectFlags() & CK_PARAMETEROUT_DELETEAFTERUSE);
    EXPECT_EQ(CKERR_INVALIDPARAMETER, message.RemoveParameter(parameter));
}

TEST_F(CKMessageContractTest, ParameterLookupRejectsInvalidIndexesAndStaleIdsWithoutChangingTheCount) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CKMessage message(Context); message.AddParameter(parameter);
    EXPECT_EQ(nullptr, message.GetParameter(-1)); EXPECT_EQ(nullptr, message.GetParameter(1));
    CKDestroyObject(parameter); EXPECT_EQ(nullptr, message.GetParameter(0)); EXPECT_EQ(1, message.GetParameterCount());
}

TEST_F(CKMessageContractTest, ParameterLookupDoesNotReturnANonparameterStoredUnderAnId) {
    CKBeObject *object = Object(); ASSERT_NE(nullptr, object); MessageProbe message(Context); message.AppendRaw(object->GetID());
    EXPECT_EQ(nullptr, message.GetParameter(0)); EXPECT_EQ(1, message.GetParameterCount());
}

TEST_F(CKMessageContractTest, DestructionPreservesUnmarkedParameters) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    { CKMessage message(Context); ASSERT_EQ(CK_OK, message.AddParameter(parameter, FALSE)); }
    EXPECT_EQ(parameter, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, DestructionDeletesMarkedParametersButKeepsOtherEntries) {
    CKParameterLocal *a = Parameter("temporary"), *b = Parameter("retained"); ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    CK_ID aID = a->GetID(), bID = b->GetID();
    { CKMessage message(Context); message.AddParameter(a, TRUE); message.AddParameter(b, FALSE); }
    EXPECT_EQ(nullptr, Context->GetObject(aID)); EXPECT_EQ(b, Context->GetObject(bID));
}

TEST_F(CKMessageContractTest, DuplicateMarkedParametersAreDeletedOnceAndLaterStaleSlotsAreSkipped) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    { CKMessage message(Context); message.AddParameter(parameter, TRUE); message.AddParameter(parameter, TRUE); }
    EXPECT_EQ(nullptr, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, DestructionToleratesAParameterDeletedBeforeTheMessage) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter);
    { CKMessage message(Context); message.AddParameter(parameter, TRUE); CKDestroyObject(parameter); }
    SUCCEED();
}

TEST_F(CKMessageContractTest, RemovingAMarkedParameterDetachesItFromMessageCleanup) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    { CKMessage message(Context); message.AddParameter(parameter, TRUE); message.RemoveParameter(parameter); }
    EXPECT_EQ(parameter, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, DeleteAfterUsePolicyIsReadFromTheLiveParameterAtDestruction) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    { CKMessage message(Context); message.AddParameter(parameter, TRUE); parameter->MessageDeleteAfterUse(FALSE); }
    EXPECT_EQ(parameter, Context->GetObject(id));
    { CKMessage message(Context); message.AddParameter(parameter, FALSE); parameter->MessageDeleteAfterUse(TRUE); }
    EXPECT_EQ(nullptr, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, TwoMessagesSharingAMarkedParameterSkipTheAlreadyDestroyedObject) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    CKMessage *first = new CKMessage(Context), *second = new CKMessage(Context);
    first->AddParameter(parameter, TRUE); second->AddParameter(parameter, TRUE);
    EXPECT_EQ(0, first->Release()); EXPECT_EQ(nullptr, Context->GetObject(id)); EXPECT_EQ(nullptr, second->GetParameter(0));
    EXPECT_EQ(0, second->Release());
}

TEST_F(CKMessageContractTest, ReferenceCountingDefersParameterCleanupUntilTheFinalRelease) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    CKMessage *message = new CKMessage(Context); message->AddParameter(parameter, TRUE);
    EXPECT_EQ(2, message->AddRef()); EXPECT_EQ(3, message->AddRef());
    EXPECT_EQ(2, message->Release()); EXPECT_EQ(parameter, Context->GetObject(id));
    EXPECT_EQ(1, message->Release()); EXPECT_EQ(parameter, Context->GetObject(id));
    EXPECT_EQ(0, message->Release()); EXPECT_EQ(nullptr, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, QueueingAddsAManagerReferenceAndProcessingReleasesOnlyThatReference) {
    CKMessageManager *manager = Context->GetMessageManager(); ASSERT_NE(nullptr, manager);
    CKMessage *message = new CKMessage(Context); message->SetMsgType(0);
    ASSERT_EQ(CK_OK, manager->SendMessage(message)); EXPECT_EQ(2, MessageProbe::References(*message));
    ASSERT_EQ(CK_OK, manager->PostProcess()); EXPECT_EQ(1, MessageProbe::References(*message)); EXPECT_EQ(0, message->Release());
}

TEST_F(CKMessageContractTest, FailedQueueingLeavesTheCallersReferenceAndParametersIntact) {
    CKParameterLocal *parameter = Parameter(); ASSERT_NE(nullptr, parameter); CK_ID id = parameter->GetID();
    CKMessage *message = new CKMessage(Context); ASSERT_EQ(CK_OK, message->AddParameter(parameter, TRUE));
    EXPECT_EQ(-1, message->GetMsgType());
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Context->GetMessageManager()->SendMessage(message));
    EXPECT_EQ(1, MessageProbe::References(*message)); EXPECT_EQ(parameter, Context->GetObject(id));
    EXPECT_EQ(parameter, message->GetParameter(0)); EXPECT_EQ(0, message->Release());
    EXPECT_EQ(nullptr, Context->GetObject(id));
}

TEST_F(CKMessageContractTest, ADetachedEmptyMessageCanBeConstructedInspectedAndReleased) {
    CKMessage *message = new CKMessage(nullptr); EXPECT_EQ(0, message->GetParameterCount()); EXPECT_EQ(nullptr, message->GetParameter(0));
    EXPECT_EQ(2, message->AddRef()); EXPECT_EQ(1, message->Release()); EXPECT_EQ(0, message->Release());
}
