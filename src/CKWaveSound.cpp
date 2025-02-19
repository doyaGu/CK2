#include "CKWaveSound.h"

CK_CLASSID CKWaveSound::m_ClassID = CKCID_WAVESOUND;

CKSOUNDHANDLE
CKWaveSound::PlayMinion(CKBOOL Background, CK3dEntity *Ent, VxVector *Position, VxVector *Direction, float MinDelay) {
    return nullptr;
}

CKERROR CKWaveSound::SetSoundFileName(const CKSTRING FileName) {
    return 0;
}

CKSTRING CKWaveSound::GetSoundFileName() {
    return nullptr;
}

int CKWaveSound::GetSoundLength() {
    return 0;
}

CKERROR CKWaveSound::GetSoundFormat(CKWaveFormat &Format) {
    return 0;
}

CK_WAVESOUND_TYPE CKWaveSound::GetType() {
    return CK_WAVESOUND_POINT;
}

void CKWaveSound::SetType(CK_WAVESOUND_TYPE Type) {

}

CKDWORD CKWaveSound::GetState() {
    return 0;
}

void CKWaveSound::SetState(CKDWORD State) {

}

void CKWaveSound::SetPriority(float Priority) {

}

float CKWaveSound::GetPriority() {
    return 0;
}

void CKWaveSound::SetLoopMode(CKBOOL Enabled) {

}

CKBOOL CKWaveSound::GetLoopMode() {
    return 0;
}

CKERROR CKWaveSound::SetFileStreaming(CKBOOL Enabled, CKBOOL RecreateSound) {
    return 0;
}

CKBOOL CKWaveSound::GetFileStreaming() {
    return 0;
}

void CKWaveSound::Play(float FadeIn, float FinalGain) {

}

void CKWaveSound::Resume() {

}

void CKWaveSound::Rewind() {

}

void CKWaveSound::Stop(float FadeOut) {

}

void CKWaveSound::Pause() {

}

CKBOOL CKWaveSound::IsPlaying() {
    return 0;
}

CKBOOL CKWaveSound::IsPaused() {
    return 0;
}

void CKWaveSound::SetGain(float Gain) {

}

float CKWaveSound::GetGain() {
    return 0;
}

void CKWaveSound::SetPitch(float Rate) {

}

float CKWaveSound::GetPitch() {
    return 0;
}

void CKWaveSound::SetPan(float Pan) {

}

float CKWaveSound::GetPan() {
    return 0;
}

CKSOUNDHANDLE CKWaveSound::GetSource() {
    return nullptr;
}

void CKWaveSound::PositionSound(CK3dEntity *Object, VxVector *Position, VxVector *Direction, CKBOOL Commit) {

}

CK3dEntity *CKWaveSound::GetAttachedEntity() {
    return nullptr;
}

void CKWaveSound::GetPosition(VxVector &Pos) {

}

void CKWaveSound::GetDirection(VxVector &Dir) {

}

void CKWaveSound::GetSound3DInformation(VxVector &Pos, VxVector &Dir, float &DistanceFromListener) {

}

void CKWaveSound::SetCone(float InAngle, float OutAngle, float OutsideGain) {

}

void CKWaveSound::GetCone(float *InAngle, float *OutAngle, float *OutsideGain) {

}

void CKWaveSound::SetMinMaxDistance(float MinDistance, float MaxDistance, CKDWORD MaxDistanceBehavior) {

}

void CKWaveSound::GetMinMaxDistance(float *MinDistance, float *MaxDistance, CKDWORD *MaxDistanceBehavior) {

}

void CKWaveSound::SetVelocity(VxVector &Pos) {

}

void CKWaveSound::GetVelocity(VxVector &Pos) {

}

void CKWaveSound::SetOrientation(VxVector &Dir, VxVector &Up) {

}

void CKWaveSound::GetOrientation(VxVector &Dir, VxVector &Up) {

}

CKERROR CKWaveSound::WriteData(CKBYTE *Buffer, int Buffersize) {
    return 0;
}

CKERROR
CKWaveSound::Lock(CKDWORD WriteCursor, CKDWORD NumBytes, void **Ptr1, CKDWORD *Bytes1, void **Ptr2, CKDWORD *Bytes2,
                  CK_WAVESOUND_LOCKMODE Flags) {
    return 0;
}

CKERROR CKWaveSound::Unlock(void *Ptr1, CKDWORD Bytes1, void *Ptr2, CKDWORD Bytes2) {
    return 0;
}

