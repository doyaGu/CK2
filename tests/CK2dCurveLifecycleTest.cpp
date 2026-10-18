#include <gtest/gtest.h>
#include <cmath>
#include <new>
#define CK_LIB
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

class CurveLifecycleProbe : public CK2dCurve {
public:
    int Capacity() const { return m_ControlPoints.Allocated(); }
    int ClampIndex(int index) { return Rindex(index); }
    float FitCoefficient() const { return FittingCoef; }
};

class CurveLifecyclePointProbe : public CK2dCurvePoint {
public:
    const Vx2DVector &FittedPosition() const { return m_RCurvePos; }
};

static void RemoveAllCurvePoints(CK2dCurve &curve) {
    while (curve.GetControlPointCount()) curve.DeleteControlPoint(curve.GetControlPoint(0));
}

static void LinearCurve(CK2dCurve &curve) {
    for (int i = 0; i < curve.GetControlPointCount(); ++i) curve.GetControlPoint(i)->SetLinear(TRUE);
}

static void ExpectCurveCoordinate(CK2dCurve &curve, int index, float x, float y) {
    CK2dCurvePoint *point = curve.GetControlPoint(index);
    EXPECT_EQ(&curve, point->GetCurve());
    EXPECT_FLOAT_EQ(x, point->GetPosition().x); EXPECT_FLOAT_EQ(y, point->GetPosition().y);
}

TEST(CK2dCurveLifecycleTest, ConstructorCreatesTwoOwnedNeutralEndpointsAndAComputedLength) {
    CurveLifecycleProbe curve;
    ASSERT_EQ(2, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(0, curve.FitCoefficient());
    ExpectCurveCoordinate(curve, 0, 0, 0); ExpectCurveCoordinate(curve, 1, 1, 1);
    EXPECT_NEAR(sqrtf(2.0f), curve.GetLength(), .00002f);
    EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
    EXPECT_FLOAT_EQ(curve.GetLength(), curve.GetControlPoint(1)->GetLength());
    for (int i = 0; i < 2; ++i) {
        EXPECT_TRUE(curve.GetControlPoint(i)->IsTCB()); EXPECT_FALSE(curve.GetControlPoint(i)->IsLinear());
        EXPECT_FLOAT_EQ(0, curve.GetControlPoint(i)->GetTension());
        EXPECT_FLOAT_EQ(0, curve.GetControlPoint(i)->GetContinuity());
        EXPECT_FLOAT_EQ(0, curve.GetControlPoint(i)->GetBias());
    }
}

TEST(CK2dCurveLifecycleTest, PointConstructorInitializesVectorsAndParametersInPoisonedStorage) {
    alignas(CurveLifecyclePointProbe) CKBYTE storage[sizeof(CurveLifecyclePointProbe)];
    memset(storage,0xA5,sizeof(storage));
    CurveLifecyclePointProbe *point = new (storage) CurveLifecyclePointProbe;
    EXPECT_EQ(nullptr,point->GetCurve()); EXPECT_TRUE(point->IsTCB()); EXPECT_FALSE(point->IsLinear());
    EXPECT_FLOAT_EQ(0,point->GetTension()); EXPECT_FLOAT_EQ(0,point->GetContinuity()); EXPECT_FLOAT_EQ(0,point->GetBias());
    EXPECT_FLOAT_EQ(0,point->GetPosition().x); EXPECT_FLOAT_EQ(0,point->GetPosition().y);
    EXPECT_FLOAT_EQ(0,point->GetInTangent().x); EXPECT_FLOAT_EQ(0,point->GetInTangent().y);
    EXPECT_FLOAT_EQ(0,point->GetOutTangent().x); EXPECT_FLOAT_EQ(0,point->GetOutTangent().y);
    EXPECT_FLOAT_EQ(0,point->FittedPosition().x); EXPECT_FLOAT_EQ(0,point->FittedPosition().y);
    point->~CurveLifecyclePointProbe();
}

TEST(CK2dCurveLifecycleTest, SeparateConstructedCurvesOwnIndependentPointArrays) {
    CK2dCurve first, second;
    ASSERT_NE(first.GetControlPoint(0), second.GetControlPoint(0));
    first.GetControlPoint(1)->SetPosition(Vx2DVector(1.0f, .5f));
    ExpectCurveCoordinate(second, 1, 1, 1);
    EXPECT_NEAR(sqrtf(2.0f), second.GetLength(), .00002f);
    EXPECT_LT(first.GetLength(), second.GetLength());
}

TEST(CK2dCurveLifecycleTest, AddedPointsAreSortedAndKeepTheirOwnerThroughArrayGrowth) {
    CurveLifecycleProbe curve; const int initialCapacity = curve.Capacity();
    const float coordinates[] = {.75f,.125f,.625f,.375f};
    for (float x : coordinates) curve.AddControlPoint(Vx2DVector(x,x));
    ASSERT_EQ(6, curve.GetControlPointCount()); EXPECT_GT(curve.Capacity(), initialCapacity);
    const float sorted[] = {0,.125f,.375f,.625f,.75f,1};
    for (int i = 0; i < 6; ++i) ExpectCurveCoordinate(curve, i, sorted[i], sorted[i]);
    LinearCurve(curve); EXPECT_NEAR(sqrtf(2.0f), curve.GetLength(), .00002f);
}

TEST(CK2dCurveLifecycleTest, AddSnapshotsAPositionBorrowedFromItsOwnArrayBeforeGrowth) {
    CurveLifecycleProbe curve; ASSERT_EQ(2, curve.Capacity());
    const Vx2DVector &position = curve.GetControlPoint(1)->GetPosition();
    curve.AddControlPoint(position);
    ASSERT_EQ(3, curve.GetControlPointCount()); EXPECT_GT(curve.Capacity(), 2);
    ExpectCurveCoordinate(curve, 1, 1, 1); ExpectCurveCoordinate(curve, 2, 1, 1);
}

TEST(CK2dCurveLifecycleTest, DeleteMiddlePointCompactsRemainingPointsAndRecomputesLength) {
    CK2dCurve curve;
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1, 0));
    curve.AddControlPoint(Vx2DVector(.25f,.5f)); LinearCurve(curve);
    EXPECT_NEAR(1.4604048f, curve.GetLength(), .000003f);
    curve.DeleteControlPoint(curve.GetControlPoint(1));
    ASSERT_EQ(2, curve.GetControlPointCount());
    ExpectCurveCoordinate(curve, 0, 0, 0); ExpectCurveCoordinate(curve, 1, 1, 0);
    EXPECT_FLOAT_EQ(1, curve.GetLength()); EXPECT_FLOAT_EQ(1, curve.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurveLifecycleTest, DeleteFirstPointResetsTotalAndTheNewFirstCumulativeLength) {
    CK2dCurve curve;
    curve.DeleteControlPoint(curve.GetControlPoint(0));
    ASSERT_EQ(1, curve.GetControlPointCount()); ExpectCurveCoordinate(curve, 0, 1, 1);
    EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
}

