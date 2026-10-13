#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "CKAll.h"

namespace {

void PutWord(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
}

// An independently authored empty format-8 file, not CKFile::EndSave output.
struct EmptyFrame {
    unsigned char Bytes[72];
    explicit EmptyFrame(unsigned int mode = 0) {
        std::memset(Bytes, 0, sizeof(Bytes));
        std::memcpy(Bytes, "Nemo Fi", 7);
        PutWord(Bytes, 12, 0x13022002);
        PutWord(Bytes, 16, 8);
        PutWord(Bytes, 24, mode);
        PutWord(Bytes, 28, 8);
        PutWord(Bytes, 56, 0x01010000);
        PutWord(Bytes, 60, 8);
        unsigned int a = 0, b = 0;
        for (int i = 0; i < sizeof(Bytes); ++i) {
            a = (a + Bytes[i]) % 65521;
            b = (b + a) % 65521;
        }
        PutWord(Bytes, 8, a | (b << 16));
    }
};

class ReuseFile : public CKFile {
public:
    explicit ReuseFile(CKContext *context) : CKFile(context) {}
    void Reset() { ClearData(); }
};

class CKFileReuseLifecycleTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    std::filesystem::path Input, Output;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        char name[100];
        std::snprintf(name, sizeof(name), "ckfile-pass36-%p.cmo", static_cast<void *>(this));
        Input = std::filesystem::temp_directory_path() / name;
        Output = Input;
        Output += ".output.cmo";
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove(Input, ignored);
        std::filesystem::remove(Output, ignored);
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
    void WriteInput(const EmptyFrame &frame) {
        FILE *file = std::fopen(Input.string().c_str(), "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(sizeof(frame.Bytes), std::fwrite(frame.Bytes, 1, sizeof(frame.Bytes), file));
        EXPECT_EQ(0, std::fclose(file));
    }
    void OpenMapped(ReuseFile &file) {
        EmptyFrame frame;
        WriteInput(frame);
        std::string path = Input.string();
        ASSERT_EQ(CK_OK, file.OpenFile(path.data()));
        ASSERT_NE(nullptr, file.m_MappedFile);
        ASSERT_NE(nullptr, file.m_Parser);
    }
    void ExpectNoInputSession(const CKFile &file) {
        EXPECT_EQ(nullptr, file.m_Parser);
        EXPECT_EQ(nullptr, file.m_MappedFile);
        EXPECT_EQ(nullptr, file.m_FileName);
        EXPECT_TRUE(file.m_FileObjects.IsEmpty());
        EXPECT_TRUE(file.m_ManagersData.IsEmpty());
        EXPECT_FALSE(file.m_ReadFileDataDone);
    }
};

TEST_F(CKFileReuseLifecycleTest, MappedFileToExternalMemoryReleasesTheMappedSession) {
    ReuseFile file(Context);
    // Record portable layout evidence without asserting original Win32 byte equality.
    RecordProperty("sizeof_CKFile", static_cast<int>(sizeof(CKFile)));
    RecordProperty("alignof_CKFile", static_cast<int>(alignof(CKFile)));
    const char *start = reinterpret_cast<const char *>(static_cast<const CKFile *>(&file));
    RecordProperty("offset_Context", static_cast<int>(reinterpret_cast<const char *>(&file.m_Context) - start));
    RecordProperty("offset_Parser", static_cast<int>(reinterpret_cast<const char *>(&file.m_Parser) - start));
    RecordProperty("offset_MappedFile", static_cast<int>(reinterpret_cast<const char *>(&file.m_MappedFile) - start));
    RecordProperty("offset_ReadFileDataDone", static_cast<int>(reinterpret_cast<const char *>(&file.m_ReadFileDataDone) - start));
    OpenMapped(file);
    EmptyFrame memory(4);
    ASSERT_EQ(CK_OK, file.OpenMemory(memory.Bytes, sizeof(memory.Bytes)));
    EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(nullptr, file.m_FileName);
    ASSERT_NE(nullptr, file.m_Parser);
    EXPECT_EQ(4u, file.m_FileInfo.FileWriteMode);
    EXPECT_EQ(CK_OK, file.LoadFileData(nullptr));
    memory.Bytes[0] = 'X'; // Input remains caller-owned after parser teardown.
    EXPECT_EQ('X', memory.Bytes[0]);
}

TEST_F(CKFileReuseLifecycleTest, ReopeningTheMappedBaseKeepsItsBackingStorageAlive) {
    ReuseFile file(Context);
    OpenMapped(file);
    VxMemoryMappedFile *mapping = file.m_MappedFile;
    void *base = mapping->GetBase();
    ASSERT_EQ(CK_OK, file.OpenMemory(base, static_cast<int>(mapping->GetFileSize())));
    EXPECT_EQ(mapping, file.m_MappedFile);
    EXPECT_STREQ(Input.string().c_str(), file.m_FileName);
    ASSERT_EQ(CK_OK, file.LoadFileData(nullptr));
    EXPECT_EQ(nullptr, file.m_MappedFile);
}

TEST_F(CKFileReuseLifecycleTest, NullMemoryReopenCancelsThePreviousMappedLoad) {
    ReuseFile file(Context);
    OpenMapped(file);
    EXPECT_EQ(CKERR_INVALIDPARAMETER, file.OpenMemory(nullptr, 0));
    ExpectNoInputSession(file);
    EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
    EmptyFrame good;
    EXPECT_EQ(CK_OK, file.Load(good.Bytes, sizeof(good.Bytes), nullptr));
}

TEST_F(CKFileReuseLifecycleTest, ShortMemoryReopenCancelsThePreviousMappedLoad) {
    ReuseFile file(Context);
    OpenMapped(file);
    unsigned char shortFrame[12] = {};
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenMemory(shortFrame, sizeof(shortFrame)));
    ExpectNoInputSession(file);
    EXPECT_EQ(CKERR_INVALIDFILE, file.LoadFileData(nullptr));
}

