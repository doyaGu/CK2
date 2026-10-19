#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <climits>
#include <vector>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);
struct SignedPitchReader {
    CKBitmapReader *Reader;
    explicit SignedPitchReader(int index) {
        CKGet_ImageReader_PluginInfo(index); Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~SignedPitchReader() { Reader->Release(); }
};
struct SignedPitchBytes {
    CKBitmapReader *Reader; void *Data = nullptr;
    explicit SignedPitchBytes(CKBitmapReader *reader) : Reader(reader) {}
    ~SignedPitchBytes() { if (Data) Reader->ReleaseMemory(Data); }
};
struct SignedPitchFile {
    std::filesystem::path Path;
    explicit SignedPitchFile(int index) {
        char name[80]; std::snprintf(name, sizeof(name), "ck-signed-pitch-%p-%d.img", this, index);
        Path = std::filesystem::temp_directory_path() / name;
    }
    ~SignedPitchFile() { std::error_code ignored; std::filesystem::remove(Path, ignored); }
};
static unsigned SignedPitchLe(const CKBYTE *p, int n) {
    unsigned value = 0; for (int i = 0; i < n; ++i) value |= unsigned(p[i]) << (8 * i); return value;
}
// Format-only decoding of the encoded output; expected pixels are supplied
// independently. No production decoder or blitter generates the expected image.
static void SignedPitchDecode(const void *memory, int size, int reader, int depth, bool rle,
                              int width, int height, std::vector<CKBYTE> &pixels) {
    ASSERT_NE(nullptr, memory); const CKBYTE *bytes = static_cast<const CKBYTE *>(memory);
    ASSERT_GE(size, reader ? 18 : 54);
    size_t pos = reader ? 18 : SignedPitchLe(bytes + 10, 4);
    ASSERT_LE(pos, size_t(size)); int count = width * height;
    std::vector<CKBYTE> filePixels(size_t(count) * 4, 255);
    if (reader == 0) {
        ASSERT_EQ(unsigned(size), SignedPitchLe(bytes + 2, 4)); ASSERT_EQ(unsigned(width), SignedPitchLe(bytes + 18, 4));
        ASSERT_EQ(unsigned(height), SignedPitchLe(bytes + 22, 4)); ASSERT_EQ(unsigned(depth), SignedPitchLe(bytes + 28, 2));
        ASSERT_EQ(rle ? 1u : 0u, SignedPitchLe(bytes + 30, 4)); ASSERT_EQ(unsigned(size - pos), SignedPitchLe(bytes + 34, 4));
    } else {
        ASSERT_EQ(unsigned(width), SignedPitchLe(bytes + 12, 2)); ASSERT_EQ(unsigned(height), SignedPitchLe(bytes + 14, 2));
        ASSERT_EQ(depth == 64 ? 8 : depth, bytes[16]);
        ASSERT_EQ((depth == 64 ? 3 : 2) + (rle ? 8 : 0), bytes[2]);
    }
    if (reader == 0 && depth == 8) {
        ASSERT_GE(pos, 1078u); std::vector<CKBYTE> indices(count); int x = 0, y = 0;
        if (rle) {
            bool end = false;
            while (!end) {
                ASSERT_LE(pos + 2, size_t(size)); unsigned n = bytes[pos++], value = bytes[pos++];
                if (!n && value <= 1) {
                    if (!value) { EXPECT_EQ(width, x); x = 0; ++y; }
                    else { EXPECT_EQ(height, y); end = true; }
                    continue;
                }
                ASSERT_TRUE(n || value >= 3); int length = n ? n : value;
                ASSERT_LT(y, height); ASSERT_LE(x + length, width);
                if (!n) ASSERT_LE(pos + length + (length & 1), size_t(size));
                for (int i = 0; i < length; ++i) indices[y * width + x++] = n ? CKBYTE(value) : bytes[pos++];
                if (!n && (length & 1)) EXPECT_EQ(0, bytes[pos++]);
            }
        } else {
            const int pitch = (width + 3) & ~3; ASSERT_EQ(pos + size_t(pitch) * height, size_t(size));
            for (int row = 0; row < height; ++row) {
                memcpy(&indices[row * width], bytes + pos, width);
                for (int pad = width; pad < pitch; ++pad) EXPECT_EQ(0, bytes[pos + pad]);
                pos += pitch;
            }
        }
        for (int i = 0; i < count; ++i) memcpy(&filePixels[i * 4], bytes + 54 + indices[i] * 4, 3);
    } else {
        const int bpp = depth == 64 ? 1 : depth / 8; int covered = 0;
        while (covered < count) {
            int n = 1; bool repeated = false;
            if (reader && rle) { ASSERT_LT(pos, size_t(size)); CKBYTE packet = bytes[pos++]; n = (packet & 127) + 1; repeated = (packet & 128) != 0; }
            ASSERT_LE(covered + n, count); size_t start = pos;
            for (int i = 0; i < n; ++i) {
                size_t offset = start + (repeated ? 0 : size_t(i) * bpp); ASSERT_LE(offset + bpp, size_t(size));
                CKBYTE *pixel = &filePixels[(covered + i) * 4];
                if (depth == 16) {
                    unsigned value = SignedPitchLe(bytes + offset, 2);
                    pixel[0] = CKBYTE((value & 31) * 255 / 31); pixel[1] = CKBYTE(((value >> 5) & 31) * 255 / 31);
                    pixel[2] = CKBYTE(((value >> 10) & 31) * 255 / 31);
                } else if (depth == 64) pixel[0] = pixel[1] = pixel[2] = bytes[offset];
                else memcpy(pixel, bytes + offset, bpp);
            }
            pos += repeated ? bpp : n * bpp; covered += n;
            if (!reader && covered % width == 0) {
                int padding = ((width * bpp + 3) & ~3) - width * bpp;
                ASSERT_LE(pos + padding, size_t(size)); for (int i = 0; i < padding; ++i) EXPECT_EQ(0, bytes[pos++]);
            }
        }
    }
    EXPECT_EQ(size_t(size), pos); pixels.resize(filePixels.size());
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        int fileY = reader && (bytes[17] & 32) ? y : height - 1 - y;
        int fileX = reader && (bytes[17] & 16) ? width - 1 - x : x;
        memcpy(&pixels[(y * width + x) * 4], &filePixels[(fileY * width + fileX) * 4], 4);
    }
}
struct SignedPitchSource {
    std::vector<CKBYTE> Storage, Palette, Alternate, Expected;
    BmpBitmapProperties Bmp;
    TgaBitmapProperties Tga;
    CKBitmapProperties *Properties;
    SignedPitchSource(int reader, int kind, int depth, bool rle, int pointerMode, bool negative = true) {
        const CKBYTE colors[] = {0,0,255, 0,255,0, 255,0,0, 0,255,255, 255,255,0, 255,0,255, 0,0,0, 255,255,255, 255,0,0};
        const CKBYTE alpha[] = {0,37,128,255,99,17,230,77,199};
        const unsigned words[] = {0xf800,0x07e0,0x001f,0xffe0,0x07ff,0xf81f,0,0xffff,0x001f};
        int pixelBytes = kind == 1 ? 3 : kind == 2 ? 2 : kind == 3 || kind == 4 ? 1 : 4;
        int pitch = 3 * pixelBytes + 5; Storage.resize(pitch * 3 + 64, 0xcc); Alternate.resize(Storage.size(), 0);
        CKBYTE *logicalZero = Storage.data() + 32 + (negative ? pitch * 2 : 0);
        Expected.resize(36);
        for (int i = 0; i < 9; ++i) {
            CKBYTE *pixel = logicalZero + (negative ? -1 : 1) * (i / 3) * pitch + (i % 3) * pixelBytes;
            if (kind == 2) { pixel[0] = CKBYTE(words[i]); pixel[1] = CKBYTE(words[i] >> 8); }
            else if (kind == 3 || kind == 4) pixel[0] = CKBYTE(i);
            else {
                for (int c = 0; c < 3; ++c) pixel[c] = colors[i * 3 + (kind == 5 ? 2 - c : c)];
                if (pixelBytes == 4) pixel[3] = kind == 6 ? 0 : alpha[i];
            }
            memcpy(&Expected[i * 4], colors + i * 3, 3);
            Expected[i * 4 + 3] = (kind == 0 || kind == 5) && depth == 32 ? alpha[i] : 255;
            if (depth == 64) {
                // Independently specified weighted luminance for the literal primary colors above.
                const CKBYTE gray[] = {76,149,29,225,178,105,0,255,29};
                Expected[i * 4] = Expected[i * 4 + 1] = Expected[i * 4 + 2] = gray[i];
            }
        }
        Bmp.m_BitDepth = rle ? 9 : depth; Tga.m_BitDepth = depth; Tga.m_UseRLE = rle;
        Properties = reader ? static_cast<CKBitmapProperties *>(&Tga) : &Bmp;
        ImageReader::FillFormatBGRA32(Properties->m_Format, 3, 3, negative ? -pitch : pitch, logicalZero);
        Properties->m_Data = pointerMode == 2 ? Alternate.data() + 32 + pitch * 2 : logicalZero;
        if (pointerMode == 1) Properties->m_Format.Image = nullptr;
        Properties->m_Format.BitsPerPixel = pixelBytes * 8;
        if (kind == 1 || kind == 6) Properties->m_Format.AlphaMask = 0;
        if (kind == 2) { Properties->m_Format.RedMask = 0xf800; Properties->m_Format.GreenMask = 0x07e0; Properties->m_Format.BlueMask = 0x001f; Properties->m_Format.AlphaMask = 0; }
        if (kind == 5) { Properties->m_Format.RedMask = B_MASK; Properties->m_Format.BlueMask = R_MASK; }
        if (kind == 3 || kind == 4) {
            const int entry = kind == 3 ? 3 : 4; Palette.resize(9 * entry, 255);
            for (int i = 0; i < 9; ++i) memcpy(&Palette[i * entry], colors + i * 3, 3);
            Properties->m_Format.ColorMapEntries = 9; Properties->m_Format.BytesPerColorEntry = entry; Properties->m_Format.ColorMap = Palette.data();
        }
    }
};
static void SignedPitchCheck(int reader, int kind, int depth, bool rle, int pointerMode = 0, bool file = false, bool negative = true) {
    SignedPitchReader owner(reader); SignedPitchSource source(reader, kind, depth, rle, pointerMode, negative);
    std::vector<CKBYTE> before = source.Storage, beforePalette = source.Palette, beforeAlternate = source.Alternate;
    BmpBitmapProperties beforeBmp = source.Bmp; TgaBitmapProperties beforeTga = source.Tga;
    SignedPitchBytes encoded(owner.Reader); int size = owner.Reader->SaveMemory(&encoded.Data, source.Properties); ASSERT_GT(size, 0);
    std::vector<CKBYTE> pixels; SignedPitchDecode(encoded.Data, size, reader, depth, rle, 3, 3, pixels); EXPECT_EQ(source.Expected, pixels);
    if (file) {
        SignedPitchFile temporary(reader); int written = owner.Reader->SaveFile(const_cast<char *>(temporary.Path.string().c_str()), source.Properties);
        ASSERT_EQ(size, written); FILE *stream = std::fopen(temporary.Path.string().c_str(), "rb"); ASSERT_NE(nullptr, stream);
        std::vector<CKBYTE> bytes(size); EXPECT_EQ(size_t(size), std::fread(bytes.data(), 1, bytes.size(), stream));
        EXPECT_EQ(EOF, std::fgetc(stream)); EXPECT_EQ(0, std::fclose(stream)); EXPECT_EQ(0, memcmp(bytes.data(), encoded.Data, size));
        std::vector<CKBYTE> filePixels; SignedPitchDecode(bytes.data(), written, reader, depth, rle, 3, 3, filePixels); EXPECT_EQ(source.Expected, filePixels);
    }
    EXPECT_EQ(before, source.Storage); EXPECT_EQ(beforePalette, source.Palette); EXPECT_EQ(beforeAlternate, source.Alternate);
    EXPECT_EQ(0, memcmp(&beforeBmp, &source.Bmp, sizeof(beforeBmp))); EXPECT_EQ(0, memcmp(&beforeTga, &source.Tga, sizeof(beforeTga)));
}
TEST(CKImageSaveSignedPitchTest, Bmp32PreservesLogicalRowOrderAndAlpha) { SignedPitchCheck(0,0,32,false); }
TEST(CKImageSaveSignedPitchTest, Tga32PreservesLogicalRowOrderAndAlpha) { SignedPitchCheck(1,0,32,false); }
TEST(CKImageSaveSignedPitchTest, Bmp24ConvertsNegativePaddedBgr24) { SignedPitchCheck(0,1,24,false); }
TEST(CKImageSaveSignedPitchTest, Tga24ConvertsNegativePaddedBgr24) { SignedPitchCheck(1,1,24,false); }
TEST(CKImageSaveSignedPitchTest, Bmp16ConvertsNegativeRgb565) { SignedPitchCheck(0,2,16,false); }
TEST(CKImageSaveSignedPitchTest, Tga16ConvertsNegativeRgb565) { SignedPitchCheck(1,2,16,false); }
TEST(CKImageSaveSignedPitchTest, Bmp8ConvertsNegativeIndexedThreeBytePalette) { SignedPitchCheck(0,3,8,false); }
TEST(CKImageSaveSignedPitchTest, BmpRle8ConvertsNegativeIndexedFourBytePalette) { SignedPitchCheck(0,4,8,true); }
TEST(CKImageSaveSignedPitchTest, TgaRle24ConvertsNegativeIndexedSource) { SignedPitchCheck(1,3,24,true); }
TEST(CKImageSaveSignedPitchTest, TgaRle32AppliesNegativeSourceMasks) { SignedPitchCheck(1,5,32,true); }
TEST(CKImageSaveSignedPitchTest, Bmp32UsesTheDataFallbackAsLogicalRowZero) { SignedPitchCheck(0,0,32,false,1); }
TEST(CKImageSaveSignedPitchTest, Tga32UsesTheDataFallbackAsLogicalRowZero) { SignedPitchCheck(1,0,32,false,1); }
TEST(CKImageSaveSignedPitchTest, Bmp8RetainsNegativeSourceColors) { SignedPitchCheck(0,0,8,false); }
TEST(CKImageSaveSignedPitchTest, BmpRle8RetainsNegativeSourceColors) { SignedPitchCheck(0,0,8,true); }
TEST(CKImageSaveSignedPitchTest, TgaGrayUsesNegativeLogicalRows) { SignedPitchCheck(1,0,64,false); }
TEST(CKImageSaveSignedPitchTest, TgaRleGrayUsesNegativeLogicalRows) { SignedPitchCheck(1,0,64,true); }
TEST(CKImageSaveSignedPitchTest, Bmp32WithoutSourceAlphaBecomesOpaque) { SignedPitchCheck(0,6,32,false); }
TEST(CKImageSaveSignedPitchTest, Tga32WithoutSourceAlphaBecomesOpaque) { SignedPitchCheck(1,6,32,false); }
TEST(CKImageSaveSignedPitchTest, ImageTakesPrecedenceWithoutChangingEitherBorrowedView) { SignedPitchCheck(0,0,32,false,2); SignedPitchCheck(1,0,32,true,2); }
TEST(CKImageSaveSignedPitchTest, SingleRowAllowsIntMinPitchWithoutNegatingItInInt) {
    for (int reader = 0; reader < 2; ++reader) {
        SignedPitchReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga; bmp.m_BitDepth = tga.m_BitDepth = 32;
        CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp; CKBYTE pixel[] = {17,43,97,128};
        ImageReader::FillFormatBGRA32(p->m_Format, 1, 1, INT_MIN, pixel); p->m_Data = pixel;
        SignedPitchBytes encoded(owner.Reader); int size = owner.Reader->SaveMemory(&encoded.Data, p); ASSERT_GT(size,0);
        std::vector<CKBYTE> decoded; SignedPitchDecode(encoded.Data, size, reader, 32, false, 1, 1, decoded);
        ASSERT_EQ(4u,decoded.size()); EXPECT_EQ(0,memcmp(pixel,decoded.data(),4)); EXPECT_EQ(INT_MIN,p->m_Format.BytesPerLine);
    }
}
TEST(CKImageSaveSignedPitchTest, NegativePitchShorterThanTheSourceRowIsRejected) {
    for (int reader = 0; reader < 2; ++reader) {
        SignedPitchReader owner(reader); SignedPitchSource source(reader,0,32,false,0); source.Properties->m_Format.BytesPerLine = -11;
        SignedPitchBytes encoded(owner.Reader); EXPECT_EQ(0, owner.Reader->SaveMemory(&encoded.Data,source.Properties)); EXPECT_EQ(nullptr,encoded.Data);
    }
}
struct SignedPitchSparseSource {
    CKBYTE *Pixels = nullptr; size_t Size = 0;
    ~SignedPitchSparseSource() {
#ifdef _WIN32
        if (Pixels) VirtualFree(Pixels,0,MEM_RELEASE);
#else
        if (Pixels) munmap(Pixels,Size);
#endif
    }
};
TEST(CKImageSaveSignedPitchTest, CumulativeNegativeRowOffsetsBeyondDwordRemainSigned) {
    for (int reader = 0; reader < 2; ++reader) {
        SignedPitchReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga; bmp.m_BitDepth = tga.m_BitDepth = 32; tga.m_UseRLE = 1;
        CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
#if UINTPTR_MAX > UINT32_MAX
        SignedPitchSparseSource source; const size_t pitch = INT_MAX; source.Size = 3 * pitch + 4;
#ifdef _WIN32
        SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
        source.Pixels = static_cast<CKBYTE *>(VirtualAlloc(nullptr,source.Size,MEM_RESERVE,PAGE_NOACCESS));
#else
        const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        void *reserved = mmap(nullptr,source.Size,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
        source.Pixels = reserved == MAP_FAILED ? nullptr : static_cast<CKBYTE *>(reserved);
#endif
        ASSERT_NE(nullptr,source.Pixels); std::vector<CKBYTE> expected(16);
        for (int y = 0; y < 4; ++y) {
            size_t offset = size_t(3 - y) * pitch, start = offset & ~(page - 1);
#ifdef _WIN32
            ASSERT_NE(nullptr,VirtualAlloc(source.Pixels + start,page * 2,MEM_COMMIT,PAGE_READWRITE));
#else
            ASSERT_EQ(0,mprotect(source.Pixels + start,page * 2,PROT_READ | PROT_WRITE));
#endif
            CKBYTE *row = source.Pixels + offset; row[0] = CKBYTE(y * 17); row[1] = CKBYTE(y * 29); row[2] = CKBYTE(y * 47); row[3] = CKBYTE(y * 63);
            memcpy(&expected[y * 4],row,4);
        }
        ImageReader::FillFormatBGRA32(p->m_Format,1,4,-INT_MAX,source.Pixels + pitch * 3); p->m_Data = p->m_Format.Image;
        SignedPitchBytes encoded(owner.Reader); int size = owner.Reader->SaveMemory(&encoded.Data,p); ASSERT_GT(size,0);
        std::vector<CKBYTE> decoded; SignedPitchDecode(encoded.Data,size,reader,32,reader != 0,1,4,decoded); EXPECT_EQ(expected,decoded);
#else
        ImageReader::FillFormatBGRA32(p->m_Format,1,4,-INT_MAX,reinterpret_cast<CKBYTE *>(uintptr_t(1))); p->m_Data = p->m_Format.Image;
        SignedPitchBytes encoded(owner.Reader); EXPECT_EQ(0,owner.Reader->SaveMemory(&encoded.Data,p)); EXPECT_EQ(nullptr,encoded.Data);
#endif
    }
}
TEST(CKImageSaveSignedPitchTest, NegativeRowsArePreservedByFileAndMemoryWriters) { SignedPitchCheck(0,1,8,true,1,true); SignedPitchCheck(1,0,32,true,1,true); }
TEST(CKImageSaveSignedPitchTest, PositivePaddedRowControlsRemainCorrect) { SignedPitchCheck(0,0,32,false,0,false,false); SignedPitchCheck(1,1,24,true,0,false,false); }

static void SignedPitchWideRows(int kind) {
    const int width = 129, height = 3, sourceBytes = kind ? 3 : 4, pitch = width * sourceBytes + 13;
    std::vector<CKBYTE> storage(pitch * height + 64, 0xcc), expected(width * height * 4);
    CKBYTE *rowZero = storage.data() + 32 + pitch * (height - 1);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        CKBYTE color[] = {CKBYTE(x * 13 + y * 3), CKBYTE(x * 11 + y * 29), CKBYTE(x * 7 + y * 101), CKBYTE(x + y * 71)};
        memcpy(rowZero - y * pitch + x * sourceBytes, color, sourceBytes);
        if (kind) color[3] = 255;
        memcpy(&expected[(y * width + x) * 4], color, 4);
    }
    std::vector<CKBYTE> before = storage;
    for (int reader = 0; reader < 2; ++reader) {
        SignedPitchReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga; bmp.m_BitDepth = tga.m_BitDepth = 32; tga.m_UseRLE = 1;
        CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
        ImageReader::FillFormatBGRA32(p->m_Format,width,height,-pitch,rowZero); p->m_Data = rowZero;
        p->m_Format.BitsPerPixel = sourceBytes * 8; if (kind) p->m_Format.AlphaMask = 0;
        SignedPitchBytes encoded(owner.Reader); int size = owner.Reader->SaveMemory(&encoded.Data,p); ASSERT_GT(size,0);
        std::vector<CKBYTE> decoded; SignedPitchDecode(encoded.Data,size,reader,32,reader != 0,width,height,decoded);
        EXPECT_EQ(expected,decoded); EXPECT_EQ(before,storage); EXPECT_EQ(-pitch,p->m_Format.BytesPerLine);
    }
}
TEST(CKImageSaveSignedPitchTest, WideNegativeBgraRowsKeepBlockAndTailPixels) { SignedPitchWideRows(0); }
TEST(CKImageSaveSignedPitchTest, WideNegativeBgr24RowsKeepBlockAndTailPixels) { SignedPitchWideRows(1); }
