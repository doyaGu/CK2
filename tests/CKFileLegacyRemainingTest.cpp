#include <gtest/gtest.h>
#include <cstring>
#include "CKAll.h"

namespace {

void PutWord37(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

unsigned int Adler37(const unsigned char *bytes, int count) {
    unsigned int a = 0, b = 0;
    for (int i = 0; i < count; ++i) { a = (a + bytes[i]) % 65521; b = (b + a) % 65521; }
    return a | (b << 16);
}

struct Frame37 {
    unsigned char Bytes[2048], Headers[512], Data[1024];
    int Version, HeaderSize, DataSize, Size, Objects, Managers;
    explicit Frame37(int version = 7) : Version(version), HeaderSize(0), DataSize(8),
        Size(0), Objects(0), Managers(0) {
        std::memset(Bytes, 0, sizeof(Bytes)); std::memset(Headers, 0, sizeof(Headers));
        std::memset(Data, 0, sizeof(Data)); PutWord37(Data, 0, 73);
    }
    void Manager(CKGUID guid, bool payload) {
        PutWord37(Data, DataSize, guid.d1); PutWord37(Data, DataSize + 4, guid.d2);
        PutWord37(Data, DataSize + 8, payload ? 12 : 0); DataSize += 12;
        if (payload) {
            PutWord37(Data, DataSize, (7u << 16) | (CKCID_OBJECT << 8) | 9u);
            PutWord37(Data, DataSize + 4, 1); PutWord37(Data, DataSize + 8, 0x1122); DataSize += 12;
        }
        ++Managers;
    }
    void Object(CK_ID id, CK_CLASSID cid, const char *name, const unsigned int *payload,
                int words, int dataVersion = 9) {
        if (Version >= 7) {
            const int length = static_cast<int>(std::strlen(name));
            PutWord37(Headers, HeaderSize, id); PutWord37(Headers, HeaderSize + 4, cid);
            PutWord37(Headers, HeaderSize + 8, 0x1000 + Objects);
            PutWord37(Headers, HeaderSize + 12, length);
            std::memcpy(Headers + HeaderSize + 16, name, length); HeaderSize += 16 + length;
        } else {
            PutWord37(Data, DataSize, id); DataSize += 4;
        }
        PutWord37(Data, DataSize, 8 + words * 4); DataSize += 4;
        PutWord37(Data, DataSize, (7u << 16) | (cid << 8) | dataVersion);
        PutWord37(Data, DataSize + 4, words); DataSize += 8;
        for (int i = 0; i < words; ++i) { PutWord37(Data, DataSize, payload[i]); DataSize += 4; }
        ++Objects;
    }
    void Refresh(int mode = CKFILE_UNCOMPRESSED, const unsigned char *packed = nullptr, int packedSize = 0) {
        std::memset(Bytes, 0, sizeof(Bytes)); std::memcpy(Bytes, "Nemo Fi", 7);
        PutWord37(Bytes, 12, 0x13022002); PutWord37(Bytes, 16, Version); PutWord37(Bytes, 24, mode);
        PutWord37(Bytes, 32, packed ? packedSize : DataSize); PutWord37(Bytes, 36, DataSize);
        PutWord37(Bytes, 40, Managers); PutWord37(Bytes, 44, Objects); PutWord37(Bytes, 48, 73);
        PutWord37(Data, 4, Objects); PutWord37(Bytes, 8, Adler37(Data, DataSize));
        std::memcpy(Bytes + 64, Headers, HeaderSize);
        std::memcpy(Bytes + 64 + HeaderSize, packed ? packed : Data, packed ? packedSize : DataSize);
        Size = 64 + HeaderSize + (packed ? packedSize : DataSize);
    }
};

class RemainingFile37 : public CKFile {
public:
    explicit RemainingFile37(CKContext *context) : CKFile(context) {}
    CKERROR ReadData() { return ReadFileData(&m_Parser); }
};

class CKFileLegacyRemainingTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        ASSERT_NE(nullptr, Context);
    }
    void TearDown() override { if (Context) CKCloseContext(Context); CKShutdown(); }
};

class CKFileLegacyManagerRecordsTest : public CKFileLegacyRemainingTest,
    public ::testing::WithParamInterface<int> {};

