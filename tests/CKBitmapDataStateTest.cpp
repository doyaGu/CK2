#include <gtest/gtest.h>
#include <climits>
#include <cstddef>
#include <type_traits>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"

// No chunk serialization is exercised by this fixture.
CKSTRING CKJustFile(CKSTRING) {
    ADD_FAILURE() << "Unexpected chunk serialization";
    return nullptr;
}

static void StateImage(CKBitmapData &bitmap, int slot = 0) {
    ASSERT_TRUE(bitmap.CreateImage(3, 2, 32, slot));
    CKDWORD *pixels = reinterpret_cast<CKDWORD *>(bitmap.LockSurfacePtr(slot));
    ASSERT_NE(nullptr, pixels);
    const CKDWORD colors[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f010, 0x12345678, 0x9abcdef0};
    memcpy(pixels, colors, sizeof(colors));
}

class StateMovieReader : public CKMovieReader {
public:
    int Reads = 0, Releases = 0, Counts = 0;
    void Release() override { ++Releases; }
    CKPluginInfo *GetReaderInfo() override { return nullptr; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    int GetMovieFrameCount() override { ++Counts; return 5; }
    int GetMovieLength() override { return 100; }
    CKERROR OpenFile(CKSTRING) override { ADD_FAILURE(); return CKERR_NOTIMPLEMENTED; }
    CKERROR OpenMemory(CKSTRING) override { ADD_FAILURE(); return CKERR_NOTIMPLEMENTED; }
    CKERROR OpenAsynchronousFile(CKSTRING) override { ADD_FAILURE(); return CKERR_NOTIMPLEMENTED; }
    CKERROR ReadFrame(int, CKMovieProperties **) override { ++Reads; ADD_FAILURE(); return CKERR_NOTIMPLEMENTED; }
};

static void StateMovie(CKBitmapData &bitmap, StateMovieReader &reader) {
    CKMovieInfo *info = new CKMovieInfo(XString("state.movie"));
    info->m_MovieReader = &reader;
    info->m_MovieCurrentSlot = 3;
    bitmap.SetMovieInfo(info);
}

TEST(CKBitmapDataStateTest, LayoutMatchesNativeWin32AndPortableX64Fields) {
    EXPECT_TRUE(std::is_standard_layout<CKBitmapData>::value);
    EXPECT_FALSE(std::is_polymorphic<CKBitmapData>::value);
    EXPECT_FALSE(std::has_virtual_destructor<CKBitmapData>::value);
    EXPECT_EQ(4u, sizeof(CKDWORD)); EXPECT_EQ(4u, sizeof(CK_BITMAP_SAVEOPTIONS));
    const size_t offsets[] = {offsetof(CKBitmapData, m_MovieInfo), offsetof(CKBitmapData, m_Slots),
        offsetof(CKBitmapData, m_Width), offsetof(CKBitmapData, m_Height), offsetof(CKBitmapData, m_CurrentSlot),
        offsetof(CKBitmapData, m_PickThreshold), offsetof(CKBitmapData, m_BitmapFlags), offsetof(CKBitmapData, m_TransColor),
        offsetof(CKBitmapData, m_SaveProperties), offsetof(CKBitmapData, m_SaveOptions)};
    const size_t win32[] = {0, 4, 16, 20, 24, 28, 32, 36, 40, 44};
    const size_t x64[] = {0, 8, 32, 36, 40, 44, 48, 52, 56, 64};
    const size_t *expected = sizeof(void *) == 4 ? win32 : x64;
    for (int i = 0; i < 10; ++i) EXPECT_EQ(expected[i], offsets[i]) << "field " << i;
    EXPECT_EQ(sizeof(void *) == 4 ? 48u : 72u, sizeof(CKBitmapData));
    EXPECT_EQ(sizeof(void *), alignof(CKBitmapData));
    EXPECT_EQ(3 * sizeof(void *), sizeof(XArray<CKBitmapSlot *>));
}

TEST(CKBitmapDataStateTest, UniqueOwnerCannotBeCopiedOrMoved) {
    EXPECT_FALSE(std::is_copy_constructible<CKBitmapData>::value);
    EXPECT_FALSE(std::is_copy_assignable<CKBitmapData>::value);
    EXPECT_FALSE(std::is_move_constructible<CKBitmapData>::value);
    EXPECT_FALSE(std::is_move_assignable<CKBitmapData>::value);
}

TEST(CKBitmapDataStateTest, DefaultsIncludeEmptyThreePointerSlotStorage) {
    CKBitmapData bitmap;
    void *pointers[3] = {reinterpret_cast<void *>(1), reinterpret_cast<void *>(1), reinterpret_cast<void *>(1)};
    memcpy(pointers, &bitmap.m_Slots, sizeof(pointers));
    for (int i = 0; i < 3; ++i) EXPECT_EQ(nullptr, pointers[i]);
    EXPECT_EQ(-1, bitmap.GetWidth()); EXPECT_EQ(-1, bitmap.GetHeight());
    EXPECT_EQ(0, bitmap.m_CurrentSlot); EXPECT_EQ(0, bitmap.GetPickThreshold());
    EXPECT_EQ(1u, bitmap.m_BitmapFlags); EXPECT_EQ(0u, bitmap.m_TransColor);
    EXPECT_EQ(nullptr, bitmap.m_MovieInfo); EXPECT_EQ(nullptr, bitmap.m_SaveProperties);
    EXPECT_EQ(CKTEXTURE_USEGLOBAL, bitmap.m_SaveOptions);
}

TEST(CKBitmapDataStateTest, DimensionsRetainStoredGeometryAfterReleaseAll) {
    CKBitmapData bitmap; StateImage(bitmap);
    ASSERT_TRUE(bitmap.ReleaseAllSlots());
    EXPECT_EQ(3, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight());
    EXPECT_NE(0u, bitmap.m_BitmapFlags & CKBITMAPDATA_INVALID);
    EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.m_CurrentSlot);
}

