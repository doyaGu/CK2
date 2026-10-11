#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "CKAll.h"

namespace {
const CKGUID ProbeGuid(0x31415926, 0x27182818);

class SaveFile : public CKFile {
public:
    explicit SaveFile(CKContext *context) : CKFile(context) {}
    int Index(CK_ID id) { return SaveFindObjectIndex(id); }
    void Finalize() { FinalizeSaveState(); }
    void Stats(int size) { WriteStats(size); }
};

class SaveManager : public CKBaseManager {
public:
    explicit SaveManager(CKContext *context)
        : CKBaseManager(context, ProbeGuid, const_cast<char *>("Save fixture")),
          Pre(0), Post(0), DataCalls(0), PreSaving(FALSE), PostSaving(FALSE),
          DataSaving(FALSE), EmitData(TRUE) {}
    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_PreSave | CKMANAGER_FUNC_PostSave;
    }
    CKERROR PreSave() override { ++Pre; PreSaving = m_Context->m_Saving; return CK_OK; }
    CKERROR PostSave() override { ++Post; PostSaving = m_Context->m_Saving; return CK_OK; }
    CKStateChunk *SaveData(CKFile *file) override {
        ++DataCalls; DataSaving = m_Context->m_Saving;
        if (!EmitData) return nullptr;
        CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, file);
        chunk->StartWrite(); chunk->WriteInt(0x12345678); chunk->CloseChunk();
        return chunk;
    }
    int Pre, Post, DataCalls;
    CKBOOL PreSaving, PostSaving, DataSaving, EmitData;
};

CKDWORD Word(const unsigned char *bytes, int offset) {
    CKDWORD value = 0;
    for (int i = 0; i < 4; ++i) value |= CKDWORD(bytes[offset + i]) << (8 * i);
    return value;
}

CKDWORD Adler(const unsigned char *bytes, int size) {
    unsigned int a = 0, b = 0;
    for (int i = 0; i < size; ++i) {
        const unsigned char value = (i >= 8 && i < 12) ? 0 : bytes[i];
        a = (a + value) % 65521u; b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

struct InterfaceProbe {
    int CreateCalls = 0, ScriptCount = 0, ProgressCalls = 0;
    CK_ID Scripts[4] = {};
    int Loaded[8] = {}, Total[8] = {};
};

CKERROR InterfaceCallback(CKUICallbackStruct &callback, void *data) {
    InterfaceProbe *probe = static_cast<InterfaceProbe *>(data);
    if (callback.Reason == CKUIM_CREATEINTERFACECHUNK) {
        ++probe->CreateCalls;
        probe->ScriptCount = callback.Objects ? callback.Objects->Size() : -1;
        for (int i = 0; i < probe->ScriptCount && i < 4; ++i) probe->Scripts[i] = (*callback.Objects)[i];
    } else if (callback.Reason == CKUIM_LOADSAVEPROGRESS && probe->ProgressCalls < 8) {
        probe->Loaded[probe->ProgressCalls] = callback.NbObjectsLoaded;
        probe->Total[probe->ProgressCalls++] = callback.NbObjectsToLoad;
    }
    return CK_OK;
}

class CKFileSaveLifecycleTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(CK_OK, CKStartUp()); }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        Context->SetFileWriteMode(CKFILE_UNCOMPRESSED);
        Manager = new SaveManager(Context);
        ASSERT_EQ(CK_OK, Context->RegisterNewManager(Manager));
        Directory = std::filesystem::path(::testing::TempDir()) /
            (std::string("CKFileSaveLifecycle-") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(Directory);
        std::filesystem::create_directories(Directory);
        Output = (Directory / "saved.cmo").string();
    }
    void TearDown() override {
        if (Context) CKCloseContext(Context);
        Context = nullptr;
        std::filesystem::remove_all(Directory);
    }
    CKObject *Object(const char *name = "object") {
        return Context->CreateObject(CKCID_OBJECT, const_cast<char *>(name));
    }
    void Start(SaveFile &file) { ASSERT_EQ(CK_OK, file.StartSave(const_cast<char *>(Output.c_str()), 0x3141)); }
    int Read(unsigned char *bytes, int capacity) {
        FILE *stream = std::fopen(Output.c_str(), "rb");
        if (!stream) return -1;
        const int result = static_cast<int>(std::fread(bytes, 1, capacity, stream));
        std::fclose(stream); return result;
    }
    void ExpectFinal(const SaveFile &file) {
        EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(1, Manager->Post);
        EXPECT_TRUE(Manager->PostSaving);
        EXPECT_EQ(nullptr, file.m_FileName); EXPECT_EQ(0, file.m_FileObjects.Size());
        EXPECT_EQ(0, file.m_ManagersData.Size()); EXPECT_EQ(0, file.m_IncludedFiles.Size());
        EXPECT_EQ(0u, file.m_Flags); EXPECT_EQ(0, file.m_ObjectsHashTable.Size());
    }
    CKContext *Context = nullptr;
    SaveManager *Manager = nullptr;
    std::filesystem::path Directory;
    std::string Output;
};

