#include <gtest/gtest.h>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"

// This unexported SDK helper belongs to chunk serialization, outside this fixture.
CKSTRING CKJustFile(CKSTRING) {
    ADD_FAILURE() << "Unexpected bitmap serialization path";
    return nullptr;
}

static VxImageDescEx SlotDesc(int width, int height, int pitch, int bpp = 32) {
    VxImageDescEx desc;
    desc.Width = width; desc.Height = height; desc.BytesPerLine = pitch;
    desc.BitsPerPixel = bpp; desc.RedMask = R_MASK; desc.GreenMask = G_MASK;
    desc.BlueMask = B_MASK; desc.AlphaMask = bpp == 32 ? A_MASK : 0;
    return desc;
}

static void SlotPixelEquals(CKBitmapData &bitmap, int slot, const CKDWORD *expected, int count) {
    const CKDWORD *pixels = reinterpret_cast<const CKDWORD *>(bitmap.LockSurfacePtr(slot));
    ASSERT_NE(nullptr, pixels);
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], pixels[i]) << "slot=" << slot << " pixel=" << i;
}

static void CreateThreeSlots(CKBitmapData &bitmap) {
    ASSERT_TRUE(bitmap.CreateImage(1, 1, 32, 0));
    ASSERT_TRUE(bitmap.CreateImage(1, 1, 32, 1));
    ASSERT_TRUE(bitmap.CreateImage(1, 1, 32, 2));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xff112233, 0));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xff445566, 1));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xff778899, 2));
    ASSERT_TRUE(bitmap.SetSlotFileName(0, "zero"));
    ASSERT_TRUE(bitmap.SetSlotFileName(1, "one"));
    ASSERT_TRUE(bitmap.SetSlotFileName(2, "two"));
}

// Deterministic plugin boundary; the bitmap and VxMath blit are real implementations.
class SlotMovieReader : public CKMovieReader {
public:
    int reads = 0;
    int lastFrame = -1;
    int releases = 0;
    int failure = 0;
    CKDWORD pixels[6] = {0x10203040,0x50607080,0xdeadbeef,0x90a0b0c0,0xd0e0f010,0xcafebabe};
    CKMovieProperties properties;
    void Release() override { ++releases; }
    CKPluginInfo *GetReaderInfo() override { return nullptr; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    int GetMovieFrameCount() override { return 3; }
    int GetMovieLength() override { return 120; }
    CKERROR OpenFile(CKSTRING) override { ADD_FAILURE(); return CKERR_INVALIDPARAMETER; }
    CKERROR OpenMemory(CKSTRING) override { ADD_FAILURE(); return CKERR_INVALIDPARAMETER; }
    CKERROR OpenAsynchronousFile(CKSTRING) override { ADD_FAILURE(); return CKERR_INVALIDPARAMETER; }
    CKERROR ReadFrame(int frame, CKMovieProperties **out) override {
        ++reads; lastFrame = frame;
        if (failure == 1) return CKERR_INVALIDPARAMETER;
        if (failure == 2) { *out = nullptr; return CK_OK; }
        properties.m_Format = SlotDesc(2, 2, 12);
        properties.m_Data = failure == 3 ? nullptr : pixels;
        *out = &properties;
        return CK_OK;
    }
};

static void AttachSlotMovie(CKBitmapData &bitmap, SlotMovieReader *reader) {
    ASSERT_TRUE(bitmap.CreateImage(2, 2));
    bitmap.m_MovieInfo = new CKMovieInfo(XString("fixture.avi"));
    bitmap.m_MovieInfo->m_MovieReader = reader;
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
}

TEST(CKBitmapDataSlotsTest, CreateIgnoresRequestedBppAndInitializesAlignedOpaqueBlack) {
    const int depths[] = {0,8,16,24,32};
    for (int depth : depths) {
        CKBitmapData bitmap;
        ASSERT_TRUE(bitmap.CreateImage(3, 2, depth));
        VxImageDescEx desc; ASSERT_TRUE(bitmap.GetImageDesc(desc));
        EXPECT_EQ(3, desc.Width); EXPECT_EQ(2, desc.Height); EXPECT_EQ(32, desc.BitsPerPixel);
        EXPECT_EQ(12, desc.BytesPerLine); EXPECT_EQ(A_MASK, desc.AlphaMask);
        EXPECT_EQ(R_MASK, desc.RedMask); EXPECT_EQ(G_MASK, desc.GreenMask); EXPECT_EQ(B_MASK, desc.BlueMask);
        EXPECT_EQ(0u, reinterpret_cast<size_t>(bitmap.LockSurfacePtr()) & 15u);
        const CKDWORD expected[] = {0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000};
        SlotPixelEquals(bitmap, 0, expected, 6);
        EXPECT_STREQ("", bitmap.GetSlotFileName(0)); EXPECT_EQ(0, bitmap.GetCurrentSlot());
        EXPECT_FALSE(bitmap.m_BitmapFlags & CKBITMAPDATA_INVALID); EXPECT_EQ(4, bitmap.ToRestore());
    }
}

TEST(CKBitmapDataSlotsTest, CreateAtHigherIndexCreatesEmptyIntermediateSlots) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(2, 1, 24, 3));
    ASSERT_EQ(4, bitmap.GetSlotCount());
    for (int i = 0; i < 3; ++i) {
        ASSERT_NE(nullptr, bitmap.m_Slots[i]); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(i));
        EXPECT_STREQ("", bitmap.GetSlotFileName(i));
    }
    const CKDWORD expected[] = {0xff000000,0xff000000}; SlotPixelEquals(bitmap, 3, expected, 2);
    EXPECT_EQ(0, bitmap.GetCurrentSlot()); // Native selection is not automatically the new slot.
}

