#include "CKMessageManager.h"

#include "CKFile.h"
#include "CKBehavior.h"
#include "CKBehaviorIO.h"
#include "CKScene.h"
#include "CKGroup.h"
#include "CKBodyPart.h"
#include "CKCharacter.h"
#include "CKDebugContext.h"
#include "CKParameterManager.h"

extern int g_MaxClassID;

CKMessageType CKMessageManager::AddMessageType(CKSTRING MsgName) {
    if (!MsgName || MsgName[0] == '\0')
        return -1;

    XString name = MsgName;
    CKStringArray::Iterator it = m_RegisteredMessageTypes.Find(name);
    if (it != m_RegisteredMessageTypes.End())
        return it - m_RegisteredMessageTypes.Begin();

    int newIndex = m_RegisteredMessageTypes.Size();
    m_RegisteredMessageTypes.PushBack(name);

    CKWaitingObjectArray **newList = new CKWaitingObjectArray *[newIndex + 1];
    if (m_MsgWaitingList) {
        memcpy(newList, m_MsgWaitingList, newIndex * sizeof(CKWaitingObjectArray *));
        delete[] m_MsgWaitingList;
    }
    newList[newIndex] = nullptr;
    m_MsgWaitingList = newList;

    return newIndex;
}

CKSTRING CKMessageManager::GetMessageTypeName(CKMessageType MsgType) {
    if (MsgType >= 0 && MsgType < m_RegisteredMessageTypes.Size())
        return m_RegisteredMessageTypes[MsgType].Str();
    return nullptr;
}

int CKMessageManager::GetMessageTypeCount() {
    return m_RegisteredMessageTypes.Size();
}

void CKMessageManager::RenameMessageType(CKMessageType MsgType, CKSTRING NewName) {
    if (MsgType >= 0 && MsgType < m_RegisteredMessageTypes.Size())
        m_RegisteredMessageTypes[MsgType] = NewName;
}

void CKMessageManager::RenameMessageType(CKSTRING OldName, CKSTRING NewName) {
    if (!OldName || !NewName)
        return;
    RenameMessageType(AddMessageType(OldName), NewName);
}

CKERROR CKMessageManager::SendMessage(CKMessage *msg) {
    if (!msg || msg->m_MessageType < 0)
        return CKERR_INVALIDPARAMETER;

    msg->AddRef();

    m_ReceivedMsgThisFrame.PushBack(msg);

    CKContext *ctx = m_Context;
    if (ctx->m_DebugContext && ctx->m_DebugContext->InDebug) {
        if (ctx->m_UICallBackFct) {
            CKUICallbackStruct cbData;
            cbData.Reason = CKUIM_DEBUGMESSAGESEND;
            cbData.DebugMessageSent = msg;
            ctx->m_UICallBackFct(cbData, ctx->m_InterfaceModeData);
        }
    }

    return CK_OK;
}

CKMessage *CKMessageManager::SendMessageSingle(CKMessageType MsgType, CKBeObject *dest, CKBeObject *sender) {
    if (!dest || MsgType < 0 || MsgType >= m_RegisteredMessageTypes.Size())
        return nullptr;

    CKMessage *msg = CreateMessageSingle(MsgType, dest, sender);
    if (msg) {
        SendMessage(msg);
        msg->Release();
    }
    return msg;
}

CKMessage *CKMessageManager::SendMessageGroup(CKMessageType MsgType, CKGroup *group, CKBeObject *sender) {
    if (!group || MsgType < 0 || MsgType >= m_RegisteredMessageTypes.Size())
        return nullptr;

    CKMessage *msg = CreateMessageGroup(MsgType, group, sender);
    if (msg) {
        SendMessage(msg);
        msg->Release();
    }
    return msg;
}

CKMessage *CKMessageManager::SendMessageBroadcast(CKMessageType MsgType, CK_CLASSID cid, CKBeObject *sender) {
    CKMessage *msg = CreateMessageBroadcast(MsgType, cid, sender);
    if (msg) {
        SendMessage(msg);
        msg->Release();
    }
    return msg;
}

