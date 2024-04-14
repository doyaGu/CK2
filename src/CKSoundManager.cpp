#include "CKSoundManager.h"

void CKSoundManager::SetListener(CK3dEntity *listener) {

}

CK3dEntity *CKSoundManager::GetListener() {
    return nullptr;
}

void CKSoundManager::SetStreamedBufferSize(CKDWORD bsize) {

}

CKDWORD CKSoundManager::GetStreamedBufferSize() {
    return 0;
}

SoundMinion *CKSoundManager::CreateMinion(CKSOUNDHANDLE source, float minimumdelay) {
    return nullptr;
}

void CKSoundManager::ReleaseMinions() {

}

void CKSoundManager::PauseMinions() {

}

void CKSoundManager::ResumeMinions() {

}

void CKSoundManager::ProcessMinions() {

}

CKSoundManager::CKSoundManager(CKContext *Context, CKSTRING smname) : CKBaseManager(Context, SOUND_MANAGER_GUID, smname) {

}

CKSoundManager::~CKSoundManager() {

}

void CKSoundManager::RegisterAttribute() {

}

CKERROR CKSoundManager::SequenceDeleted(CK_ID *objids, int count) {
    return CKBaseManager::SequenceDeleted(objids, count);
}

CKERROR CKSoundManager::PostClearAll() {
    return CKBaseManager::PostClearAll();
}