TEST_F(CKFileSaveLifecycleTest, StartSavePreservesExistingBytesUntilEndAndBeginsManagerSession) {
    FILE *stream = std::fopen(Output.c_str(), "wb"); ASSERT_NE(nullptr, stream);
    const unsigned char old[] = {1, 2, 3, 4}; ASSERT_EQ(4u, std::fwrite(old, 1, 4, stream)); std::fclose(stream);
    SaveFile file(Context); Start(file);
    EXPECT_TRUE(Context->m_Saving); EXPECT_EQ(1, Manager->Pre); EXPECT_FALSE(Manager->PreSaving);
    EXPECT_EQ(0, Manager->Post); EXPECT_EQ(0x3141u, file.m_Flags);
    unsigned char actual[16]; ASSERT_EQ(4, Read(actual, 16)); EXPECT_EQ(0, std::memcmp(old, actual, 4));
    EXPECT_EQ(CK_OK, file.EndSave()); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, NullFilenameBalancesFailedSessionWithoutWritingData) {
    SaveFile file(Context); EXPECT_EQ(CKERR_INVALIDFILE, file.StartSave(nullptr));
    EXPECT_EQ(1, Manager->Pre); EXPECT_EQ(1, Manager->Post); EXPECT_TRUE(Manager->PostSaving);
    EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(0, Manager->DataCalls); EXPECT_EQ(nullptr, file.m_FileName);
}

TEST_F(CKFileSaveLifecycleTest, UnwritableStartBalancesSessionAndCanBeRetried) {
    SaveFile file(Context); std::string missing = (Directory / "missing" / "saved.cmo").string();
    EXPECT_EQ(CKERR_CANTWRITETOFILE, file.StartSave(const_cast<char *>(missing.c_str())));
    EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(1, Manager->Post);
    ASSERT_EQ(CK_OK, file.StartSave(const_cast<char *>(Output.c_str()))); EXPECT_TRUE(Context->m_Saving);
    EXPECT_EQ(CK_OK, file.EndSave()); EXPECT_EQ(2, Manager->Pre); EXPECT_EQ(2, Manager->Post);
}

