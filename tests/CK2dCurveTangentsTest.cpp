#include <gtest/gtest.h>
#include <cmath>
#define CK_LIB
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

static void ExpectCurveTangent(const Vx2DVector &v, float x, float y) {
    EXPECT_NEAR(x, v.x, .000002f);
    EXPECT_NEAR(y, v.y, .000002f);
}

static void ThreeCurvePoints(CK2dCurve &curve) {
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1.0f, .25f));
    curve.AddControlPoint(Vx2DVector(.25f, .5f));
    ASSERT_EQ(3, curve.GetControlPointCount());
}

static void ExplicitCurveTangents(CK2dCurve &curve, const Vx2DVector &in, const Vx2DVector &out) {
    for (int i = 0; i < curve.GetControlPointCount(); ++i) {
        CK2dCurvePoint *point = curve.GetControlPoint(i);
        point->UseTCB(FALSE);
        point->GetInTangent() = in;
        point->GetOutTangent() = out;
    }
}

class TangentPointSnapshot : public CK2dCurvePoint {
public:
    explicit TangentPointSnapshot(const CK2dCurvePoint &point) : CK2dCurvePoint(point) {}
    Vx2DVector Fitted() const { return m_RCurvePos; }
};

class TangentCurveProbe : public CK2dCurve {
public:
    void Fit(float coefficient) { FittingCoef = coefficient; UpdatePointsAndTangents(); }
    void ClearPoints() { m_ControlPoints.Clear(); }
    void AppendDetachedPoint(const CK2dCurvePoint &point) { m_ControlPoints.PushBack(point); }
    void RebuildTangents() { UpdatePointsAndTangents(); }
};

TEST(CK2dCurveTangentsTest, DefaultConstructorBuildsAFiniteIdentityCurve) {
    CK2dCurve curve;
    EXPECT_NEAR(sqrtf(2.0f), curve.GetLength(), .00002f);
    ExpectCurveTangent(curve.GetControlPoint(0)->GetInTangent(), .0001f, .0001f);
    ExpectCurveTangent(curve.GetControlPoint(0)->GetOutTangent(), 1, 1);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetInTangent(), 1, 1);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetOutTangent(), .001f, .001f);
    Vx2DVector position;
    ASSERT_EQ(CK_OK, curve.GetPos(.25f, &position)); ExpectCurveTangent(position, .25f, .25f);
    EXPECT_NEAR(.75f, curve.GetY(.75f), .00002f);
}

TEST(CK2dCurveTangentsTest, FirstTCBPointReplacesEarlierTangentPayload) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *first = curve.GetControlPoint(0);
    first->GetInTangent() = Vx2DVector(8, 9); first->GetOutTangent() = Vx2DVector(6, 7);
    first->SetTension(.5f);
    EXPECT_TRUE(first->IsTCB());
    ExpectCurveTangent(first->GetInTangent(), .0001f, .0002f);
    ExpectCurveTangent(first->GetOutTangent(), .25f, .5f);
}

TEST(CK2dCurveTangentsTest, NeutralInteriorTCBUsesBothNoncollinearUnequalChords) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *middle = curve.GetControlPoint(1);
    middle->GetInTangent() = Vx2DVector(6, 7); middle->GetOutTangent() = Vx2DVector(8, 9);
    middle->SetBias(0);
    ExpectCurveTangent(middle->GetInTangent(), .25f, .0625f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, .1875f);
}

TEST(CK2dCurveTangentsTest, CombinedInteriorTCBMatchesIndependentRationalResults) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *middle = curve.GetControlPoint(1);
    middle->SetTension(.5f); middle->SetContinuity(.5f); middle->SetBias(-.5f);
    // Raw in=(7/16,-7/64), out=(3/16,3/64); scaled to chord X widths.
    ExpectCurveTangent(middle->GetInTangent(), .25f, -.0625f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, .1875f);
}

TEST(CK2dCurveTangentsTest, PositiveContinuitySeparatesIncomingAndOutgoingChordSlopes) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *middle = curve.GetControlPoint(1); middle->SetContinuity(1);
    ExpectCurveTangent(middle->GetInTangent(), .25f, -1.0f/12.0f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, 1.5f);
}

TEST(CK2dCurveTangentsTest, NegativeContinuityReversesTheTwoInteriorChordSelections) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *middle = curve.GetControlPoint(1); middle->SetContinuity(-1);
    ExpectCurveTangent(middle->GetInTangent(), .25f, .5f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, -.25f);
}

