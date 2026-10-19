#include <gtest/gtest.h>
#include <climits>
#include <cstdio>
#include <filesystem>
#include "CKAll.h"
#include "../../Plugins/AVIReader/AVIReader.h"
#include "AVIReaderTestData.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) { ADD_FAILURE() << "Unexpected serialization"; return nullptr; }
CKDataReader *CKGet_AviReader_Reader(int);
CKPluginInfo *CKGet_AviReader_PluginInfo(int);
int CKGet_AviReader_PluginInfoCount();

static void MakeTimingAvi(AviReaderTestData &data, unsigned scale, unsigned rate,
                          unsigned count, bool indexed = true, unsigned start = 0,
                          bool dropped = false) {
    ASSERT_LE(count, 64u);
    data.Start(24, 0);
    // Independent RIFF stream-header fields, not the production timing helper.
    data.Word(data.StreamCountAt - 12, scale);
    data.Word(data.StreamCountAt - 8, rate);
    data.Word(data.StreamCountAt - 4, start);
    for (unsigned i = 0; i < count; ++i) {
        const CKBYTE pixels[] = {CKBYTE(i + 1),34,51,68,85,102,0,0,
                                119,136,153,170,187,204,0,0};
        data.Frame(pixels, dropped && i == 1 ? 0 : sizeof(pixels));
    }
    data.Finish(indexed);
}

struct AviTimingFile {
    std::filesystem::path Path;
    AviTimingFile(const AviReaderTestData &data) {
        static unsigned sequence = 0;
        char name[80];
        std::snprintf(name, sizeof(name), "avi-timing-%p-%u.avi", this, ++sequence);
        Path = std::filesystem::temp_directory_path() / name;
        Write(data.Data, data.Size);
    }
    ~AviTimingFile() { std::error_code ignored; std::filesystem::remove(Path, ignored); }
    void Write(const void *bytes, unsigned size) {
        FILE *file = std::fopen(Path.string().c_str(), "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(size, std::fwrite(bytes, 1, size, file));
        EXPECT_EQ(0, std::fclose(file));
    }
};

struct AviTimingReader {
    CKMovieReader *Reader;
    AviTimingReader() {
        CKGet_AviReader_PluginInfo(0);
        Reader = static_cast<CKMovieReader *>(CKGet_AviReader_Reader(0));
    }
    ~AviTimingReader() { Reader->Release(); }
    CKERROR Open(const AviTimingFile &file) {
        return Reader->OpenFile(const_cast<char *>(file.Path.string().c_str()));
    }
};

class AVIReaderTimingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("AVI rational timing",
            CKGet_AviReader_PluginInfoCount, CKGet_AviReader_PluginInfo, CKGet_AviReader_Reader));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void Pixels(CKMovieReader *reader, unsigned frame, unsigned picture) {
        CKMovieProperties *properties = nullptr;
        ASSERT_EQ(CK_OK, reader->ReadFrame(frame, &properties));
        ASSERT_NE(nullptr, properties);
        ASSERT_NE(nullptr, properties->m_Data);
        const CKBYTE *pixels = static_cast<const CKBYTE *>(properties->m_Data);
        EXPECT_EQ(picture + 1, pixels[0]);
        EXPECT_EQ(34, pixels[1]); EXPECT_EQ(51, pixels[2]); EXPECT_EQ(255, pixels[3]);
    }
    void Check(unsigned scale, unsigned rate, unsigned count, int milliseconds,
               bool indexed = true, unsigned start = 0, bool dropped = false) {
        AviReaderTestData data;
        MakeTimingAvi(data, scale, rate, count, indexed, start, dropped);
        AviTimingFile file(data);
        AviTimingReader reader;
        ASSERT_EQ(CK_OK, reader.Open(file));
        EXPECT_EQ(count, reader.Reader->GetMovieFrameCount());
        EXPECT_EQ(milliseconds, reader.Reader->GetMovieLength());
        Pixels(reader.Reader, count - 1, count - 1);
        Pixels(reader.Reader, 0, 0);
        if (dropped) Pixels(reader.Reader, 1, 0);
        EXPECT_EQ(milliseconds, reader.Reader->GetMovieLength());
    }
};

