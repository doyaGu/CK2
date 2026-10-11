#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

namespace {

// Literal disk envelopes and relocation tables, independent of all chunk
// object/manager/subchunk writers. Offsets and lengths are DWORD counts.
void LoadRemapWire(CKStateChunk &chunk, int version, const int *words, int count,
                   const int *ids = nullptr, int idCount = 0,
                   const int *children = nullptr, int childCount = 0,
                   const int *managers = nullptr, int managerCount = 0) {
    int wire[256] = {};
    int cursor;
    if (version < CHUNK_VERSION3) {
        wire[0] = version << 16;
        wire[1] = CKCID_OBJECT;
        wire[2] = count;
        wire[4] = idCount;
        wire[5] = childCount;
        cursor = 6;
        if (version >= CHUNK_VERSION2) wire[cursor++] = managerCount;
    } else {
        unsigned int options = (idCount ? CHNK_OPTION_IDS : 0) |
            (childCount ? CHNK_OPTION_CHN : 0) | (managerCount ? CHNK_OPTION_MAN : 0);
        wire[0] = static_cast<int>((static_cast<unsigned int>(version) << 16) |
            (options << 24) | (CKCID_OBJECT << 8));
        wire[1] = count;
        cursor = 2;
    }
    ASSERT_LE(cursor + count + idCount + childCount + managerCount + 3, 256);
    if (count) std::memcpy(wire + cursor, words, count * sizeof(int));
    cursor += count;
    if (idCount) {
        if (version >= CHUNK_VERSION3) wire[cursor++] = idCount;
        std::memcpy(wire + cursor, ids, idCount * sizeof(int)); cursor += idCount;
    }
    if (childCount) {
        if (version >= CHUNK_VERSION3) wire[cursor++] = childCount;
        std::memcpy(wire + cursor, children, childCount * sizeof(int)); cursor += childCount;
    }
    if (managerCount) {
        if (version >= CHUNK_VERSION3) wire[cursor++] = managerCount;
        std::memcpy(wire + cursor, managers, managerCount * sizeof(int));
    }
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

void ExpectRemapWords(CKStateChunk &chunk, const int *expected, int count) {
    ASSERT_EQ(count * static_cast<int>(sizeof(int)), chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], chunk.ReadInt()) << "word " << i;
}

const int ManagerA = 0x12345678;
const int ManagerB = 0x23456789;
const CKGUID RemapGuid(ManagerA, ManagerB);

class TestRemapDependencies : public CKDependenciesContext {
public:
    explicit TestRemapDependencies(CKContext *context) : CKDependenciesContext(context) {}
    void AddMapping(CK_ID source, CK_ID target) { m_MapID.Insert(source, target); }
};

class CKStateChunkRemapTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    static void TearDownTestSuite() {
        if (context) CKCloseContext(context);
        context = nullptr;
        CKShutdown();
    }
    static CKContext *context;
};

CKContext *CKStateChunkRemapTest::context = nullptr;

TEST_F(CKStateChunkRemapTest, ModernIdsCountVisitedReferencesIncludingUnchangedAndUnmapped) {
    const int words[] = {11, 22, 33, 44, 4, 11, 22, 0, 33};
    const int ids[] = {0, 1, 2, -1, 4};
    const int expected[] = {111, 22, 33, 44, 4, 111, 22, 0, 33};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 9, ids, 5);
    TestRemapDependencies dependencies(context);
    dependencies.AddMapping(11, 111);
    dependencies.AddMapping(22, 22);
    EXPECT_EQ(7, chunk.RemapObjects(context, &dependencies));
    ExpectRemapWords(chunk, expected, 9);
}

TEST_F(CKStateChunkRemapTest, DependencyZeroTargetsPreserveOriginalReference) {
    const int words[] = {11, 22}; const int ids[] = {0, 1};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION1, words, 2, ids, 2);
    TestRemapDependencies dependencies(context);
    dependencies.AddMapping(11, 0);
    EXPECT_EQ(2, chunk.RemapObjects(context, &dependencies));
    ExpectRemapWords(chunk, words, 2);
}

