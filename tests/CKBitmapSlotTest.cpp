#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "VxMath.h"

static struct BitmapSlotTrace {
    struct Event { char Kind; void *Pointer; size_t Bytes; size_t Alignment; } Events[128];
    void *Live[64];
    int Count = 0, LiveCount = 0, Allocations = 0, Deletions = 0, Resizes = 0;
    const VxImageDescEx *Source = nullptr, *Destination = nullptr;
    void *SourcePixels = nullptr, *DestinationPixels = nullptr;
    void Record(char kind, void *pointer, size_t bytes = 0, size_t alignment = 0) {
        ASSERT_LT(Count, 128); Events[Count++] = {kind, pointer, bytes, alignment};
    }
    void Reset() { Count = LiveCount = Allocations = Deletions = Resizes = 0; Source = Destination = nullptr; SourcePixels = DestinationPixels = nullptr; }
} SlotTrace;

// Observe dependency calls while retaining the real fixed-SDK allocator and resizer.
static void *BitmapSlotNewAligned(size_t bytes, size_t alignment) {
    void *pointer = VxNewAligned(bytes, alignment);
    SlotTrace.Record('N', pointer, bytes, alignment); ++SlotTrace.Allocations;
    if (pointer) { EXPECT_LT(SlotTrace.LiveCount, 64); SlotTrace.Live[SlotTrace.LiveCount++] = pointer; }
    return pointer;
}

static void BitmapSlotDeleteAligned(void *pointer) {
    SlotTrace.Record('D', pointer);
    if (pointer) {
        int i = 0;
        while (i < SlotTrace.LiveCount && SlotTrace.Live[i] != pointer) ++i;
        ASSERT_LT(i, SlotTrace.LiveCount) << "Unexpected or repeated ownership release";
        SlotTrace.Live[i] = SlotTrace.Live[--SlotTrace.LiveCount]; ++SlotTrace.Deletions;
    }
    VxDeleteAligned(pointer);
}

static void BitmapSlotResizeImage32(const VxImageDescEx &source, const VxImageDescEx &destination) {
    SlotTrace.Record('R', destination.Image); ++SlotTrace.Resizes;
    SlotTrace.Source = &source; SlotTrace.Destination = &destination;
    SlotTrace.SourcePixels = source.Image; SlotTrace.DestinationPixels = destination.Image;
    VxResizeImage32(source, destination);
}

#ifndef CK_LIB
#define CK_LIB
#endif
#define VxNewAligned BitmapSlotNewAligned
#define VxDeleteAligned BitmapSlotDeleteAligned
#define VxResizeImage32 BitmapSlotResizeImage32
#include "../include/CKBitmapData.h"
#undef VxResizeImage32
#undef VxDeleteAligned
#undef VxNewAligned

static VxImageDescEx BitmapSlotDesc(int width, int height, int pitch = 0) {
    VxImageDescEx desc;
    desc.Width = width; desc.Height = height; desc.BytesPerLine = pitch ? pitch : width * 4;
    desc.BitsPerPixel = 32; desc.RedMask = R_MASK; desc.GreenMask = G_MASK;
    desc.BlueMask = B_MASK; desc.AlphaMask = A_MASK;
    return desc;
}

static void BitmapSlotPixels(CKBitmapSlot &slot, const CKDWORD *expected, int count) {
    ASSERT_NE(nullptr, slot.m_DataBuffer);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(slot.m_DataBuffer) & 15u);
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], slot.m_DataBuffer[i]) << "pixel " << i;
}

class CKBitmapSlotTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(0, SlotTrace.LiveCount); SlotTrace.Reset(); }
    void TearDown() override { EXPECT_EQ(0, SlotTrace.LiveCount); EXPECT_EQ(SlotTrace.Allocations, SlotTrace.Deletions); }
};

TEST_F(CKBitmapSlotTest, LayoutMatchesNativeWin32AndPortableX64Storage) {
    EXPECT_EQ(0u, offsetof(CKBitmapSlot, m_DataBuffer));
    EXPECT_EQ(sizeof(void *), offsetof(CKBitmapSlot, m_FileName));
    EXPECT_EQ(sizeof(void *) == 4 ? 12u : 24u, sizeof(CKBitmapSlot));
    EXPECT_EQ(sizeof(void *), alignof(CKBitmapSlot));
    EXPECT_TRUE(std::is_standard_layout<CKBitmapSlot>::value);
    EXPECT_FALSE(std::is_polymorphic<CKBitmapSlot>::value);
    EXPECT_FALSE(std::has_virtual_destructor<CKBitmapSlot>::value);
}

