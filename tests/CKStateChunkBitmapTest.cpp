#include <gtest/gtest.h>
#include <cstring>

#include "CKAll.h"
#include "CKBitmapReader.h"
#include "CKPluginManager.h"
#include "VxWindowFunctions.h"

namespace {

const CKBYTE DecodedPixels[] = {1,2,3,255, 4,5,6,255, 7,8,9,255, 10,11,12,255};
const CKBYTE EncodedBytes[] = {0x31,0x32,0x33,0x34,0x35};
const int Sentinel = 0x13572468;

VxImageDescEx ImageDesc(CKBYTE *pixels, int width = 2, int height = 2, int pitch = 8) {
    VxImageDescEx desc;
    desc.Width = width; desc.Height = height; desc.BytesPerLine = pitch;
    desc.BitsPerPixel = 32; desc.RedMask = R_MASK; desc.GreenMask = G_MASK;
    desc.BlueMask = B_MASK; desc.AlphaMask = A_MASK; desc.Image = pixels;
    return desc;
}

// Independent wire construction; no CKStateChunk writers manufacture inputs.
struct BitmapWire {
    int Words[4096]; unsigned int Masks[4096]; int Count;
    BitmapWire() : Count(0) { std::memset(Words, 0, sizeof(Words)); std::memset(Masks, 0, sizeof(Masks)); }
    void Int(int value) { Words[Count] = value; Masks[Count++] = 0xFFFFFFFFu; }
    void Bytes(const void *bytes, int count) {
        std::memcpy(Words + Count, bytes, count);
        CKBYTE *mask = reinterpret_cast<CKBYTE *>(Masks + Count);
        for (int i = 0; i < count; ++i) mask[i] = 255;
        Count += (count + 3) / 4;
    }
    void Buffer(const void *bytes, int count) { Int(count); if (count) Bytes(bytes, count); }
    void Load(CKStateChunk &chunk) const {
        int envelope[4098] = {};
        envelope[0] = (7 << 16) | (CKCID_TEXTURE << 8); envelope[1] = Count;
        std::memcpy(envelope + 2, Words, Count * 4);
        ASSERT_TRUE(chunk.ConvertFromBuffer(envelope)); chunk.StartRead();
    }
    void ExpectWritten(CKStateChunk &chunk) const {
        chunk.CloseChunk(); ASSERT_EQ(Count * 4, chunk.GetDataSize());
        chunk.StartRead();
        for (int i = 0; i < Count; ++i)
            EXPECT_EQ(static_cast<unsigned int>(Words[i]) & Masks[i],
                      static_cast<unsigned int>(chunk.ReadInt()) & Masks[i]) << "word " << i;
    }
};

struct ReaderProbe {
    bool AlphaSaved, FailSave, FailRead, Palette, Unavailable;
    int Created, Released, Saves, Reads, MemoryReleases, PixelReleases, PaletteReleases, SaveReleases;
    int LastReader, LastSize; CKBYTE LastBytes[80], SavedPixels[80]; VxImageDescEx SavedDesc;
    void Reset() { AlphaSaved = true; FailSave = FailRead = Palette = Unavailable = false;
        Created = Released = Saves = Reads = MemoryReleases = PixelReleases = PaletteReleases = SaveReleases = 0;
        LastReader = LastSize = -1; std::memset(LastBytes, 0, sizeof(LastBytes)); std::memset(SavedPixels, 0, sizeof(SavedPixels)); }
} Probe;

CKPluginInfo ReaderInfos[7];

class BitmapTestReader : public CKBitmapReader {
public:
    explicit BitmapTestReader(int index = 6) : Index(index), Saved(nullptr) { ++Probe.Created; }
    void Release() override { ++Probe.Released; delete this; }
    CKPluginInfo *GetReaderInfo() override { return &ReaderInfos[Index]; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    CKBOOL IsAlphaSaved(CKBitmapProperties *) override { return Probe.AlphaSaved; }
    void GetBitmapDefaultProperties(CKBitmapProperties **props) override { *props = &Props; }
    void SetBitmapDefaultProperties(CKBitmapProperties *) override {}
    int ReadFile(CKSTRING, CKBitmapProperties **) override { return 1; }
    int ReadASynchronousFile(CKSTRING, CKBitmapProperties **) override { return 1; }
    int SaveFile(CKSTRING, CKBitmapProperties *) override { return 0; }
    int ReadMemory(void *memory, int size, CKBitmapProperties **props) override {
        ++Probe.Reads; Probe.LastReader = Index; Probe.LastSize = size;
        if (size > 0 && size <= 80) std::memcpy(Probe.LastBytes, memory, size);
        if (Probe.FailRead) return CKBITMAPERROR_READERROR;
        Props.m_Data = new CKBYTE[16]; std::memcpy(Props.m_Data, DecodedPixels, 16);
        Props.m_Format = ImageDesc(static_cast<CKBYTE *>(Props.m_Data));
        if (Probe.Palette) { Props.m_Format.ColorMap = new CKBYTE[8]; std::memset(Props.m_Format.ColorMap, 0, 8); }
        *props = &Props; return 0;
    }
    int SaveMemory(void **memory, CKBitmapProperties *props) override {
        ++Probe.Saves; Probe.SavedDesc = props->m_Format;
        int offset = 0;
        for (int y = 0; y < props->m_Format.Height; ++y)
            for (int x = 0; x < props->m_Format.Width; ++x) {
                const int bytes = props->m_Format.BitsPerPixel / 8;
                if (offset + bytes <= 80) std::memcpy(Probe.SavedPixels + offset,
                    static_cast<CKBYTE *>(props->m_Data) + y * props->m_Format.BytesPerLine + x * bytes, bytes);
                offset += bytes;
            }
        if (Probe.FailSave) { *memory = nullptr; return 0; }
        Saved = new CKBYTE[5]; std::memcpy(Saved, EncodedBytes, 5); *memory = Saved; return 5;
    }
    void ReleaseMemory(void *memory) override {
        if (!memory) return;
        ++Probe.MemoryReleases;
        if (memory == Props.m_Data) ++Probe.PixelReleases;
        else if (memory == Props.m_Format.ColorMap) ++Probe.PaletteReleases;
        else if (memory == Saved) ++Probe.SaveReleases;
        delete[] static_cast<CKBYTE *>(memory);
    }
private:
    int Index; CKBitmapProperties Props; void *Saved;
};

int ReaderCount() { return 7; }
CKPluginInfo *ReaderInfo(int index) { return index >= 0 && index < 7 ? &ReaderInfos[index] : nullptr; }
CKDataReader *CreateReader(int index) { return Probe.Unavailable ? nullptr : new BitmapTestReader(index); }

class CKStateChunkBitmapTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        const char *extensions[] = {"bmp", "tga", "jpg", "tif", "gif", "pcx", "ckq"};
        for (int i = 0; i < 7; ++i) {
            ReaderInfos[i].m_GUID = CKGUID(0x34340001, i + 1);
            ReaderInfos[i].m_Extension = CKFileExtension(const_cast<char *>(extensions[i]));
            ReaderInfos[i].m_Type = CKPLUGIN_BITMAP_READER;
        }
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("Pass34 bitmap contract reader", ReaderCount, ReaderInfo, CreateReader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override { Probe.Reset(); }
};

void ReaderHeader(BitmapWire &wire, int type = 1, int size = 5, int guidIndex = 7) {
    wire.Int(type); wire.Int(0x00716B63); wire.Int(0x34340001); wire.Int(guidIndex); wire.Int(size);
    if (size > 0) wire.Bytes(EncodedBytes, size);
}

void LegacyFrame(BitmapWire &wire, const char *signature, bool custom = true) {
    CKBYTE bytes[10]; int size = 5;
    if (custom) { std::memcpy(bytes, signature, 5); std::memcpy(bytes + 5, EncodedBytes, 5); size = 10; }
    else std::memcpy(bytes, EncodedBytes, 5);
    wire.Int(size); wire.Buffer(bytes, size);
}

void RawFrame(BitmapWire &wire, int bits = 32, bool alpha = true) {
    wire.Int(bits); wire.Int(2); wire.Int(2); wire.Int(alpha ? static_cast<int>(A_MASK) : 0);
    wire.Int(bits == 16 ? 0xF800 : R_MASK); wire.Int(bits == 16 ? 0x07E0 : G_MASK);
    wire.Int(bits == 16 ? 0x001F : B_MASK); wire.Int(0);
    const CKBYTE blue[] = {9,13,1,5}, green[] = {10,14,2,6}, red[] = {11,15,3,7}, opacity[] = {12,16,4,8};
    wire.Buffer(blue, 4); wire.Buffer(green, 4); wire.Buffer(red, 4); wire.Buffer(opacity, alpha ? 4 : 0);
}

TEST_F(CKStateChunkBitmapTest, RawEmptyWriterAndLiteralReaderUseOneWordSentinel) {
    VxImageDescEx desc; CKStateChunk written(CKCID_TEXTURE, nullptr); written.StartWrite(); written.WriteRawBitmap(desc);
    BitmapWire wire; wire.Int(0); wire.ExpectWritten(written); wire.Int(Sentinel);
    CKStateChunk read(CKCID_TEXTURE, nullptr); wire.Load(read); desc.Width = 77;
    EXPECT_EQ(nullptr, read.ReadRawBitmap(desc)); EXPECT_EQ(77, desc.Width); EXPECT_EQ(Sentinel, read.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, RawLiteralPlanarPixelsAndDescriptorAreCanonicalBgra) {
    BitmapWire wire; RawFrame(wire); wire.Int(Sentinel); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    VxImageDescEx desc; CKBYTE *pixels = chunk.ReadRawBitmap(desc); ASSERT_NE(nullptr, pixels);
    const CKBYTE expected[] = {9,10,11,12,13,14,15,16,1,2,3,4,5,6,7,8};
    EXPECT_EQ(0, std::memcmp(pixels, expected, 16)); EXPECT_EQ(2, desc.Width); EXPECT_EQ(2, desc.Height);
    EXPECT_EQ(8, desc.BytesPerLine); EXPECT_EQ(32, desc.BitsPerPixel);
    EXPECT_EQ(R_MASK, desc.RedMask); EXPECT_EQ(G_MASK, desc.GreenMask); EXPECT_EQ(B_MASK, desc.BlueMask); EXPECT_EQ(A_MASK, desc.AlphaMask);
    delete[] pixels; EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, RawLiteralWithoutAlphaDefaultsEveryPixelToOpaque) {
    BitmapWire wire; RawFrame(wire, 24, false); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    VxImageDescEx desc; CKBYTE *pixels = chunk.ReadRawBitmap(desc); ASSERT_NE(nullptr, pixels);
    const CKBYTE expected[] = {9,10,11,255,13,14,15,255,1,2,3,255,5,6,7,255};
    EXPECT_EQ(0, std::memcmp(pixels, expected, 16)); delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, RawLiteral565DescriptorFeedsRealUpsideDownBlitWithoutColorCorruption) {
    BitmapWire wire; RawFrame(wire, 16, false); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    VxImageDescEx decoded; CKBYTE *pixels = chunk.ReadRawBitmap(decoded); ASSERT_NE(nullptr, pixels);
    decoded.Image = pixels; CKBYTE output[16] = {}; VxImageDescEx target = ImageDesc(output);
    VxDoBlitUpsideDown(decoded, target);
    const CKBYTE expected[] = {1,2,3,255,5,6,7,255,9,10,11,255,13,14,15,255};
    EXPECT_EQ(0, std::memcmp(output, expected, 16));
    EXPECT_EQ(R_MASK, decoded.RedMask); EXPECT_EQ(G_MASK, decoded.GreenMask); EXPECT_EQ(B_MASK, decoded.BlueMask);
    delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, RawShortPlaneReturnsFailureRatherThanReadingNeighborStorage) {
    BitmapWire wire; RawFrame(wire); wire.Words[8] = 1; // B length claims only one byte for four pixels.
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    CKBYTE *pixels = chunk.ReadRawBitmap(desc); EXPECT_EQ(nullptr, pixels); delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, RawMissingColorPlaneReturnsFailureInsteadOfUninitializedImage) {
    BitmapWire wire; RawFrame(wire); wire.Count = 8;
    wire.Int(0); wire.Int(0); wire.Int(0); wire.Int(0);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    CKBYTE *pixels = chunk.ReadRawBitmap(desc); EXPECT_EQ(nullptr, pixels); delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, RawIncompleteHeaderCannotPublishAnImage) {
    BitmapWire wire; wire.Int(32); wire.Int(2); wire.Int(2);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    CKBYTE *pixels = chunk.ReadRawBitmap(desc); EXPECT_EQ(nullptr, pixels); delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, RawUnknownCompressionConsumesItsPlanesAndLeavesNextRecord) {
    BitmapWire wire; RawFrame(wire); wire.Words[7] = 3; wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    EXPECT_EQ(nullptr, chunk.ReadRawBitmap(desc)); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, RawWriter32PaddedRowsMatchIndependentLosslessBottomUpPlaneFrame) {
    CKBYTE source[] = {1,2,3,4,5,6,7,8,99,99,99,99,9,10,11,12,13,14,15,16,99,99,99,99};
    VxImageDescEx desc = ImageDesc(source, 2, 2, 12);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteRawBitmap(desc);
    BitmapWire wire; RawFrame(wire); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, RawWriter24PaddedRowsMatchIndependentLosslessOpaquePlaneFrame) {
    CKBYTE source[] = {1,2,3,5,6,7,99,99,9,10,11,13,14,15,99,99};
    VxImageDescEx desc = ImageDesc(source, 2, 2, 8); desc.BitsPerPixel = 24; desc.AlphaMask = 0;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteRawBitmap(desc);
    BitmapWire wire; RawFrame(wire, 24, false); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, RawWriter565PreservesCorrectBlueGreenRedPlaneOrder) {
    CKWORD source[] = {0xF800,0x07E0,0x001F,0xFFFF};
    VxImageDescEx desc = ImageDesc(reinterpret_cast<CKBYTE *>(source), 2, 2, 4);
    desc.BitsPerPixel = 16; desc.RedMask = 0xF800; desc.GreenMask = 0x07E0; desc.BlueMask = 0x001F; desc.AlphaMask = 0;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteRawBitmap(desc);
    BitmapWire wire; wire.Int(16); wire.Int(2); wire.Int(2); wire.Int(0);
    wire.Int(0xF800); wire.Int(0x07E0); wire.Int(0x001F); wire.Int(0);
    const CKBYTE blue[] = {248,248,0,0}, green[] = {0,252,0,252}, red[] = {0,248,248,0};
    wire.Buffer(blue, 4); wire.Buffer(green, 4); wire.Buffer(red, 4); wire.Buffer(nullptr, 0); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, RawWriterCustom32MasksExtractChannelsFromDescriptor) {
    CKBYTE source[] = {1,2,3,4}; VxImageDescEx desc = ImageDesc(source, 1, 1, 4);
    desc.RedMask = 0x000000FF; desc.BlueMask = 0x00FF0000;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteRawBitmap(desc);
    BitmapWire wire; wire.Int(32); wire.Int(1); wire.Int(1); wire.Int(static_cast<int>(A_MASK));
    wire.Int(0x000000FF); wire.Int(G_MASK); wire.Int(0x00FF0000); wire.Int(0);
    wire.Buffer(source + 2, 1); wire.Buffer(source + 1, 1); wire.Buffer(source, 1); wire.Buffer(source + 3, 1);
    wire.ExpectWritten(chunk);
}

CKBitmapProperties WriterProperties() {
    CKBitmapProperties props; props.m_Ext = CKFileExtension("ckq"); props.m_ReaderGuid = CKGUID(0x34340001, 7); return props;
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterAlphaSavedUsesExactHeaderAndPluginSaveReleaseContract) {
    BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    VxImageDescEx desc = ImageDesc(const_cast<CKBYTE *>(DecodedPixels));
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire); wire.ExpectWritten(chunk);
    EXPECT_EQ(1, Probe.Saves); EXPECT_EQ(1, Probe.SaveReleases); EXPECT_EQ(0, Probe.Released);
    EXPECT_EQ(0, std::memcmp(Probe.SavedPixels, DecodedPixels, 16));
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterNoAlphaMaskNeedsNoSeparatePlane) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    VxImageDescEx desc = ImageDesc(const_cast<CKBYTE *>(DecodedPixels)); desc.AlphaMask = 0;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire); wire.ExpectWritten(chunk); EXPECT_EQ(1, Probe.SaveReleases);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterNullInputsAndSaveFailureEmitOneZeroWithoutTakingReaderOwnership) {
    BitmapTestReader reader; CKBitmapProperties props = WriterProperties(); VxImageDescEx empty;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(empty, &reader, &props);
    VxImageDescEx desc = ImageDesc(const_cast<CKBYTE *>(DecodedPixels)); Probe.FailSave = true;
    chunk.WriteReaderBitmap(desc, &reader, &props); chunk.WriteReaderBitmap(desc, nullptr, &props);
    chunk.WriteReaderBitmap(desc, &reader, nullptr); BitmapWire wire; for (int i = 0; i < 4; ++i) wire.Int(0);
    wire.ExpectWritten(chunk); EXPECT_EQ(1, Probe.Saves); EXPECT_EQ(0, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterConstantAlphaIgnoresRowPadding) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    CKBYTE source[] = {1,2,3,47,4,5,6,47,99,99,99,99}; VxImageDescEx desc = ImageDesc(source, 2, 1, 12);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(1); wire.Int(47); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterVariableAlphaCountsPixelsInsteadOfPaddedDwords) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    CKBYTE source[] = {1,2,3,4,5,6,7,8,99,99,99,99,9,10,11,12,13,14,15,16,99,99,99,99};
    VxImageDescEx desc = ImageDesc(source, 2, 2, 12); CKStateChunk chunk(CKCID_TEXTURE, nullptr);
    chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(4); const CKBYTE alpha[] = {4,8,12,16};
    wire.Buffer(alpha, 4); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriter4444AlphaTraversesTwoRowsForwardWithTheirPitch) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    CKWORD storage[16]; for (int i = 0; i < 16; ++i) storage[i] = 0xDDDD;
    storage[4] = 0xF123; storage[5] = 0x1456; storage[8] = 0x2789; storage[9] = 0x3ABC;
    VxImageDescEx desc = ImageDesc(reinterpret_cast<CKBYTE *>(storage + 4), 2, 2, 8);
    desc.BitsPerPixel = 16; desc.AlphaMask = 0xF000; desc.RedMask = 0x0F00; desc.GreenMask = 0x00F0; desc.BlueMask = 0x000F;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(4); const CKBYTE alpha[] = {240,16,32,48};
    wire.Buffer(alpha, 4); wire.ExpectWritten(chunk); EXPECT_EQ(32, Probe.SavedDesc.BitsPerPixel);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriterLowFourBitAlphaUsesExpansionWithoutNegativeShift) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    CKWORD source[] = {0x123F,0x4561}; VxImageDescEx desc = ImageDesc(reinterpret_cast<CKBYTE *>(source), 2, 1, 4);
    desc.BitsPerPixel = 16; desc.AlphaMask = 0x000F; desc.RedMask = 0xF000; desc.GreenMask = 0x0F00; desc.BlueMask = 0x00F0;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(2); const CKBYTE alpha[] = {240,16}; wire.Buffer(alpha, 2); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriter32CustomAlphaMaskReadsTheSpecifiedChannel) {
    Probe.AlphaSaved = false; BitmapTestReader reader; CKBitmapProperties props = WriterProperties();
    CKBYTE source[] = {47,1,2,3,59,4,5,6}; VxImageDescEx desc = ImageDesc(source, 2, 1, 8);
    desc.AlphaMask = 0x000000FF; desc.RedMask = 0xFF000000; desc.GreenMask = 0x00FF0000; desc.BlueMask = 0x0000FF00;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(2); const CKBYTE alpha[] = {47,59}; wire.Buffer(alpha, 2); wire.ExpectWritten(chunk);
}

