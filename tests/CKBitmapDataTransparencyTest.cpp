#include <gtest/gtest.h>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"

// The fixed SDK does not export this serialization-only path helper.
// Pixel processing must not enter that path in this fixture.
CKSTRING CKJustFile(CKSTRING) {
    ADD_FAILURE() << "Unexpected bitmap serialization path";
    return nullptr;
}

static VxImageDescEx TransparencyImage(CKDWORD *pixels, int width, int height, int rowWords) {
    VxImageDescEx desc;
    desc.Width = width; desc.Height = height; desc.BytesPerLine = rowWords * 4;
    desc.BitsPerPixel = 32; desc.RedMask = R_MASK; desc.GreenMask = G_MASK;
    desc.BlueMask = B_MASK; desc.AlphaMask = A_MASK; desc.Image = reinterpret_cast<CKBYTE *>(pixels);
    return desc;
}

TEST(CKBitmapDataTransparencyTest, ColorKeyUsesNativeRgbQuantizationAndIgnoresKeyAlpha) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x77112233);
    CKDWORD pixels[] = {0x7f112233,0xaa102232,0x01010203,0xcc112435};
    VxImageDescEx desc = TransparencyImage(pixels, 4, 1, 4); bitmap.SetAlphaForTransparentColor(desc);
    EXPECT_EQ(0x00102232u, pixels[0]); EXPECT_EQ(0x00102232u, pixels[1]);
    EXPECT_EQ(0xff000202u, pixels[2]); EXPECT_EQ(0xff102434u, pixels[3]);
}

TEST(CKBitmapDataTransparencyTest, KeyMatchesReplaceExistingAlphaWithZero) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x00112233);
    CKDWORD pixels[] = {0x00112233,0x80102233,0xff112232};
    VxImageDescEx desc = TransparencyImage(pixels, 3, 1, 3); bitmap.SetAlphaForTransparentColor(desc);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(0x00102232u, pixels[i]);
}

TEST(CKBitmapDataTransparencyTest, NonmatchingPixelsBecomeOpaqueInsteadOfKeepingOldAlpha) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x00112233);
    CKDWORD pixels[] = {0x00010203,0x40010203,0xff010203};
    VxImageDescEx desc = TransparencyImage(pixels, 3, 1, 3); bitmap.SetAlphaForTransparentColor(desc);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(0xff000202u, pixels[i]);
}

TEST(CKBitmapDataTransparencyTest, ColorKeySkipsPaddingAndProcessesEveryVisibleRow) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x00112233);
    CKDWORD data[] = {0xdeadbeef, 0x7f112233,0x01010203,0xa1b3c5d7,0xb2c4d6e8,
        0xcc112435,0xaa102232,0xc3d5e7f9,0xd4e6f810, 0xcafebabe};
    VxImageDescEx desc = TransparencyImage(data + 1, 2, 2, 4); bitmap.SetAlphaForTransparentColor(desc);
    EXPECT_EQ(0xdeadbeefu, data[0]); EXPECT_EQ(0xcafebabeu, data[9]);
    EXPECT_EQ(0x00102232u, data[1]); EXPECT_EQ(0xff000202u, data[2]);
    EXPECT_EQ(0xff102434u, data[5]); EXPECT_EQ(0x00102232u, data[6]);
    EXPECT_EQ(0xa1b3c5d7u, data[3]); EXPECT_EQ(0xb2c4d6e8u, data[4]);
    EXPECT_EQ(0xc3d5e7f9u, data[7]); EXPECT_EQ(0xd4e6f810u, data[8]);
}

TEST(CKBitmapDataTransparencyTest, OddWidthColorKeyDoesNotConsumeTheNextRowThroughPadding) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x00112233);
    CKDWORD data[] = {0x7f112233,0x01010203,0xcc112435,0xa1b3c5d7, 0xaa102232,0xcc112435,0x01010203,0xb2c4d6e8};
    VxImageDescEx desc = TransparencyImage(data, 3, 2, 4); bitmap.SetAlphaForTransparentColor(desc);
    const CKDWORD expected[] = {0x00102232,0xff000202,0xff102434,0xa1b3c5d7, 0x00102232,0xff102434,0xff000202,0xb2c4d6e8};
    for (int i = 0; i < 8; ++i) EXPECT_EQ(expected[i], data[i]);
}