TEST(CKBitmapDataSlotsTest, RecreateOnlySlotChangesDimensionsAndClearsItsName) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x12345678)); ASSERT_TRUE(bitmap.SetSlotFileName(0, "old.bmp"));
    ASSERT_TRUE(bitmap.CreateImage(3, 1, 8, 0));
    EXPECT_EQ(3, bitmap.GetWidth()); EXPECT_EQ(1, bitmap.GetHeight()); EXPECT_STREQ("", bitmap.GetSlotFileName(0));
    const CKDWORD expected[] = {0xff000000,0xff000000,0xff000000}; SlotPixelEquals(bitmap, 0, expected, 3);
}

TEST(CKBitmapDataSlotsTest, RecreateOneOfSeveralSlotsPreservesItsPeersAndSelection) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(2));
    CKBYTE *zero = bitmap.LockSurfacePtr(0); CKBYTE *two = bitmap.LockSurfacePtr(2);
    ASSERT_TRUE(bitmap.CreateImage(1, 1, 8, 1));
    EXPECT_EQ(zero, bitmap.LockSurfacePtr(0)); EXPECT_EQ(two, bitmap.LockSurfacePtr(2));
    EXPECT_EQ(0xff112233u, bitmap.GetPixel(0, 0, 0)); EXPECT_EQ(0xff778899u, bitmap.GetPixel(0, 0, 2));
    EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0, 1)); EXPECT_STREQ("", bitmap.GetSlotFileName(1));
    EXPECT_EQ(2, bitmap.GetCurrentSlot()); EXPECT_STREQ("two", bitmap.GetSlotFileName(2));
}

