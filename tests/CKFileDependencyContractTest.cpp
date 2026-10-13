#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "CKAll.h"

namespace {

const CKGUID GuidA38(0x38a10101, 0x38b20202), GuidB38(0x38c30303, 0x38d40404);
const CKGUID Missing38(0x38e50505, 0x38f60606);

CKFilePluginDependencies Value38(CKGUID first = GuidA38, CKGUID second = GuidB38) {
    CKFilePluginDependencies result; result.m_PluginCategory = CKPLUGIN_MANAGER_DLL;
    result.m_Guids.PushBack(first); result.m_Guids.PushBack(second); result.ValidGuids.Set(0);
    return result;
}

// This modern SDK adapter has no original 2.1 operator body. Its value contract
// follows the category, ordered GUIDs and the per-GUID availability exposed by
// GetMissingPlugins; bit storage beyond the GUID list is allocation state.
TEST(CKFileDependencyContractTest, ValueComparisonDistinguishesSameSizedGuidLists) {
    CKFilePluginDependencies first = Value38(), second = Value38(Missing38);
    EXPECT_FALSE(first == second); EXPECT_FALSE(second == first);
}

TEST(CKFileDependencyContractTest, ValueComparisonKeepsGuidOrderAndDuplicatePositions) {
    CKFilePluginDependencies ordered = Value38(), reversed = Value38(GuidB38, GuidA38);
    CKFilePluginDependencies repeated = Value38(GuidA38, GuidA38);
    EXPECT_FALSE(ordered == reversed); EXPECT_FALSE(ordered == repeated);
}

TEST(CKFileDependencyContractTest, ValueComparisonDistinguishesAvailabilityForTheSameGuid) {
    CKFilePluginDependencies available = Value38(), missing(available); missing.ValidGuids.Unset(0);
    EXPECT_FALSE(available == missing); EXPECT_FALSE(missing == available);
}

TEST(CKFileDependencyContractTest, ValueComparisonScopesIdenticalGuidsByCategory) {
    CKFilePluginDependencies managers = Value38(), readers(managers); readers.m_PluginCategory = CKPLUGIN_BITMAP_READER;
    EXPECT_FALSE(managers == readers);
}

TEST(CKFileDependencyContractTest, SpareValidityCapacityAndBitsDoNotChangeDependencyValue) {
    CKFilePluginDependencies first = Value38(), second(first);
    second.ValidGuids.Set(63); ASSERT_NE(first.ValidGuids.Size(), second.ValidGuids.Size());
    EXPECT_TRUE(first == second); EXPECT_TRUE(second == first);
    second.ValidGuids.Unset(63); EXPECT_TRUE(first == second);
}

TEST(CKFileDependencyContractTest, EmptyDependencyValuesIgnoreValidityStorageCapacity) {
    CKFilePluginDependencies first, second; first.m_PluginCategory = second.m_PluginCategory = CKPLUGIN_MANAGER_DLL;
    second.ValidGuids.Set(63); EXPECT_TRUE(first == second); EXPECT_TRUE(second == first);
}

TEST(CKFileDependencyContractTest, EqualDependencyCopiesRemainEqualAfterIndependentStorageGrowth) {
    CKFilePluginDependencies original = Value38(), copy(original);
    EXPECT_TRUE(original == original); EXPECT_TRUE(original == copy);
    for (int i = 0; i < 40; ++i) {
        const CKGUID guid(0x38000000, i + 1); original.m_Guids.PushBack(guid); copy.m_Guids.PushBack(guid);
    }
    copy.ValidGuids.Set(63); EXPECT_TRUE(original == copy);
    copy.ValidGuids.Set(35); EXPECT_FALSE(original == copy);
}

void Put38(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

unsigned int Read38(const unsigned char *bytes, int offset) {
    unsigned int result = 0; for (int i = 0; i < 4; ++i) result |= static_cast<unsigned int>(bytes[offset + i]) << (8 * i);
    return result;
}

struct Wire38 {
    unsigned char Bytes[1024]; int Size, Groups;
    Wire38() : Size(68), Groups(0) { std::memset(Bytes, 0, sizeof(Bytes)); }
    void Group(int category, const CKGUID *guids, int count) {
        Put38(Bytes, Size, category); Put38(Bytes, Size + 4, count); Size += 8;
        for (int i = 0; i < count; ++i) { Put38(Bytes, Size, guids[i].d1); Put38(Bytes, Size + 4, guids[i].d2); Size += 8; }
        ++Groups;
    }
    void Finish() {
        Put38(Bytes, 64, Groups); Size += 4; std::memcpy(Bytes, "Nemo Fi", 7);
        Put38(Bytes, 12, 0x13022002); Put38(Bytes, 16, 8); Put38(Bytes, 28, Size - 64);
        Put38(Bytes, 60, Size - 64); unsigned int a = 0, b = 0;
        for (int i = 0; i < Size; ++i) { a = (a + Bytes[i]) % 65521; b = (b + a) % 65521; }
        Put38(Bytes, 8, a | (b << 16));
    }
};

CKPluginInfo Plugins38[2];
int PluginCount38() { return 2; }
CKPluginInfo *PluginInfo38(int index) { return index >= 0 && index < 2 ? &Plugins38[index] : nullptr; }

class CKFileDependencyApiContractTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); Plugins38[0].m_GUID = GuidA38; Plugins38[1].m_GUID = GuidB38;
        Plugins38[0].m_Type = Plugins38[1].m_Type = CKPLUGIN_MANAGER_DLL;
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("Pass38 dependency contract", PluginCount38, PluginInfo38));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override { ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0)); }
    void TearDown() override { if (Context) CKCloseContext(Context); }
};