TEST_F(CKBitmapSlotTest, ConstructorHasNoPixelAllocationAndStartsWithAnEmptyName) {
    CKBitmapSlot slot;
    EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(0, slot.m_FileName.Length());
    EXPECT_EQ(0, SlotTrace.Count); EXPECT_EQ(0, SlotTrace.Allocations);
}

TEST_F(CKBitmapSlotTest, AllocateRequestsExactByteCountAndSixteenByteAlignment) {
    const int depths[] = {8, 16, 24, 32};
    for (int i = 0; i < 4; ++i) {
        CKBitmapSlot slot; slot.Allocate(3, 2, depths[i]);
        const BitmapSlotTrace::Event &request = SlotTrace.Events[SlotTrace.Count - 1];
        EXPECT_EQ('N', request.Kind); EXPECT_EQ(static_cast<size_t>(6 * depths[i] / 8), request.Bytes);
        EXPECT_EQ(16u, request.Alignment); ASSERT_NE(nullptr, slot.m_DataBuffer);
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(slot.m_DataBuffer) & 15u);
        memset(slot.m_DataBuffer, 0x5a, request.Bytes);
        EXPECT_EQ(0x5a, reinterpret_cast<CKBYTE *>(slot.m_DataBuffer)[request.Bytes - 1]);
    }
}

TEST_F(CKBitmapSlotTest, AllocateReleasesOldStorageBeforeRequestingItsReplacement) {
    CKBitmapSlot slot; slot.Allocate(2, 2, 32); void *old = slot.m_DataBuffer; const int first = SlotTrace.Count;
    slot.Allocate(3, 1, 32);
    ASSERT_EQ(first + 2, SlotTrace.Count); EXPECT_EQ('D', SlotTrace.Events[first].Kind); EXPECT_EQ(old, SlotTrace.Events[first].Pointer);
    EXPECT_EQ('N', SlotTrace.Events[first + 1].Kind); EXPECT_EQ(12u, SlotTrace.Events[first + 1].Bytes);
    EXPECT_EQ(1, SlotTrace.LiveCount); EXPECT_EQ(1, SlotTrace.Deletions);
}

TEST_F(CKBitmapSlotTest, AllocationAndFlushKeepTheOriginalFilename) {
    CKBitmapSlot slot; slot.m_FileName = "folder/source.image";
    slot.Allocate(2, 1, 32); EXPECT_STREQ("folder/source.image", slot.m_FileName.CStr());
    slot.Allocate(1, 3, 32); EXPECT_STREQ("folder/source.image", slot.m_FileName.CStr());
    slot.Flush(); EXPECT_STREQ("folder/source.image", slot.m_FileName.CStr()); EXPECT_EQ(nullptr, slot.m_DataBuffer);
}

TEST_F(CKBitmapSlotTest, FlushClearsOwnershipAndRepeatedFlushDoesNotFreeTwice) {
    CKBitmapSlot slot; slot.Allocate(1, 1, 32); void *pixels = slot.m_DataBuffer;
    slot.Flush(); EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(1, SlotTrace.Deletions);
    EXPECT_EQ(pixels, SlotTrace.Events[SlotTrace.Count - 1].Pointer);
    slot.Flush(); EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(1, SlotTrace.Deletions);
    EXPECT_EQ(nullptr, SlotTrace.Events[SlotTrace.Count - 1].Pointer);
}

TEST_F(CKBitmapSlotTest, FreeClearsPixelsAndFilenameAndCanBeRepeated) {
    CKBitmapSlot slot; slot.Allocate(2, 2, 32); slot.m_FileName = "erase.image";
    slot.Free(); EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(0, slot.m_FileName.Length()); EXPECT_EQ(1, SlotTrace.Deletions);
    slot.Free(); EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(0, slot.m_FileName.Length()); EXPECT_EQ(1, SlotTrace.Deletions);
}