TEST_F(CKStateChunkBitmapTest, ReaderWriter24InputIsConvertedToCanonical32BeforeSaveMemory) {
    BitmapTestReader reader; CKBitmapProperties props = WriterProperties(); CKBYTE source[] = {1,2,3,4,5,6,99,99};
    VxImageDescEx desc = ImageDesc(source, 2, 1, 8); desc.BitsPerPixel = 24; desc.AlphaMask = 0;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteReaderBitmap(desc, &reader, &props);
    const CKBYTE expected[] = {1,2,3,255,4,5,6,255}; EXPECT_EQ(0, std::memcmp(Probe.SavedPixels, expected, 8));
    EXPECT_EQ(32, Probe.SavedDesc.BitsPerPixel); EXPECT_EQ(8, Probe.SavedDesc.BytesPerLine);
}

TEST_F(CKStateChunkBitmapTest, ReaderLiteralPluginHeaderBlitsPixelsAndBalancesMemoryAndReader) {
    BitmapWire wire; ReaderHeader(wire); wire.Int(Sentinel); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output); ASSERT_TRUE(chunk.ReadReaderBitmap(desc));
    EXPECT_EQ(0, std::memcmp(output, DecodedPixels, 16)); EXPECT_EQ(6, Probe.LastReader);
    EXPECT_EQ(5, Probe.LastSize); EXPECT_EQ(0, std::memcmp(Probe.LastBytes, EncodedBytes, 5));
    EXPECT_EQ(1, Probe.PixelReleases); EXPECT_EQ(1, Probe.Released); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, ReaderLiteralConstantAlphaOverridesEveryDestinationPixel) {
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(1); wire.Int(47); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    ASSERT_TRUE(chunk.ReadReaderBitmap(desc)); for (int i = 0; i < 4; ++i) EXPECT_EQ(47, output[4*i+3]);
    EXPECT_EQ(1, Probe.Released); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, ReaderLiteralAlphaPlaneOverridesInForwardRowOrder) {
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(4); const CKBYTE alpha[] = {17,31,47,59}; wire.Buffer(alpha, 4);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    ASSERT_TRUE(chunk.ReadReaderBitmap(desc)); for (int i = 0; i < 4; ++i) EXPECT_EQ(alpha[i], output[4*i+3]);
}

