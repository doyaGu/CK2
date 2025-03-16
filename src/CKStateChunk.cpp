#include "CKStateChunk.h"

#include "VxMatrix.h"
#include "VxWindowFunctions.h"
#include "CKFile.h"
#include "CKPluginManager.h"

#include <miniz.h>

XObjectPointerArray CKStateChunk::m_TempXOPA;
XObjectArray CKStateChunk::m_TempXOA;

CKStateChunk *CreateCKStateChunk(CK_CLASSID id, CKFile *file) {
    return new CKStateChunk(id, file);
}

CKStateChunk *CreateCKStateChunk(CKStateChunk *chunk) {
    return new CKStateChunk(*chunk);
}

void DeleteCKStateChunk(CKStateChunk *chunk) {
    delete chunk;
}

CKStateChunk::CKStateChunk(CKStateChunk *chunk) {
    m_Data = NULL;
    m_ChunkClassID = 0;
    m_ChunkSize = 0;
    m_ChunkParser = NULL;
    m_Ids = NULL;
    m_Chunks = NULL;
    m_DataVersion = 0;
    m_ChunkVersion = 0;
    m_Managers = NULL;
    m_File = NULL;
    m_Dynamic = TRUE;
    Clone(chunk);
}

CKStateChunk::CKStateChunk(CK_CLASSID Cid, CKFile *f) {
    m_Data = NULL;
    m_ChunkSize = 0;
    m_ChunkParser = NULL;
    m_Ids = NULL;
    m_Chunks = NULL;
    m_Managers = NULL;
    m_DataVersion = 0;
    m_ChunkVersion = 0;
    m_ChunkClassID = Cid;
    m_File = f;
    m_Dynamic = TRUE;
}

CKStateChunk::~CKStateChunk() {
    Clear();
}

void CKStateChunk::Clear() {
    m_ChunkClassID = 0;
    m_ChunkSize = 0;

    delete[] m_Data;
    delete m_ChunkParser;
    if (m_Ids)
        delete m_Ids;
    if (m_Chunks)
        delete m_Chunks;
    if (m_Managers)
        delete m_Managers;

    m_Managers = NULL;
    m_Data = NULL;
    m_Chunks = NULL;
    m_ChunkParser = NULL;
    m_Ids = NULL;
}

void CKStateChunk::Clone(CKStateChunk *chunk) {
    if (!chunk)
        return;

    Clear();

    m_DataVersion = chunk->m_DataVersion;
    m_ChunkVersion = chunk->m_ChunkVersion;
    m_ChunkClassID = chunk->m_ChunkClassID;
    m_ChunkSize = chunk->m_ChunkSize;

    if (chunk->m_ChunkParser)
        m_ChunkParser = new ChunkParser(*chunk->m_ChunkParser);

    if (chunk->m_Ids)
        m_Ids = new IntListStruct(*chunk->m_Ids);

    if (chunk->m_Chunks)
        m_Chunks = new IntListStruct(*chunk->m_Chunks);

    if (chunk->m_Managers)
        m_Managers = new IntListStruct(*chunk->m_Managers);

    if (chunk->m_Data) {
        m_Data = new int[m_ChunkSize];
        memcpy(m_Data, chunk->m_Data, m_ChunkSize * sizeof(int));
        if (m_ChunkParser)
            m_ChunkParser->DataSize = m_ChunkSize;
    }
    m_File = chunk->m_File;
}

CK_CLASSID CKStateChunk::GetChunkClassID() {
    return m_ChunkClassID;
}

short int CKStateChunk::GetChunkVersion() {
    return m_ChunkVersion;
}

short int CKStateChunk::GetDataVersion() {
    return m_DataVersion;
}

void CKStateChunk::SetDataVersion(short int version) {
    m_DataVersion = version;
}

int CKStateChunk::GetDataSize() {
    return m_ChunkSize * (int) sizeof(int);
}

void CKStateChunk::StartRead() {
    if (m_ChunkParser) {
        m_ChunkParser->CurrentPos = 0;
        m_ChunkParser->PrevIdentifierPos = 0;
    } else {
        m_ChunkParser = new ChunkParser;
    }
    m_ChunkParser->DataSize = m_ChunkSize;
}

void CKStateChunk::StartWrite() {
    delete[] m_Data;
    m_Data = NULL;
    m_ChunkVersion = CHUNK_VERSION4;
    if (m_ChunkParser) {
        m_ChunkParser->CurrentPos = 0;
        m_ChunkParser->PrevIdentifierPos = 0;
        m_ChunkParser->DataSize = 0;
    } else {
        m_ChunkParser = new ChunkParser;
    }
}

void CKStateChunk::CheckSize(int size) {
    if (!m_ChunkParser)
        return;

    int sz = size / sizeof(int);
    if (sz + m_ChunkParser->CurrentPos > m_ChunkParser->DataSize) {
        if (sz < 500)
            sz = 500;
        int *data = new int[m_ChunkParser->DataSize + sz];
        if (m_Data) {
            memcpy(data, m_Data, m_ChunkParser->CurrentPos * sizeof(int));
            delete[] m_Data;
        }
        m_Data = data;
        m_ChunkParser->DataSize = m_ChunkParser->DataSize + sz;
    }
}

void CKStateChunk::CloseChunk() {
    if (!m_ChunkParser)
        return;

    if (m_ChunkParser->CurrentPos > m_ChunkSize)
        m_ChunkSize = m_ChunkParser->CurrentPos;

    if (m_ChunkSize > 0) {
        if (m_ChunkSize + 10 < m_ChunkParser->DataSize) {
            int *data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(data, m_Data, m_ChunkSize * sizeof(int));
                delete[] m_Data;
            }
            m_Data = data;
        }
    } else {
        delete[] m_Data;
        m_Data = NULL;
    }

    delete m_ChunkParser;
    m_ChunkParser = NULL;

    if (m_Ids)
        m_Ids->Compact();
    if (m_Chunks)
        m_Chunks->Compact();
    if (m_Managers)
        m_Managers->Compact();
}

void CKStateChunk::UpdateDataSize() {
    if (m_ChunkParser) {
        if (m_ChunkParser->CurrentPos > m_ChunkSize)
            m_ChunkSize = m_ChunkParser->CurrentPos;
    }
}

void CKStateChunk::WriteIdentifier(CKDWORD id) {
    if (!m_ChunkParser)
        StartWrite();

    CheckSize(8);
    if (m_ChunkParser->PrevIdentifierPos < m_ChunkParser->CurrentPos)
        m_Data[m_ChunkParser->PrevIdentifierPos + 1] = m_ChunkParser->CurrentPos;
    m_ChunkParser->PrevIdentifierPos = m_ChunkParser->CurrentPos;
    m_Data[m_ChunkParser->CurrentPos++] = (int) id;
    m_Data[m_ChunkParser->CurrentPos++] = 0;
}

CKDWORD CKStateChunk::ReadIdentifier() {
    if (m_ChunkParser->CurrentPos >= m_ChunkSize)
        return 0;
    m_ChunkParser->PrevIdentifierPos = m_ChunkParser->CurrentPos;
    m_ChunkParser->CurrentPos += 2;
    return m_Data[m_ChunkParser->CurrentPos - 2];
}

CKBOOL CKStateChunk::SeekIdentifier(CKDWORD identifier) {
    if (!m_Data || m_ChunkSize == 0)
        return 0;

    if (!m_ChunkParser) {
        m_ChunkParser = new ChunkParser;
        m_ChunkParser->DataSize = m_ChunkSize;
    }

    if (m_ChunkParser->PrevIdentifierPos >= m_ChunkSize - 1)
        return FALSE;

    int j = m_Data[m_ChunkParser->PrevIdentifierPos + 1];
    int i;
    if (j != 0) {
        i = j;
        while (i < m_ChunkSize && m_Data[i] != identifier) {
            if (i + 1 >= m_ChunkSize)
                return FALSE;

            i = m_Data[i + 1];
            if (i == 0) {
                while (i < m_ChunkSize && m_Data[i] != identifier) {
                    if (i + 1 >= m_ChunkSize)
                        return FALSE;

                    i = m_Data[i + 1];
                    if (i == j)
                        return FALSE;
                }
            }
        }
    } else {
        i = 0;
        while (i < m_ChunkSize && m_Data[i] != identifier) {
            if (i + 1 >= m_ChunkSize)
                return FALSE;

            i = m_Data[i + 1];
            if (i == j)
                return FALSE;
        }
    }

    if (i >= m_ChunkSize)
        return FALSE;

    m_ChunkParser->PrevIdentifierPos = i;
    m_ChunkParser->CurrentPos = i + 2;

    return TRUE;
}

