#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"

static void Word(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

static unsigned int Adler(const unsigned char *bytes, int size) {
    unsigned int a = 0, b = 0;
    for (int i = 0; i < size; ++i) { a = (a + bytes[i]) % 65521; b = (b + a) % 65521; }
    return a | (b << 16);
}

// The input is an independently authored format frame, never CKFile::EndSave output.
struct CKTestFrame {
    unsigned char Bytes[2048];
    int HeaderSize, DataSize, Size;
    explicit CKTestFrame(bool object = false) : HeaderSize(object ? 34 : 8), DataSize(object ? 12 : 0), Size(0) {
        std::memset(Bytes, 0, sizeof(Bytes)); std::memcpy(Bytes, "Nemo Fi", 7);
        Word(Bytes, 12, 0x13022002); Word(Bytes, 16, 8);
        Word(Bytes, 44, object ? 1 : 0); Word(Bytes, 48, 41);
        Word(Bytes, 56, 0x01010000);
        if (object) {
            Word(Bytes, 64, 41); Word(Bytes, 68, CKCID_BEOBJECT); Word(Bytes, 72, 0);
            Word(Bytes, 76, 10); std::memcpy(Bytes + 80, "LiteralObj", 10);
            Word(Bytes, 98, 8); Word(Bytes, 102, (7u << 16) | (CKCID_BEOBJECT << 8) | 9u);
        }
        Refresh();
    }
    void Refresh() {
        Size = 64 + HeaderSize + DataSize;
        Word(Bytes, 28, HeaderSize); Word(Bytes, 60, HeaderSize);
        Word(Bytes, 32, DataSize); Word(Bytes, 36, DataSize);
        Word(Bytes, 8, 0); Word(Bytes, 8, Adler(Bytes, Size));
    }
};

class CKTestLoadFile : public CKFile {
public:
    explicit CKTestLoadFile(CKContext *context) : CKFile(context) {}
    void Reset() { ClearData(); }
    CKERROR Data() { return ReadFileData(&m_Parser); }
    CKObject *Reference(CKFileObject &object) { return ResolveReference(&object); }
    CK_ID Lookup(int index) { return LoadFindObject(index); }
    void Finish(CKObjectArray *objects, CKDWORD flags) { FinishLoading(objects, flags); }
    CKERROR Cleanup(CKBufferParser *other, CKERROR error) { return ReturnWithParserCleanup(other,&m_Parser,error); }
};

static CK_LOADMODE LoadCallback(CK_CLASSID, CKSTRING, CKSTRING, CKObject **, void *) { return CKLOAD_RENAME; }

class CKFileLoadLifecycleTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    std::filesystem::path Path;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        ASSERT_NE(nullptr, Context);
        char name[80]; std::snprintf(name, sizeof(name), "ckfile-pass35-%p.cmo", static_cast<void *>(this));
        Path = std::filesystem::temp_directory_path() / name;
    }
    void TearDown() override {
        std::error_code ignored; std::filesystem::remove(Path, ignored);
        if (Context) CKCloseContext(Context); CKShutdown();
    }
    void Write(const CKTestFrame &frame) {
        FILE *file = std::fopen(Path.string().c_str(), "wb"); ASSERT_NE(nullptr, file);
        EXPECT_EQ(static_cast<size_t>(frame.Size), std::fwrite(frame.Bytes, 1, frame.Size, file)); std::fclose(file);
    }
    void ExpectModesReset() {
        EXPECT_EQ(CKLOAD_INVALID, Context->m_GeneralLoadMode); EXPECT_EQ(CKLOAD_INVALID, Context->m_3DObjectsLoadMode);
        EXPECT_EQ(CKLOAD_INVALID, Context->m_MeshLoadMode); EXPECT_EQ(CKLOAD_INVALID, Context->m_MatTexturesLoadMode);
        EXPECT_EQ(nullptr, Context->m_UserLoadCallBack); EXPECT_EQ(nullptr, Context->m_UserLoadCallBackArgs);
    }
};

TEST_F(CKFileLoadLifecycleTest, ConstructorStartsAnEmptyNonOwningSession) {
    CKTestLoadFile file(Context); EXPECT_EQ(Context, file.m_Context); EXPECT_EQ(nullptr, file.m_Parser);
    EXPECT_EQ(nullptr, file.m_MappedFile); EXPECT_EQ(nullptr, file.m_FileName); EXPECT_EQ(0u, file.m_SaveIDMax);
    EXPECT_EQ(FALSE, file.m_ReadFileDataDone); EXPECT_EQ(FALSE, file.m_SceneSaved);
    EXPECT_TRUE(file.m_FileObjects.IsEmpty()); EXPECT_TRUE(file.m_ManagersData.IsEmpty());
}

TEST_F(CKFileLoadLifecycleTest, DestructorResetsContextModesAndBorrowedCallbacks) {
    Context->SetAutomaticLoadMode(CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME);
    Context->SetUserLoadCallback(LoadCallback, this);
    { CKTestLoadFile file(Context); }
    ExpectModesReset(); EXPECT_FALSE(Context->m_InLoad);
}