TEST_F(CKStateChunkBitmapTest, ReaderDecodeFailureReleasesPluginWithoutAdvancingPayload) {
    Probe.FailRead = true; BitmapWire wire; ReaderHeader(wire); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16]; std::memset(output, 77, sizeof(output)); VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(5, chunk.GetCurrentPos()); EXPECT_EQ(1, Probe.Released);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(77, output[i]);
}

TEST_F(CKStateChunkBitmapTest, ReaderNegativePayloadLengthIsFailureAndNeverMovesCursorBackward) {
    BitmapWire wire; ReaderHeader(wire, 1, -8); wire.Int(Sentinel); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(5, chunk.GetCurrentPos());
    EXPECT_EQ(0, Probe.Reads); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderMissingSizeFieldDoesNotTreatEofAsValidEmptyPayload) {
    BitmapWire wire; ReaderHeader(wire, 1, 0); --wire.Count;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderTruncatedPayloadReturnsFailureAndBalancesReader) {
    BitmapWire wire; ReaderHeader(wire); wire.Words[4] = 12; CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(0, Probe.Reads); EXPECT_EQ(1, Probe.Released); EXPECT_EQ(5, chunk.GetCurrentPos());
}

TEST_F(CKStateChunkBitmapTest, ReaderMissingAlphaPlaneCannotReportACompleteImage) {
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(4); wire.Buffer(nullptr, 0);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderShortAlphaPlaneCannotBorrowNeighborBytes) {
    BitmapWire wire; ReaderHeader(wire, 2); wire.Int(4); const CKBYTE alpha[] = {47}; wire.Buffer(alpha, 1);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderZeroAndMissingDestinationStopAfterTheTypeWord) {
    BitmapWire wire; wire.Int(0); wire.Int(Sentinel); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output); EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(Sentinel, chunk.ReadInt());
    BitmapWire noDestination; ReaderHeader(noDestination); noDestination.Load(chunk); desc.Image = nullptr;
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(0, Probe.Created);
}