int CKStateChunk::SeekIdentifierAndReturnSize(CKDWORD identifier) {
    if (!m_Data || m_ChunkSize == 0)
        return -1;

    if (!m_ChunkParser) {
        m_ChunkParser = new ChunkParser;
        m_ChunkParser->DataSize = m_ChunkSize;
    }

    int i = 0;
    while (m_Data[i] != identifier) {
        i = m_Data[i + 1];
        if (i == 0)
            return -1;
    }
    m_ChunkParser->PrevIdentifierPos = i;
    int j = m_Data[i + 1];
    if (j == 0)
        j = m_ChunkSize;
    m_ChunkParser->CurrentPos = i + 2;
    return (j - i - 2) * sizeof(int);
}

void CKStateChunk::Skip(int DwordCount) {
    CheckSize(DwordCount * sizeof(int));
    if (m_ChunkParser)
        m_ChunkParser->CurrentPos += DwordCount;
}

void CKStateChunk::Goto(int DwordOffset) {
    if (m_ChunkParser)
        m_ChunkParser->CurrentPos = DwordOffset;
}

int CKStateChunk::GetCurrentPos() {
    if (m_ChunkParser)
        return m_ChunkParser->CurrentPos;
    else
        return -1;
}

void *CKStateChunk::LockWriteBuffer(int DwordCount) {
    if (!m_ChunkParser)
        return NULL;
    CheckSize(DwordCount * sizeof(int));
    return &m_Data[m_ChunkParser->CurrentPos];
}

void *CKStateChunk::LockReadBuffer() {
    if (!m_ChunkParser)
        return NULL;
    return &m_Data[m_ChunkParser->CurrentPos];
}

void CKStateChunk::AddChunk(CKStateChunk *chunk) {
    if (!m_ChunkParser || !chunk || !chunk->m_Data || chunk->m_ChunkSize == 0)
        return;

    CheckSize(chunk->m_ChunkSize * sizeof(int));
    memcpy(&m_Data[m_ChunkParser->CurrentPos], chunk->m_Data, chunk->m_ChunkSize * sizeof(int));
    if (m_ChunkParser->PrevIdentifierPos != m_ChunkParser->CurrentPos)
        m_Data[m_ChunkParser->PrevIdentifierPos + 1] = m_ChunkParser->CurrentPos;

    int j = m_ChunkParser->CurrentPos;
    if (m_Data[m_ChunkParser->CurrentPos + 1] != 0) {
        do {
            int *p = &m_Data[m_ChunkParser->CurrentPos + 1];
            j = *p + m_ChunkParser->CurrentPos;
            *p = j;
        } while (m_Data[j + 1] != 0);
    }

    if (chunk->m_Ids) {
        if (!m_Ids)
            m_Ids = new IntListStruct;
        m_Ids->Append(chunk->m_Ids, m_ChunkParser->CurrentPos);
    }

    if (chunk->m_Chunks) {
        if (!m_Chunks)
            m_Chunks = new IntListStruct;
        m_Chunks->Append(chunk->m_Chunks, m_ChunkParser->CurrentPos);
    }

    if (chunk->m_Managers) {
        if (!m_Managers)
            m_Managers = new IntListStruct;
        m_Managers->Append(chunk->m_Managers, m_ChunkParser->CurrentPos);
    }

    m_ChunkParser->PrevIdentifierPos = j;
    m_ChunkParser->CurrentPos += chunk->m_ChunkSize;;
    m_DataVersion = chunk->m_DataVersion;
    m_ChunkVersion = chunk->m_ChunkVersion;
    m_File = chunk->m_File;
}

void CKStateChunk::AddChunkAndDelete(CKStateChunk *chunk) {
    if (!m_ChunkParser || !chunk || !chunk->m_Data)
        return;

    m_Dynamic = chunk->m_Dynamic;
    if (m_ChunkParser->DataSize != 0
        || m_ChunkParser->CurrentPos != 0
        || m_ChunkParser->PrevIdentifierPos != 0
        || m_Ids
        || m_Chunks
        || m_Managers) {
        AddChunk(chunk);
    } else {
        if (m_ChunkParser) {
            m_ChunkParser = chunk->m_ChunkParser;
            chunk->m_ChunkParser->Clear();
        }
        m_ChunkSize = chunk->m_ChunkSize;
        m_Data = chunk->m_Data;
        m_Ids = chunk->m_Ids;
        m_Chunks = chunk->m_Chunks;
        m_Managers = chunk->m_Managers;
        m_File = chunk->m_File;
        m_DataVersion = chunk->m_DataVersion;
        m_ChunkVersion = chunk->m_ChunkVersion;
        chunk->m_ChunkSize = 0;
        chunk->m_Data = NULL;
        chunk->m_Managers = NULL;
        chunk->m_Chunks = NULL;
        chunk->m_Ids = NULL;
    }

    delete chunk;
}

void CKStateChunk::StartManagerSequence(CKGUID man, int count) {
    if (!m_Managers)
        m_Managers = new IntListStruct;

    m_Managers->AddEntries(m_ChunkParser->CurrentPos);
    CheckSize((count + 3) + sizeof(int));
    m_Data[m_ChunkParser->CurrentPos++] = count;
    m_Data[m_ChunkParser->CurrentPos++] = man.d1;
    m_Data[m_ChunkParser->CurrentPos++] = man.d2;
}

void CKStateChunk::WriteManagerSequence(int val) {
    m_Data[m_ChunkParser->CurrentPos++] = val;
}

void CKStateChunk::WriteManagerInt(CKGUID Manager, int val) {
    if (!m_Managers)
        m_Managers = new IntListStruct;

    m_Managers->AddEntry(m_ChunkParser->CurrentPos);
    m_Data[m_ChunkParser->CurrentPos++] = Manager.d1;
    m_Data[m_ChunkParser->CurrentPos++] = Manager.d2;
    m_Data[m_ChunkParser->CurrentPos++] = val;
}

void CKStateChunk::WriteArray_LEndian(int elementCount, int elementSize, void *srcData) {
    if (srcData && elementCount > 0 && elementSize > 0) {
        // Calculate total bytes and required dword count (rounding up)
        const int totalBytes = elementSize * elementCount;
        const int dwordCount = (totalBytes + 3) / 4; // Convert bytes to 4-byte dwords

        // Ensure buffer has space for: [totalBytes (int)] + [count (int)] + data
        CheckSize(8 + (dwordCount * 4)); // 8 bytes header + data size

        // Write array header
        m_Data[m_ChunkParser->CurrentPos++] = totalBytes;
        m_Data[m_ChunkParser->CurrentPos++] = elementCount;

        // Copy array data
        memcpy(&m_Data[m_ChunkParser->CurrentPos], srcData, totalBytes);
        m_ChunkParser->CurrentPos += dwordCount;
    } else {
        // Write empty array marker
        CheckSize(8); // Space for two zero ints
        m_Data[m_ChunkParser->CurrentPos++] = 0;
        m_Data[m_ChunkParser->CurrentPos++] = 0;
    }
}

void CKStateChunk::WriteArray_LEndian16(int elementCount, int elementSize, void *srcData) {
    WriteArray_LEndian(elementCount, elementSize, srcData);
}

int CKStateChunk::StartManagerReadSequence(CKGUID *guid) {
    int i = m_Data[m_ChunkParser->CurrentPos++];
    if (guid)
        *guid = *(CKGUID *) &m_Data[m_ChunkParser->CurrentPos];
    m_ChunkParser->CurrentPos += 2;
    return i;
}

int CKStateChunk::ReadManagerInt(CKGUID *guid) {
    if (guid)
        *guid = *(CKGUID *) &m_Data[m_ChunkParser->CurrentPos];
    m_ChunkParser->CurrentPos += 2;
    return m_Data[m_ChunkParser->CurrentPos++];
}

int CKStateChunk::ReadArray_LEndian(void **array) {
    ChunkParser *parser = m_ChunkParser;
    if (parser && parser->CurrentPos < m_ChunkSize) {
        // Read array metadata
        const int dataSizeBytes = m_Data[parser->CurrentPos++];
        const int elementCount = m_Data[parser->CurrentPos++];

        if (dataSizeBytes > 0 && elementCount > 0) {
            // Calculate needed dwords (4-byte chunks)
            const int dwordCount = (dataSizeBytes + 3) / 4;

            // Allocate and check allocation success
            void *arrayData = new CKBYTE[dataSizeBytes];
            if (!arrayData) {
                *array = NULL;
                return 0;
            }

            // Bounds check
            if (parser->CurrentPos + dwordCount <= m_ChunkSize) {
                memcpy(arrayData, &m_Data[parser->CurrentPos], dataSizeBytes);
                // Update parser position
                parser->CurrentPos += dwordCount;
                *array = arrayData;
                return elementCount;
            } else {
                // Free memory and return error if bounds check fails
                delete[] arrayData;
                *array = NULL;
                return 0;
            }
        }

        // Invalid size parameters
        *array = NULL;
    } else if (m_File) {
        // Report read error through context
        m_File->m_Context->OutputToConsole("Chunk Read Error", true);
    }

    return 0;
}