TEST_F(CKFileLoadLifecycleTest, ClearDataReleasesSessionRecordsAndKeepsContextObjectsAlive) {
    CKObject object(Context, "borrowed"); const CK_ID id = object.GetID();
    CKTestLoadFile file(Context); file.m_FileObjects.Resize(1); file.m_FileObjects[0].ObjPtr = &object;
    file.m_FileObjects[0].Name = CKStrdup("owned"); file.m_FileObjects[0].Data = CreateCKStateChunk(CKCID_OBJECT, &file);
    file.m_ManagersData.Resize(1); file.m_ManagersData[0].data = CreateCKStateChunk(CKCID_OBJECT, &file);
    file.m_IncludedFiles.PushBack(XString("owned.bin")); file.m_SceneSaved = TRUE; file.m_SaveIDMax = 77;
    file.m_ReadFileDataDone = TRUE; file.m_Flags = 123; file.Reset(); file.Reset();
    EXPECT_TRUE(file.m_FileObjects.IsEmpty()); EXPECT_TRUE(file.m_ManagersData.IsEmpty());
    EXPECT_EQ(0, file.m_IncludedFiles.Size()); EXPECT_EQ(&object, Context->GetObject(id));
    EXPECT_EQ(0u, file.m_SaveIDMax); EXPECT_EQ(0u, file.m_Flags); EXPECT_FALSE(file.m_ReadFileDataDone);
}

TEST_F(CKFileLoadLifecycleTest, NullAndShortMemoryFramesReturnExactErrorsWithoutOpening) {
    CKTestLoadFile file(Context); unsigned char bytes[32] = {};
    EXPECT_EQ(CKERR_INVALIDPARAMETER, file.OpenMemory(nullptr, 0));
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(bytes, 31));
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(bytes, 32));
    EXPECT_EQ(nullptr, file.m_Parser);
}

TEST_F(CKFileLoadLifecycleTest, UnsupportedVersionReportsObsoleteRuntime) {
    CKTestFrame frame; Word(frame.Bytes, 16, 10); frame.Refresh(); CKTestLoadFile file(Context);
    EXPECT_EQ(CKERR_OBSOLETEVIRTOOLS, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileLoadLifecycleTest, GlobalVersionAndModeFollowTheLastSuccessfulIndependentHeader) {
    CKTestFrame first,second; Word(second.Bytes,24,4); second.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK,file.OpenMemory(first.Bytes,first.Size)); EXPECT_EQ(8u,GetCurrentFileVersion());
    EXPECT_EQ(0u,GetCurrentFileLoadOption()); ASSERT_EQ(CK_OK,file.OpenMemory(second.Bytes,second.Size));
    EXPECT_EQ(8u,GetCurrentFileVersion()); EXPECT_EQ(4u,GetCurrentFileLoadOption());
    EXPECT_EQ(4u,file.m_FileInfo.FileWriteMode); EXPECT_EQ(8u,file.m_FileInfo.FileVersion);
}

TEST_F(CKFileLoadLifecycleTest, FailedCrcAndUnsupportedVersionLeavePublishedLoadGlobalsUnchanged) {
    CKTestFrame good,bad,obsolete; Word(good.Bytes,24,4); good.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK,file.OpenMemory(good.Bytes,good.Size)); bad.Bytes[56]^=1;
    EXPECT_EQ(CKERR_FILECRCERROR,file.OpenMemory(bad.Bytes,bad.Size));
    EXPECT_EQ(8u,GetCurrentFileVersion()); EXPECT_EQ(4u,GetCurrentFileLoadOption());
    Word(obsolete.Bytes,16,10); obsolete.Refresh(); EXPECT_EQ(CKERR_OBSOLETEVIRTOOLS,file.OpenMemory(obsolete.Bytes,obsolete.Size));
    EXPECT_EQ(8u,GetCurrentFileVersion()); EXPECT_EQ(4u,GetCurrentFileLoadOption());
}

TEST_F(CKFileLoadLifecycleTest, ParserCleanupPreservesTheRootParserAndExactError) {
    CKTestFrame frame; CKTestLoadFile file(Context); ASSERT_EQ(CK_OK,file.OpenMemory(frame.Bytes,frame.Size));
    CKBufferParser *root=file.m_Parser; EXPECT_EQ(CKERR_PLUGINSMISSING,file.Cleanup(root,CKERR_PLUGINSMISSING));
    EXPECT_EQ(root,file.m_Parser); EXPECT_EQ(CKERR_INVALIDFILE,file.Cleanup(nullptr,CKERR_INVALIDFILE));
    EXPECT_EQ(root,file.m_Parser); EXPECT_EQ(CK_OK,file.LoadFileData(nullptr));
}