TEST_F(CKFileSaveLifecycleTest, ObjectArrayResetsCursorSkipsUnknownIdsAndPropagatesFlags) {
    SaveFile file(Context); Start(file); CKObject *a = Object("A"), *b = Object("B");
    ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    CKObjectArray objects; objects.InsertRear(a); objects.InsertRear(CK_ID(0)); objects.InsertRear(b); objects.Next();
    file.SaveObjects(&objects, 0x123); EXPECT_TRUE(objects.EndOfList());
    ASSERT_EQ(2, file.m_FileObjects.Size()); EXPECT_EQ(a->GetID(), file.m_FileObjects[0].Object);
    EXPECT_EQ(b->GetID(), file.m_FileObjects[1].Object); EXPECT_EQ(0x123u, file.m_FileObjects[1].SaveFlags);
    EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, NullObjectArrayAndNonpositiveRawArrayCountsAreNoOps) {
    SaveFile file(Context); Start(file);
    file.SaveObjects(static_cast<CKObjectArray *>(nullptr));
    file.SaveObjects(static_cast<CK_ID *>(nullptr), 0); file.SaveObjects(static_cast<CK_ID *>(nullptr), -1);
    file.SaveObjects(static_cast<CKObject **>(nullptr), 0); file.SaveObjects(static_cast<CKObject **>(nullptr), -1);
    EXPECT_EQ(0, file.m_FileObjects.Size()); EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, IdArrayPreservesOrderSkipsMissingAndDeduplicates) {
    SaveFile file(Context); Start(file); CKObject *a = Object("A"), *b = Object("B");
    ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b); CK_ID ids[] = {b->GetID(), 0, a->GetID(), b->GetID()};
    file.SaveObjects(ids, 4, 0x456); ASSERT_EQ(2, file.m_FileObjects.Size());
    EXPECT_EQ(b->GetID(), file.m_FileObjects[0].Object); EXPECT_EQ(a->GetID(), file.m_FileObjects[1].Object);
    EXPECT_EQ(0x456u, file.m_FileObjects[0].SaveFlags); EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, PointerArraySkipsNullDynamicAndNotSavedObjects) {
    SaveFile file(Context); Start(file); CKObject *a = Object("A"), *dynamic = Object("dynamic"), *hidden = Object("hidden");
    ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, dynamic); ASSERT_NE(nullptr, hidden);
    Context->ChangeObjectDynamic(dynamic, TRUE); hidden->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED, 0);
    CKObject *objects[] = {nullptr, dynamic, hidden, a, a}; file.SaveObjects(objects, 5, 0x789);
    ASSERT_EQ(1, file.m_FileObjects.Size()); EXPECT_EQ(a->GetID(), file.m_FileObjects[0].Object);
    EXPECT_EQ(0x789u, file.m_FileObjects[0].SaveFlags); EXPECT_FALSE(file.IsObjectToBeSaved(dynamic->GetID()));
    EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, ReferencePromotionKeepsIndexAndUsesFullSaveFlags) {
    SaveFile file(Context); Start(file); CKObject *a = Object("A"), *b = Object("B");
    ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    EXPECT_EQ(-1, file.Index(a->GetID())); EXPECT_FALSE(file.IsObjectToBeSaved(a->GetID()));
    file.SaveObjectAsReference(a); file.SaveObject(b, 0x44); file.SaveObjectAsReference(a);
    EXPECT_EQ(0, file.Index(a->GetID())); EXPECT_EQ(1, file.Index(b->GetID())); EXPECT_TRUE(file.IsObjectToBeSaved(a->GetID()));
    file.SaveObject(a, 0x55); ASSERT_EQ(2, file.m_FileObjects.Size()); EXPECT_EQ(0, file.Index(a->GetID()));
    EXPECT_EQ(0x55u, file.m_FileObjects[0].SaveFlags); EXPECT_EQ(0, file.m_ReferencedObjects.Size());
    EXPECT_FALSE(file.m_AlreadyReferencedMask.IsSet(a->GetID())); EXPECT_TRUE(file.m_AlreadySavedMask.IsSet(a->GetID()));
    CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, &file); chunk->StartWrite();
    chunk->WriteObjectID(b->GetID()); chunk->CloseChunk(); chunk->StartRead(); EXPECT_EQ(1, chunk->ReadInt());
    DeleteCKStateChunk(chunk); EXPECT_EQ(CK_OK, file.EndSave()); EXPECT_EQ(-1, file.Index(a->GetID()));
}