TEST(CKBitmapDataSlotsTest, MismatchedDimensionsInMultipleSlotsAreRejectedWithoutChangingPixels) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    CKBYTE *image = bitmap.LockSurfacePtr(0);
    EXPECT_FALSE(bitmap.CreateImage(2, 1, 32, 0)); EXPECT_FALSE(bitmap.CreateImage(1, 2, 32, 3));
    EXPECT_EQ(3, bitmap.GetSlotCount()); EXPECT_EQ(1, bitmap.GetWidth()); EXPECT_EQ(image, bitmap.LockSurfacePtr(0));
    EXPECT_EQ(0xff112233u, bitmap.GetPixel(0, 0, 0)); EXPECT_STREQ("zero", bitmap.GetSlotFileName(0));
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, CreateRejectsNegativeSlotWithoutChangingExistingImage) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); CKBYTE *image = bitmap.LockSurfacePtr();
    const CKDWORD flags = bitmap.m_BitmapFlags;
    EXPECT_FALSE(bitmap.CreateImage(2, 2, 32, -1)); EXPECT_EQ(image, bitmap.LockSurfacePtr());
    EXPECT_EQ(1, bitmap.GetWidth()); EXPECT_EQ(flags, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, CreateRejectsNonpositiveDimensionsWithoutReplacingOwnedStorage) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); CKBYTE *image = bitmap.LockSurfacePtr();
    EXPECT_FALSE(bitmap.CreateImage(0, 1)); EXPECT_FALSE(bitmap.CreateImage(1, 0));
    EXPECT_FALSE(bitmap.CreateImage(-1, 2)); EXPECT_FALSE(bitmap.CreateImage(2, -1));
    EXPECT_EQ(image, bitmap.LockSurfacePtr()); EXPECT_EQ(1, bitmap.GetWidth()); EXPECT_EQ(1, bitmap.GetHeight());
    EXPECT_TRUE(bitmap.m_BitmapFlags & CKBITMAPDATA_INVALID);
}

TEST(CKBitmapDataSlotsTest, CreateAfterReleaseAllRepairsTheCurrentSlotToTheRequestedIndex) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); ASSERT_TRUE(bitmap.ReleaseAllSlots());
    ASSERT_TRUE(bitmap.CreateImage(2, 1, 32, 2));
    EXPECT_EQ(2, bitmap.GetCurrentSlot()); EXPECT_EQ(bitmap.LockSurfacePtr(2), bitmap.LockSurfacePtr());
    EXPECT_EQ(2, bitmap.GetWidth()); EXPECT_EQ(3, bitmap.GetSlotCount());
}

TEST(CKBitmapDataSlotsTest, GrowingSlotCountKeepsExistingDataAndCreatesOnlyEmptySlots) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    CKBYTE *image = bitmap.LockSurfacePtr(1); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.SetSlotCount(5)); EXPECT_EQ(5, bitmap.GetSlotCount()); EXPECT_EQ(1, bitmap.GetCurrentSlot());
    EXPECT_EQ(image, bitmap.LockSurfacePtr(1)); EXPECT_STREQ("one", bitmap.GetSlotFileName(1));
    EXPECT_EQ(0xff445566u, bitmap.GetPixel(0, 0, 1));
    EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(3)); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(4));
    EXPECT_STREQ("", bitmap.GetSlotFileName(4)); EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, ShrinkingSlotCountRetainsPrefixAndRepairsRemovedCurrentIndex) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(2));
    CKBYTE *image = bitmap.LockSurfacePtr(0); ASSERT_TRUE(bitmap.SetSlotCount(1));
    EXPECT_EQ(1, bitmap.GetSlotCount()); EXPECT_EQ(0, bitmap.GetCurrentSlot());
    EXPECT_EQ(image, bitmap.LockSurfacePtr()); EXPECT_STREQ("zero", bitmap.GetSlotFileName(0));
    EXPECT_EQ(0xff112233u, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataSlotsTest, SettingSameCountPreservesSelectionStorageNamesAndFlags) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    CKBYTE *image = bitmap.LockSurfacePtr(); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.SetSlotCount(3)); EXPECT_EQ(image, bitmap.LockSurfacePtr());
    EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_STREQ("one", bitmap.GetSlotFileName(1));
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, ZeroCountAndReleaseAllAllowImagesToBeCreatedAgain) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetSlotCount(0));
    EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot()); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr());
    EXPECT_TRUE(bitmap.m_BitmapFlags & CKBITMAPDATA_INVALID);
    ASSERT_TRUE(bitmap.SetSlotCount(2)); EXPECT_EQ(0, bitmap.GetCurrentSlot());
    ASSERT_TRUE(bitmap.CreateImage(2, 1, 32, 1)); EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0, 1));
    ASSERT_TRUE(bitmap.ReleaseAllSlots()); ASSERT_TRUE(bitmap.ReleaseAllSlots());
    EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot());
    ASSERT_TRUE(bitmap.CreateImage(1, 1)); EXPECT_EQ(0, bitmap.GetCurrentSlot());
}

