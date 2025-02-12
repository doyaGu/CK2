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

    m_RegisteredMessageTypes.PushBack(MsgName);
    m_MsgWaitingList->Resize(m_RegisteredMessageTypes.Size());
    return 0;
}

CKSTRING CKMessageManager::GetMessageTypeName(CKMessageType MsgType) {
    if (MsgType >= 0 && MsgType < m_RegisteredMessageTypes.Size())
        return nullptr;
    return m_RegisteredMessageTypes[MsgType].Str();
}

int CKMessageManager::GetMessageTypeCount() {
    return m_RegisteredMessageTypes.Size();
}

void CKMessageManager::RenameMessageType(CKMessageType MsgType, CKSTRING NewName) {
    if (MsgType >= 0 && MsgType < m_RegisteredMessageTypes.Size())
        m_RegisteredMessageTypes[MsgType] = NewName;
}

void CKMessageManager::RenameMessageType(CKSTRING OldName, CKSTRING NewName) {
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
    if (!dest)
        return nullptr;

    CKMessage *msg = CreateMessageSingle(MsgType, dest, sender);
    if (msg) {
        SendMessage(msg);
        msg->Release();
    }
    return msg;
}

CKMessage *CKMessageManager::SendMessageGroup(CKMessageType MsgType, CKGroup *group, CKBeObject *sender) {
    if (!group)
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

    CKWaitingObjectArray &waitList = m_MsgWaitingList[MsgType];

    for (XArray<CKWaitingObject>::Iterator it = waitList.Begin(); it != waitList.End(); ++it) {
        if (it->m_Behavior == behav && it->m_Output == output)
            return CKERR_ALREADYPRESENT;
    }

    CKWaitingObject newEntry = {};
    newEntry.m_Behavior = behav;
    newEntry.m_Output = output;
    newEntry.m_BeObject = obj;
    waitList.PushBack(newEntry);

    return CK_OK;
}

CKERROR CKMessageManager::RegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate, CKBeObject *obj) {
    return RegisterWait(AddMessageType(MsgName), behav, OutputToActivate, obj);
}