TEST_F(CKFileLoadLifecycleTest, InvalidClassInPackedHeaderReleasesTemporaryParserAndKeepsRootForCleanup) {
    // Python zlib of six independently authored words: savedID41, invalidclass,
    // dataoffset0, namelength0, dependencycount0, includedsize0.
    const unsigned char packed[]={0x78,0x9c,0xd3,0x64,0x60,0x60,0xf8,0xff,0xff,0x7f,0x3d,0x03,0x1a,0x00,0x00,0x45,0x26,0x03,0xa6};
    CKTestFrame frame; frame.HeaderSize=sizeof(packed); Word(frame.Bytes,44,1);
    std::memcpy(frame.Bytes+64,packed,sizeof(packed)); frame.Refresh(); Word(frame.Bytes,60,24);
    Word(frame.Bytes,8,0); Word(frame.Bytes,8,Adler(frame.Bytes,frame.Size)); CKTestLoadFile file(Context);
    EXPECT_EQ(CKERR_INVALIDFILE,file.OpenMemory(frame.Bytes,frame.Size)); ASSERT_NE(nullptr,file.m_Parser);
    CKTestFrame good; EXPECT_EQ(CK_OK,file.OpenMemory(good.Bytes,good.Size)); EXPECT_EQ(CK_OK,file.LoadFileData(nullptr));
}

TEST_F(CKFileLoadLifecycleTest, EmptyLiteralMemoryFrameLoadsAndResetsCallbacks) {
    CKTestFrame frame; CKTestLoadFile file(Context); CKObjectArray objects;
    Context->SetAutomaticLoadMode(CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME, CKLOAD_RENAME);
    Context->SetUserLoadCallback(LoadCallback, this);
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects));
    EXPECT_EQ(0, objects.GetCount()); EXPECT_EQ(nullptr, file.m_Parser); EXPECT_FALSE(Context->m_InLoad);
    ExpectModesReset(); EXPECT_EQ(8u, GetCurrentFileVersion()); EXPECT_EQ(0u, GetCurrentFileLoadOption());
}

TEST_F(CKFileLoadLifecycleTest, OpenFileAndDirectLoadUseTheActualMappedLiteralFile) {
    CKTestFrame frame; Write(frame); CKTestLoadFile file(Context); CKObjectArray objects;
    std::string path = Path.string(); ASSERT_EQ(CK_OK, file.OpenFile(path.data()));
    ASSERT_NE(nullptr, file.m_MappedFile); ASSERT_NE(nullptr, file.m_Parser);
    EXPECT_STREQ(path.c_str(), Context->GetLastCmoLoaded()); ASSERT_EQ(CK_OK, file.LoadFileData(&objects));
    EXPECT_EQ(nullptr, file.m_MappedFile); EXPECT_EQ(nullptr, file.m_Parser);
    ASSERT_EQ(CK_OK, file.Load(path.data(), &objects)); EXPECT_EQ(0, objects.GetCount());
}

TEST_F(CKFileLoadLifecycleTest, OpenFileFailureLeavesNoMappedFileOrFilename) {
    CKTestLoadFile file(Context); EXPECT_EQ(CKERR_INVALIDPARAMETER, file.OpenFile(nullptr));
    std::string path = Path.string(); EXPECT_EQ(CKERR_INVALIDFILE, file.OpenFile(path.data()));
    EXPECT_EQ(nullptr, file.m_MappedFile); EXPECT_EQ(nullptr, file.m_FileName); EXPECT_EQ(nullptr, file.m_Parser);
}

TEST_F(CKFileLoadLifecycleTest, ReopeningMemoryClearsPreviousFileClassIndices) {
    CKTestFrame first(true), second; CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(first.Bytes, first.Size)); ASSERT_EQ(1, file.m_FileObjects.Size());
    ASSERT_EQ(CK_OK, file.OpenMemory(second.Bytes, second.Size));
    EXPECT_EQ(0, file.m_FileObjects.Size()); EXPECT_TRUE(file.m_IndexByClassId[CKCID_BEOBJECT].IsEmpty());
}

TEST_F(CKFileLoadLifecycleTest, LiteralObjectIsCreatedRegisteredReturnedAndMarkedDynamic) {
    CKTestFrame frame(true); CKTestLoadFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_AS_DYNAMIC_OBJECT));
    ASSERT_EQ(1, file.m_FileObjects.Size()); CKFileObject &record = file.m_FileObjects[0];
    ASSERT_NE(nullptr, record.ObjPtr); EXPECT_EQ(CKCID_BEOBJECT, record.ObjPtr->GetClassID());
    EXPECT_STREQ("LiteralObj", record.ObjPtr->GetName()); EXPECT_TRUE(record.ObjPtr->IsDynamic());
    EXPECT_EQ(record.ObjPtr, Context->GetObject(record.CreatedObject)); EXPECT_EQ(record.CreatedObject, file.Lookup(0));
    EXPECT_EQ(1, objects.GetCount()); EXPECT_EQ(record.CreatedObject, objects.GetDataId());
    EXPECT_EQ(0u, file.Lookup(-1)); EXPECT_EQ(0u, file.Lookup(1));
}