TEST_F(CKBitmapSlotTest, FreeOfANamedEmptySlotStillClearsTheName) {
    CKBitmapSlot slot; slot.m_FileName = "external.image"; slot.Free();
    EXPECT_EQ(nullptr, slot.m_DataBuffer); EXPECT_EQ(0, slot.m_FileName.Length());
    EXPECT_EQ(0, SlotTrace.Allocations); EXPECT_EQ(0, SlotTrace.Deletions);
}

TEST_F(CKBitmapSlotTest, DestructorReleasesOwnedPixelsExactlyOnce) {
    void *pixels = nullptr;
    { CKBitmapSlot slot; slot.Allocate(3, 2, 32); slot.m_FileName = "owned.image"; pixels = slot.m_DataBuffer; }
    EXPECT_EQ(1, SlotTrace.Deletions); EXPECT_EQ(0, SlotTrace.LiveCount);
    EXPECT_EQ('D', SlotTrace.Events[SlotTrace.Count - 1].Kind); EXPECT_EQ(pixels, SlotTrace.Events[SlotTrace.Count - 1].Pointer);
}

TEST_F(CKBitmapSlotTest, DestructorAfterFlushDoesNotReleaseTheOldPixelsAgain) {
    { CKBitmapSlot slot; slot.Allocate(1, 1, 32); slot.Flush(); }
    EXPECT_EQ(1, SlotTrace.Deletions); EXPECT_EQ(0, SlotTrace.LiveCount);
    EXPECT_EQ(nullptr, SlotTrace.Events[SlotTrace.Count - 1].Pointer);
}

TEST_F(CKBitmapSlotTest, DestructorOfEmptySlotOnlyPassesNullToTheAllocator) {
    { CKBitmapSlot slot; slot.m_FileName = "name.only"; }
    ASSERT_EQ(1, SlotTrace.Count); EXPECT_EQ('D', SlotTrace.Events[0].Kind); EXPECT_EQ(nullptr, SlotTrace.Events[0].Pointer);
    EXPECT_EQ(0, SlotTrace.Deletions);
}

TEST_F(CKBitmapSlotTest, ResizeEmptySlotCreatesOpaqueBlackAndKeepsItsName) {
    CKBitmapSlot slot; slot.m_FileName = "empty.image"; VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(3, 2);
    slot.Resize(src, dst); const CKDWORD expected[] = {0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000};
    BitmapSlotPixels(slot, expected, 6); EXPECT_EQ(0, SlotTrace.Resizes); EXPECT_STREQ("empty.image", slot.m_FileName.CStr());
    EXPECT_EQ(24u, SlotTrace.Events[0].Bytes);
}

TEST_F(CKBitmapSlotTest, ResizeEmptySlotLeavesCallerDescriptorImagesUntouched) {
    CKDWORD sourceMarker = 0x11223344, destinationMarker = 0x55667788;
    CKBitmapSlot slot; VxImageDescEx src = BitmapSlotDesc(2, 1), dst = BitmapSlotDesc(1, 2);
    src.Image = reinterpret_cast<CKBYTE *>(&sourceMarker); dst.Image = reinterpret_cast<CKBYTE *>(&destinationMarker);
    slot.Resize(src, dst);
    EXPECT_EQ(&sourceMarker, static_cast<void *>(src.Image)); EXPECT_EQ(&destinationMarker, static_cast<void *>(dst.Image));
    EXPECT_EQ(0x11223344u, sourceMarker); EXPECT_EQ(0x55667788u, destinationMarker);
    EXPECT_NE(dst.Image, reinterpret_cast<CKBYTE *>(slot.m_DataBuffer)); EXPECT_EQ(0, SlotTrace.Resizes);
}

