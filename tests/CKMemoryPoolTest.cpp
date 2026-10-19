#include <gtest/gtest.h>
#include <stdint.h>

#ifndef CK_LIB
#define CK_LIB
#endif
#include "CKAll.h"
#include "../src/CKMemoryPool.cpp"

class MemoryPoolProbe : public CKMemoryPool {
public:
    explicit MemoryPoolProbe(CKContext *context, int count = 0) : CKMemoryPool(context, count) {}
    CKContext *Context() const { return m_Context; }
    int Index() const { return m_Index; }
    size_t Offset(int index) const {
        const void *fields[] = {&m_Context, &m_Memory, &m_Index};
        return static_cast<const char *>(fields[index]) - reinterpret_cast<const char *>(this);
    }
};

class CKMemoryPoolTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
    }
    void TearDown() override {
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
};

TEST_F(CKMemoryPoolTest, NullContextCreatesAnEmptyDetachedLease) {
    const int requests[] = {0, 1, 19, -1};
    for (int count : requests) {
        MemoryPoolProbe pool(nullptr, count);
        EXPECT_EQ(nullptr, pool.Context());
        EXPECT_EQ(nullptr, pool.Mem());
        EXPECT_EQ(-1, pool.Index());
    }
}

TEST_F(CKMemoryPoolTest, DefaultRequestReservesARealSlotWithoutAllocatingBuffer) {
    ASSERT_FALSE(Context->m_MemoryPoolMask.IsSet(0));
    {
        MemoryPoolProbe pool(Context);
        ASSERT_EQ(0, pool.Index());
        EXPECT_EQ(Context, pool.Context());
        EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(0));
        EXPECT_EQ(nullptr, pool.Mem());
        EXPECT_EQ(0u, Context->m_MemoryPools[0]->AllocatedSize());
    }
    EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(0));
}

TEST_F(CKMemoryPoolTest, LayoutMatchesNativeWin32SlotsAndPointerSizedX64Fields) {
    MemoryPoolProbe pool(nullptr);
    EXPECT_EQ(0u, pool.Offset(0));
    EXPECT_EQ(sizeof(void *), pool.Offset(1));
    EXPECT_EQ(2 * sizeof(void *), pool.Offset(2));
    EXPECT_EQ(sizeof(void *) == 4 ? 12u : 24u, sizeof(CKMemoryPool));
    EXPECT_EQ(sizeof(void *), alignof(CKMemoryPool));
    EXPECT_FALSE((bool)__is_polymorphic(CKMemoryPool));
    EXPECT_FALSE((bool)__is_base_of(CKObject, CKMemoryPool));
}

TEST_F(CKMemoryPoolTest, PositiveRequestForwardsNativeWordCountAndProvidesAlignedWritableStorage) {
    MemoryPoolProbe pool(Context, 19);
    ASSERT_NE(nullptr, pool.Mem());
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(pool.Mem()) & 15u);
    ASSERT_EQ(19u, Context->m_MemoryPools[pool.Index()]->AllocatedSize());
    unsigned char *bytes = static_cast<unsigned char *>(pool.Mem());
    for (int i = 0; i < 19 * 4; ++i) bytes[i] = static_cast<unsigned char>(i ^ 0x5a);
    for (int i = 0; i < 19 * 4; ++i) EXPECT_EQ(static_cast<unsigned char>(i ^ 0x5a), bytes[i]);
}

TEST_F(CKMemoryPoolTest, MemIsAConstStableViewWithoutChangingTheReservation) {
    MemoryPoolProbe pool(Context, 7);
    const CKMemoryPool &view = pool;
    void *buffer = view.Mem();
    int count = Context->m_MemoryPools.Size();
    ASSERT_NE(nullptr, buffer);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(buffer, view.Mem());
    EXPECT_EQ(count, Context->m_MemoryPools.Size());
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(pool.Index()));
}

TEST_F(CKMemoryPoolTest, NestedLeasesUseDifferentSlotsAndKeepIndependentContents) {
    MemoryPoolProbe outer(Context, 5);
    unsigned int *first = static_cast<unsigned int *>(outer.Mem());
    ASSERT_NE(nullptr, first);
    first[0] = 0x12345678u; first[4] = 0x90abcdefu;
    {
        MemoryPoolProbe inner(Context, 5);
        ASSERT_NE(outer.Index(), inner.Index());
        ASSERT_NE(first, inner.Mem());
        unsigned int *second = static_cast<unsigned int *>(inner.Mem());
        ASSERT_NE(nullptr, second);
        second[0] = 0xabcdef01u; second[4] = 0x23456789u;
        EXPECT_EQ(0x12345678u, first[0]); EXPECT_EQ(0x90abcdefu, first[4]);
    }
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(outer.Index()));
    EXPECT_EQ(0x12345678u, first[0]); EXPECT_EQ(0x90abcdefu, first[4]);
}

