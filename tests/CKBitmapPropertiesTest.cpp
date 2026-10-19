#include <gtest/gtest.h>
#include <climits>
#include <cstddef>
#include <new>
#include <type_traits>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "../include/CKBitmapReader.h"
#include "CKGlobals.h"

struct BitmapPropertyOptions : CKBitmapProperties {
    CKDWORD Quality;
    CKBYTE Tail[13];
    BitmapPropertyOptions() : Quality(0x12345678) {
        m_Size = sizeof(BitmapPropertyOptions);
        for (int i = 0; i < 13; ++i) Tail[i] = static_cast<CKBYTE>(0x80 + i);
    }
};

TEST(CKBitmapPropertiesTest, LayoutMatchesNativeWin32FieldsAndPortableX64Pointers) {
    EXPECT_TRUE(std::is_standard_layout<CKBitmapProperties>::value);
    EXPECT_FALSE(std::is_polymorphic<CKBitmapProperties>::value);
    EXPECT_FALSE(std::has_virtual_destructor<CKBitmapProperties>::value);
    EXPECT_EQ(4u, sizeof(int)); EXPECT_EQ(8u, sizeof(CKGUID)); EXPECT_EQ(4u, sizeof(CKFileExtension));
    EXPECT_EQ(0u, offsetof(CKBitmapProperties, m_Size)); EXPECT_EQ(4u, offsetof(CKBitmapProperties, m_ReaderGuid));
    EXPECT_EQ(12u, offsetof(CKBitmapProperties, m_Ext)); EXPECT_EQ(16u, offsetof(CKBitmapProperties, m_Format));
    EXPECT_EQ(sizeof(void *) == 4 ? 52u : 64u, sizeof(VxImageDescEx));
    EXPECT_EQ(sizeof(void *) == 4 ? 68u : 80u, offsetof(CKBitmapProperties, m_Data));
    EXPECT_EQ(sizeof(void *) == 4 ? 72u : 88u, sizeof(CKBitmapProperties));
    EXPECT_EQ(sizeof(void *), alignof(CKBitmapProperties));
}

TEST(CKBitmapPropertiesTest, DefaultConstructorInitializesHostSizeAndAllSemanticFields) {
    CKBitmapProperties properties;
    EXPECT_EQ(sizeof(CKBitmapProperties), properties.m_Size); EXPECT_EQ(nullptr, properties.m_Data);
    EXPECT_EQ(0u, properties.m_ReaderGuid.d1); EXPECT_EQ(0u, properties.m_ReaderGuid.d2);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(0, properties.m_Ext.m_Data[i]);
    EXPECT_EQ(sizeof(VxImageDescEx), properties.m_Format.Size);
    EXPECT_EQ(0u, properties.m_Format.Flags); EXPECT_EQ(0, properties.m_Format.Width); EXPECT_EQ(0, properties.m_Format.Height);
    EXPECT_EQ(0, properties.m_Format.BytesPerLine); EXPECT_EQ(0, properties.m_Format.BitsPerPixel);
    EXPECT_EQ(0u, properties.m_Format.RedMask); EXPECT_EQ(0u, properties.m_Format.GreenMask);
    EXPECT_EQ(0u, properties.m_Format.BlueMask); EXPECT_EQ(0u, properties.m_Format.AlphaMask);
    EXPECT_EQ(0, properties.m_Format.BytesPerColorEntry); EXPECT_EQ(0, properties.m_Format.ColorMapEntries);
    EXPECT_EQ(nullptr, properties.m_Format.ColorMap); EXPECT_EQ(nullptr, properties.m_Format.Image);
}

