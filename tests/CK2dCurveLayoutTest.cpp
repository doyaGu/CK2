#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#define CK_LIB
#include "../include/CK2dCurve.h"
#include "../src/CK2dCurve.cpp"
#include "../src/CK2dCurvePoint.cpp"

static void *TrackedCurveArray;
static int TrackedCurveArrayDeletes;

void *operator new[](size_t size) {
    void *memory = malloc(size ? size : 1);
    if (!memory) throw std::bad_alloc();
    return memory;
}

void operator delete[](void *memory) noexcept {
    if (memory && memory == TrackedCurveArray) ++TrackedCurveArrayDeletes;
    free(memory);
}

void operator delete[](void *memory, size_t) noexcept { operator delete[](memory); }

class CurveLayoutProbe : public CK2dCurve {
public:
    size_t ArrayOffset() const { return (const char *)&m_ControlPoints - (const char *)this; }
    size_t LengthOffset() const { return (const char *)&m_Length - (const char *)this; }
    size_t FitOffset() const { return (const char *)&FittingCoef - (const char *)this; }
    int Capacity() const { return m_ControlPoints.Allocated(); }
    void SetFit(float value) { FittingCoef = value; }
    float Fit() const { return FittingCoef; }
    float CachedLength() const { return m_Length; }
    void SetCachedLength(float value) { m_Length = value; }
    void SetRecord(int index, const CK2dCurvePoint &point) { m_ControlPoints[index] = point; }
};

class CurveLayoutPoint : public CK2dCurvePoint {
public:
    CurveLayoutPoint(CK2dCurve *owner, float x, float y, float identity) {
        m_Curve = owner; m_Position.Set(x, y); m_Flags = 3;
        m_Tension = identity; m_Continuity = -.25f; m_Bias = .5f; m_Length = 17;
        m_InTang.Set(.5f, .25f); m_OutTang.Set(.5f, -.25f); m_RCurvePos.Set(9, 8);
    }
    explicit CurveLayoutPoint(const CK2dCurvePoint &point) : CK2dCurvePoint(point) {}
    Vx2DVector Fitted() const { return m_RCurvePos; }
    CKDWORD Flags() const { return m_Flags; }
};

static void MakeLayoutCurve(CurveLayoutProbe &curve) {
    curve.AddControlPoint(Vx2DVector(.25f, .75f));
    curve.SetRecord(0, CurveLayoutPoint(&curve, 0, 0, .125f));
    curve.SetRecord(1, CurveLayoutPoint(&curve, .25f, .75f, .25f));
    curve.SetRecord(2, CurveLayoutPoint(&curve, 1, .5f, .5f));
    curve.SetFit(.5f); curve.Update();
}

static void ExpectOwnedLayoutPoints(CK2dCurve &curve) {
    for (int i = 0; i < curve.GetControlPointCount(); ++i) EXPECT_EQ(&curve, curve.GetControlPoint(i)->GetCurve());
}

static void ExpectLayoutPosition(CK2dCurve &curve, int index, float x, float y) {
    EXPECT_FLOAT_EQ(x, curve.GetControlPoint(index)->GetPosition().x);
    EXPECT_FLOAT_EQ(y, curve.GetControlPoint(index)->GetPosition().y);
}

TEST(CK2dCurveLayoutTest, ClassHasTheNativeWin32OrPointerSizedX64Layout) {
    CurveLayoutProbe curve;
    EXPECT_FALSE((bool)__is_polymorphic(CK2dCurve)); EXPECT_FALSE((bool)__is_base_of(CKObject, CK2dCurve));
    EXPECT_EQ(0u, curve.ArrayOffset());
    EXPECT_EQ(sizeof(void *) == 4 ? 12u : 24u, curve.LengthOffset());
    EXPECT_EQ(sizeof(void *) == 4 ? 16u : 28u, curve.FitOffset());
    EXPECT_EQ(sizeof(void *) == 4 ? 20u : 32u, sizeof(CK2dCurve));
    EXPECT_EQ(sizeof(void *), alignof(CK2dCurve));
    EXPECT_EQ(3 * sizeof(void *), sizeof(XArray<CK2dCurvePoint>));
}

