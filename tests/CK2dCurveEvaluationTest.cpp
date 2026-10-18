#include <gtest/gtest.h>
#include <cmath>

// Compile the real implementation against the fixed SDK without importing
// these two classes from the SDK DLL. Other CK2 services remain SDK imports.
#define CK_LIB
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

class EvaluationPoint : public CK2dCurvePoint {
public:
    EvaluationPoint(float x, float y, float length, CKBOOL linear,
                    float outX = 1, float outY = 1, float inX = 1, float inY = 1) {
        m_Position = Vx2DVector(x, y);
        m_RCurvePos = m_Position;
        m_Length = length;
        m_InTang = Vx2DVector(inX, inY);
        m_OutTang = Vx2DVector(outX, outY);
        m_Flags = CK2DCURVEPOINT_USETANGENTS | (linear ? CK2DCURVEPOINT_LINEAR : 0);
    }
};

class EvaluationCurve : public CK2dCurve {
public:
    EvaluationCurve() { m_ControlPoints.Clear(); m_Length = 0; }
    void Append(const EvaluationPoint &point) {
        m_ControlPoints.PushBack(point);
        m_Length = point.GetLength();
    }
    void ReserveUnrelatedTail() {
        // A removed point must not influence evaluation of the logical curve.
        // Keep its storage available so the old one-past read is deterministic.
        m_ControlPoints.PushBack(EvaluationPoint(NAN, NAN, -1, FALSE, NAN, NAN, NAN, NAN));
        m_ControlPoints.PopBack();
    }
};

static void ExpectCurvePosition(CK2dCurve &curve, float step, float x, float y) {
    Vx2DVector result(-10, -20);
    ASSERT_EQ(CK_OK, curve.GetPos(step, &result));
    EXPECT_NEAR(x, result.x, 0.00001f);
    EXPECT_NEAR(y, result.y, 0.00001f);
}

TEST(CK2dCurveEvaluationTest, NullOutputAndFewerThanTwoPointsRejectWithoutChangingOutput) {
    EvaluationCurve curve;
    Vx2DVector result(7, 9);
    EXPECT_EQ(CKERR_INVALIDPARAMETER, curve.GetPos(.5f, &result));
    curve.Append(EvaluationPoint(.3f, .4f, 0, TRUE));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, curve.GetPos(.5f, &result));
    curve.Append(EvaluationPoint(1, 1, 2, TRUE));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, curve.GetPos(.5f, nullptr));
    EXPECT_FLOAT_EQ(7, result.x); EXPECT_FLOAT_EQ(9, result.y);
}

TEST(CK2dCurveEvaluationTest, LinearPositionUsesStoredLengthAndClampsTheStep) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(.1f, .2f, 0, TRUE));
    curve.Append(EvaluationPoint(.9f, .6f, 4, TRUE));
    ExpectCurvePosition(curve, -2, .1f, .2f);
    ExpectCurvePosition(curve, .25f, .3f, .3f);
    ExpectCurvePosition(curve, .75f, .7f, .5f);
}

TEST(CK2dCurveEvaluationTest, FinalPositionIgnoresRemovedPointStorageAndIncludesStepOne) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, TRUE));
    curve.Append(EvaluationPoint(.8f, .6f, 1, FALSE));
    curve.ReserveUnrelatedTail();
    ExpectCurvePosition(curve, 1, .8f, .6f);
    ExpectCurvePosition(curve, 8, .8f, .6f);
}

TEST(CK2dCurveEvaluationTest, ZeroLengthCurveHasADefinedPosition) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(.3f, .4f, 0, FALSE));
    curve.Append(EvaluationPoint(.3f, .4f, 0, FALSE));
    curve.ReserveUnrelatedTail();
    ExpectCurvePosition(curve, 0, .3f, .4f);
    ExpectCurvePosition(curve, .5f, .3f, .4f);
    ExpectCurvePosition(curve, 1, .3f, .4f);
}

TEST(CK2dCurveEvaluationTest, UnequalSegmentLengthsSelectTheRightLocalFraction) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, TRUE));
    curve.Append(EvaluationPoint(.2f, .6f, 2, TRUE));
    curve.Append(EvaluationPoint(1, .8f, 8, TRUE));
    ExpectCurvePosition(curve, .125f, .1f, .3f);
    ExpectCurvePosition(curve, .25f, .2f, .6f);
    ExpectCurvePosition(curve, .5f, 7.0f/15.0f, 2.0f/3.0f);
}

TEST(CK2dCurveEvaluationTest, RepeatedLengthsSkipTheZeroLengthLeadingSegment) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, TRUE));
    curve.Append(EvaluationPoint(0, 0, 0, TRUE));
    curve.Append(EvaluationPoint(1, .5f, 2, TRUE));
    ExpectCurvePosition(curve, 0, 0, 0);
    ExpectCurvePosition(curve, .5f, .5f, .25f);
}

TEST(CK2dCurveEvaluationTest, CubicPositionUsesStartOutgoingAndEndIncomingTangents) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 1, 0, 9, 8));
    curve.Append(EvaluationPoint(1, 1, 3, FALSE, 7, 6, 1, 2));
    // Analytic curve: x=t, y=t*t. Unused tangents deliberately differ.
    ExpectCurvePosition(curve, .25f, .25f, .0625f);
    ExpectCurvePosition(curve, .5f, .5f, .25f);
    ExpectCurvePosition(curve, .75f, .75f, .5625f);
}