TEST(CK2dCurveLifecycleTest, DeleteLastEndpointLeavesASinglePointWithZeroTotalLength) {
    CK2dCurve curve;
    curve.DeleteControlPoint(curve.GetControlPoint(1));
    ASSERT_EQ(1, curve.GetControlPointCount()); ExpectCurveCoordinate(curve, 0, 0, 0);
    EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
}

TEST(CK2dCurveLifecycleTest, DeleteFinalPointAndRepeatedEmptyUpdatesKeepZeroLength) {
    CK2dCurve curve; RemoveAllCurvePoints(curve);
    ASSERT_EQ(0, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(0, curve.GetLength());
    curve.Update(); curve.Update(); EXPECT_FLOAT_EQ(0, curve.GetLength());
    Vx2DVector position(3,4); EXPECT_EQ(CKERR_INVALIDPARAMETER, curve.GetPos(.5f,&position));
    EXPECT_FLOAT_EQ(3, position.x); EXPECT_FLOAT_EQ(4, position.y); EXPECT_FLOAT_EQ(0, curve.GetY(.5f));
}

TEST(CK2dCurveLifecycleTest, AddingToAnEmptiedCurveRebuildsBothSingleAndTwoPointCaches) {
    CK2dCurve curve; RemoveAllCurvePoints(curve);
    curve.AddControlPoint(Vx2DVector(.25f,.5f));
    ASSERT_EQ(1, curve.GetControlPointCount()); ExpectCurveCoordinate(curve,0,.25f,.5f);
    EXPECT_FLOAT_EQ(0, curve.GetLength()); EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
    curve.AddControlPoint(Vx2DVector(.75f,.5f)); LinearCurve(curve);
    ASSERT_EQ(2, curve.GetControlPointCount()); EXPECT_FLOAT_EQ(.5f, curve.GetLength());
    EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength()); EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurveLifecycleTest, PublicPointMutationNotifiesItsOwnerAndRefreshesCumulativeLengths) {
    CK2dCurve curve;
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1,0));
    curve.AddControlPoint(Vx2DVector(.25f,.5f)); LinearCurve(curve);
    const float previous = curve.GetLength();
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(.5f,1.0f));
    EXPECT_GT(curve.GetLength(), previous); EXPECT_NEAR(sqrtf(5.0f), curve.GetLength(), .000003f);
    EXPECT_NEAR(sqrtf(1.25f), curve.GetControlPoint(1)->GetLength(), .000003f);
    EXPECT_FLOAT_EQ(curve.GetLength(),curve.GetControlPoint(2)->GetLength());
}