TEST_F(CKFileLoadLifecycleTest, BehaviorOnlyModeSkipsNonBehaviorChunkAndObjectCreation) {
    CKTestFrame frame(true); CKTestLoadFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_ONLYBEHAVIORS));
    ASSERT_EQ(1, file.m_FileObjects.Size()); EXPECT_EQ(nullptr, file.m_FileObjects[0].Data);
    EXPECT_EQ(nullptr, file.m_FileObjects[0].ObjPtr); EXPECT_EQ(0, objects.GetCount());
}

TEST_F(CKFileLoadLifecycleTest, MissingPluginStatusStillAllowsDirectLoadOfIndependentObjects) {
    CKTestFrame frame(true); const int objectHeader = 26; frame.HeaderSize = objectHeader + 24;
    std::memmove(frame.Bytes + 64 + frame.HeaderSize, frame.Bytes + 98, 12);
    Word(frame.Bytes, 64 + objectHeader, 1); Word(frame.Bytes, 68 + objectHeader, CKPLUGIN_MANAGER_DLL);
    Word(frame.Bytes, 72 + objectHeader, 1); Word(frame.Bytes, 76 + objectHeader, 0x13573511);
    Word(frame.Bytes, 80 + objectHeader, 0x24684622); frame.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(1, file.GetMissingPlugins()->Size()); EXPECT_FALSE((*file.GetMissingPlugins())[0].ValidGuids.IsSet(0));
    CKObjectArray objects; ASSERT_EQ(CK_OK, file.Load(frame.Bytes, frame.Size, &objects, CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_EQ(1, objects.GetCount());
}

TEST_F(CKFileLoadLifecycleTest, InvalidHeaderClassIsRejectedAndNextOpenStartsClean) {
    CKTestFrame bad(true), good; Word(bad.Bytes, 68, 0x7fffffff); bad.Refresh(); CKTestLoadFile file(Context);
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(bad.Bytes, bad.Size));
    EXPECT_EQ(CK_OK, file.OpenMemory(good.Bytes, good.Size)); EXPECT_TRUE(file.m_FileObjects.IsEmpty());
}

TEST_F(CKFileLoadLifecycleTest, DataFailureClearsParserMappedStorageAndCallbacks) {
    CKTestFrame frame; frame.DataSize = 6; Word(frame.Bytes, 24, CKFILE_WHOLECOMPRESSED);
    const unsigned char invalid[] = {0x78,0x9c,0xff,0xff,0xff,0xff}; std::memcpy(frame.Bytes + 72, invalid, 6);
    frame.Refresh(); Word(frame.Bytes, 36, 16); Word(frame.Bytes, 8, 0); Word(frame.Bytes, 8, Adler(frame.Bytes, frame.Size));
    Write(frame); CKTestLoadFile file(Context); std::string path = Path.string(); ASSERT_EQ(CK_OK, file.OpenFile(path.data()));
    Context->SetUserLoadCallback(LoadCallback, this); EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
    EXPECT_EQ(nullptr, file.m_Parser); EXPECT_EQ(nullptr, file.m_MappedFile); EXPECT_FALSE(Context->m_InLoad); ExpectModesReset();
}

TEST_F(CKFileLoadLifecycleTest, ColdLoadFileDataDoesNotChangeContextCallback) {
    CKTestLoadFile file(Context); Context->SetUserLoadCallback(LoadCallback, this);
    EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr)); EXPECT_EQ(LoadCallback, Context->m_UserLoadCallBack);
    EXPECT_EQ(this, Context->m_UserLoadCallBackArgs); EXPECT_FALSE(Context->m_InLoad);
}

TEST_F(CKFileLoadLifecycleTest, MissingObjectRecordLengthMustFailInsteadOfLoadingAnEmptyRecord) {
    CKTestFrame frame(true); frame.DataSize = 0; frame.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size)); EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
}

TEST_F(CKFileLoadLifecycleTest, NegativeObjectRecordLengthIsNotAnEmptyRecord) {
    CKTestFrame frame(true); Word(frame.Bytes, 98, 0xffffffffu); frame.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size)); EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
}

TEST_F(CKFileLoadLifecycleTest, DeclaredHeaderRegionMustContainTheDependencyCount) {
    CKTestFrame frame; frame.HeaderSize = 0; frame.Refresh(); CKTestLoadFile file(Context);
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileLoadLifecycleTest, TruncatedDependencyGuidsMustNotPublishAValidHeader) {
    CKTestFrame frame; frame.HeaderSize = 12; Word(frame.Bytes,64,1); Word(frame.Bytes,68,CKPLUGIN_MANAGER_DLL);
    Word(frame.Bytes,72,1); frame.Refresh(); CKTestLoadFile file(Context);
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(frame.Bytes, frame.Size));
}

TEST_F(CKFileLoadLifecycleTest, MissingIncludedFilenameLengthMustFail) {
    CKTestFrame frame; frame.HeaderSize = 12; Word(frame.Bytes,68,4); Word(frame.Bytes,72,1); frame.Refresh();
    CKTestLoadFile file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
}