CKERROR CKMessageManager::UnRegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate) {
    if (MsgType < 0 || MsgType >= m_RegisteredMessageTypes.Size())
        return CKERR_INVALIDMESSAGE;

    XArray<CKWaitingObject> &waitList = m_MsgWaitingList[MsgType];
    if (waitList.IsEmpty())
        return CK_OK;

    CKBehaviorIO *targetOutput = behav ? behav->GetOutput(OutputToActivate) : nullptr;

    // Remove matching entries
    for (XArray<CKWaitingObject>::Iterator it = waitList.Begin(); it != waitList.End();) {
        if (it->m_Behavior == behav && (!targetOutput || it->m_Output == targetOutput)) {
            it = waitList.Remove(it);
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
    XBitArray usedMessageTypes(1);
    CKStateChunk *chunk = nullptr;

    CKParameterType msgParamType = m_Context->GetParameterManager()->ParameterGuidToType(CKPGUID_MESSAGE);

    const XArray<CKFileObject> &fileObjects = SavedFile->m_FileObjects;

    const int paramOutCount = SavedFile->m_IndexByClassId[CKCID_PARAMETEROUT].Size();
    for (int i = 0; i < paramOutCount; ++i) {
        CKParameterOut *param = (CKParameterOut *)fileObjects[SavedFile->m_IndexByClassId[CKCID_PARAMETEROUT][i]].
            ObjPtr;
        if (param && param->GetType() == msgParamType) {
            int msgType;
            param->GetValue(&msgType);
            if (msgType >= 0)
                usedMessageTypes.Set(msgType);
        }
    }

    const int paramLocalCount = SavedFile->m_IndexByClassId[CKCID_PARAMETERLOCAL].Size();
    for (int i = 0; i < paramLocalCount; ++i) {
        CKParameterLocal *param = static_cast<CKParameterLocal *>(
            fileObjects[SavedFile->m_IndexByClassId[CKCID_PARAMETERLOCAL][i]].ObjPtr
        );
        if (param && param->GetType() == msgParamType) {
            int msgType;
            param->GetValue(&msgType);
            if (msgType >= 0) usedMessageTypes.Set(msgType);
        }
    }

    if (usedMessageTypes.GetSetBitPosition(0) > 0) {
        chunk = new CKStateChunk(CKCID_MESSAGEMANAGER, SavedFile);
        chunk->StartWrite();
        chunk->WriteIdentifier(0x53);

        const int typeCount = m_RegisteredMessageTypes.Size();
        chunk->WriteInt(typeCount);

        for (int i = 0; i < typeCount; ++i) {
            XString &typeName = m_RegisteredMessageTypes[i];
            chunk->WriteString(usedMessageTypes.IsSet(i) ? typeName.Str() : (CKSTRING)"");
        }

        chunk->CloseChunk();
    }

    return chunk;
}

CKERROR CKMessageManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile) {
    if (!chunk)
        return CKERR_INVALIDPARAMETER;

    chunk->StartRead();
    if (!chunk->SeekIdentifier(0x53))
        return CK_OK;

    const int typeCount = chunk->ReadInt();
    if (typeCount <= 0)
        return CK_OK;

    XArray<CKSTRING> typeNames(typeCount);
    XArray<CKMessageType> typeIDs(typeCount);

    for (int i = 0; i < typeCount; ++i) {
        CKSTRING name;
        chunk->ReadString(&name);
        typeNames.PushBack(name);
    }

    for (int i = 0; i < typeCount; ++i) {
        CKMessageType id = AddMessageType(typeNames[i]);
        typeIDs.PushBack(id);
    }

    LoadedFile->RemapManagerInt(MESSAGE_MANAGER_GUID, typeIDs.Begin(), typeCount);

    for (int i = 0; i < typeCount; ++i) {
        CKDeletePointer(typeNames[i]);
    }

    return CK_OK;
}

CKERROR CKMessageManager::PreClearAll() {
    for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
        CKWaitingObjectArray *waitingArray = &m_MsgWaitingList[i];
        if (waitingArray) {
            waitingArray->Clear();
            delete waitingArray;
        }
    }

    delete[] m_MsgWaitingList;
    m_MsgWaitingList = nullptr;

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
            CKWaitingObjectArray *waitingList = &m_MsgWaitingList[msgType];
            if (!waitingList)
                continue;

            for (CKWaitingObject *it = waitingList->Begin(); it != waitingList->End();) {
                CKBeObject *beo = it->m_BeObject;
                CKObject *recipient = msg->GetRecipient();

                bool shouldReceive = false;
                if (beo == recipient) {
                    switch (sendType) {
                    case CK_MESSAGE_BROADCAST:
                        shouldReceive = CKIsChildClassOf(beo, msg->m_BroadcastCid);
                        break;
                    case CK_MESSAGE_GROUP:
                        shouldReceive = beo->IsInGroup((CKGroup *)recipient);
                        break;
                    case CK_MESSAGE_SINGLE:
                        shouldReceive = CKIsChildClassOf(beo, CKCID_BODYPART) && beo == ((CKBodyPart *)recipient)->GetCharacter();
                    default:
                        break;
                    }
                }

                if (shouldReceive) {
                    beo->AddLastFrameMessage(msg);
                    m_LastFrameObjects.AddIfNotHere(beo);

                    if (it->m_Output && it->m_Behavior->GetFlags() & CKBEHAVIOR_WAITSFORMESSAGE) {
                        it->m_Output->Activate();
                    }

                    it->m_Behavior->ModifyFlags(0, CKBEHAVIOR_WAITSFORMESSAGE);

                    waitingList->Remove(it);
                    break;
                }

                ++it;
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
        CKWaitingObjectArray *waitingArray = &m_MsgWaitingList[i];
        if (waitingArray) {
            waitingArray->Clear();
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
        CKWaitingObjectArray *waitingList = &m_MsgWaitingList[i];
        if (!waitingList)
            continue;

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
    for (int i = 0; i < m_RegisteredMessageTypes.Size(); ++i) {
        CKWaitingObjectArray *waitingList = &m_MsgWaitingList[i];
        if (waitingList) {
            waitingList->Clear();
            delete waitingList;
        }
    }

    delete[] m_MsgWaitingList;
    m_MsgWaitingList = nullptr;

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
    RegisterDefaultMessages();
    Context->RegisterNewManager(this);
}

void CKMessageManager::AddMessageToObject(CKObject *obj, CKMessage *msg, CKScene *currentscene, CKBOOL recurse) {
    if (!obj || !msg)
        return;

    if (CKIsChildClassOf(obj, CKCID_BEOBJECT)) {
        CKBeObject *beo = (CKBeObject *)obj;
        if (beo->IsWaitingForMessages() && currentscene->IsObjectActive(beo)) {
            beo->AddLastFrameMessage(msg);
            m_LastFrameObjects.AddIfNotHere(beo);
        }

        if (recurse) {
            if (CKIsChildClassOf(obj, CKCID_BODYPART)) {
                CKBodyPart *bodyPart = (CKBodyPart *)obj;
                CKCharacter *character = bodyPart->GetCharacter();
                if (character) {
                    AddMessageToObject(character, msg, currentscene, recurse);
                }
            }

            if (msg->GetSendingType() == CK_MESSAGE_GROUP && CKIsChildClassOf(obj, CKCID_GROUP)) {
                CKGroup *group = (CKGroup *)obj;
                const int count = group->GetObjectCount();
                for (int i = 0; i < count; ++i) {
                    AddMessageToObject(group->GetObject(i), msg, currentscene, recurse);
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
        msg->m_BroadcastCid = group ? group->GetClassID() : 0;
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
