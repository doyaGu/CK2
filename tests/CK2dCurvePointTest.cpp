#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#ifndef CK_LIB
#define CK_LIB
#endif
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

class CurvePointProbe : public CK2dCurvePoint {
public:
    CurvePointProbe() {
        m_Length = 17; m_Tension = .125f; m_Continuity = -.25f; m_Bias = .5f;
        m_Position.Set(.25f, .75f); m_InTang.Set(.5f, .25f); m_OutTang.Set(.75f, -.375f);
        m_RCurvePos.Set(9, 8); m_Flags = 0x80;
    }
    explicit CurvePointProbe(const CK2dCurvePoint &point) : CK2dCurvePoint(point) {}
    using CK2dCurvePoint::SetCurve;
    using CK2dCurvePoint::SetLength;
    using CK2dCurvePoint::GetRCurvePos;
    using CK2dCurvePoint::SetRCurvePos;
    CKDWORD Flags() const { return m_Flags; }
    void SetFlags(CKDWORD flags) { m_Flags = flags; }
    size_t Offset(int field) const {
        const void *fields[] = {&m_Curve, &m_Tension, &m_Continuity, &m_Bias, &m_Length,
            &m_Position, &m_InTang, &m_OutTang, &m_RCurvePos, &m_Flags};
        return (const char *)fields[field] - (const char *)this;
    }
    static void Load(CK2dCurvePoint &point, CKStateChunk *chunk) {
        void (CK2dCurvePoint::*read)(CKStateChunk *) = &CurvePointProbe::Read;
        (point.*read)(chunk);
    }
};

class PointOwnerProbe : public CK2dCurve {
public:
    void DirtyLength() { m_Length = 987; }
    void SetFit(float fit) { FittingCoef = fit; }
};

static void PointThreeKnots(PointOwnerProbe &curve) {
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1.0f, .5f));
    curve.AddControlPoint(Vx2DVector(.25f, .75f));
}

static void PointFlatCurve(PointOwnerProbe &curve) {
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1, 0));
    curve.GetControlPoint(0)->UseTCB(FALSE); curve.GetControlPoint(1)->UseTCB(FALSE);
}

static void ExpectPointVector(const Vx2DVector &value, float x, float y) {
    EXPECT_NEAR(x, value.x, .000002f); EXPECT_NEAR(y, value.y, .000002f);
}

static void WritePointDefault(CKStateChunk &chunk, float x, float y, int tangents = 1, int linear = 1) {
    chunk.WriteIdentifier(0x4000); chunk.WriteObjectID(456);
    chunk.WriteInt(tangents); chunk.WriteInt(linear); chunk.WriteFloat(x); chunk.WriteFloat(y);
}

static void WritePointTCB(CKStateChunk &chunk, float tension = .375f, float continuity = -.5f, float bias = .625f) {
    chunk.WriteIdentifier(0x800); chunk.WriteFloat(tension); chunk.WriteFloat(continuity); chunk.WriteFloat(bias);
}

static void WritePointTangents(CKStateChunk &chunk, float ix = 2, float iy = -2, float ox = 4, float oy = 2) {
    chunk.WriteIdentifier(0x1000); chunk.WriteFloat(ix); chunk.WriteFloat(iy); chunk.WriteFloat(ox); chunk.WriteFloat(oy);
}

TEST(CK2dCurvePointTest, LayoutMatchesAllTenNativeWin32FieldsAndNaturalX64Offsets) {
    CurvePointProbe point; const size_t offsets32[] = {0,4,8,12,16,20,28,36,44,52};
    const size_t offsets64[] = {0,8,12,16,20,24,32,40,48,56};
    for (int i = 0; i < 10; ++i) EXPECT_EQ(sizeof(void *) == 4 ? offsets32[i] : offsets64[i], point.Offset(i));
    EXPECT_EQ(sizeof(void *) == 4 ? 56u : 64u, sizeof(CK2dCurvePoint)); EXPECT_EQ(sizeof(void *), alignof(CK2dCurvePoint));
    EXPECT_FALSE((bool)__is_polymorphic(CK2dCurvePoint)); EXPECT_TRUE((bool)__has_trivial_destructor(CK2dCurvePoint));
}