static CKStateChunk *ParameterReference(CKGUID guid, CKFile *file, bool identifier = true) {
    int words[] = {static_cast<int>((7u << 16) | (CKCID_PARAMETERLOCAL << 8)),4,64,0,
                   static_cast<int>(guid.d1), static_cast<int>(guid.d2)};
    if (!identifier) words[2] = 123;
    CKStateChunk *chunk = CreateCKStateChunk(CKCID_PARAMETERLOCAL, file);
    if (!chunk->ConvertFromBuffer(words)) { delete chunk; return nullptr; } return chunk;
}

TEST_F(CKFileLoadLifecycleTest, ParameterReferenceUsesExactClassTypeAndCaseSensitiveNameNotValue) {
    CKParameterLocal *wrongType = Context->CreateCKParameterLocal("Shared", CKPGUID_FLOAT);
    CKParameterLocal *wrongCase = Context->CreateCKParameterLocal("shared", CKPGUID_INT);
    CKParameterLocal *target = Context->CreateCKParameterLocal("Shared", CKPGUID_INT);
    ASSERT_NE(nullptr, wrongType); ASSERT_NE(nullptr, wrongCase); ASSERT_NE(nullptr, target);
    int value = 73; target->SetValue(&value, sizeof(value)); CKTestLoadFile file(Context); CKFileObject reference;
    reference.ObjectCid = CKCID_PARAMETERLOCAL; reference.Name = const_cast<char *>("Shared");
    reference.Data = ParameterReference(CKPGUID_INT, &file); ASSERT_NE(nullptr, reference.Data);
    EXPECT_EQ(target, file.Reference(reference)); int actual = 0; target->GetValue(&actual); EXPECT_EQ(value, actual);
    delete reference.Data;
}

TEST_F(CKFileLoadLifecycleTest, ReferenceWithoutParameterDataOrIdentifierDoesNotResolve) {
    CKTestLoadFile file(Context); CKFileObject reference; reference.Name = const_cast<char *>("Shared");
    reference.ObjectCid = CKCID_BEOBJECT; EXPECT_EQ(nullptr, file.Reference(reference));
    reference.ObjectCid = CKCID_PARAMETERLOCAL; EXPECT_EQ(nullptr, file.Reference(reference));
    reference.Data = ParameterReference(CKPGUID_INT, &file, false); EXPECT_EQ(nullptr, file.Reference(reference)); delete reference.Data;
}

TEST_F(CKFileLoadLifecycleTest, ReferenceMissingNameDoesNotMatchNamedParameter) {
    ASSERT_NE(nullptr, Context->CreateCKParameterLocal("Shared", CKPGUID_INT));
    CKTestLoadFile file(Context); CKFileObject reference; reference.ObjectCid = CKCID_PARAMETERLOCAL;
    reference.Data = ParameterReference(CKPGUID_INT, &file); ASSERT_NE(nullptr, reference.Data);
    EXPECT_EQ(nullptr, file.Reference(reference)); delete reference.Data;
}

TEST_F(CKFileLoadLifecycleTest, FinishResolvesReferenceWithoutReloadingOrReturningExistingParameter) {
    CKParameterLocal *target = Context->CreateCKParameterLocal("Shared", CKPGUID_INT); ASSERT_NE(nullptr, target);
    int value = 123; target->SetValue(&value, sizeof(value)); CKTestLoadFile file(Context); file.m_IndexByClassId.Resize(80);
    file.m_FileObjects.Resize(1); CKFileObject &reference = file.m_FileObjects[0];
    reference.Object = 0x80000029u; reference.ObjectCid = CKCID_PARAMETERLOCAL; reference.Name = CKStrdup("Shared");
    reference.Data = ParameterReference(CKPGUID_INT, &file); ASSERT_NE(nullptr, reference.Data); file.m_SaveIDMax = 41;
    CKObjectArray objects; file.Finish(&objects, CK_LOAD_DEFAULT); EXPECT_EQ(target, reference.ObjPtr);
    EXPECT_EQ(target->GetID(), reference.CreatedObject); EXPECT_EQ(41u, reference.Object);
    EXPECT_EQ(CKFileObject::CK_FO_RENAMEOBJECT, reference.Options); EXPECT_EQ(0, objects.GetCount());
    int actual = 0; target->GetValue(&actual); EXPECT_EQ(value, actual);
}

TEST_F(CKFileLoadLifecycleTest, FinishReleasesUnrecognizedManagerDataBeforeReturning) {
    CKTestLoadFile file(Context); file.m_IndexByClassId.Resize(80); file.m_ManagersData.Resize(1);
    file.m_ManagersData[0].Manager = CKGUID(0x13573511,0x24684622);
    file.m_ManagersData[0].data = CreateCKStateChunk(CKCID_OBJECT, &file);
    file.Finish(nullptr, CK_LOAD_DEFAULT); EXPECT_EQ(nullptr, file.m_ManagersData[0].data);
}