TEST_F(CKFileDependencyApiContractTest, LoadedGroupsPreserveCategoryOrderGuidDuplicatesAndPerPositionValidity) {
    const CKGUID managers[] = {GuidA38, Missing38, GuidA38, GuidB38}, readers[] = {GuidA38}, reversed[] = {GuidB38, GuidA38};
    Wire38 wire; wire.Group(CKPLUGIN_MANAGER_DLL, managers, 4); wire.Group(CKPLUGIN_BITMAP_READER, readers, 1);
    wire.Group(CKPLUGIN_MANAGER_DLL, reversed, 2); wire.Finish(); CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(wire.Bytes, wire.Size, CK_LOAD_CHECKDEPENDENCIES));
    XClassArray<CKFilePluginDependencies> *groups = file.GetMissingPlugins(); ASSERT_NE(nullptr, groups); ASSERT_EQ(3, groups->Size());
    EXPECT_EQ(CKPLUGIN_MANAGER_DLL, (*groups)[0].m_PluginCategory); EXPECT_EQ(CKPLUGIN_BITMAP_READER, (*groups)[1].m_PluginCategory);
    EXPECT_EQ(CKPLUGIN_MANAGER_DLL, (*groups)[2].m_PluginCategory); ASSERT_EQ(4, (*groups)[0].m_Guids.Size());
    for (int i = 0; i < 4; ++i) { EXPECT_EQ(managers[i], (*groups)[0].m_Guids[i]); EXPECT_EQ(i != 1, (*groups)[0].ValidGuids.IsSet(i) != 0); }
    EXPECT_FALSE((*groups)[1].ValidGuids.IsSet(0)); ASSERT_EQ(2, (*groups)[2].m_Guids.Size());
    EXPECT_EQ(GuidB38, (*groups)[2].m_Guids[0]); EXPECT_EQ(GuidA38, (*groups)[2].m_Guids[1]);
    EXPECT_TRUE((*groups)[2].ValidGuids.IsSet(0)); EXPECT_TRUE((*groups)[2].ValidGuids.IsSet(1));
}

