#include "CKParameter.h"

#include "CKAttributeManager.h"
#include "CKMessageManager.h"
#include "CKParameterManager.h"
#include "CKParameterOut.h"
#include "CKFile.h"

CK_CLASSID CKParameter::m_ClassID = CKCID_PARAMETER;

CKObject *CKParameter::GetValueObject(CKBOOL update) {
    CK_ID *idPtr = (CK_ID *)GetReadDataPtr(update);
    return m_Context->GetObject(*idPtr);
}

CKERROR CKParameter::GetValue(void *buf, CKBOOL update) {
    if (!buf) {
        return CKERR_INVALIDPARAMETER;
    }

    if (!m_Buffer) {
        return CKERR_NOTINITIALIZED;
    }

    if (m_Size == 0) {
        return CKERR_NOTINITIALIZED;
    }

    memcpy(buf, m_Buffer, m_Size);
    return CK_OK;
}

CKERROR CKParameter::SetValue(const void *buf, int size) {
    if (size > 0 && size != m_Size) {
        CKBYTE *newBuffer = new CKBYTE[size];
        if (!newBuffer)
            return CKERR_OUTOFMEMORY;
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_Size = size;
    }

    if (!m_Buffer) {
        return CKERR_NOTINITIALIZED;
    }

    if (m_Size == 0) {
        return CKERR_NOTINITIALIZED;
    }

    // Size check for memcpy
    int copySize = m_Size;
    if (size > 0 && size < m_Size) {
        copySize = size;
    }

    if (buf) {
        memcpy(m_Buffer, buf, copySize);
    }

    return CK_OK;
}

CKERROR CKParameter::CopyValue(CKParameter *param, CKBOOL UpdateParam) {
    if (!param) {
        return CKERR_INVALIDPARAMETER;
    }

    if (!IsCompatibleWith(param)) {
        return CKERR_INVALIDPARAMETERTYPE;
    }

    if (UpdateParam) {
        if ((param->m_ObjectFlags & CK_PARAMETEROUT_PARAMOP) != 0) {
            ((CKParameterOut *)param)->Update();
        }
    }

    m_ParamType->CopyFunction(this, param);
    return CK_OK;
}

CKBOOL CKParameter::IsCompatibleWith(CKParameter *param) {
    if (!param) {
        return FALSE;
    }

    int type1 = param->GetType();
    int type2 = GetType();

    CKParameterManager *paramManager = m_Context->GetParameterManager();
    return paramManager->IsTypeCompatible(type1, type2);
}

int CKParameter::GetDataSize() {
    return m_Size;
}

void *CKParameter::GetReadDataPtr(CKBOOL update) {
    return m_Buffer;
}

void *CKParameter::GetWriteDataPtr() {
    return m_Buffer;
}

CKERROR CKParameter::SetStringValue(CKSTRING Value) {
    if (m_ParamType && m_ParamType->Valid && m_ParamType->StringFunction) {
        CK_PARAMETERSTRINGFUNCTION func = m_ParamType->StringFunction;
        return func(this, Value, TRUE);
    }

    return CKERR_INVALIDOPERATION;
}

int CKParameter::GetStringValue(CKSTRING Value, CKBOOL update) {
    if (m_ParamType && m_ParamType->Valid && m_ParamType->StringFunction) {
        CK_PARAMETERSTRINGFUNCTION func = m_ParamType->StringFunction;
        return func(this, Value, FALSE);
    }

    if (Value) {
        *Value = '\0';
    }

    return CK_OK;
}

CKParameterType CKParameter::GetType() {
    return m_ParamType ? m_ParamType->Index : -1;
}