TEST(CK2dCurveLayoutTest, ThreePointerHeaderMatchesTheLiveOwnedPointRangeAndCapacity) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); void *words[3]; memcpy(words, &curve, sizeof(words));
    EXPECT_EQ(curve.GetControlPoint(0), words[0]);
    EXPECT_EQ((char *)words[0] + curve.GetControlPointCount() * sizeof(CK2dCurvePoint), words[1]);
    EXPECT_EQ((char *)words[0] + curve.Capacity() * sizeof(CK2dCurvePoint), words[2]);
    EXPECT_EQ(sizeof(void *) == 4 ? 56u : 64u, sizeof(CK2dCurvePoint));
    EXPECT_EQ(sizeof(CK2dCurvePoint), (size_t)((char *)curve.GetControlPoint(1) - (char *)curve.GetControlPoint(0)));
}

TEST(CK2dCurveLayoutTest, LengthAndFitAreSeparateFloatSlotsUsedByRealMethods) {
    CurveLayoutProbe curve; float length = 12.5f, fit = .375f;
    memcpy((char *)&curve + 3 * sizeof(void *), &length, sizeof(length));
    memcpy((char *)&curve + 3 * sizeof(void *) + sizeof(float), &fit, sizeof(fit));
    EXPECT_FLOAT_EQ(12.5f, curve.GetLength()); EXPECT_FLOAT_EQ(.375f, curve.Fit());
    CKStateChunk *chunk = curve.Dump(); chunk->StartRead(); ASSERT_TRUE(chunk->SeekIdentifier(0x100));
    chunk->ReadXObjectArray(); EXPECT_FLOAT_EQ(.375f, chunk->ReadFloat()); DeleteCKStateChunk(chunk);
    curve.Update(); EXPECT_NE(12.5f, curve.GetLength()); EXPECT_FLOAT_EQ(.375f, curve.Fit());
}

TEST(CK2dCurveLayoutTest, GrowthAndSortingKeepWholePointRecordsAndOwners) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve);
    curve.AddControlPoint(Vx2DVector(.875f, .25f)); curve.AddControlPoint(Vx2DVector(.125f, .5f));
    ASSERT_EQ(5, curve.GetControlPointCount()); ExpectOwnedLayoutPoints(curve);
    EXPECT_FLOAT_EQ(.25f, curve.GetControlPoint(2)->GetTension());
    EXPECT_FLOAT_EQ(.5f, curve.GetControlPoint(4)->GetTension());
    EXPECT_GE(curve.Capacity(), 5);
}

TEST(CK2dCurveLayoutTest, FitMovesEndpointsTowardTheirAdjacentMidpointAndInteriorTowardNeighborMean) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve);
    Vx2DVector first = CurveLayoutPoint(*curve.GetControlPoint(0)).Fitted();
    Vx2DVector middle = CurveLayoutPoint(*curve.GetControlPoint(1)).Fitted();
    Vx2DVector last = CurveLayoutPoint(*curve.GetControlPoint(2)).Fitted();
    EXPECT_FLOAT_EQ(.0625f, first.x); EXPECT_FLOAT_EQ(.1875f, first.y);
    EXPECT_FLOAT_EQ(.375f, middle.x); EXPECT_FLOAT_EQ(.5f, middle.y);
    EXPECT_FLOAT_EQ(.8125f, last.x); EXPECT_FLOAT_EQ(.5625f, last.y);
    ExpectLayoutPosition(curve, 1, .25f, .75f);
}