TEST_F(CKMemoryPoolTest, DestroyedLeaseReleasesOnlyItsOwnReservation) {
    MemoryPoolProbe outer(Context, 8);
    int innerIndex;
    {
        MemoryPoolProbe inner(Context, 8); innerIndex = inner.Index();
        ASSERT_TRUE(Context->m_MemoryPoolMask.IsSet(innerIndex));
    }
    EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(innerIndex));
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(outer.Index()));
    MemoryPoolProbe replacement(Context, 8);
    EXPECT_EQ(innerIndex, replacement.Index());
}

TEST_F(CKMemoryPoolTest, DestructionReturnsTheSlotButKeepsTheContextOwnedBufferAndCapacity) {
    VxMemoryPool *cache;
    void *buffer;
    int index;
    {
        MemoryPoolProbe pool(Context, 13); index = pool.Index(); buffer = pool.Mem();
        cache = Context->m_MemoryPools[index];
    }
    EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(index));
    ASSERT_EQ(cache, Context->m_MemoryPools[index]);
    EXPECT_EQ(buffer, cache->Buffer());
    EXPECT_EQ(13u, cache->AllocatedSize());
}

TEST_F(CKMemoryPoolTest, SmallerReusedRequestKeepsTheBufferCapacityAndWrittenBytes) {
    void *buffer;
    {
        MemoryPoolProbe pool(Context, 19); buffer = pool.Mem();
        ASSERT_NE(nullptr, buffer);
        unsigned int *words = static_cast<unsigned int *>(buffer);
        for (int i = 0; i < 19; ++i) words[i] = 0x5a000000u + i;
    }
    MemoryPoolProbe reused(Context, 3);
    ASSERT_EQ(buffer, reused.Mem());
    EXPECT_EQ(19u, Context->m_MemoryPools[reused.Index()]->AllocatedSize());
    for (int i = 0; i < 19; ++i) EXPECT_EQ(0x5a000000u + i, static_cast<unsigned int *>(reused.Mem())[i]);
}

TEST_F(CKMemoryPoolTest, ZeroSizeReusedRequestStillReservesAndReturnsTheCachedBuffer) {
    void *buffer;
    {
        MemoryPoolProbe pool(Context, 11); buffer = pool.Mem();
        ASSERT_NE(nullptr, buffer);
        static_cast<unsigned int *>(buffer)[10] = 0xabcdef01u;
    }
    MemoryPoolProbe reused(Context);
    ASSERT_EQ(buffer, reused.Mem());
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(reused.Index()));
    EXPECT_EQ(11u, Context->m_MemoryPools[reused.Index()]->AllocatedSize());
    EXPECT_EQ(0xabcdef01u, static_cast<unsigned int *>(reused.Mem())[10]);
}

TEST_F(CKMemoryPoolTest, GrowingAReusedRequestExpandsCapacityWithoutTouchingAnActiveNeighbor) {
    MemoryPoolProbe neighbor(Context, 7);
    unsigned int *words = static_cast<unsigned int *>(neighbor.Mem());
    ASSERT_NE(nullptr, words); words[0] = 0x12345678u; words[6] = 0x90abcdefu;
    int reusedIndex;
    { MemoryPoolProbe small(Context, 2); reusedIndex = small.Index(); }
    MemoryPoolProbe large(Context, 37);
    EXPECT_EQ(reusedIndex, large.Index()); ASSERT_NE(nullptr, large.Mem());
    EXPECT_EQ(37u, Context->m_MemoryPools[large.Index()]->AllocatedSize());
    unsigned int *largeWords = static_cast<unsigned int *>(large.Mem());
    largeWords[0] = 0xdeadbeefu; largeWords[36] = 0x13572468u;
    EXPECT_EQ(0xdeadbeefu, largeWords[0]); EXPECT_EQ(0x13572468u, largeWords[36]);
    EXPECT_EQ(0x12345678u, words[0]); EXPECT_EQ(0x90abcdefu, words[6]);
}

TEST_F(CKMemoryPoolTest, AReleasedLowerSlotIsReusedBeforeAllocatingAHigherOne) {
    MemoryPoolProbe *first = new MemoryPoolProbe(Context, 3);
    MemoryPoolProbe second(Context, 3), third(Context, 3);
    int released = first->Index(); delete first;
    MemoryPoolProbe replacement(Context, 3);
    EXPECT_EQ(released, replacement.Index());
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(second.Index()));
    EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(third.Index()));
}

