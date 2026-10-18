#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "CKAll.h"

static const CKGUID SessionGuid(0x38112233, 0x38445566);

static void Put(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}
static unsigned int Get(const unsigned char *bytes, int offset) {
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) value |= unsigned(bytes[offset + i]) << (8 * i);
    return value;
}

// Independent format-8 input: one named BeObject and optional real manager data.
// Format version is a wire discriminator, not an assertion about its producer.
struct CKTestSessionFrame {
    unsigned char Bytes[256];
    int Size;
    explicit CKTestSessionFrame(bool manager = false, unsigned int mode = CKFILE_UNCOMPRESSED) {
        std::memset(Bytes, 0, sizeof(Bytes));
        std::memcpy(Bytes, "Nemo Fi", 7);
        Put(Bytes, 12, 0x13022002); Put(Bytes, 16, 8); Put(Bytes, 24, mode);
        Put(Bytes, 28, 35); Put(Bytes, 60, 35);
        Put(Bytes, 32, manager ? 40 : 12); Put(Bytes, 36, manager ? 40 : 12);
        Put(Bytes, 40, manager ? 1 : 0); Put(Bytes, 44, 1); Put(Bytes, 48, 41);
        Put(Bytes, 56, 0x01010000);
        Put(Bytes, 64, 41); Put(Bytes, 68, CKCID_BEOBJECT);
        Put(Bytes, 72, manager ? 127 : 99); Put(Bytes, 76, 7);
        std::memcpy(Bytes + 80, "Session", 7);
        Put(Bytes, 91, 4); // Included-file header has its count DWORD.
        int cursor = 99;
        if (manager) {
            Put(Bytes, cursor, SessionGuid.d1); Put(Bytes, cursor + 4, SessionGuid.d2);
            Put(Bytes, cursor + 8, 16); Put(Bytes, cursor + 12, (7u << 16) | (CKCID_OBJECT << 8) | 9u);
            Put(Bytes, cursor + 16, 2); Put(Bytes, cursor + 20, 0x13572468); Put(Bytes, cursor + 24, 0x24681357);
            cursor += 28;
        }
        Put(Bytes, cursor, 8); Put(Bytes, cursor + 4, (7u << 16) | (CKCID_BEOBJECT << 8) | 9u);
        Size = cursor + 12;
        unsigned int a = 0, b = 0;
        for (int i = 0; i < Size; ++i) { a = (a + Bytes[i]) % 65521u; b = (b + a) % 65521u; }
        Put(Bytes, 8, a | (b << 16));
    }
};

struct CKTestChoiceProbe {
    CKContext *Context;
    CKObject *Existing;
    CK_LOADMODE Choice;
    int Calls = 0;
};
static CK_LOADMODE Choose(CK_CLASSID cid, CKSTRING oldName, CKSTRING newName, CKObject **object, void *data) {
    CKTestChoiceProbe *probe = static_cast<CKTestChoiceProbe *>(data);
    ++probe->Calls;
    EXPECT_EQ(CKCID_BEOBJECT, cid); EXPECT_STREQ("Session", oldName);
    EXPECT_NE(nullptr, object);
    if (object) EXPECT_EQ(probe->Existing, *object);
    EXPECT_TRUE(probe->Context->m_InLoad);
    // SDK callback's NewName is the writable context string buffer. The portable
    // CKSTRING alias is const char*, while the callback contract still fills it.
    if (probe->Choice == CKLOAD_RENAME) std::strcpy(const_cast<char *>(newName), "CallbackRenamed");
    return probe->Choice;
}

struct CKTestUIProbe {
    CKContext *Context;
    int LoadCount = 0, SaveCount = 0, InterfaceCount = 0;
    int Loaded[8] = {}, Totals[8] = {};
};
static CKERROR ObserveUI(CKUICallbackStruct &event, void *data) {
    CKTestUIProbe *probe = static_cast<CKTestUIProbe *>(data);
    if (event.Reason == CKUIM_CREATEINTERFACECHUNK) {
        ++probe->InterfaceCount;
        EXPECT_TRUE(probe->Context->m_Saving); EXPECT_NE(nullptr, event.Objects);
    } else if (event.Reason == CKUIM_LOADSAVEPROGRESS) {
        if (probe->Context->m_InLoad) {
            if (probe->LoadCount < 8) {
                probe->Loaded[probe->LoadCount] = event.NbObjectsLoaded;
                probe->Totals[probe->LoadCount] = event.NbObjectsToLoad;
            }
            ++probe->LoadCount;
        } else {
            EXPECT_TRUE(probe->Context->m_Saving); ++probe->SaveCount;
        }
    }
    return CK_OK;
}