TEST_F(CKStateChunkRemapTest, SingleRemapAllowsZeroSourceAndCountsAllRecordedIds) {
    const int words[] = {0, 11, 0}; const int ids[] = {0, 1, 2};
    const int expected[] = {77, 11, 77};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 3, ids, 3);
    EXPECT_EQ(3, chunk.RemapObject(0, 77));
    ExpectRemapWords(chunk, expected, 3);
    EXPECT_EQ(3, chunk.RemapObject(11, 0));
    ExpectRemapWords(chunk, expected, 3);
}

TEST_F(CKStateChunkRemapTest, ActualLoadSessionUsesCreatedIdsAndClearsMissingIds) {
    CKObject object(context, "RemapTarget");
    CKObjectManager *manager = static_cast<CKObjectManager *>(context->GetManagerByGuid(OBJECT_MANAGER_GUID));
    ASSERT_NE(nullptr, manager);
    manager->StartLoadSession(64);
    manager->RegisterLoadObject(&object, 11);
    const int words[] = {11, 22, 0, 100}; const int ids[] = {0, 1, 2, 3};
    const int expected[] = {static_cast<int>(object.GetID()), 0, 0, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 4, ids, 4);
    EXPECT_EQ(4, chunk.RemapObjects(context));
    ExpectRemapWords(chunk, expected, 4);
    manager->EndLoadSession();
}

TEST_F(CKStateChunkRemapTest, DependencyMapOverridesActualLoadSession) {
    CKObject object(context, "DependencyPriorityTarget");
    CKObjectManager *manager = static_cast<CKObjectManager *>(context->GetManagerByGuid(OBJECT_MANAGER_GUID));
    ASSERT_NE(nullptr, manager);
    manager->StartLoadSession(64); manager->RegisterLoadObject(&object, 11);
    const int words[] = {11, 22}; const int ids[] = {0, 1};
    const int expected[] = {88, 22};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 2, ids, 2);
    TestRemapDependencies dependencies(context); dependencies.AddMapping(11, 88);
    EXPECT_EQ(2, chunk.RemapObjects(context, &dependencies));
    ExpectRemapWords(chunk, expected, 2);
    manager->EndLoadSession();
}

TEST_F(CKStateChunkRemapTest, OutsideLoadSessionCountsButPreservesEveryRawId) {
    const int words[] = {11, 0, -1}; const int ids[] = {0, 1, 2};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 3, ids, 3);
    EXPECT_EQ(3, chunk.RemapObjects(context));
    ExpectRemapWords(chunk, words, 3);
}

TEST_F(CKStateChunkRemapTest, LegacyMarkersRemapSinglesAndSequencesOnly) {
    const int words[] = {static_cast<int>(0xe32bc4c9u), 0x134212e3, static_cast<int>(0xfcbae9dcu), 11,
        11, static_cast<int>(0xe192bd47u), 0x13246628, 0x13eab3ce, 0x7891aefc, 0x13984562,
        3, 11, 22, 0};
    int expected[14]; std::memcpy(expected, words, sizeof(words)); expected[3] = 111; expected[11] = 111;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSIONBASE, words, 14);
    EXPECT_EQ(4, chunk.RemapObject(11, 111));
    ExpectRemapWords(chunk, expected, 14);
}

TEST_F(CKStateChunkRemapTest, EmptyMetadataDoesNotScanUnmarkedModernIds) {
    const int words[] = {11, 11, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 3);
    EXPECT_EQ(0, chunk.RemapObject(11, 111));
    ExpectRemapWords(chunk, words, 3);
}

TEST_F(CKStateChunkRemapTest, ManagerSinglesCountValidIdentityMappingsAndPreserveInvalidValues) {
    const int words[] = {ManagerA, ManagerB, 0, ManagerA, ManagerB, 1,
        ManagerA, ManagerB, -1, ManagerA, ManagerB, 4, ManagerA + 1, ManagerB, 1};
    const int managers[] = {0, 3, 6, 9, 12}; int table[] = {0, 31};
    int expected[15]; std::memcpy(expected, words, sizeof(words)); expected[5] = 31;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 15, nullptr, 0, nullptr, 0, managers, 5);
    EXPECT_EQ(2, chunk.RemapManagerInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, expected, 15);
}

