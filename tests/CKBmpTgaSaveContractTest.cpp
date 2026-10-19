#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>

#include "CKBitmapReader.h"
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"

static size_t ImageWriterWrite(const void *, size_t, size_t, FILE *);
static int ImageWriterClose(FILE *);

// Compile the complete production writers with only the stdio boundary observed.
// Other reader fixtures compile these CPPs normally, without these hooks.
#define fwrite ImageWriterWrite
#define fclose ImageWriterClose
#include "../../Plugins/ImageReader/BmpReader.cpp"
#include "../../Plugins/ImageReader/TgaReader.cpp"
#undef fclose
#undef fwrite

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

static int ImageWriterFault = 0;
static int ImageWriterWrites = 0;
static int ImageWriterCloses = 0;

static size_t ImageWriterWrite(const void *data, size_t size, size_t count, FILE *stream) {
    ++ImageWriterWrites;
    if (ImageWriterFault == 1 && size && count) {
        // Write a real partial file, then report the actual number of whole elements.
        return std::fwrite(data, 1, size * count - 1, stream) / size;
    }
    return std::fwrite(data, size, count, stream);
}

static int ImageWriterClose(FILE *stream) {
    ++ImageWriterCloses;
    int result = std::fclose(stream);
    // Deterministic late flush/close failure after the real handle has been closed.
    return ImageWriterFault == 2 ? EOF : result;
}

