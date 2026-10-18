#include <gtest/gtest.h>
#include <cstddef>
#include <type_traits>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKBitmapData.cpp"

CKSTRING CKJustFile(CKSTRING) {
    ADD_FAILURE() << "Unexpected movie chunk serialization path";
    return nullptr;
}

static CKPluginInfo MoviePlugin;
static struct MovieProbe {
    int Created, Released, Opens, Reads, FrameCount, OpenError, FailRead;
    int NullPropertiesRead, NullDataRead, Width, Height;
    int ReleaseById[32], Requests[32], Events[96], EventCount;
    bool Unavailable;
    XString OpenPath;
    void Reset() {
        Created = Released = Opens = Reads = OpenError = FailRead = 0;
        NullPropertiesRead = NullDataRead = EventCount = 0;
        FrameCount = 3; Width = Height = 2; Unavailable = false; OpenPath = "";
        memset(ReleaseById, 0, sizeof(ReleaseById)); memset(Requests, 0, sizeof(Requests));
        memset(Events, 0, sizeof(Events));
    }
    void Event(int event) { if (EventCount < 96) Events[EventCount++] = event; }
} Movie;

class BitmapMovieReader : public CKMovieReader {
public:
    explicit BitmapMovieReader(int id) : Id(id), Calls(0) {}
    void Release() override {
        ++Movie.Released; ++Movie.ReleaseById[Id]; Movie.Event(400 + Id); delete this;
    }
    CKPluginInfo *GetReaderInfo() override { return &MoviePlugin; }
    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int) override { return nullptr; }
    int GetMovieFrameCount() override { return Movie.FrameCount; }
    int GetMovieLength() override { return 120; }
    CKERROR OpenFile(CKSTRING path) override {
        ++Movie.Opens; Movie.OpenPath = path; Movie.Event(200 + Id); return Movie.OpenError;
    }
    CKERROR OpenMemory(CKSTRING) override { ADD_FAILURE(); return CKMOVIEERROR_READERROR; }
    CKERROR OpenAsynchronousFile(CKSTRING) override { ADD_FAILURE(); return CKMOVIEERROR_READERROR; }
    CKERROR ReadFrame(int frame, CKMovieProperties **out) override {
        if (Movie.Reads < 32) Movie.Requests[Movie.Reads] = frame;
        ++Movie.Reads; ++Calls; Movie.Event(300 + Id);
        if (Movie.FailRead == Calls) return CKMOVIEERROR_READERROR;
        if (Movie.NullPropertiesRead == Calls) { *out = nullptr; return CK_OK; }
        Properties.m_Ext = "mvq"; Properties.m_ReaderGuid = MoviePlugin.m_GUID;
        Properties.m_Format.Width = Movie.Width; Properties.m_Format.Height = Movie.Height;
        Properties.m_Format.BitsPerPixel = 32; Properties.m_Format.BytesPerLine = 12;
        Properties.m_Format.RedMask = R_MASK; Properties.m_Format.GreenMask = G_MASK;
        Properties.m_Format.BlueMask = B_MASK; Properties.m_Format.AlphaMask = A_MASK;
        Properties.m_Data = Movie.NullDataRead == Calls ? nullptr : frame ? Later : First;
        *out = &Properties; return CK_OK;
    }
private:
    int Id, Calls;
    CKMovieProperties Properties;
    CKDWORD First[6] = {0x10203040,0x50607080,0xdeadbeef,0x90a0b0c0,0xd0e0f010,0xcafebabe};
    CKDWORD Later[6] = {0x11223344,0x55667788,0xdeadbeef,0x99aabbcc,0xddeeff00,0xcafebabe};
};

static int MoviePluginCount() { return 1; }
static CKPluginInfo *MoviePluginInfo(int) { return &MoviePlugin; }
static CKDataReader *MoviePluginCreate(int) {
    if (Movie.Unavailable) return nullptr;
    const int id = Movie.Created++; Movie.Event(100 + id); return new BitmapMovieReader(id);
}