void CKParameter::SetType(CKParameterType type) {
    CKParameterManager *pm = m_Context->GetParameterManager();
    CKParameterTypeDesc *desc = pm->GetParameterTypeDescription(type);

    if (!desc) {
        return; // Invalid type, do nothing
    }

    // If the type is already the same, no need to change it
    if (m_ParamType == desc) {
        return;
    }

    // Check if we need to replace the type completely
    bool requiresFullReplacement = false;

    if (!m_ParamType || desc->CreateDefaultFunction || m_ParamType->DeleteFunction) {
        requiresFullReplacement = true;
    } else {
        // Ensure derivation masks are up to date
        if (!pm->m_DerivationMasksUpToDate) {
            pm->UpdateDerivationTables();
        }

        CKParameterTypeDesc *oldType = m_ParamType;
        bool isCompatible = false;

        // Check type compatibility using derivation masks
        if (oldType->DerivationMask.IsSet(desc->Index) || desc->DerivationMask.IsSet(oldType->Index)) {
            isCompatible = true;
        }

        // Check class ID compatibility
        if (oldType->Cid == 0 || CKIsChildClassOf(oldType->Cid, desc->Cid)) {
            isCompatible = true;
        }

        // If the new type is compatible, just update the type reference
        if (isCompatible) {
            m_ParamType = desc;
            return;
        }

        // Otherwise, we need a full replacement
        requiresFullReplacement = true;
    }

    // Perform full type replacement if necessary
    if (requiresFullReplacement) {
        if (m_ParamType && m_ParamType->Valid && m_ParamType->DeleteFunction) {
            m_ParamType->DeleteFunction(this);
        }

        // Free existing memory
        delete[] m_Buffer;

        // Assign the new parameter type
        m_ParamType = desc;
        m_Size = desc->DefaultSize;

        if (m_Size > 0) {
            m_Buffer = new CKBYTE[m_Size];
            memset(m_Buffer, 0, m_Size);
        } else {
            m_Buffer = nullptr;
        }

        // Call CreateDefaultFunction if available
        if (m_ParamType->CreateDefaultFunction) {
            m_ParamType->CreateDefaultFunction(this);
        }
    }
}

CKGUID CKParameter::GetGUID() {
    if (m_ParamType) {
        return m_ParamType->Guid;
    }
    return CKGUID();
}

void CKParameter::SetGUID(CKGUID guid) {
    int paramType = m_Context->m_ParameterManager->ParameterGuidToType(guid);
    SetType(paramType);
}

CK_CLASSID CKParameter::GetParameterClassID() {
    return m_ParamType ? m_ParamType->Cid : 0;
}

void CKParameter::SetOwner(CKObject *o) {
    m_Owner = o;
}

CKObject *CKParameter::GetOwner() {
    return m_Owner;
}

void CKParameter::Enable(CKBOOL act) {
    if (act) {
        m_ObjectFlags &= ~CK_PARAMETERIN_DISABLED;
    } else {
        m_ObjectFlags |= CK_PARAMETERIN_DISABLED;
    }
}

CKBOOL CKParameter::IsEnabled() {
    return !(m_ObjectFlags & CK_PARAMETERIN_DISABLED);
}

CKParameter::CKParameter(CKContext *Context, CKSTRING name, int type): CKObject(Context, name) {
    CKParameterManager *pm = Context->GetParameterManager();
    m_ParamType = pm->GetParameterTypeDescription(type);
    m_Owner = nullptr;
    m_Buffer = nullptr;

    if (m_ParamType) {
        // Get the default size of the parameter
        m_Size = m_ParamType->DefaultSize;

        // Allocate memory if the default size is greater than 0
        if (m_Size > 0) {
            CKBYTE *newBuffer = new CKBYTE[m_Size];
            m_Buffer = newBuffer;
            memset(newBuffer, 0, m_Size);
        }

        // Call the create default function if it exists
        if (m_ParamType->CreateDefaultFunction) {
            m_ParamType->CreateDefaultFunction(this);
        }
    }
}

CKParameter::~CKParameter() {
    if (m_ParamType && m_ParamType->Valid && m_ParamType->DeleteFunction) {
        m_ParamType->DeleteFunction(this);
    }

    delete[] m_Buffer;
}

CK_CLASSID CKParameter::GetClassID() {
    return m_ClassID;
}