int CKStateChunk::ReadArray_LEndian16(void **array) {
    return ReadArray_LEndian(array);
}

int CKStateChunk::ReadManagerIntSequence() {
    return m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteObjectID(CK_ID obj) {
    CheckSize(sizeof(int));
    if (m_File) {
        m_Data[m_ChunkParser->CurrentPos] = m_File->SaveFindObjectIndex(obj);
    } else if (obj) {
        if (!m_Ids)
            m_Ids = new IntListStruct;
        m_Ids->AddEntry(m_ChunkParser->CurrentPos);
        m_Data[m_ChunkParser->CurrentPos] = (int) obj;
    } else {
        m_Data[m_ChunkParser->CurrentPos] = 0;
    }
    ++m_ChunkParser->CurrentPos;
}

void CKStateChunk::WriteObject(CKObject *obj) {
    if (m_ChunkClassID == -1 || obj && (m_Dynamic || !obj->IsDynamic()))
        WriteObjectID((obj) ? obj->GetID() : 0);
    else
        WriteObjectID(0);
}

void CKStateChunk::WriteObjectIDSequence(CK_ID id) {
    CheckSize(sizeof(int));
    if (m_File)
        m_Data[m_ChunkParser->CurrentPos] = m_File->SaveFindObjectIndex(id);
    else
        m_Data[m_ChunkParser->CurrentPos] = (int) id;
    ++m_ChunkParser->CurrentPos;
}

void CKStateChunk::WriteObjectSequence(CKObject *obj) {
    if (m_ChunkClassID == -1 || obj && (m_Dynamic || !obj->IsDynamic()))
        WriteObjectIDSequence((obj) ? obj->GetID() : 0);
    else
        WriteObjectIDSequence(0);
}

void CKStateChunk::StartObjectIDSequence(int count) {
    CheckSize(sizeof(int));
    if (count != 0 && !m_File) {
        if (!m_Ids)
            m_Ids = new IntListStruct;
        m_Ids->AddEntries(m_ChunkParser->CurrentPos);
    }
    m_Data[m_ChunkParser->CurrentPos++] = count;
}

CK_ID CKStateChunk::ReadObjectID() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return 0;
    }

    int id = m_Data[m_ChunkParser->CurrentPos];
    if (m_ChunkVersion >= CHUNK_VERSION1) {
        ++m_ChunkParser->CurrentPos;
        if (!m_File)
            return id;
        if (id >= 0)
            return m_File->LoadFindObject(id);
    } else {
        ++m_ChunkParser->CurrentPos;
        if (id != 0) {
            m_ChunkParser->CurrentPos += 2;
            return m_Data[m_ChunkParser->CurrentPos++];
        }
    }

    return 0;
}

CKObject *CKStateChunk::ReadObject(CKContext *context) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return NULL;
    }
    return context->GetObject(ReadObjectID());
}

void CKStateChunk::WriteObjectArray(CKObjectArray *array, CKContext *context) {
    int objectCount = (array) ? array->GetCount() : 0;
    CheckSize(objectCount * sizeof(int));
    StartObjectIDSequence(objectCount);
    if (array) {
        array->Reset();
        while (!array->EndOfList()) {
            CK_ID id = 0;
            if (context) {
                CKObject *object = array->GetData(context);
                if (object && (m_ChunkClassID == -1 || m_Dynamic || object->IsDynamic()))
                    id = object->GetID();
            } else {
                id = array->GetDataId();
            }
            if (m_File) {
                int index = -1;
                XFileObjectsTableIt it = m_File->m_ObjectsHashTable.Find(id);
                if (it != m_File->m_ObjectsHashTable.End())
                    index = *it;
                m_Data[m_ChunkParser->CurrentPos++] = index;
            } else {
                m_Data[m_ChunkParser->CurrentPos++] = id;
            }
            array->Next();
        }
    }
}

void CKStateChunk::ReadObjectArray(CKObjectArray *array) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return;
    }

    int count = m_Data[m_ChunkParser->CurrentPos++];
    if (count == 0) {
        if (array)
            array->Clear();
        return;
    }

    if (m_ChunkVersion < CHUNK_VERSION1) {
        m_ChunkParser->CurrentPos += 4;
        count = m_Data[m_ChunkParser->CurrentPos++];
    }

    if (!array)
        m_ChunkParser->CurrentPos += count;

    array->Clear();
    if (count + m_ChunkParser->CurrentPos <= m_ChunkSize) {
        if (m_File) {
            for (int i = 0; i < count; ++i) {
                int index = m_Data[m_ChunkParser->CurrentPos++];
                CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                array->InsertRear(id);
            }
        } else {
            for (int i = 0; i < count; ++i) {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                array->InsertRear(id);
            }
        }
    }
}

const XObjectArray &CKStateChunk::ReadXObjectArray() {
    m_TempXOA.Clear();
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return m_TempXOA;
    }

    int count = StartReadSequence();
    if (count != 0) {
        if (m_ChunkVersion < CHUNK_VERSION1) {
            m_ChunkParser->CurrentPos += 4;
            count = ReadInt();
            m_TempXOA.Resize(count);
            for (int i = 0; i < count; ++i)
                m_TempXOA[i] = m_Data[m_ChunkParser->CurrentPos++];
        } else {
            m_TempXOA.Resize(count);
            if (count + m_ChunkParser->CurrentPos <= m_ChunkSize) {
                if (m_File) {
                    for (int i = 0; i < count; ++i) {
                        int index = m_Data[m_ChunkParser->CurrentPos++];
                        CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                        m_TempXOA[i] = id;
                    }
                } else {
                    for (int i = 0; i < count; ++i)
                        m_TempXOA[i] = m_Data[m_ChunkParser->CurrentPos++];
                }
            }
        }
    }

    return m_TempXOA;
}

const XObjectPointerArray &CKStateChunk::ReadXObjectArray(CKContext *context) {
    m_TempXOPA.Clear();
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return m_TempXOPA;
    }

    int count = StartReadSequence();
    if (count != 0) {
        if (m_ChunkVersion < CHUNK_VERSION1) {
            m_ChunkParser->CurrentPos += 4;
            count = ReadInt();
            m_TempXOPA.Reserve(count);
            for (int i = 0; i < count; ++i) {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                CKObject *obj = context->GetObject(id);
                if (obj)
                    m_TempXOPA.PushBack(obj);
            }
        } else {
            m_TempXOPA.Reserve(count);
            if (count + m_ChunkParser->CurrentPos <= m_ChunkSize) {
                if (m_File) {
                    for (int i = 0; i < count; ++i) {
                        int index = m_Data[m_ChunkParser->CurrentPos++];
                        CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                        CKObject *obj = context->GetObject(id);
                        if (obj)
                            m_TempXOPA.PushBack(obj);
                    }
                } else {
                    for (int i = 0; i < count; ++i) {
                        CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                        CKObject *obj = context->GetObject(id);
                        if (obj)
                            m_TempXOPA.PushBack(obj);
                    }
                }
            }
        }
    }

    return m_TempXOPA;
}

CKObjectArray *CKStateChunk::ReadObjectArray() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return NULL;
    }

    int count = m_Data[m_ChunkParser->CurrentPos++];
    if (count == 0)
        return NULL;

    if (m_ChunkVersion < CHUNK_VERSION1) {
        m_ChunkParser->CurrentPos += 4;
        count = m_Data[m_ChunkParser->CurrentPos++];
        if (count == 0)
            return NULL;
    }

    CKObjectArray *array = NULL;
    if (count + m_ChunkParser->CurrentPos <= m_ChunkSize) {
        array = new CKObjectArray;
        if (!array) {
            m_ChunkParser->CurrentPos += count;
            return NULL;
        }

        if (m_File) {
            for (int i = 0; i < count; ++i) {
                int index = m_Data[m_ChunkParser->CurrentPos++];
                CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                array->InsertRear(id);
            }
        } else {
            for (int i = 0; i < count; ++i) {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                array->InsertRear(id);
            }
        }
    }

    return array;
}

void CKStateChunk::WriteSubChunk(CKStateChunk *sub) {
    int size = 4;
    if (sub) {
        size = 8 * sizeof(int);
        if (sub->m_ChunkSize != 0)
            size += sub->m_ChunkSize * sizeof(int);
        if (sub->m_Ids)
            size += sub->m_Ids->Size * sizeof(int);
        if (sub->m_Chunks)
            size += sub->m_Chunks->Size * sizeof(int);
        if (sub->m_Managers)
            size += sub->m_Managers->Size * sizeof(int);
    }
    CheckSize(size);
    m_Data[m_ChunkParser->CurrentPos++] = size / sizeof(int) - 1;
    if (sub) {
        if (!m_Chunks)
            m_Chunks = new IntListStruct;
        m_Chunks->AddEntry(m_ChunkParser->CurrentPos - 1);
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkClassID;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_DataVersion | sub->m_ChunkVersion << 16;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkSize;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_File != NULL;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Ids) ? sub->m_Ids->Size : 0;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Chunks) ? sub->m_Chunks->Size : 0;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Managers) ? sub->m_Managers->Size : 0;
        if (sub->m_ChunkSize != 0)
            WriteBufferNoSize_LEndian(sub->m_ChunkSize * sizeof(int), sub->m_Data);
        if (sub->m_Ids && sub->m_Ids->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Ids->Size * sizeof(int), sub->m_Ids->Data);
        if (sub->m_Chunks && sub->m_Chunks->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Chunks->Size * sizeof(int), sub->m_Chunks->Data);
        if (sub->m_Managers && sub->m_Managers->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Managers->Size * sizeof(int), sub->m_Managers->Data);
    }
}