static CKMovieInfo *OwnedMovieInfo(const char *name) {
    CKMovieInfo *info = new CKMovieInfo(XString(name));
    info->m_MovieReader = static_cast<CKMovieReader *>(MoviePluginCreate(0));
    return info;
}

class CKBitmapDataMovieTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); MoviePlugin.m_GUID = CKGUID(0x18610001, 1);
        MoviePlugin.m_Extension = "mvq"; MoviePlugin.m_Type = CKPLUGIN_MOVIE_READER;
        ASSERT_EQ(CK_OK, CKGetPluginManager()->RegisterStaticPlugin("Bitmap movie fixture", MoviePluginCount, MoviePluginInfo, MoviePluginCreate));
    }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override { Movie.Reset(); }
    void ExpectFirst(CKBitmapData &bitmap) {
        ASSERT_EQ(2, bitmap.GetWidth()); ASSERT_EQ(2, bitmap.GetHeight());
        const CKDWORD expected[] = {0x90a0b0c0,0xd0e0f010,0x10203040,0x50607080};
        for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
            EXPECT_EQ(expected[x + 2 * y], bitmap.GetPixel(x, y, 0));
    }
};

TEST_F(CKBitmapDataMovieTest, MovieInfoCopiesFilenameAndStartsWithNoReaderOrDecodedFrame) {
    XString filename("folder/original.mvq"); CKMovieInfo info(filename); filename = "changed.mvq";
    EXPECT_STREQ("folder/original.mvq", info.m_MovieFileName.CStr());
    EXPECT_EQ(nullptr, info.m_MovieReader); EXPECT_EQ(-1, info.m_MovieCurrentSlot);
    EXPECT_EQ(0, Movie.Opens); EXPECT_EQ(0, Movie.Reads);
}

TEST_F(CKBitmapDataMovieTest, MovieInfoDestructorReleasesItsReaderExactlyOnce) {
    { CKMovieInfo *info = OwnedMovieInfo("owner.mvq"); ASSERT_NE(nullptr, info->m_MovieReader); delete info; }
    EXPECT_EQ(1, Movie.Created); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(1, Movie.ReleaseById[0]);
}

TEST_F(CKBitmapDataMovieTest, MovieInfoLayoutMatchesWin32OffsetsAndPortablePointerSize) {
    ASSERT_TRUE(std::is_standard_layout<CKMovieInfo>::value);
    EXPECT_EQ(0u, offsetof(CKMovieInfo, m_MovieFileName));
    if (sizeof(void *) == 4) {
        EXPECT_EQ(16u, sizeof(CKMovieInfo)); EXPECT_EQ(4u, alignof(CKMovieInfo));
        EXPECT_EQ(8u, offsetof(CKMovieInfo, m_MovieReader)); EXPECT_EQ(12u, offsetof(CKMovieInfo, m_MovieCurrentSlot));
    } else {
        EXPECT_EQ(32u, sizeof(CKMovieInfo)); EXPECT_EQ(8u, alignof(CKMovieInfo));
        EXPECT_EQ(16u, offsetof(CKMovieInfo, m_MovieReader)); EXPECT_EQ(24u, offsetof(CKMovieInfo, m_MovieCurrentSlot));
    }
}

TEST_F(CKBitmapDataMovieTest, CreateInfoOpensCompletePathAndProbesFrameZeroWithoutAttachingOrSelectingIt) {
    CKBitmapData bitmap; CKMovieProperties *props = nullptr;
    CKMovieInfo *info = bitmap.CreateMovieInfo(XString("folder/source.MVQ"), &props);
    ASSERT_NE(nullptr, info); ASSERT_NE(nullptr, props); ASSERT_NE(nullptr, props->m_Data);
    EXPECT_EQ(nullptr, bitmap.m_MovieInfo); EXPECT_EQ(0, bitmap.m_Slots.Size());
    EXPECT_EQ(-1, info->m_MovieCurrentSlot); EXPECT_EQ(1, Movie.Opens); EXPECT_EQ(1, Movie.Reads);
    EXPECT_EQ(0, Movie.Requests[0]); EXPECT_STREQ("folder/source.MVQ", Movie.OpenPath.CStr());
    EXPECT_STREQ("folder/source.MVQ", info->m_MovieFileName.CStr());
    EXPECT_EQ(0, Movie.Released); delete info; EXPECT_EQ(1, Movie.Released);
}

