#include "CKMidiSound.h"

CK_CLASSID CKMidiSound::m_ClassID = CKCID_MIDISOUND;

CKERROR CKMidiSound::SetSoundFileName(CKSTRING filename) {
    return 0;
}

CKSTRING CKMidiSound::GetSoundFileName() {
    return nullptr;
}

CKDWORD CKMidiSound::GetCurrentPos() {
    return 0;
}

CKERROR CKMidiSound::Play() {
    return 0;
}

CKERROR CKMidiSound::Stop() {
    return 0;
}

CKERROR CKMidiSound::Pause(CKBOOL pause) {
    return 0;
}

CKBOOL CKMidiSound::IsPlaying() {
    return 0;
}

CKBOOL CKMidiSound::IsPaused() {
    return 0;
}

CKMidiSound::CKMidiSound(CKContext *Context, CKSTRING name) : CKSound(Context, name) {

}

CKMidiSound::~CKMidiSound() {

}

CK_CLASSID CKMidiSound::GetClassID() {
    return CKSound::GetClassID();
}

CKStateChunk *CKMidiSound::Save(CKFile *file, CKDWORD flags) {
    return CKSound::Save(file, flags);
}

CKERROR CKMidiSound::Load(CKStateChunk *chunk, CKFile *file) {
    return CKSound::Load(chunk, file);
}

int CKMidiSound::GetMemoryOccupation() {
    return CKSound::GetMemoryOccupation();
}

CKSTRING CKMidiSound::GetClassNameA() {
    return nullptr;
}

int CKMidiSound::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKMidiSound::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKMidiSound::Register() {

}

CKMidiSound *CKMidiSound::CreateInstance(CKContext *Context) {
    return nullptr;
}

CKERROR CKMidiSound::OpenFile() {
    return 0;
}

CKERROR CKMidiSound::CloseFile() {
    return 0;
}

CKERROR CKMidiSound::Prepare() {
    return 0;
}

CKERROR CKMidiSound::Start() {
    return 0;
}
