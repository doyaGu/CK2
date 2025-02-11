#include "CKSound.h"

CK_SOUND_SAVEOPTIONS CKSound::GetSaveOptions() {
    return CKSOUND_EXTERNAL;
}

CK_CLASSID CKSound::m_ClassID;

void CKSound::SetSaveOptions(CK_SOUND_SAVEOPTIONS Options) {

}

CKSound::CKSound(CKContext *Context, CKSTRING name) : CKBeObject(Context, name) {

}

CKSound::~CKSound() {

}

CK_CLASSID CKSound::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKSound::Save(CKFile *file, CKDWORD flags) {
    return CKBeObject::Save(file, flags);
}

CKERROR CKSound::Load(CKStateChunk *chunk, CKFile *file) {
    return CKBeObject::Load(chunk, file);
}

int CKSound::GetMemoryOccupation() {
    return CKBeObject::GetMemoryOccupation();
}

CKSTRING CKSound::GetClassNameA() {
    return nullptr;
}

int CKSound::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKSound::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKSound::Register() {

}

CKSound *CKSound::CreateInstance(CKContext *Context) {
    return nullptr;
}