TEST(CK2dCurveTangentsTest, BiasExtremesSelectThePreviousOrNextChord) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *middle = curve.GetControlPoint(1); middle->SetBias(1);
    ExpectCurveTangent(middle->GetInTangent(), .25f, .5f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, 1.5f);
    middle->SetBias(-1);
    ExpectCurveTangent(middle->GetInTangent(), .25f, -1.0f/12.0f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, -.25f);
}

TEST(CK2dCurveTangentsTest, LastTCBPointReplacesPayloadAndNormalizesBothEndpointTangents) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *last = curve.GetControlPoint(2);
    last->GetInTangent() = Vx2DVector(6, 7); last->GetOutTangent() = Vx2DVector(8, 9);
    last->SetTension(.5f);
    ExpectCurveTangent(last->GetInTangent(), .75f, -.25f);
    ExpectCurveTangent(last->GetOutTangent(), .001f, -.001f/3.0f);
}

TEST(CK2dCurveTangentsTest, EndpointContinuityExtremesKeepTheCorrectZeroTangent) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    CK2dCurvePoint *first = curve.GetControlPoint(0), *last = curve.GetControlPoint(2);
    first->SetContinuity(1); last->SetContinuity(1);
    ExpectCurveTangent(first->GetInTangent(), .0001f, .0002f);
    ExpectCurveTangent(first->GetOutTangent(), 0, 0);
    ExpectCurveTangent(last->GetInTangent(), 0, 0);
    ExpectCurveTangent(last->GetOutTangent(), .001f, -.001f/3.0f);
    first->SetContinuity(-1); last->SetContinuity(-1);
    ExpectCurveTangent(first->GetInTangent(), 0, 0);
    ExpectCurveTangent(first->GetOutTangent(), .25f, .5f);
    ExpectCurveTangent(last->GetInTangent(), .75f, -.25f);
    ExpectCurveTangent(last->GetOutTangent(), 0, 0);
}

TEST(CK2dCurveTangentsTest, EndpointBiasExtremesCanProduceZeroWithoutDivision) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    curve.GetControlPoint(0)->SetBias(1); curve.GetControlPoint(2)->SetBias(-1);
    for (int i = 0; i <= 2; i += 2) {
        ExpectCurveTangent(curve.GetControlPoint(i)->GetInTangent(), 0, 0);
        ExpectCurveTangent(curve.GetControlPoint(i)->GetOutTangent(), 0, 0);
    }
}

TEST(CK2dCurveTangentsTest, UnitTensionProducesFiniteZeroTangentsAtEveryPoint) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    for (int i = 0; i < 3; ++i) curve.GetControlPoint(i)->SetTension(1);
    for (int i = 0; i < 3; ++i) {
        ExpectCurveTangent(curve.GetControlPoint(i)->GetInTangent(), 0, 0);
        ExpectCurveTangent(curve.GetControlPoint(i)->GetOutTangent(), 0, 0);
    }
    EXPECT_TRUE(std::isfinite(curve.GetLength()));
}

TEST(CK2dCurveTangentsTest, ExplicitTangentsKeepTheirSlopesAndUseNativeEndpointWidths) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(2, 4), Vx2DVector(-2, 1)); curve.Update();
    ExpectCurveTangent(curve.GetControlPoint(0)->GetInTangent(), .0001f, .0002f);
    ExpectCurveTangent(curve.GetControlPoint(0)->GetOutTangent(), .25f, -.125f);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetInTangent(), .25f, .5f);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetOutTangent(), .75f, -.375f);
    ExpectCurveTangent(curve.GetControlPoint(2)->GetInTangent(), .75f, 1.5f);
    ExpectCurveTangent(curve.GetControlPoint(2)->GetOutTangent(), .001f, -.0005f);
}

TEST(CK2dCurveTangentsTest, ExplicitModeIgnoresTCBParameterEdits) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(2, 4), Vx2DVector(4, -2)); curve.Update();
    CK2dCurvePoint *middle = curve.GetControlPoint(1);
    middle->SetTension(1); middle->SetBias(1); middle->SetContinuity(-1);
    EXPECT_FALSE(middle->IsTCB());
    ExpectCurveTangent(middle->GetInTangent(), .25f, .5f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, -.375f);
}

TEST(CK2dCurveTangentsTest, PublicModeSwitchRegeneratesTCBInsteadOfKeepingExplicitData) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(2, 4), Vx2DVector(4, -2)); curve.Update();
    CK2dCurvePoint *middle = curve.GetControlPoint(1); middle->UseTCB(TRUE); curve.Update();
    ExpectCurveTangent(middle->GetInTangent(), .25f, .0625f);
    ExpectCurveTangent(middle->GetOutTangent(), .75f, .1875f);
}

