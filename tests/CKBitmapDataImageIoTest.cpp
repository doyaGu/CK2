#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include "../../Plugins/ImageReader/BmpReader.cpp"

CKPluginInfo g_ImageReader_PluginInfo[READER_COUNT];

// Chunk serialization is outside the file reader/writer paths tested here.
CKSTRING CKJustFile(CKSTRING) {
    ADD_FAILURE() << "Unexpected bitmap chunk serialization path";
    return nullptr;
}

struct ImageIoOptions : CKBitmapProperties {
    int Marker = 0;
    ImageIoOptions() { m_Size = sizeof(ImageIoOptions); }
};

static struct ImageIoProbe {
    int Created, Released, Reads, Saves, Defaults, PixelReleases, PaletteReleases;
    int PixelsAtRelease, PaletteAtRelease, BorrowedAtRelease, LastReader, Format, ReadResult, SaveResult;
    int UnavailableMask;
    bool NullProperties, NullData, NullDefaults, RealBmp;
    XString ReadPath, SavePath;
    VxImageDescEx SavedDesc;
    void *SavedData;
    CKDWORD SavedPixels[16];
    int SavedMarker;
    void Reset() {
        Created = Released = Reads = Saves = Defaults = PixelReleases = PaletteReleases = 0;
        PixelsAtRelease = PaletteAtRelease = BorrowedAtRelease = 0;
        LastReader = -1; Format = ReadResult = UnavailableMask = 0; SaveResult = 37;
        NullProperties = NullData = NullDefaults = RealBmp = false;
        ReadPath = ""; SavePath = ""; SavedData = nullptr; SavedMarker = -1;
        memset(SavedPixels, 0, sizeof(SavedPixels));
    }
} ImageIo;

static VxImageDescEx ImageIoDesc(int bpp, int pitch, int width = 2, int height = 2) {
    VxImageDescEx d; d.Width = width; d.Height = height; d.BytesPerLine = pitch; d.BitsPerPixel = bpp;
    d.RedMask = R_MASK; d.GreenMask = G_MASK; d.BlueMask = B_MASK; d.AlphaMask = bpp == 32 ? A_MASK : 0;
    return d;
}