TEST_F(CKFileDependencyApiContractTest, LoadedSameSizedDependencyGroupsRemainDifferentValues) {
    const CKGUID first[] = {GuidA38, Missing38}, second[] = {GuidB38, Missing38}; Wire38 wire;
    wire.Group(CKPLUGIN_MANAGER_DLL, first, 2); wire.Group(CKPLUGIN_MANAGER_DLL, second, 2); wire.Finish(); CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(wire.Bytes, wire.Size, CK_LOAD_CHECKDEPENDENCIES));
    XClassArray<CKFilePluginDependencies> *groups = file.GetMissingPlugins(); ASSERT_EQ(2, groups->Size());
    EXPECT_FALSE((*groups)[0] == (*groups)[1]);
}

TEST_F(CKFileDependencyApiContractTest, SameWireWithExplicitCheckChangesAvailabilityAndDependencyValue) {
    const CKGUID guids[] = {GuidA38, Missing38}; Wire38 wire; wire.Group(CKPLUGIN_MANAGER_DLL, guids, 2); wire.Finish();
    CKFile unchecked(Context), checked(Context); ASSERT_EQ(CK_OK, unchecked.OpenMemory(wire.Bytes, wire.Size));
    ASSERT_EQ(CKERR_PLUGINSMISSING, checked.OpenMemory(wire.Bytes, wire.Size, CK_LOAD_CHECKDEPENDENCIES));
    CKFilePluginDependencies &first = (*unchecked.GetMissingPlugins())[0], &second = (*checked.GetMissingPlugins())[0];
    EXPECT_FALSE(first.ValidGuids.IsSet(0)); EXPECT_TRUE(second.ValidGuids.IsSet(0)); EXPECT_FALSE(first == second);
}

TEST_F(CKFileDependencyApiContractTest, GuidCountAboveBitmapCapacityTreatsUnallocatedBitsAsUnavailable) {
    CKGUID guids[40]; for (int i = 0; i < 40; ++i) guids[i] = CKGUID(0x38000001, i + 1);
    Wire38 wire; wire.Group(CKPLUGIN_MANAGER_DLL, guids, 40); wire.Finish(); CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(wire.Bytes, wire.Size, CK_LOAD_CHECKDEPENDENCIES));
    CKFilePluginDependencies &loaded = (*file.GetMissingPlugins())[0]; ASSERT_EQ(40, loaded.m_Guids.Size());
    EXPECT_EQ(32, loaded.ValidGuids.Size()); CKFilePluginDependencies copy(loaded); copy.ValidGuids.Set(63);
    EXPECT_TRUE(loaded == copy); copy.ValidGuids.Set(35); EXPECT_FALSE(loaded == copy);
}

TEST_F(CKFileDependencyApiContractTest, ProducerUsesPluginRegistrationOrderAndCollapsesRepeatedManagerNeeds) {
    CKFile file(Context); file.m_IndexByClassId.Resize(128); file.m_ManagersData.Resize(3);
    for (int i = 0; i < 3; ++i) { file.m_ManagersData[i].data = nullptr; file.m_ManagersData[i].Manager = i == 1 ? GuidA38 : GuidB38; }
    CKGetPluginManager()->ComputeDependenciesList(&file); XClassArray<CKFilePluginDependencies> *groups = file.GetMissingPlugins();
    ASSERT_EQ(1, groups->Size()); EXPECT_EQ(CKPLUGIN_MANAGER_DLL, (*groups)[0].m_PluginCategory);
    ASSERT_EQ(2, (*groups)[0].m_Guids.Size()); EXPECT_EQ(GuidA38, (*groups)[0].m_Guids[0]); EXPECT_EQ(GuidB38, (*groups)[0].m_Guids[1]);
    EXPECT_FALSE((*groups)[0].ValidGuids.IsSet(0)); EXPECT_FALSE((*groups)[0].ValidGuids.IsSet(1));
}