CKERROR CKMessageManager::RegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate,
                                       CKBeObject *obj) {
    if (MsgType < 0 || MsgType >= m_RegisteredMessageTypes.Size())
        return CKERR_INVALIDMESSAGE;
    if (!behav)
        return CKERR_INVALIDPARAMETER;
    CKBehaviorIO *output = behav->GetOutput(OutputToActivate);
    if (!output)
        return CKERR_INVALIDPARAMETER;

    behav->ModifyFlags(CKBEHAVIOR_WAITSFORMESSAGE, 0);

    CKWaitingObjectArray *&waitList = m_MsgWaitingList[MsgType];
    if (!waitList) {
        waitList = new CKWaitingObjectArray;
    }

    for (XArray<CKWaitingObject>::Iterator it = waitList->Begin(); it != waitList->End(); ++it) {
        if (it->m_Behavior == behav && it->m_Output == output)
            return CKERR_ALREADYPRESENT;
    }

    CKWaitingObject newEntry;
    newEntry.m_Behavior = behav;
    newEntry.m_Output = output;
    newEntry.m_BeObject = obj;
    waitList->PushBack(newEntry);

    return CK_OK;
}

CKERROR CKMessageManager::RegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate, CKBeObject *obj) {
    return RegisterWait(AddMessageType(MsgName), behav, OutputToActivate, obj);
}

CKERROR CKMessageManager::UnRegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate) {
    if (MsgType < 0 || MsgType >= m_RegisteredMessageTypes.Size())
        return CKERR_INVALIDMESSAGE;
    if (!behav)
        return CKERR_INVALIDPARAMETER;

    CKWaitingObjectArray *waitList = m_MsgWaitingList[MsgType];
    if (!waitList)
        return CK_OK;

    CKBehaviorIO *output = behav->GetOutput(OutputToActivate);

    // Remove matching entries
    for (XArray<CKWaitingObject>::Iterator it = waitList->Begin(); it != waitList->End();) {
        if (it->m_Behavior == behav && (!output || it->m_Output == output)) {
            it = waitList->Remove(it);
            if (behav)
                behav->ModifyFlags(0, CKBEHAVIOR_WAITSFORMESSAGE);
        } else {
            ++it;
        }
    }
    return CK_OK;
}

CKERROR CKMessageManager::UnRegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate) {
    return UnRegisterWait(AddMessageType(MsgName), behav, OutputToActivate);
}

CKERROR CKMessageManager::RegisterDefaultMessages() {
    AddMessageType("OnClick");
    AddMessageType("OnDblClick");
    return CK_OK;
}

CKStateChunk *CKMessageManager::SaveData(CKFile *SavedFile) {
    XBitArray usedMessageTypes(m_RegisteredMessageTypes.Size());
    CKParameterType msgParamType = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_MESSAGE);

    // Check for messages used in output parameters
    const int paramOutCount = SavedFile->m_IndexByClassId[CKCID_PARAMETEROUT].Size();
    for (int i = 0; i < paramOutCount; ++i) {
        CKParameterOut *param = (CKParameterOut *) SavedFile->m_FileObjects[SavedFile->m_IndexByClassId[CKCID_PARAMETEROUT][i]].ObjPtr;
        if (param && param->GetType() == msgParamType) {
            int msgType;
            param->GetValue(&msgType);
            if (msgType >= 0) usedMessageTypes.Set(msgType);
        }
    }

    // Check for messages used in local parameters
    const int paramLocalCount = SavedFile->m_IndexByClassId[CKCID_PARAMETERLOCAL].Size();
    for (int i = 0; i < paramLocalCount; ++i) {
        CKParameterLocal *param = (CKParameterLocal *) SavedFile->m_FileObjects[SavedFile->m_IndexByClassId[CKCID_PARAMETERLOCAL][i]].ObjPtr;
        if (param && param->GetType() == msgParamType) {
            int msgType;
            param->GetValue(&msgType);
            if (msgType >= 0) usedMessageTypes.Set(msgType);
        }
    }

    if (usedMessageTypes.BitSet() > 0) {
        CKStateChunk *chunk = CreateCKStateChunk(CKCID_MESSAGEMANAGER, SavedFile);
        chunk->StartWrite();
        chunk->WriteIdentifier(0x53);
        chunk->WriteInt(m_RegisteredMessageTypes.Size());
        for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
            chunk->WriteString(usedMessageTypes.IsSet(i) ? m_RegisteredMessageTypes[i].Str() : (CKSTRING) "");
        }
        chunk->CloseChunk();
        return chunk;
    }
    return nullptr;
}

CKERROR CKMessageManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile) {
    if (!chunk) return CKERR_INVALIDPARAMETER;
    chunk->StartRead();
    if (!chunk->SeekIdentifier(0x53)) return CK_OK;

    const int typeCount = chunk->ReadInt();
    if (typeCount <= 0) return CK_OK;

    XArray<CKMessageType> typeIDs(typeCount);
    for (int i = 0; i < typeCount; ++i) {
        CKSTRING name = nullptr;
        chunk->ReadString(&name);
        typeIDs.PushBack(AddMessageType(name));
        delete[] name;
    }
    LoadedFile->RemapManagerInt(MESSAGE_MANAGER_GUID, typeIDs.Begin(), typeCount);
    return CK_OK;
}