TEST_F(CKStateChunkRemapTest, ManagerSequencesCountAllMatchingEntriesAndMapOnlyValidValues) {
    const int words[] = {5, ManagerA, ManagerB, 0, 1, -1, 2, 1,
                         2, ManagerA + 1, ManagerB, 1, 0};
    const int managers[] = {-1, 0, -1, 8}; int table[] = {0, 31};
    int expected[13]; std::memcpy(expected, words, sizeof(words)); expected[4] = 31; expected[7] = 31;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION2, words, 13, nullptr, 0, nullptr, 0, managers, 4);
    EXPECT_EQ(5, chunk.RemapManagerInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, expected, 13);
}

TEST_F(CKStateChunkRemapTest, NullConversionTablesLeavePayloadAndMetadataUntouched) {
    const int words[] = {ManagerA, ManagerB, 1}; const int managers[] = {0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 3, nullptr, 0, nullptr, 0, managers, 1);
    EXPECT_EQ(0, chunk.RemapManagerInt(RemapGuid, nullptr, 2));
    EXPECT_EQ(0, chunk.RemapParameterInt(RemapGuid, nullptr, 2));
    ExpectRemapWords(chunk, words, 3);
}

TEST_F(CKStateChunkRemapTest, EmptyManagerConversionDomainStillCountsMatchingSequence) {
    const int words[] = {3, ManagerA, ManagerB, 0, 1, -1};
    const int managers[] = {-1, 0}; int table[] = {31};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 6, nullptr, 0, nullptr, 0, managers, 2);
    EXPECT_EQ(3, chunk.RemapManagerInt(RemapGuid, table, 0));
    ExpectRemapWords(chunk, words, 6);
}

TEST_F(CKStateChunkRemapTest, ParameterUsesLinkedHeaderAndKeepsByteCountAndCursor) {
    const int words[] = {0x90, 4, 0x40, 123, 0x40, 0, ManagerA, ManagerB, 1, 4, 1};
    int expected[11]; std::memcpy(expected, words, sizeof(words)); expected[10] = 31;
    int table[] = {0, 31}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 11);
    chunk.StartRead(); chunk.ReadInt(); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(1, chunk.RemapParameterInt(RemapGuid, table, 2));
    EXPECT_EQ(1, chunk.GetCurrentPos()); ExpectRemapWords(chunk, expected, 11);
}

TEST_F(CKStateChunkRemapTest, ParameterStopsAtFirstParameterHeaderEvenWhenGuidMismatches) {
    const int words[] = {0x40, 7, ManagerA + 1, ManagerB, 1, 4, 1,
                         0x40, 0, ManagerA, ManagerB, 1, 4, 1};
    int table[] = {0, 31}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 14);
    EXPECT_EQ(0, chunk.RemapParameterInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, words, 14);
}

TEST_F(CKStateChunkRemapTest, ParameterStopsAtFirstParameterHeaderEvenWhenStorageIsNotInteger) {
    const int words[] = {0x40, 7, ManagerA, ManagerB, 2, 4, 1,
                         0x40, 0, ManagerA, ManagerB, 1, 4, 1};
    int table[] = {0, 31}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 14);
    EXPECT_EQ(0, chunk.RemapParameterInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, words, 14);
}

TEST_F(CKStateChunkRemapTest, ParameterInvalidIndicesResetToZeroAndIdentityStillCounts) {
    int table[] = {0, 1};
    const int values[] = {-1, 2, 1};
    for (int i = 0; i < 3; ++i) {
        int words[] = {0x40, 0, ManagerA, ManagerB, 1, 4, values[i]};
        int expected[7]; std::memcpy(expected, words, sizeof(words)); expected[6] = i == 2 ? 1 : 0;
        CKStateChunk chunk(CKCID_OBJECT, nullptr);
        LoadRemapWire(chunk, CHUNK_VERSION4, words, 7);
        EXPECT_EQ(1, chunk.RemapParameterInt(RemapGuid, table, 2));
        ExpectRemapWords(chunk, expected, 7);
    }
}

