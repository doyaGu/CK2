#include "CKMessage.h"

CKERROR CKMessage::SetBroadcastObjectType(CK_CLASSID type) {
    return 0;
}

CKERROR CKMessage::AddParameter(CKParameter *, CKBOOL DeleteParameterWithMessage) {
    return 0;
}

CKERROR CKMessage::RemoveParameter(CKParameter *) {
    return 0;
}

int CKMessage::GetParameterCount() {
    return 0;
}

CKParameter *CKMessage::GetParameter(int pos) {
    return nullptr;
}

CKMessage::CKMessage(CKContext *context) {
    m_MessageType = -1;
    m_SendingType = CK_MESSAGE_SINGLE;
    m_Sender = 0;
    m_Parameters = NULL;
    m_RefCount = 1;
    m_Context = context;
    m_BroadcastCid = 0;
}

CKMessage::~CKMessage() {
    if (m_Parameters)
    {
        for (XObjectArray::Iterator it = m_Parameters->Begin(); it != m_Parameters->End(); ++it)
        {
            CKObject *obj = m_Context->GetObject(*it);
            if (obj && obj->GetObjectFlags() & CK_PARAMETEROUT_DELETEAFTERUSE)
            {
                CKDestroyObject(obj, 3);
            }
        }

        delete m_Parameters;
    }

}

int CKMessage::AddRef() {
    return ++m_RefCount;
}

int CKMessage::Release() {
    int count = --m_RefCount;
    if (m_RefCount <= 0) {
        delete this;
        return 0;
    }
    return count;
}