TEST_F(CKFileLoadLifecycleTest, LoadAndSaveFailureDoesNotStartSavingOrTouchDestination) {
    CKTestLoadFile file(Context); std::string path = Path.string();
    file.LoadAndSave(const_cast<char *>("missing-pass35-input.cmo"), path.data());
    EXPECT_FALSE(std::filesystem::exists(Path)); EXPECT_FALSE(Context->m_Saving);
    EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
}

class CKTestLoadManager : public CKBaseManager {
public:
    int Before = 0, After = 0, Loads = 0; bool BeforeInLoad = true, AfterInLoad = false;
    explicit CKTestLoadManager(CKContext *context) : CKBaseManager(context, CKGUID(0x11223344,0x55667788), "Pass35 Load Probe") {
        context->RegisterNewManager(this);
    }
    CKDWORD GetValidFunctionsMask() override { return CKMANAGER_FUNC_PreLoad | CKMANAGER_FUNC_PostLoad; }
    CKERROR PreLoad() override { ++Before; BeforeInLoad = m_Context->m_InLoad != 0; return CK_OK; }
    CKERROR PostLoad() override { ++After; AfterInLoad = m_Context->m_InLoad != 0; return CK_OK; }
    CKERROR LoadData(CKStateChunk *chunk, CKFile *file) override {
        ++Loads; EXPECT_NE(nullptr, chunk); EXPECT_NE(nullptr, file); EXPECT_TRUE(m_Context->m_InLoad);
        if (chunk) { chunk->StartRead(); EXPECT_EQ(0x13572468,chunk->ReadInt()); EXPECT_EQ(0x24681357,chunk->ReadInt()); }
        return CK_OK;
    }
};

static void ManagerFrame(CKTestFrame &frame, bool compressed) {
    const unsigned char packed[] = {
        0x78,0x9c,0x73,0x31,0x56,0x12,0xec,0x28,0x4f,0x0b,0x15,0x60,0x60,0x60,0xe0,
        0x64,0x64,0x67,0x60,0x02,0xd2,0x19,0x2a,0xe1,0xc2,0xe1,0xc2,0x19,0x2a,0x00,0x44,0xbf,0x04,0x74
    };
    frame.DataSize = compressed ? sizeof(packed) : 28; Word(frame.Bytes,40,1);
    if (compressed) {
        std::memcpy(frame.Bytes + 64 + frame.HeaderSize, packed, sizeof(packed));
        Word(frame.Bytes,24,CKFILE_WHOLECOMPRESSED);
    } else {
        Word(frame.Bytes,72,0x11223344); Word(frame.Bytes,76,0x55667788); Word(frame.Bytes,80,16);
        Word(frame.Bytes,84,(7u<<16)|(CKCID_OBJECT<<8)|9); Word(frame.Bytes,88,2);
        Word(frame.Bytes,92,0x13572468); Word(frame.Bytes,96,0x24681357);
    }
    frame.Refresh(); Word(frame.Bytes,36,28); Word(frame.Bytes,8,0); Word(frame.Bytes,8,Adler(frame.Bytes,frame.Size));
}

TEST_F(CKFileLoadLifecycleTest, ManagerLifecycleInvokesRealRegisteredCallbacksAndConsumesData) {
    CKTestLoadManager *manager = new CKTestLoadManager(Context); CKTestFrame frame; ManagerFrame(frame,false); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK,file.Load(frame.Bytes,frame.Size,nullptr)); EXPECT_EQ(1,manager->Before); EXPECT_EQ(1,manager->Loads);
    EXPECT_EQ(1,manager->After); EXPECT_FALSE(manager->BeforeInLoad); EXPECT_TRUE(manager->AfterInLoad);
    ASSERT_EQ(1,file.m_ManagersData.Size()); EXPECT_EQ(nullptr,file.m_ManagersData[0].data); EXPECT_FALSE(Context->m_InLoad);
}

TEST_F(CKFileLoadLifecycleTest, PartialManagerFailureKeepsAcquiredChunkOwnedUntilReset) {
    CKTestFrame frame; ManagerFrame(frame,false); frame.DataSize=40; Word(frame.Bytes,40,2);
    Word(frame.Bytes,100,0x13573511); Word(frame.Bytes,104,0x24684622); Word(frame.Bytes,108,0xffffffffu);
    frame.Refresh(); CKTestLoadFile file(Context); ASSERT_EQ(CK_OK,file.OpenMemory(frame.Bytes,frame.Size));
    EXPECT_EQ(CKERR_INVALIDFILE,file.Data()); ASSERT_EQ(2,file.m_ManagersData.Size());
    ASSERT_NE(nullptr,file.m_ManagersData[0].data); EXPECT_EQ(nullptr,file.m_ManagersData[1].data);
    file.m_ManagersData[0].data->StartRead(); EXPECT_EQ(0x13572468,file.m_ManagersData[0].data->ReadInt());
    EXPECT_EQ(0x24681357,file.m_ManagersData[0].data->ReadInt());
    file.Reset(); EXPECT_EQ(0,file.m_ManagersData.Size()); EXPECT_EQ(nullptr,file.m_Parser); file.Reset();
}