TEST_F(CKStateChunkBitmapTest, ReaderUnknownPreferredGuidFallsBackToTheDeclaredExtension) {
    BitmapWire wire; ReaderHeader(wire, 1, 5, 99); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output); EXPECT_TRUE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(6, Probe.LastReader);
}

TEST_F(CKStateChunkBitmapTest, Bitmap2KnownLegacySignaturesSelectReaderAndTransferPixelOwnership) {
    const char *signatures[] = {"CKBMP","CKDIB","CKTGA","CKJPG","CKTIF","CKGIF","CKPCX"};
    const int indices[] = {0,0,1,2,3,4,5};
    for (int i = 0; i < 7; ++i) {
        Probe.Reset(); BitmapWire wire; LegacyFrame(wire, signatures[i]); wire.Int(Sentinel);
        CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
        CKBYTE *pixels = chunk.ReadBitmap2(desc); ASSERT_NE(nullptr, pixels);
        EXPECT_EQ(0, std::memcmp(pixels, DecodedPixels, 16)); EXPECT_EQ(2, desc.Width); EXPECT_EQ(32, desc.BitsPerPixel);
        EXPECT_EQ(indices[i], Probe.LastReader); EXPECT_EQ(5, Probe.LastSize); EXPECT_EQ(0, std::memcmp(Probe.LastBytes, EncodedBytes, 5));
        EXPECT_EQ(0, Probe.PixelReleases); EXPECT_EQ(1, Probe.Released); EXPECT_EQ(Sentinel, chunk.ReadInt()); delete[] pixels;
    }
}