void CKParameter::PreSave(CKFile *file, CKDWORD flags) {
    CKObject::PreSave(file, flags);

    // Check if parameter type is valid and if it is a child of class 1
    if (m_ParamType && CKIsChildClassOf(m_ParamType->Cid, CKCID_OBJECT)) {
        // Retrieve the GUID
        CKGUID guid = m_ParamType->Guid;

        if (guid == CKPGUID_STATE ||
            guid == CKPGUID_CRITICALSECTION ||
            guid == CKPGUID_SYNCHRO) {
            CK_ID ObjID = 0;
            GetValue(&ObjID, TRUE);

            // Retrieve the object
            CKObject *ObjectA = m_Context->GetObjectA(ObjID);

            // Save the object
            file->SaveObject(ObjectA, flags);
        }
    }
}

CKStateChunk *CKParameter::Save(CKFile *file, CKDWORD flags) {
    CKStateChunk *chunk = new CKStateChunk(CKCID_PARAMETER, file);
    CKStateChunk *baseChunk = CKObject::Save(file, flags);

    chunk->StartWrite();
    chunk->AddChunkAndDelete(baseChunk);

    CKParameterManager *pm = m_Context->GetParameterManager();
    chunk->WriteIdentifier(0x40);

    chunk->WriteGuid(GetGUID());

    // Check object flags
    if ((m_ObjectFlags & CK_OBJECT_ONLYFORFILEREFERENCE) == 0) {
        if ((m_ObjectFlags & CK_PARAMETEROUT_PARAMOP) != 0 || m_ParamType == 0) {
            chunk->WriteDword(3);
        } else if (m_ParamType->Saver_Manager.IsValid()) {
            // Handle Saver_Manager case
            int savedValue = 0;
            GetValue(&savedValue, TRUE);
            chunk->WriteManagerInt(m_ParamType->Saver_Manager, savedValue);
        } else if (m_ParamType->SaveLoadFunction) {
            // Call SaveLoadFunction
            chunk->WriteInt(0);
            CKStateChunk *subChunk = nullptr;
            m_ParamType->SaveLoadFunction(this, &subChunk, FALSE);
            chunk->WriteSubChunk(subChunk);

            // Free subChunk if allocated
            if (subChunk) {
                delete subChunk;
            }
        } else if (CKIsChildClassOf(m_ParamType->Cid, CKCID_OBJECT)) {
            // Handle Object Reference case
            chunk->WriteDword(2);
            CK_ID objID = 0;
            GetValue(&objID, TRUE);
            CKObject *obj = m_Context->GetObject(objID);
            chunk->WriteObject(obj);
        } else {
            chunk->WriteDword(1);

            if (GetGUID() == CKPGUID_PARAMETERTYPE) {
                // Special case handling
                CKGUID typeGUID = pm->ParameterTypeToGuid(*m_Buffer);
                chunk->WriteGuid(typeGUID);
            } else {
                chunk->WriteBuffer(m_Size, m_Buffer);
            }
        }
    }

    if (GetClassID() == CKCID_PARAMETER) {
        chunk->CloseChunk();
    } else {
        chunk->UpdateDataSize();
    }

    return chunk;
}