TEST(CKBitmapDataSlotsTest, NegativeCountDoesNotDeleteSlots) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); CKBYTE *image = bitmap.LockSurfacePtr(0);
    EXPECT_FALSE(bitmap.SetSlotCount(-1)); EXPECT_EQ(3, bitmap.GetSlotCount());
    EXPECT_EQ(image, bitmap.LockSurfacePtr(0)); EXPECT_STREQ("two", bitmap.GetSlotFileName(2));
}

TEST(CKBitmapDataSlotsTest, SlotSwitchRequestsRestoreOnlyOnNonCubemapTransitions) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_EQ(4, bitmap.ToRestore());
    bitmap.m_BitmapFlags &= ~CKBITMAPDATA_FORCERESTORE;
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(0, bitmap.ToRestore());
    bitmap.m_BitmapFlags |= CKBITMAPDATA_CUBEMAP;
    ASSERT_TRUE(bitmap.SetCurrentSlot(2)); EXPECT_EQ(2, bitmap.GetCurrentSlot()); EXPECT_EQ(0, bitmap.ToRestore());
    EXPECT_EQ(0xff778899u, bitmap.GetPixel(0, 0));
}

TEST(CKBitmapDataSlotsTest, InvalidSlotSwitchDoesNotChangeSelectionOrFlags) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    EXPECT_FALSE(bitmap.SetCurrentSlot(-1)); EXPECT_FALSE(bitmap.SetCurrentSlot(3));
    EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, LockFallsBackToCurrentForBothNegativeAndHighSlotIndices) {
    CKBitmapData bitmap; EXPECT_EQ(nullptr, bitmap.LockSurfacePtr()); CreateThreeSlots(bitmap);
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); CKBYTE *image = bitmap.LockSurfacePtr(1);
    EXPECT_EQ(image, bitmap.LockSurfacePtr(-1)); EXPECT_EQ(image, bitmap.LockSurfacePtr(-2));
    EXPECT_EQ(image, bitmap.LockSurfacePtr(99)); EXPECT_NE(image, bitmap.LockSurfacePtr(0));
    bitmap.m_CurrentSlot = 99; EXPECT_EQ(nullptr, bitmap.LockSurfacePtr());
}

TEST(CKBitmapDataSlotsTest, SurfaceReleaseMarksRestoreWithoutChangingPixelsOrSelection) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT; CKBYTE *image = bitmap.LockSurfacePtr();
    ASSERT_TRUE(bitmap.ReleaseSurfacePtr(-1)); EXPECT_EQ(4, bitmap.ToRestore());
    ASSERT_TRUE(bitmap.ReleaseSurfacePtr(99)); EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_EQ(image, bitmap.LockSurfacePtr());
    EXPECT_EQ(0xff445566u, bitmap.GetPixel(0, 0)); EXPECT_TRUE(bitmap.IsTransparent());
}

TEST(CKBitmapDataSlotsTest, SlotNamesAreCopiedAndInvalidIndicesDoNotUseCurrentSlot) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.SetSlotCount(2)); char name[] = "source.bmp";
    ASSERT_TRUE(bitmap.SetSlotFileName(1, name)); name[0] = 'X';
    EXPECT_STREQ("source.bmp", bitmap.GetSlotFileName(1)); EXPECT_STREQ("", bitmap.GetSlotFileName(0));
    EXPECT_EQ(nullptr, bitmap.GetSlotFileName(-1)); EXPECT_EQ(nullptr, bitmap.GetSlotFileName(2));
    EXPECT_FALSE(bitmap.SetSlotFileName(-1, "bad")); EXPECT_FALSE(bitmap.SetSlotFileName(2, "bad"));
    ASSERT_TRUE(bitmap.SetSlotFileName(1, nullptr)); EXPECT_STREQ("", bitmap.GetSlotFileName(1));
}