TEST(CK2dCurveLifecycleTest, DirectPositionEditsAreSortedAsWholePointRecordsByExplicitUpdate) {
    CK2dCurve curve; curve.AddControlPoint(Vx2DVector(.25f,.25f)); curve.AddControlPoint(Vx2DVector(.75f,.75f));
    curve.GetControlPoint(1)->SetBias(.25f); curve.GetControlPoint(1)->SetTension(.5f);
    curve.GetControlPoint(1)->GetPosition() = Vx2DVector(.875f,.5f); curve.Update();
    ExpectCurveCoordinate(curve,1,.75f,.75f); ExpectCurveCoordinate(curve,2,.875f,.5f);
    EXPECT_FLOAT_EQ(.25f,curve.GetControlPoint(2)->GetBias()); EXPECT_FLOAT_EQ(.5f,curve.GetControlPoint(2)->GetTension());
}

TEST(CK2dCurveLifecycleTest, SortComparatorMatchesNativeSignsIncludingEqualX) {
    CK2dCurvePoint low, high, equal;
    low.GetPosition()=Vx2DVector(.25f,.5f); high.GetPosition()=Vx2DVector(.75f,.5f); equal.GetPosition()=Vx2DVector(.25f,.9f);
    EXPECT_EQ(-1, CK2dCurvePoint::CurvePointSortFunc(&low,&high));
    EXPECT_EQ(1, CK2dCurvePoint::CurvePointSortFunc(&high,&low));
    EXPECT_EQ(1, CK2dCurvePoint::CurvePointSortFunc(&low,&equal));
    EXPECT_EQ(1, CK2dCurvePoint::CurvePointSortFunc(&low,&low));
}

TEST(CK2dCurveLifecycleTest, EqualXSortPreservesInsertionOrderAndParameterIdentity) {
    CK2dCurve curve; curve.AddControlPoint(Vx2DVector(.5f,.5f));
    curve.GetControlPoint(1)->SetBias(.25f); curve.AddControlPoint(Vx2DVector(.5f,.5f));
    ASSERT_EQ(4,curve.GetControlPointCount());
    EXPECT_FLOAT_EQ(.25f,curve.GetControlPoint(1)->GetBias()); EXPECT_FLOAT_EQ(0,curve.GetControlPoint(2)->GetBias());
    curve.Update(); EXPECT_FLOAT_EQ(.25f,curve.GetControlPoint(1)->GetBias()); EXPECT_FLOAT_EQ(0,curve.GetControlPoint(2)->GetBias());
}

TEST(CK2dCurveLifecycleTest, LinearSegmentsPublishTheirOwnPrefixLengths) {
    CK2dCurve curve;
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1,0)); curve.AddControlPoint(Vx2DVector(.25f,.5f)); LinearCurve(curve);
    EXPECT_FLOAT_EQ(0,curve.GetControlPoint(0)->GetLength());
    EXPECT_NEAR(.559017f,curve.GetControlPoint(1)->GetLength(),.000003f);
    EXPECT_NEAR(1.4604048f,curve.GetControlPoint(2)->GetLength(),.000003f);
    EXPECT_FLOAT_EQ(curve.GetControlPoint(2)->GetLength(),curve.GetLength());
}