CKERROR CKParameter::Load(CKStateChunk *chunk, CKFile *file) {
    if (!chunk) {
        return -1;
    }

    // Call base class Load
    CKObject::Load(chunk, file);

    // Clear specific flags in m_ObjectFlags
    m_ObjectFlags &= ~(CK_PARAMETEROUT_SETTINGS |
        CK_PARAMETERIN_DISABLED |
        CK_PARAMETERIN_THIS |
        CK_PARAMETERIN_SHARED |
        CK_PARAMETEROUT_DELETEAFTERUSE);

    // Ensure the identifier exists in the chunk
    if (!chunk->SeekIdentifier(0x40))
        return CK_OK;

    // Read GUID from chunk
    CKParameterManager *pm = m_Context->GetParameterManager();
    CKGUID paramGUID = chunk->ReadGuid();
    if (paramGUID == CKPGUID_OLDMESSAGE) {
        paramGUID = CKPGUID_MESSAGE;
    } else if (paramGUID == CKPGUID_OLDATTRIBUTE) {
        paramGUID = CKPGUID_ATTRIBUTE;
    } else if (paramGUID == CKPGUID_ID) {
        paramGUID = CKPGUID_OBJECT;
    } else if (paramGUID == CKPGUID_OLDTIME) {
        paramGUID = CKPGUID_TIME;
    }

    // Check if the current parameter type matches the loaded GUID
    if (m_ParamType) {
        if (m_ParamType->Guid != paramGUID || m_ParamType->DefaultSize != m_Size) {
            if (m_ParamType->Valid && m_ParamType->DeleteFunction) {
                m_ParamType->DeleteFunction(this);
            }

            delete[] m_Buffer;

            m_ParamType = pm->GetParameterTypeDescription(paramGUID);
            if (m_ParamType) {
                m_Size = m_ParamType->DefaultSize;
                if (m_Size > 0) {
                    m_Buffer = new CKBYTE[m_Size];
                    memset(m_Buffer, 0, m_Size);
                } else {
                    m_Buffer = nullptr;
                }

                if (m_ParamType->CreateDefaultFunction) {
                    m_ParamType->CreateDefaultFunction(this);
                }
            } else {
                m_Size = 0;
                m_Buffer = nullptr;
            }
        }
    } else {
        delete[] m_Buffer;

        m_ParamType = pm->GetParameterTypeDescription(paramGUID);

        if (m_ParamType) {
            m_Size = m_ParamType->DefaultSize;
            if (m_Size > 0) {
                m_Buffer = new CKBYTE[m_Size];
                memset(m_Buffer, 0, m_Size);
            } else {
                m_Buffer = nullptr;
            }

            if (m_ParamType->CreateDefaultFunction) {
                m_ParamType->CreateDefaultFunction(this);
            }
        } else {
            m_Size = 0;
            m_Buffer = nullptr;
        }
    }

    // Read and process data
    CKDWORD paramState = chunk->ReadDword();
    if (!m_ParamType) {
        m_Context->OutputToConsoleExBeep("%s : Unknown Parameter Type (DLL declaring this parameter may be missing)", m_Name);
        return CKERR_INVALIDPARAMETERTYPE;
    }

    if (paramState == 3) {
        return CK_OK; // No data
    }

    if (paramState == 0 && m_ParamType->SaveLoadFunction) {
        CKStateChunk *subChunk = chunk->ReadSubChunk();
        m_ParamType->SaveLoadFunction(this, &subChunk, TRUE);

        // Clean up sub-chunk
        if (subChunk) {
            DeleteCKStateChunk(subChunk);
        }
        return CK_OK;
    }

    if (paramState != 1) {
        CK_ID objID = 0;
        if (paramState == 2) {
            objID = chunk->ReadObjectID();
        } else {
            chunk->ReadInt();
            objID = chunk->ReadInt();
        }
        SetValue(&objID, sizeof(CK_ID));
        return CK_OK;
    }

    if (paramGUID == CKPGUID_PARAMETERTYPE) {
        CKGUID guid = chunk->ReadGuid();
        CKParameterType typeID = pm->ParameterGuidToType(guid);
        SetValue(&typeID, sizeof(CKParameterType));
        return CK_OK;
    }

    char *buffer = nullptr;
    int bufferSize = chunk->ReadBuffer((void **)&buffer);

    if (paramGUID == CKPGUID_OLDMESSAGE) {
        CKMessageManager *msgManager = m_Context->GetMessageManager();
        CKMessageType msgType = msgManager->AddMessageType(buffer);
        SetValue(&msgType, sizeof(CKMessageType));
    } else if (paramGUID == CKPGUID_OLDATTRIBUTE) {
        CKAttributeManager *attrManager = m_Context->GetAttributeManager();
        CKAttributeType attrType = attrManager->GetAttributeTypeByName(buffer);
        SetValue(&attrType, sizeof(CKAttributeType));
    } else if (paramGUID == CKPGUID_OLDTIME) {
        float time = *(float *)buffer;
        SetValue(&time, sizeof(float));
    }

    if ((m_ParamType->dwFlags & CKPARAMETERTYPE_VARIABLESIZE) != 0 || buffer && bufferSize > 0)
        SetValue(buffer, bufferSize);

    CKDeletePointer(buffer);

    return CK_OK;
}