class ImageIoReader : public CKBitmapReader {
public:
    explicit ImageIoReader(int index) : Index(index), Borrowed(nullptr) {
        ++ImageIo.Created;
        Properties.m_ReaderGuid = g_ImageReader_PluginInfo[index].m_GUID;
        Properties.m_Ext = g_ImageReader_PluginInfo[index].m_Extension;
        Properties.Marker = 700 + index;
    }
    void Release() override {
        ++ImageIo.Released;
        if (Borrowed && Borrowed->m_Data) ++ImageIo.BorrowedAtRelease;
        if (Properties.m_Data) { ++ImageIo.PixelsAtRelease; delete[] static_cast<CKBYTE *>(Properties.m_Data); }
        if (Properties.m_Format.ColorMap) { ++ImageIo.PaletteAtRelease; delete[] Properties.m_Format.ColorMap; }
        delete this;
    }
    CKPluginInfo *GetReaderInfo() override { return &g_ImageReader_PluginInfo[Index]; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    void GetBitmapDefaultProperties(CKBitmapProperties **out) override {
        ++ImageIo.Defaults; *out = ImageIo.NullDefaults ? nullptr : &Properties;
    }
    void SetBitmapDefaultProperties(CKBitmapProperties *) override { ADD_FAILURE(); }
    int ReadFile(CKSTRING name, CKBitmapProperties **out) override {
        ++ImageIo.Reads; ImageIo.LastReader = Index; ImageIo.ReadPath = name;
        if (ImageIo.ReadResult) return ImageIo.ReadResult;
        if (ImageIo.NullProperties) { *out = nullptr; return 0; }
        *out = &Properties;
        if (ImageIo.NullData) return 0;
        if (ImageIo.Format == 2) {
            const CKBYTE data[] = {0x33,0x22,0x11,0x66,0x55,0x44,0xaa,0xbb,
                0x99,0x88,0x77,0xcc,0xbb,0xaa,0xcc,0xdd};
            Properties.m_Data = new CKBYTE[sizeof(data)]; memcpy(Properties.m_Data, data, sizeof(data));
            Properties.m_Format = ImageIoDesc(24, 8);
        } else if (ImageIo.Format == 3) {
            Properties.m_Data = new CKBYTE[8]{1,2,0xaa,0xbb,2,1,0xcc,0xdd};
            Properties.m_Format = ImageIoDesc(8, 4);
            Properties.m_Format.RedMask = Properties.m_Format.GreenMask = Properties.m_Format.BlueMask = 0;
        } else {
            const CKDWORD data[] = {0x10203040,0x50607080,0xdeadbeef,0x90a0b0c0,0xd0e0f010,0xcafebabe};
            Properties.m_Data = new CKBYTE[sizeof(data)]; memcpy(Properties.m_Data, data, sizeof(data));
            Properties.m_Format = ImageIoDesc(32, 12, ImageIo.Format == 4 ? 3 : ImageIo.Format == 5 ? 0 : 2);
            if (ImageIo.Format == 1) Properties.m_Format.AlphaMask = 0;
        }
        if (ImageIo.Format >= 3) {
            Properties.m_Format.ColorMapEntries = 256; Properties.m_Format.BytesPerColorEntry = 4;
            Properties.m_Format.ColorMap = new CKBYTE[1024]{};
            CKDWORD *colors = reinterpret_cast<CKDWORD *>(Properties.m_Format.ColorMap);
            colors[1] = 0xff112233; colors[2] = 0xff445566;
        }
        return 0;
    }
    int ReadMemory(void *, int, CKBitmapProperties **) override { ADD_FAILURE(); return 1; }
    int ReadASynchronousFile(CKSTRING, CKBitmapProperties **) override { ADD_FAILURE(); return 1; }
    int SaveMemory(void **, CKBitmapProperties *) override { ADD_FAILURE(); return 0; }
    int SaveFile(CKSTRING name, CKBitmapProperties *p) override {
        ++ImageIo.Saves; ImageIo.LastReader = Index; ImageIo.SavePath = name;
        ImageIo.SavedDesc = p->m_Format; ImageIo.SavedData = p->m_Data; Borrowed = p;
        if (p->m_Size == sizeof(ImageIoOptions)) ImageIo.SavedMarker = static_cast<ImageIoOptions *>(p)->Marker;
        int offset = 0;
        for (int y = 0; y < p->m_Format.Height; ++y)
            for (int x = 0; x < p->m_Format.Width; ++x) {
                if (offset < 16) memcpy(ImageIo.SavedPixels + offset,
                    static_cast<CKBYTE *>(p->m_Data) + y * p->m_Format.BytesPerLine + x * 4, 4);
                ++offset;
            }
        return ImageIo.SaveResult;
    }
    void ReleaseMemory(void *memory) override {
        if (memory == Properties.m_Data) ++ImageIo.PixelReleases;
        else if (memory == Properties.m_Format.ColorMap) ++ImageIo.PaletteReleases;
        else ADD_FAILURE() << "Released memory not returned by this reader";
        delete[] static_cast<CKBYTE *>(memory);
    }
private:
    int Index;
    ImageIoOptions Properties;
    CKBitmapProperties *Borrowed;
};

static int ImageIoCount() { return READER_COUNT; }
static CKPluginInfo *ImageIoInfo(int index) { return &g_ImageReader_PluginInfo[index]; }
static CKDataReader *ImageIoCreate(int index) {
    if (ImageIo.UnavailableMask & (1 << index)) return nullptr;
    if (ImageIo.RealBmp && index == 0) return new BmpReader;
    return new ImageIoReader(index);
}

static void ImageIoPixels(CKBitmapData &bitmap) {
    ASSERT_TRUE(bitmap.CreateImage(2, 2));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x00112233)); ASSERT_TRUE(bitmap.SetPixel(1, 0, 0x40556677));
    ASSERT_TRUE(bitmap.SetPixel(0, 1, 0x808899aa)); ASSERT_TRUE(bitmap.SetPixel(1, 1, 0xffbbccdd));
}

