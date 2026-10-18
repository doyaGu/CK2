#include <gtest/gtest.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "CKAll.h"

class CKTestLayoutFile : public CKFile {
public:
    explicit CKTestLayoutFile(CKContext *context) : CKFile(context) {}
    void Reset() { ClearData(); }
};

class CKFileLayoutStateTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    char Output[128];
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        std::snprintf(Output, sizeof(Output), "CKFileLayout-pass37-%p.cmo", static_cast<void *>(this));
    }
    void TearDown() override {
        std::remove(Output);
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
};

TEST_F(CKFileLayoutStateTest, PublicLayoutMatchesWin32SdkAndExplicitPortableX64Layout) {
    CKTestLayoutFile file(Context);
    const CKFile &base = file;
    const char *start = reinterpret_cast<const char *>(&base);
    // Independent SDK/native Win32 offsets; x64 adapts host pointer widths.
    const size_t x86[] = {0,4,16,28,40,52,64,120,124,132,136,140,144,148,152,176,180,188};
    const size_t x64[] = {0,8,32,56,80,104,128,184,192,208,216,224,232,240,248,296,304,320};
    const void *members[] = {
        &base.m_SaveIDMax, &base.m_FileObjects, &base.m_ManagersData, &base.m_PluginsDep,
        &base.m_IndexByClassId, &base.m_IncludedFiles, &base.m_FileInfo, &base.m_SceneSaved,
        &base.m_AlreadySavedMask, &base.m_Flags, &base.m_FileName, &base.m_Context,
        &base.m_Parser, &base.m_MappedFile, &base.m_ObjectsHashTable, &base.m_ReadFileDataDone,
        &base.m_AlreadyReferencedMask, &base.m_ReferencedObjects
    };
    const char *names[] = {
        "SaveIDMax", "FileObjects", "ManagersData", "PluginsDep", "IndexByClassId", "IncludedFiles",
        "FileInfo", "SceneSaved", "AlreadySavedMask", "Flags", "FileName", "Context", "Parser",
        "MappedFile", "ObjectsHashTable", "ReadFileDataDone", "AlreadyReferencedMask", "ReferencedObjects"
    };
    const size_t *expected = sizeof(void *) == 8 ? x64 : x86;
    for (int i = 0; i < 18; ++i) {
        SCOPED_TRACE(names[i]);
        EXPECT_EQ(expected[i], static_cast<size_t>(reinterpret_cast<const char *>(members[i]) - start));
    }
    EXPECT_EQ(sizeof(void *) == 8 ? 344u : 200u, sizeof(CKFile));
    EXPECT_EQ(sizeof(void *) == 8 ? 8u : 4u, alignof(CKFile));
}

TEST_F(CKFileLayoutStateTest, ConstructorInitializesSessionMembersWithoutClaimingHeaderSummaryValues) {
    CKTestLayoutFile file(Context);
    EXPECT_EQ(Context, file.m_Context);
    EXPECT_EQ(0u, file.m_SaveIDMax);
    EXPECT_EQ(0u, file.m_Flags);
    EXPECT_FALSE(file.m_SceneSaved);
    EXPECT_FALSE(file.m_ReadFileDataDone);
    EXPECT_EQ(nullptr, file.m_FileName);
    EXPECT_EQ(nullptr, file.m_Parser);
    EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(0, file.m_FileObjects.Size());
    EXPECT_EQ(0, file.m_ManagersData.Size());
    EXPECT_EQ(0, file.m_PluginsDep.Size());
    EXPECT_EQ(0, file.m_IndexByClassId.Size());
    EXPECT_EQ(0, file.m_IncludedFiles.Size());
    EXPECT_EQ(0, file.m_ObjectsHashTable.Size());
    EXPECT_EQ(0, file.m_ReferencedObjects.Size());
    EXPECT_EQ(0, file.m_AlreadySavedMask.BitSet());
    EXPECT_EQ(0, file.m_AlreadyReferencedMask.BitSet());
    // m_FileInfo is an uninitialized POD in the original constructor as well.
}

static void PutWord(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

TEST_F(CKFileLayoutStateTest, IndependentHeaderPublishesSummaryAndResetPreservesThatSummary) {
    unsigned char bytes[72] = {};
    std::memcpy(bytes, "Nemo Fi", 7);
    PutWord(bytes, 12, 0x13022002);
    PutWord(bytes, 16, 8);
    PutWord(bytes, 24, CKFILE_FORVIEWER);
    PutWord(bytes, 28, 8);
    PutWord(bytes, 48, 73);
    PutWord(bytes, 52, 11);
    PutWord(bytes, 56, 0x02050030);
    PutWord(bytes, 60, 8);
    unsigned int a = 0, b = 0;
    for (int i = 0; i < sizeof(bytes); ++i) {
        a = (a + bytes[i]) % 65521;
        b = (b + a) % 65521;
    }
    PutWord(bytes, 8, a | (b << 16));
    CKTestLayoutFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(bytes, sizeof(bytes), CK_LOAD_AS_DYNAMIC_OBJECT));
    EXPECT_EQ(73u, file.m_SaveIDMax);
    EXPECT_EQ(CK_LOAD_AS_DYNAMIC_OBJECT, file.m_Flags);
    EXPECT_EQ(CKFILE_FORVIEWER, file.m_FileInfo.FileWriteMode);
    EXPECT_EQ(8u, file.m_FileInfo.FileVersion);
    EXPECT_EQ(0x13022002u, file.m_FileInfo.CKVersion);
    EXPECT_EQ(11u, file.m_FileInfo.ProductVersion);
    EXPECT_EQ(0x02050030u, file.m_FileInfo.ProductBuild);
    EXPECT_EQ(72u, file.m_FileInfo.FileSize);
    EXPECT_EQ(73u, file.m_FileInfo.MaxIDSaved);
    EXPECT_EQ(a | (b << 16), file.m_FileInfo.Crc);
    EXPECT_EQ(8u, file.m_FileInfo.Hdr1PackSize);
    EXPECT_EQ(8u, file.m_FileInfo.Hdr1UnPackSize);
    EXPECT_EQ(0u, file.m_FileInfo.DataPackSize);
    EXPECT_EQ(0u, file.m_FileInfo.DataUnPackSize);
    EXPECT_EQ(0u, file.m_FileInfo.ObjectCount);
    EXPECT_EQ(0u, file.m_FileInfo.ManagerCount);
    ASSERT_NE(nullptr, file.m_Parser);
    EXPECT_EQ(reinterpret_cast<char *>(bytes), file.m_Parser->m_Buffer);
    EXPECT_FALSE(file.m_Parser->m_Valid);
    EXPECT_EQ(72, file.m_Parser->m_CursorPos);
    EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(nullptr, file.m_FileName);
    ASSERT_GT(file.m_IndexByClassId.Size(), CKCID_OBJECT);
    const CKFileInfo summary = file.m_FileInfo;
    file.Reset();
    EXPECT_EQ(0, std::memcmp(&summary, &file.m_FileInfo, sizeof(summary)));
    EXPECT_EQ(0u, file.m_Flags);
    EXPECT_EQ(0u, file.m_SaveIDMax);
    EXPECT_EQ(nullptr, file.m_Parser);
    EXPECT_EQ(Context, file.m_Context);
    EXPECT_EQ(0, file.m_IndexByClassId.Size());
    // Product metadata is carried independently; this literal is not a 2.5 golden.
}

