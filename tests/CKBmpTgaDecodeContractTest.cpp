#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "CKBitmapReader.h"
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

struct BmpTgaDecodeReader {
    CKBitmapReader *Reader;
    explicit BmpTgaDecodeReader(int index) {
        CKGet_ImageReader_PluginInfo(index);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~BmpTgaDecodeReader() { if (Reader) Reader->Release(); }
};

static void BmpTgaPut(CKBYTE *bytes, int offset, CKDWORD value, int count = 4) {
    for (int i = 0; i < count; ++i) bytes[offset + i] = static_cast<CKBYTE>(value >> (8 * i));
}

static const CKBYTE BmpTgaPalette[4][4] = {
    {0x10,0x20,0x30,255}, {0x40,0x50,0x60,255},
    {0x70,0x80,0x90,255}, {0xa0,0xb0,0xc0,255}
};

static int BmpTgaMakeBmp(CKBYTE (&bytes)[256], int width, int height, int depth,
                         int compression, const CKBYTE *payload, int count, int paletteCount = 0) {
    memset(bytes, 0, sizeof(bytes));
    const int offset = 54 + 4 * paletteCount;
    bytes[0] = 'B'; bytes[1] = 'M';
    BmpTgaPut(bytes, 2, offset + count); BmpTgaPut(bytes, 10, offset); BmpTgaPut(bytes, 14, 40);
    BmpTgaPut(bytes, 18, width); BmpTgaPut(bytes, 22, static_cast<CKDWORD>(height));
    BmpTgaPut(bytes, 26, 1, 2); BmpTgaPut(bytes, 28, depth, 2);
    BmpTgaPut(bytes, 30, compression); BmpTgaPut(bytes, 34, count); BmpTgaPut(bytes, 46, paletteCount);
    for (int i = 0; i < paletteCount; ++i) memcpy(bytes + 54 + i * 4, BmpTgaPalette[i], 3);
    memcpy(bytes + offset, payload, count);
    return offset + count;
}

static void BmpTgaExpectPixels(CKBitmapProperties *properties, int width, int height,
                                const CKBYTE *expected, size_t count) {
    ASSERT_NE(nullptr, properties); ASSERT_NE(nullptr, properties->m_Data);
    EXPECT_EQ(width, properties->m_Format.Width); EXPECT_EQ(height, properties->m_Format.Height);
    EXPECT_EQ(width * 4, properties->m_Format.BytesPerLine);
    EXPECT_EQ(32, properties->m_Format.BitsPerPixel);
    EXPECT_EQ(0xff000000u, properties->m_Format.AlphaMask);
    EXPECT_EQ(properties->m_Data, properties->m_Format.Image);
    ASSERT_EQ(static_cast<size_t>(width * height * 4), count);
    EXPECT_EQ(0, memcmp(expected, properties->m_Data, count));
}

static void BmpTgaExpectIndices(CKBitmapProperties *properties, int width, int height,
                                 const CKBYTE *indices, int count) {
    CKBYTE expected[64]; ASSERT_LE(count, 16);
    for (int i = 0; i < count; ++i) memcpy(expected + i * 4, BmpTgaPalette[indices[i]], 4);
    BmpTgaExpectPixels(properties, width, height, expected, count * 4);
}

TEST(CKBmpTgaDecodeContractTest, BmpRgb32BottomUpPreservesAllAlphaBytes) {
    const CKBYTE payload[] = {7,8,9,128, 10,11,12,255, 1,2,3,0, 4,5,6,127};
    const CKBYTE expected[] = {1,2,3,0, 4,5,6,127, 7,8,9,128, 10,11,12,255};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, 2, 32, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    BmpTgaExpectPixels(properties, 2, 2, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, BmpRgb32TopDownPreservesAllAlphaBytes) {
    const CKBYTE payload[] = {1,2,3,0, 4,5,6,127, 7,8,9,128, 10,11,12,255};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, -2, 32, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    BmpTgaExpectPixels(properties, 2, 2, payload, sizeof(payload));
}

TEST(CKBmpTgaDecodeContractTest, BmpRgb32DoesNotReplaceFullyTransparentAlphaWithOpaque) {
    const CKBYTE payload[] = {1,2,3,0, 4,5,6,0};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, 1, 32, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    BmpTgaExpectPixels(properties, 2, 1, payload, sizeof(payload));
}

TEST(CKBmpTgaDecodeContractTest, BmpFileReadKeepsTheStoredRgb32Transparency) {
    const CKBYTE payload[] = {1,2,3,37, 4,5,6,128};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, 1, 32, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0);
    char name[96]; std::snprintf(name, sizeof(name), "ck-bmp-alpha-%p.bmp", &owner);
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    FILE *file = std::fopen(path.string().c_str(), "wb"); ASSERT_NE(nullptr, file);
    size_t written = std::fwrite(bytes, 1, size, file); int closed = std::fclose(file);
    ASSERT_EQ(static_cast<size_t>(size), written); ASSERT_EQ(0, closed);
    CKBitmapProperties *properties = nullptr;
    int result = owner.Reader->ReadFile(const_cast<char *>(path.string().c_str()), &properties);
    std::error_code error; std::filesystem::remove(path, error);
    ASSERT_EQ(0, result);
    BmpTgaExpectPixels(properties, 2, 1, payload, sizeof(payload));
}

TEST(CKBmpTgaDecodeContractTest, BmpRgb24PaddingCannotBecomeAlpha) {
    const CKBYTE payload[] = {4,5,6,0, 1,2,3,0xcc};
    const CKBYTE expected[] = {1,2,3,255, 4,5,6,255};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 1, 2, 24, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    BmpTgaExpectPixels(properties, 1, 2, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, BmpRgb16UsesOpaqueRgb555) {
    const CKBYTE payload[] = {0x00,0x7c, 0xe0,0x03};
    const CKBYTE expected[] = {0,0,255,255, 0,255,0,255};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, 1, 16, BI_RGB, payload, sizeof(payload));
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    BmpTgaExpectPixels(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, BmpCoreHeaderReadsThreeBytePaletteEntries) {
    CKBYTE bytes[36] = {};
    bytes[0] = 'B'; bytes[1] = 'M'; bytes[2] = sizeof(bytes); bytes[10] = 32;
    bytes[14] = 12; bytes[18] = 2; bytes[20] = 1; bytes[22] = 1; bytes[24] = 1;
    memcpy(bytes + 26, BmpTgaPalette[0], 3); memcpy(bytes + 29, BmpTgaPalette[1], 3);
    bytes[32] = 0x40;
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    const CKBYTE indices[] = {0,1}; BmpTgaExpectIndices(properties, 2, 1, indices, 2);
}

TEST(CKBmpTgaDecodeContractTest, BmpInfoPaletteReservedBytesDoNotSupplyPixelAlpha) {
    const CKBYTE payload[] = {0,1,0,0};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 2, 1, 8, BI_RGB, payload, sizeof(payload), 2);
    bytes[57] = 0; bytes[61] = 37;
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {0,1}; BmpTgaExpectIndices(properties, 2, 1, indices, 2);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle8EarlyEndUsesPaletteZeroForUntouchedPixels) {
    const CKBYTE payload[] = {1,1,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 4, 1, 8, BI_RLE8, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {1,0,0,0}; BmpTgaExpectIndices(properties, 4, 1, indices, 4);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle4EarlyEndUsesPaletteZeroForUntouchedPixels) {
    const CKBYTE payload[] = {1,0x10,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 4, 1, 4, BI_RLE4, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {1,0,0,0}; BmpTgaExpectIndices(properties, 4, 1, indices, 4);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle8DeltaPreservesPaletteZeroHolesAndBottomUpOrder) {
    const CKBYTE payload[] = {1,1,0,2,1,0,1,2,0,0,3,3,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 3, 2, 8, BI_RLE8, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {3,3,3,1,0,2}; BmpTgaExpectIndices(properties, 3, 2, indices, 6);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle4AbsoluteNibblesLeavePaletteZeroAfterEarlyEnd) {
    const CKBYTE payload[] = {0,3,0x12,0x30,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 5, 1, 4, BI_RLE4, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {1,2,3,0,0}; BmpTgaExpectIndices(properties, 5, 1, indices, 5);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle8AbsoluteOddPaddingAndEolDoNotBecomePixels) {
    const CKBYTE payload[] = {0,3,1,2,3,0xff,0,0,3,1,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 3, 2, 8, BI_RLE8, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {1,1,1,1,2,3}; BmpTgaExpectIndices(properties, 3, 2, indices, 6);
}

TEST(CKBmpTgaDecodeContractTest, BmpRle4EncodedRunAlternatesTheTwoNibbles) {
    const CKBYTE payload[] = {4,0x12,0,1};
    CKBYTE bytes[256]; int size = BmpTgaMakeBmp(bytes, 4, 1, 4, BI_RLE4, payload, sizeof(payload), 4);
    BmpTgaDecodeReader owner(0); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &properties));
    const CKBYTE indices[] = {1,2,1,2}; BmpTgaExpectIndices(properties, 4, 1, indices, 4);
}

static void BmpTgaTgaHeader(CKBYTE *bytes, int width, int height, int depth, int type, int descriptor) {
    memset(bytes, 0, 18); bytes[2] = static_cast<CKBYTE>(type);
    BmpTgaPut(bytes, 12, width, 2); BmpTgaPut(bytes, 14, height, 2);
    bytes[16] = static_cast<CKBYTE>(depth); bytes[17] = static_cast<CKBYTE>(descriptor);
}

static void BmpTgaCheckOrigins(bool rle) {
    const int order[4][4] = {{2,3,0,1}, {3,2,1,0}, {0,1,2,3}, {1,0,3,2}};
    const CKBYTE colors[4][3] = {{1,11,21}, {2,12,22}, {3,13,23}, {4,14,24}};
    const CKBYTE expected[] = {1,11,21,255, 2,12,22,255, 3,13,23,255, 4,14,24,255};
    for (int i = 0; i < 4; ++i) {
        SCOPED_TRACE(i);
        CKBYTE bytes[31]; BmpTgaTgaHeader(bytes, 2, 2, 24, rle ? 10 : 2, i * 0x10);
        int offset = rle ? 19 : 18;
        if (rle) bytes[18] = 3;
        for (int j = 0; j < 4; ++j) memcpy(bytes + offset + j * 3, colors[order[i][j]], 3);
        BmpTgaDecodeReader owner(1); CKBitmapProperties *properties = nullptr;
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, offset + 12, &properties));
        BmpTgaExpectPixels(properties, 2, 2, expected, sizeof(expected));
    }
}

TEST(CKBmpTgaDecodeContractTest, TgaRawPixelsHonorAllFourOriginCombinations) { BmpTgaCheckOrigins(false); }
TEST(CKBmpTgaDecodeContractTest, TgaRleRawPacketHonorsAllFourOriginCombinations) { BmpTgaCheckOrigins(true); }

TEST(CKBmpTgaDecodeContractTest, TgaRlePacketsCanSpanRowsWithoutLosingPixels) {
    CKBYTE bytes[29]; BmpTgaTgaHeader(bytes, 2, 3, 24, 10, 0x20);
    const CKBYTE payload[] = {0x83,1,2,3, 1,4,5,6,7,8,9}; memcpy(bytes + 18, payload, sizeof(payload));
    const CKBYTE expected[] = {1,2,3,255, 1,2,3,255, 1,2,3,255, 1,2,3,255, 4,5,6,255, 7,8,9,255};
    BmpTgaDecodeReader owner(1); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    BmpTgaExpectPixels(properties, 2, 3, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, TgaPaletteOriginAndStoredAlphaAreAppliedToIndices) {
    CKBYTE bytes[28]; BmpTgaTgaHeader(bytes, 2, 1, 8, 1, 0x28);
    bytes[1] = 1; bytes[3] = 5; bytes[5] = 2; bytes[7] = 32;
    const CKBYTE payload[] = {1,2,3,37, 4,5,6,128, 6,5}; memcpy(bytes + 18, payload, sizeof(payload));
    const CKBYTE expected[] = {4,5,6,128, 1,2,3,37};
    BmpTgaDecodeReader owner(1); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    BmpTgaExpectPixels(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, TgaGray16PreservesTheSeparateAlphaByte) {
    CKBYTE bytes[22]; BmpTgaTgaHeader(bytes, 2, 1, 16, 3, 0x28);
    const CKBYTE payload[] = {0x11,0, 0x77,128}; memcpy(bytes + 18, payload, sizeof(payload));
    const CKBYTE expected[] = {0x11,0x11,0x11,0, 0x77,0x77,0x77,128};
    BmpTgaDecodeReader owner(1); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    BmpTgaExpectPixels(properties, 2, 1, expected, sizeof(expected));
}

TEST(CKBmpTgaDecodeContractTest, TgaRgb16DescriptorEnablesTheStoredAlphaBit) {
    CKBYTE bytes[22]; BmpTgaTgaHeader(bytes, 2, 1, 16, 2, 0x21);
    const CKBYTE payload[] = {0,0x7c, 0xe0,0x83}; memcpy(bytes + 18, payload, sizeof(payload));
    const CKBYTE expected[] = {0,0,255,0, 0,255,0,255};
    BmpTgaDecodeReader owner(1); CKBitmapProperties *properties = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, sizeof(bytes), &properties));
    BmpTgaExpectPixels(properties, 2, 1, expected, sizeof(expected));
}

static void BmpTgaRejectTga(CKBYTE *bytes, int size) {
    CKBYTE good[24]; BmpTgaTgaHeader(good, 2, 1, 24, 2, 0x20);
    const CKBYTE payload[] = {1,2,3,4,5,6}; memcpy(good + 18, payload, sizeof(payload));
    BmpTgaDecodeReader owner(1); CKBitmapProperties *before = nullptr, *after = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(good, sizeof(good), &before));
    void *pixels = before->m_Data;
    int result = owner.Reader->ReadMemory(bytes, size, &after);
    EXPECT_EQ(CKBITMAPERROR_FILECORRUPTED, result);
    if (result != 0) {
        EXPECT_EQ(before, after); EXPECT_EQ(pixels, after->m_Data);
        const CKBYTE expected[] = {1,2,3,255, 4,5,6,255};
        BmpTgaExpectPixels(after, 2, 1, expected, sizeof(expected));
    }
    ASSERT_EQ(0, owner.Reader->ReadMemory(good, sizeof(good), &after));
}

TEST(CKBmpTgaDecodeContractTest, TgaOversizedRepeatPacketDoesNotSucceedByClipping) {
    CKBYTE bytes[22]; BmpTgaTgaHeader(bytes, 2, 1, 24, 10, 0x20);
    bytes[18] = 0x82; bytes[19] = 1; bytes[20] = 2; bytes[21] = 3;
    BmpTgaRejectTga(bytes, sizeof(bytes));
}

TEST(CKBmpTgaDecodeContractTest, TgaOversizedRawPacketDoesNotIgnoreTheExtraPixel) {
    CKBYTE bytes[28]; BmpTgaTgaHeader(bytes, 2, 1, 24, 10, 0x20);
    bytes[18] = 2; const CKBYTE payload[] = {1,2,3,4,5,6,7,8,9}; memcpy(bytes + 19, payload, sizeof(payload));
    BmpTgaRejectTga(bytes, sizeof(bytes));
}

TEST(CKBmpTgaDecodeContractTest, TgaShortOversizedRawPacketCannotMasqueradeAsACompleteImage) {
    CKBYTE bytes[25]; BmpTgaTgaHeader(bytes, 2, 1, 24, 10, 0x20);
    bytes[18] = 2; const CKBYTE payload[] = {1,2,3,4,5,6}; memcpy(bytes + 19, payload, sizeof(payload));
    BmpTgaRejectTga(bytes, sizeof(bytes));
}

TEST(CKBmpTgaDecodeContractTest, TgaTruncatedLegalPacketPreservesTheOldImageAndCanBeRetried) {
    CKBYTE bytes[24]; BmpTgaTgaHeader(bytes, 2, 1, 24, 10, 0x20);
    bytes[18] = 1; const CKBYTE payload[] = {1,2,3,4,5}; memcpy(bytes + 19, payload, sizeof(payload));
    BmpTgaRejectTga(bytes, sizeof(bytes));
}
