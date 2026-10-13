#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "CKAll.h"

namespace {

// Link CK2Static: these replacements must observe CK2's real chunk allocation
// and deletion calls, including their scalar/array kind. No mock chunks are used.
struct Allocation {
    std::uintptr_t Address;
    int Frees;
    bool Array;
    bool ArrayFree;
};
Allocation Ledger[64];
int AllocationCount = 0;
bool Tracking = false;
bool Overflow = false;

void Allocated(void *pointer, bool array) {
    if (!Tracking) return;
    if (AllocationCount == 64) { Overflow = true; return; }
    Allocation &entry = Ledger[AllocationCount++];
    entry.Address = reinterpret_cast<std::uintptr_t>(pointer);
    entry.Frees = 0; entry.Array = array; entry.ArrayFree = false;
}

void Released(void *pointer, bool array) {
    if (!Tracking || !pointer) return;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    for (int i = AllocationCount - 1; i >= 0; --i) {
        if (Ledger[i].Address == address && Ledger[i].Frees == 0) {
            ++Ledger[i].Frees; Ledger[i].ArrayFree = array; return;
        }
    }
}

void StartTracking() { AllocationCount = 0; Overflow = false; Tracking = true; }

void ExpectAllReleasedAndRecoverFailedBaseline() {
    ASSERT_FALSE(Tracking);
    EXPECT_FALSE(Overflow);
    EXPECT_GE(AllocationCount, 3) << "CK2Static allocation observation is required";
    for (int i = 0; i < AllocationCount; ++i) {
        EXPECT_EQ(1, Ledger[i].Frees) << "allocation " << i;
        if (Ledger[i].Frees) EXPECT_EQ(Ledger[i].Array, Ledger[i].ArrayFree) << "allocation kind " << i;
        // The frozen-before helper frees its shell but leaks simple payload/parser
        // allocations. Recover those after observing the bug; never hide the leak.
        if (!Ledger[i].Frees) std::free(reinterpret_cast<void *>(Ledger[i].Address));
    }
}

void Word(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset+i] = static_cast<unsigned char>(value >> (i*8));
}

// The offsets and bytes are format expectations, not a copied production struct.
struct LiteralFile {
    unsigned char Bytes[122];
    LiteralFile() {
        std::memset(Bytes, 0, sizeof(Bytes));
        std::memcpy(Bytes, "Nemo Fi", 7);
        Word(Bytes, 12, 0x13022002); Word(Bytes, 16, 8); Word(Bytes, 24, 4);
        Word(Bytes, 28, 30); Word(Bytes, 32, 28); Word(Bytes, 36, 28);
        Word(Bytes, 40, 1); Word(Bytes, 44, 1); Word(Bytes, 48, 0x1234);
        Word(Bytes, 52, 11); Word(Bytes, 56, 0x02050030); Word(Bytes, 60, 30);
        Word(Bytes, 64, 0x1234); Word(Bytes, 68, CKCID_OBJECT); Word(Bytes, 72, 12);
        Word(Bytes, 76, 6); std::memcpy(Bytes+80, "Record", 6);
        // Header tail: zero plugin dependencies and zero included-file section.
        Word(Bytes, 94, 0x11223344); Word(Bytes, 98, 0x55667788); Word(Bytes, 102, 0);
        Word(Bytes, 106, 12); Word(Bytes, 110, (7u<<16)|(CKCID_OBJECT<<8)|9u);
        Word(Bytes, 114, 1); Word(Bytes, 118, 0xA1B2C3D4);
        Refresh();
    }
    void Refresh() {
        Word(Bytes, 8, 0);
        unsigned int a = 0, b = 0;
        for (int i = 0; i < sizeof(Bytes); ++i) { a=(a+Bytes[i])%65521; b=(b+a)%65521; }
        Word(Bytes, 8, a|(b<<16));
    }
};

class RecordFile : public CKFile {
public:
    explicit RecordFile(CKContext *context) : CKFile(context) {}
    CKERROR ReadData() { return ReadFileData(&m_Parser); }
    void Reset() { ClearData(); }
};

class CKFileRecordTypesTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    }
    void TearDown() override {
        Tracking = false;
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
};

} // namespace