TEST_F(CKFileSaveLifecycleTest, ReferenceOnlyObjectIsSignedOnDiskAndTemporaryFlagIsCleared) {
    SaveFile file(Context); Start(file); CKObject *object = Object("reference"); ASSERT_NE(nullptr, object);
    const CK_ID id = object->GetID(); file.SaveObjectAsReference(object); ASSERT_EQ(CK_OK, file.EndSave());
    unsigned char bytes[16384]; const int size = Read(bytes, sizeof(bytes)); ASSERT_GE(size, 80);
    EXPECT_EQ(id | 0x80000000u, Word(bytes, 64)); EXPECT_EQ(CKCID_OBJECT, Word(bytes, 68));
    EXPECT_EQ(9u, Word(bytes, 76)); EXPECT_EQ(0, std::memcmp(bytes + 80, "reference", 9));
    EXPECT_EQ(0u, object->GetObjectFlags() & CK_OBJECT_ONLYFORFILEREFERENCE);
    EXPECT_EQ(1u, Word(bytes, 44)); EXPECT_EQ(id, Word(bytes, 48));
    EXPECT_EQ(Adler(bytes, 64 + Word(bytes, 28) + Word(bytes, 32)), Word(bytes, 8));
}

TEST_F(CKFileSaveLifecycleTest, SavedObjectsHaveIndependentHeaderOffsetsAndContiguousChunks) {
    SaveFile file(Context); Start(file); CKObject *a = Object("A"), *b = Object("LongName"); ASSERT_NE(nullptr, a); ASSERT_NE(nullptr, b);
    file.SaveObject(a); file.SaveObject(b); ASSERT_EQ(CK_OK, file.EndSave());
    unsigned char bytes[16384]; int size = Read(bytes, sizeof(bytes)); ASSERT_GE(size, 105);
    EXPECT_EQ(0, std::memcmp(bytes, "Nemo Fi", 7)); EXPECT_EQ(8u, Word(bytes, 16)); EXPECT_EQ(0u, Word(bytes, 24));
    EXPECT_EQ(2u, Word(bytes, 44)); EXPECT_EQ(a->GetID(), Word(bytes, 64)); EXPECT_EQ(1u, Word(bytes, 76));
    EXPECT_EQ('A', bytes[80]); EXPECT_EQ(b->GetID(), Word(bytes, 81)); EXPECT_EQ(8u, Word(bytes, 93));
    EXPECT_EQ(0, std::memcmp(bytes + 97, "LongName", 8));
    const unsigned int first = Word(bytes, 72), second = Word(bytes, 89);
    ASSERT_LT(first + 4, static_cast<unsigned int>(size)); ASSERT_LT(second + 4, static_cast<unsigned int>(size));
    EXPECT_EQ(first + 4 + Word(bytes, first), second); EXPECT_GT(Word(bytes, first), 0u);
    EXPECT_EQ(Adler(bytes, 64 + Word(bytes, 28) + Word(bytes, 32)), Word(bytes, 8));
    EXPECT_EQ(static_cast<CKDWORD>(size), file.m_FileInfo.FileSize); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, SavedManagerRecordsAreContiguousAndCarryExactIndependentPayload) {
    SaveFile file(Context); Start(file); ASSERT_EQ(CK_OK, file.EndSave());
    EXPECT_EQ(1, Manager->DataCalls); EXPECT_TRUE(Manager->DataSaving);
    unsigned char bytes[16384]; int size = Read(bytes, sizeof(bytes)); ASSERT_GE(size, 76);
    unsigned int cursor = 64 + Word(bytes, 28); bool found = false;
    for (unsigned int i = 0; i < Word(bytes, 40); ++i) {
        ASSERT_LE(cursor + 12, static_cast<unsigned int>(size));
        const CKDWORD count = Word(bytes, cursor + 8); ASSERT_LE(cursor + 12 + count, static_cast<unsigned int>(size));
        if (Word(bytes, cursor) == ProbeGuid.d1 && Word(bytes, cursor + 4) == ProbeGuid.d2) {
            found = true; ASSERT_EQ(12u, count); EXPECT_EQ(1u, Word(bytes, cursor + 16));
            EXPECT_EQ(0x12345678u, Word(bytes, cursor + 20));
        }
        cursor += 12 + count;
    }
    EXPECT_TRUE(found); EXPECT_EQ(64 + Word(bytes, 28) + Word(bytes, 32), cursor); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, InterfaceCallbackReceivesOnlyScriptsWithoutExistingInterfaceChunks) {
    InterfaceProbe probe; Context->SetInterfaceMode(TRUE, InterfaceCallback, &probe);
    SaveFile file(Context); Start(file);
    CKBehavior *needs = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR, const_cast<char *>("needs")));
    CKBehavior *has = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR, const_cast<char *>("has")));
    CKBehavior *ordinary = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR, const_cast<char *>("ordinary")));
    ASSERT_NE(nullptr, needs); ASSERT_NE(nullptr, has); ASSERT_NE(nullptr, ordinary);
    needs->UseGraph(); needs->ModifyFlags(CKBEHAVIOR_SCRIPT, 0);
    has->UseGraph(); has->ModifyFlags(CKBEHAVIOR_SCRIPT, 0); ordinary->UseGraph();
    CKStateChunk *interfaceChunk = CreateCKStateChunk(CKCID_BEHAVIOR, nullptr);
    interfaceChunk->StartWrite(); interfaceChunk->WriteInt(73); interfaceChunk->CloseChunk();
    has->SetInterfaceChunk(interfaceChunk);
    file.SaveObject(needs); file.SaveObject(has); file.SaveObject(ordinary);
    ASSERT_EQ(CK_OK, file.EndSave()); EXPECT_EQ(1, probe.CreateCalls); EXPECT_EQ(1, probe.ScriptCount);
    EXPECT_EQ(needs->GetID(), probe.Scripts[0]); EXPECT_EQ(3, probe.ProgressCalls);
    for (int i = 0; i < 3; ++i) { EXPECT_EQ(i, probe.Loaded[i]); EXPECT_EQ(3, probe.Total[i]); }
    EXPECT_EQ(interfaceChunk, has->GetInterfaceChunk()); ExpectFinal(file);
    Context->SetInterfaceMode(FALSE, nullptr, nullptr);
}