TEST(CKBitmapDataSlotsTest, ReleaseMiddleSlotCompactsContentAndKeepsNativeNumericSelection) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    CKBYTE *last = bitmap.LockSurfacePtr(2); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.ReleaseSlot(0)); EXPECT_EQ(2, bitmap.GetSlotCount());
    EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_EQ(last, bitmap.LockSurfacePtr());
    EXPECT_STREQ("one", bitmap.GetSlotFileName(0)); EXPECT_STREQ("two", bitmap.GetSlotFileName(1));
    EXPECT_EQ(0xff445566u, bitmap.GetPixel(0, 0, 0)); EXPECT_EQ(0xff778899u, bitmap.GetPixel(0, 0, 1));
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, ReleaseLastCurrentSlotClampsSelectionAndRequestsRestore) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(2));
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.ReleaseSlot(2)); EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_EQ(4, bitmap.ToRestore());
    EXPECT_EQ(0xff445566u, bitmap.GetPixel(0, 0)); EXPECT_STREQ("one", bitmap.GetSlotFileName(1));
}

TEST(CKBitmapDataSlotsTest, ReleaseOnlySlotInvalidatesAndRejectsFurtherRemoval) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1));
    ASSERT_TRUE(bitmap.ReleaseSlot(0)); EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot());
    EXPECT_TRUE(bitmap.m_BitmapFlags & CKBITMAPDATA_INVALID); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr());
    EXPECT_FALSE(bitmap.ReleaseSlot(0)); EXPECT_FALSE(bitmap.ReleaseSlot(-1));
}

TEST(CKBitmapDataSlotsTest, ReleaseEmptySlotCompactsWithoutChangingRemainingStorage) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1, 32, 2));
    CKBYTE *image = bitmap.LockSurfacePtr(2); ASSERT_TRUE(bitmap.ReleaseSlot(1));
    EXPECT_EQ(image, bitmap.LockSurfacePtr(1)); EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(0)); EXPECT_EQ(2, bitmap.GetSlotCount());
}

TEST(CKBitmapDataSlotsTest, ReleaseAllPreservesDimensionsAndNonValidityFlags) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap);
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_CUBEMAP;
    ASSERT_TRUE(bitmap.ReleaseAllSlots()); EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot());
    EXPECT_EQ(1, bitmap.GetWidth()); EXPECT_EQ(1, bitmap.GetHeight());
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_CUBEMAP | CKBITMAPDATA_INVALID, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, ResizeSameDimensionsPreservesEverySlotPointerAndMetadata) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetCurrentSlot(2));
    CKBYTE *images[] = {bitmap.LockSurfacePtr(0),bitmap.LockSurfacePtr(1),bitmap.LockSurfacePtr(2)};
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.ResizeImages(1, 1));
    for (int i = 0; i < 3; ++i) EXPECT_EQ(images[i], bitmap.LockSurfacePtr(i));
    EXPECT_EQ(2, bitmap.GetCurrentSlot()); EXPECT_STREQ("two", bitmap.GetSlotFileName(2));
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataSlotsTest, ResizeSinglePixelReplicatesAllChannelsInEveryAllocatedSlot) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x12345678, 0));
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT;
    ASSERT_TRUE(bitmap.ResizeImages(3, 2)); EXPECT_EQ(3, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight());
    const CKDWORD first[] = {0x12345678,0x12345678,0x12345678,0x12345678,0x12345678,0x12345678};
    const CKDWORD second[] = {0xff445566,0xff445566,0xff445566,0xff445566,0xff445566,0xff445566};
    const CKDWORD third[] = {0xff778899,0xff778899,0xff778899,0xff778899,0xff778899,0xff778899};
    SlotPixelEquals(bitmap, 0, first, 6); SlotPixelEquals(bitmap, 1, second, 6); SlotPixelEquals(bitmap, 2, third, 6);
    EXPECT_EQ(1, bitmap.GetCurrentSlot()); EXPECT_STREQ("one", bitmap.GetSlotFileName(1));
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags); // Native resize does not set restore.
}

TEST(CKBitmapDataSlotsTest, ResizeUpscalePreservesDistinctImageCorners) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(2, 2));
    bitmap.SetPixel(0, 0, 0xffff0000); bitmap.SetPixel(1, 0, 0xff00ff00);
    bitmap.SetPixel(0, 1, 0xff0000ff); bitmap.SetPixel(1, 1, 0xffffffff);
    ASSERT_TRUE(bitmap.ResizeImages(4, 4));
    EXPECT_EQ(0xffff0000u, bitmap.GetPixel(0, 0)); EXPECT_EQ(0xff00ff00u, bitmap.GetPixel(3, 0));
    EXPECT_EQ(0xff0000ffu, bitmap.GetPixel(0, 3)); EXPECT_EQ(0xffffffffu, bitmap.GetPixel(3, 3));
}