TEST_F(CKFileLoadLifecycleTest, WholeCompressionFlagPermitsUncompressedEqualSizeFallback) {
    CKTestLoadManager *manager=new CKTestLoadManager(Context); CKTestFrame frame; ManagerFrame(frame,false);
    Word(frame.Bytes,24,CKFILE_WHOLECOMPRESSED); frame.Refresh(); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK,file.Load(frame.Bytes,frame.Size,nullptr)); EXPECT_EQ(1,manager->Loads);
    ASSERT_EQ(1,file.m_ManagersData.Size()); EXPECT_EQ(nullptr,file.m_ManagersData[0].data);
}

TEST_F(CKFileLoadLifecycleTest, CompressedObjectsReadIncludedFileFromOriginalTail) {
    CKTestFrame frame; frame.HeaderSize=12; Word(frame.Bytes,68,4); Word(frame.Bytes,72,1); ManagerFrame(frame,true);
    const std::string name=Path.filename().string()+".bin";
    const XString folder=Context->GetPathManager()->GetVirtoolsTemporaryFolder();
    const std::filesystem::path output=std::filesystem::path(folder.CStr())/name;
    std::error_code ignored; std::filesystem::remove(output,ignored);
    int tail=frame.Size; Word(frame.Bytes,tail,static_cast<unsigned int>(name.size())); tail+=4;
    std::memcpy(frame.Bytes+tail,name.data(),name.size()); tail+=static_cast<int>(name.size());
    Word(frame.Bytes,tail,5); tail+=4; const unsigned char payload[]={0,0x80,0xff,17,43};
    std::memcpy(frame.Bytes+tail,payload,5); frame.Size=tail+5;
    CKTestLoadFile file(Context); ASSERT_EQ(CK_OK,file.Load(frame.Bytes,frame.Size,nullptr));
    FILE *included=std::fopen(output.string().c_str(),"rb");
    EXPECT_NE(nullptr,included);
    if(included) { unsigned char actual[6]={}; EXPECT_EQ(5u,std::fread(actual,1,6,included));
        EXPECT_EQ(0,std::memcmp(payload,actual,5)); std::fclose(included); }
    std::filesystem::remove(output,ignored);
}

TEST_F(CKFileLoadLifecycleTest, LegacySixLoadsIdentifierNameAndChunkClassFromLiteralData) {
    CKTestFrame frame; Word(frame.Bytes,16,6); frame.HeaderSize=0; frame.DataSize=48;
    Word(frame.Bytes,64,41); Word(frame.Bytes,68,1); Word(frame.Bytes,72,41); Word(frame.Bytes,76,32);
    Word(frame.Bytes,80,(7u<<16)|(CKCID_BEOBJECT<<8)|9); Word(frame.Bytes,84,6);
    // StateChunk strings store the NUL in their byte count, with DWORD padding.
    Word(frame.Bytes,88,1); Word(frame.Bytes,92,0); Word(frame.Bytes,96,9); std::memcpy(frame.Bytes+100,"Legacy35",9);
    frame.Refresh(); Word(frame.Bytes,8,Adler(frame.Bytes+64,48)); CKTestLoadFile file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK,file.Load(frame.Bytes,frame.Size,&objects)); ASSERT_EQ(1,file.m_FileObjects.Size());
    EXPECT_EQ(CKCID_BEOBJECT,file.m_FileObjects[0].ObjectCid); EXPECT_STREQ("Legacy35",file.m_FileObjects[0].Name);
    ASSERT_NE(nullptr,file.m_FileObjects[0].ObjPtr); EXPECT_STREQ("Legacy35",file.m_FileObjects[0].ObjPtr->GetName());
    EXPECT_EQ(1,objects.GetCount());
}

TEST_F(CKFileLoadLifecycleTest, LegacySevenAllowsZeroSizeRecordWithSavedIdentityInHeader) {
    // Exact independent92-byte v7 frame: header64 + objectheader16 + data12.
    CKTestFrame frame; Word(frame.Bytes,16,7); frame.HeaderSize=16; frame.DataSize=12;
    Word(frame.Bytes,44,1); Word(frame.Bytes,64,1); Word(frame.Bytes,68,CKCID_OBJECT);
    Word(frame.Bytes,72,88); Word(frame.Bytes,76,0);
    Word(frame.Bytes,80,1); Word(frame.Bytes,84,1); Word(frame.Bytes,88,0);
    frame.Refresh(); Word(frame.Bytes,8,Adler(frame.Bytes+80,12)); ASSERT_EQ(92,frame.Size);
    CKTestLoadFile file(Context); ASSERT_EQ(CK_OK,file.OpenMemory(frame.Bytes,frame.Size));
    ASSERT_EQ(CK_OK,file.Data()); ASSERT_EQ(1,file.m_FileObjects.Size());
    EXPECT_EQ(1u,file.m_FileObjects[0].Object); EXPECT_EQ(CKCID_OBJECT,file.m_FileObjects[0].ObjectCid);
    EXPECT_EQ(88,file.m_FileObjects[0].FileIndex); EXPECT_EQ(nullptr,file.m_FileObjects[0].Name);
    EXPECT_EQ(nullptr,file.m_FileObjects[0].Data); EXPECT_EQ(1u,file.m_SaveIDMax);
}