TEST_F(CKBitmapDataMovieTest, CreateInfoDoesNotReplaceAnAlreadyAttachedMovie) {
    CKBitmapData bitmap; bitmap.SetMovieInfo(OwnedMovieInfo("existing.mvq")); CKMovieInfo *old = bitmap.m_MovieInfo;
    CKMovieProperties *props = nullptr; CKMovieInfo *separate = bitmap.CreateMovieInfo(XString("separate.mvq"), &props);
    ASSERT_NE(nullptr, separate); EXPECT_EQ(old, bitmap.m_MovieInfo); EXPECT_EQ(0, Movie.Released);
    delete separate; EXPECT_EQ(1, Movie.Released); EXPECT_EQ(0, Movie.ReleaseById[0]);
    EXPECT_STREQ("existing.mvq", bitmap.GetMovieFileName());
}

TEST_F(CKBitmapDataMovieTest, CreateInfoWithoutReaderLeavesBitmapStorageUntouched) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); CKBYTE *surface = bitmap.LockSurfacePtr();
    CKMovieProperties *props = nullptr; EXPECT_EQ(nullptr, bitmap.CreateMovieInfo(XString("source.zzz"), &props));
    EXPECT_EQ(nullptr, props); EXPECT_EQ(0, Movie.Created); EXPECT_EQ(surface, bitmap.LockSurfacePtr());
    Movie.Unavailable = true; EXPECT_EQ(nullptr, bitmap.CreateMovieInfo(XString("source.mvq"), &props));
    EXPECT_EQ(0, Movie.Created); EXPECT_EQ(0, Movie.Opens); EXPECT_EQ(0, Movie.Reads);
}

TEST_F(CKBitmapDataMovieTest, OpenFailureSkipsFrameProbeAndReleasesTheReaderOnce) {
    CKBitmapData bitmap; CKMovieProperties *props = nullptr; Movie.OpenError = CKMOVIEERROR_READERROR;
    EXPECT_EQ(nullptr, bitmap.CreateMovieInfo(XString("source.mvq"), &props));
    EXPECT_EQ(1, Movie.Opens); EXPECT_EQ(0, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(1, Movie.ReleaseById[0]);
}

TEST_F(CKBitmapDataMovieTest, FirstFrameProbeFailureReleasesReaderWithoutLeavingAnAttachedMovie) {
    CKBitmapData bitmap; CKMovieProperties *props = nullptr; Movie.FailRead = 1;
    EXPECT_EQ(nullptr, bitmap.CreateMovieInfo(XString("source.mvq"), &props));
    EXPECT_EQ(1, Movie.Opens); EXPECT_EQ(1, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
}

TEST_F(CKBitmapDataMovieTest, LoadMoviePerformsTheNativeTwoZeroFrameRequestsAndBlitsActualPixels) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString("folder/source.mvq")));
    EXPECT_EQ(1, Movie.Opens); EXPECT_EQ(2, Movie.Reads); EXPECT_EQ(0, Movie.Requests[0]); EXPECT_EQ(0, Movie.Requests[1]);
    EXPECT_EQ(0, Movie.Released); EXPECT_EQ(0, bitmap.GetCurrentSlot()); EXPECT_EQ(3, bitmap.GetSlotCount());
    EXPECT_EQ(1, bitmap.m_Slots.Size()); EXPECT_EQ(4, bitmap.ToRestore());
    EXPECT_STREQ("folder/source.mvq", bitmap.GetMovieFileName()); EXPECT_NE(nullptr, bitmap.GetMovieReader()); ExpectFirst(bitmap);
}