TEST_F(CKFileReuseLifecycleTest, CrcFailureAfterMappedOpenCanBeReplacedByValidMemory) {
    ReuseFile file(Context);
    OpenMapped(file);
    EmptyFrame bad, good;
    bad.Bytes[56] ^= 1;
    EXPECT_EQ(CKERR_FILECRCERROR, file.OpenMemory(bad.Bytes, sizeof(bad.Bytes)));
    EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(nullptr, file.m_FileName);
    ASSERT_EQ(CK_OK, file.OpenMemory(good.Bytes, sizeof(good.Bytes)));
    EXPECT_EQ(CK_OK, file.LoadFileData(nullptr));
}

TEST_F(CKFileReuseLifecycleTest, MissingFileAfterMappedOpenCanBeReplacedByValidMemory) {
    ReuseFile file(Context);
    OpenMapped(file);
    std::string missing = Output.string();
    EXPECT_EQ(CKERR_INVALIDFILE, file.OpenFile(missing.data()));
    ExpectNoInputSession(file);
    EmptyFrame memory;
    EXPECT_EQ(CK_OK, file.Load(memory.Bytes, sizeof(memory.Bytes), nullptr));
}

TEST_F(CKFileReuseLifecycleTest, BadMappedHeaderCanBeReplacedByExternalMemory) {
    EmptyFrame bad;
    bad.Bytes[56] ^= 1;
    WriteInput(bad);
    ReuseFile file(Context);
    std::string path = Input.string();
    EXPECT_EQ(CKERR_FILECRCERROR, file.OpenFile(path.data()));
    EmptyFrame memory;
    ASSERT_EQ(CK_OK, file.OpenMemory(memory.Bytes, sizeof(memory.Bytes)));
    EXPECT_EQ(nullptr, file.m_MappedFile);
    EXPECT_EQ(nullptr, file.m_FileName);
    EXPECT_EQ(CK_OK, file.LoadFileData(nullptr));
}

