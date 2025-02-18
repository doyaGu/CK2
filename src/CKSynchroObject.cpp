#include "CKSynchroObject.h"

#include "CKBeObject.h"

CK_CLASSID CKSynchroObject::m_ClassID = CKCID_SYNCHRO;

void CKSynchroObject::Reset() {
    m_Passed.Clear();
    m_Arrived.Clear();
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

CKBeObject *CKSynchroObject::GetRendezVousArrivedObject(int pos) {
    return nullptr;
}

CKSynchroObject::CKSynchroObject(CKContext *Context, CKSTRING name) : CKObject(Context, name) {
    m_MaxWaiters = 0;
}

CKSynchroObject::~CKSynchroObject() {
    Reset();
}

CK_CLASSID CKSynchroObject::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKSynchroObject::Save(CKFile *file, CKDWORD flags) {
    return CKObject::Save(file, flags);
}

CKERROR CKSynchroObject::Load(CKStateChunk *chunk, CKFile *file) {
    return CKObject::Load(chunk, file);
}

void CKSynchroObject::CheckPostDeletion() {
    m_Arrived.Check(m_Context);
    m_Passed.Check(m_Context);
}

int CKSynchroObject::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation() + 44 + 12 * m_Arrived.GetCount() + 12 * m_Passed.GetCount();
}

CKBOOL CKSynchroObject::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    if (CKIsChildClassOf(obj, CKCID_BEOBJECT)) {
        if (m_Arrived.PtrFind(obj) || m_Passed.PtrFind(obj))
            return TRUE;
    }
    return CKObject::IsObjectUsed(obj, cid);
}

CKSTRING CKSynchroObject::GetClassName() {
    return "Synchronization Object";
}

int CKSynchroObject::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKSynchroObject::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKSynchroObject::Register() {
    CKClassNeedNotificationFrom(m_ClassID, CKBeObject::m_ClassID);
    CKClassRegisterDefaultOptions(m_ClassID, 4);
    CKClassRegisterAssociatedParameter(m_ClassID, CKPGUID_SYNCHRO);
}

CKSynchroObject *CKSynchroObject::CreateInstance(CKContext *Context) {
    return new CKSynchroObject(Context);
}

CK_CLASSID CKStateObject::m_ClassID;

CKBOOL CKStateObject::IsStateActive() {
    return FALSE;
}

void CKStateObject::EnterState() {
}

void CKStateObject::LeaveState() {
}

CKStateObject::CKStateObject(CKContext *Context, CKSTRING name) {
}

CKStateObject::~CKStateObject() {
}

CK_CLASSID CKStateObject::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKStateObject::Save(CKFile *file, CKDWORD flags) {
    return nullptr;
}

CKERROR CKStateObject::Load(CKStateChunk *chunk, CKFile *file) {
    return CKERROR();
}

int CKStateObject::GetMemoryOccupation() {
    return 0;
}

CKSTRING CKStateObject::GetClassName() {
    return CKSTRING();
}

int CKStateObject::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKStateObject::GetDependencies(int i, int mode) {
    return CKSTRING();
}

void CKStateObject::Register() {
}

CKStateObject *CKStateObject::CreateInstance(CKContext *Context) {
    return nullptr;
}

CK_CLASSID CKCriticalSectionObject::m_ClassID;

void CKCriticalSectionObject::Reset() {
}

CKBOOL CKCriticalSectionObject::EnterCriticalSection(CKBeObject *asker) {
    return CKBOOL();
}

CKBOOL CKCriticalSectionObject::LeaveCriticalSection(CKBeObject *asker) {
    return CKBOOL();
}

CKCriticalSectionObject::CKCriticalSectionObject(CKContext *Context, CKSTRING name) {
}

CKCriticalSectionObject::~CKCriticalSectionObject() {
}

CK_CLASSID CKCriticalSectionObject::GetClassID() {
    return m_ClassID;
}

CKStateChunk *CKCriticalSectionObject::Save(CKFile *file, CKDWORD flags) {
    return nullptr;
}

CKERROR CKCriticalSectionObject::Load(CKStateChunk *chunk, CKFile *file) {
    return CKERROR();
}

void CKCriticalSectionObject::CheckPostDeletion() {
}

int CKCriticalSectionObject::GetMemoryOccupation() {
    return 0;
}

CKBOOL CKCriticalSectionObject::IsObjectUsed(CKObject *obj, CK_CLASSID cid) {
    return CKBOOL();
}

CKSTRING CKCriticalSectionObject::GetClassName() {
    return CKSTRING();
}

int CKCriticalSectionObject::GetDependenciesCount(int mode) {
    return 0;
}

CKSTRING CKCriticalSectionObject::GetDependencies(int i, int mode) {
    return CKSTRING();
}

void CKCriticalSectionObject::Register() {
}

CKCriticalSectionObject *CKCriticalSectionObject::CreateInstance(CKContext *Context) {
    return nullptr;
}