TEST(CK2dCurvePointTest, ConstScalarGettersReadIndependentFloatSlots) {
    const CurvePointProbe point;
    EXPECT_FLOAT_EQ(.125f, point.GetTension()); EXPECT_FLOAT_EQ(-.25f, point.GetContinuity());
    EXPECT_FLOAT_EQ(.5f, point.GetBias()); EXPECT_FLOAT_EQ(17, point.GetLength()); EXPECT_EQ(nullptr, point.GetCurve());
}

TEST(CK2dCurvePointTest, BorrowedOwnerSetCurveDoesNotInsertTransferOwnershipOrNotify) {
    PointOwnerProbe first, second; CurvePointProbe point; first.DirtyLength(); second.DirtyLength();
    point.SetCurve(&first); EXPECT_EQ(&first, point.GetCurve()); EXPECT_FLOAT_EQ(987, first.GetLength());
    point.SetCurve(&second); EXPECT_EQ(&second, point.GetCurve()); EXPECT_EQ(2, second.GetControlPointCount());
    EXPECT_FLOAT_EQ(987, second.GetLength()); point.SetCurve(nullptr); EXPECT_EQ(nullptr, point.GetCurve());
}

TEST(CK2dCurvePointTest, PointCopiesAreValueRecordsWithBorrowedOwnerAndIndependentVectors) {
    PointOwnerProbe curve; curve.DirtyLength(); CurvePointProbe source; source.SetCurve(&curve);
    CurvePointProbe copy(source), assigned; assigned = source;
    EXPECT_EQ(&curve, copy.GetCurve()); EXPECT_EQ(&curve, assigned.GetCurve()); EXPECT_FLOAT_EQ(987, curve.GetLength());
    assigned.GetPosition().Set(.75f, .5f); assigned.GetInTangent().Set(8, 9);
    ExpectPointVector(source.GetPosition(), .25f, .75f); ExpectPointVector(source.GetInTangent(), .5f, .25f);
    EXPECT_FLOAT_EQ(17, copy.GetLength()); EXPECT_EQ(0x80u, copy.Flags());
}

TEST(CK2dCurvePointTest, DestroyingAPointDoesNotDeleteOrNotifyItsBorrowedOwner) {
    PointOwnerProbe curve; curve.DirtyLength();
    { CurvePointProbe point; point.SetCurve(&curve); }
    EXPECT_FLOAT_EQ(987, curve.GetLength()); curve.Update(); EXPECT_NEAR(sqrtf(2.0f), curve.GetLength(), .000003f);
}

TEST(CK2dCurvePointTest, InternalLengthSetterIsAPlainCacheWriteWithoutNotification) {
    PointOwnerProbe curve; curve.DirtyLength(); CurvePointProbe point; point.SetCurve(&curve); point.SetLength(12.5f);
    EXPECT_FLOAT_EQ(12.5f, point.GetLength()); EXPECT_FLOAT_EQ(987, curve.GetLength());
    point.SetLength(0); EXPECT_FLOAT_EQ(0, point.GetLength());
}

TEST(CK2dCurvePointTest, FittedCacheGetAndSetExposeTheStoredVectorWithoutNotification) {
    PointOwnerProbe curve; curve.DirtyLength(); CurvePointProbe point; point.SetCurve(&curve);
    Vx2DVector fit(.625f, .875f); point.SetRCurvePos(fit); EXPECT_EQ(&point.GetRCurvePos(), &point.GetRCurvePos());
    ExpectPointVector(point.GetRCurvePos(), .625f, .875f); point.GetRCurvePos().y = .25f;
    ExpectPointVector(point.GetRCurvePos(), .625f, .25f); ExpectPointVector(point.GetPosition(), .25f, .75f);
    EXPECT_FLOAT_EQ(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, WritablePositionReferenceRequiresExplicitNotification) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.GetControlPoint(0)->SetLinear(TRUE);
    curve.DirtyLength(); CK2dCurvePoint *point = curve.GetControlPoint(1);
    EXPECT_EQ(&point->GetPosition(), &point->GetPosition()); point->GetPosition().y = .5f;
    EXPECT_FLOAT_EQ(987, curve.GetLength()); point->NotifyUpdate(); EXPECT_NEAR(sqrtf(1.25f), curve.GetLength(), .000003f);
}