void CKStateChunk::StartSubChunkSequence(int count) {
    if (!m_Chunks)
        m_Chunks = new IntListStruct;
    m_Chunks->AddEntries(m_ChunkParser->CurrentPos - 1);
    CheckSize(sizeof(int));
    m_Data[m_ChunkParser->CurrentPos++] = count;
}

int CKStateChunk::StartReadSequence() {
    return m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteSubChunkSequence(CKStateChunk *sub) {
    int size = 4;
    if (sub) {
        size = 8 * sizeof(int);
        if (sub->m_ChunkSize != 0)
            size += sub->m_ChunkSize * sizeof(int);
        if (sub->m_Ids)
            size += sub->m_Ids->Size * sizeof(int);
        if (sub->m_Chunks)
            size += sub->m_Chunks->Size * sizeof(int);
        if (sub->m_Managers)
            size += sub->m_Managers->Size * sizeof(int);
    }
    CheckSize(size);
    m_Data[m_ChunkParser->CurrentPos++] = size / sizeof(int) - 1;
    if (sub) {
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkClassID;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_DataVersion | sub->m_ChunkVersion << 16;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkSize;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_File != NULL;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Ids) ? sub->m_Ids->Size : 0;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Chunks) ? sub->m_Chunks->Size : 0;
        m_Data[m_ChunkParser->CurrentPos++] = (sub->m_Managers) ? sub->m_Managers->Size : 0;
        if (sub->m_ChunkSize != 0)
            WriteBufferNoSize_LEndian(sub->m_ChunkSize * sizeof(int), sub->m_Data);
        if (sub->m_Ids && sub->m_Ids->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Ids->Size * sizeof(int), sub->m_Ids->Data);
        if (sub->m_Chunks && sub->m_Chunks->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Chunks->Size * sizeof(int), sub->m_Chunks->Data);
        if (sub->m_Managers && sub->m_Managers->Size > 0)
            WriteBufferNoSize_LEndian(sub->m_Managers->Size * sizeof(int), sub->m_Managers->Data);
    }
}

CKStateChunk *CKStateChunk::ReadSubChunk() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return NULL;
    }

    int size = m_Data[m_ChunkParser->CurrentPos++];
    if (size <= 0 || size + m_ChunkParser->CurrentPos > m_ChunkSize)
        return NULL;

    int classID = m_Data[m_ChunkParser->CurrentPos++];
    CKStateChunk *sub = new CKStateChunk(classID, NULL);

    if (m_ChunkVersion >= CHUNK_VERSION1) {
        int version = m_Data[m_ChunkParser->CurrentPos++];
        sub->m_DataVersion = (short int) (version & 0x0000FFFF);
        sub->m_ChunkVersion = (short int) ((version & 0xFFFF0000) >> 16);
        sub->m_ChunkSize = m_Data[m_ChunkParser->CurrentPos++];
        CKBOOL hasFile = m_Data[m_ChunkParser->CurrentPos++];
        int idCount = m_Data[m_ChunkParser->CurrentPos++];
        int chunkCount = m_Data[m_ChunkParser->CurrentPos++];
        int managerCount = 0;
        if (m_ChunkVersion > 4)
            managerCount = m_Data[m_ChunkParser->CurrentPos++];
        if (sub->m_ChunkSize != 0) {
            sub->m_Data = new int[sub->m_ChunkSize];
            if (sub->m_Data)
                ReadAndFillBuffer_LEndian(sub->m_ChunkSize * sizeof(int), sub->m_Data);
        }

        if (idCount > 0) {
            sub->m_Ids = new IntListStruct;
            sub->m_Ids->AllocatedSize = idCount;
            sub->m_Ids->Size = idCount;
            sub->m_Ids->Data = new int[idCount];
            if (sub->m_Ids->Data)
                ReadAndFillBuffer_LEndian(idCount * sizeof(int), sub->m_Ids->Data);
        }

        if (chunkCount > 0) {
            sub->m_Chunks = new IntListStruct;
            sub->m_Chunks->AllocatedSize = chunkCount;
            sub->m_Chunks->Size = chunkCount;
            sub->m_Chunks->Data = new int[chunkCount];
            if (sub->m_Chunks->Data)
                ReadAndFillBuffer_LEndian(chunkCount * sizeof(int), sub->m_Chunks->Data);
        }

        if (managerCount > 0) {
            sub->m_Managers = new IntListStruct;
            sub->m_Managers->AllocatedSize = managerCount;
            sub->m_Managers->Size = managerCount;
            sub->m_Managers->Data = new int[managerCount];
            if (sub->m_Managers->Data)
                ReadAndFillBuffer_LEndian(managerCount * sizeof(int), sub->m_Managers->Data);
        }

        if (hasFile && m_File)
            sub->m_File = m_File;

        return sub;
    } else {
        sub->m_ChunkSize = m_Data[m_ChunkParser->CurrentPos++];
        ++m_ChunkParser->CurrentPos;
        if (sub->m_ChunkSize == 0)
            return sub;
        sub->m_Data = new int[sub->m_ChunkSize];
        if (sub->m_Data) {
            memcpy(sub->m_Data, &m_Data[m_ChunkParser->CurrentPos], sub->m_ChunkSize * sizeof(int));
            m_ChunkParser->CurrentPos += sub->m_ChunkSize;
            return sub;
        }
    }

    delete sub;
    return NULL;
}

void CKStateChunk::WriteByte(CKCHAR byte) {
    CheckSize(sizeof(int));
    m_Data[m_ChunkParser->CurrentPos++] = byte;
}

CKBYTE CKStateChunk::ReadByte() {
    if (m_ChunkParser->CurrentPos >= m_ChunkSize)
        return 0;
    return m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteWord(CKWORD data) {
    CheckSize(sizeof(int));
    m_Data[m_ChunkParser->CurrentPos++] = data;
}

CKWORD CKStateChunk::ReadWord() {
    if (m_ChunkParser->CurrentPos >= m_ChunkSize)
        return 0;
    return m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteInt(int data) {
    CheckSize(sizeof(int));
    m_Data[m_ChunkParser->CurrentPos++] = data;
}

int CKStateChunk::ReadInt() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return 0;
    }
    return m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteDword(CKDWORD data) {
    WriteInt(data);
}

CKDWORD CKStateChunk::ReadDword() {
    return ReadInt();
}

void CKStateChunk::WriteDwordAsWords(CKDWORD data) {
    WriteInt(data);
}

CKDWORD CKStateChunk::ReadDwordAsWords() {
    return ReadInt();
}

void CKStateChunk::WriteGuid(CKGUID data) {
    CheckSize(sizeof(CKGUID));
    m_Data[m_ChunkParser->CurrentPos++] = data.d1;
    m_Data[m_ChunkParser->CurrentPos++] = data.d2;
}

CKGUID CKStateChunk::ReadGuid() {
    CKGUID guid;
    if (m_ChunkParser->CurrentPos >= m_ChunkSize)
        return guid;
    guid.d1 = m_Data[m_ChunkParser->CurrentPos++];
    guid.d2 = m_Data[m_ChunkParser->CurrentPos++];
    return guid;
}

void CKStateChunk::WriteFloat(float data) {
    WriteInt(*(int *) &data);
}

float CKStateChunk::ReadFloat() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return 0;
    }
    return *(float *) &m_Data[m_ChunkParser->CurrentPos++];
}

void CKStateChunk::WriteBuffer(int size, void *buf) {
    WriteBuffer_LEndian(size, buf);
}

void CKStateChunk::WriteBuffer_LEndian(int size, void *buf) {
    if (!buf) {
        CheckSize(sizeof(CKGUID));
        m_Data[m_ChunkParser->CurrentPos++] = 0;
    } else {
        int sz = (size + 3) / sizeof(int);
        CheckSize((sz + 1) * sizeof(int));
        m_Data[m_ChunkParser->CurrentPos++] = size;
        memcpy(&m_Data[m_ChunkParser->CurrentPos], buf, size);
        m_ChunkParser->CurrentPos += sz;
    }
}

void CKStateChunk::WriteBuffer_LEndian16(int size, void *buf) {
    WriteBuffer_LEndian(size, buf);
}

