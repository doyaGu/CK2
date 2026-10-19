#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "CKBitmapReader.h"
#include "../../Plugins/ImageReader/PcxReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

struct PcxDecodeReader {
    CKBitmapReader *Reader;
    PcxDecodeReader() {
        CKGet_ImageReader_PluginInfo(2);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(2));
    }
    ~PcxDecodeReader() { if (Reader) Reader->Release(); }
};

static void PcxDecodeWord(CKBYTE *bytes, int offset, int value) {
    bytes[offset] = static_cast<CKBYTE>(value);
    bytes[offset + 1] = static_cast<CKBYTE>(value >> 8);
}

static void PcxDecodeHeader(CKBYTE *bytes, int width, int height, int bits, int planes,
                            int bytesPerLine, int encoding = 1) {
    memset(bytes, 0, 128);
    bytes[0] = 10; bytes[1] = 5; bytes[2] = static_cast<CKBYTE>(encoding);
    bytes[3] = static_cast<CKBYTE>(bits);
    PcxDecodeWord(bytes, 8, width - 1); PcxDecodeWord(bytes, 10, height - 1);
    bytes[65] = static_cast<CKBYTE>(planes);
    PcxDecodeWord(bytes, 66, bytesPerLine); bytes[68] = 1;
}

static void PcxDecodeKnownSample(CKBYTE (&bytes)[134]) {
    PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    const CKBYTE planes[] = {0x33, 0x66, 0x22, 0x55, 0x11, 0x44};
    memcpy(bytes + 128, planes, sizeof(planes));
}

static void PcxDecodeExpect(CKBitmapProperties *properties, int width, int height,
                             const CKBYTE *expected, size_t count) {
    ASSERT_NE(nullptr, properties);
    ASSERT_NE(nullptr, properties->m_Data);
    EXPECT_EQ(width, properties->m_Format.Width);
    EXPECT_EQ(height, properties->m_Format.Height);
    EXPECT_EQ(width * 4, properties->m_Format.BytesPerLine);
    EXPECT_EQ(32, properties->m_Format.BitsPerPixel);
    EXPECT_EQ(properties->m_Data, properties->m_Format.Image);
    ASSERT_EQ(static_cast<size_t>(width * height * 4), count);
    EXPECT_EQ(0, memcmp(expected, properties->m_Data, count));
}

static void PcxDecodeRejectPreservingImage(CKBYTE *bytes, int size) {
    PcxDecodeReader owner;
    CKBYTE good[134]; PcxDecodeKnownSample(good);
    CKBitmapProperties *before = nullptr, *after = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(good, sizeof(good), &before));
    void *pixels = before->m_Data;
    int result = owner.Reader->ReadMemory(bytes, size, &after);
    EXPECT_EQ(CKBITMAPERROR_FILECORRUPTED, result);
    if (result != 0) {
        EXPECT_EQ(before, after);
        EXPECT_EQ(pixels, after->m_Data);
        const CKBYTE expected[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
        PcxDecodeExpect(after, 2, 1, expected, sizeof(expected));
    }
}