TEST_F(CKFileLoadLifecycleTest, LegacyDependencyPreloadRetainsChunksForTheSubsequentLoad) {
    CKTestFrame frame; Word(frame.Bytes,16,6); frame.HeaderSize=0; frame.DataSize=24;
    Word(frame.Bytes,64,41); Word(frame.Bytes,68,1); Word(frame.Bytes,72,41); Word(frame.Bytes,76,8);
    Word(frame.Bytes,80,(7u<<16)|(CKCID_BEOBJECT<<8)|9); Word(frame.Bytes,84,0);
    frame.Refresh(); Word(frame.Bytes,8,Adler(frame.Bytes+64,24)); CKTestLoadFile file(Context);
    ASSERT_EQ(CK_OK,file.OpenMemory(frame.Bytes,frame.Size,CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_TRUE(file.m_ReadFileDataDone); EXPECT_EQ(nullptr,file.m_Parser); ASSERT_EQ(1,file.m_FileObjects.Size());
    ASSERT_NE(nullptr,file.m_FileObjects[0].Data); CKObjectArray objects;
    ASSERT_EQ(CK_OK,file.LoadFileData(&objects)); EXPECT_EQ(1,objects.GetCount());
}

TEST_F(CKFileLoadLifecycleTest, FailedLegacyDependencyPreloadCannotBecomeASuccessfulLoad) {
    CKTestFrame frame; Word(frame.Bytes,16,6); frame.HeaderSize=0; frame.DataSize=8;
    Word(frame.Bytes,64,0); Word(frame.Bytes,68,0); frame.Refresh(); Word(frame.Bytes,8,123);
    CKTestLoadFile file(Context); EXPECT_EQ(CKERR_FILECRCERROR,file.OpenMemory(frame.Bytes,frame.Size,CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_FALSE(file.m_ReadFileDataDone); EXPECT_EQ(nullptr,file.m_Parser);
    EXPECT_EQ(CKERR_INVALIDFILE,file.LoadFileData(nullptr));
}

TEST_F(CKFileLoadLifecycleTest, LoadAndSaveConvertsLiteralObjectAndRestoresAttributeSavingFlag) {
    CKTestFrame frame(true); Write(frame); CKTestLoadFile file(Context); std::string source=Path.string();
    std::filesystem::path destination=Path; destination+=".new"; std::string output=destination.string();
    std::error_code ignored; std::filesystem::remove(destination,ignored);
    file.LoadAndSave(source.data(),output.data()); EXPECT_TRUE(std::filesystem::exists(destination));
    EXPECT_FALSE(Context->m_Saving); EXPECT_EQ(0,Context->GetAttributeManager()->m_Saving);
    FILE *saved=std::fopen(output.c_str(),"rb"); ASSERT_NE(nullptr,saved); unsigned char header[64]={};
    EXPECT_EQ(64u,std::fread(header,1,64,saved)); std::fclose(saved);
    EXPECT_EQ(0,std::memcmp(header,"Nemo",4)); EXPECT_EQ(8,header[16]); EXPECT_EQ(1,header[44]);
    std::filesystem::remove(destination,ignored);
}

TEST_F(CKFileLoadLifecycleTest, ManagerRemapVisitsEveryObjectChunkAndLeavesOtherGuidsUntouched) {
    CKTestLoadFile file(Context); const CKGUID wanted(0x12345678,0x24681357),other(0x11111111,0x22222222);
    file.m_FileObjects.Resize(3); const int conversion[]={101,202,303};
    for(int i=0;i<2;++i) {
        const CKGUID guid=i?other:wanted;
        const int wire[]={static_cast<int>((CHNK_OPTION_MAN<<24)|(7u<<16)|(CKCID_OBJECT<<8)),3,
                          static_cast<int>(guid.d1),static_cast<int>(guid.d2),1,1,0};
        CKStateChunk *chunk=CreateCKStateChunk(CKCID_OBJECT,&file);
        ASSERT_TRUE(chunk->ConvertFromBuffer(const_cast<int *>(wire))); file.m_FileObjects[i].Data=chunk;
    }
    file.RemapManagerInt(wanted,const_cast<int *>(conversion),3);
    for(int i=0;i<2;++i) { CKStateChunk *chunk=file.m_FileObjects[i].Data; chunk->StartRead();
        EXPECT_EQ(i?other:wanted,chunk->ReadGuid()); EXPECT_EQ(i?1:202,chunk->ReadInt()); }
}