TEST_F(CKFileLayoutStateTest, ReferencePromotionKeepsHashAndClassIndexConsistentWithSaveQueue) {
    CKTestLayoutFile file(Context);
    ASSERT_EQ(CK_OK, file.StartSave(Output, 0x1234));
    EXPECT_STREQ(Output, file.m_FileName);
    EXPECT_NE(static_cast<void *>(Output), static_cast<void *>(file.m_FileName));
    CKObject *object = Context->CreateObject(CKCID_OBJECT, "layout-reference");
    ASSERT_NE(nullptr, object);
    const CK_ID id = object->GetID();
    file.SaveObjectAsReference(object);
    ASSERT_EQ(1, file.m_FileObjects.Size());
    ASSERT_EQ(1, file.m_ReferencedObjects.Size());
    EXPECT_EQ(object, file.m_ReferencedObjects[0]);
    EXPECT_TRUE(file.m_AlreadyReferencedMask.IsSet(id));
    EXPECT_FALSE(file.m_AlreadySavedMask.IsSet(id));
    int index = -1;
    ASSERT_TRUE(file.m_ObjectsHashTable.LookUp(id, index));
    ASSERT_EQ(0, index);
    ASSERT_EQ(1, file.m_IndexByClassId[CKCID_OBJECT].Size());
    EXPECT_EQ(index, file.m_IndexByClassId[CKCID_OBJECT][0]);
    file.SaveObject(object, 0x4321);
    ASSERT_EQ(1, file.m_FileObjects.Size());
    EXPECT_EQ(0, file.m_ReferencedObjects.Size());
    EXPECT_FALSE(file.m_AlreadyReferencedMask.IsSet(id));
    EXPECT_TRUE(file.m_AlreadySavedMask.IsSet(id));
    EXPECT_EQ(id, file.m_SaveIDMax);
    EXPECT_EQ(0x4321u, file.m_FileObjects[index].SaveFlags);
    EXPECT_EQ(object, file.m_FileObjects[index].ObjPtr);
    EXPECT_EQ(CK_OK, file.EndSave());
    EXPECT_FALSE(Context->m_Saving);
    EXPECT_EQ(Context, file.m_Context);
    EXPECT_EQ(nullptr, file.m_FileName);
    EXPECT_EQ(0, file.m_ObjectsHashTable.Size());
    EXPECT_EQ(0, file.m_FileObjects.Size());
    EXPECT_EQ(0, file.m_AlreadySavedMask.BitSet());
    EXPECT_EQ(0, file.m_AlreadyReferencedMask.BitSet());
    EXPECT_EQ(object, Context->GetObject(id));
}

TEST_F(CKFileLayoutStateTest, ActualSceneSaveMarksSceneStateAndFinalizationClearsTheSession) {
    CKTestLayoutFile file(Context);
    ASSERT_EQ(CK_OK, file.StartSave(Output, 0x5678));
    CKObject *scene = Context->CreateObject(CKCID_SCENE, "layout-scene");
    ASSERT_NE(nullptr, scene);
    EXPECT_FALSE(file.m_SceneSaved);
    file.SaveObject(scene);
    EXPECT_TRUE(file.m_SceneSaved);
    EXPECT_TRUE(file.m_AlreadySavedMask.IsSet(scene->GetID()));
    EXPECT_GE(file.m_SaveIDMax, scene->GetID());
    ASSERT_GT(file.m_IndexByClassId[CKCID_SCENE].Size(), 0);
    const int index = file.m_IndexByClassId[CKCID_SCENE][0];
    ASSERT_GE(index, 0);
    ASSERT_LT(index, file.m_FileObjects.Size());
    EXPECT_EQ(scene, file.m_FileObjects[index].ObjPtr);
    ASSERT_EQ(CK_OK, file.EndSave());
    EXPECT_FALSE(file.m_SceneSaved);
    EXPECT_FALSE(file.m_ReadFileDataDone);
    EXPECT_EQ(0u, file.m_Flags);
    EXPECT_EQ(0u, file.m_SaveIDMax);
    EXPECT_EQ(0, file.m_IndexByClassId.Size());
}
