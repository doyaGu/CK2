#include <gtest/gtest.h>
#include <cmath>
#define CK_LIB
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

class CurveSerializationProbe : public CK2dCurve {
public:
    float Fit() const { return FittingCoef; }
    void SetFit(float fit) { FittingCoef = fit; }
    void SetRecord(int index, const CK2dCurvePoint &point) { m_ControlPoints[index] = point; }
};

class CurveWirePoint : public CK2dCurvePoint {
public:
    CurveWirePoint(CK2dCurve *owner, float x, float y, CKDWORD flags = 3) {
        m_Curve = owner; m_Position.Set(x, y); m_Flags = flags;
        m_Tension = .125f; m_Continuity = -.25f; m_Bias = .5f;
        m_InTang.Set(.5f, .25f); m_OutTang.Set(.75f, -.375f);
        m_Length = 1234.0f; m_RCurvePos.Set(9, 8);
    }
    explicit CurveWirePoint(const CK2dCurvePoint &point) : CK2dCurvePoint(point) {}
    CKDWORD Flags() const { return m_Flags; }
};

class CurveChunkOwner {
public:
    explicit CurveChunkOwner(CKStateChunk *chunk) : m_Chunk(chunk) {}
    ~CurveChunkOwner() { DeleteCKStateChunk(m_Chunk); }
    CKStateChunk *Get() { return m_Chunk; }
private:
    CKStateChunk *m_Chunk;
};

static void WriteCurveHeader(CKStateChunk &chunk, int count, float fit, short version = 10) {
    chunk.StartWrite(); chunk.SetDataVersion(version); chunk.WriteIdentifier(0x100);
    chunk.StartObjectIDSequence(count);
    for (int i = 0; i < count; ++i) chunk.WriteObjectIDSequence(0);
    chunk.WriteFloat(fit);
}

static void WriteCurvePoint(CKStateChunk &chunk, float x, float y, CKDWORD flags = 3) {
    chunk.WriteDword(flags | 0x10000000);
    const float values[] = {x, y, .125f, -.25f, .5f, 777, -888, .5f, .25f, .75f, -.375f};
    for (float value : values) chunk.WriteFloat(value);
}

static void ExpectCurvePoint(CK2dCurve &curve, int index, float x, float y) {
    CK2dCurvePoint *point = curve.GetControlPoint(index);
    EXPECT_EQ(&curve, point->GetCurve());
    EXPECT_FLOAT_EQ(x, point->GetPosition().x); EXPECT_FLOAT_EQ(y, point->GetPosition().y);
}

static void WriteLegacyPoint(CKStateChunk &point, float x, float y, float tension) {
    point.StartWrite(); point.SetDataVersion(0);
    point.WriteIdentifier(0x4000); point.WriteObjectID(0);
    point.WriteInt(1); point.WriteInt(1);
    point.WriteFloat(x); point.WriteFloat(y);
    point.WriteIdentifier(0x800); point.WriteFloat(tension); point.WriteFloat(-.25f); point.WriteFloat(.5f);
    point.WriteIdentifier(0x1000);
    point.WriteFloat(.5f); point.WriteFloat(.25f);
    point.WriteFloat(.75f); point.WriteFloat(-.375f);
    point.CloseChunk();
}

static void WriteLegacyCurve(CKStateChunk &chunk, CKStateChunk **points, int count, short version = 0) {
    chunk.StartWrite(); chunk.SetDataVersion(version);
    chunk.WriteIdentifier(0x40); chunk.WriteFloat(.375f);
    chunk.WriteIdentifier(0x3f800); chunk.WriteInt(count);
    for (int i = 0; i < count; ++i) { chunk.WriteObjectID(0); chunk.WriteSubChunk(points[i]); }
    chunk.CloseChunk();
}

TEST(CK2dCurveSerializationTest, DumpWritesNativeHeaderAndZeroObjectSlots) {
    CK2dCurve curve; CurveChunkOwner owner(curve.Dump()); CKStateChunk *chunk = owner.Get();
    ASSERT_NE(nullptr, chunk); EXPECT_EQ(CKCHNK_2DCURVE, chunk->GetChunkClassID());
    EXPECT_EQ(10, chunk->GetDataVersion()); chunk->StartRead(); ASSERT_TRUE(chunk->SeekIdentifier(0x100));
    const XObjectArray &objects = chunk->ReadXObjectArray(); ASSERT_EQ(2, objects.Size());
    EXPECT_EQ(0u, objects[0]); EXPECT_EQ(0u, objects[1]); EXPECT_FLOAT_EQ(0, chunk->ReadFloat());
}