TEST(CKBitmapDataStateTest, DimensionsTrackResizeAndRejectIncompatibleAdditionalImage) {
    CKBitmapData bitmap; StateImage(bitmap); StateImage(bitmap, 1);
    EXPECT_FALSE(bitmap.CreateImage(7, 4, 32, 1));
    EXPECT_EQ(3, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight());
    ASSERT_TRUE(bitmap.ResizeImages(4, 3));
    EXPECT_EQ(4, bitmap.GetWidth()); EXPECT_EQ(3, bitmap.GetHeight());
}

TEST(CKBitmapDataStateTest, DimensionGettersReturnStoredSignedValuesWithoutValidation) {
    CKBitmapData bitmap; bitmap.m_Width = -9; bitmap.m_Height = 0;
    EXPECT_EQ(-9, bitmap.GetWidth()); EXPECT_EQ(0, bitmap.GetHeight());
}

TEST(CKBitmapDataStateTest, PickThresholdStoresUnclampedSignedValues) {
    CKBitmapData bitmap;
    const int values[] = {INT_MIN, -1, 0, 1, 255, 256, INT_MAX};
    for (int i = 0; i < 7; ++i) { bitmap.SetPickThreshold(values[i]); EXPECT_EQ(values[i], bitmap.GetPickThreshold()); }
}

TEST(CKBitmapDataStateTest, PickThresholdChangesNoOtherImageOrSaveState) {
    CKBitmapData bitmap; StateImage(bitmap); ASSERT_TRUE(bitmap.SetSlotFileName(0, "pick.image"));
    bitmap.m_BitmapFlags = 0x8000002a; bitmap.m_TransColor = 0x12345678;
    bitmap.SetSaveOptions(CKTEXTURE_EXTERNAL); void *surface = bitmap.LockSurfacePtr();
    bitmap.SetPickThreshold(128);
    EXPECT_EQ(128, bitmap.m_PickThreshold); EXPECT_EQ(0x8000002au, bitmap.m_BitmapFlags);
    EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0));
    EXPECT_STREQ("pick.image", bitmap.GetSlotFileName(0)); EXPECT_EQ(0x12345678u, bitmap.m_TransColor);
    EXPECT_EQ(CKTEXTURE_EXTERNAL, bitmap.GetSaveOptions()); EXPECT_EQ(nullptr, bitmap.GetSaveFormat());
}

