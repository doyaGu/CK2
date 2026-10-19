#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>

#include "CKBitmapReader.h"
#include "../../Plugins/ImageReader/BmpReader.h"
#include "../../Plugins/ImageReader/TgaReader.h"
#include "../../Plugins/ImageReader/PcxReader.h"

CKDataReader *CKGet_ImageReader_Reader(int index);
CKPluginInfo *CKGet_ImageReader_PluginInfo(int index);

// Observe actual codec array deletion without substituting the codec or allocator.
// All production translation units in this executable use these replaceable operators.
static void *ImageWatchedAllocation = nullptr;
static int ImageWatchedDeletes = 0;

void operator delete[](void *memory) noexcept {
    if (memory && memory == ImageWatchedAllocation)
        ++ImageWatchedDeletes;
    ::operator delete(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    ::operator delete[](memory);
}

struct ImageAllocationWatch {
    explicit ImageAllocationWatch(void *memory) {
        ImageWatchedAllocation = memory;
        ImageWatchedDeletes = 0;
    }
    ~ImageAllocationWatch() { ImageWatchedAllocation = nullptr; }
};

struct ImageLifecycleReader {
    CKBitmapReader *Reader;
    explicit ImageLifecycleReader(int index) {
        CKGet_ImageReader_PluginInfo(index);
        Reader = static_cast<CKBitmapReader *>(CKGet_ImageReader_Reader(index));
    }
    ~ImageLifecycleReader() { Release(); }
    void Release() {
        if (Reader) Reader->Release();
        Reader = nullptr;
    }
};

struct ImageLifecycleFile {
    std::filesystem::path Path;
    ImageLifecycleFile() {
        static unsigned int sequence = 0;
        char name[96];
        std::snprintf(name, sizeof(name), "ck-image-lifetime-%p-%u.img", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
    }
    ~ImageLifecycleFile() { std::error_code error; std::filesystem::remove(Path, error); }
};

static int ImageLifecycleSample(int reader, CKBYTE (&bytes)[134], bool alternate = false) {
    memset(bytes, 0, sizeof(bytes));
    int size;
    if (reader == 0) {
        bytes[0] = 'B'; bytes[1] = 'M'; bytes[2] = 62; bytes[10] = 54;
        bytes[14] = 40; bytes[18] = 2; bytes[22] = 1;
        bytes[26] = 1; bytes[28] = 24; bytes[34] = 8;
        const CKBYTE pixels[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        memcpy(bytes + 54, pixels, sizeof(pixels));
        size = 62;
        if (alternate) bytes[54] = 0x77;
    } else if (reader == 1) {
        bytes[2] = 2; bytes[12] = 2; bytes[14] = 1; bytes[16] = 24; bytes[17] = 0x20;
        const CKBYTE pixels[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        memcpy(bytes + 18, pixels, sizeof(pixels));
        size = 24;
        if (alternate) bytes[18] = 0x77;
    } else {
        bytes[0] = 10; bytes[1] = 5; bytes[2] = 1; bytes[3] = 8;
        bytes[8] = 1; bytes[65] = 3; bytes[66] = 2; bytes[68] = 1;
        const CKBYTE planes[] = {0x33, 0x66, 0x22, 0x55, 0x11, 0x44};
        memcpy(bytes + 128, planes, sizeof(planes));
        size = 134;
        if (alternate) bytes[132] = 0x77;
    }
    return size;
}

static void ImageLifecycleExpectPixels(CKBitmapProperties *properties, bool alternate = false) {
    ASSERT_NE(nullptr, properties);
    ASSERT_NE(nullptr, properties->m_Data);
    EXPECT_EQ(2, properties->m_Format.Width);
    EXPECT_EQ(1, properties->m_Format.Height);
    EXPECT_EQ(8, properties->m_Format.BytesPerLine);
    EXPECT_EQ(32, properties->m_Format.BitsPerPixel);
    EXPECT_EQ(properties->m_Data, properties->m_Format.Image);
    CKBYTE expected[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    if (alternate) expected[0] = 0x77;
    EXPECT_EQ(0, memcmp(expected, properties->m_Data, sizeof(expected)));
}

static CKBitmapProperties *ImageLifecycleSetOptions(CKBitmapReader *reader, int index,
                                                    CKBYTE *borrowedPixels, CKBYTE *borrowedPalette) {
    BmpBitmapProperties bmp;
    TgaBitmapProperties tga;
    PcxBitmapProperties pcx;
    bmp.m_BitDepth = tga.m_BitDepth = pcx.m_BitDepth = 32;
    tga.m_UseRLE = pcx.m_UseRLE = 1;
    CKBitmapProperties *input = index == 0 ? static_cast<CKBitmapProperties *>(&bmp)
        : index == 1 ? static_cast<CKBitmapProperties *>(&tga) : static_cast<CKBitmapProperties *>(&pcx);
    input->m_Data = borrowedPixels;
    input->m_Format.Image = borrowedPixels;
    input->m_Format.ColorMap = borrowedPalette;
    input->m_Format.Width = 123;
    reader->SetBitmapDefaultProperties(input);
    CKBitmapProperties *defaults = nullptr;
    reader->GetBitmapDefaultProperties(&defaults);
    return defaults;
}

TEST(CKImageReaderPropertyLifecycleTest, EveryReaderReturnsStableBorrowedDefaultsWithItsIdentity) {
    const int sizes[] = {sizeof(BmpBitmapProperties), sizeof(TgaBitmapProperties), sizeof(PcxBitmapProperties)};
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        ASSERT_NE(nullptr, owner.Reader);
        CKBitmapProperties *first = nullptr, *second = nullptr;
        owner.Reader->GetBitmapDefaultProperties(&first);
        owner.Reader->GetBitmapDefaultProperties(&second);
        ASSERT_NE(nullptr, first);
        EXPECT_EQ(first, second);
        EXPECT_EQ(sizes[i], first->m_Size);
        EXPECT_EQ(owner.Reader->GetReaderInfo()->m_GUID, first->m_ReaderGuid);
        EXPECT_EQ(nullptr, first->m_Data);
        EXPECT_EQ(nullptr, first->m_Format.Image);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, PcxRetainsReservedPropertiesWithoutAdvertisingSaving) {
    ImageLifecycleReader owner(2);
    CKBYTE pixels[8] = {}, palette[4] = {};
    CKBitmapProperties *defaults = ImageLifecycleSetOptions(owner.Reader, 2, pixels, palette);
    ASSERT_NE(nullptr, defaults);
    EXPECT_EQ(32u, static_cast<PcxBitmapProperties *>(defaults)->m_BitDepth);
    EXPECT_EQ(1u, static_cast<PcxBitmapProperties *>(defaults)->m_UseRLE);
    EXPECT_EQ(PCXREADER_GUID, defaults->m_ReaderGuid);
    EXPECT_EQ(0, defaults->m_Format.Width);
    EXPECT_EQ(nullptr, defaults->m_Data);
    EXPECT_EQ(nullptr, defaults->m_Format.Image);
    EXPECT_EQ(nullptr, defaults->m_Format.ColorMap);
    EXPECT_EQ(5, owner.Reader->GetFlags());
    EXPECT_EQ(0, owner.Reader->GetOptionsCount());
    void *encoded = pixels;
    EXPECT_EQ(0, owner.Reader->SaveMemory(&encoded, defaults));
    EXPECT_EQ(nullptr, encoded);
}

TEST(CKImageReaderPropertyLifecycleTest, DefaultOptionsRemainIndependentOfDecodedPixels) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE borrowed[8] = {}, palette[4] = {}, bytes[134];
        CKBitmapProperties *defaults = ImageLifecycleSetOptions(owner.Reader, i, borrowed, palette);
        ASSERT_NE(nullptr, defaults);
        CKBitmapProperties *decoded = nullptr;
        int size = ImageLifecycleSample(i, bytes);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        ImageLifecycleExpectPixels(decoded);
        EXPECT_NE(defaults, decoded);
        EXPECT_EQ(nullptr, defaults->m_Data);
        EXPECT_EQ(nullptr, defaults->m_Format.Image);
        EXPECT_EQ(nullptr, defaults->m_Format.ColorMap);
        EXPECT_EQ(0, defaults->m_Format.Width);
        CKDWORD depth;
        memcpy(&depth, reinterpret_cast<CKBYTE *>(defaults) + sizeof(CKBitmapProperties), sizeof(depth));
        EXPECT_EQ(32u, depth);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, UpdatingDefaultsDoesNotReplaceOrReleaseTheLiveImage) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE borrowed[8] = {}, palette[4] = {}, bytes[134];
        CKBitmapProperties *decoded = nullptr;
        int size = ImageLifecycleSample(i, bytes);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        ImageAllocationWatch watch(decoded->m_Data);
        ASSERT_NE(nullptr, ImageLifecycleSetOptions(owner.Reader, i, borrowed, palette));
        EXPECT_EQ(0, ImageWatchedDeletes);
        ImageLifecycleExpectPixels(decoded);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, SuccessfulMemoryReadReleasesThePreviousAllocation) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE bytes[134];
        CKBitmapProperties *decoded = nullptr, *second = nullptr;
        int size = ImageLifecycleSample(i, bytes);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        void *oldPixels = decoded->m_Data;
        ImageAllocationWatch watch(oldPixels);
        ImageLifecycleSample(i, bytes, true);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &second));
        EXPECT_EQ(decoded, second);
        EXPECT_EQ(1, ImageWatchedDeletes);
        ImageLifecycleExpectPixels(second, true);
        owner.Release();
        EXPECT_EQ(1, ImageWatchedDeletes);
        if (!ImageWatchedDeletes) ::operator delete[](oldPixels); // Clean up the reproduced pre-fix leak.
    }
}