TEST(CK2dCurveSerializationTest, DumpWritesEveryFieldInNativeOrderAndZeroReservedSlots) {
    CurveSerializationProbe curve; curve.SetFit(.375f);
    curve.SetRecord(0, CurveWirePoint(&curve, .25f, .75f, 0x83));
    curve.SetRecord(1, CurveWirePoint(&curve, 1, 0, 3));
    CurveChunkOwner owner(curve.Dump()); CKStateChunk *chunk = owner.Get();
    chunk->StartRead(); ASSERT_TRUE(chunk->SeekIdentifier(0x100)); chunk->ReadXObjectArray();
    EXPECT_FLOAT_EQ(.375f, chunk->ReadFloat()); EXPECT_EQ(0x10000083u, chunk->ReadDword());
    const float expected[] = {.25f,.75f,.125f,-.25f,.5f,0,0,.5f,.25f,.75f,-.375f};
    for (float value : expected) EXPECT_FLOAT_EQ(value, chunk->ReadFloat());
    EXPECT_EQ(0x10000003u, chunk->ReadDword()); EXPECT_FLOAT_EQ(1, chunk->ReadFloat());
    EXPECT_FLOAT_EQ(0, chunk->ReadFloat());
}

TEST(CK2dCurveSerializationTest, DumpDoesNotUpdateOrMutateLivePointFlagsAndCaches) {
    CurveSerializationProbe curve; curve.SetRecord(0, CurveWirePoint(&curve, .8f, .6f, 0x83));
    CurveChunkOwner owner(curve.Dump());
    ExpectCurvePoint(curve, 0, .8f, .6f); EXPECT_EQ(0x83u, CurveWirePoint(*curve.GetControlPoint(0)).Flags());
    EXPECT_FLOAT_EQ(1234, curve.GetControlPoint(0)->GetLength());
}

TEST(CK2dCurveSerializationTest, EmptyDumpStillContainsCountAndFittingCoefficient) {
    CurveSerializationProbe curve; curve.SetFit(.25f);
    while (curve.GetControlPointCount()) curve.DeleteControlPoint(curve.GetControlPoint(0));
    CurveChunkOwner owner(curve.Dump()); CKStateChunk *chunk = owner.Get();
    chunk->StartRead(); ASSERT_TRUE(chunk->SeekIdentifier(0x100));
    EXPECT_EQ(0, chunk->ReadXObjectArray().Size()); EXPECT_FLOAT_EQ(.25f, chunk->ReadFloat());
}

TEST(CK2dCurveSerializationTest, SinglePointDumpContainsAnInlineRecord) {
    CK2dCurve curve; curve.DeleteControlPoint(curve.GetControlPoint(1));
    CurveChunkOwner owner(curve.Dump()); CKStateChunk *chunk = owner.Get();
    chunk->StartRead(); ASSERT_TRUE(chunk->SeekIdentifier(0x100));
    EXPECT_EQ(1, chunk->ReadXObjectArray().Size()); chunk->ReadFloat();
    EXPECT_EQ(0x10000000u, chunk->ReadDword()); EXPECT_FLOAT_EQ(0, chunk->ReadFloat());
}

TEST(CK2dCurveSerializationTest, DumpsAreIndependentOwnedChunks) {
    CK2dCurve curve; CurveChunkOwner first(curve.Dump()), second(curve.Dump());
    ASSERT_NE(first.Get(), second.Get()); first.Get()->Clear();
    second.Get()->StartRead(); ASSERT_TRUE(second.Get()->SeekIdentifier(0x100));
    EXPECT_EQ(2, second.Get()->ReadXObjectArray().Size());
}

TEST(CK2dCurveSerializationTest, ReadConsumesManualInlineRecordsAndSortsWholePoints) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 2, 0);
    WriteCurvePoint(chunk, 1, .5f, 0x83); WriteCurvePoint(chunk, 0, 0); chunk.CloseChunk();
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, 0, 0); ExpectCurvePoint(curve, 1, 1, .5f);
    EXPECT_EQ(0x83u, CurveWirePoint(*curve.GetControlPoint(1)).Flags());
    EXPECT_FLOAT_EQ(.125f, curve.GetControlPoint(1)->GetTension());
    EXPECT_FLOAT_EQ(-.25f, curve.GetControlPoint(1)->GetContinuity());
    EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(1)->GetBias());
    EXPECT_FLOAT_EQ(1, curve.GetControlPoint(0)->GetOutTangent().x);
    EXPECT_FLOAT_EQ(-.5f, curve.GetControlPoint(0)->GetOutTangent().y);
    EXPECT_FLOAT_EQ(1, curve.GetControlPoint(1)->GetInTangent().x);
    EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(1)->GetInTangent().y);
}