TEST_F(CKFileSaveLifecycleTest, EmptySaveStillOffersEmptyScriptListWithoutProgressEvents) {
    InterfaceProbe probe; Context->SetInterfaceMode(TRUE, InterfaceCallback, &probe);
    SaveFile file(Context); Start(file); ASSERT_EQ(CK_OK, file.EndSave());
    EXPECT_EQ(1, probe.CreateCalls); EXPECT_EQ(0, probe.ScriptCount); EXPECT_EQ(0, probe.ProgressCalls);
    Context->SetInterfaceMode(FALSE, nullptr, nullptr);
}

TEST_F(CKFileSaveLifecycleTest, EmptyObjectsAndOptionalNullManagerDataProduceValidEmptyFile) {
    SaveFile file(Context); Start(file); Manager->EmitData = FALSE;
    // All built-in managers remain alive. Limit the save registry to a valid
    // optional-data manager to exercise the documented SaveData()==nullptr path.
    XManagerHashTable original = Context->m_ManagerTable;
    Context->m_ManagerTable.Clear(); Context->m_ManagerTable.Insert(ProbeGuid, Manager);
    CKERROR result = file.EndSave(); Context->m_ManagerTable = original;
    EXPECT_EQ(CK_OK, result); EXPECT_EQ(1, Manager->DataCalls);
    unsigned char bytes[128]; ASSERT_EQ(76, Read(bytes, sizeof(bytes)));
    EXPECT_EQ(0u, Word(bytes, 32)); EXPECT_EQ(0u, Word(bytes, 36)); EXPECT_EQ(0u, Word(bytes, 40));
    EXPECT_EQ(0u, Word(bytes, 44)); EXPECT_EQ(12u, Word(bytes, 28));
    EXPECT_EQ(0u, Word(bytes, 64)); EXPECT_EQ(4u, Word(bytes, 68)); EXPECT_EQ(0u, Word(bytes, 72));
    EXPECT_EQ(Adler(bytes, 76), Word(bytes, 8)); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, FailedFinalOpenReleasesSaveStateAndBalancesCallbacks) {
    SaveFile file(Context); Start(file); CKObject *object = Object(); ASSERT_NE(nullptr, object); file.SaveObject(object);
    ASSERT_TRUE(std::filesystem::remove(Output)); ASSERT_TRUE(std::filesystem::create_directory(Output));
    EXPECT_EQ(CKERR_CANTWRITETOFILE, file.EndSave()); EXPECT_EQ(1, Manager->DataCalls); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, FinalizeClearsSessionAndRestoresLoadCallbacks) {
    SaveFile file(Context); Start(file); CKObject *object = Object(); ASSERT_NE(nullptr, object); file.SaveObject(object);
    Context->m_GeneralLoadMode = CKLOAD_REPLACE; Context->m_MeshLoadMode = CKLOAD_REPLACE;
    Context->m_UserLoadCallBackArgs = &file;
    file.Finalize(); ExpectFinal(file); EXPECT_EQ(CKLOAD_INVALID, Context->m_GeneralLoadMode);
    EXPECT_EQ(CKLOAD_INVALID, Context->m_MeshLoadMode); EXPECT_EQ(nullptr, Context->m_UserLoadCallBack);
    EXPECT_EQ(nullptr, Context->m_UserLoadCallBackArgs); EXPECT_EQ(0, Manager->DataCalls);
}

