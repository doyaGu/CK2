#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization path"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

struct AviTimelineFile {
    std::filesystem::path Path;
    AviTimelineFile() {
        static unsigned sequence = 0;
        char name[80]; std::snprintf(name, sizeof(name), "avi-timeline-%p-%u.avi", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
    }
    ~AviTimelineFile() { std::error_code ignored; std::filesystem::remove(Path, ignored); }
    void Write(const void *data, size_t size) {
        FILE *file = std::fopen(Path.string().c_str(), "wb"); ASSERT_NE(nullptr, file);
        EXPECT_EQ(size, std::fwrite(data, 1, size, file)); EXPECT_EQ(0, std::fclose(file));
    }
};
struct AviTimelineBytes {
    CKBYTE Data[8192] = {};
    unsigned Size = 0;
    unsigned Offsets[8] = {}, Sizes[8] = {};
    void Word(unsigned at, unsigned value, unsigned bytes = 4) {
        for (unsigned i = 0; i < bytes; ++i) Data[at+i] = CKBYTE(value >> (8*i));
    }
    void Append(const void *data, unsigned size) { memcpy(Data+Size, data, size); Size += size; }
    unsigned Begin(const char *id) { unsigned at = Size; Append(id, 4); Size += 4; return at; }
    unsigned List(const char *id) { unsigned at = Begin("LIST"); Append(id, 4); return at; }
    void End(unsigned at) { Word(at+4, Size-at-8); if (Size & 1) Data[Size++] = 0; }
    void Make(bool indexed, unsigned emptyMask = 0, bool records = false, bool junk = false,
              bool delta = false, unsigned count = 4, unsigned start = 0, bool absoluteIndex = false,
              bool standardIndex = false) {
        Size = 0; memset(Data, 0, sizeof(Data));
        unsigned riff = Begin("RIFF"); Append("AVI ", 4);
        unsigned hdrl = List("hdrl"), avih = Begin("avih"); Size += 56;
        Word(avih+8, 40000); Word(avih+20, indexed ? 0x10 : 0); Word(avih+24, count);
        Word(avih+32, 1); Word(avih+40, delta ? 4 : 2); Word(avih+44, delta ? 4 : 2); End(avih);
        unsigned strl = List("strl"), strh = Begin("strh"); Size += 56;
        memcpy(Data+strh+8, "vids", 4); if (delta) memcpy(Data+strh+12, "CRAM", 4);
        Word(strh+28, 1); Word(strh+32, 25); Word(strh+36, start); Word(strh+40, count);
        Word(strh+44, 16); Word(strh+60, delta ? 4 : 2, 2); Word(strh+62, delta ? 4 : 2, 2); End(strh);
        unsigned strf = Begin("strf"); Size += 40;
        Word(strf+8, 40); Word(strf+12, delta ? 4 : 2); Word(strf+16, delta ? 4 : 2);
        Word(strf+20, 1, 2); Word(strf+22, delta ? 16 : 24, 2); Word(strf+28, delta ? 32 : 16);
        if (delta) memcpy(Data+strf+24, "CRAM", 4);
        End(strf);
        unsigned standard = 0;
        if (standardIndex) {
            standard = Begin("indx"); Size += 24+count*8;
            Word(standard+8, 2, 2); Word(standard+11, 1, 1); Word(standard+12, count);
            memcpy(Data+standard+16, delta ? "00dc" : "00db", 4);
            End(standard);
        }
        End(strl); End(hdrl);
        unsigned movi = List("movi"), rec = records ? List("rec ") : 0;
        for (unsigned i = 0; i < count; ++i) {
            if (junk) {
                unsigned j = Begin("JUNK"); Append("xyz", 3); End(j);
                j = Begin("01db"); Append("ignored!", 8); End(j);
            }
            Offsets[i] = Size; unsigned frame = Begin(delta ? "00dc" : "00db");
            if (!(emptyMask & (1u << i))) {
                if (delta) {
                    // 4x4 MS Video1 solid red/green blocks, then a delta skip block.
                    const CKBYTE red[] = {0x00, 0xfc}, green[] = {0xe0, 0x83}, skip[] = {0x01, 0x84};
                    Append(i == 0 ? red : i == 2 ? green : skip, 2);
                } else {
                    const CKBYTE first[] = {17,34,51,68,85,102,0,0,119,136,153,170,187,204,0,0};
                    const CKBYTE second[] = {1,2,3,4,5,6,0,0,7,8,9,10,11,12,0,0};
                    Append(i < 2 ? first : second, 16);
                }
            }
            Sizes[i] = Size-frame-8; End(frame);
        }
        if (records) End(rec); End(movi);
        if (standardIndex) {
            for (unsigned i = 0; i < count; ++i) {
                Word(standard+32+i*8, Offsets[i]+8);
                Word(standard+36+i*8, Sizes[i] | (delta && i != 0 && i != 2 ? 0x80000000u : 0));
            }
        }
        if (indexed) {
            unsigned idx = Begin("idx1");
            for (unsigned i = 0; i < count; ++i) {
                Append(delta ? "00dc" : "00db", 4); unsigned entry = Size; Size += 12;
                Word(entry, delta && i != 0 && i != 2 ? 0 : 0x10);
                Word(entry+4, absoluteIndex ? Offsets[i] : Offsets[i]-movi-8); Word(entry+8, Sizes[i]);
            }
            End(idx);
        }
        End(riff);
    }
};
struct AviTimelineReader {
    CKMovieReader *Reader;
    AviTimelineReader() { CKGet_AviReader_PluginInfo(0); Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0)); }
    ~AviTimelineReader() { Reader->Release(); }
    CKERROR Open(const AviTimelineFile &file) { return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str())); }
    void Expect(unsigned frame, int picture, bool delta = false) {
        CKMovieProperties *properties = nullptr;
        ASSERT_EQ(CK_OK, Reader->ReadFrame(frame, &properties)) << frame;
        ASSERT_NE(nullptr, properties); ASSERT_NE(nullptr, properties->m_Data);
        const CKBYTE *pixels = static_cast<const CKBYTE *>(properties->m_Data);
        EXPECT_EQ(delta ? 4 : 2, properties->m_Format.Width); EXPECT_EQ(32, properties->m_Format.BitsPerPixel);
        const CKBYTE expected[2][16] = {
            {17,34,51,255,68,85,102,255,119,136,153,255,170,187,204,255},
            {1,2,3,255,4,5,6,255,7,8,9,255,10,11,12,255}};
        if (delta || picture < 0) {
            for (int i = 0; i < (delta ? 16 : 4); ++i) {
                EXPECT_EQ(0, pixels[4*i]); EXPECT_EQ(picture == 1 ? 255 : 0, pixels[4*i+1]);
                EXPECT_EQ(picture == 0 ? 255 : 0, pixels[4*i+2]); EXPECT_EQ(255, pixels[4*i+3]);
            }
        } else EXPECT_EQ(0, memcmp(expected[picture], pixels, 16));
    }
};
class AVIReaderTimelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("AVI timeline", CKGet_AviReader_PluginInfoCount, CKGet_AviReader_PluginInfo, CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Check(bool indexed, unsigned mask, bool rec = false, bool junk = false, bool delta = false) {
        AviTimelineBytes bytes; bytes.Make(indexed, mask, rec, junk, delta); AviTimelineFile file; file.Write(bytes.Data, bytes.Size);
        AviTimelineReader owner; ASSERT_EQ(CK_OK, owner.Open(file));
        EXPECT_EQ(4, owner.Reader->GetMovieFrameCount()); EXPECT_EQ(160, owner.Reader->GetMovieLength());
        const unsigned order[] = {3,0,2,1,3,1,0,2,2};
        for (unsigned frame : order) {
            int source = int(frame); while (source >= 0 && (mask & (1u << source))) --source;
            if (source < 0) { source = 0; while (source < 4 && (mask & (1u << source))) ++source; }
            owner.Expect(frame, source == 4 ? -1 : source < 2 ? 0 : 1, delta);
        }
    }
};
TEST_F(AVIReaderTimelineTest, IndexedControlRetainsIndependentFrameIdentity) { Check(true, 0); }
TEST_F(AVIReaderTimelineTest, IndexlessVideoPublishesAllFramesAndDuration) { Check(false, 0); }
TEST_F(AVIReaderTimelineTest, IndexlessRecordListIsTraversed) { Check(false, 0, true); }
TEST_F(AVIReaderTimelineTest, IndexlessOddJunkAndOtherStreamChunksDoNotBecomeFrames) { Check(false, 0, false, true); }
TEST_F(AVIReaderTimelineTest, IndexlessRecordListWithInterleavedJunkPreservesFrameOrder) { Check(false, 0, true, true); }
TEST_F(AVIReaderTimelineTest, IndexedEmptyMiddleFrameKeepsItsTimeAndPreviousPicture) { Check(true, 2); }
TEST_F(AVIReaderTimelineTest, IndexedEmptyFinalFrameRemainsReadable) { Check(true, 8); }
TEST_F(AVIReaderTimelineTest, IndexedConsecutiveEmptyFramesDoNotCollapseTheTimeline) { Check(true, 6); }
TEST_F(AVIReaderTimelineTest, IndexedLeadingEmptyFramesUseTheFirstDecodedPicture) { Check(true, 3); }
TEST_F(AVIReaderTimelineTest, IndexedEntirelyEmptyMovieProducesOpaqueBlackFrames) { Check(true, 15); }
TEST_F(AVIReaderTimelineTest, IndexlessEmptyMiddleFrameKeepsItsTimeAndPreviousPicture) { Check(false, 2); }
TEST_F(AVIReaderTimelineTest, IndexlessEmptyFinalFrameRemainsReadable) { Check(false, 8); }
TEST_F(AVIReaderTimelineTest, IndexlessLeadingEmptyFramesUseTheFirstDecodedPicture) { Check(false, 3); }
TEST_F(AVIReaderTimelineTest, IndexlessEntirelyEmptyMovieProducesOpaqueBlackFrames) { Check(false, 15); }
TEST_F(AVIReaderTimelineTest, RecordListWithEmptyFramesKeepsRandomAccessIdentity) { Check(false, 10, true, true); }
TEST_F(AVIReaderTimelineTest, DeltaCodecControlKeepsItsPreviousFrameOnSkipBlocks) { Check(true, 0, false, false, true); }
TEST_F(AVIReaderTimelineTest, DeltaCodecIndexedDroppedFramesDoNotBreakDependencyReplay) { Check(true, 10, false, false, true); }
TEST_F(AVIReaderTimelineTest, DeltaCodecIndexlessDroppedFramesDoNotBreakDependencyReplay) { Check(false, 10, true, false, true); }
TEST_F(AVIReaderTimelineTest, AbsoluteIndexOffsetsRetainEmptySamples) {
    AviTimelineBytes bytes; bytes.Make(true, 10, false, false, false, 4, 0, true); AviTimelineFile file; file.Write(bytes.Data, bytes.Size);
    AviTimelineReader owner; ASSERT_EQ(CK_OK, owner.Open(file)); EXPECT_EQ(4, owner.Reader->GetMovieFrameCount()); owner.Expect(3,1); owner.Expect(1,0);
}
TEST_F(AVIReaderTimelineTest, OpenDmlStandardIndexRetainsEmptySampleTimesAndPictures) {
    AviTimelineBytes bytes; bytes.Make(false, 10, false, false, false, 4, 0, false, true);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner;
    ASSERT_EQ(CK_OK, owner.Open(file)); EXPECT_EQ(4, owner.Reader->GetMovieFrameCount()); EXPECT_EQ(160, owner.Reader->GetMovieLength());
    owner.Expect(3,1); owner.Expect(1,0); owner.Expect(2,1); owner.Expect(0,0);
}
TEST_F(AVIReaderTimelineTest, OpenDmlStandardIndexDeltaReplayPreservesEmptySamples) {
    AviTimelineBytes bytes; bytes.Make(false, 10, false, false, true, 4, 0, false, true);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner;
    ASSERT_EQ(CK_OK, owner.Open(file)); EXPECT_EQ(4, owner.Reader->GetMovieFrameCount());
    owner.Expect(3,1,true); owner.Expect(1,0,true); owner.Expect(2,1,true); owner.Expect(0,0,true);
}
TEST_F(AVIReaderTimelineTest, InvalidIndexFallsBackToTheActualMovieChunks) {
    AviTimelineBytes bytes; bytes.Make(true); bytes.Word(bytes.Size-8, 0xfffffff0u); // Damage last index offset.
    // Damage all offsets so the existing parser cannot produce a partial index.
    for (unsigned i = 0; i < 4; ++i) bytes.Word(bytes.Size-64+i*16+8, 0xfffffff0u);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner;
    ASSERT_EQ(CK_OK, owner.Open(file)); EXPECT_EQ(4, owner.Reader->GetMovieFrameCount()); owner.Expect(3,1); owner.Expect(0,0);
}
TEST_F(AVIReaderTimelineTest, MalformedMovieChunkCannotBorrowBytesPastItsList) {
    AviTimelineBytes bytes; bytes.Make(false); bytes.Word(bytes.Offsets[3]+4, 1000);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner;
    EXPECT_NE(CK_OK, owner.Open(file)); EXPECT_EQ(0, owner.Reader->GetMovieFrameCount());
}
TEST_F(AVIReaderTimelineTest, MovieWithoutAnyVideoChunksIsNotOpened) {
    AviTimelineBytes bytes; bytes.Make(false);
    for (unsigned i = 0; i < 4; ++i) memcpy(bytes.Data+bytes.Offsets[i], "JUNK", 4);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner; EXPECT_NE(CK_OK, owner.Open(file));
}
TEST_F(AVIReaderTimelineTest, NonzeroStreamStartDoesNotOffsetLogicalFrameIndicesOrExtendDuration) {
    AviTimelineBytes bytes; bytes.Make(true, 0, false, false, false, 4, 7);
    AviTimelineFile file; file.Write(bytes.Data, bytes.Size); AviTimelineReader owner;
    ASSERT_EQ(CK_OK, owner.Open(file)); EXPECT_EQ(160, owner.Reader->GetMovieLength()); owner.Expect(0,0); owner.Expect(3,1);
}
TEST_F(AVIReaderTimelineTest, BitmapConsumerRetainsDroppedSlotsAndCopiesTheRightPixels) {
    AviTimelineBytes bytes; bytes.Make(false, 10, true); AviTimelineFile file; file.Write(bytes.Data, bytes.Size);
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(4, bitmap.GetSlotCount());
    const unsigned order[] = {3,1,2,0};
    for (unsigned frame : order) {
        ASSERT_TRUE(bitmap.SetCurrentSlot(frame)); EXPECT_EQ(frame < 2 ? 0xff998877u : 0xff090807u, bitmap.GetPixel(0,0));
    }
}
TEST_F(AVIReaderTimelineTest, IndexlessMovieCanBeReopenedAfterARejectedFile) {
    AviTimelineBytes bytes; bytes.Make(false, 2); AviTimelineFile file; file.Write("bad",3); AviTimelineReader owner;
    EXPECT_NE(CK_OK, owner.Open(file)); file.Write(bytes.Data, bytes.Size); ASSERT_EQ(CK_OK, owner.Open(file)); owner.Expect(1,0);
}