struct ImageWriterOwner {
    CKBitmapReader *Reader;
    explicit ImageWriterOwner(int index) {
        CKGet_ImageReader_PluginInfo(index);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~ImageWriterOwner() { if (Reader) Reader->Release(); }
};

struct ImageWriterFile {
    std::filesystem::path Path;
    ImageWriterFile() {
        static unsigned int sequence = 0;
        char name[96]; std::snprintf(name, sizeof(name), "ck-writer-%p-%u.img", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
        ImageWriterFault = ImageWriterWrites = ImageWriterCloses = 0;
    }
    ~ImageWriterFile() {
        ImageWriterFault = 0;
        std::error_code error; std::filesystem::remove(Path, error);
    }
};

struct ImageWriterBytes {
    CKBitmapReader *Reader;
    void *Data = nullptr;
    explicit ImageWriterBytes(CKBitmapReader *reader) : Reader(reader) {}
    ~ImageWriterBytes() { if (Data) Reader->ReleaseMemory(Data); }
};

static CKDWORD WriterLe(const CKBYTE *bytes, int count) {
    CKDWORD value = 0;
    for (int i = 0; i < count; ++i) value |= static_cast<CKDWORD>(bytes[i]) << (8 * i);
    return value;
}

static void WriterSource(CKBitmapProperties &properties, CKBYTE *pixels, int width, int height,
                         int depth, int pitch, CKDWORD r = R_MASK, CKDWORD g = G_MASK,
                         CKDWORD b = B_MASK, CKDWORD a = A_MASK) {
    properties.m_Data = pixels;
    properties.m_Format.Width = width; properties.m_Format.Height = height;
    properties.m_Format.BitsPerPixel = depth; properties.m_Format.BytesPerLine = pitch;
    properties.m_Format.RedMask = r; properties.m_Format.GreenMask = g;
    properties.m_Format.BlueMask = b; properties.m_Format.AlphaMask = a;
    properties.m_Format.Image = nullptr;
}

static void WriterRawExpected(int reader, const CKBYTE *bytes, int length, int width, int height,
                              int depth, const CKBYTE *topDownPixels) {
    const int pixelBytes = depth / 8;
    const int offset = reader == 0 ? static_cast<int>(WriterLe(bytes + 10, 4)) : 18;
    const int pitch = reader == 0 ? ((width * depth + 31) / 32) * 4 : width * pixelBytes;
    ASSERT_EQ(offset + pitch * height, length);
    if (reader == 0) {
        EXPECT_EQ('B', bytes[0]); EXPECT_EQ('M', bytes[1]);
        EXPECT_EQ(static_cast<CKDWORD>(length), WriterLe(bytes + 2, 4));
        EXPECT_EQ(static_cast<CKDWORD>(depth), WriterLe(bytes + 28, 2));
        EXPECT_EQ(0u, WriterLe(bytes + 30, 4));
    } else {
        EXPECT_EQ(2, bytes[2]); EXPECT_EQ(depth, bytes[16]);
    }
    for (int y = 0; y < height; ++y) {
        const int fileY = reader == 1 && (bytes[17] & 0x20) ? y : height - 1 - y;
        const CKBYTE *row = bytes + offset + fileY * pitch;
        for (int x = 0; x < width; ++x) {
            const int fileX = reader == 1 && (bytes[17] & 0x10) ? width - 1 - x : x;
            EXPECT_EQ(0, memcmp(row + fileX * pixelBytes, topDownPixels + (y * width + x) * 4, pixelBytes));
        }
        for (int p = width * pixelBytes; p < pitch; ++p) EXPECT_EQ(0, row[p]);
    }
}

static void WriterCheckSource(int reader, int inputKind) {
    ImageWriterOwner owner(reader); ImageWriterBytes encoded(owner.Reader);
    BmpBitmapProperties bmp; TgaBitmapProperties tga;
    CKBitmapProperties *properties = reader == 0 ? static_cast<CKBitmapProperties *>(&bmp) : &tga;
    CKBYTE pixels[16]; memset(pixels, 0xcc, sizeof(pixels));
    CKBYTE palette[] = {0x11,0x22,0x33, 0x44,0x55,0x66};
    CKBYTE expected[] = {0x11,0x22,0x33,255, 0x44,0x55,0x66,255,
                        0x44,0x55,0x66,255, 0x11,0x22,0x33,255};
    int outputDepth = 24;
    if (inputKind == 0) {
        WriterSource(*properties, pixels, 2, 2, 24, 8, R_MASK,G_MASK,B_MASK,0);
        const CKBYTE rows[] = {0x11,0x22,0x33,0x44,0x55,0x66,0xcc,0xcc,
                              0x44,0x55,0x66,0x11,0x22,0x33,0xcc,0xcc};
        memcpy(pixels, rows, sizeof(rows));
    } else if (inputKind == 1) {
        WriterSource(*properties, pixels, 2, 2, 16, 8, 0xf800,0x07e0,0x001f,0);
        const CKBYTE rows[] = {0x00,0xf8,0xe0,0x07,0xcc,0xcc,0xcc,0xcc,
                              0xe0,0x07,0x00,0xf8,0xcc,0xcc,0xcc,0xcc};
        memcpy(pixels, rows, sizeof(rows));
        const CKBYTE colors[] = {0,0,255,255, 0,255,0,255, 0,255,0,255, 0,0,255,255};
        memcpy(expected, colors, sizeof(colors));
    } else if (inputKind == 2) {
        WriterSource(*properties, pixels, 2, 2, 8, 8, 0,0,0,0);
        pixels[0] = 0; pixels[1] = 1; pixels[8] = 1; pixels[9] = 0;
        properties->m_Format.ColorMap = palette;
        properties->m_Format.ColorMapEntries = 2; properties->m_Format.BytesPerColorEntry = 3;
    } else if (inputKind == 3) {
        WriterSource(*properties, pixels, 2, 1, 32, 8, B_MASK,G_MASK,R_MASK,A_MASK);
        const CKBYTE rows[] = {0x33,0x22,0x11,37, 0x66,0x55,0x44,128};
        memcpy(pixels, rows, sizeof(rows)); expected[3] = 37; expected[7] = 128;
        outputDepth = 32;
    } else {
        WriterSource(*properties, pixels, 2, 1, 32, 8, R_MASK,G_MASK,B_MASK,0);
        const CKBYTE rows[] = {0x11,0x22,0x33,0, 0x44,0x55,0x66,0};
        memcpy(pixels, rows, sizeof(rows)); outputDepth = 32;
    }
    bmp.m_BitDepth = tga.m_BitDepth = outputDepth;
    CKBYTE before[16]; memcpy(before, pixels, sizeof(pixels));
    int length = owner.Reader->SaveMemory(&encoded.Data, properties);
    ASSERT_GT(length, 0); ASSERT_NE(nullptr, encoded.Data);
    WriterRawExpected(reader, static_cast<CKBYTE *>(encoded.Data), length, 2,
                      properties->m_Format.Height, outputDepth, expected);
    EXPECT_EQ(0, memcmp(before, pixels, sizeof(pixels)));
    EXPECT_EQ(pixels, properties->m_Data); EXPECT_EQ(nullptr, properties->m_Format.Image);
}

TEST(CKBmpTgaSaveContractTest, BmpConvertsPaddedBgr24Source) { WriterCheckSource(0,0); }
TEST(CKBmpTgaSaveContractTest, TgaConvertsPaddedBgr24Source) { WriterCheckSource(1,0); }
TEST(CKBmpTgaSaveContractTest, BmpConvertsRgb565Source) { WriterCheckSource(0,1); }
TEST(CKBmpTgaSaveContractTest, TgaConvertsRgb565Source) { WriterCheckSource(1,1); }
TEST(CKBmpTgaSaveContractTest, BmpConvertsIndexedSourceThroughItsPalette) { WriterCheckSource(0,2); }
TEST(CKBmpTgaSaveContractTest, TgaConvertsIndexedSourceThroughItsPalette) { WriterCheckSource(1,2); }
TEST(CKBmpTgaSaveContractTest, BmpAppliesSourceComponentMasks) { WriterCheckSource(0,3); }
TEST(CKBmpTgaSaveContractTest, TgaAppliesSourceComponentMasks) { WriterCheckSource(1,3); }
TEST(CKBmpTgaSaveContractTest, SourceWithoutAlphaProducesOpaque32BitOutput) {
    WriterCheckSource(0,4); WriterCheckSource(1,4);
}

TEST(CKBmpTgaSaveContractTest, CanonicalImageViewTakesPrecedenceOverTheDataFallback) {
    const CKBYTE expected[] = {1,2,3,37, 4,5,6,128}; CKBYTE pixels[8]; memcpy(pixels, expected, 8);
    CKBYTE other[8] = {};
    for (int reader = 0; reader < 2; ++reader) {
        ImageWriterOwner owner(reader); ImageWriterBytes encoded(owner.Reader);
        BmpBitmapProperties bmp; TgaBitmapProperties tga;
        CKBitmapProperties *p = reader == 0 ? static_cast<CKBitmapProperties *>(&bmp) : &tga;
        WriterSource(*p, other, 2, 1, 32, 8); p->m_Format.Image = pixels;
        bmp.m_BitDepth = tga.m_BitDepth = 32;
        int length = owner.Reader->SaveMemory(&encoded.Data, p); ASSERT_GT(length, 0);
        WriterRawExpected(reader, static_cast<CKBYTE *>(encoded.Data), length, 2, 1, 32, expected);
        EXPECT_EQ(other, p->m_Data); EXPECT_EQ(pixels, p->m_Format.Image);
    }
}

TEST(CKBmpTgaSaveContractTest, BmpIndexedBlackWhiteAndRgb555HaveIndependentExpectedColors) {
    CKBYTE pixels[] = {0,0,0,255, 255,255,255,255};
    ImageWriterOwner owner(0); BmpBitmapProperties properties;
    WriterSource(properties, pixels, 2, 1, 32, 8);
    for (int depth = 8; depth <= 16; depth += 8) {
        ImageWriterBytes encoded(owner.Reader); properties.m_BitDepth = depth;
        int length = owner.Reader->SaveMemory(&encoded.Data, &properties); ASSERT_GT(length, 0);
        CKBYTE *bytes = static_cast<CKBYTE *>(encoded.Data); int offset = WriterLe(bytes + 10, 4);
        EXPECT_EQ(static_cast<CKDWORD>(depth), WriterLe(bytes + 28, 2));
        if (depth == 8) {
            ASSERT_GE(length, offset + 4);
            for (int pixel = 0; pixel < 2; ++pixel) {
                const int entry = 54 + bytes[offset + pixel] * 4; ASSERT_LE(entry + 4, offset);
                for (int component = 0; component < 3; ++component)
                    EXPECT_EQ(pixel ? 255 : 0, bytes[entry + component]);
            }
            EXPECT_EQ(0, bytes[offset + 2]); EXPECT_EQ(0, bytes[offset + 3]);
        } else {
            const CKBYTE payload[] = {0,0,255,127}; EXPECT_EQ(58, length);
            EXPECT_EQ(0, memcmp(bytes + offset, payload, 4));
        }
    }
}

// Independent, bounded packet readers for checking writer output, never production decoders.
static void WriterCheckBmpRle(const CKBYTE *bytes, int length, int width, int height, const CKBYTE *gray) {
    ASSERT_GE(length, 1078); EXPECT_EQ(1u, WriterLe(bytes + 30, 4));
    EXPECT_EQ(static_cast<CKDWORD>(length), WriterLe(bytes + 2, 4));
    const int pixelOffset = WriterLe(bytes + 10, 4);
    int pos = pixelOffset, x = 0, y = height - 1, covered = 0;
    while (pos + 2 <= length) {
        int count = bytes[pos++], value = bytes[pos++];
        if (!count && value == 1) { EXPECT_EQ(width * height, covered); EXPECT_EQ(length, pos); return; }
        if (!count && value == 0) { EXPECT_EQ(width, x); x = 0; --y; continue; }
        ASSERT_GE(y, 0); int n = count ? count : value; ASSERT_GE(n, 1); ASSERT_LE(x + n, width);
        if (!count) ASSERT_LE(pos + n + (n & 1), length);
        for (int i = 0; i < n; ++i) {
            const int entry = 54 + (count ? value : bytes[pos + i]) * 4; ASSERT_LE(entry + 4, pixelOffset);
            for (int component = 0; component < 3; ++component) EXPECT_EQ(gray[y * width + x], bytes[entry + component]);
            ++x;
        }
        covered += n;
        if (!count) pos += n + (n & 1);
    }
    FAIL() << "Missing BMP RLE end-of-bitmap escape";
}

TEST(CKBmpTgaSaveContractTest, BmpRleRowsHaveCorrectOrderAndEndEscapes) {
    CKBYTE pixels[] = {17,17,17,255, 34,34,34,255, 51,51,51,255,
                       68,68,68,255, 68,68,68,255, 68,68,68,255};
    const CKBYTE expected[] = {17,34,51,68,68,68};
    ImageWriterOwner owner(0); ImageWriterBytes encoded(owner.Reader); BmpBitmapProperties p;
    WriterSource(p, pixels, 3, 2, 32, 12); p.m_BitDepth = 9;
    int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    WriterCheckBmpRle(static_cast<CKBYTE *>(encoded.Data), length, 3, 2, expected);
}

TEST(CKBmpTgaSaveContractTest, BmpRleHandlesOddLiteralsAndRunsBeyond255Pixels) {
    CKBYTE pixels[1036], gray[259];
    for (int i = 0; i < 259; ++i) {
        gray[i] = i < 3 ? static_cast<CKBYTE>(i + 1) : 77;
        pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = gray[i]; pixels[i * 4 + 3] = 255;
    }
    ImageWriterOwner owner(0); ImageWriterBytes encoded(owner.Reader); BmpBitmapProperties p;
    WriterSource(p, pixels, 259, 1, 32, sizeof(pixels)); p.m_BitDepth = 9;
    int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    WriterCheckBmpRle(static_cast<CKBYTE *>(encoded.Data), length, 259, 1, gray);
}

TEST(CKBmpTgaSaveContractTest, TgaRawOptionsHaveCorrectHeadersAndQuantizedPixelBytes) {
    CKBYTE pixels[] = {255,255,255,37, 0,0,0,128};
    ImageWriterOwner owner(1); TgaBitmapProperties p; WriterSource(p, pixels, 2, 1, 32, 8);
    const int depths[] = {16,24,32,64};
    for (int depth : depths) {
        ImageWriterBytes encoded(owner.Reader); p.m_BitDepth = depth;
        int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
        CKBYTE *bytes = static_cast<CKBYTE *>(encoded.Data);
        EXPECT_EQ(depth == 64 ? 3 : 2, bytes[2]); EXPECT_EQ(depth == 64 ? 8 : depth, bytes[16]);
        EXPECT_EQ(depth == 32 ? 8 : 0, bytes[17] & 15);
        if (depth == 16) {
            const CKBYTE expected[] = {255,127,0,0}; EXPECT_EQ(22, length); EXPECT_EQ(0, memcmp(bytes + 18, expected, 4));
        } else if (depth == 64) {
            EXPECT_EQ(20, length); EXPECT_EQ(255, bytes[18]); EXPECT_EQ(0, bytes[19]);
        } else WriterRawExpected(1, bytes, length, 2, 1, depth, pixels);
    }
}

static void WriterCheckTgaRle(const CKBYTE *bytes, int length, int width, int height, const CKBYTE *bgra) {
    ASSERT_GE(length, 18); EXPECT_EQ(10, bytes[2]); EXPECT_EQ(32, bytes[16]);
    int pos = 18, covered = 0;
    while (covered < width * height) {
        ASSERT_LT(pos, length); int packet = bytes[pos++], count = (packet & 127) + 1;
        ASSERT_LE(covered + count, width * height);
        for (int i = 0; i < count; ++i) {
            ASSERT_LE(pos + 4, length);
            int x = covered % width, y = covered / width;
            if (!(bytes[17] & 0x20)) y = height - 1 - y;
            if (bytes[17] & 0x10) x = width - 1 - x;
            EXPECT_EQ(0, memcmp(bytes + pos, bgra + (y * width + x) * 4, 4)); ++covered;
            if (!(packet & 128)) pos += 4;
        }
        if (packet & 128) pos += 4;
    }
    EXPECT_EQ(length, pos);
}

TEST(CKBmpTgaSaveContractTest, TgaRleSplitsRunsAt128WithoutLosingTheTail) {
    CKBYTE pixels[520];
    for (int i = 0; i < 130; ++i) {
        pixels[i * 4] = 1; pixels[i * 4 + 1] = 2; pixels[i * 4 + 2] = 3; pixels[i * 4 + 3] = 37;
    }
    pixels[129 * 4] = 4;
    ImageWriterOwner owner(1); ImageWriterBytes encoded(owner.Reader); TgaBitmapProperties p;
    WriterSource(p, pixels, 130, 1, 32, sizeof(pixels)); p.m_BitDepth = 32; p.m_UseRLE = 1;
    int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    WriterCheckTgaRle(static_cast<CKBYTE *>(encoded.Data), length, 130, 1, pixels);
}

TEST(CKBmpTgaSaveContractTest, TgaRleMixesRawAndRepeatedPixelsAcrossRows) {
    CKBYTE pixels[] = {1,2,3,37, 4,5,6,128, 7,8,9,255, 7,8,9,255,
                       7,8,9,255, 10,11,12,0, 13,14,15,255, 16,17,18,37};
    ImageWriterOwner owner(1); ImageWriterBytes encoded(owner.Reader); TgaBitmapProperties p;
    WriterSource(p, pixels, 4, 2, 32, 16); p.m_BitDepth = 32; p.m_UseRLE = 1;
    int length = owner.Reader->SaveMemory(&encoded.Data, &p); ASSERT_GT(length, 0);
    WriterCheckTgaRle(static_cast<CKBYTE *>(encoded.Data), length, 4, 2, pixels);
}

static int WriterFileCase(int reader, int fault) {
    ImageWriterOwner owner(reader); ImageWriterBytes encoded(owner.Reader); ImageWriterFile file;
    CKBYTE pixels[] = {1,2,3,37, 4,5,6,128}; BmpBitmapProperties bmp; TgaBitmapProperties tga;
    CKBitmapProperties *p = reader == 0 ? static_cast<CKBitmapProperties *>(&bmp) : &tga;
    WriterSource(*p, pixels, 2, 1, 32, 8); bmp.m_BitDepth = tga.m_BitDepth = 32;
    int expected = owner.Reader->SaveMemory(&encoded.Data, p); EXPECT_GT(expected, 0);
    ImageWriterFault = fault;
    int result = owner.Reader->SaveFile(const_cast<char *>(file.Path.string().c_str()), p);
    ImageWriterFault = 0;
    EXPECT_EQ(1, ImageWriterWrites); EXPECT_EQ(1, ImageWriterCloses);
    EXPECT_EQ(static_cast<uintmax_t>(expected - (fault == 1 ? 1 : 0)), std::filesystem::file_size(file.Path));
    EXPECT_EQ(pixels, p->m_Data); EXPECT_EQ(nullptr, p->m_Format.Image);
    if (!fault) {
        CKBYTE bytes[128];
        if (expected >= static_cast<int>(sizeof(bytes))) { ADD_FAILURE() << "Unexpected test file size"; return 0; }
        FILE *stream = std::fopen(file.Path.string().c_str(), "rb"); EXPECT_NE(nullptr, stream);
        if (stream) { EXPECT_EQ(static_cast<size_t>(expected), std::fread(bytes, 1, expected, stream)); std::fclose(stream); }
        EXPECT_EQ(0, memcmp(bytes, encoded.Data, expected));
    }
    return result;
}

TEST(CKBmpTgaSaveContractTest, FileAndMemoryWritersReturnTheirActualByteCounts) {
    EXPECT_GT(WriterFileCase(0,0), 0); EXPECT_GT(WriterFileCase(1,0), 0);
}
TEST(CKBmpTgaSaveContractTest, PartialWritesReturnZeroAndCloseTheActualFileOnce) {
    EXPECT_EQ(0, WriterFileCase(0,1)); EXPECT_EQ(0, WriterFileCase(1,1));
}
TEST(CKBmpTgaSaveContractTest, LateCloseFailuresReturnZeroAndCloseTheActualHandleOnce) {
    EXPECT_EQ(0, WriterFileCase(0,2)); EXPECT_EQ(0, WriterFileCase(1,2));
}

TEST(CKBmpTgaSaveContractTest, OpenFailureReturnsZeroWithoutWritingOrClosingAnInvalidStream) {
    CKBYTE pixels[] = {1,2,3,255};
    for (int reader = 0; reader < 2; ++reader) {
        ImageWriterOwner owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
        CKBitmapProperties *p = reader == 0 ? static_cast<CKBitmapProperties *>(&bmp) : &tga;
        WriterSource(*p, pixels, 1, 1, 32, 4); ImageWriterWrites = ImageWriterCloses = 0;
        std::string path = std::filesystem::temp_directory_path().string();
        EXPECT_EQ(0, owner.Reader->SaveFile(const_cast<char *>(path.c_str()), p));
        EXPECT_EQ(0, ImageWriterWrites); EXPECT_EQ(0, ImageWriterCloses);
    }
}

static void WriterTgaDimension(int width, int height) {
    CKBYTE *pixels = new CKBYTE[width * height * 4]();
    ImageWriterOwner owner(1); ImageWriterBytes encoded(owner.Reader); TgaBitmapProperties p;
    WriterSource(p, pixels, width, height, 32, width * 4);
    int result = owner.Reader->SaveMemory(&encoded.Data, &p);
    EXPECT_EQ(0, result); EXPECT_EQ(nullptr, encoded.Data);
    delete[] pixels;
}
TEST(CKBmpTgaSaveContractTest, TgaWidth65536CannotSilentlyWrapToZero) { WriterTgaDimension(65536,1); }
TEST(CKBmpTgaSaveContractTest, TgaHeight65536CannotSilentlyWrapToZero) { WriterTgaDimension(1,65536); }

TEST(CKBmpTgaSaveContractTest, TgaMaximumRepresentableWidthRemainsSupported) {
    CKBYTE *pixels = new CKBYTE[65535 * 4]; memset(pixels, 255, 65535 * 4);
    ImageWriterOwner owner(1); ImageWriterBytes encoded(owner.Reader); TgaBitmapProperties p;
    WriterSource(p, pixels, 65535, 1, 32, 65535 * 4);
    int length = owner.Reader->SaveMemory(&encoded.Data, &p);
    EXPECT_EQ(18 + 65535 * 3, length);
    if (encoded.Data) {
        CKBYTE *bytes = static_cast<CKBYTE *>(encoded.Data);
        EXPECT_EQ(65535u, WriterLe(bytes + 12, 2)); EXPECT_EQ(1u, WriterLe(bytes + 14, 2));
        EXPECT_EQ(255, bytes[18]); EXPECT_EQ(255, bytes[length - 1]);
    }
    delete[] pixels;
}

TEST(CKBmpTgaSaveContractTest, ConversionLeavesBorrowedPixelsPaletteAndPropertiesUnchanged) {
    CKBYTE pixels[] = {0,1,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
    CKBYTE palette[] = {1,2,3,4,5,6}; CKBYTE before[6]; memcpy(before, palette, sizeof(before));
    for (int reader = 0; reader < 2; ++reader) {
        ImageWriterOwner owner(reader); ImageWriterBytes encoded(owner.Reader);
        BmpBitmapProperties bmp; TgaBitmapProperties tga;
        CKBitmapProperties *p = reader == 0 ? static_cast<CKBitmapProperties *>(&bmp) : &tga;
        WriterSource(*p, pixels, 2, 1, 8, 8, 0,0,0,0);
        p->m_Format.ColorMap = palette; p->m_Format.BytesPerColorEntry = 3; p->m_Format.ColorMapEntries = 2;
        CKBYTE snapshot[sizeof(TgaBitmapProperties)]; memcpy(snapshot, p, p->m_Size);
        EXPECT_GT(owner.Reader->SaveMemory(&encoded.Data, p), 0);
        EXPECT_EQ(0, memcmp(snapshot, p, p->m_Size)); EXPECT_EQ(0, memcmp(before, palette, sizeof(before)));
        EXPECT_EQ(0, pixels[0]); EXPECT_EQ(1, pixels[1]);
    }
}