static void ImageIoExpectPixels(CKBitmapData &bitmap, const CKDWORD *expected, int slot = 0) {
    ASSERT_EQ(2, bitmap.GetWidth()); ASSERT_EQ(2, bitmap.GetHeight());
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
        EXPECT_EQ(expected[x + 2 * y], bitmap.GetPixel(x, y, slot));
}

static ImageIoOptions ImageIoSavedFormat(int reader, const char *ext, int marker = 12345) {
    ImageIoOptions p; p.m_ReaderGuid = g_ImageReader_PluginInfo[reader].m_GUID; p.m_Ext = ext; p.Marker = marker;
    return p;
}

static void ImageIoWord(CKBYTE *data, int at, CKDWORD value, int bytes = 4) {
    for (int i = 0; i < bytes; ++i) data[at + i] = static_cast<CKBYTE>(value >> (i * 8));
}

class CKBitmapDataImageIoTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        const char *extensions[] = {"bmp","qif","alt"};
        for (int i = 0; i < READER_COUNT; ++i) {
            g_ImageReader_PluginInfo[i].m_Type = CKPLUGIN_BITMAP_READER;
            g_ImageReader_PluginInfo[i].m_Extension = extensions[i];
            g_ImageReader_PluginInfo[i].m_GUID = i ? CKGUID(0x18510001, i) : BMPREADER_GUID;
        }
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("Bitmap image IO fixture", ImageIoCount, ImageIoInfo, ImageIoCreate));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override {
        ImageIo.Reset();
#ifdef _WIN32
        const int pid = _getpid();
#else
        const int pid = getpid();
#endif
        Directory = std::filesystem::temp_directory_path() / ("CKBitmapDataImageIo-" + std::to_string(pid)) /
            ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::create_directories(Directory);
    }
    void TearDown() override { std::filesystem::remove_all(Directory); }
    XString Path(const char *name) { return XString((Directory / name).string().c_str()); }
    void ExpectReleased(int pixels = 0, int palette = 0) {
        EXPECT_EQ(ImageIo.Created, ImageIo.Released); EXPECT_EQ(pixels, ImageIo.PixelReleases);
        EXPECT_EQ(palette, ImageIo.PaletteReleases); EXPECT_EQ(0, ImageIo.PixelsAtRelease);
        EXPECT_EQ(0, ImageIo.PaletteAtRelease); EXPECT_EQ(0, ImageIo.BorrowedAtRelease);
    }
    void LiteralBmp(XString name) {
        CKBYTE bytes[70] = {}; bytes[0] = 'B'; bytes[1] = 'M';
        ImageIoWord(bytes, 2, 70); ImageIoWord(bytes, 10, 54); ImageIoWord(bytes, 14, 40);
        ImageIoWord(bytes, 18, 2); ImageIoWord(bytes, 22, 2); ImageIoWord(bytes, 26, 1, 2);
        ImageIoWord(bytes, 28, 24, 2); ImageIoWord(bytes, 34, 16);
        const CKBYTE data[] = {0x99,0x88,0x77,0xcc,0xbb,0xaa,0xdd,0xee,
            0x33,0x22,0x11,0x66,0x55,0x44,0xaa,0xbb};
        memcpy(bytes + 54, data, sizeof(data));
        FILE *f = fopen(name.CStr(), "wb"); ASSERT_NE(nullptr, f);
        EXPECT_EQ(sizeof(bytes), fwrite(bytes, 1, sizeof(bytes), f)); EXPECT_EQ(0, fclose(f));
    }
    void ExpectSavedBmp(XString name, const CKBYTE expected[16]) {
        CKBYTE bytes[71] = {}; FILE *f = fopen(name.CStr(), "rb"); ASSERT_NE(nullptr, f);
        const size_t size = fread(bytes, 1, sizeof(bytes), f); EXPECT_EQ(0, fclose(f));
        ASSERT_EQ(70u, size); EXPECT_EQ('B', bytes[0]); EXPECT_EQ('M', bytes[1]);
        EXPECT_EQ(54, bytes[10]); EXPECT_EQ(40, bytes[14]); EXPECT_EQ(2, bytes[18]); EXPECT_EQ(2, bytes[22]);
        EXPECT_EQ(1, bytes[26]); EXPECT_EQ(24, bytes[28]);
        for (int i = 0; i < 16; ++i) EXPECT_EQ(expected[i], bytes[54 + i]) << "pixel byte " << i;
    }
    std::filesystem::path Directory;
};