void *operator new(std::size_t bytes) {
    void *pointer = std::malloc(bytes ? bytes : 1);
    if (!pointer) throw std::bad_alloc();
    Allocated(pointer, false); return pointer;
}
void *operator new[](std::size_t bytes) {
    void *pointer = std::malloc(bytes ? bytes : 1);
    if (!pointer) throw std::bad_alloc();
    Allocated(pointer, true); return pointer;
}
void operator delete(void *pointer) noexcept { Released(pointer, false); std::free(pointer); }
void operator delete[](void *pointer) noexcept { Released(pointer, true); std::free(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { Released(pointer, false); std::free(pointer); }
void operator delete[](void *pointer, std::size_t) noexcept { Released(pointer, true); std::free(pointer); }

namespace {

TEST_F(CKFileRecordTypesTest, ConstructorInitializesEveryDeclaredFieldAndRecordByte) {
    alignas(CKFileObject) unsigned char storage[sizeof(CKFileObject)];
    std::memset(storage, 0xA5, sizeof(storage));
    CKFileObject *record = new (storage) CKFileObject;
    EXPECT_EQ(0u, record->Object); EXPECT_EQ(0u, record->CreatedObject);
    EXPECT_EQ(0, record->ObjectCid); EXPECT_EQ(nullptr, record->ObjPtr);
    EXPECT_EQ(nullptr, record->Name); EXPECT_EQ(nullptr, record->Data);
    EXPECT_EQ(0, record->PostPackSize); EXPECT_EQ(0, record->PrePackSize);
    EXPECT_EQ(CKFileObject::CK_FO_DEFAULT, record->Options);
    EXPECT_EQ(0, record->FileIndex); EXPECT_EQ(0u, record->SaveFlags);
    for (unsigned char byte : storage) EXPECT_EQ(0, byte);
    record->~CKFileObject();
}

TEST_F(CKFileRecordTypesTest, ObjectRecordHasExpectedPlatformOffsetsAndScalarWidths) {
    const bool x64 = sizeof(void *) == 8;
    EXPECT_EQ(4u, sizeof(CK_ID)); EXPECT_EQ(4u, sizeof(CK_CLASSID)); EXPECT_EQ(4u, sizeof(CKDWORD));
    EXPECT_EQ(4u, sizeof(CKFileObject::CK_FO_OPTIONS));
    EXPECT_EQ(0, CKFileObject::CK_FO_DEFAULT); EXPECT_EQ(1, CKFileObject::CK_FO_RENAMEOBJECT);
    EXPECT_EQ(2, CKFileObject::CK_FO_REPLACEOBJECT); EXPECT_EQ(3, CKFileObject::CK_FO_DONTLOADOBJECT);
    EXPECT_EQ(x64 ? 64u : 44u, sizeof(CKFileObject));
    EXPECT_EQ(x64 ? 8u : 4u, alignof(CKFileObject));
    EXPECT_EQ(0u, offsetof(CKFileObject, Object));
    EXPECT_EQ(4u, offsetof(CKFileObject, CreatedObject));
    EXPECT_EQ(8u, offsetof(CKFileObject, ObjectCid));
    EXPECT_EQ(x64 ? 16u : 12u, offsetof(CKFileObject, ObjPtr));
    EXPECT_EQ(x64 ? 24u : 16u, offsetof(CKFileObject, Name));
    EXPECT_EQ(x64 ? 32u : 20u, offsetof(CKFileObject, Data));
    EXPECT_EQ(x64 ? 40u : 24u, offsetof(CKFileObject, PostPackSize));
    EXPECT_EQ(x64 ? 44u : 28u, offsetof(CKFileObject, PrePackSize));
    EXPECT_EQ(x64 ? 48u : 32u, offsetof(CKFileObject, Options));
    EXPECT_EQ(x64 ? 52u : 36u, offsetof(CKFileObject, FileIndex));
    EXPECT_EQ(x64 ? 56u : 40u, offsetof(CKFileObject, SaveFlags));
}

TEST_F(CKFileRecordTypesTest, CanBeLoadRequiresBothLivePointersAndAllowsAllLoadableOptions) {
    CKObject object(Context, "predicate object");
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    for (int option = 0; option < 4; ++option) {
        for (int pointers = 0; pointers < 4; ++pointers) {
            CKFileObject record;
            record.Options = static_cast<CKFileObject::CK_FO_OPTIONS>(option);
            record.ObjPtr = (pointers & 1) ? &object : nullptr;
            record.Data = (pointers & 2) ? &chunk : nullptr;
            EXPECT_EQ(pointers == 3 && option != 3, !!record.CanBeLoad()) << option << "/" << pointers;
        }
    }
}

TEST_F(CKFileRecordTypesTest, CleanDataReleasesLiveChunkParserAndPayloadWithScalarShell) {
    CKFileObject record;
    StartTracking();
    record.Data = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    record.Data->StartWrite(); record.Data->WriteIdentifier(0x77); record.Data->WriteInt(123);
    record.CleanData(); record.CleanData();
    Tracking = false;
    EXPECT_EQ(nullptr, record.Data);
    ExpectAllReleasedAndRecoverFailedBaseline();
}

TEST_F(CKFileRecordTypesTest, CleanDataPreservesBorrowedObjectNameAndRecordMetadata) {
    CKObject object(Context, "borrowed object");
    CKFileObject record;
    record.ObjPtr = &object; record.Name = CKStrdup("owned separately by CKFile");
    record.Object = 123; record.CreatedObject = object.GetID(); record.ObjectCid = CKCID_OBJECT;
    record.Options = CKFileObject::CK_FO_REPLACEOBJECT;
    record.FileIndex = 45; record.SaveFlags = 0x01234567; record.PrePackSize = 12; record.PostPackSize = 8;
    const CK_ID id = object.GetID();
    StartTracking();
    record.Data = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    record.Data->StartWrite(); record.Data->WriteInt(456); record.Data->CloseChunk();
    record.CleanData(); record.CleanData();
    Tracking = false;
    EXPECT_EQ(nullptr, record.Data); EXPECT_EQ(&object, record.ObjPtr); EXPECT_EQ(&object, Context->GetObject(id));
    EXPECT_STREQ("owned separately by CKFile", record.Name);
    EXPECT_EQ(123u, record.Object); EXPECT_EQ(id, record.CreatedObject); EXPECT_EQ(CKCID_OBJECT, record.ObjectCid);
    EXPECT_EQ(CKFileObject::CK_FO_REPLACEOBJECT, record.Options);
    EXPECT_EQ(45, record.FileIndex); EXPECT_EQ(0x01234567u, record.SaveFlags);
    EXPECT_EQ(12, record.PrePackSize); EXPECT_EQ(8, record.PostPackSize);
    ExpectAllReleasedAndRecoverFailedBaseline();
    CKDeletePointer(const_cast<char *>(record.Name));
}

TEST_F(CKFileRecordTypesTest, CleanDataAcceptsNullAndLeavesOtherFieldsUntouched) {
    CKFileObject record; record.Object = 42;
    record.CleanData(); record.CleanData();
    EXPECT_EQ(nullptr, record.Data); EXPECT_EQ(42u, record.Object);
}

TEST_F(CKFileRecordTypesTest, ManagerRecordHasExpectedPlatformGuidAndPointerLayout) {
    const bool x64 = sizeof(void *) == 8;
    EXPECT_EQ(8u, sizeof(CKGUID)); EXPECT_EQ(4u, alignof(CKGUID));
    EXPECT_EQ(x64 ? 16u : 12u, sizeof(CKFileManagerData));
    EXPECT_EQ(x64 ? 8u : 4u, alignof(CKFileManagerData));
    EXPECT_EQ(0u, offsetof(CKFileManagerData, data));
    EXPECT_EQ(x64 ? 8u : 4u, offsetof(CKFileManagerData, Manager));
    CKFileManagerData record = {nullptr, CKGUID(0, 0)};
    EXPECT_EQ(nullptr, record.data); EXPECT_EQ(0u, record.Manager.d[0]); EXPECT_EQ(0u, record.Manager.d[1]);
}

TEST_F(CKFileRecordTypesTest, ClearDataReleasesBothRecordChunkKindsAndKeepsContextObjectAlive) {
    RecordFile file(Context); CKObject object(Context, "borrowed"); const CK_ID id=object.GetID();
    file.m_FileObjects.Resize(1); file.m_FileObjects[0].ObjPtr=&object;
    file.m_ManagersData.Resize(1); file.m_ManagersData[0].Manager=CKGUID(11,22);
    StartTracking();
    file.m_FileObjects[0].Data=CreateCKStateChunk(CKCID_OBJECT,&file);
    file.m_FileObjects[0].Data->StartWrite(); file.m_FileObjects[0].Data->WriteInt(1);
    file.m_ManagersData[0].data=CreateCKStateChunk(CKCID_OBJECT,&file);
    file.m_ManagersData[0].data->StartWrite(); file.m_ManagersData[0].data->WriteInt(2);
    file.Reset(); file.Reset(); Tracking=false;
    EXPECT_EQ(&object,Context->GetObject(id));
    EXPECT_EQ(0,file.m_FileObjects.Size()); EXPECT_EQ(0,file.m_ManagersData.Size());
    ExpectAllReleasedAndRecoverFailedBaseline();
}

TEST_F(CKFileRecordTypesTest, LiteralHeaderProjectsAllSupportedPartZeroAndPartOneFields) {
    LiteralFile wire; RecordFile file(Context);
    ASSERT_EQ(CK_OK,file.OpenMemory(wire.Bytes,sizeof(wire.Bytes),CK_LOAD_DEFAULT));
    const CKFileInfo &info=file.m_FileInfo;
    EXPECT_EQ(0x13022002u,info.CKVersion); EXPECT_EQ(8u,info.FileVersion); EXPECT_EQ(4u,info.FileWriteMode);
    EXPECT_EQ(30u,info.Hdr1PackSize); EXPECT_EQ(30u,info.Hdr1UnPackSize);
    EXPECT_EQ(28u,info.DataPackSize); EXPECT_EQ(28u,info.DataUnPackSize);
    EXPECT_EQ(1u,info.ManagerCount); EXPECT_EQ(1u,info.ObjectCount); EXPECT_EQ(0x1234u,info.MaxIDSaved);
    EXPECT_EQ(11u,info.ProductVersion); EXPECT_EQ(0x02050030u,info.ProductBuild);
    EXPECT_EQ(sizeof(wire.Bytes),info.FileSize);
    unsigned int expectedCrc=0;
    for (int i=0;i<4;++i) expectedCrc |= static_cast<unsigned int>(wire.Bytes[8+i]) << (i*8);
    EXPECT_EQ(expectedCrc,info.Crc);
}

TEST_F(CKFileRecordTypesTest, LiteralObjectAndManagerRecordsPreserveWireIdentityAndOwnedData) {
    LiteralFile wire; RecordFile file(Context);
    ASSERT_EQ(CK_OK,file.OpenMemory(wire.Bytes,sizeof(wire.Bytes),CK_LOAD_DEFAULT));
    ASSERT_EQ(1,file.m_FileObjects.Size()); CKFileObject &record=file.m_FileObjects[0];
    EXPECT_EQ(0x1234u,record.Object); EXPECT_EQ(CKCID_OBJECT,record.ObjectCid); EXPECT_EQ(12,record.FileIndex);
    EXPECT_STREQ("Record",record.Name); EXPECT_EQ(nullptr,record.ObjPtr); EXPECT_EQ(nullptr,record.Data);
    ASSERT_EQ(CK_OK,file.ReadData()); ASSERT_EQ(1,file.m_ManagersData.Size());
    EXPECT_EQ(0x11223344u,file.m_ManagersData[0].Manager.d[0]);
    EXPECT_EQ(0x55667788u,file.m_ManagersData[0].Manager.d[1]); EXPECT_EQ(nullptr,file.m_ManagersData[0].data);
    ASSERT_NE(nullptr,record.Data); EXPECT_EQ(4,record.PrePackSize); EXPECT_EQ(4,record.PostPackSize);
    record.Data->StartRead(); EXPECT_EQ(static_cast<int>(0xA1B2C3D4u),record.Data->ReadInt());
}

TEST_F(CKFileRecordTypesTest, SignatureIsCheckedAtTheFirstFourWireBytes) {
    LiteralFile wire; RecordFile file(Context); wire.Bytes[0]='X'; wire.Refresh();
    EXPECT_EQ(CKERR_INVALIDFILE,file.OpenMemory(wire.Bytes,sizeof(wire.Bytes)));
    EXPECT_EQ(nullptr,file.m_Parser);
}

TEST_F(CKFileRecordTypesTest, HeaderCrcRejectsAChangedPartOneField) {
    LiteralFile wire; RecordFile file(Context); wire.Bytes[56]^=1;
    EXPECT_EQ(CKERR_FILECRCERROR,file.OpenMemory(wire.Bytes,sizeof(wire.Bytes)));
}

} // namespace