void ManagerObjects37(Frame37 &frame, int mode) {
    frame.Manager(CKGUID(0x11223344, 0x55667788), true);
    frame.Manager(CKGUID(0xabcdef01, 0x23456789), false);
    const unsigned int first[] = {0x9037, 0, 741}, second[] = {0x9037, 0, 852};
    frame.Object(73, CKCID_BEOBJECT, "HeaderA", first, 3);
    frame.Object(74, CKCID_BEOBJECT, "HeaderB", second, 3);
    // Standard zlib stream for the explicitly assembled 92-byte format7 data.
    const unsigned char packed[] = {0x78,0x9c,0xf3,0x64,0x60,0x60,0x60,0x02,0x62,0x17,0x63,0x25,
        0xc1,0x8e,0xf2,0xb4,0x50,0x1e,0x20,0x9b,0x93,0x91,0x9d,0x81,0x11,0x48,0x2b,0x09,0x32,
        0x30,0x30,0xbe,0x3f,0xbb,0xba,0x33,0xdd,0x55,0x19,0xc8,0x65,0x10,0x01,0xc9,0x09,0xb3,
        0x33,0x30,0x03,0x69,0xf3,0x09,0x0c,0x60,0xf0,0x94,0x09,0xbb,0x78,0x08,0x90,0x03,0x00,
        0x12,0xeb,0x0a,0x01};
    if (mode == 1 || mode == 8) frame.Refresh(mode, packed, sizeof(packed));
    else frame.Refresh();
}

TEST_P(CKFileLegacyManagerRecordsTest, ManagersPrecedeObjectsAndZeroManagerDoesNotShiftFollowingChunks) {
    Frame37 frame(GetParam() == 6 ? 6 : 7); ManagerObjects37(frame, GetParam());
    RemainingFile37 file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(CK_OK, file.ReadData()); ASSERT_EQ(2, file.m_ManagersData.Size());
    EXPECT_EQ(CKGUID(0x11223344, 0x55667788), file.m_ManagersData[0].Manager);
    EXPECT_EQ(CKGUID(0xabcdef01, 0x23456789), file.m_ManagersData[1].Manager);
    ASSERT_NE(nullptr, file.m_ManagersData[0].data);
    file.m_ManagersData[0].data->StartRead(); EXPECT_EQ(0x1122, file.m_ManagersData[0].data->ReadInt());
    EXPECT_EQ(nullptr, file.m_ManagersData[1].data); ASSERT_EQ(2, file.m_FileObjects.Size());
    for (int i = 0; i < 2; ++i) {
        CKFileObject &object = file.m_FileObjects[i]; EXPECT_EQ(73u + i, object.Object);
        EXPECT_EQ(CKCID_BEOBJECT, object.ObjectCid); ASSERT_NE(nullptr, object.Data);
        EXPECT_EQ(12, object.PrePackSize); EXPECT_EQ(12, object.PostPackSize);
        object.Data->StartRead(); ASSERT_TRUE(object.Data->SeekIdentifier(0x9037));
        EXPECT_EQ(i == 0 ? 741 : 852, object.Data->ReadInt());
        if (frame.Version == 7) {
            EXPECT_STREQ(i == 0 ? "HeaderA" : "HeaderB", object.Name);
            EXPECT_EQ(0x1000 + i, object.FileIndex);
        }
    }
}

TEST_P(CKFileLegacyManagerRecordsTest, DependencyPreloadRetainsBothManagerAndObjectChunks) {
    Frame37 frame(GetParam() == 6 ? 6 : 7); ManagerObjects37(frame, GetParam());
    RemainingFile37 file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_TRUE(file.m_ReadFileDataDone); EXPECT_EQ(nullptr, file.m_Parser);
    ASSERT_EQ(2, file.m_ManagersData.Size()); ASSERT_NE(nullptr, file.m_ManagersData[0].data);
    EXPECT_EQ(nullptr, file.m_ManagersData[1].data); ASSERT_EQ(2, file.m_FileObjects.Size());
    ASSERT_NE(nullptr, file.m_FileObjects[0].Data); ASSERT_NE(nullptr, file.m_FileObjects[1].Data);
    ASSERT_EQ(2, file.m_PluginsDep.Size());
    EXPECT_EQ(0, file.m_PluginsDep[0].m_Guids.Size()); EXPECT_EQ(0, file.m_PluginsDep[1].m_Guids.Size());
}

TEST_P(CKFileLegacyManagerRecordsTest, DataCrcCoversPayloadButExcludesProductAndObjectHeaders) {
    Frame37 frame(GetParam() == 6 ? 6 : 7); ManagerObjects37(frame, GetParam());
    RemainingFile37 file(Context);
    PutWord37(frame.Bytes, 56, 0x02050030);
    if (frame.Version == 7) { frame.Bytes[80] = 'Z'; PutWord37(frame.Bytes, 72, 0x1234); }
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size)); ASSERT_EQ(CK_OK, file.ReadData());
    EXPECT_EQ(0x02050030u, file.m_FileInfo.ProductBuild);
    ASSERT_EQ(2, file.m_FileObjects.Size());
    if (frame.Version == 7) {
        EXPECT_STREQ("ZeaderA", file.m_FileObjects[0].Name); EXPECT_EQ(0x1234, file.m_FileObjects[0].FileIndex);
    }
    frame.Bytes[8] ^= 1; RemainingFile37 wrongCrc(Context);
    ASSERT_EQ(CK_OK, wrongCrc.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_EQ(CKERR_FILECRCERROR, wrongCrc.ReadData());
}

