#include "CKBehaviorManager.h"

CKERROR CKBehaviorManager::Execute(float delta) {
    return 0;
}

int CKBehaviorManager::GetObjectsCount() {
    return 0;
}

CKBeObject *CKBehaviorManager::GetObject(int pos) {
    return nullptr;
}

int CKBehaviorManager::GetBehaviorMaxIteration() {
    return 0;
}

void CKBehaviorManager::SetBehaviorMaxIteration(int n) {

}

int CKBehaviorManager::RemoveObjectNextFrame(CKBeObject *beo) {
    return 0;
}

CKStateChunk *CKBehaviorManager::SaveData(CKFile *SavedFile) {
    return CKBaseManager::SaveData(SavedFile);
}

CKERROR CKBehaviorManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile) {
    return CKBaseManager::LoadData(chunk, LoadedFile);
}

CKERROR CKBehaviorManager::OnCKPlay() {
    return CKBaseManager::OnCKPlay();
}

CKERROR CKBehaviorManager::OnCKPause() {
    return CKBaseManager::OnCKPause();
}

CKERROR CKBehaviorManager::PreProcess() {
    return CKBaseManager::PreProcess();
}

CKERROR CKBehaviorManager::PreClearAll() {
    return CKBaseManager::PreClearAll();
}

CKERROR CKBehaviorManager::SequenceDeleted(CK_ID *objids, int count) {
    return CKBaseManager::SequenceDeleted(objids, count);
}

CKERROR CKBehaviorManager::SequenceToBeDeleted(CK_ID *objids, int count) {
    return CKBaseManager::SequenceToBeDeleted(objids, count);
}

CKBehaviorManager::CKBehaviorManager(CKContext *Context) : CKBaseManager(Context, BEHAVIOR_MANAGER_GUID, "Behavior Manager") {

}