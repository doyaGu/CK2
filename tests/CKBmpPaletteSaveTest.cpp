#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <filesystem>
#include "../../Plugins/ImageReader/BmpReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

struct PaletteReader {
    CKBitmapReader *Reader;
    PaletteReader() { CKGet_ImageReader_PluginInfo(0); Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(0)); }
    ~PaletteReader() { Reader->Release(); }
};
struct PaletteBytes {
    CKBitmapReader *Reader; void *Data = nullptr;
    explicit PaletteBytes(CKBitmapReader *reader) : Reader(reader) {}
    ~PaletteBytes() { if (Data) Reader->ReleaseMemory(Data); }
};
static unsigned PaletteLe(const CKBYTE *p, int n) {
    unsigned value = 0; for (int i = 0; i < n; ++i) value |= unsigned(p[i]) << (8 * i); return value;
}
static void PaletteSource(BmpBitmapProperties &p, CKBYTE *pixels, int width, int height, int pitch, bool rle) {
    ImageReader::FillFormatBGRA32(p.m_Format, width, height, pitch, pixels);
    p.m_Data = pixels; p.m_BitDepth = rle ? 9 : 8;
}
// Read only the output format specification. No production decoder/quantizer is
// called to obtain expected colors, indices or quality bounds.
static void PaletteDecode(const void *memory, int length, int width, int height, bool rle,
                          std::vector<CKBYTE> &decoded) {
    ASSERT_NE(nullptr, memory); ASSERT_GE(length, 1078);
    const CKBYTE *bytes = static_cast<const CKBYTE *>(memory);
    ASSERT_EQ('B', bytes[0]); ASSERT_EQ('M', bytes[1]); ASSERT_EQ(unsigned(length), PaletteLe(bytes + 2, 4));
    ASSERT_EQ(unsigned(width), PaletteLe(bytes + 18, 4)); ASSERT_EQ(unsigned(height), PaletteLe(bytes + 22, 4));
    ASSERT_EQ(8u, PaletteLe(bytes + 28, 2)); ASSERT_EQ(rle ? 1u : 0u, PaletteLe(bytes + 30, 4));
    const size_t offset = PaletteLe(bytes + 10, 4); ASSERT_GE(offset, 1078u); ASSERT_LE(offset, size_t(length));
    ASSERT_EQ(unsigned(length - offset), PaletteLe(bytes + 34, 4));
    for (int i = 0; i < 256; ++i) EXPECT_EQ(0, bytes[54 + 4 * i + 3]);
    std::vector<CKBYTE> indices(size_t(width) * height); size_t pos = offset;
    if (!rle) {
        int pitch = (width + 3) & ~3; ASSERT_EQ(offset + size_t(pitch) * height, size_t(length));
        for (int y = 0; y < height; ++y) {
            memcpy(&indices[size_t(y) * width], bytes + pos, width);
            for (int pad = width; pad < pitch; ++pad) EXPECT_EQ(0, bytes[pos + pad]);
            pos += pitch;
        }
    } else {
        int x = 0, y = 0; bool end = false;
        while (!end) {
            ASSERT_LE(pos + 2, size_t(length)); unsigned count = bytes[pos++], value = bytes[pos++];
            if (!count && value <= 1) {
                if (!value) { EXPECT_EQ(width, x); x = 0; ++y; }
                else { EXPECT_EQ(height, y); end = true; }
                continue;
            }
            ASSERT_TRUE(count || value >= 3); int n = count ? count : value;
            ASSERT_LT(y, height); ASSERT_LE(x + n, width);
            if (!count) ASSERT_LE(pos + n + (n & 1), size_t(length));
            for (int i = 0; i < n; ++i) indices[size_t(y) * width + x++] = count ? CKBYTE(value) : bytes[pos++];
            if (!count && (n & 1)) EXPECT_EQ(0, bytes[pos++]);
        }
    }
    EXPECT_EQ(size_t(length), pos); decoded.resize(size_t(width) * height * 3);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        unsigned index = indices[size_t(height - 1 - y) * width + x];
        memcpy(&decoded[(size_t(y) * width + x) * 3], bytes + 54 + index * 4, 3);
    }
}
static void PaletteExact(int kind, bool rle) {
    PaletteReader owner; BmpBitmapProperties p; std::vector<CKBYTE> source, palette;
    const CKBYTE colors[] = {0,0,255, 0,255,0, 255,0,0, 0,255,255, 255,255,0, 255,0,255};
    const int width = 3, height = 2;
    int pixelBytes = kind == 1 ? 3 : kind == 2 ? 2 : kind == 3 || kind == 4 ? 1 : 4;
    int pitch = width * pixelBytes + 5;
    source.resize(pitch * height, 0xcc);
    for (int i = 0; i < width * height; ++i) {
        CKBYTE *pixel = &source[(i / width) * pitch + (i % width) * pixelBytes];
        if (kind == 2) {
            const unsigned words[] = {0xf800,0x07e0,0x001f,0xffe0,0x07ff,0xf81f};
            pixel[0] = CKBYTE(words[i]); pixel[1] = CKBYTE(words[i] >> 8);
        } else if (kind == 3 || kind == 4) pixel[0] = CKBYTE(i);
        else {
            for (int c = 0; c < 3; ++c) pixel[c] = colors[i * 3 + (kind == 5 ? 2 - c : c)];
            if (pixelBytes == 4) pixel[3] = CKBYTE(i * 51);
        }
    }
    PaletteSource(p, source.data(), width, height, pitch, rle); p.m_Format.BitsPerPixel = pixelBytes * 8;
    if (kind == 2) { p.m_Format.RedMask = 0xf800; p.m_Format.GreenMask = 0x07e0; p.m_Format.BlueMask = 0x001f; p.m_Format.AlphaMask = 0; }
    if (kind == 1) p.m_Format.AlphaMask = 0;
    if (kind == 5) { p.m_Format.RedMask = B_MASK; p.m_Format.BlueMask = R_MASK; }
    if (kind == 3 || kind == 4) {
        int entryBytes = kind == 3 ? 3 : 4; palette.resize(6 * entryBytes, 0x17);
        for (int i = 0; i < 6; ++i) memcpy(&palette[i * entryBytes], colors + i * 3, 3);
        p.m_Format.ColorMap = palette.data(); p.m_Format.ColorMapEntries = 6; p.m_Format.BytesPerColorEntry = entryBytes;
    }
    if (kind == 6) p.m_Format.Image = nullptr; // CKBitmapData-style m_Data fallback.
    std::vector<CKBYTE> before = source, beforePalette = palette; BmpBitmapProperties beforeProperties = p;
    PaletteBytes encoded(owner.Reader); int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    std::vector<CKBYTE> decoded; PaletteDecode(encoded.Data, length, width, height, rle, decoded);
    ASSERT_EQ(sizeof(colors), decoded.size()); EXPECT_EQ(0, memcmp(colors, decoded.data(), sizeof(colors)));
    EXPECT_EQ(before, source); EXPECT_EQ(beforePalette, palette); EXPECT_EQ(0, memcmp(&beforeProperties, &p, sizeof(p)));
    EXPECT_FALSE(owner.Reader->IsAlphaSaved(&p));
}
TEST(CKBmpPaletteSaveTest, RawPreservesChromaticPrimariesAndRowOrder) { PaletteExact(0, false); }
TEST(CKBmpPaletteSaveTest, RlePreservesChromaticPrimariesAndRowOrder) { PaletteExact(0, true); }
TEST(CKBmpPaletteSaveTest, RawConvertsPaddedBgr24BeforeQuantizing) { PaletteExact(1, false); }
TEST(CKBmpPaletteSaveTest, RleConvertsPaddedBgr24BeforeQuantizing) { PaletteExact(1, true); }
TEST(CKBmpPaletteSaveTest, RawConvertsRgb565BeforeQuantizing) { PaletteExact(2, false); }
TEST(CKBmpPaletteSaveTest, RleConvertsRgb565BeforeQuantizing) { PaletteExact(2, true); }
TEST(CKBmpPaletteSaveTest, RawKeepsBorrowedThreeBytePaletteColors) { PaletteExact(3, false); }
TEST(CKBmpPaletteSaveTest, RleKeepsBorrowedFourBytePaletteColors) { PaletteExact(4, true); }
TEST(CKBmpPaletteSaveTest, SourceMasksAreAppliedBeforePaletteSelection) { PaletteExact(5, true); }
TEST(CKBmpPaletteSaveTest, DataFallbackRetainsItsChromaticPixels) { PaletteExact(6, false); }