TEST(CKBitmapPropertiesTest, ConstructionOverDirtyStorageDoesNotDependOnZeroFilledAllocation) {
    alignas(CKBitmapProperties) CKBYTE storage[sizeof(CKBitmapProperties)]; memset(storage, 0xa5, sizeof(storage));
    CKBitmapProperties *properties = new (storage) CKBitmapProperties;
    EXPECT_EQ(sizeof(CKBitmapProperties), properties->m_Size); EXPECT_EQ(nullptr, properties->m_Data);
    EXPECT_EQ(CKGUID(), properties->m_ReaderGuid); EXPECT_EQ(0, properties->m_Ext.m_Data[0]);
    EXPECT_EQ(sizeof(VxImageDescEx), properties->m_Format.Size); EXPECT_EQ(0, properties->m_Format.Width);
    EXPECT_EQ(nullptr, properties->m_Format.ColorMap); EXPECT_EQ(nullptr, properties->m_Format.Image);
    properties->~CKBitmapProperties();
}

TEST(CKBitmapPropertiesTest, NestedDescriptorIsInitializedRatherThanAnAllZeroBlob) {
    CKBitmapProperties properties;
    EXPECT_NE(0, properties.m_Format.Size); EXPECT_EQ(sizeof(VxImageDescEx), properties.m_Format.Size);
    EXPECT_EQ(sizeof(CKBitmapProperties), properties.m_Size);
    EXPECT_EQ(0, properties.m_Format.BitsPerPixel); EXPECT_EQ(0, properties.m_Format.BytesPerLine);
}

TEST(CKBitmapPropertiesTest, ExtendedOptionsMustExplicitlyUpdateTheReportedSize) {
    struct InheritedOptions : CKBitmapProperties { CKDWORD Extra = 0x76543210; } inherited;
    EXPECT_GT(sizeof(inherited), sizeof(CKBitmapProperties)); EXPECT_EQ(sizeof(CKBitmapProperties), inherited.m_Size);
    BitmapPropertyOptions options; EXPECT_EQ(sizeof(BitmapPropertyOptions), options.m_Size);
    EXPECT_GT(options.m_Size, static_cast<int>(sizeof(CKBitmapProperties))); EXPECT_LT(options.m_Size, 256);
    EXPECT_EQ(0x12345678u, options.Quality); EXPECT_EQ(0x8c, options.Tail[12]);
}

TEST(CKBitmapPropertiesTest, GuidAndExtensionRemainSeparateOpaqueReaderIdentityFields) {
    CKBitmapProperties properties; properties.m_ReaderGuid = CKGUID(0x11223344, 0xaabbccdd); properties.m_Ext = CKFileExtension(".PNG");
    const CKBYTE expected[] = {'P', 'N', 'G', 0}; EXPECT_EQ(0, memcmp(expected, properties.m_Ext.m_Data, 4));
    EXPECT_EQ(0x11223344u, properties.m_ReaderGuid.d1); EXPECT_EQ(0xaabbccddu, properties.m_ReaderGuid.d2);
    EXPECT_EQ(nullptr, properties.m_Data); EXPECT_EQ(0, properties.m_Format.Width);
}

TEST(CKBitmapPropertiesTest, DataAndDescriptorImageAreIndependentBorrowedPointers) {
    CKBYTE pixels[16] = {}, image[16] = {}, palette[8] = {};
    CKBitmapProperties properties; properties.m_Data = pixels; properties.m_Format.Image = image;
    properties.m_Format.ColorMap = palette; properties.m_Format.ColorMapEntries = 2; properties.m_Format.BytesPerColorEntry = 4;
    EXPECT_EQ(pixels, properties.m_Data); EXPECT_EQ(image, properties.m_Format.Image);
    EXPECT_EQ(palette, properties.m_Format.ColorMap); EXPECT_NE(properties.m_Data, properties.m_Format.Image);
    properties.m_Data = nullptr; EXPECT_EQ(image, properties.m_Format.Image); EXPECT_EQ(palette, properties.m_Format.ColorMap);
}

TEST(CKBitmapPropertiesTest, DestructionDoesNotReleaseCallerOwnedPixelsOrPalette) {
    EXPECT_TRUE(std::is_trivially_destructible<CKBitmapProperties>::value);
    CKBYTE *pixels = new CKBYTE[16]{0x12}; CKBYTE *palette = new CKBYTE[8]{0x34};
    { CKBitmapProperties properties; properties.m_Data = pixels; properties.m_Format.Image = pixels; properties.m_Format.ColorMap = palette; }
    EXPECT_EQ(0x12, pixels[0]); EXPECT_EQ(0x34, palette[0]); pixels[15] = 0x56; palette[7] = 0x78;
    EXPECT_EQ(0x56, pixels[15]); EXPECT_EQ(0x78, palette[7]); delete[] pixels; delete[] palette;
}