class CKTestSessionManager : public CKBaseManager {
public:
    explicit CKTestSessionManager(CKContext *context)
        : CKBaseManager(context, SessionGuid, const_cast<char *>("Pass38 sessions")) {}
    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_PreLoad | CKMANAGER_FUNC_PostLoad | CKMANAGER_FUNC_PreSave | CKMANAGER_FUNC_PostSave;
    }
    CKERROR PreLoad() override { EXPECT_FALSE(m_Context->m_InLoad); ++PreLoads; return CK_OK; }
    CKERROR PostLoad() override {
        EXPECT_TRUE(m_Context->m_InLoad); EXPECT_EQ(nullptr, m_Context->m_UserLoadCallBack);
        EXPECT_EQ(CKLOAD_INVALID, m_Context->m_GeneralLoadMode); ++PostLoads; return CK_OK;
    }
    CKERROR LoadData(CKStateChunk *chunk, CKFile *file) override {
        EXPECT_TRUE(m_Context->m_InLoad); EXPECT_NE(nullptr, chunk); EXPECT_NE(nullptr, file);
        if (chunk) {
            chunk->StartRead(); EXPECT_EQ(0x13572468, chunk->ReadInt()); EXPECT_EQ(0x24681357, chunk->ReadInt());
        }
        ++DataLoads; return CK_OK;
    }
    CKERROR PreSave() override { EXPECT_FALSE(m_Context->m_Saving); ++PreSaves; return CK_OK; }
    CKERROR PostSave() override { EXPECT_TRUE(m_Context->m_Saving); ++PostSaves; return CK_OK; }
    CKStateChunk *SaveData(CKFile *file) override {
        EXPECT_TRUE(m_Context->m_Saving); SaveFlags = file->m_Flags;
        AttributeSaving = m_Context->GetAttributeManager()->m_Saving;
        CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, file);
        chunk->StartWrite(); chunk->WriteInt(0x13572468); chunk->WriteInt(0x24681357); chunk->CloseChunk();
        ++DataSaves; return chunk;
    }
    int PreLoads = 0, PostLoads = 0, DataLoads = 0, PreSaves = 0, PostSaves = 0, DataSaves = 0;
    CKDWORD SaveFlags = 0; int AttributeSaving = 0;
};

class CKFileSessionContractTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    std::filesystem::path Directory;
    static void SetUpTestSuite() { ASSERT_EQ(CK_OK, CKStartUp()); }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        Context->SetFileWriteMode(CKFILE_UNCOMPRESSED);
        Directory = std::filesystem::temp_directory_path() /
            (std::string("ckfile-pass38-") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::create_directories(Directory);
    }
    void TearDown() override {
        if (Context) CKCloseContext(Context);
        std::error_code ignored; std::filesystem::remove_all(Directory, ignored);
    }
    CKObject *Existing() { return Context->CreateObject(CKCID_BEOBJECT, const_cast<char *>("Session")); }
    CKTestSessionManager *Manager() {
        CKTestSessionManager *manager = new CKTestSessionManager(Context);
        EXPECT_EQ(CK_OK, Context->RegisterNewManager(manager)); return manager;
    }
    std::string Path(const char *name) { return (Directory / name).string(); }
    void Write(const std::string &path, const CKTestSessionFrame &frame) {
        FILE *stream = std::fopen(path.c_str(), "wb"); ASSERT_NE(nullptr, stream);
        EXPECT_EQ(static_cast<size_t>(frame.Size), std::fwrite(frame.Bytes, 1, frame.Size, stream));
        EXPECT_EQ(0, std::fclose(stream));
    }
    void ExpectChoiceCleared() {
        EXPECT_EQ(nullptr, Context->m_UserLoadCallBack); EXPECT_EQ(nullptr, Context->m_UserLoadCallBackArgs);
        EXPECT_EQ(CKLOAD_INVALID, Context->m_GeneralLoadMode); EXPECT_EQ(CKLOAD_INVALID, Context->m_3DObjectsLoadMode);
        EXPECT_EQ(CKLOAD_INVALID, Context->m_MeshLoadMode); EXPECT_EQ(CKLOAD_INVALID, Context->m_MatTexturesLoadMode);
        EXPECT_FALSE(Context->m_InLoad); EXPECT_EQ(nullptr, Context->m_ObjectManager->m_LoadSession);
    }
};

