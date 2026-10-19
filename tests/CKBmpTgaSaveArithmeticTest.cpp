#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <climits>
#include <new>
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

// Observe the actual production allocation boundary. No encoder is replaced.
// Mode 2 stops before touching a deliberately unbacked huge source descriptor.
static int SaveAllocationMode = 0;
static int SaveAllocationCount = 0;
static int SaveGuardErrors = 0;
static size_t SaveFirstAllocation = 0;
static size_t SaveLargestAllocation = 0;
struct alignas(std::max_align_t) SaveAllocationHeader { size_t Size; bool Tracked; };

void *operator new[](size_t size) {
    if (SaveAllocationMode) {
        if (!SaveAllocationCount++) SaveFirstAllocation = size;
        if (size > SaveLargestAllocation) SaveLargestAllocation = size;
        if (SaveAllocationMode == 2) throw std::bad_alloc();
    }
    if (size > SIZE_MAX - sizeof(SaveAllocationHeader) - 32) throw std::bad_alloc();
    SaveAllocationHeader *h = static_cast<SaveAllocationHeader *>(std::malloc(sizeof(*h) + size + 32));
    if (!h) throw std::bad_alloc();
    h->Size = size; h->Tracked = SaveAllocationMode != 0;
    unsigned char *bytes = reinterpret_cast<unsigned char *>(h + 1);
    memset(bytes + size, 0xa7, 32);
    return bytes;
}
void operator delete[](void *memory) noexcept {
    if (!memory) return;
    SaveAllocationHeader *h = static_cast<SaveAllocationHeader *>(memory) - 1;
    if (h->Tracked) {
        const unsigned char *bytes = static_cast<unsigned char *>(memory);
        for (int i = 0; i < 32; ++i) if (bytes[h->Size + i] != 0xa7) { ++SaveGuardErrors; break; }
    }
    std::free(h);
}
void operator delete[](void *memory, size_t) noexcept { ::operator delete[](memory); }