TEST(CKBitmapDataTransparencyTest, SingleColumnColorKeyUsesTheRowStride) {
    CKBitmapData bitmap; bitmap.SetTransparentColor(0x00112233);
    CKDWORD data[] = {0x7f112233,0xa1b3c5d7,0xb2c4d6e8, 0x01010203,0xc3d5e7f9,0xd4e6f810};
    VxImageDescEx desc = TransparencyImage(data, 1, 2, 3); bitmap.SetAlphaForTransparentColor(desc);
    EXPECT_EQ(0x00102232u, data[0]); EXPECT_EQ(0xff000202u, data[3]);
    EXPECT_EQ(0xa1b3c5d7u, data[1]); EXPECT_EQ(0xb2c4d6e8u, data[2]);
    EXPECT_EQ(0xc3d5e7f9u, data[4]); EXPECT_EQ(0xd4e6f810u, data[5]);
}

TEST(CKBitmapDataTransparencyTest, EmptyAndNullImagesDoNotTouchTheSuppliedStorage) {
    CKBitmapData bitmap; CKDWORD pixel = 0x7f112233;
    VxImageDescEx desc = TransparencyImage(&pixel, 0, 1, 1); bitmap.SetAlphaForTransparentColor(desc);
    desc.Width = 1; desc.Height = 0; bitmap.SetAlphaForTransparentColor(desc);
    desc.Height = 1; desc.Image = nullptr; bitmap.SetAlphaForTransparentColor(desc); bitmap.SetBorderColorForClamp(desc);
    EXPECT_EQ(0x7f112233u, pixel);
}

TEST(CKBitmapDataTransparencyTest, TightClampClearsOnlyBorderAlphaAndPreservesRgbAndInterior) {
    CKBitmapData bitmap;
    CKDWORD pixels[] = {0xa0112233,0xb0223344,0xc0334455, 0xd0445566,0xe0556677,0xf0667788, 0x90778899,0x808899aa,0x7099aabb};
    VxImageDescEx desc = TransparencyImage(pixels, 3, 3, 3); bitmap.SetBorderColorForClamp(desc);
    const CKDWORD expected[] = {0x00112233,0x00223344,0x00334455, 0x00445566,0xe0556677,0x00667788, 0x00778899,0x008899aa,0x0099aabb};
    for (int i = 0; i < 9; ++i) EXPECT_EQ(expected[i], pixels[i]);
}

TEST(CKBitmapDataTransparencyTest, PaddedClampUsesTheVisibleRightEdgeAndLeavesPaddingUntouched) {
    CKBitmapData bitmap;
    CKDWORD data[] = {0xdeadbeef, 0xa0112233,0xb0223344,0xc0334455,0xa1b3c5d7,0xb2c4d6e8,
        0xd0445566,0xe0556677,0xf0667788,0xc3d5e7f9,0xd4e6f810,
        0x90778899,0x808899aa,0x7099aabb,0xe5f70911,0xf6081a22, 0xcafebabe};
    VxImageDescEx desc = TransparencyImage(data + 1, 3, 3, 5); bitmap.SetBorderColorForClamp(desc);
    const CKDWORD expected[] = {0xdeadbeef, 0x00112233,0x00223344,0x00334455,0xa1b3c5d7,0xb2c4d6e8,
        0x00445566,0xe0556677,0x00667788,0xc3d5e7f9,0xd4e6f810,
        0x00778899,0x008899aa,0x0099aabb,0xe5f70911,0xf6081a22, 0xcafebabe};
    for (int i = 0; i < 17; ++i) EXPECT_EQ(expected[i], data[i]);
}

TEST(CKBitmapDataTransparencyTest, SingleRowClampDoesNotModifyItsTrailingPadding) {
    CKBitmapData bitmap; CKDWORD pixels[] = {0xa0112233,0xb0223344,0xc0334455,0xa1b3c5d7,0xb2c4d6e8};
    VxImageDescEx desc = TransparencyImage(pixels, 3, 1, 5); bitmap.SetBorderColorForClamp(desc);
    const CKDWORD expected[] = {0x00112233,0x00223344,0x00334455,0xa1b3c5d7,0xb2c4d6e8};
    for (int i = 0; i < 5; ++i) EXPECT_EQ(expected[i], pixels[i]);
}