TEST_F(CKBitmapSlotTest, ResizeAllocatesThenResamplesBeforeReleasingOldStorage) {
    CKBitmapSlot slot; slot.Allocate(1, 1, 32); slot.m_DataBuffer[0] = 0x12345678; void *old = slot.m_DataBuffer;
    VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(2, 2); const int first = SlotTrace.Count;
    slot.Resize(src, dst);
    ASSERT_EQ(first + 3, SlotTrace.Count); EXPECT_EQ('N', SlotTrace.Events[first].Kind);
    EXPECT_EQ('R', SlotTrace.Events[first + 1].Kind); EXPECT_EQ('D', SlotTrace.Events[first + 2].Kind);
    EXPECT_EQ(old, SlotTrace.Events[first + 2].Pointer); EXPECT_NE(old, slot.m_DataBuffer);
    EXPECT_EQ(&src, SlotTrace.Source); EXPECT_EQ(&dst, SlotTrace.Destination);
    EXPECT_EQ(old, SlotTrace.SourcePixels); EXPECT_EQ(slot.m_DataBuffer, SlotTrace.DestinationPixels);
    EXPECT_EQ(1, SlotTrace.LiveCount);
}

TEST_F(CKBitmapSlotTest, ResizeSameDimensionsStillAllocatesAnIndependentReplacement) {
    CKBitmapSlot slot; slot.Allocate(2, 2, 32); const CKDWORD expected[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f010};
    memcpy(slot.m_DataBuffer, expected, sizeof(expected)); void *old = slot.m_DataBuffer;
    VxImageDescEx src = BitmapSlotDesc(2, 2), dst = BitmapSlotDesc(2, 2); slot.Resize(src, dst);
    EXPECT_NE(old, slot.m_DataBuffer); EXPECT_EQ(1, SlotTrace.Resizes); BitmapSlotPixels(slot, expected, 4);
}

TEST_F(CKBitmapSlotTest, ResizeOnePixelReplicatesAllArgbChannels) {
    CKBitmapSlot slot; slot.Allocate(1, 1, 32); slot.m_DataBuffer[0] = 0x12345678;
    VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(3, 2); slot.Resize(src, dst);
    const CKDWORD expected[] = {0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678};
    BitmapSlotPixels(slot, expected, 6);
}

TEST_F(CKBitmapSlotTest, ResizeInterpolatesAnIndependentFourByFourArgbGrid) {
    CKBitmapSlot slot; slot.Allocate(2, 2, 32);
    const CKDWORD input[] = {0x00102030, 0x40506070, 0x8090a0b0, 0xc0d0e0f0}; memcpy(slot.m_DataBuffer, input, sizeof(input));
    VxImageDescEx src = BitmapSlotDesc(2, 2), dst = BitmapSlotDesc(4, 4); slot.Resize(src, dst);
    const CKDWORD expected[] = {0x00102030, 0x20304050, 0x40506070, 0x40506070,
        0x40506070, 0x60708090, 0x8090a0b0, 0x8090a0b0,
        0x8090a0b0, 0xa0b0c0d0, 0xc0d0e0f0, 0xc0d0e0f0,
        0x8090a0b0, 0xa0b0c0d0, 0xc0d0e0f0, 0xc0d0e0f0};
    BitmapSlotPixels(slot, expected, 16);
}

TEST_F(CKBitmapSlotTest, ResizeDownsamplesTheCoveredArgbPixels) {
    CKBitmapSlot slot; slot.Allocate(4, 4, 32);
    const CKDWORD input[] = {0x10203040, 0x11111111, 0x50607080, 0x22222222,
        0x33333333, 0x44444444, 0x55555555, 0x66666666,
        0x90a0b0c0, 0x77777777, 0xd0e0f010, 0x88888888,
        0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc};
    memcpy(slot.m_DataBuffer, input, sizeof(input)); VxImageDescEx src = BitmapSlotDesc(4, 4), dst = BitmapSlotDesc(2, 2); slot.Resize(src, dst);
    // Each output is the channel-wise mean of its four covered source pixels.
    const CKDWORD expected[] = {0x262a2e32, 0x4b4f5357, 0x92969a9e, 0xb7bbbf87}; BitmapSlotPixels(slot, expected, 4);
}

TEST_F(CKBitmapSlotTest, ResizeReadsSourceRowsUsingTheSuppliedPitch) {
    CKBitmapSlot slot; slot.Allocate(3, 2, 32);
    const CKDWORD input[] = {0x10203040, 0x50607080, 0xdeadbeef, 0x90a0b0c0, 0xd0e0f010, 0xcafebabe};
    memcpy(slot.m_DataBuffer, input, sizeof(input)); VxImageDescEx src = BitmapSlotDesc(2, 2, 12), dst = BitmapSlotDesc(2, 2); slot.Resize(src, dst);
    const CKDWORD expected[] = {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f010}; BitmapSlotPixels(slot, expected, 4);
}