TEST(CK2dCurveLifecycleTest, CubicLengthMatchesTheIndependentHundredChordParabolaValue) {
    CK2dCurve curve;
    curve.GetControlPoint(0)->UseTCB(FALSE); curve.GetControlPoint(1)->UseTCB(FALSE);
    curve.GetControlPoint(0)->SetOutTangent(Vx2DVector(1,0)); curve.GetControlPoint(1)->SetInTangent(Vx2DVector(1,2));
    // x=t, y=t^2 sampled at1/100: independently summed polygon length.
    EXPECT_NEAR(1.4789354f,curve.GetLength(),.000003f);
    EXPECT_FLOAT_EQ(0,curve.GetControlPoint(0)->GetLength());
    EXPECT_FLOAT_EQ(curve.GetLength(),curve.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurveLifecycleTest, MixedLinearThenCubicUsesEachSegmentsOwnLinearFlag) {
    CK2dCurve curve; curve.AddControlPoint(Vx2DVector(.25f,0.0f));
    for (int i=0;i<3;++i) curve.GetControlPoint(i)->UseTCB(FALSE);
    curve.GetControlPoint(0)->SetLinear(TRUE);
    curve.GetControlPoint(1)->SetOutTangent(Vx2DVector(.75f,0.0f)); curve.GetControlPoint(2)->SetInTangent(Vx2DVector(.75f,2.0f));
    EXPECT_NEAR(.25f,curve.GetControlPoint(1)->GetLength(),.000003f);
    EXPECT_NEAR(1.5580974f,curve.GetLength(),.000003f);
}

TEST(CK2dCurveLifecycleTest, MixedCubicThenLinearIgnoresCurvedTangentsForTheLinearSegment) {
    CK2dCurve curve; curve.AddControlPoint(Vx2DVector(.25f,0.0f));
    for (int i=0;i<3;++i) curve.GetControlPoint(i)->UseTCB(FALSE);
    curve.GetControlPoint(0)->SetOutTangent(Vx2DVector(1,0)); curve.GetControlPoint(1)->SetInTangent(Vx2DVector(1,0));
    curve.GetControlPoint(1)->SetOutTangent(Vx2DVector(.75f,3.0f)); curve.GetControlPoint(2)->SetInTangent(Vx2DVector(.75f,-2.0f));
    curve.GetControlPoint(1)->SetLinear(TRUE);
    EXPECT_NEAR(1.5f,curve.GetLength(),.000003f);
}

TEST(CK2dCurveLifecycleTest, RepeatedUpdatesReplaceRatherThanAccumulateCachedLengths) {
    CK2dCurve curve;
    curve.GetControlPoint(0)->UseTCB(FALSE); curve.GetControlPoint(1)->UseTCB(FALSE);
    curve.GetControlPoint(0)->SetOutTangent(Vx2DVector(1,0)); curve.GetControlPoint(1)->SetInTangent(Vx2DVector(1,2));
    const float expected=curve.GetLength();
    for (int i=0;i<10;++i) curve.Update();
    EXPECT_FLOAT_EQ(expected,curve.GetLength()); EXPECT_FLOAT_EQ(expected,curve.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurveLifecycleTest, CountAndValidIndexedAccessReferToTheActualOwnedPoints) {
    CK2dCurve curve; curve.AddControlPoint(Vx2DVector(.5f,.25f));
    ASSERT_EQ(3,curve.GetControlPointCount());
    EXPECT_EQ(curve.GetControlPoint(0)+1,curve.GetControlPoint(1));
    EXPECT_EQ(curve.GetControlPoint(1)+1,curve.GetControlPoint(2));
    ExpectCurveCoordinate(curve,0,0,0); ExpectCurveCoordinate(curve,1,.5f,.25f); ExpectCurveCoordinate(curve,2,1,1);
}

TEST(CK2dCurveLifecycleTest, InternalIndexClampingMatchesNativeEvenForAnEmptyCurve) {
    CurveLifecycleProbe curve;
    EXPECT_EQ(0,curve.ClampIndex(-3)); EXPECT_EQ(0,curve.ClampIndex(0)); EXPECT_EQ(1,curve.ClampIndex(1)); EXPECT_EQ(1,curve.ClampIndex(20));
    RemoveAllCurvePoints(curve);
    EXPECT_EQ(0,curve.ClampIndex(-1)); EXPECT_EQ(-1,curve.ClampIndex(0)); EXPECT_EQ(-1,curve.ClampIndex(20));
}

TEST(CK2dCurveLifecycleTest, OwnedNotificationRefreshesDataWhileDetachedNotificationIsANoop) {
    CK2dCurve curve; LinearCurve(curve);
    curve.GetControlPoint(1)->GetPosition()=Vx2DVector(1,0); curve.GetControlPoint(1)->NotifyUpdate();
    EXPECT_FLOAT_EQ(1,curve.GetLength());
    CK2dCurvePoint detached; detached.GetPosition()=Vx2DVector(.25f,.5f);
    detached.NotifyUpdate(); EXPECT_EQ(nullptr,detached.GetCurve());
    EXPECT_FLOAT_EQ(.25f,detached.GetPosition().x); EXPECT_FLOAT_EQ(.5f,detached.GetPosition().y);
}