TEST_F(AVIReaderTimingTest, NtscFilmTwelveFramesRoundsExactHalfUp) { Check(1001,24000,12,501); }
TEST_F(AVIReaderTimingTest, NtscFilmThirtySixFramesRoundsExactHalfUp) { Check(1001,24000,36,1502); }
TEST_F(AVIReaderTimingTest, EquivalentRationalClockRetainsTheSameDuration) { Check(100100,2400000,12,501); }
TEST_F(AVIReaderTimingTest, NtscVideoUsesTheCompleteStreamDuration) { Check(1001,30000,3,100); }
TEST_F(AVIReaderTimingTest, NtscDoubleRateRoundsFractionalMilliseconds) { Check(1001,60000,36,601); }
TEST_F(AVIReaderTimingTest, BelowHalfMillisecondRoundsDown) { Check(1,2001,1,0); }
TEST_F(AVIReaderTimingTest, AboveHalfMillisecondRoundsUp) { Check(1,1999,1,1); }
TEST_F(AVIReaderTimingTest, ExactHalfMillisecondRoundsUp) { Check(1,2000,1,1); }
TEST_F(AVIReaderTimingTest, TwoThirdsOfASecondRoundsOnceAtTheEnd) { Check(1,3,2,667); }
TEST_F(AVIReaderTimingTest, FractionalRoundingCarriesIntoTheNextSecond) { Check(1999,2000,1,1000); }
TEST_F(AVIReaderTimingTest, UnsignedClockFieldsRetainTheirFullRange) { Check(0xffffffffu,0xffffffffu,3,3000); }
TEST_F(AVIReaderTimingTest, HighBitRateIsNotTreatedAsNegative) { Check(0xffffffffu,0x80000000u,1,2000); }
TEST_F(AVIReaderTimingTest, FrameTimesScaleProductUsesMoreThanThirtyTwoBits) { Check(0xffffffffu,0xfffffffbu,36,36000); }
TEST_F(AVIReaderTimingTest, LargestRepresentableDurationIsExact) { Check(2147483647u,1000,1,INT_MAX); }
TEST_F(AVIReaderTimingTest, FractionalCarryAtTheDurationLimitIsExact) { Check(4294967293u,2000,1,INT_MAX); }
TEST_F(AVIReaderTimingTest, LongRepresentableDurationRemainsPositive) { Check(2147483,1,1,2147483000); }
TEST_F(AVIReaderTimingTest, DurationBeyondThePublicIntegerRangeSaturates) { Check(2147483648u,1000,1,INT_MAX); }
TEST_F(AVIReaderTimingTest, HugeSamplePeriodDoesNotWrapToANegativeDuration) { Check(0xffffffffu,1,36,INT_MAX); }
TEST_F(AVIReaderTimingTest, ZeroScaleKeepsTheExistingUnknownDurationPolicy) { Check(0,25,3,0); }
TEST_F(AVIReaderTimingTest, ZeroRateKeepsTheExistingUnknownDurationPolicy) { Check(1,0,3,0); }
TEST_F(AVIReaderTimingTest, EntirelyMissingClockStillAllowsFrameAccess) { Check(0,0,3,0); }
TEST_F(AVIReaderTimingTest, IndexlessFrameCountUsesTheSameRationalClock) { Check(1001,24000,12,501,false); }
TEST_F(AVIReaderTimingTest, DroppedSamplesStillContributeTheirWholePeriod) { Check(1001,24000,12,501,true,0,true); }
TEST_F(AVIReaderTimingTest, StreamStartDoesNotExtendRelativeMovieDuration) { Check(1001,24000,12,501,true,17); }
TEST_F(AVIReaderTimingTest, StreamClockOverridesTheAdvisoryMainHeaderPeriod) {
    AviReaderTestData data; MakeTimingAvi(data,1001,24000,12); data.Word(32,1000000);
    AviTimingFile file(data); AviTimingReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(501,reader.Reader->GetMovieLength());
}
TEST_F(AVIReaderTimingTest, ActualSamplesOverrideInconsistentDeclaredCounts) {
    AviReaderTestData data; MakeTimingAvi(data,1001,24000,12);
    data.Word(data.FrameCountAt,300); data.Word(data.StreamCountAt,900);
    AviTimingFile file(data); AviTimingReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(12,reader.Reader->GetMovieFrameCount());
    EXPECT_EQ(501,reader.Reader->GetMovieLength()); Pixels(reader.Reader,11,11);
}
TEST_F(AVIReaderTimingTest, ReopeningReplacesThePreviousStreamClock) {
    AviReaderTestData data; MakeTimingAvi(data,1,25,3);
    AviTimingFile file(data); AviTimingReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); EXPECT_EQ(120,reader.Reader->GetMovieLength());
    MakeTimingAvi(data,1001,24000,12); AviTimingFile second(data);
    ASSERT_EQ(CK_OK,reader.Open(second)); EXPECT_EQ(501,reader.Reader->GetMovieLength());
    Pixels(reader.Reader,11,11);
}
TEST_F(AVIReaderTimingTest, FailedOpenClearsTheClockBeforeTheNextSuccessfulOpen) {
    AviReaderTestData data; MakeTimingAvi(data,0xffffffffu,1,3);
    AviTimingFile file(data); AviTimingReader reader;
    ASSERT_EQ(CK_OK,reader.Open(file)); AviTimingFile bad(data); bad.Write("bad",3);
    EXPECT_NE(CK_OK,reader.Open(bad)); EXPECT_EQ(0,reader.Reader->GetMovieLength());
    MakeTimingAvi(data,1001,24000,12); AviTimingFile second(data);
    ASSERT_EQ(CK_OK,reader.Open(second)); EXPECT_EQ(501,reader.Reader->GetMovieLength());
}
TEST_F(AVIReaderTimingTest, ClosedAndInvalidStreamDurationQueriesReturnZero) {
    AviReaderTestData data; MakeTimingAvi(data,1001,24000,12);
    AviTimingFile file(data); AviDemuxer demuxer;
    EXPECT_EQ(0,demuxer.GetDurationMs(0));
    ASSERT_TRUE(demuxer.Open(file.Path.string().c_str()));
    EXPECT_EQ(0,demuxer.GetDurationMs(-1)); EXPECT_EQ(0,demuxer.GetDurationMs(1));
    EXPECT_EQ(501,demuxer.GetDurationMs(0)); demuxer.Close();
    EXPECT_EQ(0,demuxer.GetDurationMs(0));
}
TEST_F(AVIReaderTimingTest, BitmapConsumerKeepsFractionalRateSlotsAndPixelIdentity) {
    AviReaderTestData data; MakeTimingAvi(data,1001,24000,12,true,0,true);
    AviTimingFile file(data); CKBitmapData bitmap;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString(file.Path.string().c_str())));
    EXPECT_EQ(12,bitmap.GetSlotCount());
    ASSERT_TRUE(bitmap.SetCurrentSlot(11)); EXPECT_EQ(0xff998877u,bitmap.GetPixel(0,0));
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(0xff998877u,bitmap.GetPixel(0,0));
}