TEST(CKBitmapDataStateTest, DynamicHintTreatsAllNonzeroInputsAsEnabledAndReturnsMask) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = 0x8000002a;
    const int values[] = {1, 2, -7, INT_MIN};
    for (int i = 0; i < 4; ++i) { bitmap.SetDynamicHint(values[i]); EXPECT_EQ(64, bitmap.GetDynamicHint()); EXPECT_EQ(0x8000006au, bitmap.m_BitmapFlags); }
    bitmap.SetDynamicHint(0); EXPECT_EQ(0, bitmap.GetDynamicHint()); EXPECT_EQ(0x8000002au, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataStateTest, DynamicHintDoesNotCreateSlotsOrDirtyExistingPixels) {
    CKBitmapData bitmap; bitmap.SetDynamicHint(TRUE); EXPECT_EQ(0, bitmap.m_Slots.Size());
    EXPECT_EQ(-1, bitmap.GetWidth()); EXPECT_EQ(-1, bitmap.GetHeight());
    StateImage(bitmap); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_CLAMPUPTODATE;
    void *surface = bitmap.LockSurfacePtr(); bitmap.SetDynamicHint(TRUE);
    EXPECT_EQ(0, bitmap.ToRestore()); EXPECT_EQ(surface, bitmap.LockSurfacePtr());
    EXPECT_EQ(0x9abcdef0u, bitmap.GetPixel(2, 1)); EXPECT_EQ(74u, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataStateTest, DynamicHintSurvivesImageResizeAndRelease) {
    CKBitmapData bitmap; bitmap.SetDynamicHint(TRUE); StateImage(bitmap);
    ASSERT_TRUE(bitmap.ResizeImages(2, 3)); EXPECT_EQ(64, bitmap.GetDynamicHint());
    ASSERT_TRUE(bitmap.ReleaseAllSlots()); EXPECT_EQ(64, bitmap.GetDynamicHint());
}

TEST(CKBitmapDataStateTest, CubeMapCreatesSixEmptyFacesWithoutInventingDimensions) {
    CKBitmapData bitmap; bitmap.SetCubeMap(-1);
    EXPECT_EQ(16, bitmap.IsCubeMap()); ASSERT_EQ(6, bitmap.m_Slots.Size());
    EXPECT_EQ(-1, bitmap.GetWidth()); EXPECT_EQ(-1, bitmap.GetHeight());
    for (int i = 0; i < 6; ++i) { EXPECT_NE(nullptr, bitmap.m_Slots[i]); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(i)); EXPECT_STREQ("", bitmap.GetSlotFileName(i)); }
}

TEST(CKBitmapDataStateTest, CubeMapPreservesExistingFacePixelsAndNames) {
    CKBitmapData bitmap; StateImage(bitmap); ASSERT_TRUE(bitmap.SetSlotFileName(0, "face.image"));
    void *first = bitmap.LockSurfacePtr(); bitmap.SetCubeMap(TRUE); bitmap.SetCubeMap(TRUE);
    EXPECT_EQ(6, bitmap.GetSlotCount()); EXPECT_EQ(first, bitmap.LockSurfacePtr(0));
    EXPECT_EQ(0x12345678u, bitmap.GetPixel(1, 1, 0)); EXPECT_STREQ("face.image", bitmap.GetSlotFileName(0));
}

TEST(CKBitmapDataStateTest, CubeMapTrimsExcessFacesAndRepairsOutOfRangeSelection) {
    CKBitmapData bitmap; StateImage(bitmap); ASSERT_TRUE(bitmap.SetSlotCount(8));
    ASSERT_TRUE(bitmap.SetCurrentSlot(7)); bitmap.SetCubeMap(TRUE);
    EXPECT_EQ(6, bitmap.m_Slots.Size()); EXPECT_EQ(0, bitmap.m_CurrentSlot);
    EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataStateTest, DisablingCubeMapKeepsStorageAndSelectedFace) {
    CKBitmapData bitmap; bitmap.SetCubeMap(TRUE); ASSERT_TRUE(bitmap.CreateImage(3, 2, 32, 4));
    ASSERT_TRUE(bitmap.SetCurrentSlot(4)); void *surface = bitmap.LockSurfacePtr(4);
    bitmap.SetCubeMap(FALSE); EXPECT_EQ(0, bitmap.IsCubeMap()); EXPECT_EQ(6, bitmap.GetSlotCount());
    EXPECT_EQ(4, bitmap.GetCurrentSlot()); EXPECT_EQ(surface, bitmap.LockSurfacePtr());
}