TEST_F(CKFileSaveLifecycleTest, MissingPluginsReturnsBorrowedMutableListAndRestartClearsIt) {
    SaveFile file(Context); Start(file); auto *list = file.GetMissingPlugins(); ASSERT_NE(nullptr, list);
    EXPECT_EQ(&file.m_PluginsDep, list); CKFilePluginDependencies dependency;
    dependency.m_PluginCategory = CKPLUGIN_BITMAP_READER; dependency.m_Guids.PushBack(ProbeGuid); list->PushBack(dependency);
    EXPECT_EQ(1, file.GetMissingPlugins()->Size()); file.Finalize(); EXPECT_EQ(0, list->Size());
    ASSERT_EQ(CK_OK, file.StartSave(const_cast<char *>(Output.c_str()))); EXPECT_EQ(list, file.GetMissingPlugins());
    EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, WriteStatsIsNoOpForZeroAndNonzeroInterfaceSizes) {
    SaveFile file(Context); Start(file); CKObject *object = Object(); ASSERT_NE(nullptr, object); file.SaveObject(object);
    file.Stats(0); file.Stats(17); file.Stats(0x7fffffff);
    EXPECT_TRUE(Context->m_Saving); EXPECT_EQ(1, file.m_FileObjects.Size()); EXPECT_EQ(0, Manager->DataCalls);
    EXPECT_EQ(0, Manager->Post); EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, IncludeRejectsEmptyMissingResolvedAndInvalidCategoryPaths) {
    SaveFile file(Context); Start(file);
    EXPECT_FALSE(file.IncludeFile(nullptr)); EXPECT_FALSE(file.IncludeFile(const_cast<char *>("")));
    EXPECT_FALSE(file.IncludeFile(const_cast<char *>("missing-fixture.bin"), DATA_PATH_IDX));
    EXPECT_FALSE(file.IncludeFile(const_cast<char *>("missing-fixture.bin"), 99999));
    EXPECT_EQ(0, file.m_IncludedFiles.Size()); EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, IncludeDeduplicatesCaseInsensitivelyAndKeepsOriginalSpelling) {
    SaveFile file(Context); Start(file);
    EXPECT_TRUE(file.IncludeFile(const_cast<char *>("Asset.bin"), -1));
    EXPECT_FALSE(file.IncludeFile(const_cast<char *>("asset.bin"), -1));
    EXPECT_FALSE(file.IncludeFile(const_cast<char *>("Asset.bin"), -1));
    ASSERT_EQ(1, file.m_IncludedFiles.Size()); EXPECT_STREQ("Asset.bin", file.m_IncludedFiles[0].Str());
    EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, IncludeResolvesSearchCategoryBeforeDeduplicating) {
    SaveFile file(Context); Start(file); std::string asset = (Directory / "asset.bin").string();
    FILE *stream = std::fopen(asset.c_str(), "wb"); ASSERT_NE(nullptr, stream); std::fputc(42, stream); std::fclose(stream);
    XString path(Directory.string().c_str()); ASSERT_GE(Context->GetPathManager()->AddPath(DATA_PATH_IDX, path), 0);
    ASSERT_TRUE(file.IncludeFile(const_cast<char *>("asset.bin"), DATA_PATH_IDX));
    EXPECT_FALSE(file.IncludeFile(const_cast<char *>("asset.bin"), DATA_PATH_IDX));
    ASSERT_EQ(1, file.m_IncludedFiles.Size()); EXPECT_EQ(CK_OK, file.EndSave());
}