TEST(CKBitmapDataTransparencyTest, SingleColumnClampDoesNotModifyAnyRowPadding) {
    CKBitmapData bitmap; CKDWORD pixels[] = {0xa0112233,0xa1b3c5d7,0xb2c4d6e8, 0xb0223344,0xc3d5e7f9,0xd4e6f810};
    VxImageDescEx desc = TransparencyImage(pixels, 1, 2, 3); bitmap.SetBorderColorForClamp(desc);
    const CKDWORD expected[] = {0x00112233,0xa1b3c5d7,0xb2c4d6e8, 0x00223344,0xc3d5e7f9,0xd4e6f810};
    for (int i = 0; i < 6; ++i) EXPECT_EQ(expected[i], pixels[i]);
}

TEST(CKBitmapDataTransparencyTest, SinglePixelClampPreservesRgbAndGuardWords) {
    CKBitmapData bitmap; CKDWORD pixels[] = {0xdeadbeef,0xa0112233,0xa1b3c5d7,0xb2c4d6e8,0xcafebabe};
    VxImageDescEx desc = TransparencyImage(pixels + 1, 1, 1, 3); bitmap.SetBorderColorForClamp(desc);
    EXPECT_EQ(0xdeadbeefu, pixels[0]); EXPECT_EQ(0x00112233u, pixels[1]); EXPECT_EQ(0xa1b3c5d7u, pixels[2]);
    EXPECT_EQ(0xb2c4d6e8u, pixels[3]); EXPECT_EQ(0xcafebabeu, pixels[4]);
}

TEST(CKBitmapDataTransparencyTest, ClampIsIdempotentAndOnlySetsItsOwnBitmapFlag) {
    CKBitmapData bitmap; CKDWORD flags = 0x80000000u | CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_DYNAMIC;
    bitmap.m_BitmapFlags = flags; CKDWORD pixels[] = {0xaa112233,0xbb223344}; VxImageDescEx desc = TransparencyImage(pixels, 2, 1, 2);
    bitmap.SetBorderColorForClamp(desc); bitmap.SetBorderColorForClamp(desc);
    EXPECT_EQ(flags | CKBITMAPDATA_CLAMPUPTODATE, bitmap.m_BitmapFlags);
    EXPECT_EQ(0x00112233u, pixels[0]); EXPECT_EQ(0x00223344u, pixels[1]);
}

TEST(CKBitmapDataTransparencyTest, ApplyingColorKeyDoesNotChangeBitmapFlags) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = 0x80000000u | CKBITMAPDATA_DYNAMIC; bitmap.m_TransColor = 0x112233;
    CKDWORD pixel = 0x7f112233; VxImageDescEx desc = TransparencyImage(&pixel, 1, 1, 1); bitmap.SetAlphaForTransparentColor(desc);
    EXPECT_EQ(0x80000000u | CKBITMAPDATA_DYNAMIC, bitmap.m_BitmapFlags); EXPECT_EQ(0x00102232u, pixel);
}

TEST(CKBitmapDataTransparencyTest, ChangingColorWhileTransparencyIsDisabledDoesNotRequestRestore) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = CKBITMAPDATA_DYNAMIC; bitmap.SetTransparentColor(0xaabbccdd);
    EXPECT_EQ(0xaabbccddu, bitmap.GetTransparentColor()); EXPECT_EQ(CKBITMAPDATA_DYNAMIC, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataTransparencyTest, EnabledColorChangeRequestsRestoreOnlyWhenTheStoredColorChanges) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_DYNAMIC; bitmap.m_TransColor = 0x112233;
    bitmap.SetTransparentColor(0x112233); EXPECT_EQ(0, bitmap.ToRestore());
    bitmap.SetTransparentColor(0x80112233); EXPECT_EQ(CKBITMAPDATA_FORCERESTORE, bitmap.ToRestore());
    EXPECT_EQ(0x80112233u, bitmap.GetTransparentColor()); EXPECT_NE(0u, bitmap.m_BitmapFlags & CKBITMAPDATA_DYNAMIC);
}