TEST_F(CKStateChunkBitmapTest, Bitmap2RawLegacyPayloadFallsBackToTgaWithoutStrippingBytes) {
    BitmapWire wire; LegacyFrame(wire, nullptr, false); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    VxImageDescEx desc; CKBYTE *pixels = chunk.ReadBitmap2(desc); ASSERT_NE(nullptr, pixels);
    EXPECT_EQ(1, Probe.LastReader); EXPECT_EQ(5, Probe.LastSize); EXPECT_EQ(0, std::memcmp(Probe.LastBytes, EncodedBytes, 5)); delete[] pixels;
}

TEST_F(CKStateChunkBitmapTest, Bitmap2DecodeFailureLeavesCallerDescriptorAndBalancesReader) {
    Probe.FailRead = true; BitmapWire wire; LegacyFrame(wire, "CKBMP"); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    VxImageDescEx desc; desc.Width = 77; EXPECT_EQ(nullptr, chunk.ReadBitmap2(desc)); EXPECT_EQ(77, desc.Width); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, LegacyShortPayloadIsConsumedWithoutReadingFiveByteHeaderOrCallingPlugin) {
    BitmapWire wire; wire.Int(3); const CKBYTE bytes[] = {1,2,3}; wire.Buffer(bytes, 3); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    EXPECT_EQ(nullptr, chunk.ReadBitmap2(desc)); EXPECT_EQ(Sentinel, chunk.ReadInt());
    wire.Load(chunk); EXPECT_EQ(nullptr, chunk.ReadBitmap()); EXPECT_EQ(Sentinel, chunk.ReadInt()); EXPECT_EQ(0, Probe.Created);
}