TEST(CK2dCurveSerializationTest, ReadDiscardsReservedLengthTimeAndRecomputesPolylineCaches) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 3, 0);
    WriteCurvePoint(chunk, 0, 0); WriteCurvePoint(chunk, .25f, .5f); WriteCurvePoint(chunk, 1, 0); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk));
    EXPECT_NEAR(1.460404813f, curve.GetLength(), .000003f);
    EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
    EXPECT_NEAR(.559016994f, curve.GetControlPoint(1)->GetLength(), .000003f);
}

TEST(CK2dCurveSerializationTest, AllNonInlineRecordsRemoveOldPoints) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 2, .375f);
    chunk.WriteDword(3); chunk.WriteDword(0); chunk.CloseChunk();
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk));
    EXPECT_EQ(0, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(.375f, curve.Fit());
}

TEST(CK2dCurveSerializationTest, TrailingNonInlineRecordDoesNotCreateAPhantomPoint) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 3, 0);
    WriteCurvePoint(chunk, 0, 0); WriteCurvePoint(chunk, 1, .5f); chunk.WriteDword(2); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 1, 1, .5f);
}

TEST(CK2dCurveSerializationTest, LeadingNonInlineRecordKeepsTheLaterInlineData) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 2, 0);
    chunk.WriteDword(3); WriteCurvePoint(chunk, .75f, .25f); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(1, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, .75f, .25f); EXPECT_FLOAT_EQ(0, curve.GetLength());
}

TEST(CK2dCurveSerializationTest, InterleavedNonInlineRecordsCompactWithoutLosingLaterPoints) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 5, 0);
    chunk.WriteDword(0); WriteCurvePoint(chunk, 1, .5f); chunk.WriteDword(1);
    WriteCurvePoint(chunk, 0, 0); chunk.WriteDword(2); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, 0, 0); ExpectCurvePoint(curve, 1, 1, .5f);
    EXPECT_NEAR(sqrtf(1.25f), curve.GetLength(), .000003f);
}

TEST(CK2dCurveSerializationTest, ZeroCountClearsAnExistingCurve) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 0, .625f); chunk.CloseChunk();
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk));
    EXPECT_EQ(0, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(.625f, curve.Fit());
}

TEST(CK2dCurveSerializationTest, SingleInlineRecordHasOwnerAndZeroLength) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 1, 0);
    WriteCurvePoint(chunk, .75f, .25f); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(1, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, .75f, .25f); EXPECT_FLOAT_EQ(0, curve.GetLength());
    EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
}

TEST(CK2dCurveSerializationTest, ObjectArrayValuesOnlyDetermineTheRecordCount) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); chunk.StartWrite(); chunk.SetDataVersion(10); chunk.WriteIdentifier(0x100);
    chunk.StartObjectIDSequence(2); chunk.WriteObjectIDSequence(123); chunk.WriteObjectIDSequence(456);
    chunk.WriteFloat(0); WriteCurvePoint(chunk, 0, .5f); WriteCurvePoint(chunk, 1, 0); chunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, 0, .5f); ExpectCurvePoint(curve, 1, 1, 0);
}

TEST(CK2dCurveSerializationTest, RepeatedReadReplacesPointCountAndParameters) {
    CKStateChunk first(CKCHNK_2DCURVE, nullptr), second(CKCHNK_2DCURVE, nullptr);
    WriteCurveHeader(first, 3, .375f); WriteCurvePoint(first, 0, 0); WriteCurvePoint(first, .5f, .5f); WriteCurvePoint(first, 1, 1); first.CloseChunk();
    WriteCurveHeader(second, 1, .625f); WriteCurvePoint(second, .25f, .75f); second.CloseChunk();
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&first)); ASSERT_EQ(3, curve.GetControlPointCount());
    ASSERT_EQ(CK_OK, curve.Read(&second)); ASSERT_EQ(1, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, .25f, .75f); EXPECT_FLOAT_EQ(.625f, curve.Fit()); EXPECT_FLOAT_EQ(0, curve.GetLength());
}

TEST(CK2dCurveSerializationTest, MissingIdentifiersPreserveStateAndRefreshCaches) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); chunk.StartWrite(); chunk.SetDataVersion(10);
    chunk.WriteIdentifier(0x20); chunk.WriteInt(55); chunk.CloseChunk();
    CurveSerializationProbe curve; curve.SetFit(.375f);
    curve.SetRecord(0, CurveWirePoint(&curve, 0, 0)); curve.SetRecord(1, CurveWirePoint(&curve, 1, 0));
    ASSERT_EQ(CK_OK, curve.Read(&chunk)); EXPECT_EQ(2, curve.GetControlPointCount());
    EXPECT_FLOAT_EQ(.375f, curve.Fit()); EXPECT_FLOAT_EQ(1, curve.GetLength());
}