CKDWORD CKWaveSound::GetPlayPosition() {
    return 0;
}

int CKWaveSound::GetPlayedMs() {
    return 0;
}

CKERROR CKWaveSound::Create(CKBOOL FileStreaming, CKSTRING Filename) {
    if (FileStreaming) {
        m_State |= CK_WAVESOUND_FILESTREAMED;
    } else {
        m_State &= ~CK_WAVESOUND_FILESTREAMED;
    }
    SetSoundFileName(Filename);
    SaveSettings();
    return Recreate();
}

CKERROR CKWaveSound::Create(CK_WAVESOUND_TYPE Type, CKWaveFormat *Format, int Size) {
    if (!Format || !m_SoundManager)
        return CKERR_INVALIDPARAMETER;
    Release();

    m_Source = m_SoundManager->CreateSource(Type, Format, Size, GetFileStreaming());
    if (!m_Source)
        return CKERR_INVALIDPARAMETER;

    m_BufferSize = Size;
    m_WaveFormat = *Format;
    return CK_OK;
}

CKERROR CKWaveSound::SetReader(CKSoundReader *Reader) {
    return 0;
}

CKSoundReader *CKWaveSound::GetReader() {
    return nullptr;
}

void CKWaveSound::SetDataToRead(int Size) {

}

CKERROR CKWaveSound::Recreate(CKBOOL Safe) {
    return 0;
}

void CKWaveSound::Release() {

}

CKERROR CKWaveSound::TryRecreate() {
    return 0;
}

void CKWaveSound::UpdatePosition(float deltaT) {

}

void CKWaveSound::UpdateFade() {

}

CKERROR CKWaveSound::WriteDataFromReader() {
    return 0;
}

void CKWaveSound::FillWithBlanks(CKBOOL IncBf) {

}

void CKWaveSound::InternalStop() {

}

CKWaveSound::CKWaveSound(CKContext *Context, CKSTRING Name): CKSound(Context, Name) {
    m_Source = nullptr;
    m_FinalGain = 1.0;
    m_FadeTime = 0.0;
    m_CurrentTime = 0.0;
    m_State = 1;
    m_Position = VxVector();
    m_OldPosition = VxVector();
    m_OldPosition = VxVector(0.0f, 0.0f, 1.0f);
    m_BufferPos = -1;
    m_SoundReader = nullptr;
    m_DataRead = 0;
    m_DataPlayed = 0;
    m_OldCursorPos = 0;
    m_DataToRead = 0;
    m_BufferSize = 0;
    m_AttachedObject = 0;
    m_Duration = 0;
    memset(&m_WaveFormat, 0, sizeof(m_WaveFormat));
    m_SoundManager = (CKSoundManager *)m_Context->GetManagerByGuid(SOUND_MANAGER_GUID);
}

CKWaveSound::~CKWaveSound() {
    Release();
}

CK_CLASSID CKWaveSound::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKWaveSound::Save(CKFile *File, CKDWORD Flags) {
    return CKSound::Save(File, Flags);
}

CKERROR CKWaveSound::Load(CKStateChunk *Chunk, CKFile *File) {
    return CKSound::Load(Chunk, File);
}

CKERROR CKWaveSound::RemapDependencies(CKDependenciesContext &context) {
    return CKBeObject::RemapDependencies(context);
}

CKERROR CKWaveSound::Copy(CKObject &o, CKDependenciesContext &context) {
    return CKBeObject::Copy(o, context);
}

void CKWaveSound::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKWaveSound::GetMemoryOccupation() {
    return CKSound::GetMemoryOccupation();
}

CKSTRING CKWaveSound::GetClassName() {
    return "Wave Sound";
}

int CKWaveSound::GetDependenciesCount(int Mode) {
    return CKObject::GetDependenciesCount(Mode);
}

CKSTRING CKWaveSound::GetDependencies(int i, int Mode) {
    return CKObject::GetDependencies(i, Mode);
}

void CKWaveSound::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKBeObject::m_ClassID);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_WAVESOUND);
}

CKWaveSound *CKWaveSound::CreateInstance(CKContext *Context) {
    return new CKWaveSound(Context);
}

int CKWaveSound::GetDistanceFromCursor() {
    return 0;
}

void CKWaveSound::InternalSetGain(float Gain) {

}

void CKWaveSound::SaveSettings() {

}

void CKWaveSound::RestoreSettings() {

}

void *CKWaveSound::ReallocSource(void *oSource, int alreadyWritten, int newSize) {
    return nullptr;
}