TEST(CK2dCurvePointTest, WritableTangentReferencesRequireExplicitNotification) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.DirtyLength(); CK2dCurvePoint *point = curve.GetControlPoint(0);
    EXPECT_NE(&point->GetInTangent(), &point->GetOutTangent());
    point->GetOutTangent().Set(2, 2); EXPECT_FLOAT_EQ(987, curve.GetLength()); point->NotifyUpdate();
    ExpectPointVector(point->GetOutTangent(), 1, 1); EXPECT_NEAR(.125f, curve.GetY(.5f), .000003f);
}

TEST(CK2dCurvePointTest, UseTCBOnlySwitchesItsInverseBitAndDoesNotRecompute) {
    PointOwnerProbe curve; PointFlatCurve(curve); CK2dCurvePoint *point = curve.GetControlPoint(0); curve.DirtyLength();
    point->GetOutTangent().Set(2, 3); point->UseTCB(-7); EXPECT_EQ(1, point->IsTCB());
    ExpectPointVector(point->GetOutTangent(), 2, 3); EXPECT_FLOAT_EQ(987, curve.GetLength());
    point->UseTCB(FALSE); EXPECT_EQ(0, point->IsTCB()); EXPECT_FLOAT_EQ(987, curve.GetLength());
    CurvePointProbe detached; detached.SetFlags(0x82); detached.UseTCB(FALSE); EXPECT_EQ(0x83u, detached.Flags());
    detached.UseTCB(TRUE); EXPECT_EQ(0x82u, detached.Flags()); EXPECT_EQ(2, detached.IsLinear());
}

TEST(CK2dCurvePointTest, SetLinearPreservesOtherBitsAndIsLinearReturnsTheNativeMask) {
    CurvePointProbe point; point.SetFlags(0x81); point.SetLinear(-3);
    EXPECT_EQ(0x83u, point.Flags()); EXPECT_EQ(2, point.IsLinear()); EXPECT_EQ(0, point.IsTCB());
    point.SetLinear(FALSE); EXPECT_EQ(0x81u, point.Flags()); EXPECT_EQ(0, point.IsLinear());
}

TEST(CK2dCurvePointTest, BiasSetterUpdatesTheOwnedTcbTangents) {
    PointOwnerProbe curve; PointThreeKnots(curve); CK2dCurvePoint *point = curve.GetControlPoint(1); curve.DirtyLength();
    point->SetBias(.5f); EXPECT_FLOAT_EQ(.5f, point->GetBias());
    ExpectPointVector(point->GetOutTangent(), .75f, 1); ExpectPointVector(point->GetInTangent(), .25f, 1.0f/3);
    EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, ContinuitySetterUpdatesIncomingAndOutgoingWeightsDifferently) {
    PointOwnerProbe curve; PointThreeKnots(curve); CK2dCurvePoint *point = curve.GetControlPoint(1); curve.DirtyLength();
    point->SetContinuity(.5f); EXPECT_FLOAT_EQ(.5f, point->GetContinuity());
    ExpectPointVector(point->GetOutTangent(), .75f, 1); ExpectPointVector(point->GetInTangent(), .25f, 0);
    EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, TensionSetterCanCollapseTheOwnedTcbTangents) {
    PointOwnerProbe curve; PointThreeKnots(curve); CK2dCurvePoint *point = curve.GetControlPoint(1); curve.DirtyLength();
    point->SetTension(1); EXPECT_FLOAT_EQ(1, point->GetTension());
    ExpectPointVector(point->GetInTangent(), 0, 0); ExpectPointVector(point->GetOutTangent(), 0, 0);
    EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, OwnedLinearSetterRebuildsGeometryImmediately) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.GetControlPoint(0)->SetOutTangent(Vx2DVector(2, 2));
    EXPECT_NEAR(.125f, curve.GetY(.5f), .000003f); curve.DirtyLength(); curve.GetControlPoint(0)->SetLinear(TRUE);
    EXPECT_FLOAT_EQ(1, curve.GetLength()); EXPECT_FLOAT_EQ(0, curve.GetY(.5f));
}

TEST(CK2dCurvePointTest, PositionSetterSortsTheCompletePointRecordAndRefreshesPrefixes) {
    PointOwnerProbe curve; PointThreeKnots(curve); curve.GetControlPoint(1)->SetBias(.5f);
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1.25f, .25f));
    EXPECT_FLOAT_EQ(1, curve.GetControlPoint(1)->GetPosition().x);
    EXPECT_FLOAT_EQ(1.25f, curve.GetControlPoint(2)->GetPosition().x); EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(2)->GetBias());
    EXPECT_FLOAT_EQ(curve.GetLength(), curve.GetControlPoint(2)->GetLength());
}