TEST_F(CKBitmapDataImageIoTest, LoadPaddedArgbPreservesAlphaAndCopiesExtendedFormatOptions) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(XString("folder/source.QIF"), 0));
    const CKDWORD expected[] = {0x10203040,0x50607080,0x90a0b0c0,0xd0e0f010}; ImageIoExpectPixels(bitmap, expected);
    EXPECT_STREQ("folder/source.QIF", ImageIo.ReadPath.CStr()); EXPECT_STREQ("folder/source.QIF", bitmap.GetSlotFileName(0));
    ASSERT_NE(nullptr, bitmap.GetSaveFormat()); EXPECT_EQ(sizeof(ImageIoOptions), bitmap.GetSaveFormat()->m_Size);
    EXPECT_EQ(701, static_cast<ImageIoOptions *>(bitmap.GetSaveFormat())->Marker); EXPECT_EQ(1, ImageIo.LastReader);
    ExpectReleased(1);
}

TEST_F(CKBitmapDataImageIoTest, LoadWithoutAlphaMaskMakesEveryPixelOpaque) {
    ImageIo.Format = 1; CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(XString("source.qif")));
    const CKDWORD expected[] = {0xff203040,0xff607080,0xffa0b0c0,0xffe0f010}; ImageIoExpectPixels(bitmap, expected); ExpectReleased(1);
}

TEST_F(CKBitmapDataImageIoTest, LoadPadded24BitRgbConvertsAllRows) {
    ImageIo.Format = 2; CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(XString("source.qif")));
    const CKDWORD expected[] = {0xff112233,0xff445566,0xff778899,0xffaabbcc}; ImageIoExpectPixels(bitmap, expected); ExpectReleased(1);
}

TEST_F(CKBitmapDataImageIoTest, LoadIndexedImageUsesPaletteAndReleasesBothPluginBuffers) {
    ImageIo.Format = 3; CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(XString("source.qif")));
    const CKDWORD expected[] = {0xff112233,0xff445566,0xff445566,0xff112233}; ImageIoExpectPixels(bitmap, expected); ExpectReleased(1, 1);
}

TEST_F(CKBitmapDataImageIoTest, LoadHighSlotCreatesOnlyEmptyIntermediateSlots) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(XString("source.qif"), 3));
    EXPECT_EQ(4, bitmap.GetSlotCount()); EXPECT_EQ(0, bitmap.GetCurrentSlot());
    for (int i = 0; i < 3; ++i) { EXPECT_EQ(nullptr, bitmap.LockSurfacePtr(i)); EXPECT_STREQ("", bitmap.GetSlotFileName(i)); }
    const CKDWORD expected[] = {0x10203040,0x50607080,0x90a0b0c0,0xd0e0f010}; ImageIoExpectPixels(bitmap, expected, 3); ExpectReleased(1);
}