TEST(CKBitmapDataSlotsTest, ResizeDownscaleConstantImagePreservesExactArgbAndSlotName) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(4, 3));
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 4; ++x) ASSERT_TRUE(bitmap.SetPixel(x, y, 0x80402010));
    ASSERT_TRUE(bitmap.SetSlotFileName(0, "constant")); ASSERT_TRUE(bitmap.ResizeImages(2, 1));
    const CKDWORD expected[] = {0x80402010,0x80402010}; SlotPixelEquals(bitmap, 0, expected, 2);
    EXPECT_STREQ("constant", bitmap.GetSlotFileName(0));
}

TEST(CKBitmapDataSlotsTest, ResizeAllocatesOpaqueBlackForPreviouslyEmptySlots) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); ASSERT_TRUE(bitmap.SetSlotCount(3));
    ASSERT_TRUE(bitmap.SetSlotFileName(2, "empty")); ASSERT_TRUE(bitmap.ResizeImages(2, 2));
    const CKDWORD expected[] = {0xff000000,0xff000000,0xff000000,0xff000000};
    for (int i = 0; i < 3; ++i) SlotPixelEquals(bitmap, i, expected, 4);
    EXPECT_STREQ("empty", bitmap.GetSlotFileName(2));
}

TEST(CKBitmapDataSlotsTest, ResizeInvalidDimensionsAndEmptyBitmapLeaveExistingStateUntouched) {
    CKBitmapData bitmap; EXPECT_FALSE(bitmap.ResizeImages(1, 1)); ASSERT_TRUE(bitmap.CreateImage(2, 1));
    CKBYTE *image = bitmap.LockSurfacePtr(); const CKDWORD flags = bitmap.m_BitmapFlags;
    EXPECT_FALSE(bitmap.ResizeImages(0, 2)); EXPECT_FALSE(bitmap.ResizeImages(2, 0));
    EXPECT_FALSE(bitmap.ResizeImages(-1, 2)); EXPECT_FALSE(bitmap.ResizeImages(2, -1));
    EXPECT_EQ(image, bitmap.LockSurfacePtr()); EXPECT_EQ(2, bitmap.GetWidth()); EXPECT_EQ(1, bitmap.GetHeight());
    EXPECT_EQ(flags, bitmap.m_BitmapFlags);
    CKBitmapData slotsOnly; ASSERT_TRUE(slotsOnly.SetSlotCount(2)); EXPECT_FALSE(slotsOnly.ResizeImages(2, 2));
}

TEST(CKBitmapDataSlotsTest, ImageDescriptorReportsGeometryAndClearsTheSurfacePointer) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(3, 2));
    VxImageDescEx desc; desc.Image = bitmap.LockSurfacePtr(); ASSERT_TRUE(bitmap.GetImageDesc(desc));
    EXPECT_EQ(nullptr, desc.Image); EXPECT_EQ(3, desc.Width); EXPECT_EQ(2, desc.Height);
    EXPECT_EQ(12, desc.BytesPerLine); EXPECT_EQ(32, desc.BitsPerPixel); EXPECT_EQ(A_MASK, desc.AlphaMask);
}

