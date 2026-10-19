#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <type_traits>

#include "CKBitmapReader.h"
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"
#include "../../Plugins/ImageReader/PcxReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

struct BitmapReaderOwner {
    CKBitmapReader *Reader;
    explicit BitmapReaderOwner(int index) : Reader(nullptr) {
        CKGet_ImageReader_PluginInfo(index);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~BitmapReaderOwner() { if (Reader) Reader->Release(); }
};

struct BitmapReaderTestFile {
    std::filesystem::path Path;
    BitmapReaderTestFile() {
        static unsigned int sequence = 0;
        char name[96];
        std::snprintf(name, sizeof(name), "ck-reader-%p-%u.bmp", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
    }
    ~BitmapReaderTestFile() { std::error_code error; std::filesystem::remove(Path, error); }
};

static void ReaderWriteWord(CKBYTE *bytes, int offset, unsigned int value, int count = 4) {
    for (int i = 0; i < count; ++i) bytes[offset + i] = static_cast<CKBYTE>(value >> (8 * i));
}

static void ReaderMakeBmp(CKBYTE (&bytes)[62]) {
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = 'B'; bytes[1] = 'M';
    ReaderWriteWord(bytes, 2, 62); ReaderWriteWord(bytes, 10, 54); ReaderWriteWord(bytes, 14, 40);
    ReaderWriteWord(bytes, 18, 2); ReaderWriteWord(bytes, 22, 1);
    ReaderWriteWord(bytes, 26, 1, 2); ReaderWriteWord(bytes, 28, 24, 2); ReaderWriteWord(bytes, 34, 8);
    const CKBYTE pixels[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(bytes + 54, pixels, sizeof(pixels));
}

static void ReaderExpectPixels(CKBitmapProperties *properties) {
    ASSERT_NE(nullptr, properties);
    ASSERT_NE(nullptr, properties->m_Data);
    EXPECT_EQ(2, properties->m_Format.Width); EXPECT_EQ(1, properties->m_Format.Height);
    EXPECT_EQ(32, properties->m_Format.BitsPerPixel);
    EXPECT_EQ(8, properties->m_Format.BytesPerLine);
    const CKBYTE expected[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    EXPECT_EQ(0, memcmp(expected, properties->m_Data, sizeof(expected)));
}

static void ReaderSetSavePixels(CKBitmapProperties &properties, CKBYTE *pixels) {
    properties.m_Format.Width = 2; properties.m_Format.Height = 1;
    properties.m_Format.BitsPerPixel = 32; properties.m_Format.BytesPerLine = 8;
    properties.m_Format.RedMask = 0x00ff0000; properties.m_Format.GreenMask = 0x0000ff00;
    properties.m_Format.BlueMask = 0x000000ff; properties.m_Format.AlphaMask = 0xff000000;
    properties.m_Format.Image = nullptr; properties.m_Data = pixels;
}

TEST(CKBitmapReaderContractTest, AbstractSingleBaseLayoutAndMethodSignaturesArePreserved) {
    EXPECT_TRUE((std::is_base_of<CKDataReader, CKBitmapReader>::value));
    EXPECT_TRUE(std::is_abstract<CKBitmapReader>::value);
    EXPECT_TRUE(std::has_virtual_destructor<CKBitmapReader>::value);
    EXPECT_EQ(sizeof(void *), sizeof(CKBitmapReader));
    EXPECT_EQ(alignof(void *), alignof(CKBitmapReader));
    static_assert(std::is_same<decltype(&CKBitmapReader::ReadMemory), int (CKBitmapReader::*)(void *, int, CKBitmapProperties **)>::value, "ReadMemory ABI");
    static_assert(std::is_same<decltype(&CKBitmapReader::SaveMemory), int (CKBitmapReader::*)(void **, CKBitmapProperties *)>::value, "SaveMemory ABI");
    static_assert(std::is_same<decltype(&CKBitmapReader::ReleaseMemory), void (CKBitmapReader::*)(void *)>::value, "ReleaseMemory ABI");
    BitmapReaderOwner owner(0);
    ASSERT_NE(nullptr, owner.Reader);
    CKDataReader *base = owner.Reader;
    EXPECT_EQ(static_cast<void *>(base), static_cast<void *>(owner.Reader));
    EXPECT_EQ(owner.Reader, dynamic_cast<CKBitmapReader *>(base));
}

TEST(CKBitmapReaderContractTest, BaseFlagsAdvertiseFileAndMemoryLoadAndSaveWithoutStreaming) {
    BitmapReaderOwner owner(0);
    ASSERT_NE(nullptr, owner.Reader);
    EXPECT_EQ(15, owner.Reader->CKBitmapReader::GetFlags());
    EXPECT_EQ(0, owner.Reader->CKBitmapReader::GetFlags() & (CK_DATAREADER_STREAMFILE | CK_DATAREADER_STREAMURL));
}

TEST(CKBitmapReaderContractTest, BaseAlphaPolicyDoesNotInspectOrClaimOwnershipOfProperties) {
    BitmapReaderOwner owner(0);
    ASSERT_NE(nullptr, owner.Reader);
    BmpBitmapProperties properties;
    properties.m_BitDepth = 32;
    EXPECT_EQ(FALSE, owner.Reader->CKBitmapReader::IsAlphaSaved(nullptr));
    EXPECT_EQ(FALSE, owner.Reader->CKBitmapReader::IsAlphaSaved(&properties));
    EXPECT_EQ(TRUE, owner.Reader->IsAlphaSaved(&properties));
    EXPECT_EQ(32u, properties.m_BitDepth);
}

TEST(CKBitmapReaderContractTest, ProductionReadersAdvertiseTheirImplementedCapabilities) {
    const int expected[] = {15, 15, 5};
    for (int i = 0; i < 3; ++i) {
        BitmapReaderOwner owner(i);
        ASSERT_NE(nullptr, owner.Reader);
        CKDataReader *base = owner.Reader;
        EXPECT_EQ(expected[i], base->GetFlags()) << i;
        EXPECT_NE(nullptr, base->GetReaderInfo());
    }
}

TEST(CKBitmapReaderContractTest, DefaultPropertiesAreBorrowedStableBlocksWithReaderIdentity) {
    for (int i = 0; i < 2; ++i) {
        BitmapReaderOwner owner(i);
        ASSERT_NE(nullptr, owner.Reader);
        CKBitmapProperties *first = nullptr, *second = nullptr;
        owner.Reader->GetBitmapDefaultProperties(&first);
        owner.Reader->GetBitmapDefaultProperties(&second);
        ASSERT_NE(nullptr, first);
        EXPECT_EQ(first, second);
        EXPECT_GE(first->m_Size, static_cast<int>(sizeof(CKBitmapProperties)));
        EXPECT_EQ(owner.Reader->GetReaderInfo()->m_GUID, first->m_ReaderGuid);
        EXPECT_EQ(nullptr, first->m_Data);
    }
}

TEST(CKBitmapReaderContractTest, ExtendedSaveOptionsReachTheConcreteReaderThroughTheBaseInterface) {
    BitmapReaderOwner bmp(0), tga(1);
    ASSERT_NE(nullptr, bmp.Reader); ASSERT_NE(nullptr, tga.Reader);
    BmpBitmapProperties bmpOptions; bmpOptions.m_BitDepth = 32;
    TgaBitmapProperties tgaOptions; tgaOptions.m_BitDepth = 32; tgaOptions.m_UseRLE = 1;
    bmp.Reader->SetBitmapDefaultProperties(&bmpOptions);
    tga.Reader->SetBitmapDefaultProperties(&tgaOptions);
    CKBitmapProperties *properties = nullptr;
    bmp.Reader->GetBitmapDefaultProperties(&properties);
    ASSERT_NE(nullptr, properties);
    EXPECT_EQ(32u, static_cast<BmpBitmapProperties *>(properties)->m_BitDepth);
    tga.Reader->GetBitmapDefaultProperties(&properties);
    ASSERT_NE(nullptr, properties);
    EXPECT_EQ(32u, static_cast<TgaBitmapProperties *>(properties)->m_BitDepth);
    EXPECT_EQ(1u, static_cast<TgaBitmapProperties *>(properties)->m_UseRLE);
    EXPECT_EQ(TRUE, tga.Reader->IsAlphaSaved(properties));
}

TEST(CKBitmapReaderContractTest, BmpMemoryReadReturnsZeroAndDecodesIndependentKnownBytes) {
    CKBYTE bytes[62]; ReaderMakeBmp(bytes);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    ReaderExpectPixels(properties);
}

TEST(CKBitmapReaderContractTest, TgaMemoryReadReturnsZeroAndDecodesIndependentKnownBytes) {
    CKBYTE bytes[24] = {};
    bytes[2] = 2; bytes[12] = 2; bytes[14] = 1; bytes[16] = 24; bytes[17] = 0x20;
    const CKBYTE pixels[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(bytes + 18, pixels, sizeof(pixels));
    BitmapReaderOwner owner(1); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    ReaderExpectPixels(properties);
}

TEST(CKBitmapReaderContractTest, PcxMemoryReadUsesItsLoadOnlyInterfaceAndKnownPlanarBytes) {
    CKBYTE bytes[134] = {};
    bytes[0] = 10; bytes[1] = 5; bytes[2] = 1; bytes[3] = 8; bytes[8] = 1;
    bytes[65] = 3; bytes[66] = 2; bytes[68] = 1;
    const CKBYTE planes[] = {0x33, 0x66, 0x22, 0x55, 0x11, 0x44};
    memcpy(bytes + 128, planes, sizeof(planes));
    BitmapReaderOwner owner(2); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    ReaderExpectPixels(properties);
}

TEST(CKBitmapReaderContractTest, FileReadUsesTheSameZeroSuccessConventionAndDecodedPixelContract) {
    CKBYTE bytes[62]; ReaderMakeBmp(bytes);
    BitmapReaderTestFile file;
    FILE *stream = std::fopen(file.Path.string().c_str(), "wb");
    ASSERT_NE(nullptr, stream);
    const size_t written = std::fwrite(bytes, 1, sizeof(bytes), stream);
    EXPECT_EQ(0, std::fclose(stream)); ASSERT_EQ(sizeof(bytes), written);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadFile(file.Path.string().c_str(), &properties));
    ReaderExpectPixels(properties);
}

TEST(CKBitmapReaderContractTest, TruncatedInputReturnsAnErrorAndTheReaderCanStillReadAValidImage) {
    CKBYTE invalid[3] = {'B', 'M', 0}, bytes[62]; ReaderMakeBmp(bytes);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    EXPECT_NE(0, owner.Reader->ReadMemory(invalid, sizeof(invalid), &properties));
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    ReaderExpectPixels(properties);
}

TEST(CKBitmapReaderContractTest, UnsupportedAsynchronousReadDoesNotReportFalseSuccess) {
    for (int i = 0; i < 3; ++i) {
        BitmapReaderOwner owner(i); ASSERT_NE(nullptr, owner.Reader);
        CKBitmapProperties *properties = nullptr;
        EXPECT_EQ(CKBITMAPERROR_UNSUPPORTEDFUNCTION, owner.Reader->ReadASynchronousFile("unused.bmp", &properties));
        EXPECT_EQ(nullptr, properties);
    }
}

TEST(CKBitmapReaderContractTest, SaveMemoryReturnsByteCountAndIndependentBmpFileBytes) {
    CKBYTE pixels[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    BmpBitmapProperties properties; ReaderSetSavePixels(properties, pixels);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    void *memory = nullptr;
    const int count = owner.Reader->SaveMemory(&memory, &properties);
    ASSERT_GT(count, 0); ASSERT_NE(nullptr, memory);
    CKBYTE expected[62]; ReaderMakeBmp(expected);
    EXPECT_EQ(sizeof(expected), count);
    if (count == sizeof(expected)) {
        const CKBYTE *bytes = static_cast<CKBYTE *>(memory);
        EXPECT_EQ(0, memcmp(expected, bytes, 38));
        EXPECT_EQ(0, memcmp(expected + 54, bytes + 54, 8));
    }
    owner.Reader->ReleaseMemory(memory);
    EXPECT_EQ(0x11, pixels[0]); EXPECT_EQ(pixels, properties.m_Data);
}

TEST(CKBitmapReaderContractTest, SaveFileReturnsBytesWrittenAndPreservesBorrowedSourcePixels) {
    CKBYTE pixels[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    BmpBitmapProperties properties; ReaderSetSavePixels(properties, pixels);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    BitmapReaderTestFile file;
    EXPECT_EQ(62, owner.Reader->SaveFile(file.Path.string().c_str(), &properties));
    FILE *stream = std::fopen(file.Path.string().c_str(), "rb");
    ASSERT_NE(nullptr, stream);
    CKBYTE bytes[63] = {}, expected[62]; ReaderMakeBmp(expected);
    const size_t count = std::fread(bytes, 1, sizeof(bytes), stream);
    EXPECT_EQ(0, std::fclose(stream));
    EXPECT_EQ(sizeof(expected), count);
    EXPECT_EQ(0, memcmp(expected, bytes, 38));
    EXPECT_EQ(0, memcmp(expected + 54, bytes + 54, 8));
    EXPECT_EQ(pixels, properties.m_Data); EXPECT_EQ(0x66, pixels[6]);
}

TEST(CKBitmapReaderContractTest, PcxUnsupportedSaveReturnsZeroAndNoEncodedBuffer) {
    BitmapReaderOwner owner(2); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties properties;
    void *memory = &properties;
    EXPECT_EQ(0, owner.Reader->SaveMemory(&memory, &properties));
    EXPECT_EQ(nullptr, memory);
    BitmapReaderTestFile file;
    EXPECT_EQ(0, owner.Reader->SaveFile(file.Path.string().c_str(), &properties));
    EXPECT_FALSE(std::filesystem::exists(file.Path));
}

TEST(CKBitmapReaderContractTest, DecodedStorageIsReleasedByItsReaderBeforeReleasingTheReaderItself) {
    CKBYTE bytes[62]; ReaderMakeBmp(bytes);
    BitmapReaderOwner owner(0); ASSERT_NE(nullptr, owner.Reader);
    CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    ASSERT_NE(nullptr, properties); ASSERT_NE(nullptr, properties->m_Data);
    owner.Reader->ReleaseMemory(properties->m_Data);
    properties->m_Data = nullptr; properties->m_Format.Image = nullptr;
    owner.Reader->ReleaseMemory(nullptr);
    // Clearing the borrowed view prevents a second release when its reader dies.
    CKDataReader *base = owner.Reader;
    owner.Reader = nullptr;
    base->Release();
}
