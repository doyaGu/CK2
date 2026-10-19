#include <gtest/gtest.h>
#include <cstring>
#include <type_traits>

#include "CKDataReader.h"
#include "CKAll.h"
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);
int CKGet_ImageReader_PluginInfoCount();

struct DataReaderOwner {
    CKDataReader *Reader;
    explicit DataReaderOwner(int index) : Reader(CKGet_ImageReader_Reader(index)) {}
    ~DataReaderOwner() { if (Reader) Reader->Release(); }
};

static void DataReaderBmp(CKBYTE (&bytes)[62]) {
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = 'B'; bytes[1] = 'M'; bytes[2] = 62; bytes[10] = 54;
    bytes[14] = 40; bytes[18] = 2; bytes[22] = 1; bytes[26] = 1; bytes[28] = 24; bytes[34] = 8;
    const CKBYTE pixels[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(bytes + 54, pixels, sizeof(pixels));
}

class CKDataReaderContractTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("Production image readers for base contract",
            CKGet_ImageReader_PluginInfoCount, CKGet_ImageReader_PluginInfo, CKGet_ImageReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override { ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0)); }
    void TearDown() override { if (Context) CKCloseContext(Context); }

    CKPluginEntry *Entry(CKGUID guid) {
        return CKGetPluginManager()->FindComponent(guid, CKPLUGIN_BITMAP_READER);
    }
    CKStructStruct *Settings(CKGUID guid) {
        CKPluginEntry *entry = Entry(guid);
        if (!entry || !entry->m_ReadersInfo) return nullptr;
        CKParameterManager *manager = Context->GetParameterManager();
        return manager->GetStructDescByType(manager->ParameterGuidToType(entry->m_ReadersInfo->m_SettingsParameterGuid));
    }
};

TEST_F(CKDataReaderContractTest, AbstractBaseHasOneVirtualPointerAndTheCommonTypedSlots) {
    EXPECT_TRUE(std::is_abstract<CKDataReader>::value);
    EXPECT_TRUE(std::has_virtual_destructor<CKDataReader>::value);
    EXPECT_EQ(sizeof(void *), sizeof(CKDataReader));
    EXPECT_EQ(alignof(void *), alignof(CKDataReader));
    static_assert(std::is_same<decltype(&CKDataReader::Release), void (CKDataReader::*)()>::value, "Release ABI");
    static_assert(std::is_same<decltype(&CKDataReader::GetReaderInfo), CKPluginInfo *(CKDataReader::*)()>::value, "metadata ABI");
    static_assert(std::is_same<decltype(&CKDataReader::GetOptionsCount), int (CKDataReader::*)()>::value, "count ABI");
    static_assert(std::is_same<decltype(&CKDataReader::GetOptionDescription), CKSTRING (CKDataReader::*)(int)>::value, "description ABI");
    static_assert(std::is_same<decltype(&CKDataReader::GetFlags), CK_DATAREADER_FLAGS (CKDataReader::*)()>::value, "flags ABI");
    DataReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapReader *bitmap = dynamic_cast<CKBitmapReader *>(owner.Reader);
    ASSERT_NE(nullptr, bitmap);
    EXPECT_EQ(static_cast<void *>(owner.Reader), static_cast<void *>(bitmap));
}

TEST_F(CKDataReaderContractTest, FactorySelectedReadersExposeStableBorrowedPluginIdentity) {
    const CKGUID expected[] = {CKGUID(0x614a243f, 0x437b3343), CKGUID(0xbca97223, 0x48578bca), CKGUID(0x585c7216, 0x33302657)};
    const char *extensions[] = {"Bmp", "Tga", "Pcx"};
    for (int i = 0; i < 3; ++i) {
        DataReaderOwner owner(i); ASSERT_NE(nullptr, owner.Reader);
        CKPluginInfo *first = owner.Reader->GetReaderInfo();
        ASSERT_NE(nullptr, first);
        EXPECT_EQ(first, owner.Reader->GetReaderInfo());
        EXPECT_EQ(expected[i], first->m_GUID);
        EXPECT_EQ(CKPLUGIN_BITMAP_READER, first->m_Type);
        EXPECT_STREQ(extensions[i], first->m_Extension);
        ASSERT_NE(nullptr, Entry(expected[i]));
        EXPECT_EQ(first->m_GUID, Entry(expected[i])->m_PluginInfo.m_GUID);
    }
}