TEST(CKImageReaderPropertyLifecycleTest, SuccessfulFileReadReleasesThePreviousAllocation) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        ImageLifecycleFile file;
        CKBYTE bytes[134];
        CKBitmapProperties *decoded = nullptr, *second = nullptr;
        int size = ImageLifecycleSample(i, bytes);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        void *oldPixels = decoded->m_Data;
        ImageLifecycleSample(i, bytes, true);
        FILE *stream = std::fopen(file.Path.string().c_str(), "wb");
        ASSERT_NE(nullptr, stream);
        size_t written = std::fwrite(bytes, 1, size, stream);
        int closed = std::fclose(stream);
        ASSERT_EQ(static_cast<size_t>(size), written);
        ASSERT_EQ(0, closed);
        ImageAllocationWatch watch(oldPixels);
        ASSERT_EQ(0, owner.Reader->ReadFile(const_cast<char *>(file.Path.string().c_str()), &second));
        EXPECT_EQ(decoded, second);
        EXPECT_EQ(1, ImageWatchedDeletes);
        ImageLifecycleExpectPixels(second, true);
        owner.Release();
        EXPECT_EQ(1, ImageWatchedDeletes);
        if (!ImageWatchedDeletes) ::operator delete[](oldPixels);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, RejectedHeaderPreservesThePreviousImage) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE bytes[134];
        CKBitmapProperties *decoded = nullptr, *failed = nullptr;
        int size = ImageLifecycleSample(i, bytes);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        void *oldPixels = decoded->m_Data;
        ImageAllocationWatch watch(oldPixels);
        CKBYTE invalid = 0;
        EXPECT_NE(0, owner.Reader->ReadMemory(&invalid, 1, &failed));
        EXPECT_EQ(decoded, failed);
        EXPECT_EQ(oldPixels, decoded->m_Data);
        EXPECT_EQ(0, ImageWatchedDeletes);
        ImageLifecycleExpectPixels(decoded);
        owner.Release();
        EXPECT_EQ(1, ImageWatchedDeletes);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, TruncatedTgaRlePreservesThePreviousImageAfterDecodeStarts) {
    ImageLifecycleReader owner(1);
    CKBYTE bytes[134];
    int size = ImageLifecycleSample(1, bytes);
    CKBitmapProperties *decoded = nullptr, *failed = nullptr;
    ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
    void *oldPixels = decoded->m_Data;
    ImageAllocationWatch watch(oldPixels);
    bytes[2] = 10; bytes[18] = 0x81; // Two-pixel RLE run, but only two of its three color bytes.
    EXPECT_NE(0, owner.Reader->ReadMemory(bytes, 21, &failed));
    EXPECT_EQ(decoded, failed);
    EXPECT_EQ(oldPixels, decoded->m_Data);
    EXPECT_EQ(0, ImageWatchedDeletes);
    ImageLifecycleExpectPixels(decoded);
}