TEST(CKBitmapDataStateTest, CubeMapOnlyChangesItsFlagBeyondTheSlotResize) {
    CKBitmapData bitmap; StateImage(bitmap); bitmap.m_BitmapFlags = 0x8000006a;
    bitmap.m_PickThreshold = -17; bitmap.m_TransColor = 0x12345678;
    bitmap.SetCubeMap(TRUE); EXPECT_EQ(0x8000007au, bitmap.m_BitmapFlags);
    bitmap.SetCubeMap(FALSE); EXPECT_EQ(0x8000006au, bitmap.m_BitmapFlags);
    EXPECT_EQ(-17, bitmap.m_PickThreshold); EXPECT_EQ(0x12345678u, bitmap.m_TransColor);
}

TEST(CKBitmapDataStateTest, MovieCubeHintKeepsNativeRejectedResizeSemantics) {
    StateMovieReader reader;
    { CKBitmapData bitmap; StateImage(bitmap); StateMovie(bitmap, reader); CKMovieInfo *owner = bitmap.m_MovieInfo;
      bitmap.SetCubeMap(TRUE); EXPECT_EQ(16, bitmap.IsCubeMap()); EXPECT_EQ(1, bitmap.m_Slots.Size());
      EXPECT_EQ(owner, bitmap.m_MovieInfo); EXPECT_EQ(3, bitmap.GetCurrentSlot());
      EXPECT_EQ(0, reader.Reads); EXPECT_EQ(0, reader.Counts); EXPECT_EQ(0, reader.Releases);
      bitmap.SetCubeMap(FALSE); EXPECT_EQ(0, bitmap.IsCubeMap()); EXPECT_EQ(1, bitmap.m_Slots.Size()); }
    EXPECT_EQ(1, reader.Releases);
}

TEST(CKBitmapDataStateTest, SetPixelWritesExactArgbToIndependentNonSquareSurface) {
    CKBitmapData bitmap; StateImage(bitmap);
    ASSERT_TRUE(bitmap.SetPixel(2, 0, 0x01020304)); ASSERT_TRUE(bitmap.SetPixel(0, 1, 0xffaabbcc));
    const CKDWORD expected[] = {0x10203040, 0x50607080, 0x01020304, 0xffaabbcc, 0x12345678, 0x9abcdef0};
    const CKDWORD *pixels = reinterpret_cast<const CKDWORD *>(bitmap.LockSurfacePtr());
    for (int i = 0; i < 6; ++i) EXPECT_EQ(expected[i], pixels[i]);
}

TEST(CKBitmapDataStateTest, GetPixelReadsLiteralCpuStorageWithoutUsingSetPixel) {
    CKBitmapData bitmap; StateImage(bitmap);
    const CKDWORD expected[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f010, 0x12345678, 0x9abcdef0};
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 3; ++x) EXPECT_EQ(expected[x + 3 * y], bitmap.GetPixel(x, y));
}

TEST(CKBitmapDataStateTest, EveryNegativeSlotUsesNumericCurrentSlot) {
    CKBitmapData bitmap; StateImage(bitmap); StateImage(bitmap, 1); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    const int values[] = {-1, -2, INT_MIN};
    for (int i = 0; i < 3; ++i) { ASSERT_TRUE(bitmap.SetPixel(i, 0, 0xa0a0a000u + i, values[i])); EXPECT_EQ(0xa0a0a000u + i, bitmap.GetPixel(i, 0, values[i])); }
    EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0, 0));
}

TEST(CKBitmapDataStateTest, ExplicitSlotIgnoresTheCurrentSelection) {
    CKBitmapData bitmap; StateImage(bitmap); StateImage(bitmap, 1); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    ASSERT_TRUE(bitmap.SetPixel(1, 1, 0x00112233, 0));
    EXPECT_EQ(0x00112233u, bitmap.GetPixel(1, 1, 0)); EXPECT_EQ(0x12345678u, bitmap.GetPixel(1, 1));
}

TEST(CKBitmapDataStateTest, InvalidExplicitAndCurrentSlotsFailWithoutTouchingPixels) {
    CKBitmapData bitmap; StateImage(bitmap);
    EXPECT_FALSE(bitmap.SetPixel(0, 0, 0, INT_MAX)); EXPECT_EQ(0u, bitmap.GetPixel(0, 0, INT_MAX));
    bitmap.m_CurrentSlot = -1; EXPECT_FALSE(bitmap.SetPixel(0, 0, 0)); EXPECT_EQ(0u, bitmap.GetPixel(0, 0));
    bitmap.m_CurrentSlot = 3; EXPECT_FALSE(bitmap.SetPixel(0, 0, 0, -2)); EXPECT_EQ(0u, bitmap.GetPixel(0, 0, -2));
    EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0, 0));
}