INSTANTIATE_TEST_SUITE_P(FormatSixAndSevenDataModes, CKFileLegacyManagerRecordsTest,
    ::testing::Values(6, 0, 1, 8));

class LegacyManager37 : public CKBaseManager {
public:
    int Calls = 0, Value = 0; bool SawInLoad = false;
    explicit LegacyManager37(CKContext *context) : CKBaseManager(context,
        CKGUID(0x11223344, 0x55667788), "Legacy37 Manager") { context->RegisterNewManager(this); }
    CKERROR LoadData(CKStateChunk *chunk, CKFile *file) override {
        ++Calls; EXPECT_NE(nullptr, file); SawInLoad = m_Context->m_InLoad != 0;
        if (chunk) { chunk->StartRead(); Value = chunk->ReadInt(); }
        return CK_OK;
    }
};

TEST_F(CKFileLegacyRemainingTest, PreloadedVersionSevenLoadsRegisteredManagerAndCreatesNamedObjects) {
    LegacyManager37 *manager = new LegacyManager37(Context); Frame37 frame; ManagerObjects37(frame, 8);
    RemainingFile37 file(Context); CKObjectArray objects;
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(CK_OK, file.LoadFileData(&objects)); EXPECT_EQ(1, manager->Calls);
    EXPECT_EQ(0x1122, manager->Value); EXPECT_TRUE(manager->SawInLoad); EXPECT_FALSE(Context->m_InLoad);
    ASSERT_EQ(2, objects.GetCount()); ASSERT_EQ(2, file.m_FileObjects.Size());
    ASSERT_NE(nullptr, file.m_FileObjects[0].ObjPtr); ASSERT_NE(nullptr, file.m_FileObjects[1].ObjPtr);
    EXPECT_STREQ("HeaderA", file.m_FileObjects[0].ObjPtr->GetName());
    EXPECT_STREQ("HeaderB", file.m_FileObjects[1].ObjPtr->GetName());
    EXPECT_EQ(nullptr, file.m_ManagersData[0].data);
}

TEST_F(CKFileLegacyRemainingTest, BehaviorOnlyVersionSevenSkipsNonBehaviorPayloadWithoutLosingNextRecord) {
    Frame37 frame; const unsigned int normal[] = {0x9037, 0, 741}, behavior[] = {0x9037, 0, 852};
    frame.Object(73, CKCID_BEOBJECT, "Normal", normal, 3);
    frame.Object(74, CKCID_BEHAVIOR, "Behavior", behavior, 3); frame.Refresh();
    RemainingFile37 file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_ONLYBEHAVIORS));
    ASSERT_EQ(CK_OK, file.ReadData()); ASSERT_EQ(2, file.m_FileObjects.Size());
    EXPECT_EQ(nullptr, file.m_FileObjects[0].Data); ASSERT_NE(nullptr, file.m_FileObjects[1].Data);
    file.m_FileObjects[1].Data->StartRead(); ASSERT_TRUE(file.m_FileObjects[1].Data->SeekIdentifier(0x9037));
    EXPECT_EQ(852, file.m_FileObjects[1].Data->ReadInt());
}

class CKFileLegacyBehaviorDependenciesTest : public CKFileLegacyRemainingTest,
    public ::testing::WithParamInterface<int> {};

TEST_P(CKFileLegacyBehaviorDependenciesTest, DependencyPreloadReadsPrototypeAndBothNewDataOrders) {
    const unsigned int first = 0x6a3724e9, second = 0x1234567b;
    unsigned int payload[5] = {0, 0, 0, 0, 0}; int count = 5;
    if (GetParam() == 0) { payload[0] = CK_STATESAVE_BEHAVIORPROTOGUID; payload[2] = first; payload[3] = second; count = 4; }
    else if (GetParam() == 4) { payload[0] = CK_STATESAVE_BEHAVIORNEWDATA; payload[2] = first; payload[3] = second; payload[4] = CKBEHAVIOR_BUILDINGBLOCK; }
    else { payload[0] = CK_STATESAVE_BEHAVIORNEWDATA; payload[2] = CKBEHAVIOR_BUILDINGBLOCK; payload[3] = first; payload[4] = second; }
    Frame37 frame; frame.Object(73, CKCID_BEHAVIOR, "Missing", payload, count, GetParam() == 4 ? 4 : 5); frame.Refresh();
    RemainingFile37 file(Context);
    EXPECT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    EXPECT_TRUE(file.m_ReadFileDataDone); EXPECT_EQ(nullptr, file.m_Parser);
    ASSERT_EQ(2, file.m_PluginsDep.Size()); EXPECT_EQ(CKPLUGIN_BEHAVIOR_DLL, file.m_PluginsDep[0].m_PluginCategory);
    ASSERT_EQ(1, file.m_PluginsDep[0].m_Guids.Size()); EXPECT_EQ(CKGUID(first, second), file.m_PluginsDep[0].m_Guids[0]);
    EXPECT_FALSE(file.m_PluginsDep[0].ValidGuids.IsSet(0)); ASSERT_EQ(1, file.m_FileObjects.Size());
    ASSERT_NE(nullptr, file.m_FileObjects[0].Data);
}

