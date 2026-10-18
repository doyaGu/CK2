#include <gtest/gtest.h>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include "CKAll.h"

static const CKGUID KnownGuid(0x37a10101, 0x37b20202);
static const CKGUID MissingGuid(0x37c30303, 0x37d40404);

TEST(CKFilePluginDependenciesTest, PortableLayoutPreservesTheThreeNativeFieldPositions) {
    EXPECT_TRUE(std::is_standard_layout<CKFilePluginDependencies>::value);
    EXPECT_EQ(0u, offsetof(CKFilePluginDependencies, m_PluginCategory));
    EXPECT_EQ(sizeof(void *) == 8 ? 8u : 4u, offsetof(CKFilePluginDependencies, m_Guids));
    EXPECT_EQ(sizeof(void *) == 8 ? 32u : 16u, offsetof(CKFilePluginDependencies, ValidGuids));
    EXPECT_EQ(sizeof(void *) == 8 ? 48u : 24u, sizeof(CKFilePluginDependencies));
    EXPECT_EQ(alignof(void *), alignof(CKFilePluginDependencies));
}

TEST(CKFilePluginDependenciesTest, DefaultConstructionCreatesEmptyGuidsAndClearValidityCapacity) {
    CKFilePluginDependencies dependency;
    EXPECT_EQ(0, dependency.m_Guids.Size());
    EXPECT_EQ(32, dependency.ValidGuids.Size());
    for (int i = 0; i < dependency.ValidGuids.Size(); ++i)
        EXPECT_FALSE(dependency.ValidGuids.IsSet(i));
    // The native constructor does not initialize m_PluginCategory.
}

static void Populate(CKFilePluginDependencies &dependency, int count = 2) {
    dependency.m_PluginCategory = CKPLUGIN_MANAGER_DLL;
    for (int i = 0; i < count; ++i)
        dependency.m_Guids.PushBack(CKGUID(0x37000001, i + 1));
    dependency.ValidGuids.Set(0);
    if (count > 32) dependency.ValidGuids.Set(32);
}

TEST(CKFilePluginDependenciesTest, CopyConstructionOwnsIndependentGuidAndValidityStorage) {
    CKFilePluginDependencies original;
    Populate(original, 40);
    CKFilePluginDependencies copy(original);
    EXPECT_EQ(original.m_PluginCategory, copy.m_PluginCategory);
    ASSERT_EQ(40, copy.m_Guids.Size());
    EXPECT_NE(original.m_Guids.Begin(), copy.m_Guids.Begin());
    original.m_Guids[0] = MissingGuid;
    original.ValidGuids.Unset(0);
    original.ValidGuids.Unset(32);
    EXPECT_EQ(CKGUID(0x37000001, 1), copy.m_Guids[0]);
    EXPECT_TRUE(copy.ValidGuids.IsSet(0));
    EXPECT_TRUE(copy.ValidGuids.IsSet(32));
}

TEST(CKFilePluginDependenciesTest, AssignmentCopiesEveryFieldAndRemainsValidAfterSourceDestruction) {
    CKFilePluginDependencies copy;
    Populate(copy);
    copy.ValidGuids.Set(63);
    {
        CKFilePluginDependencies source;
        Populate(source, 40);
        source.m_PluginCategory = CKPLUGIN_BEHAVIOR_DLL;
        copy = source;
        copy = copy;
    }
    EXPECT_EQ(CKPLUGIN_BEHAVIOR_DLL, copy.m_PluginCategory);
    ASSERT_EQ(40, copy.m_Guids.Size());
    EXPECT_EQ(CKGUID(0x37000001, 40), copy.m_Guids[39]);
    EXPECT_TRUE(copy.ValidGuids.IsSet(0));
    EXPECT_TRUE(copy.ValidGuids.IsSet(32));
    EXPECT_FALSE(copy.ValidGuids.IsSet(63));
}

TEST(CKFilePluginDependenciesTest, ProductionPushBackKeepsDifferentGuidRecordsWhenCountsMatch) {
    XClassArray<CKFilePluginDependencies> list;
    for (int i = 0; i < 17; ++i) {
        CKFilePluginDependencies entry;
        entry.m_PluginCategory = CKPLUGIN_MANAGER_DLL;
        entry.m_Guids.PushBack(CKGUID(0x37110000, i + 1));
        if (i & 1) entry.ValidGuids.Set(0);
        list.PushBack(entry); // Real consumers append; they do not call equality.
    }
    ASSERT_EQ(17, list.Size());
    for (int i = 0; i < list.Size(); ++i) {
        EXPECT_EQ(CKGUID(0x37110000, i + 1), list[i].m_Guids[0]);
        EXPECT_EQ((i & 1) != 0, list[i].ValidGuids.IsSet(0) != 0);
    }
}