TEST_F(CKStateChunkRemapTest, SingleAndPackedChildrenRemapAndKeepSiblingFramesSeparate) {
    // Top single child@0, then packed sequence@10 holding a null and child.
    const int words[] = {9, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, 0, 1, 0, 0, 11, 0,
                         2, 0, 9, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, 0, 1, 0, 0, 22, 0};
    const int children[] = {0, -1, 10};
    int expected[22]; std::memcpy(expected, words, sizeof(words)); expected[8] = 111; expected[20] = 222;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 22, nullptr, 0, children, 3);
    TestRemapDependencies dependencies(context); dependencies.AddMapping(11, 111); dependencies.AddMapping(22, 222);
    EXPECT_EQ(2, chunk.RemapObjects(context, &dependencies));
    ExpectRemapWords(chunk, expected, 22);
}

TEST_F(CKStateChunkRemapTest, VersionOneParentUsesSixWordChildHeaderWithoutManagerCount) {
    const int words[] = {8, CKCID_OBJECT, CHUNK_VERSION1 << 16, 1, 0, 1, 0, 11, 0};
    const int children[] = {0};
    int expected[9]; std::memcpy(expected, words, sizeof(words)); expected[7] = 111;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION1, words, 9, nullptr, 0, children, 1);
    EXPECT_EQ(1, chunk.RemapObject(11, 111));
    ExpectRemapWords(chunk, expected, 9);
}

TEST_F(CKStateChunkRemapTest, DeepNestedChildrenReceiveDependencyMappingAndAccumulateCounts) {
    const int words[] = {18, CKCID_OBJECT, CHUNK_VERSION4 << 16, 10, 0, 0, 1, 0,
                        9, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, 0, 1, 0, 0, 11, 0, 0};
    const int children[] = {0};
    int expected[19]; std::memcpy(expected, words, sizeof(words)); expected[16] = 111;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 19, nullptr, 0, children, 1);
    EXPECT_EQ(1, chunk.RemapObject(11, 111));
    ExpectRemapWords(chunk, expected, 19);
}

TEST_F(CKStateChunkRemapTest, LegacyPackedChildIsSkippedBeforeModernChild) {
    // Legacy frame's second header token+4 equals declared frame DWORD size.
    const int words[] = {2, 5, CKCID_OBJECT, 1, 0, 11, 0,
                         9, CKCID_OBJECT, CHUNK_VERSION4 << 16, 1, 0, 1, 0, 0, 22, 0};
    const int children[] = {-1, 0};
    int expected[17]; std::memcpy(expected, words, sizeof(words)); expected[15] = 222;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 17, nullptr, 0, children, 2);
    EXPECT_EQ(1, chunk.RemapObject(22, 222));
    ExpectRemapWords(chunk, expected, 17);
}

TEST_F(CKStateChunkRemapTest, RecursiveManagerChildReceivesGuidTableAndCountsIdentity) {
    const int words[] = {11, CKCID_OBJECT, CHUNK_VERSION4 << 16, 3, 0, 0, 0, 1, ManagerA, ManagerB, 0, 0};
    const int children[] = {0}; int table[] = {0, 31};
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 12, nullptr, 0, children, 1);
    EXPECT_EQ(1, chunk.RemapManagerInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, words, 12);
}

TEST_F(CKStateChunkRemapTest, RecursiveParameterChildReceivesGuidTableAndEntryCount) {
    const int words[] = {14, CKCID_PARAMETER, CHUNK_VERSION4 << 16, 7, 0, 0, 0, 0,
                         0x40, 0, ManagerA, ManagerB, 1, 4, 1};
    const int children[] = {0}; int table[] = {0, 31};
    int expected[15]; std::memcpy(expected, words, sizeof(words)); expected[14] = 31;
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadRemapWire(chunk, CHUNK_VERSION4, words, 15, nullptr, 0, children, 1);
    EXPECT_EQ(1, chunk.RemapParameterInt(RemapGuid, table, 2));
    ExpectRemapWords(chunk, expected, 15);
}

} // namespace
