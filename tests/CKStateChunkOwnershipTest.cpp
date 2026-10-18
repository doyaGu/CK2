#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "CKAll.h"

// This executable links CK2Static. Unlike an EXE allocator override next to a
// separate DLL, these replacements observe the actual CK2 allocation calls.
// The fixed ledger never allocates. GTest assertions run after tracking stops.
struct CKTestAllocation {
    std::uintptr_t Address;
    std::size_t Bytes;
    int Frees;
    bool Array;
};
static const int LedgerCapacity = 512;
static CKTestAllocation Ledger[LedgerCapacity];
static int AllocationCount = 0;
static bool Tracking = false;
static bool Overflow = false;
static int UnmatchedRepeatedFrees = 0;
static std::uintptr_t BorrowedAddress = 0;
static int BorrowedFrees = 0;

static void RecordAllocation(void *address, std::size_t bytes, bool array) {
    if (!Tracking) return;
    if (AllocationCount == LedgerCapacity) { Overflow = true; return; }
    CKTestAllocation &record = Ledger[AllocationCount++];
    record.Address = reinterpret_cast<std::uintptr_t>(address);
    record.Bytes = bytes; record.Frees = 0; record.Array = array;
}

static void RecordFree(void *address) {
    if (!Tracking || !address) return;
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(address);
    if (BorrowedAddress && value == BorrowedAddress) ++BorrowedFrees;
    for (int i = AllocationCount - 1; i >= 0; --i) {
        if (Ledger[i].Address == value && Ledger[i].Frees == 0) { ++Ledger[i].Frees; return; }
    }
    for (int i = AllocationCount - 1; i >= 0; --i) {
        if (Ledger[i].Address == value) { ++UnmatchedRepeatedFrees; return; }
    }
}

class CKTestTrackingWindow {
public:
    explicit CKTestTrackingWindow(void *borrowed = nullptr) {
        AllocationCount = 0; Overflow = false; UnmatchedRepeatedFrees = 0;
        BorrowedAddress = reinterpret_cast<std::uintptr_t>(borrowed); BorrowedFrees = 0;
        Tracking = true;
    }
    ~CKTestTrackingWindow() { Tracking = false; }
    void Stop() { Tracking = false; }
private:
    CKTestTrackingWindow(const CKTestTrackingWindow &);
    CKTestTrackingWindow &operator=(const CKTestTrackingWindow &);
};

static void ExpectEveryAllocationReleased(int count = -1) {
    ASSERT_FALSE(Tracking); ASSERT_FALSE(Overflow); EXPECT_EQ(0, UnmatchedRepeatedFrees);
    if (count >= 0) EXPECT_EQ(count, AllocationCount);
    for (int i = 0; i < AllocationCount; ++i) EXPECT_EQ(1, Ledger[i].Frees) << "allocation " << i << ", bytes " << Ledger[i].Bytes;
}

static void LoadLiteral(CKStateChunk &chunk) {
    int wire[] = {(7 << 24) | (CHUNK_VERSION4 << 16) | (CKCID_PARAMETER << 8) | 0x5A,
                  6, 0x1122, 3, 41, 0x3344, 0, 43, 2, 2, 5, 1, 4, 1, 2};
    chunk.ConvertFromBuffer(wire);
    chunk.StartRead();
}

void *operator new(std::size_t bytes) {
    void *address = std::malloc(bytes ? bytes : 1);
    if (!address) throw std::bad_alloc();
    RecordAllocation(address, bytes, false); return address;
}

void *operator new[](std::size_t bytes) {
    void *address = std::malloc(bytes ? bytes : 1);
    if (!address) throw std::bad_alloc();
    RecordAllocation(address, bytes, true); return address;
}

void operator delete(void *address) noexcept { RecordFree(address); std::free(address); }
void operator delete[](void *address) noexcept { RecordFree(address); std::free(address); }
void operator delete(void *address, std::size_t) noexcept { RecordFree(address); std::free(address); }
void operator delete[](void *address, std::size_t) noexcept { RecordFree(address); std::free(address); }