TEST_F(CKMemoryPoolTest, SimultaneousLeasesGrowBeyondInitialSlotsAndKeepEveryBufferDistinct) {
    MemoryPoolProbe *leases[40] = {};
    for (int i = 0; i < 40; ++i) {
        leases[i] = new MemoryPoolProbe(Context, 3);
        EXPECT_EQ(i, leases[i]->Index());
        ASSERT_NE(nullptr, leases[i]->Mem());
        static_cast<unsigned int *>(leases[i]->Mem())[2] = 0x12000000u + i;
        for (int j = 0; j < i; ++j) EXPECT_NE(leases[j]->Mem(), leases[i]->Mem());
    }
    EXPECT_GE(Context->m_MemoryPools.Size(), 40);
    for (int i = 0; i < 40; ++i) {
        EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(i));
        EXPECT_EQ(0x12000000u + i, static_cast<unsigned int *>(leases[i]->Mem())[2]);
        delete leases[i];
        EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(i));
    }
}

TEST_F(CKMemoryPoolTest, ReleasingAroundTheBitmapWordBoundaryDoesNotClearNeighborReservations) {
    MemoryPoolProbe *leases[34] = {};
    for (int i = 0; i < 34; ++i) leases[i] = new MemoryPoolProbe(Context, 1);
    delete leases[31]; leases[31] = nullptr;
    {
        MemoryPoolProbe reused(Context, 1); EXPECT_EQ(31, reused.Index());
        EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(30)); EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(32));
    }
    delete leases[32]; leases[32] = nullptr;
    {
        MemoryPoolProbe first(Context, 1), second(Context, 1);
        EXPECT_EQ(31, first.Index()); EXPECT_EQ(32, second.Index());
        EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(30)); EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(33));
    }
    for (int i = 0; i < 34; ++i) delete leases[i];
}

TEST_F(CKMemoryPoolTest, DifferentContextsOwnIndependentReservationsAndCaches) {
    CKContext *other = nullptr; ASSERT_EQ(CK_OK, CKCreateContext(&other, nullptr, 0, 0));
    {
        MemoryPoolProbe first(Context, 9), second(other, 9);
        EXPECT_EQ(first.Index(), second.Index());
        ASSERT_NE(nullptr, first.Mem()); ASSERT_NE(nullptr, second.Mem());
        EXPECT_NE(first.Mem(), second.Mem());
        static_cast<unsigned int *>(first.Mem())[0] = 0x12345678u;
        static_cast<unsigned int *>(second.Mem())[0] = 0x90abcdefu;
        EXPECT_EQ(0x12345678u, static_cast<unsigned int *>(first.Mem())[0]);
        EXPECT_EQ(0x90abcdefu, static_cast<unsigned int *>(second.Mem())[0]);
    }
    EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(0)); EXPECT_FALSE(other->m_MemoryPoolMask.IsSet(0));
    CKCloseContext(other);
}

TEST_F(CKMemoryPoolTest, LeaseDestructionDoesNotDestroyItsBorrowedContextOrObjects) {
    CKObject *object = Context->CreateObject(CKCID_OBJECT, const_cast<char *>("retained object"));
    ASSERT_NE(nullptr, object); CK_ID id = object->GetID();
    { MemoryPoolProbe pool(Context, 4); EXPECT_EQ(Context, pool.Context()); }
    EXPECT_EQ(object, Context->GetObject(id));
    ASSERT_NE(nullptr, Context->GetMessageManager());
    MemoryPoolProbe next(Context, 4); EXPECT_NE(nullptr, next.Mem());
}

TEST_F(CKMemoryPoolTest, RepeatedAcquireAndReleaseDoesNotAccumulateReservations) {
    int initialSlots = Context->m_MemoryPools.Size();
    for (int count = 1; count <= 40; ++count) {
        { MemoryPoolProbe pool(Context, count); EXPECT_EQ(0, pool.Index()); ASSERT_NE(nullptr, pool.Mem()); }
        EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(0));
    }
    EXPECT_EQ(initialSlots, Context->m_MemoryPools.Size());
    EXPECT_EQ(40u, Context->m_MemoryPools[0]->AllocatedSize());
}

TEST_F(CKMemoryPoolTest, ExceptionUnwindingReturnsTheReservationAndKeepsTheCache) {
    void *buffer = nullptr;
    try {
        MemoryPoolProbe pool(Context, 6); buffer = pool.Mem();
        EXPECT_TRUE(Context->m_MemoryPoolMask.IsSet(pool.Index()));
        throw 123;
    } catch (int value) {
        EXPECT_EQ(123, value);
    }
    EXPECT_FALSE(Context->m_MemoryPoolMask.IsSet(0));
    MemoryPoolProbe reused(Context, 6); EXPECT_EQ(buffer, reused.Mem());
}