TEST_F(CKFileSessionContractTest, TwoStageLoadKeepsChoiceCallbackUntilItCreatesTheObject) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    CKTestChoiceProbe choice{Context, existing, CKLOAD_RENAME}; Context->SetUserLoadCallback(Choose, &choice);
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDUPLICATES));
    EXPECT_EQ(0, choice.Calls); EXPECT_EQ(Choose, Context->m_UserLoadCallBack);
    ASSERT_EQ(CK_OK, file.LoadFileData(&objects)); ASSERT_EQ(1, choice.Calls);
    ASSERT_EQ(1, file.m_FileObjects.Size()); EXPECT_NE(existing, file.m_FileObjects[0].ObjPtr);
    EXPECT_STREQ("CallbackRenamed", file.m_FileObjects[0].ObjPtr->GetName());
    EXPECT_STREQ("Session", existing->GetName()); EXPECT_EQ(1, objects.GetCount()); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, UseCurrentRegistersExistingObjectWithoutReloadingOrReturningIt) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    existing->ModifyObjectFlags(CK_OBJECT_HIERACHICALHIDE, CK_OBJECT_VISIBLE);
    const CKDWORD oldFlags = existing->GetObjectFlags(); const int oldCount = Context->GetObjectCount();
    CKTestChoiceProbe choice{Context, existing, CKLOAD_USECURRENT}; Context->SetUserLoadCallback(Choose, &choice);
    CKTestUIProbe ui{Context}; Context->SetInterfaceMode(TRUE, ObserveUI, &ui);
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects,
        static_cast<CK_LOAD_FLAGS>(CK_LOAD_CHECKDUPLICATES | CK_LOAD_AS_DYNAMIC_OBJECT)));
    EXPECT_EQ(1, choice.Calls); EXPECT_EQ(oldCount, Context->GetObjectCount());
    EXPECT_EQ(existing, file.m_FileObjects[0].ObjPtr); EXPECT_EQ(existing->GetID(), file.m_FileObjects[0].CreatedObject);
    EXPECT_EQ(CKFileObject::CK_FO_RENAMEOBJECT, file.m_FileObjects[0].Options);
    EXPECT_EQ(oldFlags, existing->GetObjectFlags()); EXPECT_EQ(0, objects.GetCount()); EXPECT_EQ(0, ui.LoadCount);
    ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, DefaultLoadCreatesDuplicatesWithoutInvokingTheChoiceCallback) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    CKTestChoiceProbe choice{Context, existing, CKLOAD_USECURRENT}; Context->SetUserLoadCallback(Choose, &choice);
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_DEFAULT));
    EXPECT_EQ(0, choice.Calls); EXPECT_NE(existing, file.m_FileObjects[0].ObjPtr);
    EXPECT_STREQ("Session", file.m_FileObjects[0].ObjPtr->GetName()); EXPECT_EQ(1, objects.GetCount());
    ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, AutomaticRenameCreatesAUniqueNameAndReturnsTheNewObject) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    Context->SetAutomaticLoadMode(CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME);
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_AUTOMATICMODE));
    CKObject *created = file.m_FileObjects[0].ObjPtr; ASSERT_NE(nullptr, created); EXPECT_NE(existing, created);
    EXPECT_STRNE("Session", created->GetName()); EXPECT_EQ(created->GetID(), objects.GetDataId());
    EXPECT_EQ(created, Context->GetObjectByNameAndClass(created->GetName(), CKCID_BEOBJECT)); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, UserChoiceTakesPrecedenceOverAutomaticRename) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    Context->SetAutomaticLoadMode(CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME);
    CKTestChoiceProbe choice{Context, existing, CKLOAD_USECURRENT}; Context->SetUserLoadCallback(Choose, &choice);
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_AUTOMATICMODE));
    EXPECT_EQ(1, choice.Calls); EXPECT_EQ(existing, file.m_FileObjects[0].ObjPtr);
    EXPECT_EQ(0, objects.GetCount()); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, RepeatedLoadRearmsCallbacksAndAppendsOnlyNewlyLoadedObjects) {
    CKObject *existing = Existing(); ASSERT_NE(nullptr, existing);
    CKTestChoiceProbe first{Context, existing, CKLOAD_USECURRENT}, second{Context, existing, CKLOAD_RENAME};
    CKTestSessionFrame frame; CKFile file(Context); CKObjectArray objects; objects.InsertRear(existing);
    Context->SetUserLoadCallback(Choose, &first);
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_CHECKDUPLICATES));
    EXPECT_EQ(1, objects.GetCount()); ExpectChoiceCleared();
    Context->SetUserLoadCallback(Choose, &second);
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_CHECKDUPLICATES));
    EXPECT_EQ(1, first.Calls); EXPECT_EQ(1, second.Calls); ASSERT_EQ(2, objects.GetCount());
    objects.Reset(); EXPECT_EQ(existing->GetID(), objects.GetDataId()); objects.Next();
    EXPECT_EQ(file.m_FileObjects[0].CreatedObject, objects.GetDataId());
    EXPECT_EQ(1, file.m_IndexByClassId[CKCID_BEOBJECT].Size()); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, ManagerPayloadAndProgressRepeatAcrossMappedAndMemoryLoads) {
    CKTestSessionManager *manager = Manager(); CKTestUIProbe ui{Context}; Context->SetInterfaceMode(TRUE, ObserveUI, &ui);
    CKTestSessionFrame frame(true); std::string input = Path("source.cmo"); Write(input, frame);
    CKFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(input.data(), &objects)); ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects));
    EXPECT_EQ(2, manager->PreLoads); EXPECT_EQ(2, manager->PostLoads); EXPECT_EQ(2, manager->DataLoads);
    ASSERT_EQ(2, ui.LoadCount); EXPECT_EQ(1, ui.Loaded[0]); EXPECT_EQ(1, ui.Loaded[1]);
    EXPECT_EQ(1, ui.Totals[0]); EXPECT_EQ(1, ui.Totals[1]); EXPECT_EQ(2, objects.GetCount());
    EXPECT_EQ(nullptr, file.m_Parser); EXPECT_EQ(nullptr, file.m_MappedFile);
    ASSERT_EQ(1, file.m_ManagersData.Size()); EXPECT_EQ(nullptr, file.m_ManagersData[0].data); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, ConsecutiveSaveSessionsResetObjectMasksFlagsAndOutputSummary) {
    CKTestSessionManager *manager = Manager(); CKObject *object = Existing(); ASSERT_NE(nullptr, object);
    std::string first = Path("first.cmo"), second = Path("second.cmo"); CKFile file(Context);
    ASSERT_EQ(CK_OK, file.StartSave(first.data(), 0x3811)); file.SaveObject(object); ASSERT_EQ(CK_OK, file.EndSave());
    EXPECT_EQ(0x3811u, manager->SaveFlags); EXPECT_EQ(1u, file.m_FileInfo.ObjectCount);
    ASSERT_EQ(CK_OK, file.StartSave(second.data(), 0x3822)); file.SaveObject(object); ASSERT_EQ(CK_OK, file.EndSave());
    EXPECT_EQ(0x3822u, manager->SaveFlags); EXPECT_EQ(2, manager->PreSaves); EXPECT_EQ(2, manager->PostSaves);
    EXPECT_EQ(2, manager->DataSaves); EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(0u, file.m_Flags);
    EXPECT_EQ(0, file.m_FileObjects.Size()); EXPECT_EQ(1u, file.m_FileInfo.ObjectCount);
    FILE *stream = std::fopen(second.c_str(), "rb"); ASSERT_NE(nullptr, stream); unsigned char bytes[64] = {};
    EXPECT_EQ(64u, std::fread(bytes, 1, sizeof(bytes), stream)); EXPECT_EQ(0, std::fclose(stream));
    EXPECT_EQ(1u, Get(bytes, 44)); EXPECT_EQ(object->GetID(), Get(bytes, 48));
}