TEST_F(CKBitmapDataMovieTest, LoadMovieFrameSelectionUsesReaderOwnedDataAndPreservesIndependentSurface) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString("source.mvq"))); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    ASSERT_TRUE(bitmap.SetCurrentSlot(1)); EXPECT_EQ(3, Movie.Reads); EXPECT_EQ(1, Movie.Requests[2]);
    const CKDWORD expected[] = {0x99aabbcc,0xddeeff00,0x11223344,0x55667788};
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) EXPECT_EQ(expected[x + 2 * y], bitmap.GetPixel(x, y, 0));
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); EXPECT_EQ(1, bitmap.GetCurrentSlot());
}

TEST_F(CKBitmapDataMovieTest, LoadMovieCanReplaceTheDimensionsOfAnExistingSingleImage) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(3, 4));
    ASSERT_TRUE(bitmap.SetSlotFileName(0, "old.bmp")); ASSERT_TRUE(bitmap.LoadMovieFile(XString("source.mvq")));
    EXPECT_STREQ("", bitmap.GetSlotFileName(0)); ExpectFirst(bitmap);
}

TEST_F(CKBitmapDataMovieTest, FailedMovieReplacementReleasesOldReaderBeforeOpeningTheNewOne) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString("old.mvq"))); ExpectFirst(bitmap);
    CKBYTE *surface = bitmap.LockSurfacePtr(0); const int start = Movie.EventCount;
    Movie.OpenError = CKMOVIEERROR_READERROR; EXPECT_FALSE(bitmap.LoadMovieFile(XString("new.mvq")));
    ASSERT_EQ(start + 4, Movie.EventCount);
    EXPECT_EQ(400, Movie.Events[start]); EXPECT_EQ(101, Movie.Events[start + 1]);
    EXPECT_EQ(201, Movie.Events[start + 2]); EXPECT_EQ(401, Movie.Events[start + 3]);
    EXPECT_EQ(nullptr, bitmap.GetMovieReader()); EXPECT_EQ(nullptr, bitmap.GetMovieFileName());
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); ExpectFirst(bitmap); EXPECT_EQ(2, Movie.Released);
}

TEST_F(CKBitmapDataMovieTest, SuccessfulMovieReplacementReleasesOnlyOldReaderUntilFinalDestruction) {
    { CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString("old.mvq")));
      ASSERT_TRUE(bitmap.LoadMovieFile(XString("new.mvq")));
      EXPECT_EQ(2, Movie.Created); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(1, Movie.ReleaseById[0]);
      EXPECT_EQ(0, Movie.ReleaseById[1]); EXPECT_STREQ("new.mvq", bitmap.GetMovieFileName()); ExpectFirst(bitmap); }
    EXPECT_EQ(2, Movie.Released); EXPECT_EQ(1, Movie.ReleaseById[1]);
}

TEST_F(CKBitmapDataMovieTest, LoadMovieWithoutReaderKeepsExistingCpuPixelsButClearsPreviousMovie) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.LoadMovieFile(XString("old.mvq"))); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    EXPECT_FALSE(bitmap.LoadMovieFile(XString("missing.zzz"))); EXPECT_EQ(1, Movie.Released);
    EXPECT_EQ(nullptr, bitmap.m_MovieInfo); EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); ExpectFirst(bitmap);
}

TEST_F(CKBitmapDataMovieTest, LoadMovieOpenOrFirstProbeFailurePropagatesFalseWithoutAllocatingSlots) {
    CKBitmapData bitmap; Movie.OpenError = CKMOVIEERROR_READERROR; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    Movie.OpenError = 0; Movie.FailRead = 1; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(2, Movie.Released); EXPECT_EQ(0, bitmap.m_Slots.Size()); EXPECT_EQ(nullptr, bitmap.m_MovieInfo);
}