TEST(CK2dCurvePointTest, OutgoingSetterNormalizesExplicitTangentsAndUpdatesEvaluation) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.DirtyLength();
    curve.GetControlPoint(0)->SetOutTangent(Vx2DVector(2, 2));
    ExpectPointVector(curve.GetControlPoint(0)->GetOutTangent(), 1, 1);
    EXPECT_NEAR(.125f, curve.GetY(.5f), .000003f); EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, IncomingSetterNormalizesExplicitTangentsAndUpdatesEvaluation) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.DirtyLength();
    curve.GetControlPoint(1)->SetInTangent(Vx2DVector(2, -2));
    ExpectPointVector(curve.GetControlPoint(1)->GetInTangent(), 1, -1);
    EXPECT_NEAR(.125f, curve.GetY(.5f), .000003f); EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, TangentSettersDoNotDisableTcbAndTheOwnerRecomputesTcbValues) {
    PointOwnerProbe curve; PointThreeKnots(curve); CK2dCurvePoint *point = curve.GetControlPoint(1);
    point->SetOutTangent(Vx2DVector(9, 8)); EXPECT_EQ(1, point->IsTCB());
    ExpectPointVector(point->GetOutTangent(), .75f, .375f);
    point->SetInTangent(Vx2DVector(8, 9)); ExpectPointVector(point->GetInTangent(), .25f, .125f);
}

TEST(CK2dCurvePointTest, DetachedPublicSettersStoreValuesWithoutAnOwner) {
    CurvePointProbe point; point.SetTension(.625f); point.SetContinuity(.375f); point.SetBias(-.5f);
    point.SetPosition(Vx2DVector(.5f, .25f)); point.SetInTangent(Vx2DVector(2, 3)); point.SetOutTangent(Vx2DVector(4, 5));
    EXPECT_FLOAT_EQ(.625f, point.GetTension()); EXPECT_FLOAT_EQ(.375f, point.GetContinuity()); EXPECT_FLOAT_EQ(-.5f, point.GetBias());
    ExpectPointVector(point.GetPosition(), .5f, .25f); ExpectPointVector(point.GetInTangent(), 2, 3); ExpectPointVector(point.GetOutTangent(), 4, 5);
    EXPECT_EQ(nullptr, point.GetCurve()); EXPECT_FLOAT_EQ(17, point.GetLength());
}

TEST(CK2dCurvePointTest, SettersAcceptReferencesBorrowedFromTheSamePoint) {
    CurvePointProbe point; point.SetPosition(point.GetOutTangent()); ExpectPointVector(point.GetPosition(), .75f, -.375f);
    point.SetInTangent(point.GetPosition()); point.SetOutTangent(point.GetInTangent());
    ExpectPointVector(point.GetInTangent(), .75f, -.375f); ExpectPointVector(point.GetOutTangent(), .75f, -.375f);
}

TEST(CK2dCurvePointTest, NullReadPreservesEveryInitializedField) {
    CurvePointProbe point; CurvePointProbe::Load(point, nullptr);
    EXPECT_FLOAT_EQ(17, point.GetLength()); EXPECT_FLOAT_EQ(.125f, point.GetTension()); EXPECT_EQ(0x80u, point.Flags());
    ExpectPointVector(point.GetPosition(), .25f, .75f); ExpectPointVector(point.GetRCurvePos(), 9, 8);
}

TEST(CK2dCurvePointTest, MissingLegacySectionsDoNotChangePointState) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); chunk.WriteIdentifier(0x20); chunk.WriteInt(99); chunk.CloseChunk();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk); EXPECT_FLOAT_EQ(.125f, point.GetTension());
    EXPECT_EQ(0x80u, point.Flags()); ExpectPointVector(point.GetPosition(), .25f, .75f);
}