TEST_F(CKFileSessionContractTest, LoadAndSaveUsesSourceWriteModeAndConversionFlagsDuringRealManagerSave) {
    CKTestSessionManager *manager = Manager(); CKTestSessionFrame frame(true, CKFILE_WHOLECOMPRESSED);
    std::string input = Path("source.cmo"), output = Path("converted.cmo"); Write(input, frame);
    CKFile file(Context); file.LoadAndSave(input.data(), output.data());
    EXPECT_EQ(1, manager->DataLoads); EXPECT_EQ(1, manager->DataSaves); EXPECT_EQ(0u, manager->SaveFlags);
    EXPECT_EQ(1, manager->AttributeSaving); EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
    EXPECT_EQ(CKFILE_WHOLECOMPRESSED, Context->GetFileWriteMode()); EXPECT_FALSE(Context->m_Saving);
    FILE *stream = std::fopen(output.c_str(), "rb"); ASSERT_NE(nullptr, stream); unsigned char bytes[64] = {};
    EXPECT_EQ(64u, std::fread(bytes, 1, sizeof(bytes), stream)); EXPECT_EQ(0, std::fclose(stream));
    EXPECT_EQ(8u, Get(bytes, 16)); EXPECT_EQ(unsigned(CKFILE_WHOLECOMPRESSED), Get(bytes, 24)); EXPECT_EQ(1u, Get(bytes, 44));
    CKObjectArray objects; ASSERT_EQ(CK_OK, file.Load(output.data(), &objects)); EXPECT_EQ(1, objects.GetCount());
    EXPECT_EQ(2, manager->DataLoads); ExpectChoiceCleared();
}