TEST(CK2dCurveTangentsTest, ZeroXExplicitTangentsRemainVerticalAndFiniteAtEveryPoint) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(0, 2), Vx2DVector(0, -3)); curve.Update();
    for (int i = 0; i < 3; ++i) {
        ExpectCurveTangent(curve.GetControlPoint(i)->GetInTangent(), 0, 2);
        ExpectCurveTangent(curve.GetControlPoint(i)->GetOutTangent(), 0, -3);
    }
    EXPECT_TRUE(std::isfinite(curve.GetLength())); EXPECT_TRUE(std::isfinite(curve.GetY(.5f)));
}

TEST(CK2dCurveTangentsTest, SubEpsilonNonzeroTangentsAreRescaledRatherThanDiscarded) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(1e-8f, 2e-8f), Vx2DVector(-1e-8f, -2e-8f)); curve.Update();
    ExpectCurveTangent(curve.GetControlPoint(0)->GetInTangent(), .0001f, .0002f);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetInTangent(), .25f, .5f);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetOutTangent(), .75f, 1.5f);
    ExpectCurveTangent(curve.GetControlPoint(2)->GetOutTangent(), .001f, .002f);
}

TEST(CK2dCurveTangentsTest, SmallNonzeroChordWidthsAreNotReplacedByEpsilon) {
    CK2dCurve curve;
    curve.AddControlPoint(Vx2DVector(1e-8f, 1e-8f));
    EXPECT_FLOAT_EQ(1e-8f, curve.GetControlPoint(0)->GetOutTangent().x);
    EXPECT_FLOAT_EQ(1e-8f, curve.GetControlPoint(1)->GetInTangent().x);
}

TEST(CK2dCurveTangentsTest, ZeroChordWidthsUseTheNativeFallbackWithoutChangingSlope) {
    CK2dCurve curve;
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(0, 1));
    ExplicitCurveTangents(curve, Vx2DVector(2, 4), Vx2DVector(4, 8)); curve.Update();
    for (int i = 0; i < 2; ++i) ExpectCurveTangent(curve.GetControlPoint(i)->GetInTangent(), .0001f, .0002f);
    ExpectCurveTangent(curve.GetControlPoint(0)->GetOutTangent(), .0001f, .0002f);
    ExpectCurveTangent(curve.GetControlPoint(1)->GetOutTangent(), .001f, .002f);
}

TEST(CK2dCurveTangentsTest, RepeatedUpdatesAreIdempotentForExplicitAndTCBModes) {
    CK2dCurve curve; ThreeCurvePoints(curve);
    ExplicitCurveTangents(curve, Vx2DVector(2, 4), Vx2DVector(4, -2));
    curve.GetControlPoint(1)->UseTCB(TRUE); curve.Update();
    Vx2DVector in[3], out[3];
    for (int i = 0; i < 3; ++i) { in[i] = curve.GetControlPoint(i)->GetInTangent(); out[i] = curve.GetControlPoint(i)->GetOutTangent(); }
    for (int repeat = 0; repeat < 8; ++repeat) curve.Update();
    for (int i = 0; i < 3; ++i) {
        ExpectCurveTangent(curve.GetControlPoint(i)->GetInTangent(), in[i].x, in[i].y);
        ExpectCurveTangent(curve.GetControlPoint(i)->GetOutTangent(), out[i].x, out[i].y);
    }
}

TEST(CK2dCurveTangentsTest, FittedPositionsFollowEndpointAndInteriorNeighborMidpoints) {
    TangentCurveProbe curve; ThreeCurvePoints(curve); curve.Fit(.5f);
    ExpectCurveTangent(TangentPointSnapshot(*curve.GetControlPoint(0)).Fitted(), .0625f, .125f);
    ExpectCurveTangent(TangentPointSnapshot(*curve.GetControlPoint(1)).Fitted(), .375f, .3125f);
    ExpectCurveTangent(TangentPointSnapshot(*curve.GetControlPoint(2)).Fitted(), .8125f, .3125f);
}

TEST(CK2dCurveTangentsTest, TangentOnlyUpdateLeavesEmptyAndSinglePointCurvesAlone) {
    TangentCurveProbe curve; curve.ClearPoints(); curve.RebuildTangents();
    CK2dCurvePoint detached;
    detached.GetPosition() = Vx2DVector(.25f, .5f);
    detached.GetInTangent() = Vx2DVector(2, 3); detached.GetOutTangent() = Vx2DVector(4, 5);
    curve.AppendDetachedPoint(detached); curve.RebuildTangents();
    ExpectCurveTangent(curve.GetControlPoint(0)->GetInTangent(), 2, 3);
    ExpectCurveTangent(curve.GetControlPoint(0)->GetOutTangent(), 4, 5);
}
