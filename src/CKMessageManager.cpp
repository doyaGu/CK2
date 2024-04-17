#include "CKMessageManager.h"


CKMessageType CKMessageManager::AddMessageType(CKSTRING MsgName) {
    if (!MsgName || MsgName[0] == '\0')
        return -1;

    m_RegistredMessageTypes.PushBack(MsgName);
    m_MsgWaitingList->Resize(m_RegistredMessageTypes.Size());
    return 0;
}

CKSTRING CKMessageManager::GetMessageTypeName(CKMessageType MsgType) {
    if (MsgType >= 0 && MsgType < m_RegistredMessageTypes.Size())
        return NULL;
    return m_RegistredMessageTypes[MsgType].Str();
}

int CKMessageManager::GetMessageTypeCount() {
    return m_RegistredMessageTypes.Size();
}

void CKMessageManager::RenameMessageType(CKMessageType MsgType, CKSTRING NewName) {
    if (MsgType >= 0 && MsgType < m_RegistredMessageTypes.Size())
        m_RegistredMessageTypes[MsgType] = NewName;
}

void CKMessageManager::RenameMessageType(CKSTRING OldName, CKSTRING NewName) {
    RenameMessageType(AddMessageType(OldName), NewName);
}

CKERROR CKMessageManager::SendMessage(CKMessage *msg) {
    return 0;
}

CKMessage *CKMessageManager::SendMessageSingle(CKMessageType MsgType, CKBeObject *dest, CKBeObject *sender) {
    return nullptr;
}

CKMessage *CKMessageManager::SendMessageGroup(CKMessageType MsgType, CKGroup *group, CKBeObject *sender) {
    return nullptr;
}

CKMessage *CKMessageManager::SendMessageBroadcast(CKMessageType MsgType, CK_CLASSID id, CKBeObject *sender) {
    return nullptr;
}

CKERROR
CKMessageManager::RegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate, CKBeObject *obj) {
    return 0;
}

CKERROR CKMessageManager::RegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate, CKBeObject *obj) {
    return RegisterWait(AddMessageType(MsgName), behav, OutputToActivate, obj);
}

CKERROR CKMessageManager::UnRegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate) {
    return 0;
}

CKERROR CKMessageManager::UnRegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate) {
    return UnRegisterWait(AddMessageType(MsgName), behav, OutputToActivate);
}

CKERROR CKMessageManager::RegisterDefaultMessages() {
    return 0;
}

CKStateChunk *CKMessageManager::SaveData(CKFile *SavedFile) {
    return CKBaseManager::SaveData(SavedFile);
}

CKERROR CKMessageManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile) {
    return CKBaseManager::LoadData(chunk, LoadedFile);
}

CKERROR CKMessageManager::PreClearAll() {
    return CKBaseManager::PreClearAll();
}

CKERROR CKMessageManager::PostProcess() {
    return CKBaseManager::PostProcess();
}

CKERROR CKMessageManager::OnCKReset() {
    return CKBaseManager::OnCKReset();
}

CKERROR CKMessageManager::SequenceToBeDeleted(CK_ID *objids, int count) {
    return CKBaseManager::SequenceToBeDeleted(objids, count);
}

CKMessageManager::~CKMessageManager() {

}

CKMessageManager::CKMessageManager(CKContext *Context) : CKBaseManager(Context, MESSAGE_MANAGER_GUID, (CKSTRING)"Message Manager") {
    RegisterDefaultMessages();
    Context->RegisterNewManager(this);
}

void CKMessageManager::AddMessageToObject(CKObject *obj, CKMessage *msg, CKScene *currentscene, CKBOOL recurse) {

}

CKMessage *CKMessageManager::CreateMessage() {
    return nullptr;
}