TEST_F(CKBitmapSlotTest, ResizeUpdatesOnlyImagePointersInLiveSourceDescriptors) {
    CKBYTE sourceMarker = 1, destinationMarker = 2;
    CKBitmapSlot slot; slot.Allocate(1, 1, 32); slot.m_DataBuffer[0] = 0xffaabbcc; void *old = slot.m_DataBuffer;
    VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(2, 3);
    src.Image = &sourceMarker; dst.Image = &destinationMarker; slot.Resize(src, dst);
    EXPECT_EQ(old, static_cast<void *>(src.Image)); EXPECT_EQ(slot.m_DataBuffer, static_cast<void *>(dst.Image));
    EXPECT_EQ(1, src.Width); EXPECT_EQ(1, src.Height); EXPECT_EQ(4, src.BytesPerLine);
    EXPECT_EQ(2, dst.Width); EXPECT_EQ(3, dst.Height); EXPECT_EQ(8, dst.BytesPerLine);
    EXPECT_EQ(32, src.BitsPerPixel); EXPECT_EQ(A_MASK, dst.AlphaMask); EXPECT_EQ(R_MASK, src.RedMask);
    EXPECT_EQ(1, sourceMarker); EXPECT_EQ(2, destinationMarker);
}

TEST_F(CKBitmapSlotTest, ResizeKeepsFilenameAndOtherSlotsIndependent) {
    CKBitmapSlot first, second; first.Allocate(1, 1, 32); second.Allocate(1, 1, 32);
    first.m_DataBuffer[0] = 0xff112233; second.m_DataBuffer[0] = 0x80445566;
    first.m_FileName = "first.image"; second.m_FileName = "second.image"; void *peer = second.m_DataBuffer;
    VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(2, 2); first.Resize(src, dst);
    EXPECT_STREQ("first.image", first.m_FileName.CStr()); EXPECT_STREQ("second.image", second.m_FileName.CStr());
    EXPECT_EQ(peer, second.m_DataBuffer); EXPECT_EQ(0x80445566u, second.m_DataBuffer[0]); EXPECT_EQ(2, SlotTrace.LiveCount);
}

TEST_F(CKBitmapSlotTest, FlushAndFreeDoNotReleaseAnotherSlotsPixels) {
    CKBitmapSlot first, second; first.Allocate(1, 1, 32); second.Allocate(1, 1, 32);
    second.m_DataBuffer[0] = 0x12345678; void *peer = second.m_DataBuffer;
    first.Flush(); first.Free(); EXPECT_EQ(1, SlotTrace.LiveCount); EXPECT_EQ(peer, second.m_DataBuffer);
    EXPECT_EQ(0x12345678u, second.m_DataBuffer[0]);
}

TEST_F(CKBitmapSlotTest, FreeThenResizeCreatesBlackWithoutReusingTheOldName) {
    CKBitmapSlot slot; slot.Allocate(1, 1, 32); slot.m_DataBuffer[0] = 0x12345678; slot.m_FileName = "old.image"; slot.Free();
    VxImageDescEx src = BitmapSlotDesc(1, 1), dst = BitmapSlotDesc(2, 1); slot.Resize(src, dst);
    const CKDWORD expected[] = {0xff000000, 0xff000000}; BitmapSlotPixels(slot, expected, 2);
    EXPECT_EQ(0, slot.m_FileName.Length()); EXPECT_EQ(0, SlotTrace.Resizes);
}

TEST_F(CKBitmapSlotTest, FilenameOwnsItsStringStorageIndependentlyOfTheInput) {
    CKBitmapSlot slot; XString original("folder/source.image"); slot.m_FileName = original;
    original = "changed.image"; EXPECT_STREQ("folder/source.image", slot.m_FileName.CStr());
    slot.Allocate(1, 1, 32); slot.Flush(); EXPECT_STREQ("folder/source.image", slot.m_FileName.CStr());
}