TEST(CKBitmapPropertiesTest, MemberCopyAndAssignmentPreserveValuesWithoutOwningPointerTargets) {
    EXPECT_TRUE(std::is_trivially_copyable<CKBitmapProperties>::value);
    CKBYTE pixels[16] = {}, palette[8] = {}; CKBitmapProperties original;
    original.m_ReaderGuid = CKGUID(1, 2); original.m_Ext = CKFileExtension("tga"); original.m_Format.Width = 2;
    original.m_Format.Height = 2; original.m_Format.BitsPerPixel = 32; original.m_Data = pixels;
    original.m_Format.Image = pixels; original.m_Format.ColorMap = palette;
    CKBitmapProperties copied(original), assigned; assigned = original;
    EXPECT_EQ(sizeof(CKBitmapProperties), copied.m_Size); EXPECT_EQ(CKGUID(1, 2), copied.m_ReaderGuid);
    EXPECT_EQ(pixels, copied.m_Data); EXPECT_EQ(palette, copied.m_Format.ColorMap); EXPECT_EQ(pixels, assigned.m_Format.Image);
    copied.m_Format.Width = 7; assigned.m_Ext = CKFileExtension("bmp");
    EXPECT_EQ(2, original.m_Format.Width); EXPECT_STREQ("tga", original.m_Ext.m_Data); EXPECT_STREQ("tga", copied.m_Ext.m_Data);
}

// These are real fixed-SDK dependency calls. They validate the property block
// protocol here, without claiming that current CKGlobals.cpp was recompiled.
TEST(CKBitmapPropertiesTest, SdkCopyRejectsNullAndNonpositiveOrOversizedBlocks) {
    EXPECT_EQ(nullptr, CKCopyBitmapProperties(nullptr)); CKBitmapProperties properties;
    const int sizes[] = {INT_MIN, -1, 0, 256, 257, INT_MAX};
    for (int i = 0; i < 6; ++i) { properties.m_Size = sizes[i]; EXPECT_EQ(nullptr, CKCopyBitmapProperties(&properties)); }
}

TEST(CKBitmapPropertiesTest, SdkCopyPreservesExtendedOptionBytesAndShallowPointerValues) {
    alignas(BitmapPropertyOptions) CKBYTE storage[sizeof(BitmapPropertyOptions)]; memset(storage, 0xa5, sizeof(storage));
    BitmapPropertyOptions *original = new (storage) BitmapPropertyOptions; CKBYTE pixels[16] = {}, palette[8] = {};
    original->m_ReaderGuid = CKGUID(0x11223344, 0xaabbccdd); original->m_Ext = CKFileExtension("tga");
    original->m_Format.Width = 2; original->m_Format.Height = 2; original->m_Format.BytesPerLine = 8;
    original->m_Format.BitsPerPixel = 32; original->m_Data = pixels; original->m_Format.Image = pixels; original->m_Format.ColorMap = palette;
    CKBitmapProperties *copy = CKCopyBitmapProperties(original); ASSERT_NE(nullptr, copy); EXPECT_NE(original, copy);
    EXPECT_EQ(0, memcmp(storage, copy, sizeof(storage))); EXPECT_EQ(pixels, copy->m_Data); EXPECT_EQ(palette, copy->m_Format.ColorMap);
    original->Quality = 0; original->Tail[12] = 0; const size_t qualityOffset = reinterpret_cast<CKBYTE *>(&original->Quality) - storage;
    const size_t tailOffset = reinterpret_cast<CKBYTE *>(original->Tail) - storage;
    CKDWORD quality = 0; memcpy(&quality, reinterpret_cast<CKBYTE *>(copy) + qualityOffset, 4);
    EXPECT_EQ(0x12345678u, quality); EXPECT_EQ(0x8c, reinterpret_cast<CKBYTE *>(copy)[tailOffset + 12]);
    CKDeleteBitmapProperties(copy); original->~BitmapPropertyOptions(); pixels[15] = 0x56; palette[7] = 0x78;
    EXPECT_EQ(0x56, pixels[15]); EXPECT_EQ(0x78, palette[7]);
}