class DependencySaveManager38 : public CKBaseManager {
public:
    bool Emit = true;
    DependencySaveManager38(CKContext *context, CKGUID guid) : CKBaseManager(context, guid, "Dependency38 manager") { context->RegisterNewManager(this); }
    CKStateChunk *SaveData(CKFile *file) override {
        if (!Emit) return nullptr;
        CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, file); chunk->StartWrite(); chunk->WriteInt(0x3838); chunk->CloseChunk(); return chunk;
    }
};

TEST_F(CKFileDependencyApiContractTest, SaveProducerWritesCategoryOrderedGuidsAndReaderDerivesAvailability) {
    DependencySaveManager38 *second = new DependencySaveManager38(Context, GuidB38);
    new DependencySaveManager38(Context, GuidA38); Context->SetFileWriteMode(CKFILE_UNCOMPRESSED);
    char temporaryName[100]; std::snprintf(temporaryName, sizeof(temporaryName), "ckfile-dependency-contract38-%p.cmo", static_cast<void *>(this));
    std::filesystem::path path = std::filesystem::temp_directory_path() / temporaryName;
    std::string name = path.string(); CKFile writer(Context); ASSERT_EQ(CK_OK, writer.StartSave(name.data()));
    ASSERT_EQ(CK_OK, writer.EndSave()); FILE *input = std::fopen(name.c_str(), "rb"); ASSERT_NE(nullptr, input);
    unsigned char bytes[4096]; const int count = static_cast<int>(std::fread(bytes, 1, sizeof(bytes), input)); std::fclose(input);
    ASSERT_GE(count, 100); EXPECT_EQ(8u, Read38(bytes, 16)); EXPECT_EQ(1u, Read38(bytes, 64));
    EXPECT_EQ(CKPLUGIN_MANAGER_DLL, static_cast<int>(Read38(bytes, 68))); EXPECT_EQ(2u, Read38(bytes, 72));
    EXPECT_EQ(GuidA38.d1, Read38(bytes, 76)); EXPECT_EQ(GuidA38.d2, Read38(bytes, 80));
    EXPECT_EQ(GuidB38.d1, Read38(bytes, 84)); EXPECT_EQ(GuidB38.d2, Read38(bytes, 88));
    EXPECT_EQ(4u, Read38(bytes, 92)); EXPECT_EQ(0u, Read38(bytes, 96)); // No validity bitmap on disk.
    CKFile unchecked(Context), checked(Context); ASSERT_EQ(CK_OK, unchecked.OpenMemory(bytes, count));
    ASSERT_EQ(CK_OK, checked.OpenMemory(bytes, count, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(1, checked.GetMissingPlugins()->Size()); EXPECT_FALSE((*unchecked.GetMissingPlugins())[0].ValidGuids.IsSet(0));
    EXPECT_TRUE((*checked.GetMissingPlugins())[0].ValidGuids.IsSet(0)); EXPECT_TRUE((*checked.GetMissingPlugins())[0].ValidGuids.IsSet(1));
    EXPECT_FALSE((*unchecked.GetMissingPlugins())[0] == (*checked.GetMissingPlugins())[0]);
    second->Emit = false; ASSERT_EQ(CK_OK, writer.StartSave(name.data())); ASSERT_EQ(CK_OK, writer.EndSave());
    {
        CKFile next(Context); ASSERT_EQ(CK_OK, next.OpenFile(name.data(), CK_LOAD_CHECKDEPENDENCIES));
        ASSERT_EQ(1, next.GetMissingPlugins()->Size()); ASSERT_EQ(1, (*next.GetMissingPlugins())[0].m_Guids.Size());
        EXPECT_EQ(GuidA38, (*next.GetMissingPlugins())[0].m_Guids[0]);
    }
    std::error_code ignored; std::filesystem::remove(path, ignored);
}

} // namespace