void CKParameter::CheckPostDeletion() {
    if (m_ParamType && m_ParamType->CheckFunction) {
        m_ParamType->CheckFunction(this);
    }
}

int CKParameter::GetMemoryOccupation() {
    return CKObject::GetMemoryOccupation() + 16 + m_Size;
}

int CKParameter::IsObjectUsed(CKObject *o, CK_CLASSID cid) {
    return CKObject::IsObjectUsed(o, cid);
}

CKERROR CKParameter::PrepareDependencies(CKDependenciesContext &context) {
    CKERROR err = CKObject::PrepareDependencies(context);
    if (err != CK_OK) {
        return err;
    }

    if (context.IsInMode(CK_DEPENDENCIES_DELETE) && m_ParamType && m_ParamType->Valid && m_ParamType->DeleteFunction) {
        m_ParamType->DeleteFunction(this);
        m_ParamType = nullptr;
    } else if (context.IsInMode(CK_DEPENDENCIES_BUILD)) {
        if (GetParameterClassID()) {
            CKObject *object = m_Context->GetObjectA(*reinterpret_cast<CK_ID *>(m_Buffer));
            if (object) {
                object->PrepareDependencies(context);
            }
        }

        // Check if the GUID matches a specific value and handle array dependencies
        CKGUID paramGUID = GetGUID();
        if (paramGUID == CKPGUID_OBJECTARRAY) {
            XSObjectArray *objs = nullptr;
            GetValue(&objs, FALSE);
            if (objs) {
                objs->Prepare(context);
            }
        }
    }

    return context.FinishPrepareDependencies(this, m_ClassID);
}

CKERROR CKParameter::RemapDependencies(CKDependenciesContext &context) {
    CKERROR err = CKObject::RemapDependencies(context);
    if (err != CK_OK) {
        return err;
    }

    m_Owner = context.Remap(m_Owner);

    if (m_ParamType) {
        // If the parameter type has a class ID, remap buffer
        if (m_ParamType->Cid) {
            CK_ID *id = (CK_ID *)m_Buffer;
            CK_ID newID = context.RemapID(*id);
            if (newID != 0) {
                *id = newID;
            }
        } else if (m_ParamType->Guid == CKPGUID_OBJECTARRAY) {
            XSObjectArray *objs = *(XSObjectArray **)m_Buffer;
            objs->Remap(context);
        }
    }

    return CK_OK;
}

CKERROR CKParameter::Copy(CKObject &o, CKDependenciesContext &context) {
    CKERROR err = CKObject::Copy(o, context);
    if (err != CK_OK) {
        return err;
    }

    m_ObjectFlags &= ~CK_OBJECT_PARAMMASK;
    m_ObjectFlags |= o.GetObjectFlags() & CK_OBJECT_PARAMMASK;

    CKParameter *param = (CKParameter *)&o;
    m_Owner = param->m_Owner;

    if (param->m_ParamType) {
        SetType(param->m_ParamType->Index);
    } else {
        SetType(-1);
    }

    CopyValue(param, FALSE);

    return CK_OK;
}

CKSTRING CKParameter::GetClassName() {
    return "Parameter";
}

int CKParameter::GetDependenciesCount(int mode) {
    return CKObject::GetDependenciesCount(mode);
}

CKSTRING CKParameter::GetDependencies(int i, int mode) {
    return CKObject::GetDependencies(i, mode);
}

void CKParameter::Register() {
    CKCLASSNOTIFYFROM(CKParameter, CKObject);
    CKCLASSDEFAULTOPTIONS(CKParameter, 1);
}

CKParameter *CKParameter::CreateInstance(CKContext *Context) {
    return new CKParameter(Context);
}

void CKParameter::MessageDeleteAfterUse(CKBOOL act) {
    if (act) {
        m_ObjectFlags |= CK_OBJECT_TEMPMARKER;
    } else {
        m_ObjectFlags &= ~CK_OBJECT_TEMPMARKER;
    }
}