TEST(CKStateChunkOwnershipTest, FactoryOwnerReleasesObjectPayloadParserAndThreeLists) {
    CKTestTrackingWindow window;
    CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    LoadLiteral(*chunk);
    const bool ownersPresent = chunk->GetIds() && chunk->GetChunks() && chunk->GetManagers();
    const int first = chunk->ReadInt();
    DeleteCKStateChunk(chunk);
    window.Stop();
    EXPECT_TRUE(ownersPresent); EXPECT_EQ(0x1122, first);
    ExpectEveryAllocationReleased(9);
    int arrays = 0; int listOwners = 0; int parserOwners = 0; int chunkOwners = 0;
    for (int i = 0; i < AllocationCount; ++i) {
        if (Ledger[i].Array) ++arrays;
        else if (Ledger[i].Bytes == sizeof(IntListStruct)) ++listOwners;
        else if (Ledger[i].Bytes == sizeof(ChunkParser)) ++parserOwners;
        if (!Ledger[i].Array && Ledger[i].Bytes == sizeof(CKStateChunk)) ++chunkOwners;
    }
    EXPECT_EQ(4, arrays); EXPECT_EQ(1, chunkOwners);
    // On Win32 list and parser objects both occupy12 bytes. Their combined
    // count remains independently observable; Win64 distinguishes them.
    EXPECT_EQ(4, listOwners + parserOwners);
}

TEST(CKStateChunkOwnershipTest, StackOwnerReleasesAllEightInternalAllocations) {
    CKTestTrackingWindow window;
    {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadLiteral(chunk);
    }
    window.Stop(); ExpectEveryAllocationReleased(8);
}

TEST(CKStateChunkOwnershipTest, FactoryDeepCloneReleasesEighteenIndependentAllocations) {
    CKTestTrackingWindow window;
    CKStateChunk *source = CreateCKStateChunk(CKCID_OBJECT, nullptr); LoadLiteral(*source);
    CKStateChunk *copy = CreateCKStateChunk(source);
    const bool distinct = source != copy && source->GetIds() != copy->GetIds()
        && source->GetChunks() != copy->GetChunks() && source->GetManagers() != copy->GetManagers()
        && source->GetIds()->Data != copy->GetIds()->Data
        && source->GetChunks()->Data != copy->GetChunks()->Data
        && source->GetManagers()->Data != copy->GetManagers()->Data
        && source->LockReadBuffer() != copy->LockReadBuffer();
    DeleteCKStateChunk(source);
    const int first = copy->ReadInt(); const int offset = copy->GetIds()->Data[1];
    DeleteCKStateChunk(copy);
    window.Stop(); EXPECT_TRUE(distinct); EXPECT_EQ(0x1122, first); EXPECT_EQ(5, offset);
    ExpectEveryAllocationReleased(18);
}

TEST(CKStateChunkOwnershipTest, RepeatedStartWriteAndClearReleaseOldOwnersExactlyOnce) {
    CKTestTrackingWindow window;
    CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    for (int i = 0; i < 20; ++i) {
        LoadLiteral(*chunk);
        chunk->StartWrite(); chunk->WriteObjectID(17); chunk->WriteManagerInt(CKGUID(19, 23), 29);
        chunk->Clear(); chunk->Clear();
    }
    DeleteCKStateChunk(chunk);
    window.Stop(); EXPECT_GT(AllocationCount, 100); ExpectEveryAllocationReleased();
}

TEST(CKStateChunkOwnershipTest, EmptyPlacementOwnerDoesNotAllocateOrFreeCallerStorage) {
    alignas(CKStateChunk) unsigned char storage[sizeof(CKStateChunk)];
    CKTestTrackingWindow window(storage);
    CKStateChunk *chunk = new (storage) CKStateChunk(CKCID_OBJECT, nullptr);
    chunk->~CKStateChunk();
    window.Stop(); EXPECT_EQ(0, BorrowedFrees); ExpectEveryAllocationReleased(0);
}