TEST(CKBitmapDataTransparencyTest, EnablingTransparencyRequestsRestoreOnlyOnTheTransition) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = CKBITMAPDATA_DYNAMIC; bitmap.SetTransparent(-7);
    EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.IsTransparent()); EXPECT_EQ(CKBITMAPDATA_FORCERESTORE, bitmap.ToRestore());
    bitmap.m_BitmapFlags &= ~CKBITMAPDATA_FORCERESTORE; bitmap.SetTransparent(TRUE);
    EXPECT_EQ(0, bitmap.ToRestore()); EXPECT_EQ(CKBITMAPDATA_DYNAMIC | CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataTransparencyTest, DisablingTransparencyPreservesOtherBitsAndNativeRestorePolicy) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = CKBITMAPDATA_DYNAMIC | CKBITMAPDATA_TRANSPARENT;
    bitmap.SetTransparent(FALSE); EXPECT_EQ(CKBITMAPDATA_DYNAMIC, bitmap.m_BitmapFlags); EXPECT_EQ(0, bitmap.IsTransparent()); EXPECT_EQ(0, bitmap.ToRestore());
    bitmap.m_BitmapFlags |= CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_FORCERESTORE; bitmap.SetTransparent(FALSE);
    EXPECT_EQ(CKBITMAPDATA_DYNAMIC | CKBITMAPDATA_FORCERESTORE, bitmap.m_BitmapFlags);
}

TEST(CKBitmapDataTransparencyTest, TransparencyAndRestoreQueriesReturnTheirNativeMasks) {
    CKBitmapData bitmap; bitmap.m_BitmapFlags = 0x80000000u;
    EXPECT_EQ(0, bitmap.IsTransparent()); EXPECT_EQ(0, bitmap.ToRestore());
    bitmap.m_BitmapFlags |= CKBITMAPDATA_TRANSPARENT | CKBITMAPDATA_FORCERESTORE;
    EXPECT_EQ(2, bitmap.IsTransparent()); EXPECT_EQ(4, bitmap.ToRestore());
}

TEST(CKBitmapDataTransparencyTest, ColorKeyThenClampUsesDescriptorGeometryAndPreservesInteriorAlpha) {
    CKBitmapData bitmap; bitmap.m_Width = 99; bitmap.m_Height = 99; bitmap.SetTransparentColor(0x00112233);
    CKDWORD pixels[] = {0x7f112233,0xcc112435,0x01010203,0xa1b3c5d7, 0xcc112435,0x01010203,0xaa102232,0xb2c4d6e8,
        0x01010203,0xcc112435,0x7f112233,0xc3d5e7f9};
    VxImageDescEx desc = TransparencyImage(pixels, 3, 3, 4);
    bitmap.SetAlphaForTransparentColor(desc); bitmap.SetBorderColorForClamp(desc);
    const CKDWORD expected[] = {0x00102232,0x00102434,0x00000202,0xa1b3c5d7, 0x00102434,0xff000202,0x00102232,0xb2c4d6e8,
        0x00000202,0x00102434,0x00102232,0xc3d5e7f9};
    for (int i = 0; i < 12; ++i) EXPECT_EQ(expected[i], pixels[i]);
    EXPECT_EQ(99, bitmap.GetWidth()); EXPECT_EQ(99, bitmap.GetHeight());
}

TEST(CKBitmapDataTransparencyTest, HelpersProcessAnActualOwnedSlotSurfaceWithoutReplacingIt) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(3, 3)); CKBYTE *surface = bitmap.LockSurfacePtr(); ASSERT_NE(nullptr, surface);
    CKDWORD *pixels = reinterpret_cast<CKDWORD *>(surface); for (int i = 0; i < 9; ++i) pixels[i] = 0x7f010203;
    pixels[4] = 0xaa112233; bitmap.SetTransparentColor(0x00112233);
    VxImageDescEx desc; ASSERT_TRUE(bitmap.GetImageDesc(desc)); desc.Image = surface;
    bitmap.SetAlphaForTransparentColor(desc); bitmap.SetBorderColorForClamp(desc);
    EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(0x00102232u, bitmap.GetPixel(1, 1)); EXPECT_EQ(0x00000202u, bitmap.GetPixel(0, 0));
    EXPECT_TRUE(bitmap.ReleaseSurfacePtr()); EXPECT_EQ(4, bitmap.ToRestore());
}