TEST(CK2dCurvePointTest, DetachedDefaultReadUsesRawVectorAndPreservesUnspecifiedFields) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointDefault(chunk, .75f, .5f, -7, 2); chunk.CloseChunk();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk);
    EXPECT_EQ(0x83u, point.Flags()); EXPECT_EQ(nullptr, point.GetCurve()); ExpectPointVector(point.GetPosition(), .75f, .5f);
    EXPECT_FLOAT_EQ(.125f, point.GetTension()); ExpectPointVector(point.GetInTangent(), .5f, .25f); EXPECT_FLOAT_EQ(17, point.GetLength());
}

TEST(CK2dCurvePointTest, ZeroLegacyBooleansClearOnlyTheirTwoFlagBits) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointDefault(chunk, .75f, .5f, 0, 0); chunk.CloseChunk();
    CurvePointProbe point; point.SetFlags(0x83); CurvePointProbe::Load(point, &chunk);
    EXPECT_EQ(0x80u, point.Flags()); EXPECT_EQ(1, point.IsTCB()); EXPECT_EQ(0, point.IsLinear());
}

TEST(CK2dCurvePointTest, DetachedTcbOnlyReadPreservesPositionTangentsAndFlags) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointTCB(chunk); chunk.CloseChunk();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk);
    EXPECT_FLOAT_EQ(.375f, point.GetTension()); EXPECT_FLOAT_EQ(-.5f, point.GetContinuity()); EXPECT_FLOAT_EQ(.625f, point.GetBias());
    EXPECT_EQ(0x80u, point.Flags()); ExpectPointVector(point.GetPosition(), .25f, .75f); ExpectPointVector(point.GetOutTangent(), .75f, -.375f);
}

TEST(CK2dCurvePointTest, DetachedTangentsOnlyReadUsesTwoRawEightByteVectors) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointTangents(chunk); chunk.CloseChunk();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk);
    ExpectPointVector(point.GetInTangent(), 2, -2); ExpectPointVector(point.GetOutTangent(), 4, 2);
    EXPECT_FLOAT_EQ(.125f, point.GetTension()); ExpectPointVector(point.GetPosition(), .25f, .75f);
}

TEST(CK2dCurvePointTest, FullReadFindsReorderedSectionsAndDoesNotInterpretObjectIdAsOwner) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); chunk.SetDataVersion(10);
    WritePointTangents(chunk); WritePointTCB(chunk); WritePointDefault(chunk, .75f, .25f); chunk.CloseChunk();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk);
    EXPECT_EQ(nullptr, point.GetCurve()); EXPECT_EQ(0x83u, point.Flags()); EXPECT_FLOAT_EQ(.625f, point.GetBias());
    ExpectPointVector(point.GetPosition(), .75f, .25f); ExpectPointVector(point.GetInTangent(), 2, -2); ExpectPointVector(point.GetOutTangent(), 4, 2);
    ExpectPointVector(point.GetRCurvePos(), 9, 8); EXPECT_FLOAT_EQ(17, point.GetLength());
}