TEST(CKPcxDecodeContractTest, RawRgbHonorsPlanePaddingAndNonzeroOrigin) {
    CKBYTE bytes[152]; PcxDecodeHeader(bytes, 3, 2, 8, 3, 4, 0);
    PcxDecodeWord(bytes, 4, 10); PcxDecodeWord(bytes, 8, 12);
    PcxDecodeWord(bytes, 6, 20); PcxDecodeWord(bytes, 10, 21);
    const CKBYTE planes[] = {1, 2, 3, 0xee, 4, 5, 6, 0xee, 7, 8, 9, 0xee,
                            10, 11, 12, 0xee, 13, 14, 15, 0xee, 16, 17, 18, 0xee};
    memcpy(bytes + 128, planes, sizeof(planes));
    const CKBYTE expected[] = {7,4,1,255, 8,5,2,255, 9,6,3,255,
                              16,13,10,255, 17,14,11,255, 18,15,12,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 3, 2, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, RleEscapedHighValuesProduceIndependentRgbPixels) {
    CKBYTE bytes[134]; PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    const CKBYTE encoded[] = {0xc2, 0xc1, 0xc2, 0xff, 0xc2, 0xc0};
    memcpy(bytes + 128, encoded, sizeof(encoded));
    const CKBYTE expected[] = {0xc0,0xff,0xc1,255, 0xc0,0xff,0xc1,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, ARunMayContinueAcrossColorPlanes) {
    CKBYTE bytes[130]; PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    bytes[128] = 0xc6; bytes[129] = 0x44;
    const CKBYTE expected[] = {0x44,0x44,0x44,255, 0x44,0x44,0x44,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, NativeCompatibleRunContinuationAcrossRowsIsNotDiscarded) {
    CKBYTE bytes[134]; PcxDecodeHeader(bytes, 2, 2, 8, 3, 2);
    const CKBYTE encoded[] = {0xc8,0x44, 0x11,0x22,0x33,0x55};
    memcpy(bytes + 128, encoded, sizeof(encoded));
    const CKBYTE expected[] = {0x44,0x44,0x44,255, 0x44,0x44,0x44,255,
                              0x33,0x11,0x44,255, 0x55,0x22,0x44,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 2, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, NativeZeroCountPacketDoesNotInventAnExtraPixel) {
    CKBYTE bytes[136]; PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    const CKBYTE encoded[] = {0xc0,0x99, 0x11,0x22,0x33,0x44,0x55,0x66};
    memcpy(bytes + 128, encoded, sizeof(encoded));
    const CKBYTE expected[] = {0x55,0x33,0x11,255, 0x66,0x44,0x22,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, MaximumRunCountFillsAllSixtyThreeDecodedBytes) {
    CKBYTE bytes[130]; PcxDecodeHeader(bytes, 21, 1, 8, 3, 21);
    bytes[128] = 0xff; bytes[129] = 0x17;
    CKBYTE expected[84];
    for (int i = 0; i < 21; ++i) {
        expected[i * 4] = expected[i * 4 + 1] = expected[i * 4 + 2] = 0x17;
        expected[i * 4 + 3] = 255;
    }
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 21, 1, expected, sizeof(expected));
}

static void PcxDecodePalette(CKBYTE *bytes, int offset) {
    bytes[offset] = 12; memset(bytes + offset + 1, 0, 768);
    const CKBYTE colors[] = {0x10,0x20,0x30, 0x40,0x50,0x60};
    memcpy(bytes + offset + 4, colors, sizeof(colors));
}

TEST(CKPcxDecodeContractTest, IndexedRowsUseTheTrailingPaletteWithoutChangingPixelIndices) {
    CKBYTE bytes[901]; PcxDecodeHeader(bytes, 2, 2, 8, 1, 2);
    bytes[128] = 1; bytes[129] = 2; bytes[130] = 2; bytes[131] = 1;
    PcxDecodePalette(bytes, 132);
    const CKBYTE expected[] = {0x30,0x20,0x10,255, 0x60,0x50,0x40,255,
                              0x60,0x50,0x40,255, 0x30,0x20,0x10,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 2, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, CompleteIndexedImageWithoutPaletteKeepsTheGrayscaleFallback) {
    CKBYTE bytes[130]; PcxDecodeHeader(bytes, 2, 1, 8, 1, 2);
    bytes[128] = 0x11; bytes[129] = 0x77;
    const CKBYTE expected[] = {0x11,0x11,0x11,255, 0x77,0x77,0x77,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, PaletteImmediatelyFollowingPixelsStillWorksWithTrailingMetadata) {
    CKBYTE bytes[902]; PcxDecodeHeader(bytes, 2, 1, 8, 1, 2);
    bytes[128] = 1; bytes[129] = 2; PcxDecodePalette(bytes, 130);
    bytes[899] = 'E'; bytes[900] = 'N'; bytes[901] = 'D';
    const CKBYTE expected[] = {0x30,0x20,0x10,255, 0x60,0x50,0x40,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, PlanarAndPackedLowBitDepthRowsKeepTheirHeaderPalette) {
    const int bits[] = {1,1,2,4}; const int planes[] = {1,4,1,1};
    for (int i = 0; i < 4; ++i) {
        SCOPED_TRACE(i);
        CKBYTE bytes[132]; PcxDecodeHeader(bytes, 2, 1, bits[i], planes[i], 1, 0);
        const CKBYTE colors[] = {0x10,0x20,0x30, 0x40,0x50,0x60};
        memcpy(bytes + 16, colors, sizeof(colors));
        memset(bytes + 128, 0, 4);
        bytes[128] = bits[i] == 1 ? 0x40 : bits[i] == 2 ? 0x10 : 0x01;
        const CKBYTE expected[] = {0x30,0x20,0x10,255, 0x60,0x50,0x40,255};
        PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, 128 + planes[i], &properties));
        PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
    }
}

TEST(CKPcxDecodeContractTest, FourthColorPlaneSuppliesActualAlphaValues) {
    CKBYTE bytes[136]; PcxDecodeHeader(bytes, 2, 1, 8, 4, 2, 0);
    const CKBYTE planes[] = {1,2, 3,4, 5,6, 7,8};
    memcpy(bytes + 128, planes, sizeof(planes));
    const CKBYTE expected[] = {5,3,1,7, 6,4,2,8};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, EveryRawPayloadTruncationLeavesThePreviousImageIntact) {
    CKBYTE bytes[134]; PcxDecodeKnownSample(bytes); bytes[2] = 0;
    for (int size = 128; size < 134; ++size) {
        SCOPED_TRACE(size); PcxDecodeRejectPreservingImage(bytes, size);
    }
}

TEST(CKPcxDecodeContractTest, EveryRleLiteralPayloadTruncationLeavesThePreviousImageIntact) {
    CKBYTE bytes[134]; PcxDecodeKnownSample(bytes);
    for (int size = 128; size < 134; ++size) {
        SCOPED_TRACE(size); PcxDecodeRejectPreservingImage(bytes, size);
    }
}

TEST(CKPcxDecodeContractTest, MissingRunValueCannotBorrowAByteFromTheVgaPalette) {
    CKBYTE bytes[898]; PcxDecodeHeader(bytes, 2, 1, 8, 1, 2);
    bytes[128] = 0xc2; PcxDecodePalette(bytes, 129);
    PcxDecodeRejectPreservingImage(bytes, sizeof(bytes));
}

TEST(CKPcxDecodeContractTest, PaletteBytesCannotCompleteAShortIndexedImage) {
    for (int encoding = 0; encoding < 2; ++encoding) {
        SCOPED_TRACE(encoding);
        CKBYTE bytes[898]; PcxDecodeHeader(bytes, 2, 1, 8, 1, 2, encoding);
        bytes[128] = 1; PcxDecodePalette(bytes, 129);
        PcxDecodeRejectPreservingImage(bytes, sizeof(bytes));
    }
}

TEST(CKPcxDecodeContractTest, RunBeyondTheEntireImageIsRejectedInsteadOfClipped) {
    CKBYTE bytes[130]; PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    bytes[128] = 0xc7; bytes[129] = 0x44;
    PcxDecodeRejectPreservingImage(bytes, sizeof(bytes));
}

TEST(CKPcxDecodeContractTest, AnUndersizedPlaneStrideCannotProduceAnInventedBlackColumn) {
    const int bits[] = {1,2,4,8,8,8}; const int planes[] = {1,1,1,1,3,4};
    for (int i = 0; i < 6; ++i) {
        SCOPED_TRACE(i);
        CKBYTE bytes[132]; PcxDecodeHeader(bytes, 8 / bits[i] + 1, 1, bits[i], planes[i], 1, 0);
        memset(bytes + 128, 0x11, 4);
        PcxDecodeRejectPreservingImage(bytes, 128 + planes[i]);
    }
}

TEST(CKPcxDecodeContractTest, MissingFinalRowIsNotReportedAsSuccessfulBlackPixels) {
    for (int encoding = 0; encoding < 2; ++encoding) {
        SCOPED_TRACE(encoding);
        CKBYTE bytes[134]; PcxDecodeKnownSample(bytes);
        bytes[2] = static_cast<CKBYTE>(encoding); bytes[10] = 1;
        PcxDecodeRejectPreservingImage(bytes, sizeof(bytes));
    }
}

TEST(CKPcxDecodeContractTest, FileDecodeFailurePreservesThePreviousImageAndAllowsRetry) {
    PcxDecodeReader owner; CKBYTE bytes[134]; PcxDecodeKnownSample(bytes);
    CKBitmapProperties *before = nullptr, *after = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &before));
    void *pixels = before->m_Data;
    char name[96]; std::snprintf(name, sizeof(name), "ck-pcx-truncated-%p.pcx", &owner);
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    FILE *stream = std::fopen(path.string().c_str(), "wb");
    ASSERT_NE(nullptr, stream);
    size_t written = std::fwrite(bytes, 1, 129, stream);
    int closed = std::fclose(stream);
    ASSERT_EQ(129u, written); ASSERT_EQ(0, closed);
    int result = owner.Reader->ReadFile(const_cast<char *>(path.string().c_str()), &after);
    std::error_code error; std::filesystem::remove(path, error);
    EXPECT_EQ(CKBITMAPERROR_FILECORRUPTED, result);
    if (result != 0) EXPECT_EQ(pixels, after->m_Data);
    EXPECT_EQ(before, after);
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &after));
    const CKBYTE expected[] = {0x11,0x22,0x33,255, 0x44,0x55,0x66,255};
    PcxDecodeExpect(after, 2, 1, expected, sizeof(expected));
}

TEST(CKPcxDecodeContractTest, RepeatedSingleBytePacketsPreserveAllSixPlaneValues) {
    CKBYTE bytes[140]; PcxDecodeHeader(bytes, 2, 1, 8, 3, 2);
    const CKBYTE values[] = {0x11,0x22,0x33,0x44,0x55,0x66};
    for (int i = 0; i < 6; ++i) { bytes[128 + i * 2] = 0xc1; bytes[129 + i * 2] = values[i]; }
    const CKBYTE expected[] = {0x55,0x33,0x11,255, 0x66,0x44,0x22,255};
    PcxDecodeReader owner; CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    PcxDecodeExpect(properties, 2, 1, expected, sizeof(expected));
}