TEST_F(CKStateChunkBitmapTest, BitmapHandleDecodeProducesActualPixelsAndReleasesBothReaderAllocations) {
#ifndef _WIN32
    GTEST_SKIP() << "Handle pixel contract gate is Win32/x64; non-Windows surface implementation remains separate.";
#else
    Probe.Palette = true; BitmapWire wire; LegacyFrame(wire, "CKBMP"); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); BITMAP_HANDLE bitmap = chunk.ReadBitmap(); ASSERT_NE(nullptr, bitmap);
    VxImageDescEx desc; CKBYTE *pixels = VxConvertBitmap(bitmap, desc); ASSERT_NE(nullptr, pixels);
    EXPECT_EQ(0, std::memcmp(pixels, DecodedPixels, 16)); EXPECT_EQ(1, Probe.PixelReleases); EXPECT_EQ(1, Probe.PaletteReleases);
    EXPECT_EQ(1, Probe.Released); EXPECT_EQ(Sentinel, chunk.ReadInt()); delete[] pixels; VxDeleteBitmap(bitmap);
#endif
}

TEST_F(CKStateChunkBitmapTest, BitmapHandleDecodeFailureReleasesReaderAndReturnsNull) {
    Probe.FailRead = true; BitmapWire wire; LegacyFrame(wire, "CKBMP"); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    EXPECT_EQ(nullptr, chunk.ReadBitmap()); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, BitmapHandleWriterFallbackEncodesExactLegacyHeaderAndActualConvertedPixels) {
#ifndef _WIN32
    GTEST_SKIP() << "Handle pixel contract gate is Win32/x64.";
#else
    VxImageDescEx desc = ImageDesc(const_cast<CKBYTE *>(DecodedPixels)); BITMAP_HANDLE bitmap = VxCreateBitmap(desc); ASSERT_NE(nullptr, bitmap);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteBitmap(bitmap, "zzz");
    BitmapWire wire; LegacyFrame(wire, "CKBMP"); wire.ExpectWritten(chunk);
    EXPECT_EQ(1, Probe.Saves); EXPECT_EQ(1, Probe.SaveReleases); EXPECT_EQ(1, Probe.Released);
    EXPECT_EQ(0, std::memcmp(Probe.SavedPixels, DecodedPixels, 16)); VxDeleteBitmap(bitmap);
#endif
}