TEST_F(CKDataReaderContractTest, HostCachesTheActualReaderCountsAndCapabilities) {
    const CKGUID guids[] = {BMPREADER_GUID, TGAREADER_GUID, CKGUID(0x585c7216, 0x33302657)};
    const int counts[] = {1, 2, 0}, flags[] = {15, 15, 5};
    for (int i = 0; i < 3; ++i) {
        DataReaderOwner owner(i); ASSERT_NE(nullptr, owner.Reader);
        CKPluginEntry *entry = Entry(guids[i]); ASSERT_NE(nullptr, entry); ASSERT_NE(nullptr, entry->m_ReadersInfo);
        EXPECT_EQ(counts[i], owner.Reader->GetOptionsCount());
        EXPECT_EQ(counts[i], entry->m_ReadersInfo->m_OptionCount);
        EXPECT_EQ(flags[i], owner.Reader->GetFlags());
        EXPECT_EQ(flags[i], entry->m_ReadersInfo->m_ReaderFlags);
    }
}

TEST_F(CKDataReaderContractTest, HostBuildsTheBmpDepthEnumFromTheProductionDescription) {
    CKStructStruct *settings = Settings(BMPREADER_GUID);
    ASSERT_NE(nullptr, settings); ASSERT_EQ(1, settings->NbData);
    EXPECT_STREQ("Bit Depth", settings->Desc[0]);
    CKParameterManager *manager = Context->GetParameterManager();
    CKEnumStruct *depth = manager->GetEnumDescByType(manager->ParameterGuidToType(settings->Guids[0]));
    ASSERT_NE(nullptr, depth); ASSERT_EQ(5, depth->NbData);
    const int expected[] = {8, 9, 16, 24, 32};
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(expected[i], depth->Vals[i]); EXPECT_NE('\0', depth->Desc[i][0]); }
}

TEST_F(CKDataReaderContractTest, HostBuildsSeparateTgaDepthEnumAndRleBoolean) {
    CKStructStruct *settings = Settings(TGAREADER_GUID);
    ASSERT_NE(nullptr, settings); ASSERT_EQ(2, settings->NbData);
    EXPECT_STREQ("Bit Depth", settings->Desc[0]);
    EXPECT_STREQ("Run Length Encoding", settings->Desc[1]);
    EXPECT_EQ(CKPGUID_BOOL, settings->Guids[1]);
    CKParameterManager *manager = Context->GetParameterManager();
    CKEnumStruct *depth = manager->GetEnumDescByType(manager->ParameterGuidToType(settings->Guids[0]));
    ASSERT_NE(nullptr, depth); ASSERT_EQ(4, depth->NbData);
    const int expected[] = {16, 24, 32, 64};
    for (int i = 0; i < 4; ++i) EXPECT_EQ(expected[i], depth->Vals[i]);
}

TEST_F(CKDataReaderContractTest, HostTransfersIndependentOptionValuesThroughGeneratedParameters) {
    CKDWORD original[] = {32, 1};
    CKParameterOut *options = CKGetPluginManager()->GetReaderOptionData(Context, original, CKFileExtension("tga"));
    ASSERT_NE(nullptr, options);
    CKStructStruct *settings = Context->GetParameterManager()->GetStructDescByType(options->GetType());
    ASSERT_NE(nullptr, settings); ASSERT_EQ(2, settings->NbData);
    const CK_ID *ids = static_cast<const CK_ID *>(options->GetReadDataPtr()); ASSERT_NE(nullptr, ids);
    const CKDWORD replacement[] = {16, 0};
    for (int i = 0; i < 2; ++i) {
        CKParameter *member = static_cast<CKParameter *>(Context->GetObject(ids[i])); ASSERT_NE(nullptr, member);
        CKDWORD value = 0;
        ASSERT_EQ(CK_OK, member->GetValue(&value)); EXPECT_EQ(original[i], value);
        ASSERT_EQ(CK_OK, member->SetValue(&replacement[i], sizeof(CKDWORD)));
    }
    struct GuardedOptions { CKDWORD Before, Values[2], After; } output = {0xaabbccdd, {0, 0}, 0x11223344};
    ASSERT_TRUE(CKGetPluginManager()->SetReaderOptionData(Context, output.Values, options, CKFileExtension("tga")));
    EXPECT_EQ(16u, output.Values[0]); EXPECT_EQ(0u, output.Values[1]);
    EXPECT_EQ(0xaabbccddu, output.Before); EXPECT_EQ(0x11223344u, output.After);
    EXPECT_EQ(32u, original[0]); EXPECT_EQ(1u, original[1]);
}