void CKStateChunk::WriteBufferNoSize(int size, void *buf) {
    WriteBufferNoSize_LEndian(size, buf);
}

void CKStateChunk::WriteBufferNoSize_LEndian(int size, void *buf) {
    if (buf) {
        int sz = (size + 3) / sizeof(int);
        CheckSize(sz * sizeof(int));
        memcpy(&m_Data[m_ChunkParser->CurrentPos], buf, size);
        m_ChunkParser->CurrentPos += sz;
    }
}

void CKStateChunk::WriteBufferNoSize_LEndian16(int size, void *buf) {
    WriteBufferNoSize_LEndian(size, buf);
}

int CKStateChunk::ReadBuffer(void **buffer) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return 0;
    }

    CKBYTE *buf = NULL;
    int size = m_Data[m_ChunkParser->CurrentPos];
    int sz = (size + 3) / sizeof(int);
    ++m_ChunkParser->CurrentPos;
    if (sz + m_ChunkParser->CurrentPos <= m_ChunkSize) {
        if (size > 0) {
            buf = new CKBYTE[size];
            if (!buf) {
                *buffer = NULL;
                return 0;
            }
            memcpy(buf, &m_Data[m_ChunkParser->CurrentPos], size);
            m_ChunkParser->CurrentPos += sz;
        }
        *buffer = buf;
        return size;
    }

    *buffer = NULL;
    return 0;
}

void CKStateChunk::ReadAndFillBuffer(void *buffer) {
    ReadAndFillBuffer_LEndian(buffer);
}

void CKStateChunk::ReadAndFillBuffer_LEndian(void *buffer) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return;
    }

    int size = m_Data[m_ChunkParser->CurrentPos];
    ++m_ChunkParser->CurrentPos;
    if (size > 0) {
        memcpy(buffer, &m_Data[m_ChunkParser->CurrentPos], size);
        m_ChunkParser->CurrentPos += (size + 3) / sizeof(int);
    }
}

void CKStateChunk::ReadAndFillBuffer_LEndian16(void *buffer) {
    ReadAndFillBuffer_LEndian(buffer);
}

void CKStateChunk::ReadAndFillBuffer(int size, void *buffer) {
    ReadAndFillBuffer_LEndian(size, buffer);
}

void CKStateChunk::ReadAndFillBuffer_LEndian(int size, void *buffer) {
    if (buffer)
        memcpy(buffer, &m_Data[m_ChunkParser->CurrentPos], size);
    m_ChunkParser->CurrentPos += (size + 3) / sizeof(int);
}

void CKStateChunk::ReadAndFillBuffer_LEndian16(int size, void *buffer) {
    ReadAndFillBuffer_LEndian(size, buffer);
}

void CKStateChunk::WriteString(char *str) {
    if (!str) {
        CheckSize(sizeof(int));
        m_Data[m_ChunkParser->CurrentPos++] = 0;
    } else {
        int len = strlen(str);
        int size = len + 1;
        int sz = (size + sizeof(int)) / sizeof(int);
        CheckSize((sz + 1) * sizeof(int));
        m_Data[this->m_ChunkParser->CurrentPos++] = size;
        if (size > 0) {
            memcpy(&m_Data[m_ChunkParser->CurrentPos], str, size);
            m_ChunkParser->CurrentPos += size;
        }
    }
}

int CKStateChunk::ReadString(CKSTRING *str) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return 0;
    }

    CKSTRING s = NULL;
    int size = m_Data[m_ChunkParser->CurrentPos];
    int sz = (size + 3) / sizeof(int);
    ++m_ChunkParser->CurrentPos;
    if (sz + m_ChunkParser->CurrentPos <= m_ChunkSize) {
        if (size > 0) {
            s = new char[size];
            if (!s) {
                *str = NULL;
                return 0;
            }
            memcpy(s, &m_Data[m_ChunkParser->CurrentPos], size);
            m_ChunkParser->CurrentPos += sz;
        }
        *str = s;
        return size;
    }

    *str = NULL;
    return 0;
}

void CKStateChunk::ReadString(XString &str) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        str = "";
        return;
    }

    CKDWORD length = m_Data[m_ChunkParser->CurrentPos++];

    CKDWORD dwordCount = (length + 3) >> 2; // (length+3)/4

    if ((m_ChunkParser->CurrentPos + dwordCount) > m_ChunkSize) {
        str = "";
        return;
    }

    if (length == 0) {
        str = "";
        return;
    }

    str = (char *) &m_Data[m_ChunkParser->CurrentPos];
    m_ChunkParser->CurrentPos += dwordCount;
}

void CKStateChunk::WriteVector(const VxVector &v) {
    CheckSize(sizeof(VxVector));
    memcpy(&m_Data[m_ChunkParser->CurrentPos], &v, sizeof(VxVector));
    m_ChunkParser->CurrentPos += sizeof(VxVector) / sizeof(int);
}

void CKStateChunk::WriteVector(const VxVector *v) {
    if (v)
        WriteVector(*v);
}

void CKStateChunk::ReadVector(VxVector &v) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return;
    }

    memcpy(&v, &m_Data[m_ChunkParser->CurrentPos], sizeof(VxVector));
    m_ChunkParser->CurrentPos += sizeof(VxVector) / sizeof(int);
}

void CKStateChunk::ReadVector(VxVector *v) {
    if (v)
        ReadVector(*v);
}

void CKStateChunk::WriteMatrix(const VxMatrix &mat) {
    CheckSize(sizeof(VxMatrix));
    memcpy(&m_Data[m_ChunkParser->CurrentPos], &mat, sizeof(VxMatrix));
    m_ChunkParser->CurrentPos += sizeof(VxMatrix) / sizeof(int);
}

void CKStateChunk::ReadMatrix(VxMatrix &mat) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize) {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return;
    }

    memcpy(&mat, &m_Data[m_ChunkParser->CurrentPos], sizeof(VxMatrix));
    m_ChunkParser->CurrentPos += sizeof(VxMatrix) / sizeof(int);
}

int CKStateChunk::ConvertToBuffer(void *buffer) {
    int size = 2 * sizeof(int) + m_ChunkSize * sizeof(int);
    CKBYTE chunkOptions = 0;
    if (m_Ids) {
        size += m_Ids->Size * sizeof(int);
        chunkOptions |= CHNK_OPTION_IDS;
    }
    if (m_Chunks) {
        size += m_Chunks->Size * sizeof(int);
        chunkOptions |= CHNK_OPTION_CHN;
    }
    if (m_Managers) {
        size += m_Managers->Size * sizeof(int);
        chunkOptions |= CHNK_OPTION_MAN;
    }
    if (m_File) {
        chunkOptions |= CHNK_OPTION_FILE;
    }

    if (buffer) {
        int pos = 0;
        int *buf = (int *) buffer;
        CKWORD chunkVersion = m_ChunkVersion | chunkOptions;
        CKWORD dataVersion = m_DataVersion | m_ChunkClassID << 8;
        buf[pos++] = dataVersion | chunkVersion << 16;
        buf[pos++] = m_ChunkSize;
        memcpy(&buf[pos], m_Data, m_ChunkSize * sizeof(int));
        pos += m_ChunkSize;
        if (m_Ids) {
            memcpy(&buf[pos], m_Ids->Data, m_Ids->Size * sizeof(int));
            pos += m_Ids->Size;
        }
        if (m_Chunks) {
            memcpy(&buf[pos], m_Chunks->Data, m_Chunks->Size * sizeof(int));
            pos += m_Chunks->Size;
        }
        if (m_Managers) {
            memcpy(&buf[pos], m_Managers->Data, m_Managers->Size * sizeof(int));
            pos += m_Managers->Size;
        }
    }

    return size;
}