TEST_F(CKBitmapDataImageIoTest, LoadMissingReaderAndNegativeSlotLeaveExistingImageUntouched) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    EXPECT_FALSE(bitmap.LoadSlotImage(XString("source.zzz"))); EXPECT_FALSE(bitmap.LoadSlotImage(XString("source.qif"), -1));
    EXPECT_EQ(0, ImageIo.Created); EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0));
}

TEST_F(CKBitmapDataImageIoTest, ReadErrorReleasesReaderAndKeepsPixelsNameAndSaveFormat) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ASSERT_TRUE(bitmap.SetSlotFileName(0, "old.bmp"));
    ImageIoOptions p = ImageIoSavedFormat(2, "alt"); bitmap.SetSaveFormat(&p); CKBitmapProperties *format = bitmap.GetSaveFormat();
    ImageIo.ReadResult = CKBITMAPERROR_READERROR; EXPECT_FALSE(bitmap.LoadSlotImage(XString("bad.qif")));
    EXPECT_EQ(1, ImageIo.Reads); EXPECT_EQ(0, ImageIo.Saves); EXPECT_EQ(format, bitmap.GetSaveFormat());
    EXPECT_STREQ("old.bmp", bitmap.GetSlotFileName(0)); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0)); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, NullReadPropertiesOrPixelsReturnFalseWithoutCreatingSlots) {
    CKBitmapData bitmap; ImageIo.NullProperties = true; EXPECT_FALSE(bitmap.LoadSlotImage(XString("bad.qif")));
    ImageIo.NullProperties = false; ImageIo.NullData = true; EXPECT_FALSE(bitmap.LoadSlotImage(XString("bad.qif")));
    EXPECT_EQ(0, bitmap.GetSlotCount()); EXPECT_EQ(2, ImageIo.Reads); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, FailedDimensionMatchRollsBackSlotGrowthAndReleasesDecodedBuffers) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    ASSERT_TRUE(bitmap.SetSlotFileName(0, "old.bmp")); ImageIo.Format = 4;
    EXPECT_FALSE(bitmap.LoadSlotImage(XString("different.qif"), 3));
    EXPECT_EQ(1, bitmap.GetSlotCount()); EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(2, bitmap.GetWidth());
    EXPECT_STREQ("old.bmp", bitmap.GetSlotFileName(0)); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0)); ExpectReleased(1, 1);
}

TEST_F(CKBitmapDataImageIoTest, InvalidDecodedDimensionsDoNotReplaceExistingSurface) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr(); ImageIo.Format = 5;
    EXPECT_FALSE(bitmap.LoadSlotImage(XString("zero.qif"))); EXPECT_EQ(surface, bitmap.LockSurfacePtr());
    EXPECT_EQ(2, bitmap.GetWidth()); ExpectReleased(1, 1);
}

TEST_F(CKBitmapDataImageIoTest, MovieModeRefusesSlotGrowthAndReleasesDecodedBuffers) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    bitmap.m_MovieInfo = new CKMovieInfo(XString("existing.avi")); ImageIo.Format = 3;
    EXPECT_FALSE(bitmap.LoadSlotImage(XString("source.qif"), 1));
    EXPECT_EQ(1, bitmap.m_Slots.Size()); EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); ExpectReleased(1, 1);
}