TEST(CK2dCurveSerializationTest, NullChunkReturnsNativeErrorWithoutChangingTheCurve) {
    CK2dCurve curve; EXPECT_EQ(CKERR_INVALIDPARAMETER, curve.Read(nullptr));
    EXPECT_EQ(2, curve.GetControlPointCount()); EXPECT_NEAR(sqrtf(2.0f), curve.GetLength(), .000003f);
}

TEST(CK2dCurveSerializationTest, LegacyUnsortedSubchunksKeepPositionAndTcbTogether) {
    CKStateChunk a(0, nullptr), b(0, nullptr), c(0, nullptr), chunk(CKCHNK_2DCURVE, nullptr);
    WriteLegacyPoint(a, .75f, .25f, .75f); WriteLegacyPoint(b, 0, 0, .125f); WriteLegacyPoint(c, 1, .5f, .5f);
    CKStateChunk *points[] = {&a, &b, &c}; WriteLegacyCurve(chunk, points, 3);
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(3, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, 0, 0); ExpectCurvePoint(curve, 1, .75f, .25f); ExpectCurvePoint(curve, 2, 1, .5f);
    EXPECT_FLOAT_EQ(.125f, curve.GetControlPoint(0)->GetTension());
    EXPECT_FLOAT_EQ(.75f, curve.GetControlPoint(1)->GetTension()); EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(2)->GetTension());
    EXPECT_FLOAT_EQ(.375f, curve.Fit());
}

TEST(CK2dCurveSerializationTest, LegacyNullSubchunkCreatesANeutralPointInsteadOfReusingOldData) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); CKStateChunk *points[] = {nullptr}; WriteLegacyCurve(chunk, points, 1);
    CurveSerializationProbe curve; curve.SetRecord(0, CurveWirePoint(&curve, .75f, .5f));
    ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(1, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, 0, 0); EXPECT_TRUE(curve.GetControlPoint(0)->IsTCB());
    EXPECT_FALSE(curve.GetControlPoint(0)->IsLinear()); EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetTension());
}

TEST(CK2dCurveSerializationTest, LegacyZeroCountClearsPointsAndReadsFit) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteLegacyCurve(chunk, nullptr, 0);
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk));
    EXPECT_EQ(0, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(.375f, curve.Fit());
}

TEST(CK2dCurveSerializationTest, VersionOneIgnoresObsoleteSections) {
    CKStateChunk a(0, nullptr), chunk(CKCHNK_2DCURVE, nullptr); WriteLegacyPoint(a, .75f, .25f, .5f);
    CKStateChunk *points[] = {&a}; WriteLegacyCurve(chunk, points, 1, 1);
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    EXPECT_FLOAT_EQ(0, curve.Fit()); ExpectCurvePoint(curve, 0, 0, 0); ExpectCurvePoint(curve, 1, 1, 1);
}

TEST(CK2dCurveSerializationTest, ModernSectionOverridesLegacySectionsInVersionZero) {
    CKStateChunk point(0, nullptr), chunk(CKCHNK_2DCURVE, nullptr); WriteLegacyPoint(point, .5f, .5f, .125f);
    chunk.StartWrite(); chunk.SetDataVersion(0); chunk.WriteIdentifier(0x40); chunk.WriteFloat(.375f);
    chunk.WriteIdentifier(0x3f800); chunk.WriteInt(1); chunk.WriteObjectID(0); chunk.WriteSubChunk(&point);
    chunk.WriteIdentifier(0x100); chunk.StartObjectIDSequence(1); chunk.WriteObjectIDSequence(0);
    chunk.WriteFloat(.625f); WriteCurvePoint(chunk, .75f, .25f); chunk.CloseChunk();
    CurveSerializationProbe curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(1, curve.GetControlPointCount());
    ExpectCurvePoint(curve, 0, .75f, .25f); EXPECT_FLOAT_EQ(.625f, curve.Fit());
}

TEST(CK2dCurveSerializationTest, ModernReadStartsFromTheChunkBeginningAndCanBeRepeated) {
    CKStateChunk chunk(CKCHNK_2DCURVE, nullptr); WriteCurveHeader(chunk, 2, 0);
    WriteCurvePoint(chunk, 0, 0); WriteCurvePoint(chunk, 1, .5f); chunk.CloseChunk();
    chunk.StartRead(); ASSERT_TRUE(chunk.SeekIdentifier(0x100)); chunk.ReadXObjectArray(); chunk.ReadFloat();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&chunk)); ASSERT_EQ(CK_OK, curve.Read(&chunk));
    ASSERT_EQ(2, curve.GetControlPointCount()); ExpectCurvePoint(curve, 1, 1, .5f);
}