TEST_F(CKFileReuseLifecycleTest, ResetReleasesOwnedRecordsAndPreservesBorrowedObjects) {
    ReuseFile file(Context);
    OpenMapped(file);
    CKObject object(Context, "borrowed");
    const CK_ID id = object.GetID();
    file.m_FileObjects.Resize(1);
    file.m_FileObjects[0].ObjPtr = &object;
    file.m_FileObjects[0].Name = CKStrdup("owned");
    file.m_FileObjects[0].Data = CreateCKStateChunk(CKCID_OBJECT, &file);
    file.m_ManagersData.Resize(1);
    file.m_ManagersData[0].data = CreateCKStateChunk(CKCID_OBJECT, &file);
    file.m_IncludedFiles.PushBack(XString("retired.bin"));
    file.m_PluginsDep.Resize(1);
    file.m_AlreadySavedMask.Set(id);
    file.m_AlreadyReferencedMask.Set(id);
    file.m_ReferencedObjects.PushBack(&object);
    file.m_ObjectsHashTable.Insert(id, 0);
    file.Reset();
    file.Reset();
    ExpectNoInputSession(file);
    EXPECT_EQ(&object, Context->GetObject(id));
    EXPECT_EQ(0, file.m_IncludedFiles.Size());
    EXPECT_EQ(0, file.m_PluginsDep.Size());
    EXPECT_TRUE(file.m_ReferencedObjects.IsEmpty());
    EXPECT_FALSE(file.IsObjectToBeSaved(id));
    EXPECT_EQ(0u, file.m_AlreadySavedMask.IsSet(id));
    EXPECT_EQ(0u, file.m_AlreadyReferencedMask.IsSet(id));
}

TEST_F(CKFileReuseLifecycleTest, LoadedMemorySessionCanBeReplacedByActualMappedFile) {
    ReuseFile file(Context);
    EmptyFrame memory(4);
    ASSERT_EQ(CK_OK, file.Load(memory.Bytes, sizeof(memory.Bytes), nullptr));
    OpenMapped(file);
    EXPECT_FALSE(file.m_ReadFileDataDone);
    EXPECT_EQ(0u, file.m_FileInfo.FileWriteMode);
    EXPECT_EQ(CK_OK, file.LoadFileData(nullptr));
}

TEST_F(CKFileReuseLifecycleTest, LoadAndSaveCreatesAValidIndependentOutputFile) {
    EmptyFrame frame(4);
    WriteInput(frame);
    std::string input = Input.string(), output = Output.string();
    ReuseFile file(Context);
    file.LoadAndSave(input.data(), output.data());
    ASSERT_TRUE(std::filesystem::exists(Output));
    EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
    EXPECT_FALSE(Context->m_Saving);
    EXPECT_EQ(CK_OK, file.Load(output.data(), nullptr));
    EXPECT_EQ(8u, file.m_FileInfo.FileVersion);
    EXPECT_EQ(4u, file.m_FileInfo.FileWriteMode);
}

TEST_F(CKFileReuseLifecycleTest, LoadAndSaveFailedInputDoesNotCreateOutputAndAllowsReuse) {
    std::string missing = Input.string(), output = Output.string();
    ReuseFile file(Context);
    file.LoadAndSave(missing.data(), output.data());
    EXPECT_FALSE(std::filesystem::exists(Output));
    EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
    EXPECT_FALSE(Context->m_Saving);
    EmptyFrame good;
    EXPECT_EQ(CK_OK, file.Load(good.Bytes, sizeof(good.Bytes), nullptr));
}

TEST_F(CKFileReuseLifecycleTest, LoadAndSaveFailedOutputRestoresAttributeSaveModeAndAllowsReuse) {
    EmptyFrame frame;
    WriteInput(frame);
    std::string input = Input.string();
    std::string directory = std::filesystem::temp_directory_path().string();
    ReuseFile file(Context);
    file.LoadAndSave(input.data(), directory.data());
    EXPECT_EQ(0, Context->GetAttributeManager()->m_Saving);
    EXPECT_FALSE(Context->m_Saving);
    EXPECT_EQ(CK_OK, file.Load(frame.Bytes, sizeof(frame.Bytes), nullptr));
}

} // namespace