TEST_F(CKStateChunkBitmapTest, BitmapHandleNullWritesTwoZerosAndReleasesSelectedReader) {
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteBitmap(nullptr, "bmp");
    BitmapWire wire; wire.Int(0); wire.Int(0); wire.ExpectWritten(chunk); EXPECT_EQ(0, Probe.Saves); EXPECT_EQ(1, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, BitmapHandleSaveFailureWritesTwoZerosAndReleasesReader) {
#ifndef _WIN32
    GTEST_SKIP() << "Handle pixel contract gate is Win32/x64.";
#else
    Probe.FailSave = true; VxImageDescEx desc = ImageDesc(const_cast<CKBYTE *>(DecodedPixels));
    BITMAP_HANDLE bitmap = VxCreateBitmap(desc); ASSERT_NE(nullptr, bitmap);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite(); chunk.WriteBitmap(bitmap, "bmp");
    BitmapWire wire; wire.Int(0); wire.Int(0); wire.ExpectWritten(chunk); EXPECT_EQ(1, Probe.Released); VxDeleteBitmap(bitmap);
#endif
}


// Independently authored Pillow grayscale JPEG; a constant 1x1 value 37.
// Decoder regression never uses CKStateChunk/CKJpegDecoder to create input.
const CKBYTE LiteralGray37Jpeg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04, 0x03, 0x02, 0x02, 0x02, 0x02, 0x05, 0x04,
    0x04, 0x03, 0x04, 0x06, 0x05, 0x06, 0x06, 0x06, 0x05, 0x06, 0x06, 0x06, 0x07, 0x09, 0x08, 0x06,
    0x07, 0x09, 0x07, 0x06, 0x06, 0x08, 0x0B, 0x08, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x06, 0x08,
    0x0B, 0x0C, 0x0B, 0x0A, 0x0C, 0x09, 0x0A, 0x0A, 0x0A, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00,
    0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
    0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2,
    0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
    0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
    0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA,
    0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0xFC, 0x93, 0xAF, 0xFF, 0xD9
};