TEST(CKStateChunkOwnershipTest, ParserAndIteratorLifetimesDoNotAllocateOrFreePlacementStorage) {
    alignas(ChunkParser) unsigned char parserStorage[sizeof(ChunkParser)];
    alignas(ChunkIteratorData) unsigned char iteratorStorage[sizeof(ChunkIteratorData)];
    CKTestTrackingWindow window(parserStorage);
    ChunkParser *parser = new (parserStorage) ChunkParser;
    parser->~ChunkParser();
    ChunkIteratorData *iterator = new (iteratorStorage) ChunkIteratorData;
    iterator->~ChunkIteratorData();
    window.Stop(); EXPECT_EQ(0, BorrowedFrees); ExpectEveryAllocationReleased(0);
}

TEST(CKStateChunkOwnershipTest, ListGrowthReleasesEveryReplacedBufferAndFinalBuffer) {
    CKTestTrackingWindow window;
    int last = 0; int finalCount = 0;
    {
        IntListStruct list;
        for (int i = 0; i < 129; ++i) { list.AddEntry(i); list.AddEntries(i + 400); }
        last = list.Data[list.Size - 1]; finalCount = list.Size;
    }
    window.Stop(); EXPECT_EQ(528, last); EXPECT_EQ(387, finalCount);
    EXPECT_GT(AllocationCount, 5); ExpectEveryAllocationReleased();
}

TEST(CKStateChunkOwnershipTest, CompactListCopyReleasesSourceAndCopySeparately) {
    CKTestTrackingWindow window;
    bool distinct = false; int copied = 0;
    {
        IntListStruct source; source.AddEntry(31); source.AddEntries(37);
        IntListStruct copy(source); distinct = source.Data != copy.Data;
        copied = copy.Data[2]; source.Data[2] = 41;
    }
    window.Stop(); EXPECT_TRUE(distinct); EXPECT_EQ(37, copied); ExpectEveryAllocationReleased(3);
}

TEST(CKStateChunkOwnershipTest, EmptyListAndZeroLengthCopyBalanceTheirOwnAllocations) {
    CKTestTrackingWindow window;
    {
        IntListStruct source;
        IntListStruct copy(source);
    }
    window.Stop(); ExpectEveryAllocationReleased(1);
    EXPECT_EQ(0u, Ledger[0].Bytes); EXPECT_TRUE(Ledger[0].Array);
}

class CKStateChunkBorrowedOwnershipTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp()); ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        ASSERT_NE(nullptr, Context);
    }
    static void TearDownTestSuite() { if (Context) CKCloseContext(Context); Context = nullptr; CKShutdown(); }
    static CKContext *Context;
};
CKContext *CKStateChunkBorrowedOwnershipTest::Context = nullptr;

TEST_F(CKStateChunkBorrowedOwnershipTest, ChunkDestructionNeverReleasesBorrowedFile) {
    CKFile *file = new CKFile(Context);
    file->m_FileObjects.Resize(1); file->m_FileObjects[0].CreatedObject = 43;
    CKTestTrackingWindow window(file);
    CKStateChunk *chunk = CreateCKStateChunk(CKCID_OBJECT, file);
    int wire[] = {(CHNK_OPTION_FILE << 24) | (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), 1, 0};
    chunk->ConvertFromBuffer(wire); chunk->StartRead();
    const CK_ID mapped = chunk->ReadObjectID();
    DeleteCKStateChunk(chunk);
    window.Stop(); EXPECT_EQ(43u, mapped); EXPECT_EQ(0, BorrowedFrees); ExpectEveryAllocationReleased(3);
    EXPECT_EQ(1, file->m_FileObjects.Size()); EXPECT_EQ(43u, file->m_FileObjects[0].CreatedObject);
    delete file;
}