static void PutWord(unsigned char *bytes, int offset, unsigned int value) {
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

struct CKTestDependencyFrame {
    unsigned char Bytes[512];
    int Size;
    CKTestDependencyFrame(int category, const CKGUID *guids, int count) : Size(80 + count * 8) {
        std::memset(Bytes, 0, sizeof(Bytes));
        std::memcpy(Bytes, "Nemo Fi", 7);
        PutWord(Bytes, 12, 0x13022002);
        PutWord(Bytes, 16, 8);
        PutWord(Bytes, 28, Size - 64);
        PutWord(Bytes, 56, 0x01010000);
        PutWord(Bytes, 60, Size - 64);
        PutWord(Bytes, 64, 1);
        PutWord(Bytes, 68, category);
        PutWord(Bytes, 72, count);
        for (int i = 0; i < count; ++i) {
            PutWord(Bytes, 76 + i * 8, guids[i].d1);
            PutWord(Bytes, 80 + i * 8, guids[i].d2);
        }
        // Dependency count/category/count/GUIDs followed by an empty inclusion table.
        unsigned int a = 0, b = 0;
        for (int i = 0; i < Size; ++i) {
            a = (a + Bytes[i]) % 65521;
            b = (b + a) % 65521;
        }
        PutWord(Bytes, 8, a | (b << 16));
    }
};

static CKPluginInfo PluginInfo;
static int PluginCount() { return 1; }
static CKPluginInfo *PluginDetails(int index) { return index == 0 ? &PluginInfo : nullptr; }

class CKFilePluginDependencyLoadTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        PluginInfo.m_GUID = KnownGuid;
        PluginInfo.m_Type = CKPLUGIN_MANAGER_DLL;
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin(
            "Pass37 dependency contract", PluginCount, PluginDetails));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override { ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0)); }
    void TearDown() override { if (Context) CKCloseContext(Context); }
};

TEST_F(CKFilePluginDependencyLoadTest, MissingPluginListPreservesCategoryOrderAndEveryGuid) {
    const CKGUID guids[] = {MissingGuid, KnownGuid, CKGUID(0x37e50505, 0x37f60606)};
    CKTestDependencyFrame frame(CKPLUGIN_MANAGER_DLL, guids, 3);
    CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    XClassArray<CKFilePluginDependencies> *list = file.GetMissingPlugins();
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(1, list->Size());
    CKFilePluginDependencies &dependency = (*list)[0];
    EXPECT_EQ(CKPLUGIN_MANAGER_DLL, dependency.m_PluginCategory);
    ASSERT_EQ(3, dependency.m_Guids.Size());
    for (int i = 0; i < 3; ++i) EXPECT_EQ(guids[i], dependency.m_Guids[i]);
    EXPECT_FALSE(dependency.ValidGuids.IsSet(0));
    EXPECT_TRUE(dependency.ValidGuids.IsSet(1));
    EXPECT_FALSE(dependency.ValidGuids.IsSet(2));
}

TEST_F(CKFilePluginDependencyLoadTest, ValidityIsComputedOnlyWhenDependencyCheckWasRequested) {
    CKTestDependencyFrame frame(CKPLUGIN_MANAGER_DLL, &KnownGuid, 1);
    CKFile file(Context);
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size));
    ASSERT_EQ(1, file.GetMissingPlugins()->Size());
    EXPECT_FALSE((*file.GetMissingPlugins())[0].ValidGuids.IsSet(0));
    ASSERT_EQ(CK_OK, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(1, file.GetMissingPlugins()->Size());
    EXPECT_TRUE((*file.GetMissingPlugins())[0].ValidGuids.IsSet(0));
}

TEST_F(CKFilePluginDependencyLoadTest, CategoryScopesThePluginLookupEvenWhenGuidIsRegistered) {
    CKTestDependencyFrame frame(CKPLUGIN_BITMAP_READER, &KnownGuid, 1);
    CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(1, file.GetMissingPlugins()->Size());
    EXPECT_EQ(CKPLUGIN_BITMAP_READER, (*file.GetMissingPlugins())[0].m_PluginCategory);
    EXPECT_FALSE((*file.GetMissingPlugins())[0].ValidGuids.IsSet(0));
}

TEST_F(CKFilePluginDependencyLoadTest, GuidCountAndBitStorageCapacityRemainDistinct) {
    CKGUID guids[40];
    for (int i = 0; i < 40; ++i) guids[i] = CKGUID(0x37f70000, i + 1);
    guids[35] = KnownGuid;
    CKTestDependencyFrame frame(CKPLUGIN_MANAGER_DLL, guids, 40);
    CKFile file(Context);
    ASSERT_EQ(CKERR_PLUGINSMISSING, file.OpenMemory(frame.Bytes, frame.Size, CK_LOAD_CHECKDEPENDENCIES));
    ASSERT_EQ(1, file.GetMissingPlugins()->Size());
    CKFilePluginDependencies &dependency = (*file.GetMissingPlugins())[0];
    ASSERT_EQ(40, dependency.m_Guids.Size());
    EXPECT_GE(dependency.ValidGuids.Size(), 40);
    EXPECT_TRUE(dependency.ValidGuids.IsSet(35));
    EXPECT_FALSE(dependency.ValidGuids.IsSet(34));
    EXPECT_FALSE(dependency.ValidGuids.IsSet(39));
}