CKERROR CKMessageManager::PreClearAll() {
    if (m_MsgWaitingList) {
        for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
            CKWaitingObjectArray *waitingArray = m_MsgWaitingList[i];
            if (waitingArray) {
                delete waitingArray;
                m_MsgWaitingList[i] = nullptr;
            }
        }
        delete[] m_MsgWaitingList;
        m_MsgWaitingList = nullptr;
    }
    m_RegisteredMessageTypes.Clear();
    for (int i = 0; i < m_ReceivedMsgThisFrame.Size(); ++i) {
        CKMessage *msg = m_ReceivedMsgThisFrame[i];
        if (msg) {
            msg->Release();
        }
    }
    m_ReceivedMsgThisFrame.Clear();
    m_LastFrameObjects.Clear();
    return RegisterDefaultMessages();
}

CKERROR CKMessageManager::PostProcess() {
    for (auto it = m_LastFrameObjects.Begin(); it != m_LastFrameObjects.End(); ++it) {
        CKBeObject *beObj = (CKBeObject *)*it;
        if (beObj)
            beObj->RemoveAllLastFrameMessage();
    }
    m_LastFrameObjects.Clear();

    CKScene *currentScene = m_Context->GetCurrentScene();

    for (int i = 0; i < m_ReceivedMsgThisFrame.Size(); ++i) {
        CKMessage *msg = m_ReceivedMsgThisFrame[i];
        if (!msg)
            continue;

        CKMessageType msgType = msg->GetMsgType();
        CK_MESSAGE_SENDINGTYPE sendType = msg->GetSendingType();

        // Dispatch to general listeners
        if (sendType == CK_MESSAGE_BROADCAST) {
            CK_CLASSID broadcastCid = msg->m_BroadcastCid;
            for (CK_CLASSID cid = 0; cid < g_MaxClassID; ++cid) {
                if (!CKIsChildClassOf(cid, broadcastCid))
                    continue;

                int count = m_Context->GetObjectsCountByClassID(cid);
                CK_ID *ids = m_Context->GetObjectsListByClassID(cid);
                for (int j = 0; j < count; ++j) {
                    CKBeObject *obj = (CKBeObject *)m_Context->GetObject(ids[j]);
                    AddMessageToObject(obj, msg, currentScene, FALSE);
                }
            }
        } else {
            CKObject *recipient = msg->GetRecipient();
            AddMessageToObject(recipient, msg, currentScene, TRUE);
        }

        if (msgType >= 0 && msgType < m_RegisteredMessageTypes.Size()) {
            CKWaitingObjectArray *&waitingList = m_MsgWaitingList[msgType];
            if (!waitingList) continue;

            for (CKWaitingObjectArray::Iterator it = waitingList->Begin(); it != waitingList->End();) {
                CKBeObject *waitTarget = it->m_BeObject;
                bool received = false;

                if (!waitTarget) {
                    // Global listener, receives all messages of this type
                    received = true;
                } else {
                    switch (sendType) {
                    case CK_MESSAGE_SINGLE:
                        received = (waitTarget == msg->GetRecipient());
                        break;
                    case CK_MESSAGE_GROUP:
                        received = waitTarget->IsInGroup((CKGroup *) msg->GetRecipient());
                        break;
                    case CK_MESSAGE_BROADCAST:
                        received = CKIsChildClassOf(waitTarget, msg->m_BroadcastCid);
                        break;
                    }
                }

                if (received) {
                    if (it->m_Output && (it->m_Behavior->GetFlags() & CKBEHAVIOR_WAITSFORMESSAGE)) {
                        it->m_Output->Activate();
                    }
                    it->m_Behavior->ModifyFlags(0, CKBEHAVIOR_WAITSFORMESSAGE);
                    it = waitingList->Remove(it);
                } else {
                    ++it;
                }
            }
        }
    }

    for (int i = 0; i < m_ReceivedMsgThisFrame.Size(); ++i) {
        CKMessage *msg = m_ReceivedMsgThisFrame[i];
        if (msg) msg->Release();
    }
    m_ReceivedMsgThisFrame.Clear();
    return CK_OK;
}