TEST(CKBitmapDataSlotsTest, SetSlotImageCopiesPaddedArgbAndDoesNotAlterAnotherSlot) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(2, 2)); ASSERT_TRUE(bitmap.CreateImage(2, 2, 32, 1));
    CKBYTE *other = bitmap.LockSurfacePtr(0);
    CKDWORD words[] = {0x10203040,0x50607080,0xdeadbeef,0x90a0b0c0,0xd0e0f010,0xcafebabe};
    CKBYTE *input = new CKBYTE[sizeof(words)]; memcpy(input, words, sizeof(words));
    VxImageDescEx desc = SlotDesc(2, 2, 12);
    ASSERT_TRUE(bitmap.SetSlotImage(1, input, desc));
    const CKDWORD expected[] = {0x10203040,0x50607080,0x90a0b0c0,0xd0e0f010}; SlotPixelEquals(bitmap, 1, expected, 4);
    EXPECT_EQ(other, bitmap.LockSurfacePtr(0)); EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0, 0));
    EXPECT_EQ(0, bitmap.GetCurrentSlot()); EXPECT_EQ(4, bitmap.ToRestore()); EXPECT_NE(desc.Image, bitmap.LockSurfacePtr(1));
}

TEST(CKBitmapDataSlotsTest, SetSlotImageConvertsPadded24BitRgbToOpaqueArgb) {
    CKBitmapData bitmap; CKBYTE *input = new CKBYTE[16];
    const CKBYTE source[] = {0x33,0x22,0x11,0x66,0x55,0x44,0xaa,0xbb,0x99,0x88,0x77,0xcc,0xbb,0xaa,0xcc,0xdd};
    memcpy(input, source, sizeof(source)); VxImageDescEx desc = SlotDesc(2, 2, 8, 24);
    ASSERT_TRUE(bitmap.SetSlotImage(0, input, desc));
    const CKDWORD expected[] = {0xff112233,0xff445566,0xff778899,0xffaabbcc}; SlotPixelEquals(bitmap, 0, expected, 4);
}

TEST(CKBitmapDataSlotsTest, SetSlotImageConvertsIndexedPixelsUsingTheSuppliedPalette) {
    CKBitmapData bitmap; CKBYTE *input = new CKBYTE[4]{1,2,2,1};
    VxImageDescEx desc = SlotDesc(2, 1, 4, 8); desc.RedMask = desc.GreenMask = desc.BlueMask = 0;
    desc.ColorMapEntries = 256; desc.BytesPerColorEntry = 4; desc.ColorMap = new CKBYTE[1024]{};
    CKDWORD *palette = reinterpret_cast<CKDWORD *>(desc.ColorMap); palette[1] = 0xff112233; palette[2] = 0xff445566;
    ASSERT_TRUE(bitmap.SetSlotImage(0, input, desc));
    const CKDWORD expected[] = {0xff112233,0xff445566}; SlotPixelEquals(bitmap, 0, expected, 2);
}

TEST(CKBitmapDataSlotsTest, FailedSetSlotImageLeavesInputAndPaletteOwnedByCaller) {
    CKBitmapData bitmap; CreateThreeSlots(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    CKBYTE *input = new CKBYTE[8]{1,2,3,4,5,6,7,8}; VxImageDescEx desc = SlotDesc(2, 1, 8);
    desc.ColorMap = new CKBYTE[4]{9,10,11,12}; CKBYTE *imageField = input + 1; desc.Image = imageField;
    EXPECT_FALSE(bitmap.SetSlotImage(0, input, desc)); EXPECT_EQ(1, input[0]); EXPECT_EQ(8, input[7]);
    EXPECT_EQ(9, desc.ColorMap[0]); EXPECT_EQ(imageField, desc.Image); EXPECT_EQ(surface, bitmap.LockSurfacePtr(0));
    delete[] input; delete[] desc.ColorMap;
    EXPECT_FALSE(bitmap.SetSlotImage(0, nullptr, desc));
}

TEST(CKBitmapDataSlotsTest, MovieSlotCountUsesFrameCountRatherThanPhysicalImageSlots) {
    SlotMovieReader reader;
    { CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
      EXPECT_EQ(1, bitmap.m_Slots.Size()); EXPECT_EQ(3, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot()); }
    EXPECT_EQ(1, reader.releases);
}

TEST(CKBitmapDataSlotsTest, MovieFrameSelectionBlitsUpsideDownAndSkipsPadding) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
    ASSERT_TRUE(bitmap.SetCurrentSlot(1));
    EXPECT_EQ(1, reader.reads); EXPECT_EQ(1, reader.lastFrame); EXPECT_EQ(1, bitmap.GetCurrentSlot());
    const CKDWORD expected[] = {0x90a0b0c0,0xd0e0f010,0x10203040,0x50607080}; SlotPixelEquals(bitmap, 0, expected, 4);
    EXPECT_EQ(4, bitmap.ToRestore()); EXPECT_EQ(reader.pixels, reinterpret_cast<CKDWORD *>(reader.properties.m_Format.Image));
    EXPECT_EQ(0xdeadbeefu, reader.pixels[2]); EXPECT_EQ(0xcafebabeu, reader.pixels[5]);
}