TEST(CK2dCurveLayoutTest, ZeroFitLeavesFittedPositionsAtTheControlPoints) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); curve.SetFit(0); curve.Update();
    for (int i = 0; i < 3; ++i) {
        Vx2DVector fitted = CurveLayoutPoint(*curve.GetControlPoint(i)).Fitted();
        EXPECT_FLOAT_EQ(curve.GetControlPoint(i)->GetPosition().x, fitted.x);
        EXPECT_FLOAT_EQ(curve.GetControlPoint(i)->GetPosition().y, fitted.y);
    }
}

TEST(CK2dCurveLayoutTest, FitOnlyChangesTheFittedCacheAndLeavesNativeEvaluationAndLengthUnchanged) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve);
    // The original evaluates raw positions, even though it also publishes a fitted cache.
    EXPECT_NEAR(1.58113883f, curve.GetLength(), .000003f);
    Vx2DVector pos; ASSERT_EQ(CK_OK, curve.GetPos(.5f, &pos));
    EXPECT_NEAR(.25f, pos.x, .000003f); EXPECT_NEAR(.75f, pos.y, .000003f);
    ExpectLayoutPosition(curve, 0, 0, 0); ExpectLayoutPosition(curve, 2, 1, .5f);
}

TEST(CK2dCurveLayoutTest, FiniteFitIsUsedAsStoredRatherThanSilentlyClamped) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); curve.SetFit(2); curve.Update();
    Vx2DVector first = CurveLayoutPoint(*curve.GetControlPoint(0)).Fitted();
    EXPECT_FLOAT_EQ(.25f, first.x); EXPECT_FLOAT_EQ(.75f, first.y); EXPECT_FLOAT_EQ(2, curve.Fit());
}

TEST(CK2dCurveLayoutTest, LengthIsDerivedPrefixTotalAndIsNotACapacityOrPointCount) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); curve.SetFit(0); curve.Update();
    EXPECT_NEAR(1.58113883f, curve.GetLength(), .000003f);
    EXPECT_FLOAT_EQ(0, curve.GetControlPoint(0)->GetLength());
    EXPECT_NEAR(.790569415f, curve.GetControlPoint(1)->GetLength(), .000003f);
    EXPECT_FLOAT_EQ(curve.GetLength(), curve.GetControlPoint(2)->GetLength());
    curve.DeleteControlPoint(curve.GetControlPoint(1)); EXPECT_NEAR(sqrtf(1.25f), curve.GetLength(), .000003f);
}

TEST(CK2dCurveLayoutTest, EmptyStorageRetainsCapacityButHasZeroLogicalCountAndTotal) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); const int capacity = curve.Capacity();
    while (curve.GetControlPointCount()) curve.DeleteControlPoint(curve.GetControlPoint(0));
    EXPECT_EQ(0, curve.GetControlPointCount()); EXPECT_EQ(capacity, curve.Capacity()); EXPECT_FLOAT_EQ(0, curve.GetLength());
    void *words[3]; memcpy(words, &curve, sizeof(words)); EXPECT_EQ(words[0], words[1]); EXPECT_NE(words[0], words[2]);
}

TEST(CK2dCurveLayoutTest, CopyConstructionOwnsIndependentStorageAndRebindsEveryPoint) {
    CurveLayoutProbe source; MakeLayoutCurve(source); CurveLayoutProbe copy(source);
    ASSERT_EQ(3, copy.GetControlPointCount()); ASSERT_NE(source.GetControlPoint(0), copy.GetControlPoint(0));
    ExpectOwnedLayoutPoints(copy); ExpectOwnedLayoutPoints(source);
    EXPECT_FLOAT_EQ(source.GetLength(), copy.GetLength()); EXPECT_FLOAT_EQ(.5f, copy.Fit());
    EXPECT_FLOAT_EQ(.25f, copy.GetControlPoint(1)->GetTension());
}

TEST(CK2dCurveLayoutTest, CopyConstructionPublishesTheStoredCachesWithoutAnImplicitUpdate) {
    CurveLayoutProbe source; MakeLayoutCurve(source); source.SetCachedLength(27);
    CurveLayoutProbe copy(source); EXPECT_FLOAT_EQ(27, copy.GetLength()); EXPECT_FLOAT_EQ(.5f, copy.Fit());
    EXPECT_FLOAT_EQ(source.GetControlPoint(1)->GetLength(), copy.GetControlPoint(1)->GetLength());
}

