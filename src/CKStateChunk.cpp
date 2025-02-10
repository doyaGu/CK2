#include "CKStateChunk.h"
#include "CKFile.h"
#include "VxMatrix.h"

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
    return m_ChunkSize * (int)sizeof(int);
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

    int j = m_Data[m_ChunkParser->PrevIdentifierPos + 1];
    int i;
    if (j != 0) {
        i = j;
        while (m_Data[i] != identifier) {
            i = m_Data[i + 1];
            if (i == 0) {
                while (m_Data[i] != identifier) {
                    i = m_Data[i + 1];
                    if (i == j)
                        return FALSE;
                }
            }
        }
    } else {
        i = 0;
        while (m_Data[i] != identifier) {
            i = m_Data[i + 1];
            if (i == j)
                return FALSE;
        }
    }
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

void CKStateChunk::WriteArray_LEndian(int, int, void*)
{
}

void CKStateChunk::WriteArray_LEndian16(int, int, void*)
{
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

int CKStateChunk::ReadArray_LEndian(void**)
{
    return CK_OK;
}

int CKStateChunk::ReadArray_LEndian16(void**)
{
    return CK_OK;
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
    if (array)
    {
        array->Reset();
        while (!array->EndOfList())
        {
            CK_ID id = 0;
            if (context)
            {
                CKObject *object = array->GetData(context);
                if (object && (m_ChunkClassID == -1 || m_Dynamic || object->IsDynamic()))
                    id = object->GetID();
            }
            else
            {
                id = array->GetDataId();
            }
            if (m_File)
            {
                int index = -1;
                XFileObjectsTableIt it = m_File->m_ObjectsHashTable.Find(id);
                if (it != m_File->m_ObjectsHashTable.End())
                    index = *it;
                m_Data[m_ChunkParser->CurrentPos++] = index;
            }
            else
            {
                m_Data[m_ChunkParser->CurrentPos++] = id;
            }
            array->Next();
        }
    }
}

void CKStateChunk::ReadObjectArray(CKObjectArray *array) {
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
    {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return;
    }

    int count = m_Data[m_ChunkParser->CurrentPos++];
    if (count == 0)
    {
        if (array)
            array->Clear();
        return;
    }

    if (m_ChunkVersion < 4)
    {
        m_ChunkParser->CurrentPos += 4;
        count = m_Data[m_ChunkParser->CurrentPos++];
    }

    if (!array)
        m_ChunkParser->CurrentPos += count;

    array->Clear();
    if (count + m_ChunkParser->CurrentPos <= m_ChunkSize)
    {
        if (m_File)
        {
            for (int i = 0; i < count; ++i)
            {
                int index = m_Data[m_ChunkParser->CurrentPos++];
                CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                array->InsertRear(id);
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                array->InsertRear(id);
            }
        }
    }
}

const XObjectArray &CKStateChunk::ReadXObjectArray() {
    m_TempXOA.Clear();
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
    {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return m_TempXOA;
    }

    int count = StartReadSequence();
    if (count != 0)
    {
        if (m_ChunkVersion < 4)
        {
            m_ChunkParser->CurrentPos += 4;
            count = ReadInt();
            m_TempXOA.Resize(count);
            for (int i = 0; i < count; ++i)
                m_TempXOA[i] = m_Data[m_ChunkParser->CurrentPos++];
        }
        else
        {
            m_TempXOA.Resize(count);
            if (count + m_ChunkParser->CurrentPos <= m_ChunkSize)
            {
                if (m_File)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        int index = m_Data[m_ChunkParser->CurrentPos++];
                        CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                        m_TempXOA[i] = id;
                    }
                }
                else
                {
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
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
    {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return m_TempXOPA;
    }

    int count = StartReadSequence();
    if (count != 0)
    {
        if (m_ChunkVersion < 4)
        {
            m_ChunkParser->CurrentPos += 4;
            count = ReadInt();
            m_TempXOPA.Reserve(count);
            for (int i = 0; i < count; ++i)
            {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                CKObject *obj = context->GetObject(id);
                if (obj)
                    m_TempXOPA.PushBack(obj);
            }
        }
        else
        {
            m_TempXOPA.Reserve(count);
            if (count + m_ChunkParser->CurrentPos <= m_ChunkSize)
            {
                if (m_File)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        int index = m_Data[m_ChunkParser->CurrentPos++];
                        CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                        CKObject *obj = context->GetObject(id);
                        if (obj)
                            m_TempXOPA.PushBack(obj);
                    }
                }
                else
                {
                    for (int i = 0; i < count; ++i)
                    {
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
    if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
    {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return NULL;
    }

    int count = m_Data[m_ChunkParser->CurrentPos++];
    if (count == 0)
        return NULL;

    if (m_ChunkVersion < 4)
    {
        m_ChunkParser->CurrentPos += 4;
        count = m_Data[m_ChunkParser->CurrentPos++];
        if (count == 0)
            return NULL;

    }

    CKObjectArray *array = NULL;
    if (count + m_ChunkParser->CurrentPos <= m_ChunkSize)
    {
        array = new CKObjectArray;
        if (!array)
        {
            m_ChunkParser->CurrentPos += count;
            return NULL;
        }

        if (m_File)
        {
            for (int i = 0; i < count; ++i)
            {
                int index = m_Data[m_ChunkParser->CurrentPos++];
                CK_ID id = (index >= 0) ? m_File->m_FileObjects[index].CreatedObject : 0;
                array->InsertRear(id);
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                CK_ID id = m_Data[m_ChunkParser->CurrentPos++];
                array->InsertRear(id);
            }
        }
    }

    return array;
}

void CKStateChunk::WriteSubChunk(CKStateChunk *sub) {
    int size = 4;
    if (sub)
    {
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
    if (sub)
    {
        if (!m_Chunks)
            m_Chunks = new IntListStruct;
        m_Chunks->AddEntry(m_ChunkParser->CurrentPos - 1);
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkClassID;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_DataVersion << 16 | sub->m_ChunkVersion;
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
    if (sub)
    {
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
    if (sub)
    {
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_ChunkClassID;
        m_Data[m_ChunkParser->CurrentPos++] = sub->m_DataVersion << 16 | sub->m_ChunkVersion;
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

CKStateChunk* CKStateChunk::ReadSubChunk() {
   if (!m_ChunkParser || m_ChunkParser->CurrentPos >= m_ChunkSize)
    {
        if (m_File)
            m_File->m_Context->OutputToConsole("Chunk Read error");
        return NULL;
    }

    int size = m_Data[m_ChunkParser->CurrentPos++];
    if (size == 0 || size + m_ChunkParser->CurrentPos > m_ChunkSize)
        return NULL;

    int classID = m_Data[m_ChunkParser->CurrentPos++];
    CKStateChunk *sub = new CKStateChunk(classID, NULL);

    if (m_ChunkVersion < 4)
    {
        sub->m_ChunkSize = m_Data[m_ChunkParser->CurrentPos++];
        ++m_ChunkParser->CurrentPos;
        if (sub->m_ChunkSize == 0)
            return sub;
        sub->m_Data = new int[sub->m_ChunkSize];
        if (sub->m_Data)
        {
            memcpy(sub->m_Data, &m_Data[m_ChunkParser->CurrentPos], sub->m_ChunkSize * sizeof(int));
            m_ChunkParser->CurrentPos += sub->m_ChunkSize;
            return sub;
        }
    }
    else
    {
        int count = m_Data[m_ChunkParser->CurrentPos];
        if (count + sizeof(int) != size)
        {
            int val = m_Data[m_ChunkParser->CurrentPos++];
            sub->m_ChunkVersion = (short int)(val & 0x0000FFFF);
            sub->m_DataVersion = (short int)((val & 0xFFFF0000) >> 16);
            sub->m_ChunkSize = m_Data[m_ChunkParser->CurrentPos++];
            CKBOOL hasFile = m_Data[m_ChunkParser->CurrentPos++];
            int idCount = m_Data[m_ChunkParser->CurrentPos++];
            int chunkCount = m_Data[m_ChunkParser->CurrentPos++];
            int managerCount = 0;
            if (m_ChunkVersion > 4)
                managerCount = m_Data[m_ChunkParser->CurrentPos++];
            if (sub->m_ChunkSize != 0)
            {
                sub->m_Data = new int[sub->m_ChunkSize];
                if (sub->m_Data)
                    ReadAndFillBuffer_LEndian(sub->m_ChunkSize * sizeof(int), sub->m_Data);
            }

            if (idCount > 0)
            {
                sub->m_Ids = new IntListStruct;
                sub->m_Ids->AllocatedSize = idCount;
                sub->m_Ids->Size = idCount;
                sub->m_Ids->Data = new int[idCount];
                if (sub->m_Ids->Data)
                    ReadAndFillBuffer_LEndian(idCount * sizeof(int), sub->m_Ids->Data);
            }

            if (chunkCount > 0)
            {
                sub->m_Chunks = new IntListStruct;
                sub->m_Chunks->AllocatedSize = chunkCount;
                sub->m_Chunks->Size = chunkCount;
                sub->m_Chunks->Data = new int[chunkCount];
                if (sub->m_Chunks->Data)
                    ReadAndFillBuffer_LEndian(chunkCount * sizeof(int), sub->m_Chunks->Data);
            }

            if (managerCount > 0)
            {
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
        }
    }

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
    if (!str)
    {
        CheckSize(sizeof(int));
        m_Data[m_ChunkParser->CurrentPos++] = 0;
    }
    else
    {
        int len = strlen(str);
        int size = len + 1;
        int sz = (size + sizeof(int)) / sizeof(int);
        CheckSize((sz + 1) * sizeof(int));
        m_Data[this->m_ChunkParser->CurrentPos++] = size;
        if (size > 0)
        {
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

    if (buffer)
    {
        int pos = 0;
        int *buf = (int *)buffer;
        buf[pos++] = (((m_ChunkClassID << 8) | m_DataVersion) << 16) | ((chunkOptions << 8) | m_ChunkVersion);
        buf[pos++] = m_ChunkSize;
        memcpy(&buf[pos], m_Data, m_ChunkSize * sizeof(int));
        pos += m_ChunkSize;
        if (m_Ids)
        {
            memcpy(&buf[pos], m_Ids->Data, m_Ids->Size * sizeof(int));
            pos += m_Ids->Size;
        }
        if (m_Chunks)
        {
            memcpy(&buf[pos], m_Chunks->Data, m_Chunks->Size * sizeof(int));
            pos += m_Chunks->Size;
        }
        if (m_Managers)
        {
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
    int *buf = (int *)buffer;

    Clear();
    int val = buf[pos++];
    m_DataVersion = (short)(val & 0x0000FFFF);
    m_DataVersion &= 0x00FF;
    m_ChunkVersion = (short)((val & 0xFFFF0000) >> 16);
    m_ChunkVersion &= 0x00FF;

    if (m_ChunkVersion < CHUNK_VERSION2)
    {
        m_ChunkClassID = buf[pos++];
        m_ChunkSize = buf[pos++];
        ++pos;
        int idCount = buf[pos++];
        int chunkCount = buf[pos++];

        if (m_ChunkSize != 0)
        {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if (idCount > 0)
        {
            m_Ids = new IntListStruct;
            m_Ids->AllocatedSize = idCount;
            m_Ids->Size = idCount;
            m_Ids->Data = new int[idCount];
            if (m_Ids->Data)
            {
                memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                pos += idCount;
            }
        }

        if (chunkCount > 0)
        {
            m_Chunks = new IntListStruct;
            m_Chunks->AllocatedSize = chunkCount;
            m_Chunks->Size = chunkCount;
            m_Chunks->Data = new int[chunkCount];
            if (m_Chunks->Data)
            {
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

        if (m_ChunkSize != 0)
        {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if (idCount > 0)
        {
            m_Ids = new IntListStruct;
            m_Ids->AllocatedSize = idCount;
            m_Ids->Size = idCount;
            m_Ids->Data = new int[idCount];
            if (m_Ids->Data)
            {
                memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                pos += idCount;
            }
        }

        if (chunkCount > 0)
        {
            m_Chunks = new IntListStruct;
            m_Chunks->AllocatedSize = chunkCount;
            m_Chunks->Size = chunkCount;
            m_Chunks->Data = new int[chunkCount];
            if (m_Chunks->Data)
            {
                memcpy(m_Chunks->Data, &buf[pos], chunkCount * sizeof(int));
                pos += chunkCount;
            }
        }

        if (managerCount > 0)
        {
            m_Managers = new IntListStruct;
            m_Managers->AllocatedSize = managerCount;
            m_Managers->Size = managerCount;
            m_Managers->Data = new int[managerCount];
            if (m_Managers->Data)
            {
                memcpy(m_Managers->Data, &buf[pos], managerCount * sizeof(int));
                pos += managerCount;
            }
        }

        m_File = NULL;
        return TRUE;
    } else if (m_ChunkVersion <= CHUNK_VERSION4) {
        CKBYTE chunkOptions = (m_ChunkVersion & 0xFF00) >> 8;

        m_ChunkClassID = (m_DataVersion & 0xFF00) >> 8;
        m_ChunkSize = buf[pos++];

        if (m_ChunkSize != 0)
        {
            m_Data = new int[m_ChunkSize];
            if (m_Data) {
                memcpy(m_Data, &buf[pos], m_ChunkSize * sizeof(int));
                pos += m_ChunkSize;
            }
        }

        if ((chunkOptions & CHNK_OPTION_FILE) == 0)
            m_File = NULL;

        if ((chunkOptions & CHNK_OPTION_IDS) != 0)
        {
            int idCount = buf[pos++];
            if (idCount > 0)
            {
                m_Ids = new IntListStruct;
                m_Ids->AllocatedSize = idCount;
                m_Ids->Size = idCount;
                m_Ids->Data = new int[idCount];
                if (m_Ids->Data)
                {
                    memcpy(m_Ids->Data, &buf[pos], idCount * sizeof(int));
                    pos += idCount;
                }
            }
        }

        if ((chunkOptions & CHNK_OPTION_CHN) != 0)
        {
            int chunkCount = buf[pos++];
            if (chunkCount > 0)
            {
                m_Chunks = new IntListStruct;
                m_Chunks->AllocatedSize = chunkCount;
                m_Chunks->Size = chunkCount;
                m_Chunks->Data = new int[chunkCount];
                if (m_Chunks->Data)
                {
                    memcpy(m_Chunks->Data, &buf[pos], chunkCount * sizeof(int));
                    pos += chunkCount;
                }
            }
        }

        if ((chunkOptions & CHNK_OPTION_MAN) != 0)
        {
            int managerCount = buf[pos++];
            if (managerCount > 0)
            {
                m_Managers = new IntListStruct;
                m_Managers->AllocatedSize = managerCount;
                m_Managers->Size = managerCount;
                m_Managers->Data = new int[managerCount];
                if (m_Managers->Data)
                {
                    memcpy(m_Managers->Data, &buf[pos], managerCount * sizeof(int));
                    pos += managerCount;
                }
            }
        }

        return TRUE;
    }

    return FALSE;
}

int CKStateChunk::RemapObject(CK_ID old_id, CK_ID new_id) {
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
    if (m_Chunks)
    {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Ids)
    {
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
    if (m_Chunks)
    {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Managers)
    {
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
    if (m_Chunks)
    {
        data.Chunks = m_Chunks->Data;
        data.ChunkCount = m_Chunks->Size;
    }
    if (m_Ids)
    {
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

CKBOOL ReadRawBitmapHeader(VxImageDescEx &desc) {
    return FALSE;
}

CKBOOL ReadRawBitmapData(VxImageDescEx &desc) {
    return FALSE;
}

CKBYTE *CKStateChunk::ReadRawBitmap(VxImageDescEx &desc) {
    return nullptr;
}

void CKStateChunk::WriteBitmap(BITMAP_HANDLE bitmap, CKSTRING ext) {
}

BITMAP_HANDLE CKStateChunk::ReadBitmap() {
    return nullptr;
}

CKDWORD CKStateChunk::ComputeCRC(CKDWORD adler)
{
    return adler32(adler, (unsigned char *)m_Data, m_ChunkSize * sizeof(int));
}

void CKStateChunk::Pack(int CompressionLevel) {
    uLongf size = m_ChunkSize * sizeof(int);
    Bytef *buf = new Bytef[size];
    if (compress2(buf, &size, (const Bytef *)m_Data, size, CompressionLevel) == Z_OK)
    {
        Bytef *data = new Bytef[size];
        memcpy(data, m_Data, size);
        delete[] m_Data;
        m_Data = (int *)data;
        m_ChunkSize = size;
    }
    delete[] buf;
}

CKBOOL CKStateChunk::UnPack(int DestSize) {
    uLongf size = DestSize;
    Bytef *buf = new Bytef[DestSize];
    int err = uncompress(buf, &size, (const Bytef *)m_Data, m_ChunkSize);
    if (err == Z_OK)
    {
        int sz = DestSize / sizeof(int);
        int *data = new int [sz];
        memcpy(data, buf, DestSize);
        delete[] m_Data;
        m_Data = data;
        m_ChunkSize = sz;
    }
    delete[] buf;
    return err == Z_OK;
}

CKBOOL CKStateChunk::ReadRawBitmapHeader(VxImageDescEx &desc) {
    return 0;
}

CKBOOL CKStateChunk::ReadRawBitmapData(VxImageDescEx &desc) {
    return 0;
}

int CKStateChunk::IterateAndDo(ChunkIterateFct fct, ChunkIteratorData *it) {
    fct(it);
    return 0;
}

int CKStateChunk::ObjectRemapper(ChunkIteratorData *it) {
    return 0;
}

int CKStateChunk::ManagerRemapper(ChunkIteratorData *it) {
    return 0;
}

int CKStateChunk::ParameterRemapper(ChunkIteratorData *it) {
    return 0;
}