TEST_F(CKBitmapDataMovieTest, MissingFirstProbePropertiesFailsAndClosesTheReader) {
    CKBitmapData bitmap; Movie.NullPropertiesRead = 1; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(1, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
    EXPECT_EQ(0, bitmap.m_Slots.Size());
}

TEST_F(CKBitmapDataMovieTest, ProbeMayReturnOnlyGeometryIfTheActualSecondReadProvidesPixels) {
    CKBitmapData bitmap; Movie.NullDataRead = 1;
    ASSERT_TRUE(bitmap.LoadMovieFile(XString("source.mvq"))); EXPECT_EQ(2, Movie.Reads); ExpectFirst(bitmap);
}

TEST_F(CKBitmapDataMovieTest, InvalidProbedDimensionsFailBeforeTheSecondFrameRead) {
    CKBitmapData bitmap; Movie.Width = 0; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(1, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(nullptr, bitmap.m_MovieInfo);
    EXPECT_EQ(0, bitmap.m_Slots.Size());
}

TEST_F(CKBitmapDataMovieTest, ExistingMultipleImagesRejectMismatchedMovieGeometryWithoutFalseSuccess) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); ASSERT_TRUE(bitmap.CreateImage(1, 1, 32, 1));
    ASSERT_TRUE(bitmap.SetPixel(0, 0, 0xff123456, 0)); CKBYTE *surface = bitmap.LockSurfacePtr(0);
    EXPECT_FALSE(bitmap.LoadMovieFile(XString("different.mvq"))); EXPECT_EQ(1, Movie.Reads); EXPECT_EQ(1, Movie.Released);
    EXPECT_EQ(nullptr, bitmap.m_MovieInfo); EXPECT_EQ(2, bitmap.m_Slots.Size()); EXPECT_EQ(1, bitmap.GetWidth());
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); EXPECT_EQ(0xff123456u, bitmap.GetPixel(0, 0, 0));
}