TEST(CK2dCurveLayoutTest, EditingACopyNotifiesTheCopyAndLeavesTheSourceUntouched) {
    CurveLayoutProbe source; MakeLayoutCurve(source); source.SetFit(0); source.Update(); CurveLayoutProbe copy(source);
    copy.GetControlPoint(1)->SetPosition(Vx2DVector(.25f, .25f));
    EXPECT_NEAR(1.14412281f, copy.GetLength(), .000003f);
    EXPECT_NEAR(1.58113883f, source.GetLength(), .000003f); ExpectLayoutPosition(source, 1, .25f, .75f);
}

TEST(CK2dCurveLayoutTest, CopyAssignmentReusesStorageWhenPossibleAndRebindsEveryPoint) {
    CurveLayoutProbe source, destination; MakeLayoutCurve(source); MakeLayoutCurve(destination);
    CK2dCurvePoint *storage = destination.GetControlPoint(0); CK2dCurve &result = (destination = source);
    EXPECT_EQ(&destination, &result); EXPECT_EQ(storage, destination.GetControlPoint(0)); ExpectOwnedLayoutPoints(destination);
    EXPECT_FLOAT_EQ(source.GetLength(), destination.GetLength()); EXPECT_FLOAT_EQ(.5f, destination.Fit());
}

TEST(CK2dCurveLayoutTest, CopyAssignmentGrowsDestinationAndDoesNotRetainItsOldOwner) {
    CurveLayoutProbe source, destination; MakeLayoutCurve(source);
    for (int i = 1; i < 8; ++i) source.AddControlPoint(Vx2DVector(i / 8.0f, .25f));
    destination = source; ASSERT_EQ(source.GetControlPointCount(), destination.GetControlPointCount());
    ASSERT_NE(source.GetControlPoint(0), destination.GetControlPoint(0)); ExpectOwnedLayoutPoints(destination);
}

TEST(CK2dCurveLayoutTest, AssignmentFromAnEmptyCurveClearsLogicalStateAndCopiesFit) {
    CurveLayoutProbe source, destination; source.SetFit(.375f);
    while (source.GetControlPointCount()) source.DeleteControlPoint(source.GetControlPoint(0));
    destination = source; EXPECT_EQ(0, destination.GetControlPointCount());
    EXPECT_FLOAT_EQ(0, destination.GetLength()); EXPECT_FLOAT_EQ(.375f, destination.Fit());
    destination.AddControlPoint(Vx2DVector(.5f, .25f)); ExpectOwnedLayoutPoints(destination);
}

TEST(CK2dCurveLayoutTest, SelfAssignmentPreservesStorageOwnershipParametersAndCache) {
    CurveLayoutProbe curve; MakeLayoutCurve(curve); CK2dCurvePoint *storage = curve.GetControlPoint(0);
    const float length = curve.GetLength(); CK2dCurve &alias = curve; alias = alias;
    EXPECT_EQ(storage, curve.GetControlPoint(0)); ExpectOwnedLayoutPoints(curve);
    EXPECT_FLOAT_EQ(length, curve.GetLength()); EXPECT_FLOAT_EQ(.5f, curve.Fit());
}

TEST(CK2dCurveLayoutTest, ChainedAssignmentsBindEachDestinationIndependently) {
    CurveLayoutProbe source, first, second; MakeLayoutCurve(source);
    first = second = source; ExpectOwnedLayoutPoints(first); ExpectOwnedLayoutPoints(second);
    EXPECT_NE(first.GetControlPoint(0), second.GetControlPoint(0));
}