static void PaletteGenerated(int kind, bool rle) {
    std::vector<CKBYTE> pixels;
    if (kind <= 1) {
        for (int i = 0; i < 256; ++i) {
            pixels.push_back(CKBYTE(i)); pixels.push_back(kind ? CKBYTE(i * 37) : CKBYTE(i));
            pixels.push_back(kind ? CKBYTE(255 - i) : CKBYTE(i)); pixels.push_back(CKBYTE(i));
        }
    } else if (kind == 2) {
        for (int i = 0; i < 256; ++i) {
            pixels.push_back(CKBYTE(i & 7)); pixels.push_back(CKBYTE((i >> 3) & 7));
            pixels.push_back(CKBYTE((i >> 6) & 3)); pixels.push_back(CKBYTE(i));
        }
    } else {
        if (kind == 4) for (int color = 0; color < 3; ++color) for (int i = 0; i < 2048; ++i) {
            for (int c = 0; c < 3; ++c) pixels.push_back(color == c ? 255 : 0); pixels.push_back(255);
        }
        for (int r = 0; r < 8; ++r) for (int g = 0; g < 8; ++g) for (int b = 0; b < 8; ++b) {
            pixels.push_back(CKBYTE(b * 255 / 7)); pixels.push_back(CKBYTE(g * 255 / 7));
            pixels.push_back(CKBYTE(r * 255 / 7)); pixels.push_back(255);
        }
    }
    const int width = int(pixels.size() / 4);
    PaletteReader owner; BmpBitmapProperties p; PaletteSource(p, pixels.data(), width, 1, width * 4, rle);
    PaletteBytes encoded(owner.Reader); int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    std::vector<CKBYTE> decoded; PaletteDecode(encoded.Data, length, width, 1, rle, decoded);
    ASSERT_EQ(size_t(width) * 3, decoded.size());
    unsigned maximumError = 0; double squareError = 0;
    for (int i = 0; i < width; ++i) for (int c = 0; c < 3; ++c) {
        int error = int(decoded[i * 3 + c]) - pixels[i * 4 + c]; if (error < 0) error = -error;
        if (unsigned(error) > maximumError) maximumError = unsigned(error);
        squareError += error * error;
        if (kind == 4 && i < 6144) EXPECT_LE(error, 8);
    }
    if (kind <= 2) EXPECT_EQ(0u, maximumError);
    else if (kind == 3) { EXPECT_LE(maximumError, 20u); EXPECT_LE(squareError / (width * 3), 150.0); }
    else EXPECT_LE(squareError / (width * 3), 150.0);
}
TEST(CKBmpPaletteSaveTest, All256GraysRemainExact) { PaletteGenerated(0, false); }
TEST(CKBmpPaletteSaveTest, RawPreservesExactly256DistinctRgbColors) { PaletteGenerated(1, false); }
TEST(CKBmpPaletteSaveTest, RlePreservesExactly256DistinctRgbColors) { PaletteGenerated(1, true); }
TEST(CKBmpPaletteSaveTest, ExactPaletteDoesNotMergeColorsSharingFiveBitBins) { PaletteGenerated(2, true); }
TEST(CKBmpPaletteSaveTest, Raw512ColorCubeHasBoundedQuantizationError) { PaletteGenerated(3, false); }
TEST(CKBmpPaletteSaveTest, Rle512ColorCubeHasBoundedQuantizationError) { PaletteGenerated(3, true); }
TEST(CKBmpPaletteSaveTest, DominantColorsRemainAccurateAmongMoreThan256Colors) { PaletteGenerated(4, true); }