CKERROR CKMessageManager::OnCKReset() {
    for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
        if (m_MsgWaitingList[i]) {
            m_MsgWaitingList[i]->Clear();
        }
    }

    m_ReceivedMsgThisFrame.Clear();

    for (int i = 0; i < m_LastFrameObjects.Size(); ++i) {
        CKBeObject *obj = (CKBeObject *)m_LastFrameObjects[i];
        if (obj) {
            obj->RemoveAllLastFrameMessage();
        }
    }
    m_LastFrameObjects.Clear();

    return CK_OK;
}

CKERROR CKMessageManager::SequenceToBeDeleted(CK_ID *objids, int count) {
    for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
        CKWaitingObjectArray *waitingList = m_MsgWaitingList[i];
        if (!waitingList) continue;

        for (auto it = waitingList->Begin(); it != waitingList->End();) {
            CKWaitingObject &wo = *it;
            if (wo.m_BeObject->IsToBeDeleted() || wo.m_Output->IsToBeDeleted()) {
                it = waitingList->Remove(it);
            } else {
                ++it;
            }
        }
    }

    m_LastFrameObjects.Check();
    return CK_OK;
}

CKMessageManager::~CKMessageManager() {
    if (m_MsgWaitingList) {
        for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
            CKWaitingObjectArray *waitingList = m_MsgWaitingList[i];
            if (waitingList) {
                waitingList->Clear();
                delete waitingList;
            }
        }

        delete[] m_MsgWaitingList;
        m_MsgWaitingList = nullptr;
    }

    for (int i = 0; i < m_ReceivedMsgThisFrame.Size(); ++i) {
        CKMessage *msg = m_ReceivedMsgThisFrame[i];
        if (msg) {
            msg->Release();
        }
    }
    m_ReceivedMsgThisFrame.Clear();

    m_LastFrameObjects.Clear();

    m_RegisteredMessageTypes.Clear();
}

CKMessageManager::CKMessageManager(CKContext *Context) : CKBaseManager(Context, MESSAGE_MANAGER_GUID, (CKSTRING)"Message Manager") {
    m_MsgWaitingList = nullptr;
    RegisterDefaultMessages();
    Context->RegisterNewManager(this);
}

void CKMessageManager::AddMessageToObject(CKObject *obj, CKMessage *msg, CKScene *currentscene, CKBOOL recurse) {
    if (!obj || !msg) return;

    if (CKIsChildClassOf(obj, CKCID_BEOBJECT)) {
        CKBeObject *beo = (CKBeObject *) obj;
        if (beo->IsWaitingForMessages() && currentscene->IsObjectActive(beo)) {
            beo->AddLastFrameMessage(msg);
            m_LastFrameObjects.AddIfNotHere(beo);
        }

        if (recurse) {
            if (msg->GetSendingType() == CK_MESSAGE_GROUP && obj->GetClassID() == CKCID_GROUP) {
                CKGroup *group = (CKGroup *) obj;
                for (int i = 0; i < group->GetObjectCount(); ++i) {
                    AddMessageToObject(group->GetObject(i), msg, currentscene, TRUE);
                }
            } else if (obj->GetClassID() == CKCID_BODYPART) {
                CKBodyPart *bodyPart = (CKBodyPart *) obj;
                CKCharacter *character = bodyPart->GetCharacter();
                if (character) {
                    AddMessageToObject(character, msg, currentscene, TRUE);
                }
            }
        }
    }
}

CKMessage *CKMessageManager::CreateMessageSingle(CKMessageType type, CKBeObject *dest, CKBeObject *sender) {
    CKMessage *msg = new CKMessage(m_Context);
    if (msg) {
        msg->m_MessageType = type;
        msg->m_Sender = sender ? sender->GetID() : 0;
        msg->m_Recipient = dest ? dest->GetID() : 0;
        msg->m_SendingType = CK_MESSAGE_SINGLE;
    }
    return msg;
}

CKMessage *CKMessageManager::CreateMessageGroup(CKMessageType type, CKGroup *group, CKBeObject *sender) {
    CKMessage *msg = new CKMessage(m_Context);
    if (msg) {
        msg->m_MessageType = type;
        msg->m_Sender = sender ? sender->GetID() : 0;
        msg->m_Recipient = group ? group->GetID() : 0;
        msg->m_SendingType = CK_MESSAGE_GROUP;
    }
    return msg;
}

CKMessage *CKMessageManager::CreateMessageBroadcast(CKMessageType type, CK_CLASSID objType, CKBeObject *sender) {
    CKMessage *msg = new CKMessage(m_Context);
    if (msg) {
        msg->m_MessageType = type;
        msg->m_Sender = sender ? sender->GetID() : 0;
        msg->m_SendingType = CK_MESSAGE_BROADCAST;
        msg->SetBroadcastObjectType(objType);
    }
    return msg;
}