TEST_F(CKStateChunkBitmapTest, RawHistoricalJpegPlanesDecodeIndependentLiteralPixelsAndExactAlpha) {
    BitmapWire wire; wire.Int(32); wire.Int(1); wire.Int(1); wire.Int(static_cast<int>(A_MASK));
    wire.Int(R_MASK); wire.Int(G_MASK); wire.Int(B_MASK); wire.Int(0x11);
    for (int i = 0; i < 3; ++i) wire.Buffer(LiteralGray37Jpeg, sizeof(LiteralGray37Jpeg));
    const CKBYTE alpha[] = {47}; wire.Buffer(alpha, 1); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    CKBYTE *pixels = chunk.ReadRawBitmap(desc); ASSERT_NE(nullptr, pixels);
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(37, pixels[i], 1);
    EXPECT_EQ(47, pixels[3]); EXPECT_EQ(1, desc.Width); EXPECT_EQ(1, desc.Height);
    delete[] pixels; EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, RawCorruptJpegPlanesFailAndConsumeTheSeparateAlphaFrame) {
    BitmapWire wire; wire.Int(32); wire.Int(1); wire.Int(1); wire.Int(static_cast<int>(A_MASK));
    wire.Int(R_MASK); wire.Int(G_MASK); wire.Int(B_MASK); wire.Int(1);
    for (int i = 0; i < 3; ++i) wire.Buffer(EncodedBytes, 5);
    const CKBYTE alpha[] = {47}; wire.Buffer(alpha, 1); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc;
    EXPECT_EQ(nullptr, chunk.ReadRawBitmap(desc)); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST_F(CKStateChunkBitmapTest, ReaderUnavailablePluginStopsAfterExtensionAndGuid) {
    BitmapWire wire; ReaderHeader(wire, 1, 5, 99); wire.Words[1] = 0x007A7A7A;
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16] = {}; VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(0, Probe.Created);
}

TEST_F(CKStateChunkBitmapTest, LegacyReadersUnavailableReturnNullAndPreserveTheFollowingRecord) {
    Probe.Unavailable = true; BitmapWire wire; LegacyFrame(wire, "CKBMP"); wire.Int(Sentinel);
    CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk); VxImageDescEx desc; desc.Width = 77;
    EXPECT_EQ(nullptr, chunk.ReadBitmap2(desc)); EXPECT_EQ(77, desc.Width); EXPECT_EQ(Sentinel, chunk.ReadInt());
    wire.Load(chunk); EXPECT_EQ(nullptr, chunk.ReadBitmap()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    EXPECT_EQ(0, Probe.Created); EXPECT_EQ(0, Probe.Reads); EXPECT_EQ(0, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, BitmapWriterUnavailableRequestedAndFallbackReadersWriteTheEmptyFrame) {
    Probe.Unavailable = true; CKStateChunk chunk(CKCID_TEXTURE, nullptr); chunk.StartWrite();
    chunk.WriteBitmap(nullptr, "tga"); BitmapWire wire; wire.Int(0); wire.Int(0); wire.ExpectWritten(chunk);
    EXPECT_EQ(0, Probe.Created); EXPECT_EQ(0, Probe.Saves); EXPECT_EQ(0, Probe.Released);
}

TEST_F(CKStateChunkBitmapTest, ReaderRegisteredButUnavailableProviderReturnsFailureWithoutConsumingPayload) {
    Probe.Unavailable = true; BitmapWire wire; ReaderHeader(wire); CKStateChunk chunk(CKCID_TEXTURE, nullptr); wire.Load(chunk);
    CKBYTE output[16]; std::memset(output, 77, sizeof(output)); VxImageDescEx desc = ImageDesc(output);
    EXPECT_FALSE(chunk.ReadReaderBitmap(desc)); EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(0, Probe.Created);
    EXPECT_EQ(0, Probe.Reads); for (int i = 0; i < 16; ++i) EXPECT_EQ(77, output[i]);
}

} // namespace