TEST(CKBmpPaletteSaveTest, RleIgnoresAlphaWithoutPremultiplyingRgb) {
    const int width = 260; std::vector<CKBYTE> pixels(width * 4);
    for (int i = 0; i < width; ++i) { pixels[i * 4] = 21; pixels[i * 4 + 1] = 123; pixels[i * 4 + 2] = 231; pixels[i * 4 + 3] = CKBYTE(i); }
    PaletteReader owner; BmpBitmapProperties p; PaletteSource(p, pixels.data(), width, 1, width * 4, true);
    PaletteBytes encoded(owner.Reader); int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0); EXPECT_LT(length, 1100);
    std::vector<CKBYTE> decoded; PaletteDecode(encoded.Data, length, width, 1, true, decoded); ASSERT_EQ(width * 3u, decoded.size());
    for (int i = 0; i < width; ++i) EXPECT_EQ(0, memcmp(&decoded[i * 3], pixels.data(), 3));
}
TEST(CKBmpPaletteSaveTest, FileAndMemoryProduceTheSameColorImage) {
    PaletteReader owner;
    for (int rle = 0; rle < 2; ++rle) {
        CKBYTE pixels[] = {7,31,251,0, 199,23,47,255, 71,173,11,37}; BmpBitmapProperties p;
        PaletteSource(p, pixels, 3, 1, sizeof(pixels), rle != 0); PaletteBytes encoded(owner.Reader);
        int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
        char name[80]; std::snprintf(name, sizeof(name), "ck-palette-%p-%d.bmp", &p, rle);
        std::filesystem::path path = std::filesystem::temp_directory_path() / name;
        int written = owner.Reader->SaveFile(const_cast<char *>(path.string().c_str()), &p); EXPECT_EQ(length, written);
        FILE *file = std::fopen(path.string().c_str(), "rb"); ASSERT_NE(nullptr, file);
        std::vector<CKBYTE> bytes(length); EXPECT_EQ(size_t(length), std::fread(bytes.data(), 1, bytes.size(), file));
        EXPECT_EQ(EOF, std::fgetc(file)); EXPECT_EQ(0, std::fclose(file)); EXPECT_EQ(0, memcmp(bytes.data(), encoded.Data, length));
        std::error_code ignored; std::filesystem::remove(path, ignored);
        std::vector<CKBYTE> decoded; PaletteDecode(bytes.data(), written, 3, 1, rle != 0, decoded); ASSERT_EQ(9u, decoded.size());
        for (int i = 0; i < 3; ++i) EXPECT_EQ(0, memcmp(&decoded[i * 3], pixels + i * 4, 3));
    }
}
TEST(CKBmpPaletteSaveTest, ReusingAReaderDoesNotKeepThePreviousPalette) {
    PaletteReader owner;
    for (int i = 0; i < 3; ++i) {
        CKBYTE pixel[] = {0,0,0,255}; pixel[i] = 255; BmpBitmapProperties p; PaletteSource(p, pixel, 1, 1, 4, true);
        PaletteBytes encoded(owner.Reader); int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
        std::vector<CKBYTE> decoded; PaletteDecode(encoded.Data, length, 1, 1, true, decoded); ASSERT_EQ(3u, decoded.size());
        EXPECT_EQ(0, memcmp(decoded.data(), pixel, 3));
    }
}