CKBOOL CKStateChunk::ConvertFromBuffer(void *buffer) {
    if (!buffer)
        return FALSE;

    int pos = 0;
    int *buf = (int *) buffer;

    Clear();
    int val = buf[pos++];
    m_DataVersion = (short) (val & 0x0000FFFF);
    m_DataVersion &= 0x00FF;
    m_ChunkVersion = (short) ((val & 0xFFFF0000) >> 16);
    m_ChunkVersion &= 0x00FF;

    if (m_ChunkVersion < CHUNK_VERSION2) {
        m_ChunkClassID = buf[pos++];
        m_ChunkSize = buf[pos++];
        ++pos;
        int idCount = buf[pos++];
        int chunkCount = buf[pos++];

        if (m_ChunkSize != 0) {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if (idCount > 0) {
            m_Ids = new IntListStruct;
            m_Ids->AllocatedSize = idCount;
            m_Ids->Size = idCount;
            m_Ids->Data = new int[idCount];
            if (m_Ids->Data) {
                memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                pos += idCount;
            }
        }

        if (chunkCount > 0) {
            m_Chunks = new IntListStruct;
            m_Chunks->AllocatedSize = chunkCount;
            m_Chunks->Size = chunkCount;
            m_Chunks->Data = new int[chunkCount];
            if (m_Chunks->Data) {
                memcpy(m_Chunks->Data, &buf[pos], chunkCount * sizeof(int));
                pos += chunkCount;
            }
        }

        m_File = NULL;
        return TRUE;
    } else if (m_ChunkVersion == CHUNK_VERSION2) {
        m_ChunkClassID = buf[pos++];
        m_ChunkSize = buf[pos++];
        ++pos;
        int idCount = buf[pos++];
        int chunkCount = buf[pos++];
        int managerCount = buf[pos++];

        if (m_ChunkSize != 0) {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if (idCount > 0) {
            m_Ids = new IntListStruct;
            m_Ids->AllocatedSize = idCount;
            m_Ids->Size = idCount;
            m_Ids->Data = new int[idCount];
            if (m_Ids->Data) {
                memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                pos += idCount;
            }
        }

        if (chunkCount > 0) {
            m_Chunks = new IntListStruct;
            m_Chunks->AllocatedSize = chunkCount;
            m_Chunks->Size = chunkCount;
            m_Chunks->Data = new int[chunkCount];
            if (m_Chunks->Data) {
                memcpy(m_Chunks->Data, &buf[pos], chunkCount * sizeof(int));
                pos += chunkCount;
            }
        }

        if (managerCount > 0) {
            m_Managers = new IntListStruct;
            m_Managers->AllocatedSize = managerCount;
            m_Managers->Size = managerCount;
            m_Managers->Data = new int[managerCount];
            if (m_Managers->Data) {
                memcpy(m_Managers->Data, &buf[pos], managerCount * sizeof(int));
                pos += managerCount;
            }
        }

        m_File = NULL;
        return TRUE;
    } else if (m_ChunkVersion <= CHUNK_VERSION4) {
        m_DataVersion = (short) (val & 0x0000FFFF);
        m_ChunkVersion = (short) ((val & 0xFFFF0000) >> 16);

        CKBYTE chunkOptions = (m_ChunkVersion & 0xFF00) >> 8;

        m_ChunkClassID = (m_DataVersion & 0xFF00) >> 8;
        m_ChunkSize = buf[pos++];

        if (m_ChunkSize != 0) {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if ((chunkOptions & CHNK_OPTION_FILE) == 0)
            m_File = NULL;

        if ((chunkOptions & CHNK_OPTION_IDS) != 0) {
            int idCount = buf[pos++];
            if (idCount > 0) {
                m_Ids = new IntListStruct;
                m_Ids->AllocatedSize = idCount;
                m_Ids->Size = idCount;
                m_Ids->Data = new int[idCount];
                if (m_Ids->Data) {
                    memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                    pos += idCount;
                }
            }
        }

        if ((chunkOptions & CHNK_OPTION_CHN) != 0) {
            int chunkCount = buf[pos++];
            if (chunkCount > 0) {
                m_Chunks = new IntListStruct;
                m_Chunks->AllocatedSize = chunkCount;
                m_Chunks->Size = chunkCount;
                m_Chunks->Data = new int[chunkCount];
                if (m_Chunks->Data) {
                    memcpy(m_Chunks->Data, &buf[pos], chunkCount * sizeof(int));
                    pos += chunkCount;
                }
            }
        }

        if ((chunkOptions & CHNK_OPTION_MAN) != 0) {
            int managerCount = buf[pos++];
            if (managerCount > 0) {
                m_Managers = new IntListStruct;
                m_Managers->AllocatedSize = managerCount;
                m_Managers->Size = managerCount;
                m_Managers->Data = new int[managerCount];
                if (m_Managers->Data) {
                    memcpy(m_Managers->Data, &buf[pos], managerCount * sizeof(int));
                    pos += managerCount;
                }
            }
        }

        m_DataVersion &= 0x00FF;
        m_ChunkVersion &= 0x00FF;
        return TRUE;
    }

    return FALSE;
}

int CKStateChunk::RemapObject(CK_ID old_id, CK_ID new_id) {
    if (old_id == 0 || new_id == 0)
        return 0;

    CKDependenciesContext depContext(NULL);
    depContext.m_MapID.Insert(old_id, new_id);
    return RemapObjects(NULL, &depContext);
}

int CKStateChunk::RemapParameterInt(CKGUID ParameterType, int *ConversionTable, int NbEntries) {
    if (!ConversionTable)
        return 0;
    ChunkIteratorData data;
    data.ConversionTable = ConversionTable;
    data.NbEntries = NbEntries;
    data.Guid = ParameterType;
    data.Flag = TRUE;
    data.ChunkVersion = m_ChunkVersion;
    data.Data = m_Data;
    data.ChunkSize = m_ChunkSize;
    if (m_Chunks) {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Ids) {
        data.Ids = m_Ids->Data;
        data.IdCount = m_Ids->Size;
    }
    return IterateAndDo(ParameterRemapper, &data);
}

int CKStateChunk::RemapManagerInt(CKGUID Manager, int *ConversionTable, int NbEntries) {
    if (!ConversionTable)
        return 0;
    ChunkIteratorData data;
    data.ConversionTable = ConversionTable;
    data.NbEntries = NbEntries;
    data.Guid = Manager;
    data.Flag = TRUE;
    data.ChunkVersion = m_ChunkVersion;
    data.Data = m_Data;
    data.ChunkSize = m_ChunkSize;
    if (m_Chunks) {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Managers) {
        data.Managers = m_Managers->Data;
        data.ManagerCount = m_Managers->Size;
    }
    return IterateAndDo(ManagerRemapper, &data);
}

int CKStateChunk::RemapObjects(CKContext *context, CKDependenciesContext *Depcontext) {
    ChunkIteratorData data;
    data.Flag = TRUE;
    data.Context = context;
    data.DepContext = Depcontext;
    data.ChunkVersion = m_ChunkVersion;
    data.Data = m_Data;
    data.ChunkSize = m_ChunkSize;
    if (m_Chunks) {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Ids) {
        data.Ids = m_Ids->Data;
        data.IdCount = m_Ids->Size;
    }
    return IterateAndDo(ObjectRemapper, &data);
}

void CKStateChunk::WriteReaderBitmap(const VxImageDescEx &desc, CKBitmapReader *reader, CKBitmapProperties *bp) {
}

CKBOOL CKStateChunk::ReadReaderBitmap(const VxImageDescEx &desc) {
    return FALSE;
}

void CKStateChunk::WriteRawBitmap(const VxImageDescEx &desc) {
}

CKBYTE *CKStateChunk::ReadRawBitmap(VxImageDescEx &desc) {
    return nullptr;
}

void CKStateChunk::WriteBitmap(BITMAP_HANDLE bitmap, CKSTRING ext) {
}

BITMAP_HANDLE CKStateChunk::ReadBitmap() {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
        return NULL;

    // Bitmap format identifiers
    const char *CK_TGA_HEADER = "CKTGA";
    const char *CK_JPG_HEADER = "CKJPG";
    const char *CK_BMP_HEADER = "CKBMP";
    const char *CK_TIF_HEADER = "CKTIF";
    const char *CK_GIF_HEADER = "CKGIF";
    const char *CK_PCX_HEADER = "CKPCX";

    BITMAP_HANDLE bitmapHandle = NULL;
    void *buffer = NULL;
    CKBitmapProperties *bitmapProps = NULL;

    // Read bitmap header information
    ReadInt(); // Skip version/flags field
    int bufferSize = ReadBuffer(&buffer);

    if (!buffer || bufferSize < 5) {
        delete[] (CKBYTE *) buffer;
        return NULL;
    }

    // Detect file format from header
    char formatHeader[6] = {0};
    memcpy(formatHeader, buffer, 5);

    const char *fileExtension = NULL;
    bool hasCustomHeader = true;

    if (!stricmp(formatHeader, CK_TGA_HEADER)) {
        fileExtension = "TGA";
    } else if (!stricmp(formatHeader, CK_JPG_HEADER)) {
        fileExtension = "JPG";
    } else if (!stricmp(formatHeader, CK_BMP_HEADER)) {
        fileExtension = "BMP";
    } else if (!stricmp(formatHeader, CK_TIF_HEADER)) {
        fileExtension = "TIF";
    } else if (!stricmp(formatHeader, CK_GIF_HEADER)) {
        fileExtension = "GIF";
    } else if (!stricmp(formatHeader, CK_PCX_HEADER)) {
        fileExtension = "PCX";
    } else {
        fileExtension = "TGA";
        hasCustomHeader = false;
    }

    // Get appropriate bitmap reader
    CKFileExtension ext(fileExtension);
    CKBitmapReader *reader = CKGetPluginManager()->GetBitmapReader(ext);

    if (reader) {
        char *dataStart = static_cast<char *>(buffer);
        int dataSize = bufferSize;

        // Skip custom header if present
        if (hasCustomHeader) {
            dataStart += 5;
            dataSize -= 5;
        }

        // Read bitmap from memory
        if (reader->ReadMemory(dataStart, dataSize, &bitmapProps)) {
            // Create bitmap from decoded data
            bitmapProps->m_Format.Image = (XBYTE *) bitmapProps->m_Data;
            bitmapHandle = VxCreateBitmap(bitmapProps->m_Format);

            // Cleanup reader allocations
            reader->ReleaseMemory(bitmapProps->m_Data);
            reader->ReleaseMemory(bitmapProps->m_Format.ColorMap);
            bitmapProps->m_Data = NULL;
            bitmapProps->m_Format.ColorMap = NULL;
        }
        reader->Release();
    }

    delete[] (CKBYTE *) buffer;
    return bitmapHandle;
}

CKBYTE *CKStateChunk::ReadBitmap2(VxImageDescEx &desc) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
        return NULL;

    // Bitmap format identifiers
    const char *CK_TGA_HEADER = "CKTGA";
    const char *CK_JPG_HEADER = "CKJPG";
    const char *CK_BMP_HEADER = "CKBMP";
    const char *CK_TIF_HEADER = "CKTIF";
    const char *CK_GIF_HEADER = "CKGIF";
    const char *CK_PCX_HEADER = "CKPCX";

    CKBYTE *bitmapData = NULL;
    void *buffer = NULL;
    CKBitmapProperties *bitmapProps = NULL;

    // Read bitmap header information
    ReadInt(); // Skip version/flags field
    int bufferSize = ReadBuffer(&buffer);

    if (!buffer || bufferSize < 5) {
        delete[] (CKBYTE *) buffer;
        return NULL;
    }

    // Detect file format from header
    char formatHeader[6] = {0};
    memcpy(formatHeader, buffer, 5);

    const char *fileExtension = NULL;
    bool hasCustomHeader = true;

    if (!_stricmp(formatHeader, CK_TGA_HEADER)) {
        fileExtension = "TGA";
    } else if (!_stricmp(formatHeader, CK_JPG_HEADER)) {
        fileExtension = "JPG";
    } else if (!_stricmp(formatHeader, CK_BMP_HEADER)) {
        fileExtension = "BMP";
    } else if (!_stricmp(formatHeader, CK_TIF_HEADER)) {
        fileExtension = "TIF";
    } else if (!_stricmp(formatHeader, CK_GIF_HEADER)) {
        fileExtension = "GIF";
    } else if (!_stricmp(formatHeader, CK_PCX_HEADER)) {
        fileExtension = "PCX";
    } else {
        fileExtension = "TGA";
        hasCustomHeader = false;
    }

    // Get appropriate bitmap reader
    CKFileExtension ext(fileExtension);
    CKBitmapReader *reader = CKGetPluginManager()->GetBitmapReader(ext);
    if (reader) {
        char *dataStart = static_cast<char *>(buffer);
        int dataSize = bufferSize;

        // Skip custom header if present
        if (hasCustomHeader) {
            dataStart += 5;
            dataSize -= 5;
        }

        // Read bitmap from memory
        bool readSuccess = reader->ReadMemory(dataStart, dataSize, &bitmapProps);
        if (readSuccess && bitmapProps) {
            // Copy image description to output
            memcpy(&desc, &bitmapProps->m_Format, sizeof(VxImageDescEx));

            // Transfer ownership of bitmap data
            bitmapData = (CKBYTE *) bitmapProps->m_Data;
            bitmapProps->m_Data = NULL; // Prevent double-free

            // Cleanup format resources
            reader->ReleaseMemory(bitmapProps->m_Format.ColorMap);
            bitmapProps->m_Format.ColorMap = NULL;

            // Free bitmap properties if allocated by reader
            if (bitmapProps) {
                delete bitmapProps;
            }
        } else {
            // Clean up if read failed
            if (bitmapProps) {
                if (bitmapProps->m_Data) {
                    reader->ReleaseMemory(bitmapProps->m_Data);
                }
                if (bitmapProps->m_Format.ColorMap) {
                    reader->ReleaseMemory(bitmapProps->m_Format.ColorMap);
                }
                delete bitmapProps;
            }
        }
        reader->Release();
    }

    delete[] (CKBYTE *) buffer;
    return bitmapData;
}

CKDWORD CKStateChunk::ComputeCRC(CKDWORD adler) {
    return adler32(adler, (unsigned char *) m_Data, m_ChunkSize * sizeof(int));
}

void CKStateChunk::Pack(int CompressionLevel) {
    uLongf srcSize = m_ChunkSize * sizeof(int);
    uLongf destSize = srcSize;
    Bytef *buf = new Bytef[destSize];

    if (compress2(buf, &destSize, (const Bytef *) m_Data, srcSize, CompressionLevel) == Z_OK) {
        // Allocate just enough memory for the compressed data
        Bytef *data = new Bytef[destSize];
        // Copy from buf (compressed data), not m_Data
        memcpy(data, buf, destSize);
        delete[] m_Data;
        m_Data = (int *) data;
        // Size should be in ints, not bytes
        m_ChunkSize = (destSize + sizeof(int) - 1) / sizeof(int);
    }
    delete[] buf;
}

CKBOOL CKStateChunk::UnPack(int DestSize) {
    uLongf size = DestSize;
    Bytef *buf = new Bytef[DestSize];
    if (!buf) return FALSE;  // Check allocation

    int err = uncompress(buf, &size, (const Bytef *) m_Data, m_ChunkSize * sizeof(int));
    if (err == Z_OK) {
        int sz = (DestSize + sizeof(int) - 1) / sizeof(int);  // Ceiling division
        int *data = new int[sz];
        if (!data) {
            delete[] buf;
            return FALSE;
        }

        memcpy(data, buf, DestSize);
        delete[] m_Data;
        m_Data = data;
        m_ChunkSize = sz;
    }
    delete[] buf;
    return err == Z_OK;
}

void CKStateChunk::AttributePatch(CKBOOL preserveData, int *ConversionTable, int NbEntries) {
    // Skip processing if managers array already exists
    if (m_Managers)
        return;

    // Read sequence length and store current position
    int sequenceLength = StartReadSequence();
    int initialPosition = GetCurrentPos();

    // Skip over the sequence data
    Skip(sequenceLength);

    if (!preserveData) {
        // Clean up subchunks if not preserving data
        StartReadSequence();
        while (sequenceLength-- > 0) {
            CKStateChunk *subChunk = ReadSubChunk();
            if (subChunk) {
                delete subChunk;
            }
        }
    }

    // Initialize managers list
    m_Managers = new IntListStruct();
    m_Managers->AddEntries(initialPosition);

    // Prepare for conversion
    Skip(sizeof(CKDWORD) * 3); // Skip header data

    if (ConversionTable && NbEntries > 0 && sequenceLength > 0) {
        do {
            int *currentPosPtr = &m_Data[m_ChunkParser->CurrentPos];
            int originalValue = *currentPosPtr;

            // Check if value needs conversion
            if (!(originalValue & 0x80000000) && originalValue < NbEntries) {
                *currentPosPtr = ConversionTable[originalValue];
            }

            m_ChunkParser->CurrentPos++;
        } while (--sequenceLength);
    }

    // Reset chunk position to attribute section
    SeekIdentifier(CK_STATESAVE_NEWATTRIBUTES);
}

int CKStateChunk::IterateAndDo(ChunkIterateFct fct, ChunkIteratorData *it) {
    int result = fct(it);

    if (it->Chunks) {
        int chunkIndex = 0;
        const int chunkCount = it->ChunkCount;

        while (chunkIndex < chunkCount) {
            ChunkIteratorData localIt;
            memset(&localIt, 0, sizeof(ChunkIteratorData));
            localIt.CopyFctData(it);

            const int currentChunk = it->Chunks[chunkIndex];

            if (currentChunk >= 0) {
                // Process normal chunk
                int *chunkData = it->Data + currentChunk;
                const int dataSize = *chunkData;

                if (dataSize > 0 && chunkData[2] + 4 != dataSize) {
                    localIt.ChunkVersion = static_cast<short>(chunkData[2] >> 16);
                    localIt.ChunkSize = chunkData[3];
                    localIt.IdCount = chunkData[5];
                    localIt.ChunkCount = chunkData[6];

                    // Handle version-specific manager count
                    if (it->ChunkVersion <= 4) {
                        localIt.ManagerCount = 0;
                        localIt.Data = chunkData + 7;
                    } else {
                        localIt.ManagerCount = chunkData[7];
                        localIt.Data = chunkData + 8;
                    }

                    // Set up ID and chunk pointers
                    if (localIt.IdCount > 0) {
                        localIt.Ids = localIt.Data + localIt.ChunkSize;
                    }
                    if (localIt.ChunkCount > 0) {
                        localIt.Chunks = localIt.Ids + localIt.IdCount;
                    }
                    if (localIt.ManagerCount > 0) {
                        localIt.Managers = localIt.Chunks + localIt.ChunkCount;
                    }

                    result += IterateAndDo(fct, &localIt);
                }
                chunkIndex++;
            } else {
                // Process packed chunks
                int *packedData = it->Data + (-currentChunk);
                const int packedCount = *packedData++;

                for (int i = 0; i < packedCount; i++) {
                    const int subChunkSize = *packedData++;
                    if (subChunkSize <= 0) continue;

                    int *subChunkData = packedData;
                    packedData += subChunkSize; // Move to next subchunk

                    localIt.ChunkVersion = static_cast<short>(subChunkData[1] >> 16);
                    localIt.ChunkSize = subChunkData[2];
                    localIt.IdCount = subChunkData[4];
                    localIt.ChunkCount = subChunkData[5];

                    // Version-specific handling
                    if (it->ChunkVersion <= 4) {
                        localIt.ManagerCount = 0;
                        localIt.Data = subChunkData + 6;
                    } else {
                        localIt.ManagerCount = subChunkData[6];
                        localIt.Data = subChunkData + 7;
                    }

                    // Set up pointers
                    if (localIt.IdCount > 0) {
                        localIt.Ids = localIt.Data + localIt.ChunkSize;
                    }
                    if (localIt.ChunkCount > 0) {
                        localIt.Chunks = localIt.Ids + localIt.IdCount;
                    }
                    if (localIt.ManagerCount > 0) {
                        localIt.Managers = localIt.Chunks + localIt.ChunkCount;
                    }

                    result += IterateAndDo(fct, &localIt);
                }
                chunkIndex++;
            }
        }
    }

    return result;
}

int CKStateChunk::ObjectRemapper(ChunkIteratorData *it) {
    ChunkIteratorData *iterData = it;
    int remappedCount = 0;

    if (iterData->ChunkVersion >= 4) {
        // Handle new chunk format with ID lists
        if (!iterData->Ids || iterData->IdCount <= 0)
            return remappedCount;

        for (int i = 0; i < iterData->IdCount;) {
            int currentIdPos = iterData->Ids[i];

            if (currentIdPos >= 0) {
                // Single object ID remapping
                CK_ID &oldId = (CK_ID &) iterData->Data[currentIdPos];
                CK_ID newId = 0;

                if (iterData->DepContext) {
                    // Use dependency context mapping
                    auto it = iterData->DepContext->m_MapID.Find(oldId);
                    newId = (it) ? *it : 0;
                } else {
                    // Use direct object manager resolution
                    newId = iterData->Context->m_ObjectManager->RealId(oldId);
                }

                if (newId) {
                    oldId = newId;
                    remappedCount++;
                }
                i++;
            } else {
                // Sequence of object IDs (negative marker followed by count)
                int sequenceCount = iterData->Ids[i + 1];
                int startPos = i + 2;
                int endPos = startPos + sequenceCount;

                for (int j = startPos; j < endPos; j++) {
                    CK_ID &oldId = (CK_ID &) iterData->Data[j];
                    CK_ID newId = 0;

                    if (iterData->DepContext) {
                        auto it = iterData->DepContext->m_MapID.Find(oldId);
                        newId = (it) ? *it : 0;
                    } else {
                        newId = iterData->Context->m_ObjectManager->RealId(oldId);
                    }

                    if (newId) {
                        oldId = newId;
                        remappedCount++;
                    }
                }
                i = endPos;
            }
        }
    } else {
        // Handle legacy chunk formats
        const CKDWORD OBJID_MARKER[] = {0xE32BC4C9, 0x134212E3, 0xFCBAE9DC};
        const CKDWORD SEQ_MARKER[] = {0xE192BD47, 0x13246628, 0x13EAB3CE, 0x7891AEFC, 0x13984562};

        // Process single object ID markers
        for (int pos = 3; pos < iterData->ChunkSize; pos++) {
            if (pos >= 3 &&
                iterData->Data[pos - 3] == OBJID_MARKER[0] &&
                iterData->Data[pos - 2] == OBJID_MARKER[1] &&
                iterData->Data[pos - 1] == OBJID_MARKER[2]) {
                CK_ID &oldId = (CK_ID &) iterData->Data[pos];
                CK_ID newId = 0;

                if (iterData->DepContext) {
                    auto it = iterData->DepContext->m_MapID.Find(oldId);
                    newId = (it) ? *it : 0;
                } else {
                    newId = iterData->Context->m_ObjectManager->RealId(oldId);
                }

                if (newId) {
                    oldId = newId;
                    remappedCount++;
                }
            }
        }

        // Process sequence markers
        for (int pos = 2; pos < iterData->ChunkSize - 5; pos++) {
            if (iterData->Data[pos] == SEQ_MARKER[0] &&
                iterData->Data[pos + 1] == SEQ_MARKER[1] &&
                iterData->Data[pos + 2] == SEQ_MARKER[2] &&
                iterData->Data[pos + 3] == SEQ_MARKER[3] &&
                iterData->Data[pos + 4] == SEQ_MARKER[4]) {
                int sequenceCount = iterData->Data[pos + 5];
                int startPos = pos + 6;
                int endPos = startPos + sequenceCount;

                if (endPos > iterData->ChunkSize)
                    break;

                for (int j = startPos; j < endPos; j++) {
                    CK_ID &oldId = (CK_ID &) iterData->Data[j];
                    CK_ID newId = 0;

                    if (iterData->DepContext) {
                        auto it = iterData->DepContext->m_MapID.Find(oldId);
                        newId = (it) ? *it : 0;
                    } else {
                        newId = iterData->Context->m_ObjectManager->RealId(oldId);
                    }

                    if (newId) {
                        oldId = newId;
                        remappedCount++;
                    }
                }
                pos = endPos - 1;
            }
        }
    }

    return remappedCount;
}

int CKStateChunk::ManagerRemapper(ChunkIteratorData *it) {
    int remappedCount = 0;
    int currentIndex = 0;

    if (!it->Managers || it->ManagerCount <= 0)
        return remappedCount;

    while (currentIndex < it->ManagerCount) {
        int *managers = it->Managers;
        int dataPosition = managers[currentIndex];

        if (dataPosition >= 0) {
            // Single manager entry processing
            int *data = it->Data;
            int guidPart1 = data[dataPosition];
            int guidPart2 = data[dataPosition + 1];
            int valuePosition = dataPosition + 2;

            if (it->Guid.d1 == guidPart1 && it->Guid.d2 == guidPart2) {
                int &valueRef = data[valuePosition];
                if (valueRef >= 0 && valueRef < it->NbEntries) {
                    valueRef = it->ConversionTable[valueRef];
                    remappedCount++;
                }
            }
            currentIndex++;
        } else {
            // Sequence of manager entries processing
            int *data = it->Data;
            currentIndex++; // Move to sequence count position
            int sequenceStart = managers[currentIndex];

            int sequenceCount = data[sequenceStart];
            int guidPart1 = data[sequenceStart + 1];
            int guidPart2 = data[sequenceStart + 2];

            if (it->Guid.d1 == guidPart1 && it->Guid.d2 == guidPart2) {
                int valueStartPos = sequenceStart + 3;
                int valueEndPos = valueStartPos + sequenceCount;

                for (int valuePos = valueStartPos; valuePos < valueEndPos; valuePos++) {
                    int &valueRef = data[valuePos];
                    if (valueRef >= 0 && valueRef < it->NbEntries) {
                        valueRef = it->ConversionTable[valueRef];
                    }
                }
                remappedCount += sequenceCount;
            }
            currentIndex++;
        }
    }

    return remappedCount;
}

int CKStateChunk::ParameterRemapper(ChunkIteratorData *it) {
    int *data = it->Data;
    if (!data) return 0;

    // Search for parameter marker sequence
    int currentPos = 0;
    while (true) {
        if (data[currentPos] == 0x40) {
            // PARAMETER_MARKER
            break;
        }

        // Follow linked list pointer
        int nextPos = data[currentPos + 1];
        if (nextPos >= it->ChunkSize || nextPos <= currentPos || nextPos == 0) {
            return 0; // Invalid pointer structure
        }
        currentPos = nextPos;
    }

    // Verify parameter header structure
    const int headerStart = currentPos + 2;
    if (headerStart <= 0) return 0;

    const int guidPart1 = data[headerStart];
    const int guidPart2 = data[headerStart + 1];
    const int paramType = data[headerStart + 2];

    // Validate GUID and parameter type
    if (it->Guid.d1 != guidPart1 ||
        it->Guid.d2 != guidPart2 ||
        paramType != 1) {
        return 0;
    }

    // Get and convert parameter value
    int &paramValueRef = data[headerStart + 4];
    if (paramValueRef >= 0 && paramValueRef < it->NbEntries) {
        paramValueRef = it->ConversionTable[paramValueRef];
        return 1; // Successfully remapped
    }

    return 0; // Invalid parameter index
}