TEST(CK2dCurvePointTest, OwnedDefaultReadRefreshesGeometryAfterThePositionIsRead) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.GetControlPoint(0)->SetLinear(TRUE);
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointDefault(chunk, 1, .5f); chunk.CloseChunk();
    CurvePointProbe::Load(*curve.GetControlPoint(1), &chunk);
    EXPECT_NEAR(sqrtf(1.25f), curve.GetLength(), .000003f);
    EXPECT_FLOAT_EQ(curve.GetLength(), curve.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurvePointTest, OwnedFullReadCompletesFieldsBeforeSortingThePointRecord) {
    PointOwnerProbe curve; PointThreeKnots(curve);
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointDefault(chunk, 1.25f, .25f);
    WritePointTCB(chunk); WritePointTangents(chunk); chunk.CloseChunk(); curve.SetFit(.5f);
    CurvePointProbe::Load(*curve.GetControlPoint(1), &chunk);
    ASSERT_EQ(3, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(1, curve.GetControlPoint(1)->GetPosition().x);
    CK2dCurvePoint *loaded = curve.GetControlPoint(2); EXPECT_FLOAT_EQ(1.25f, loaded->GetPosition().x);
    EXPECT_FLOAT_EQ(.375f, loaded->GetTension()); EXPECT_FLOAT_EQ(.625f, loaded->GetBias());
    ExpectPointVector(loaded->GetInTangent(), .25f, -.25f); ExpectPointVector(loaded->GetOutTangent(), .001f, .0005f);
    EXPECT_EQ(&curve, loaded->GetCurve()); EXPECT_FLOAT_EQ(curve.GetLength(), loaded->GetLength());
}

TEST(CK2dCurvePointTest, OwnedTcbOnlyReadRefreshesTheNewParametersAndLength) {
    PointOwnerProbe curve; PointThreeKnots(curve); curve.DirtyLength();
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointTCB(chunk, 1, 0, 0); chunk.CloseChunk();
    CurvePointProbe::Load(*curve.GetControlPoint(1), &chunk);
    ExpectPointVector(curve.GetControlPoint(1)->GetInTangent(), 0, 0); ExpectPointVector(curve.GetControlPoint(1)->GetOutTangent(), 0, 0);
    EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, OwnedTangentsOnlyReadNormalizesAndRefreshesEvaluation) {
    PointOwnerProbe curve; PointFlatCurve(curve); curve.DirtyLength();
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointTangents(chunk, 2, -2, 4, 0); chunk.CloseChunk();
    CurvePointProbe::Load(*curve.GetControlPoint(1), &chunk);
    ExpectPointVector(curve.GetControlPoint(1)->GetInTangent(), 1, -1);
    EXPECT_NEAR(.125f, curve.GetY(.5f), .000003f); EXPECT_NE(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, OwnedMissingSectionsDoNotNotifyTheOwner) {
    PointOwnerProbe curve; curve.DirtyLength(); CKStateChunk chunk(0, nullptr);
    chunk.StartWrite(); chunk.WriteIdentifier(0x20); chunk.WriteInt(99); chunk.CloseChunk();
    CurvePointProbe::Load(*curve.GetControlPoint(0), &chunk); EXPECT_FLOAT_EQ(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, OwnedNullReadDoesNotNotifyTheOwner) {
    PointOwnerProbe curve; curve.DirtyLength(); CurvePointProbe::Load(*curve.GetControlPoint(0), nullptr);
    EXPECT_FLOAT_EQ(987, curve.GetLength());
}

TEST(CK2dCurvePointTest, RepeatedReadPreservesDecodedDataAndCanReseekAUsedChunk) {
    CKStateChunk chunk(0, nullptr); chunk.StartWrite(); WritePointDefault(chunk, .75f, .25f); WritePointTCB(chunk); chunk.CloseChunk();
    chunk.StartRead(); ASSERT_TRUE(chunk.SeekIdentifier(0x800)); chunk.ReadFloat();
    CurvePointProbe point; CurvePointProbe::Load(point, &chunk); CurvePointProbe::Load(point, &chunk);
    ExpectPointVector(point.GetPosition(), .75f, .25f); EXPECT_FLOAT_EQ(.375f, point.GetTension()); EXPECT_EQ(0x83u, point.Flags());
}

TEST(CK2dCurvePointTest, LegacyCurveReaderStillPublishesDetachedPointDataBeforeItsFinalUpdate) {
    CKStateChunk a(0, nullptr), b(0, nullptr), curveChunk(CKCHNK_2DCURVE, nullptr);
    a.StartWrite(); WritePointDefault(a, 1, .5f); WritePointTCB(a); a.CloseChunk();
    b.StartWrite(); WritePointDefault(b, 0, 0); WritePointTCB(b, .125f, 0, 0); b.CloseChunk();
    curveChunk.StartWrite(); curveChunk.SetDataVersion(0); curveChunk.WriteIdentifier(0x3f800); curveChunk.WriteInt(2);
    curveChunk.WriteObjectID(0); curveChunk.WriteSubChunk(&a); curveChunk.WriteObjectID(0); curveChunk.WriteSubChunk(&b); curveChunk.CloseChunk();
    CK2dCurve curve; ASSERT_EQ(CK_OK, curve.Read(&curveChunk)); ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectPointVector(curve.GetControlPoint(0)->GetPosition(), 0, 0); EXPECT_FLOAT_EQ(.125f, curve.GetControlPoint(0)->GetTension());
    ExpectPointVector(curve.GetControlPoint(1)->GetPosition(), 1, .5f); EXPECT_FLOAT_EQ(.375f, curve.GetControlPoint(1)->GetTension());
    EXPECT_EQ(&curve, curve.GetControlPoint(1)->GetCurve()); EXPECT_NEAR(sqrtf(1.25f), curve.GetLength(), .000003f);
}
