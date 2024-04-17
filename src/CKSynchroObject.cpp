#include "CKSynchroObject.h"

CK_CLASSID CKSynchroObject::m_ClassID;

void CKSynchroObject::Reset() {

}

void CKSynchroObject::SetRendezVousNumberOfWaiters(int waiters) {

}

int CKSynchroObject::GetRendezVousNumberOfWaiters() {
    return 0;
}

CKBOOL CKSynchroObject::CanIPassRendezVous(CKBeObject *asker) {
    return 0;
}

int CKSynchroObject::GetRendezVousNumberOfArrivedObjects() {
    return 0;
}

//CKBeObject *CKSynchroObject::GetRendezVousArrivedObject(int pos) {
//    return nullptr;
//}

CKSynchroObject::CKSynchroObject(CKContext *Context, CKSTRING name) : CKObject(Context, name) {

}

CKSynchroObject::~CKSynchroObject() {

}

CK_CLASSID CKSynchroObject::GetClassID() {
    return CKObject::GetClassID();
}

CKStateChunk *CKSynchroObject::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKSynchroObject::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKSynchroObject::CheckPostDeletion() {
    CKObject::CheckPostDeletion();
}

int CKSynchroObject::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation();
}

CKBOOL CKSynchroObject::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(obj, cid);
}

CKSTRING CKSynchroObject::GetClassNameA() {
    return nullptr;
}

int CKSynchroObject::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKSynchroObject::GetDependencies(int i, int mode) {
    return nullptr;
}

void CKSynchroObject::Register() {

}

CKSynchroObject *CKSynchroObject::CreateInstance(CKContext *Context) {
    return nullptr;
}

CKBOOL CKStateObject::IsStateActive()
{
    return FALSE;
}

CK_CLASSID CKCriticalSectionObject::m_ClassID;

void CKCriticalSectionObject::Reset()
{
}

CKBOOL CKCriticalSectionObject::EnterCriticalSection(CKBeObject* asker)
{
    return CKBOOL();
}

CKBOOL CKCriticalSectionObject::LeaveCriticalSection(CKBeObject* asker)
{
    return CKBOOL();
}

CKCriticalSectionObject::CKCriticalSectionObject(CKContext* Context, CKSTRING name)
{
}

CKCriticalSectionObject::~CKCriticalSectionObject()
{
}

CK_CLASSID CKCriticalSectionObject::GetClassID()
{
    return CK_CLASSID();
}

CKStateChunk* CKCriticalSectionObject::Save(CKFile* file, CKDWORD flags)
{
    return nullptr;
}

CKERROR CKCriticalSectionObject::Load(CKStateChunk* chunk, CKFile* file)
{
    return CKERROR();
}

void CKCriticalSectionObject::CheckPostDeletion()
{
}

int CKCriticalSectionObject::GetMemoryOccupation()
{
    return 0;
}

CKBOOL CKCriticalSectionObject::IsObjectUsed(CKObject* obj, CK_CLASSID cid)
{
    return CKBOOL();
}

CKSTRING CKCriticalSectionObject::GetClassName()
{
    return CKSTRING();
}

int CKCriticalSectionObject::GetDependenciesCount(int mode)
{
    return 0;
}

CKSTRING CKCriticalSectionObject::GetDependencies(int i, int mode)
{
    return CKSTRING();
}

void CKCriticalSectionObject::Register()
{
}

CKCriticalSectionObject* CKCriticalSectionObject::CreateInstance(CKContext* Context)
{
    return nullptr;
}