TEST(CKBitmapDataStateTest, AllocatedSlotWithoutCpuSurfaceHasNoPixel) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.SetSlotCount(2));
    EXPECT_FALSE(bitmap.SetPixel(0, 0, 0xff112233, 1)); EXPECT_EQ(0u, bitmap.GetPixel(0, 0, 1));
    StateImage(bitmap); bitmap.m_Slots[0]->Flush();
    EXPECT_FALSE(bitmap.SetPixel(0, 0, 0xff112233, 0)); EXPECT_EQ(0u, bitmap.GetPixel(0, 0, 0));
}

TEST(CKBitmapDataStateTest, ZeroArgbIsStoredSuccessfullyWithoutInventingAlpha) {
    CKBitmapData bitmap; StateImage(bitmap); EXPECT_TRUE(bitmap.SetPixel(2, 1, 0));
    EXPECT_EQ(0u, reinterpret_cast<CKDWORD *>(bitmap.LockSurfacePtr())[5]); EXPECT_EQ(0u, bitmap.GetPixel(2, 1));
}

TEST(CKBitmapDataStateTest, PixelAccessDoesNotDirtyOrReprocessTransparencyUntilRelease) {
    CKBitmapData bitmap; StateImage(bitmap); bitmap.m_BitmapFlags = 0x8000004a; bitmap.m_TransColor = 0x10203040;
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x10203040)); EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0));
    EXPECT_EQ(0x8000004au, bitmap.m_BitmapFlags); EXPECT_EQ(0, bitmap.ToRestore());
    ASSERT_TRUE(bitmap.ReleaseSurfacePtr()); EXPECT_EQ(4, bitmap.ToRestore());
    EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataStateTest, MoviePixelsUsePhysicalStorageRatherThanLogicalFrameIndex) {
    StateMovieReader reader;
    { CKBitmapData bitmap; StateImage(bitmap); StateMovie(bitmap, reader);
      EXPECT_EQ(5, bitmap.GetSlotCount()); EXPECT_EQ(3, bitmap.GetCurrentSlot()); EXPECT_EQ(0, bitmap.m_CurrentSlot);
      EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0)); ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x00112233));
      EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0, 0)); EXPECT_FALSE(bitmap.SetPixel(0, 0, 0, 3));
      EXPECT_EQ(0u, bitmap.GetPixel(0, 0, 3)); EXPECT_EQ(0, reader.Reads); EXPECT_EQ(3, bitmap.m_MovieInfo->m_MovieCurrentSlot); }
    EXPECT_EQ(1, reader.Releases);
}

TEST(CKBitmapDataStateTest, SlotRemovalKeepsNativeNumericSelectionUntilItBecomesInvalid) {
    CKBitmapData bitmap; StateImage(bitmap); StateImage(bitmap, 1); StateImage(bitmap, 2);
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xff010203, 1)); ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xffaabbcc, 2));
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); ASSERT_TRUE(bitmap.ReleaseSlot(0));
    EXPECT_EQ(1, bitmap.m_CurrentSlot); EXPECT_EQ(0xffaabbccu, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataStateTest, TransparentColorStoresAllBitsIndependentlyOfThePixelMask) {
    CKBitmapData bitmap; StateImage(bitmap); bitmap.SetTransparentColor(0xa1b2c3d4);
    EXPECT_EQ(0xa1b2c3d4u, bitmap.m_TransColor); bitmap.SetTransparent(TRUE); bitmap.SetTransparent(FALSE);
    EXPECT_EQ(0xa1b2c3d4u, bitmap.GetTransparentColor()); EXPECT_EQ(0x10203040u, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataStateTest, MovieOwnershipAndCpuSlotsRemainSeparateOnClear) {
    StateMovieReader reader;
    CKBitmapData bitmap; StateImage(bitmap); void *surface = bitmap.LockSurfacePtr(); StateMovie(bitmap, reader);
    bitmap.SetMovieInfo(nullptr); EXPECT_EQ(1, reader.Releases); EXPECT_EQ(nullptr, bitmap.m_MovieInfo);
    EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(1, bitmap.m_Slots.Size());
    EXPECT_EQ(0x9abcdef0u, bitmap.GetPixel(2, 1)); EXPECT_EQ(3, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight());
}