TEST_F(CKFileSessionContractTest, LoadAndSaveSamePathReleasesMappingBeforeReplacingTheInput) {
    CKTestSessionManager *manager = Manager(); CKTestSessionFrame frame(true); std::string path = Path("in-place.cmo"); Write(path, frame);
    CKFile file(Context); file.LoadAndSave(path.data(), path.data());
    EXPECT_EQ(1, manager->DataSaves); EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(nullptr, file.m_Parser); ASSERT_EQ(CK_OK, file.Load(path.data(), nullptr)); EXPECT_EQ(2, manager->DataLoads);
    EXPECT_EQ(1u, file.m_FileInfo.ObjectCount); EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
}

TEST_F(CKFileSessionContractTest, ConsecutiveConversionsDoNotLeakPreviousWriteModesOrObjectQueues) {
    CKTestSessionManager *manager = Manager(); CKTestSessionFrame plain(true), packed(true, CKFILE_WHOLECOMPRESSED);
    std::string source1 = Path("plain.cmo"), source2 = Path("packed.cmo"), output1 = Path("out1.cmo"), output2 = Path("out2.cmo");
    Write(source1, plain); Write(source2, packed); CKFile file(Context);
    file.LoadAndSave(source2.data(), output2.data()); file.LoadAndSave(source1.data(), output1.data());
    EXPECT_EQ(2, manager->DataLoads); EXPECT_EQ(2, manager->DataSaves); EXPECT_EQ(2, manager->PreSaves); EXPECT_EQ(2, manager->PostSaves);
    EXPECT_EQ(CKFILE_UNCOMPRESSED, Context->GetFileWriteMode()); EXPECT_EQ(1u, file.m_FileInfo.ObjectCount);
    EXPECT_EQ(0, file.m_FileObjects.Size()); EXPECT_EQ(0, file.m_ObjectsHashTable.Size()); EXPECT_EQ(0u, file.m_Flags);
    CKObjectArray objects; ASSERT_EQ(CK_OK, file.Load(output1.data(), &objects)); EXPECT_EQ(1, objects.GetCount());
    EXPECT_EQ(3, manager->DataLoads); EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving); ExpectChoiceCleared();
}