struct ArithmeticReader {
    CKBitmapReader *Reader;
    explicit ArithmeticReader(int index) {
        CKGet_ImageReader_PluginInfo(index);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~ArithmeticReader() { Reader->Release(); }
};
static void ArithmeticSource(CKBitmapProperties &p, CKBYTE *pixels, int w, int h, int pitch) {
    ImageReader::FillFormatBGRA32(p.m_Format, w, h, pitch, pixels); p.m_Data = pixels;
}
static void ArithmeticObserve(int mode) {
    SaveAllocationCount = SaveGuardErrors = 0;
    SaveFirstAllocation = SaveLargestAllocation = 0; SaveAllocationMode = mode;
}
static unsigned ArithmeticLe(const CKBYTE *p, int count) {
    unsigned result = 0; for (int i = 0; i < count; ++i) result |= unsigned(p[i]) << (i * 8);
    return result;
}
static void ArithmeticPreflight(int reader, int width, int height, int pitch, size_t expectedAllocation) {
    ArithmeticReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
    bmp.m_BitDepth = tga.m_BitDepth = 32;
    CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
    // No pixels are read in this preallocation witness. Any attempted allocation
    // is intercepted before encoding; it must have the mathematically correct size.
    ArithmeticSource(*p, reinterpret_cast<CKBYTE *>(uintptr_t(1)), width, height, pitch);
    void *encoded = nullptr; bool threw = false; int result = -1;
    ArithmeticObserve(2);
    try { result = owner.Reader->SaveMemory(&encoded, p); } catch (const std::bad_alloc &) { threw = true; }
    SaveAllocationMode = 0;
    EXPECT_EQ(nullptr, encoded);
    EXPECT_EQ(expectedAllocation != 0, threw);
    EXPECT_EQ(expectedAllocation ? 1 : 0, SaveAllocationCount);
    EXPECT_EQ(expectedAllocation, SaveFirstAllocation);
    if (!expectedAllocation) EXPECT_EQ(0, result);
}
TEST(CKBmpTgaSaveArithmeticTest, BmpWideRowRetainsTheHighBitsBeforeAllocation) {
    ArithmeticPreflight(0, 134217728, 1, 536870912, size_t(536870966));
}
TEST(CKBmpTgaSaveArithmeticTest, BmpWrappedPixelProductFailsBeforeReadingPixels) {
    ArithmeticPreflight(0, 4, 1073741824, 16, 0);
}
TEST(CKBmpTgaSaveArithmeticTest, BmpPositiveIntBoundaryIncludesTheFileHeader) {
    ArithmeticPreflight(0, 1, 536870898, 4, size_t(2147483646));
    ArithmeticPreflight(0, 1, 536870899, 4, 0);
}
TEST(CKBmpTgaSaveArithmeticTest, BmpDwordRepresentableButNegativeIntLengthIsRejected) {
    ArithmeticPreflight(0, 65536, 8192, 262144, 0);
}
TEST(CKBmpTgaSaveArithmeticTest, TgaPositiveIntBoundaryIncludesTheFileHeader) {
    ArithmeticPreflight(1, 32768, 16384, 131072, 0);
}
TEST(CKBmpTgaSaveArithmeticTest, TgaDwordPixelProductCannotWrapIntoAnAllocation) {
    ArithmeticPreflight(1, 65535, 65535, 262140, 0);
}
TEST(CKBmpTgaSaveArithmeticTest, OversizedRawFileFailsBeforeCreatingTheDestination) {
    for (int reader = 0; reader < 2; ++reader) {
        ArithmeticReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
        bmp.m_BitDepth = tga.m_BitDepth = 32;
        CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
        ArithmeticSource(*p, reinterpret_cast<CKBYTE *>(uintptr_t(1)), 32768, 16384, 131072);
        char name[80]; std::snprintf(name, sizeof(name), "ck-save-size-%p-%d.img", p, reader);
        std::filesystem::path path = std::filesystem::temp_directory_path() / name;
        std::string filename = path.string(); std::error_code ignored; std::filesystem::remove(path, ignored);
        ArithmeticObserve(2); bool threw = false; int result = -1;
        try { result = owner.Reader->SaveFile(const_cast<char *>(filename.c_str()), p); }
        catch (const std::bad_alloc &) { threw = true; }
        SaveAllocationMode = 0;
        EXPECT_FALSE(threw); EXPECT_EQ(0, result); EXPECT_EQ(0, SaveAllocationCount);
        EXPECT_FALSE(std::filesystem::exists(path)); std::filesystem::remove(path, ignored);
    }
}

// Independent format readers check final packets, palette colors, row order and
// exact stream termination. They never call the production decoding functions.
static void ArithmeticBmpPixels(const CKBYTE *data, int length, int width, int height,
                                const std::vector<CKBYTE> &pixels) {
    ASSERT_GE(length, 1078); ASSERT_EQ(unsigned(length), ArithmeticLe(data + 2, 4));
    ASSERT_EQ(8u, ArithmeticLe(data + 28, 2)); ASSERT_EQ(1u, ArithmeticLe(data + 30, 4));
    size_t pos = ArithmeticLe(data + 10, 4); int x = 0, y = 0;
    ASSERT_EQ(unsigned(length) - unsigned(pos), ArithmeticLe(data + 34, 4));
    bool end = false;
    while (pos < size_t(length) && !end) {
        ASSERT_LE(pos + 2, size_t(length)); unsigned count = data[pos++], value = data[pos++];
        if (!count && value <= 1) {
            if (value == 0) { EXPECT_EQ(width, x); x = 0; ++y; }
            else { EXPECT_EQ(height, y); end = true; }
            continue;
        }
        ASSERT_TRUE(count || value >= 3); unsigned n = count ? count : value;
        ASSERT_LE(x + int(n), width); ASSERT_LT(y, height);
        for (unsigned i = 0; i < n; ++i) {
            ASSERT_LT(pos, size_t(length)); unsigned index = count ? value : data[pos++];
            CKBYTE gray = pixels[(size_t(height - 1 - y) * width + x++) * 4];
            const CKBYTE *palette = data + 54 + index * 4;
            EXPECT_EQ(gray, palette[0]); EXPECT_EQ(gray, palette[1]); EXPECT_EQ(gray, palette[2]);
        }
        if (!count && (value & 1)) { ASSERT_LT(pos, size_t(length)); EXPECT_EQ(0, data[pos++]); }
    }
    EXPECT_TRUE(end); EXPECT_EQ(size_t(length), pos);
}
static void ArithmeticTgaPixels(const CKBYTE *data, int length, int width, int height,
                                int depth, const std::vector<CKBYTE> &pixels) {
    ASSERT_GE(length, 18); ASSERT_EQ(unsigned(width), ArithmeticLe(data + 12, 2));
    ASSERT_EQ(unsigned(height), ArithmeticLe(data + 14, 2));
    const int bytes = depth == 64 ? 1 : depth / 8;
    ASSERT_EQ(bytes * 8, data[16]); ASSERT_EQ(depth == 64 ? 11 : 10, data[2]);
    size_t pos = 18; int index = 0;
    while (index < width * height) {
        ASSERT_LT(pos, size_t(length)); CKBYTE packet = data[pos++]; int count = (packet & 127) + 1;
        ASSERT_LE(index + count, width * height);
        size_t start = pos;
        for (int i = 0; i < count; ++i, ++index) {
            size_t offset = (packet & 128) ? start : start + i * bytes;
            ASSERT_LE(offset + bytes, size_t(length));
            int y = index / width; int x = index % width;
            if (!(data[17] & 32)) y = height - 1 - y;
            if (data[17] & 16) x = width - 1 - x;
            const CKBYTE *pixel = &pixels[(size_t(y) * width + x) * 4];
            if (depth == 16) {
                unsigned word = ArithmeticLe(data + offset, 2);
                EXPECT_EQ(unsigned(pixel[0] / 8), word & 31);
                EXPECT_EQ(unsigned(pixel[1] / 8), (word >> 5) & 31);
                EXPECT_EQ(unsigned(pixel[2] / 8), (word >> 10) & 31);
            } else {
                for (int c = 0; c < bytes; ++c) EXPECT_EQ(pixel[c], data[offset + c]);
            }
        }
        pos = start + ((packet & 128) ? bytes : count * bytes);
    }
    EXPECT_EQ(size_t(length), pos);
}
static void ArithmeticPackets(int reader, int depth, int pattern) {
    const int width = pattern == 2 ? 512 : 517, height = pattern == 2 ? 512 : 5, pitch = width * 4 + 12;
    std::vector<CKBYTE> source(size_t(pitch) * height, 0xcc), expected(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        CKBYTE value = pattern == 2 ? 68 : pattern == 0 ? CKBYTE((x * 73 + y * 29) % 256) :
            CKBYTE(((x % 263) < 3 ? 0 : ((x % 263) < 5 ? 255 : (x % 263) < 133 ? 136 : x * 71)));
        CKBYTE *p = &source[size_t(y) * pitch + x * 4]; p[0] = p[1] = p[2] = value; p[3] = 255;
        memcpy(&expected[(size_t(y) * width + x) * 4], p, 4);
    }
    std::vector<CKBYTE> before = source;
    ArithmeticReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
    bmp.m_BitDepth = 9; tga.m_BitDepth = depth; tga.m_UseRLE = 1;
    CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
    ArithmeticSource(*p, source.data(), width, height, pitch);
    ArithmeticObserve(1); void *data = nullptr; int length = owner.Reader->SaveMemory(&data, p);
    SaveAllocationMode = 0;
    ASSERT_GT(length, 0); ASSERT_NE(nullptr, data);
    EXPECT_EQ(1, SaveAllocationCount); EXPECT_EQ(size_t(length), SaveFirstAllocation);
    EXPECT_EQ(size_t(length), SaveLargestAllocation);
    if (pattern == 2) EXPECT_LT(length, width * height / 8);
    if (reader) ArithmeticTgaPixels(static_cast<CKBYTE *>(data), length, width, height, depth, expected);
    else ArithmeticBmpPixels(static_cast<CKBYTE *>(data), length, width, height, expected);
    owner.Reader->ReleaseMemory(data); EXPECT_EQ(0, SaveGuardErrors); EXPECT_EQ(before, source);
}
TEST(CKBmpTgaSaveArithmeticTest, BmpLiteral255OddPaddingAndTailPacketsFitExactly) { ArithmeticPackets(0, 8, 0); }
TEST(CKBmpTgaSaveArithmeticTest, BmpMixedRunsAndLiteralsFitExactly) { ArithmeticPackets(0, 8, 1); }
TEST(CKBmpTgaSaveArithmeticTest, BmpCompressibleRowsDoNotNeedAnUncompressedBuffer) { ArithmeticPackets(0, 8, 2); }
TEST(CKBmpTgaSaveArithmeticTest, TgaGrayLiteral128TailPacketsFitExactly) { ArithmeticPackets(1, 64, 0); }
TEST(CKBmpTgaSaveArithmeticTest, TgaGrayMixedRunsAndLiteralsFitExactly) { ArithmeticPackets(1, 64, 1); }
TEST(CKBmpTgaSaveArithmeticTest, Tga16MixedRunsAndLiteralsFitExactly) { ArithmeticPackets(1, 16, 1); }
TEST(CKBmpTgaSaveArithmeticTest, Tga24MixedRunsAndLiteralsFitExactly) { ArithmeticPackets(1, 24, 1); }
TEST(CKBmpTgaSaveArithmeticTest, Tga32Literal128TailPacketsFitExactly) { ArithmeticPackets(1, 32, 0); }
TEST(CKBmpTgaSaveArithmeticTest, TgaCompressibleImageAllocatesOnlyItsEncodedLength) { ArithmeticPackets(1, 32, 2); }

struct ArithmeticVirtualImage {
    CKBYTE *Pixels = nullptr;
    size_t Size = 0;
    ~ArithmeticVirtualImage() {
#ifdef _WIN32
        if (Pixels) VirtualFree(Pixels, 0, MEM_RELEASE);
#else
        if (Pixels) munmap(Pixels, Size);
#endif
    }
};
#if UINTPTR_MAX == UINT32_MAX
static int ArithmeticSaveContained(CKBitmapReader *reader, void **data, CKBitmapProperties *p,
                                   bool *accessViolation) {
#if defined(_MSC_VER) && defined(_WIN32)
    __try { return reader->SaveMemory(data, p); }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        *accessViolation = true;
        return -1;
    }
#else
    return reader->SaveMemory(data, p);
#endif
}
#endif
static void ArithmeticSparseRows(int reader, bool rle) {
#if UINTPTR_MAX == UINT32_MAX
    ArithmeticReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
    bmp.m_BitDepth = rle ? 9 : 32; tga.m_BitDepth = 32; tga.m_UseRLE = rle;
    CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
    ArithmeticSource(*p, reinterpret_cast<CKBYTE *>(uintptr_t(1)), 1, 4, INT_MAX);
    ArithmeticObserve(2); void *data = nullptr; int length = -1; bool threw = false, accessed = false;
    try { length = ArithmeticSaveContained(owner.Reader, &data, p, &accessed); }
    catch (const std::bad_alloc &) { threw = true; }
    SaveAllocationMode = 0;
    EXPECT_FALSE(accessed); EXPECT_FALSE(threw); EXPECT_EQ(0, length);
    EXPECT_EQ(nullptr, data); EXPECT_EQ(0, SaveAllocationCount);
#else
    const size_t pitch = INT_MAX, extent = pitch * 3 + 4;
    ArithmeticVirtualImage image; image.Size = extent;
#ifdef _WIN32
    SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    image.Pixels = static_cast<CKBYTE *>(VirtualAlloc(nullptr, extent, MEM_RESERVE, PAGE_NOACCESS));
#else
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *reserved = mmap(nullptr, extent, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    image.Pixels = reserved == MAP_FAILED ? nullptr : static_cast<CKBYTE *>(reserved);
#endif
    ASSERT_NE(nullptr, image.Pixels);
    std::vector<CKBYTE> expected(16);
    for (int y = 0; y < 4; ++y) {
        CKBYTE *row = image.Pixels + size_t(y) * pitch;
        size_t start = (size_t(y) * pitch) & ~(page - 1);
#ifdef _WIN32
        ASSERT_NE(nullptr, VirtualAlloc(image.Pixels + start, page * 2, MEM_COMMIT, PAGE_READWRITE));
#else
        ASSERT_EQ(0, mprotect(image.Pixels + start, page * 2, PROT_READ | PROT_WRITE));
#endif
        row[0] = row[1] = row[2] = CKBYTE(y * 64); row[3] = 255;
        memcpy(&expected[y * 4], row, 4);
    }
    ArithmeticReader owner(reader); BmpBitmapProperties bmp; TgaBitmapProperties tga;
    bmp.m_BitDepth = rle ? 9 : 32; tga.m_BitDepth = 32; tga.m_UseRLE = rle;
    CKBitmapProperties *p = reader ? static_cast<CKBitmapProperties *>(&tga) : &bmp;
    ArithmeticSource(*p, image.Pixels, 1, 4, INT_MAX);
    void *data = nullptr; int length = owner.Reader->SaveMemory(&data, p); ASSERT_GT(length, 0);
    const CKBYTE *bytes = static_cast<CKBYTE *>(data);
    if (rle && reader) ArithmeticTgaPixels(bytes, length, 1, 4, 32, expected);
    else if (rle) ArithmeticBmpPixels(bytes, length, 1, 4, expected);
    else {
        int offset = reader ? 18 : 54; ASSERT_EQ(offset + 16, length);
        for (int y = 0; y < 4; ++y) EXPECT_EQ(0, memcmp(bytes + offset + y * 4, &expected[(3 - y) * 4], 4));
    }
    owner.Reader->ReleaseMemory(data);
#endif
}
TEST(CKBmpTgaSaveArithmeticTest, BmpRawSourceRowsBeyondDwordOffsets) { ArithmeticSparseRows(0, false); }
TEST(CKBmpTgaSaveArithmeticTest, BmpRleSourceRowsBeyondDwordOffsets) { ArithmeticSparseRows(0, true); }
TEST(CKBmpTgaSaveArithmeticTest, TgaRawSourceRowsBeyondDwordOffsets) { ArithmeticSparseRows(1, false); }
TEST(CKBmpTgaSaveArithmeticTest, TgaRleSourceRowsBeyondDwordOffsets) { ArithmeticSparseRows(1, true); }