INSTANTIATE_TEST_SUITE_P(PrototypeAndNewDataVersionOrders, CKFileLegacyBehaviorDependenciesTest,
    ::testing::Values(0, 4, 5));

TEST_F(CKFileLegacyRemainingTest, NonBuildingBlockNewDataDoesNotReportItsGuidAsMissingPlugin) {
    for (int version = 4; version <= 5; ++version) {
        const unsigned int oldData[] = {CK_STATESAVE_BEHAVIORNEWDATA, 0, 0x6a3724e9, 0x1234567b, 0};
        const unsigned int newData[] = {CK_STATESAVE_BEHAVIORNEWDATA, 0, 0, 0x6a3724e9, 0x1234567b};
        Frame37 frame; frame.Object(73, CKCID_BEHAVIOR, "Script", version == 4 ? oldData : newData, 5, version); frame.Refresh();
        RemainingFile37 file(Context);
        ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
        ASSERT_EQ(2, file.m_PluginsDep.Size()); EXPECT_EQ(0, file.m_PluginsDep[0].m_Guids.Size());
    }
}

TEST_F(CKFileLegacyRemainingTest, MissingBehaviorIdentifierDoesNotInventPluginDependency) {
    const unsigned int payload[] = {0x9037, 0, 741}; Frame37 frame;
    frame.Object(73, CKCID_BEHAVIOR, "NoPrototype", payload, 3); frame.Refresh(); RemainingFile37 file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(2, file.m_PluginsDep.Size()); EXPECT_EQ(0, file.m_PluginsDep[0].m_Guids.Size());
}

TEST_F(CKFileLegacyRemainingTest, DependencyChecksRemainExplicitForOldFormats) {
    const unsigned int payload[] = {CK_STATESAVE_BEHAVIORPROTOGUID, 0, 0x6a3724e9, 0x1234567b};
    Frame37 frame; frame.Object(73, CKCID_BEHAVIOR, "Missing", payload, 4); frame.Refresh();
    RemainingFile37 file(Context); ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    EXPECT_FALSE(file.m_ReadFileDataDone); ASSERT_NE(nullptr, file.m_Parser); EXPECT_EQ(0, file.m_PluginsDep.Size());
    ASSERT_EQ(CK_OK, file.ReadData()); ASSERT_EQ(1, file.m_FileObjects.Size()); ASSERT_NE(nullptr, file.m_FileObjects[0].Data);
}

TEST_F(CKFileLegacyRemainingTest, EmptyVersionSevenObjectChunkPreservesFollowingPayloadAndIdentity) {
    Frame37 frame; const unsigned int payload[] = {0x9037, 0, 852};
    frame.Object(73, CKCID_BEOBJECT, "Empty", nullptr, 0); frame.Object(74, CKCID_BEOBJECT, "Next", payload, 3);
    // A zero-size record is distinct from a converted chunk with zero data words.
    std::memmove(frame.Data + 12, frame.Data + 20, frame.DataSize - 20); frame.DataSize -= 8;
    PutWord37(frame.Data, 8, 0); frame.Refresh(); RemainingFile37 file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size)); ASSERT_EQ(CK_OK, file.ReadData());
    ASSERT_EQ(2, file.m_FileObjects.Size()); EXPECT_STREQ("Empty", file.m_FileObjects[0].Name);
    EXPECT_EQ(nullptr, file.m_FileObjects[0].Data); EXPECT_EQ(74u, file.m_FileObjects[1].Object);
    ASSERT_NE(nullptr, file.m_FileObjects[1].Data); file.m_FileObjects[1].Data->StartRead();
    ASSERT_TRUE(file.m_FileObjects[1].Data->SeekIdentifier(0x9037)); EXPECT_EQ(852, file.m_FileObjects[1].Data->ReadInt());
}

} // namespace