TEST_F(CKBitmapDataMovieTest, SecondFrameReadFailureReturnsFalseAndRetainsTheCreatedBlackSurface) {
    CKBitmapData bitmap; Movie.FailRead = 2; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(2, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
    EXPECT_EQ(1, bitmap.GetSlotCount()); EXPECT_EQ(0xff000000u, bitmap.GetPixel(0, 0, 0));
    EXPECT_EQ(2, bitmap.GetWidth()); EXPECT_EQ(2, bitmap.GetHeight()); // No transactional image rollback is promised.
}

TEST_F(CKBitmapDataMovieTest, NullSecondReadPropertiesOrPixelsFailAndReleaseEachReader) {
    CKBitmapData bitmap; Movie.NullPropertiesRead = 2; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    Movie.NullPropertiesRead = 0; Movie.NullDataRead = 2; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(2, Movie.Released); EXPECT_EQ(4, Movie.Reads); EXPECT_EQ(nullptr, bitmap.m_MovieInfo);
}

TEST_F(CKBitmapDataMovieTest, ZeroReportedFramesFailBeforeAttemptingActualFrameSelection) {
    CKBitmapData bitmap; Movie.FrameCount = 0; EXPECT_FALSE(bitmap.LoadMovieFile(XString("bad.mvq")));
    EXPECT_EQ(1, Movie.Reads); EXPECT_EQ(1, Movie.Released); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
}

TEST_F(CKBitmapDataMovieTest, SetMovieInfoTransfersOwnershipWithoutOpeningReadingOrChangingStorage) {
    CKBitmapData bitmap; ASSERT_TRUE(bitmap.CreateImage(1, 1)); CKBYTE *surface = bitmap.LockSurfacePtr();
    bitmap.m_BitmapFlags = CKBITMAPDATA_TRANSPARENT; CKMovieInfo *info = OwnedMovieInfo("transfer.mvq");
    info->m_MovieCurrentSlot = 2; CKMovieReader *reader = info->m_MovieReader; bitmap.SetMovieInfo(info);
    EXPECT_EQ(info, bitmap.m_MovieInfo); EXPECT_EQ(reader, bitmap.GetMovieReader()); EXPECT_EQ(2, bitmap.GetCurrentSlot());
    EXPECT_STREQ("transfer.mvq", bitmap.GetMovieFileName()); EXPECT_EQ(0, Movie.Opens); EXPECT_EQ(0, Movie.Reads);
    EXPECT_EQ(surface, bitmap.LockSurfacePtr(0)); EXPECT_EQ(CKBITMAPDATA_TRANSPARENT, bitmap.m_BitmapFlags);
}

TEST_F(CKBitmapDataMovieTest, SetMovieInfoReplacementAndRepeatedClearReleaseEachOwnerOnce) {
    CKBitmapData bitmap; bitmap.SetMovieInfo(OwnedMovieInfo("first.mvq"));
    bitmap.SetMovieInfo(OwnedMovieInfo("second.mvq")); EXPECT_EQ(1, Movie.Released);
    EXPECT_STREQ("second.mvq", bitmap.GetMovieFileName()); bitmap.SetMovieInfo(nullptr);
    EXPECT_EQ(2, Movie.Released); EXPECT_EQ(nullptr, bitmap.GetMovieReader()); EXPECT_EQ(nullptr, bitmap.GetMovieFileName());
    bitmap.SetMovieInfo(nullptr); EXPECT_EQ(2, Movie.Released);
}

TEST_F(CKBitmapDataMovieTest, SettingTheAlreadyOwnedMovieInfoDoesNotReleaseItsReader) {
    { CKBitmapData bitmap; CKMovieInfo *info = OwnedMovieInfo("same.mvq"); bitmap.SetMovieInfo(info);
      bitmap.SetMovieInfo(info); EXPECT_EQ(0, Movie.Released);
      // Preserve the observed failure while preventing a dangling owner from breaking teardown.
      if (Movie.Released) { bitmap.m_MovieInfo = nullptr; return; }
      EXPECT_EQ(info, bitmap.m_MovieInfo); EXPECT_STREQ("same.mvq", bitmap.GetMovieFileName());
      EXPECT_NE(nullptr, bitmap.GetMovieReader()); }
    EXPECT_EQ(1, Movie.Released);
}

TEST_F(CKBitmapDataMovieTest, GettersDistinguishNoMovieFromAnEmptyMovieNameAndMissingReader) {
    CKBitmapData bitmap; EXPECT_EQ(nullptr, bitmap.GetMovieFileName()); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
    bitmap.SetMovieInfo(new CKMovieInfo(XString("")));
    EXPECT_STREQ("", bitmap.GetMovieFileName()); EXPECT_EQ(nullptr, bitmap.GetMovieReader());
    EXPECT_EQ(-1, bitmap.GetCurrentSlot()); EXPECT_EQ(0, bitmap.GetSlotCount());
}

TEST_F(CKBitmapDataMovieTest, SeparateBitmapsHaveIndependentReaderOwnershipAndSurfaceData) {
    CKBitmapData first;
    { CKBitmapData second; ASSERT_TRUE(first.LoadMovieFile(XString("first.mvq")));
      ASSERT_TRUE(second.LoadMovieFile(XString("second.mvq")));
      EXPECT_NE(first.GetMovieReader(), second.GetMovieReader()); EXPECT_NE(first.LockSurfacePtr(0), second.LockSurfacePtr(0));
      ASSERT_TRUE(second.SetCurrentSlot(1)); ExpectFirst(first); EXPECT_EQ(0, Movie.Released); }
    EXPECT_EQ(1, Movie.Released); EXPECT_EQ(0, Movie.ReleaseById[0]); ExpectFirst(first);
}
