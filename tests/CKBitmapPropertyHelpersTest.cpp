#include <gtest/gtest.h>
#include <climits>
#include <cstring>
#include <new>

#include "CKBitmapReader.h"
#include "CKGlobals.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

struct BitmapPropertyGuardedPages {
    CKBYTE *Memory;
    size_t PageSize;

    BitmapPropertyGuardedPages() : Memory(nullptr), PageSize(0) {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        PageSize = info.dwPageSize;
        Memory = static_cast<CKBYTE *>(VirtualAlloc(nullptr, PageSize * 2,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        DWORD previous = 0;
        if (Memory && !VirtualProtect(Memory + PageSize, PageSize, PAGE_NOACCESS, &previous)) {
            VirtualFree(Memory, 0, MEM_RELEASE);
            Memory = nullptr;
        }
#else
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) return;
        PageSize = static_cast<size_t>(pageSize);
        void *memory = mmap(nullptr, PageSize * 2, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (memory == MAP_FAILED) return;
        Memory = static_cast<CKBYTE *>(memory);
        if (mprotect(Memory + PageSize, PageSize, PROT_NONE) != 0) {
            munmap(Memory, PageSize * 2);
            Memory = nullptr;
        }
#endif
    }

    ~BitmapPropertyGuardedPages() {
#ifdef _WIN32
        if (Memory) VirtualFree(Memory, 0, MEM_RELEASE);
#else
        if (Memory) munmap(Memory, PageSize * 2);
#endif
    }
};

TEST(CKBitmapPropertyHelpersTest, RejectsNullAndAllSizeBoundariesOutsideTheByteBlockContract) {
    EXPECT_EQ(nullptr, CKCopyBitmapProperties(nullptr));
    CKBitmapProperties properties;
    const int sizes[] = {INT_MIN, -256, -1, 0, 256, 257, INT_MAX};
    for (int i = 0; i < 7; ++i) {
        properties.m_Size = sizes[i];
        EXPECT_EQ(nullptr, CKCopyBitmapProperties(&properties)) << sizes[i];
    }
}

TEST(CKBitmapPropertyHelpersTest, CopiesEveryPositiveByteCountThrough255WithoutChangingTheSource) {
    alignas(CKBitmapProperties) CKBYTE storage[256];
    CKBYTE expected[256];
    for (int size = 1; size < 256; ++size) {
        for (int i = 0; i < 256; ++i) storage[i] = static_cast<CKBYTE>(i ^ size);
        CKBitmapProperties *source = new (storage) CKBitmapProperties;
        source->m_Size = size;
        memcpy(expected, storage, sizeof(expected));
        CKBitmapProperties *copy = CKCopyBitmapProperties(source);
        ASSERT_NE(nullptr, copy) << size;
        EXPECT_NE(source, copy);
        EXPECT_EQ(0, memcmp(expected, copy, size)) << size;
        EXPECT_EQ(0, memcmp(expected, storage, sizeof(storage))) << size;
        // A short block is raw bytes, not a complete properties object.
        storage[size - 1] ^= 0xff;
        EXPECT_EQ(expected[size - 1], reinterpret_cast<CKBYTE *>(copy)[size - 1]);
        CKDeleteBitmapProperties(copy);
        source->~CKBitmapProperties();
    }
}

TEST(CKBitmapPropertyHelpersTest, ReadsOnlyTheDeclaredBytesAtAnInaccessiblePageBoundary) {
    BitmapPropertyGuardedPages pages;
    ASSERT_NE(nullptr, pages.Memory);
    CKBYTE expected[256];
    const int sizes[] = {8, 16, static_cast<int>(sizeof(CKBitmapProperties)), 128, 248};
    for (int i = 0; i < 5; ++i) {
        const int size = sizes[i];
        CKBYTE *source = pages.Memory + pages.PageSize - size;
        for (int j = 0; j < size; ++j) source[j] = static_cast<CKBYTE>(j + 17);
        memcpy(source, &size, sizeof(size));
        memcpy(expected, source, size);
        // The pointer is aligned; only m_Size and the declared byte payload are
        // present. A base-size copy of the short blocks would cross the guard.
        CKBitmapProperties *copy = CKCopyBitmapProperties(reinterpret_cast<CKBitmapProperties *>(source));
        ASSERT_NE(nullptr, copy);
        EXPECT_EQ(0, memcmp(expected, copy, size));
        CKDeleteBitmapProperties(copy);
    }
}

TEST(CKBitmapPropertyHelpersTest, PreservesFlatExtensionBytesAndIndependentBorrowedPointerValues) {
    struct Options : CKBitmapProperties {
        CKDWORD Quality;
        CKBYTE Tail[19];
    };
    alignas(Options) CKBYTE storage[sizeof(Options)];
    memset(storage, 0xa5, sizeof(storage));
    Options *source = new (storage) Options;
    CKBYTE data[16] = {}, image[16] = {}, palette[8] = {};
    source->m_Size = sizeof(Options);
    source->m_ReaderGuid = CKGUID(0x11223344, 0x55667788);
    source->m_Ext = CKFileExtension(".PNG");
    source->m_Data = data;
    source->m_Format.Image = image;
    source->m_Format.ColorMap = palette;
    source->m_Format.Width = 2;
    source->m_Format.Height = 2;
    source->m_Format.BytesPerLine = 8;
    source->m_Format.BitsPerPixel = 32;
    source->Quality = 91;
    for (int i = 0; i < 19; ++i) source->Tail[i] = static_cast<CKBYTE>(0x80 + i);
    CKBitmapProperties *copy = CKCopyBitmapProperties(source);
    ASSERT_NE(nullptr, copy);
    EXPECT_EQ(0, memcmp(storage, copy, sizeof(storage)));
    EXPECT_EQ(data, copy->m_Data);
    EXPECT_EQ(image, copy->m_Format.Image);
    EXPECT_EQ(palette, copy->m_Format.ColorMap);
    EXPECT_STREQ("PNG", copy->m_Ext.m_Data);
    EXPECT_EQ(CKGUID(0x11223344, 0x55667788), copy->m_ReaderGuid);
    source->Quality = 0;
    const size_t offset = reinterpret_cast<CKBYTE *>(&source->Quality) - storage;
    CKDWORD copiedQuality = 0;
    memcpy(&copiedQuality, reinterpret_cast<CKBYTE *>(copy) + offset, sizeof(copiedQuality));
    EXPECT_EQ(91u, copiedQuality);
    CKDeleteBitmapProperties(copy);
    source->~Options();
}

TEST(CKBitmapPropertyHelpersTest, ReleasingTheCopyLeavesCallerOwnedDataImageAndPaletteAlive) {
    CKBYTE *data = new CKBYTE[16]{0x12};
    CKBYTE *image = new CKBYTE[16]{0x34};
    CKBYTE *palette = new CKBYTE[8]{0x56};
    CKBitmapProperties source;
    source.m_Data = data;
    source.m_Format.Image = image;
    source.m_Format.ColorMap = palette;
    CKBitmapProperties *copy = CKCopyBitmapProperties(&source);
    ASSERT_NE(nullptr, copy);
    CKDeleteBitmapProperties(copy);
    EXPECT_EQ(0x12, data[0]);
    EXPECT_EQ(0x34, image[0]);
    EXPECT_EQ(0x56, palette[0]);
    data[15] = 0x78;
    image[15] = 0x9a;
    palette[7] = 0xbc;
    EXPECT_EQ(data, source.m_Data);
    EXPECT_EQ(image, source.m_Format.Image);
    EXPECT_EQ(palette, source.m_Format.ColorMap);
    EXPECT_EQ(0x78, static_cast<CKBYTE *>(source.m_Data)[15]);
    EXPECT_EQ(0x9a, source.m_Format.Image[15]);
    EXPECT_EQ(0xbc, source.m_Format.ColorMap[7]);
    delete[] data;
    delete[] image;
    delete[] palette;
}

TEST(CKBitmapPropertyHelpersTest, CopiedStorageOutlivesItsSourceAndDeleteAcceptsNullAndByteArrays) {
    CKBitmapProperties *copy = nullptr;
    {
        CKBitmapProperties source;
        source.m_ReaderGuid = CKGUID(7, 19);
        source.m_Ext = CKFileExtension("bmp");
        source.m_Format.Width = 123;
        copy = CKCopyBitmapProperties(&source);
    }
    ASSERT_NE(nullptr, copy);
    EXPECT_EQ(CKGUID(7, 19), copy->m_ReaderGuid);
    EXPECT_STREQ("bmp", copy->m_Ext.m_Data);
    EXPECT_EQ(123, copy->m_Format.Width);
    CKDeletePointer(copy);
    CKDeletePointer(nullptr);
    CKDeletePointer(new CKBYTE[0]);
    CKDeletePointer(new CKBYTE[1]{0x12});
    CKDeletePointer(new CKBYTE[4096]{});
}