TEST(CKImageReaderPropertyLifecycleTest, CallerCanReleaseAndClearPixelsBeforeReusingTheReader) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE bytes[134];
        int size = ImageLifecycleSample(i, bytes);
        CKBitmapProperties *decoded = nullptr;
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        owner.Reader->ReleaseMemory(decoded->m_Data);
        decoded->m_Data = nullptr;
        decoded->m_Format.Image = nullptr;
        ImageLifecycleSample(i, bytes, true);
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        ImageLifecycleExpectPixels(decoded, true);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, ReaderDestructionReleasesItsLatestPixelsExactlyOnce) {
    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        ImageLifecycleReader owner(i);
        CKBYTE bytes[134];
        int size = ImageLifecycleSample(i, bytes);
        CKBitmapProperties *decoded = nullptr;
        ASSERT_EQ(0, owner.Reader->ReadMemory(bytes, size, &decoded));
        ImageAllocationWatch watch(decoded->m_Data);
        owner.Release();
        EXPECT_EQ(1, ImageWatchedDeletes);
    }
}

TEST(CKImageReaderPropertyLifecycleTest, FreeBitmapDataClearsTheAliasToTheReleasedPixels) {
    BmpBitmapProperties properties;
    properties.m_Data = new CKBYTE[8];
    properties.m_Format.Image = static_cast<CKBYTE *>(properties.m_Data);
    ImageAllocationWatch watch(properties.m_Data);
    ImageReader::FreeBitmapData(&properties);
    EXPECT_EQ(1, ImageWatchedDeletes);
    EXPECT_EQ(nullptr, properties.m_Data);
    EXPECT_EQ(nullptr, properties.m_Format.Image);
    ImageReader::FreeBitmapData(&properties);
    EXPECT_EQ(1, ImageWatchedDeletes);
}