TEST_F(CKBitmapDataImageIoTest, SaveSelectsRequestedSlotAndNormalizesSuccessfulByteCountToTrue) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ASSERT_TRUE(bitmap.CreateImage(2, 2, 32, 1));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x12345678, 1));
    EXPECT_EQ(TRUE, bitmap.SaveImage("folder/result.QIF", 1));
    EXPECT_EQ(1, ImageIo.LastReader); EXPECT_STREQ("folder/result.QIF", ImageIo.SavePath.CStr());
    EXPECT_EQ(0x12345678u, ImageIo.SavedPixels[0]); EXPECT_EQ(0xff000000u, ImageIo.SavedPixels[3]);
    EXPECT_EQ(bitmap.LockSurfacePtr(1), ImageIo.SavedData); EXPECT_EQ(nullptr, ImageIo.SavedDesc.Image);
    EXPECT_EQ(32, ImageIo.SavedDesc.BitsPerPixel); EXPECT_EQ(8, ImageIo.SavedDesc.BytesPerLine); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, SaveWithoutExtensionChoosesBmpAndAddsItsSuffix) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); EXPECT_EQ(TRUE, bitmap.SaveImage("folder/result"));
    EXPECT_EQ(0, ImageIo.LastReader); EXPECT_STREQ("folder/result.bmp", ImageIo.SavePath.CStr()); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, RequestedSaveFormatUsesStoredGuidOptionsAndReplacesTheFilenameSuffix) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIoOptions p = ImageIoSavedFormat(2, "alt"); bitmap.SetSaveFormat(&p);
    EXPECT_EQ(TRUE, bitmap.SaveImage("folder/result.qif", 0, TRUE));
    EXPECT_EQ(2, ImageIo.LastReader); EXPECT_EQ(12345, ImageIo.SavedMarker); EXPECT_EQ(0, ImageIo.Defaults);
    EXPECT_STREQ("folder/result.alt", ImageIo.SavePath.CStr()); EXPECT_EQ(nullptr, bitmap.GetSaveFormat()->m_Data); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, IgnoringStoredFormatUsesReaderDefaultsForFilenameExtension) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIoOptions p = ImageIoSavedFormat(2, "alt"); bitmap.SetSaveFormat(&p);
    EXPECT_EQ(TRUE, bitmap.SaveImage("result.qif", 0, FALSE));
    EXPECT_EQ(1, ImageIo.LastReader); EXPECT_EQ(701, ImageIo.SavedMarker); EXPECT_EQ(1, ImageIo.Defaults); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, PreferredReaderGuidWinsEvenWhenItsExtensionDiffers) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIoOptions p = ImageIoSavedFormat(2, "qif"); bitmap.SetSaveFormat(&p);
    EXPECT_EQ(TRUE, bitmap.SaveImage("result.bmp", 0, TRUE)); EXPECT_EQ(2, ImageIo.LastReader);
    EXPECT_STREQ("result.qif", ImageIo.SavePath.CStr()); EXPECT_EQ(12345, ImageIo.SavedMarker); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, UnknownPreferredGuidFallsBackToTheStoredExtension) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIoOptions p = ImageIoSavedFormat(1, "qif");
    p.m_ReaderGuid = CKGUID(0x7fedcba9, 0x12345678); bitmap.SetSaveFormat(&p);
    EXPECT_EQ(TRUE, bitmap.SaveImage("result.bmp", 0, TRUE)); EXPECT_EQ(1, ImageIo.LastReader);
    EXPECT_EQ(12345, ImageIo.SavedMarker); EXPECT_EQ(0, ImageIo.Defaults); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, UnavailableStoredFormatFallsBackToBmpRatherThanFilenameExtension) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIoOptions p = ImageIoSavedFormat(2, "alt"); bitmap.SetSaveFormat(&p);
    ImageIo.UnavailableMask = 1 << 2;
    EXPECT_EQ(TRUE, bitmap.SaveImage("folder/result.qif", 0, TRUE)); EXPECT_EQ(0, ImageIo.LastReader);
    EXPECT_STREQ("folder/result.bmp", ImageIo.SavePath.CStr()); EXPECT_EQ(700, ImageIo.SavedMarker); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, UseFormatWithoutStoredPropertiesUsesTheFilenameAndDefaults) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); EXPECT_EQ(TRUE, bitmap.SaveImage("result.qif", 0, TRUE));
    EXPECT_EQ(1, ImageIo.LastReader); EXPECT_EQ(701, ImageIo.SavedMarker); EXPECT_EQ(1, ImageIo.Defaults); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, SaveFailureClearsBorrowedPixelPointerAndLeavesSourceIntact) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    ImageIoOptions p = ImageIoSavedFormat(1, "qif"); bitmap.SetSaveFormat(&p); ImageIo.SaveResult = 0;
    EXPECT_EQ(FALSE, bitmap.SaveImage("result.qif", 0, TRUE)); EXPECT_EQ(nullptr, bitmap.GetSaveFormat()->m_Data);
    EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0)); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, SaveWithoutReaderOrNameDoesNotCallWriter) {
    CKBitmapData bitmap; ImageIoPixels(bitmap);
    EXPECT_FALSE(bitmap.SaveImage(nullptr)); EXPECT_FALSE(bitmap.SaveImage("result.zzz"));
    EXPECT_EQ(0, ImageIo.Created); EXPECT_EQ(0, ImageIo.Saves);
}