TEST(CKBitmapPropertiesTest, SdkCopyAcceptsTheFullPositiveByteBlockRangeWithoutConstructingATail) {
    alignas(CKBitmapProperties) CKBYTE storage[256];
    const int sizes[] = {1, 4, 7, static_cast<int>(sizeof(CKBitmapProperties)) - 1, static_cast<int>(sizeof(CKBitmapProperties)), 255};
    for (int i = 0; i < 6; ++i) {
        memset(storage, 0x5a, sizeof(storage)); CKBitmapProperties *original = new (storage) CKBitmapProperties; original->m_Size = sizes[i];
        CKBitmapProperties *copy = CKCopyBitmapProperties(original); ASSERT_NE(nullptr, copy);
        // Short blocks are bytes, not complete CKBitmapProperties objects.
        EXPECT_EQ(0, memcmp(storage, copy, sizes[i])); CKDeleteBitmapProperties(copy); original->~CKBitmapProperties();
    }
}

TEST(CKBitmapPropertiesTest, SdkDeletingAPropertyBlockDoesNotDestroyItsBorrowedData) {
    CKBYTE *pixels = new CKBYTE[16]{0x21}; CKBYTE *palette = new CKBYTE[8]{0x43};
    CKBitmapProperties original; original.m_Data = pixels; original.m_Format.Image = pixels; original.m_Format.ColorMap = palette;
    CKBitmapProperties *copy = CKCopyBitmapProperties(&original); ASSERT_NE(nullptr, copy); CKDeleteBitmapProperties(copy);
    EXPECT_EQ(0x21, pixels[0]); EXPECT_EQ(0x43, palette[0]); pixels[15] = 0x65; palette[7] = 0x87;
    EXPECT_EQ(pixels, original.m_Data); EXPECT_EQ(palette, original.m_Format.ColorMap);
    delete[] pixels; delete[] palette; CKDeleteBitmapProperties(nullptr);
}

TEST(CKBitmapPropertiesTest, FormatFlagsGeometryAndMasksRemainExactWhenCopied) {
    CKBitmapProperties original; original.m_Format.Flags = 0x80000001; original.m_Format.Width = 3; original.m_Format.Height = 2;
    original.m_Format.BytesPerLine = 16; original.m_Format.BitsPerPixel = 32;
    original.m_Format.RedMask = 0x00ff0000; original.m_Format.GreenMask = 0x0000ff00; original.m_Format.BlueMask = 0x000000ff; original.m_Format.AlphaMask = 0xff000000;
    original.m_Format.BytesPerColorEntry = 4; original.m_Format.ColorMapEntries = 2;
    CKBitmapProperties *copy = CKCopyBitmapProperties(&original); ASSERT_NE(nullptr, copy);
    EXPECT_EQ(0x80000001u, copy->m_Format.Flags); EXPECT_EQ(3, copy->m_Format.Width); EXPECT_EQ(2, copy->m_Format.Height);
    EXPECT_EQ(16, copy->m_Format.BytesPerLine); EXPECT_EQ(32, copy->m_Format.BitsPerPixel);
    EXPECT_EQ(0x00ff0000u, copy->m_Format.RedMask); EXPECT_EQ(0x0000ff00u, copy->m_Format.GreenMask); EXPECT_EQ(0x000000ffu, copy->m_Format.BlueMask); EXPECT_EQ(0xff000000u, copy->m_Format.AlphaMask);
    EXPECT_EQ(4, copy->m_Format.BytesPerColorEntry); EXPECT_EQ(2, copy->m_Format.ColorMapEntries); CKDeleteBitmapProperties(copy);
}