TEST_F(CKFileSaveLifecycleTest, IncludedPayloadUsesBasenameAndExactBytesOutsideCoveredCRCSections) {
    SaveFile file(Context); Start(file); std::string asset = (Directory / "Payload.bin").string();
    const unsigned char payload[] = {0, 0xff, 0x31, 0x7f, 0x80};
    FILE *stream = std::fopen(asset.c_str(), "wb"); ASSERT_NE(nullptr, stream);
    ASSERT_EQ(sizeof(payload), std::fwrite(payload, 1, sizeof(payload), stream)); std::fclose(stream);
    ASSERT_TRUE(file.IncludeFile(const_cast<char *>(asset.c_str()), -1)); ASSERT_EQ(CK_OK, file.EndSave());
    unsigned char bytes[16384]; const int size = Read(bytes, sizeof(bytes)); ASSERT_GT(size, 76);
    const unsigned int end = 64 + Word(bytes, 28) + Word(bytes, 32);
    ASSERT_EQ(end + 4 + 11 + 4 + sizeof(payload), static_cast<unsigned int>(size));
    EXPECT_EQ(1u, Word(bytes, 72)); EXPECT_EQ(11u, Word(bytes, end));
    EXPECT_EQ(0, std::memcmp(bytes + end + 4, "Payload.bin", 11)); EXPECT_EQ(sizeof(payload), Word(bytes, end + 15));
    EXPECT_EQ(0, std::memcmp(bytes + end + 19, payload, sizeof(payload)));
    EXPECT_EQ(Adler(bytes, end), Word(bytes, 8)); EXPECT_EQ(end, file.m_FileInfo.FileSize); ExpectFinal(file);
}

TEST_F(CKFileSaveLifecycleTest, MissingDirectIncludeWritesZeroSizeStubWithValidTrailerCount) {
    SaveFile file(Context); Start(file); std::string missing = (Directory / "missing.bin").string();
    ASSERT_TRUE(file.IncludeFile(const_cast<char *>(missing.c_str()), -1)); ASSERT_EQ(CK_OK, file.EndSave());
    unsigned char bytes[16384]; const int size = Read(bytes, sizeof(bytes)); ASSERT_GT(size, 76);
    const unsigned int end = 64 + Word(bytes, 28) + Word(bytes, 32);
    EXPECT_EQ(end + 4 + 11 + 4, static_cast<unsigned int>(size)); EXPECT_EQ(1u, Word(bytes, 72));
    EXPECT_EQ(11u, Word(bytes, end)); EXPECT_EQ(0, std::memcmp(bytes + end + 4, "missing.bin", 11));
    EXPECT_EQ(0u, Word(bytes, end + 15));
}
} // namespace