TEST(CK2dCurveEvaluationTest, PositionDoesNotClampCubicOvershootToTheUnitSquare) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 1, -4));
    curve.Append(EvaluationPoint(1, 0, 2, FALSE, 1, 0, 1, 4));
    ExpectCurvePosition(curve, .5f, .5f, -1);
}

TEST(CK2dCurveEvaluationTest, PublicLinearCurveUpdateFeedsPositionAndHeightEvaluation) {
    CK2dCurve curve;
    curve.GetControlPoint(0)->SetLinear(TRUE);
    curve.GetControlPoint(1)->SetPosition(Vx2DVector(1.0f, .5f));
    ExpectCurvePosition(curve, .5f, .5f, .25f);
    EXPECT_NEAR(.25f, curve.GetY(.5f), .00001f);
}

TEST(CK2dCurveEvaluationTest, HeightForFewerThanTwoPointsIsZero) {
    EvaluationCurve curve;
    EXPECT_FLOAT_EQ(0, curve.GetY(.5f));
    curve.Append(EvaluationPoint(.2f, .4f, 0, TRUE));
    EXPECT_FLOAT_EQ(0, curve.GetY(.5f));
}

TEST(CK2dCurveEvaluationTest, HeightClampsTheInputAndInterpolatesLinearSegments) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, .2f, 0, TRUE));
    curve.Append(EvaluationPoint(1, .6f, 1, TRUE));
    EXPECT_NEAR(.2f, curve.GetY(-2), .00001f);
    EXPECT_NEAR(.3f, curve.GetY(.25f), .00001f);
    EXPECT_NEAR(.6f, curve.GetY(2), .00001f);
}

TEST(CK2dCurveEvaluationTest, HeightBeforeTheFirstControlPointUsesItsClampedValue) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(.2f, .4f, 0, TRUE));
    curve.Append(EvaluationPoint(.8f, .9f, 1, TRUE));
    EXPECT_FLOAT_EQ(.4f, curve.GetY(0));
    EXPECT_FLOAT_EQ(.4f, curve.GetY(.1f));
}

TEST(CK2dCurveEvaluationTest, HeightBeyondTheLastControlPointReturnsZeroWithoutReadingTheTail) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(.2f, .4f, 0, TRUE));
    curve.Append(EvaluationPoint(.8f, .9f, 1, TRUE));
    curve.ReserveUnrelatedTail();
    EXPECT_FLOAT_EQ(0, curve.GetY(.95f));
    EXPECT_FLOAT_EQ(0, curve.GetY(4));
}

TEST(CK2dCurveEvaluationTest, HeightClampsExactAndLinearResultsAsTheNativeEntryDoes) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, -1, 0, TRUE));
    curve.Append(EvaluationPoint(1, 2, 4, TRUE));
    EXPECT_FLOAT_EQ(0, curve.GetY(0));
    EXPECT_FLOAT_EQ(0, curve.GetY(.25f));
    EXPECT_FLOAT_EQ(.5f, curve.GetY(.5f));
    EXPECT_FLOAT_EQ(1, curve.GetY(.75f));
    EXPECT_FLOAT_EQ(1, curve.GetY(1));
}

TEST(CK2dCurveEvaluationTest, HeightSolvesAnAnalyticCubicWithLinearX) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 1, 0));
    curve.Append(EvaluationPoint(1, 1, 2, FALSE, 1, 1, 1, 2));
    EXPECT_NEAR(.0625f, curve.GetY(.25f), .00002f);
    EXPECT_NEAR(.25f, curve.GetY(.5f), .00002f);
    EXPECT_NEAR(.5625f, curve.GetY(.75f), .00002f);
}

TEST(CK2dCurveEvaluationTest, HeightInvertsNonlinearXInsteadOfUsingTheXAsParameter) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 0, 1));
    curve.Append(EvaluationPoint(1, 1, 2, FALSE, 1, 1, 2, 1));
    // x=t*t, y=t.
    EXPECT_NEAR(.25f, curve.GetY(.0625f), .00004f);
    EXPECT_NEAR(.5f, curve.GetY(.25f), .00004f);
    EXPECT_NEAR(.75f, curve.GetY(.5625f), .00004f);
}

TEST(CK2dCurveEvaluationTest, HeightClampsCubicOvershoot) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 1, -4));
    curve.Append(EvaluationPoint(1, 0, 2, FALSE, 1, 0, 1, 4));
    EXPECT_FLOAT_EQ(0, curve.GetY(.5f));
}

TEST(CK2dCurveEvaluationTest, ExactInteriorKnotUsesItsStoredHeight) {
    EvaluationCurve curve;
    curve.Append(EvaluationPoint(0, 0, 0, FALSE, 1, 5));
    curve.Append(EvaluationPoint(.25f, .7f, 1, FALSE, 1, -6, 1, -4));
    curve.Append(EvaluationPoint(1, .3f, 3, TRUE));
    EXPECT_FLOAT_EQ(.7f, curve.GetY(.25f));
}