TEST_F(CKDataReaderContractTest, ReadOnlyPcxDoesNotRegisterUnusedSaveSettings) {
    CKPluginEntry *entry = Entry(CKGUID(0x585c7216, 0x33302657));
    ASSERT_NE(nullptr, entry); ASSERT_NE(nullptr, entry->m_ReadersInfo);
    EXPECT_EQ(0, entry->m_ReadersInfo->m_OptionCount);
    EXPECT_EQ(CKGUID(), entry->m_ReadersInfo->m_SettingsParameterGuid);
    CKDWORD value = 24;
    EXPECT_EQ(nullptr, CKGetPluginManager()->GetReaderOptionData(Context, &value, CKFileExtension("pcx")));
}

TEST_F(CKDataReaderContractTest, DescriptionQueriesDoNotResetActiveConcreteSaveOptions) {
    DataReaderOwner owner(1); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapReader *bitmap = static_cast<CKBitmapReader *>(owner.Reader);
    TgaBitmapProperties options; options.m_BitDepth = 32; options.m_UseRLE = 1;
    bitmap->SetBitmapDefaultProperties(&options);
    for (int i = 0; i < owner.Reader->GetOptionsCount(); ++i) {
        CKSTRING description = owner.Reader->GetOptionDescription(i);
        ASSERT_NE(nullptr, description); EXPECT_NE(nullptr, std::strchr(description, ':'));
    }
    CKBitmapProperties *properties = nullptr; bitmap->GetBitmapDefaultProperties(&properties);
    ASSERT_NE(nullptr, properties);
    EXPECT_EQ(32u, static_cast<TgaBitmapProperties *>(properties)->m_BitDepth);
    EXPECT_EQ(1u, static_cast<TgaBitmapProperties *>(properties)->m_UseRLE);
}

TEST_F(CKDataReaderContractTest, ReleaseThroughTheBaseLeavesAnotherDecodedReaderUsable) {
    DataReaderOwner first(0), second(0); ASSERT_NE(nullptr, first.Reader); ASSERT_NE(nullptr, second.Reader);
    CKBYTE bytes[62]; DataReaderBmp(bytes);
    CKBitmapProperties *one = nullptr, *two = nullptr;
    ASSERT_EQ(0, static_cast<CKBitmapReader *>(first.Reader)->ReadMemory(bytes, sizeof(bytes), &one));
    ASSERT_EQ(0, static_cast<CKBitmapReader *>(second.Reader)->ReadMemory(bytes, sizeof(bytes), &two));
    ASSERT_NE(nullptr, one); ASSERT_NE(nullptr, two); ASSERT_NE(one->m_Data, two->m_Data);
    first.Reader->Release(); first.Reader = nullptr;
    const CKBYTE expected[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    EXPECT_EQ(0, memcmp(expected, two->m_Data, sizeof(expected)));
    EXPECT_EQ(15, second.Reader->GetFlags()); EXPECT_EQ(1, second.Reader->GetOptionsCount());
}

TEST_F(CKDataReaderContractTest, VirtualBaseDeletionOfARealReaderPreservesSharedPluginMetadata) {
    DataReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKPluginInfo *metadata = owner.Reader->GetReaderInfo(); ASSERT_NE(nullptr, metadata);
    CKBYTE bytes[62]; DataReaderBmp(bytes);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, static_cast<CKBitmapReader *>(owner.Reader)->ReadMemory(bytes, sizeof(bytes), &properties));
    ASSERT_NE(nullptr, properties); ASSERT_NE(nullptr, properties->m_Data);
    // In-process ABI coverage of the actual virtual destructor; public plugin
    // consumers normally use Release so destruction stays in the plugin CRT.
    delete owner.Reader; owner.Reader = nullptr;
    EXPECT_EQ(CKGUID(0x614a243f, 0x437b3343), metadata->m_GUID);
    EXPECT_STREQ("Bmp", metadata->m_Extension);
    ASSERT_NE(nullptr, Entry(metadata->m_GUID));
    EXPECT_EQ(metadata->m_GUID, Entry(metadata->m_GUID)->m_PluginInfo.m_GUID);
}