TEST_F(CKBitmapDataImageIoTest, SaveWithoutSurfaceOrDefaultPropertiesReleasesReader) {
    CKBitmapData bitmap; EXPECT_FALSE(bitmap.SaveImage("result.qif"));
    ImageIoPixels(bitmap); ImageIo.NullDefaults = true; EXPECT_FALSE(bitmap.SaveImage("result.qif"));
    EXPECT_EQ(0, ImageIo.Saves); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, SaveHighSlotFallsBackToCurrentAsTheSurfaceContractRequires) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); EXPECT_EQ(TRUE, bitmap.SaveImage("result.qif", 99));
    EXPECT_EQ(0x00112233u, ImageIo.SavedPixels[0]); EXPECT_EQ(0xffbbccddu, ImageIo.SavedPixels[3]); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveReplicatesAlphaToOpaqueGrayAndPreservesRawWriterReturn) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    EXPECT_EQ(37, bitmap.SaveImageAlpha("result.qif"));
    const CKDWORD expected[] = {0xff000000,0xff404040,0xff808080,0xffffffff};
    for (int i = 0; i < 4; ++i) EXPECT_EQ(expected[i], ImageIo.SavedPixels[i]);
    EXPECT_NE(surface, ImageIo.SavedData); EXPECT_EQ(nullptr, ImageIo.SavedDesc.Image);
    EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0)); EXPECT_EQ(0x40556677u, bitmap.GetPixel(1, 0)); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveSelectsSlotAndUsesFilenameInsteadOfStoredSaveFormat) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ASSERT_TRUE(bitmap.CreateImage(2, 2, 32, 1));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0x22556677, 1)); ImageIoOptions p = ImageIoSavedFormat(2, "alt"); bitmap.SetSaveFormat(&p);
    EXPECT_EQ(37, bitmap.SaveImageAlpha("folder/result.qif", 1)); EXPECT_EQ(1, ImageIo.LastReader);
    EXPECT_EQ(0xff222222u, ImageIo.SavedPixels[0]); EXPECT_EQ(1, ImageIo.Defaults); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveWithoutSuffixUsesBmpAndDefaultCurrentSlot) {
    CKBitmapData bitmap; ImageIoPixels(bitmap);
    EXPECT_EQ(37, bitmap.SaveImageAlpha("folder/alpha", -1)); EXPECT_EQ(0, ImageIo.LastReader);
    EXPECT_STREQ("folder/alpha.bmp", ImageIo.SavePath.CStr()); EXPECT_EQ(0xff404040u, ImageIo.SavedPixels[1]); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveFailureCleansBorrowedPointerAndDoesNotChangeOriginalAlpha) {
    CKBitmapData bitmap; ImageIoPixels(bitmap); ImageIo.SaveResult = 0;
    EXPECT_EQ(0, bitmap.SaveImageAlpha("result.qif")); EXPECT_EQ(0x808899aau, bitmap.GetPixel(0, 1)); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveWithoutNameOrReaderDoesNotCallWriter) {
    CKBitmapData bitmap; ImageIoPixels(bitmap);
    EXPECT_FALSE(bitmap.SaveImageAlpha(nullptr)); EXPECT_FALSE(bitmap.SaveImageAlpha("result.zzz"));
    EXPECT_EQ(0, ImageIo.Created); EXPECT_EQ(0, ImageIo.Saves);
}