TEST(CKBitmapDataSlotsTest, RepeatedMovieFrameDoesNotDecodeOrSetRestoreAgain) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); bitmap.m_BitmapFlags &= ~CKBITMAPDATA_FORCERESTORE;
    reader.failure = 1; ASSERT_TRUE(bitmap.SetCurrentSlot(0));
    EXPECT_EQ(1, reader.reads); EXPECT_EQ(0, bitmap.ToRestore());
}

TEST(CKBitmapDataSlotsTest, FailedMovieReadDoesNotCommitFrameAndCanBeRetried) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
    ASSERT_TRUE(bitmap.SetCurrentSlot(0)); bitmap.m_BitmapFlags &= ~CKBITMAPDATA_FORCERESTORE;
    reader.failure = 1; EXPECT_FALSE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(0, bitmap.GetCurrentSlot());
    EXPECT_EQ(2, reader.reads); EXPECT_EQ(0, bitmap.ToRestore()); reader.failure = 0;
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(3, reader.reads); EXPECT_EQ(1, bitmap.GetCurrentSlot());
}

TEST(CKBitmapDataSlotsTest, MissingMoviePropertiesOrDataDoNotCommitFrameOrReplaceSurface) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    reader.failure = 2; EXPECT_FALSE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(-1, bitmap.GetCurrentSlot());
    reader.failure = 3; EXPECT_FALSE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(-1, bitmap.GetCurrentSlot());
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0, 0));
    reader.failure = 0; ASSERT_TRUE(bitmap.SetCurrentSlot(0)); EXPECT_EQ(3, reader.reads);
}

TEST(CKBitmapDataSlotsTest, InvalidMovieIndicesDoNotDecodeOrChangeFrame) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
    EXPECT_FALSE(bitmap.SetCurrentSlot(-1)); EXPECT_FALSE(bitmap.SetCurrentSlot(3)); EXPECT_FALSE(bitmap.SetCurrentSlot(99));
    EXPECT_EQ(0, reader.reads); EXPECT_EQ(-1, bitmap.GetCurrentSlot()); EXPECT_EQ(0, bitmap.ToRestore());
}

TEST(CKBitmapDataSlotsTest, MovieModeRejectsResizeCountChangesAndSlotRemoval) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    EXPECT_FALSE(bitmap.ResizeImages(4, 4)); EXPECT_FALSE(bitmap.SetSlotCount(2));
    EXPECT_FALSE(bitmap.ReleaseSlot(0)); EXPECT_FALSE(bitmap.ReleaseAllSlots());
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); EXPECT_EQ(2, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight());
    EXPECT_EQ(1, bitmap.m_Slots.Size()); EXPECT_EQ(3, bitmap.GetSlotCount()); EXPECT_EQ(0, reader.reads);
}

TEST(CKBitmapDataSlotsTest, MovieWithoutReaderReportsZeroFramesAndCannotSwitch) {
    CKBitmapData bitmap; AttachSlotMovie(bitmap, nullptr);
    EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(-1, bitmap.GetCurrentSlot()); EXPECT_FALSE(bitmap.SetCurrentSlot(0));
}

TEST(CKBitmapDataSlotsTest, MovieWithoutAnAllocatedDestinationDoesNotCommitTheDecodedFrame) {
    SlotMovieReader reader; CKBitmapData bitmap; AttachSlotMovie(bitmap, &reader);
    bitmap.m_Slots[0]->Flush(); EXPECT_FALSE(bitmap.SetCurrentSlot(0));
    EXPECT_EQ(-1, bitmap.GetCurrentSlot()); EXPECT_EQ(1, reader.reads); EXPECT_EQ(0, bitmap.ToRestore());
}