TEST(CKImageReaderPropertyLifecycleTest, FreeBitmapDataLeavesUnrelatedBorrowedViewsAndPaletteAlone) {
    CKBYTE borrowed[8] = {}, palette[4] = {};
    BmpBitmapProperties properties;
    properties.m_Data = new CKBYTE[8];
    properties.m_Format.Image = borrowed;
    properties.m_Format.ColorMap = palette;
    ImageAllocationWatch watch(properties.m_Data);
    ImageReader::FreeBitmapData(&properties);
    EXPECT_EQ(1, ImageWatchedDeletes);
    EXPECT_EQ(nullptr, properties.m_Data);
    EXPECT_EQ(borrowed, properties.m_Format.Image);
    EXPECT_EQ(palette, properties.m_Format.ColorMap);
}

TEST(CKImageReaderPropertyLifecycleTest, ChangedBmpDefaultsDriveTheActualEncodedHeaderAndPixels) {
    ImageLifecycleReader owner(0);
    BmpBitmapProperties wanted;
    wanted.m_BitDepth = 32;
    owner.Reader->SetBitmapDefaultProperties(&wanted);
    CKBitmapProperties *defaults = nullptr;
    owner.Reader->GetBitmapDefaultProperties(&defaults);
    ASSERT_NE(nullptr, defaults);
    CKBYTE pixels[] = {0x11, 0x22, 0x33, 0xff, 0x44, 0x55, 0x66, 0xff};
    ImageReader::FillFormatBGRA32(defaults->m_Format, 2, 1, 8, pixels);
    defaults->m_Data = pixels;
    void *encoded = nullptr;
    int length = owner.Reader->SaveMemory(&encoded, defaults);
    defaults->m_Data = nullptr;
    ASSERT_GT(length, 0);
    ASSERT_NE(nullptr, encoded);
    CKBYTE *bytes = static_cast<CKBYTE *>(encoded);
    EXPECT_EQ('B', bytes[0]); EXPECT_EQ('M', bytes[1]);
    EXPECT_EQ(32, bytes[28]); EXPECT_EQ(0, bytes[29]);
    CKDWORD offset = 0;
    memcpy(&offset, bytes + 10, sizeof(offset));
    ASSERT_LE(offset + sizeof(pixels), static_cast<CKDWORD>(length));
    EXPECT_EQ(0, memcmp(bytes + offset, pixels, sizeof(pixels)));
    owner.Reader->ReleaseMemory(encoded);
}

TEST(CKImageReaderPropertyLifecycleTest, ChangedTgaDefaultsDriveTheActualRlePacket) {
    ImageLifecycleReader owner(1);
    TgaBitmapProperties wanted;
    wanted.m_BitDepth = 32; wanted.m_UseRLE = 1;
    owner.Reader->SetBitmapDefaultProperties(&wanted);
    CKBitmapProperties *defaults = nullptr;
    owner.Reader->GetBitmapDefaultProperties(&defaults);
    ASSERT_NE(nullptr, defaults);
    CKBYTE pixels[] = {0x11, 0x22, 0x33, 0xff, 0x11, 0x22, 0x33, 0xff};
    ImageReader::FillFormatBGRA32(defaults->m_Format, 2, 1, 8, pixels);
    defaults->m_Data = pixels;
    void *encoded = nullptr;
    int length = owner.Reader->SaveMemory(&encoded, defaults);
    defaults->m_Data = nullptr;
    ASSERT_GE(length, 23);
    ASSERT_NE(nullptr, encoded);
    CKBYTE *bytes = static_cast<CKBYTE *>(encoded);
    EXPECT_EQ(10, bytes[2]); EXPECT_EQ(32, bytes[16]);
    EXPECT_EQ(1, bytes[18] & 0x7f); // A valid two-pixel packet may be raw or a repeated run.
    int payloadSize = bytes[18] & 0x80 ? 4 : 8;
    ASSERT_GE(length, 19 + payloadSize);
    EXPECT_EQ(0, memcmp(bytes + 19, pixels, payloadSize));
    owner.Reader->ReleaseMemory(encoded);
}