TEST(CK2dCurveLayoutTest, RvalueConstructionKeepsPointOwnersAndTheSourceStateCoherent) {
    CurveLayoutProbe source; MakeLayoutCurve(source); const int originalCount = source.GetControlPointCount();
    CK2dCurve destination(static_cast<CK2dCurve &&>(source)); ExpectOwnedLayoutPoints(destination);
    EXPECT_EQ(originalCount, destination.GetControlPointCount());
    if (source.GetControlPointCount() == 0) EXPECT_FLOAT_EQ(0, source.GetLength());
    else { EXPECT_EQ(originalCount, source.GetControlPointCount()); ExpectOwnedLayoutPoints(source); }
}

TEST(CK2dCurveLayoutTest, RvalueAssignmentKeepsPointOwnersAndTheSourceStateCoherent) {
    CurveLayoutProbe source; MakeLayoutCurve(source); CK2dCurve destination;
    destination = static_cast<CK2dCurve &&>(source); ExpectOwnedLayoutPoints(destination);
    EXPECT_EQ(3, destination.GetControlPointCount());
    if (source.GetControlPointCount() == 0) EXPECT_FLOAT_EQ(0, source.GetLength());
    else ExpectOwnedLayoutPoints(source);
}

TEST(CK2dCurveLayoutTest, CopySurvivesSourceDestructionAndSubsequentPointMutation) {
    CK2dCurve copy;
    {
        CurveLayoutProbe source; MakeLayoutCurve(source); source.SetFit(0); source.Update(); copy = source;
    }
    // Check ownership before calling a setter, to avoid deliberately dereferencing a stale owner in the old code.
    for (int i = 0; i < copy.GetControlPointCount(); ++i) ASSERT_EQ(&copy, copy.GetControlPoint(i)->GetCurve());
    copy.GetControlPoint(1)->SetPosition(Vx2DVector(.25f, .25f));
    EXPECT_NEAR(1.14412281f, copy.GetLength(), .000003f);
}

TEST(CK2dCurveLayoutTest, DestroyingOneCopyReleasesOnlyItsOwnPointArray) {
    CurveLayoutProbe source; MakeLayoutCurve(source); CurveLayoutProbe *copy = new CurveLayoutProbe(source);
    TrackedCurveArray = copy->GetControlPoint(0); TrackedCurveArrayDeletes = 0;
    delete copy; EXPECT_EQ(1, TrackedCurveArrayDeletes); TrackedCurveArray = nullptr;
    ExpectOwnedLayoutPoints(source); EXPECT_EQ(3, source.GetControlPointCount());
    EXPECT_FLOAT_EQ(.25f, source.GetControlPoint(1)->GetTension());
}

TEST(CK2dCurveLayoutTest, DestructorReleasesAnEmptiedCurvesReservedStorageExactlyOnce) {
    CurveLayoutProbe *curve = new CurveLayoutProbe; MakeLayoutCurve(*curve);
    TrackedCurveArray = curve->GetControlPoint(0); TrackedCurveArrayDeletes = 0;
    while (curve->GetControlPointCount()) curve->DeleteControlPoint(curve->GetControlPoint(0));
    EXPECT_EQ(0, TrackedCurveArrayDeletes); delete curve;
    EXPECT_EQ(1, TrackedCurveArrayDeletes); TrackedCurveArray = nullptr;
}

TEST(CK2dCurveLayoutTest, HeapArrayAndPlacementObjectsRetainTheirOwnPointOwners) {
    CK2dCurve *curves = new CK2dCurve[2]; ExpectOwnedLayoutPoints(curves[0]); ExpectOwnedLayoutPoints(curves[1]);
    EXPECT_NE(curves[0].GetControlPoint(0), curves[1].GetControlPoint(0)); delete[] curves;
    alignas(CK2dCurve) unsigned char storage[sizeof(CK2dCurve)]; memset(storage, 0xA5, sizeof(storage));
    CK2dCurve *placed = new (storage) CK2dCurve; ExpectOwnedLayoutPoints(*placed);
    EXPECT_NEAR(sqrtf(2.0f), placed->GetLength(), .000003f); placed->~CK2dCurve();
}