TEST_F(CKBitmapDataImageIoTest, AlphaSaveWithoutSurfaceOrDefaultsReleasesReader) {
    CKBitmapData bitmap; EXPECT_FALSE(bitmap.SaveImageAlpha("result.qif"));
    ImageIoPixels(bitmap); ImageIo.NullDefaults = true; EXPECT_FALSE(bitmap.SaveImageAlpha("result.qif"));
    EXPECT_EQ(0, ImageIo.Saves); ExpectReleased();
}

TEST_F(CKBitmapDataImageIoTest, RealBmpLoadUsesIndependentBottomUpFileAndExactVisiblePixels) {
    ImageIo.RealBmp = true; XString filename = Path("literal.bmp"); LiteralBmp(filename);
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadSlotImage(filename));
    const CKDWORD expected[] = {0xff112233,0xff445566,0xff778899,0xffaabbcc}; ImageIoExpectPixels(bitmap, expected);
    EXPECT_STREQ(filename.CStr(), bitmap.GetSlotFileName(0));
}

TEST_F(CKBitmapDataImageIoTest, RealBmpSaveWritesIndependentBottomUpHeaderAndPixelBytes) {
    ImageIo.RealBmp = true; CKBitmapData bitmap; ImageIoPixels(bitmap); XString filename = Path("output.bmp");
    EXPECT_EQ(TRUE, bitmap.SaveImage(filename.Str()));
    const CKBYTE expected[] = {0xaa,0x99,0x88,0xdd,0xcc,0xbb,0,0,0x33,0x22,0x11,0x77,0x66,0x55,0,0};
    ExpectSavedBmp(filename, expected);
}

TEST_F(CKBitmapDataImageIoTest, RealBmpAlphaSaveWritesOpaqueGrayBytesWithoutModifyingSource) {
    ImageIo.RealBmp = true; CKBitmapData bitmap; ImageIoPixels(bitmap); XString filename = Path("alpha.bmp");
    EXPECT_EQ(70, bitmap.SaveImageAlpha(filename.Str()));
    const CKBYTE expected[] = {0x80,0x80,0x80,0xff,0xff,0xff,0,0,0,0,0,0x40,0x40,0x40,0,0};
    ExpectSavedBmp(filename, expected); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0)); EXPECT_EQ(0xffbbccddu, bitmap.GetPixel(1, 1));
}

TEST_F(CKBitmapDataImageIoTest, RealBmpMissingCorruptAndUnwritableFilesReportFailure) {
    ImageIo.RealBmp = true; CKBitmapData bitmap; ImageIoPixels(bitmap); CKBYTE *surface = bitmap.LockSurfacePtr();
    XString missing = Path("missing.bmp"); EXPECT_FALSE(bitmap.LoadSlotImage(missing));
    XString corrupt = Path("corrupt.bmp"); FILE *f = fopen(corrupt.CStr(), "wb"); ASSERT_NE(nullptr, f);
    EXPECT_EQ(6u, fwrite("notbmp", 1, 6, f)); EXPECT_EQ(0, fclose(f)); EXPECT_FALSE(bitmap.LoadSlotImage(corrupt));
    XString output = Path("missing/output.bmp"); EXPECT_FALSE(bitmap.SaveImage(output.Str())); EXPECT_FALSE(bitmap.SaveImageAlpha(output.Str()));
    EXPECT_EQ(surface, bitmap.LockSurfacePtr()); EXPECT_EQ(0x00112233u, bitmap.GetPixel(0, 0));
}
