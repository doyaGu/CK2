#include <gtest/gtest.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <miniz.h>

#include "CKAll.h"

// This target links CK2Static: these replacements observe the real parser
// allocation calls, rather than an unrelated executable allocator beside CK2.dll.
struct CKTestAllocation {
    void *Address;
    int Frees;
};
static CKTestAllocation Ledger[64];
static int AllocationCount = 0;
static bool Tracking = false;
static bool LedgerOverflow = false;
static void *Borrowed = nullptr;
static int BorrowedFrees = 0;

static void RecordAllocation(void *address) {
    if (!Tracking) return;
    if (AllocationCount == 64) { LedgerOverflow = true; return; }
    Ledger[AllocationCount].Address = address;
    Ledger[AllocationCount++].Frees = 0;
}

static void RecordFree(void *address) {
    if (!Tracking || !address) return;
    if (address == Borrowed) ++BorrowedFrees;
    for (int i = AllocationCount - 1; i >= 0; --i) {
        if (Ledger[i].Address == address && Ledger[i].Frees == 0) {
            ++Ledger[i].Frees;
            return;
        }
    }
}

static void StartTracking(void *borrowed) {
    AllocationCount = 0; LedgerOverflow = false;
    Borrowed = borrowed; BorrowedFrees = 0; Tracking = true;
}

static void ExpectReleased(int count) {
    ASSERT_FALSE(Tracking);
    ASSERT_FALSE(LedgerOverflow);
    EXPECT_EQ(count, AllocationCount);
    EXPECT_EQ(0, BorrowedFrees);
    for (int i = 0; i < AllocationCount; ++i) EXPECT_EQ(1, Ledger[i].Frees) << i;
}

static const unsigned char PackedText[] = {
    // Python zlib.compress(b"parser literal bytes", 6), generated independently.
    0x78, 0x9c, 0x2b, 0x48, 0x2c, 0x2a, 0x4e, 0x2d, 0x52, 0xc8, 0xc9,
    0x2c, 0x49, 0x2d, 0x4a, 0xcc, 0x51, 0x48, 0xaa, 0x2c, 0x49, 0x2d,
    0x06, 0x00, 0x52, 0xe7, 0x07, 0xe2
};
static const char LiteralText[] = "parser literal bytes";

static CKDWORD IndependentAdler(const unsigned char *data, int count, CKDWORD seed) {
    unsigned int a = seed & 0xffffu, b = seed >> 16;
    for (int i = 0; i < count; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void *operator new(std::size_t size) {
    void *address = std::malloc(size ? size : 1);
    if (!address) throw std::bad_alloc();
    RecordAllocation(address); return address;
}
void *operator new[](std::size_t size) {
    void *address = std::malloc(size ? size : 1);
    if (!address) throw std::bad_alloc();
    RecordAllocation(address); return address;
}
void operator delete(void *address) noexcept { RecordFree(address); std::free(address); }
void operator delete[](void *address) noexcept { RecordFree(address); std::free(address); }
void operator delete(void *address, std::size_t) noexcept { RecordFree(address); std::free(address); }
void operator delete[](void *address, std::size_t) noexcept { RecordFree(address); std::free(address); }

TEST(CKBufferParserTest, ConstructorBorrowsBufferAndInitializesCursorAndOwnership) {
    char bytes[] = {11, 22, 33};
    CKBufferParser parser(bytes, sizeof(bytes));
    EXPECT_EQ(bytes, parser.m_Buffer);
    EXPECT_FALSE(parser.IsValid());
    EXPECT_EQ(sizeof(bytes), parser.Size());
    EXPECT_EQ(0, parser.CursorPos());
    EXPECT_EQ(sizeof(void *) == 8 ? 24u : 16u, sizeof(CKBufferParser));
    EXPECT_EQ(0u, offsetof(CKBufferParser, m_Buffer));
    EXPECT_EQ(sizeof(void *), offsetof(CKBufferParser, m_CursorPos));
    EXPECT_EQ(sizeof(void *) + 4, offsetof(CKBufferParser, m_Valid));
    EXPECT_EQ(sizeof(void *) + 8, offsetof(CKBufferParser, m_Size));
}

TEST(CKBufferParserTest, RawReadConsumesExactBytesAndPreservesAdjacentSentinels) {
    const unsigned char bytes[] = {0x91, 0x12, 0x34, 0x56, 0x92};
    CKBufferParser parser(const_cast<unsigned char *>(bytes), sizeof(bytes));
    unsigned char result[] = {0x81, 0, 0, 0, 0x82};
    parser.Seek(1);
    ASSERT_TRUE(parser.Read(result + 1, 3));
    EXPECT_EQ(4, parser.CursorPos());
    const unsigned char expected[] = {0x81, 0x12, 0x34, 0x56, 0x82};
    EXPECT_EQ(0, std::memcmp(expected, result, sizeof(result)));
}

TEST(CKBufferParserTest, RawReadFailureDoesNotMoveCursorOrPublishPartialBytes) {
    unsigned char bytes[] = {1, 2, 3};
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(2);
    unsigned char result[] = {0x81, 0x82};
    EXPECT_FALSE(parser.Read(result, 2));
    EXPECT_FALSE(parser.Read(result, -1));
    EXPECT_EQ(2, parser.CursorPos());
    EXPECT_EQ(0x81, result[0]); EXPECT_EQ(0x82, result[1]);
}

TEST(CKBufferParserTest, RawWriteAdvancesAndCannotOverwriteFollowingBytes) {
    unsigned char bytes[] = {0x91, 0, 0, 0, 0x92};
    const unsigned char input[] = {0x12, 0x34, 0x56};
    CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    ASSERT_TRUE(parser.Write(input, 3));
    EXPECT_EQ(4, parser.CursorPos());
    EXPECT_FALSE(parser.Write(input, 2)); EXPECT_FALSE(parser.Write(input, -1));
    EXPECT_EQ(4, parser.CursorPos());
    const unsigned char expected[] = {0x91, 0x12, 0x34, 0x56, 0x92};
    EXPECT_EQ(0, std::memcmp(expected, bytes, sizeof(bytes)));
}

TEST(CKBufferParserTest, ZeroByteRawOperationsPreserveEndCursorAndStorage) {
    unsigned char byte = 0xA7;
    CKBufferParser parser(&byte, 1); parser.Seek(1);
    EXPECT_TRUE(parser.Read(&byte, 0)); EXPECT_TRUE(parser.Write(&byte, 0));
    EXPECT_EQ(1, parser.CursorPos()); EXPECT_EQ(0xA7, byte);
}

TEST(CKBufferParserTest, ReadIntUsesUnalignedLiteralFrameAndLeavesTrailingByte) {
    unsigned char bytes[] = {0xEE, 0x78, 0x56, 0x34, 0x12, 0xEF};
    CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    EXPECT_EQ(0x12345678, parser.ReadInt()); EXPECT_EQ(5, parser.CursorPos());
    EXPECT_EQ(0, parser.ReadInt()); EXPECT_EQ(5, parser.CursorPos());
}

TEST(CKBufferParserTest, ReadStringOwnsCopyAndAppendsTerminatorWithoutConsumingNextByte) {
    unsigned char bytes[] = {3, 0, 0, 0, 'a', 0, 'z', 0x71};
    CKBufferParser parser(bytes, sizeof(bytes));
    char *string = parser.ReadString(); ASSERT_NE(nullptr, string);
    EXPECT_EQ(7, parser.CursorPos());
    bytes[4] = 'x';
    EXPECT_EQ('a', string[0]); EXPECT_EQ(0, string[1]);
    EXPECT_EQ('z', string[2]); EXPECT_EQ(0, string[3]);
    delete[] string;
    unsigned char next = 0; ASSERT_TRUE(parser.Read(&next, 1)); EXPECT_EQ(0x71, next);
}

TEST(CKBufferParserTest, ReadStringZeroNegativeAndShortLengthRetainOnlyConsumedPrefix) {
    int lengths[] = {0, -7, 5};
    for (int i = 0; i < 3; ++i) {
        unsigned char bytes[6] = {0, 0, 0, 0, 0x31, 0x32};
        std::memcpy(bytes, lengths + i, sizeof(int));
        CKBufferParser parser(bytes, sizeof(bytes));
        EXPECT_EQ(nullptr, parser.ReadString()); EXPECT_EQ(4, parser.CursorPos());
        EXPECT_EQ(0x31, bytes[4]); EXPECT_EQ(0x32, bytes[5]);
    }
    char shortPrefix[3] = {}; CKBufferParser parser(shortPrefix, sizeof(shortPrefix));
    EXPECT_EQ(nullptr, parser.ReadString()); EXPECT_EQ(0, parser.CursorPos());
}

TEST(CKBufferParserTest, SeekClampsAndSkipSupportsForwardAndBackwardRelativeMovement) {
    char bytes[8] = {}; CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(-1); EXPECT_EQ(0, parser.CursorPos());
    parser.Seek(99); EXPECT_EQ(8, parser.CursorPos());
    parser.Skip(-3); EXPECT_EQ(5, parser.CursorPos());
    parser.Skip(2); EXPECT_EQ(7, parser.CursorPos());
    parser.Skip(2); EXPECT_EQ(8, parser.CursorPos());
    parser.Skip(-99); EXPECT_EQ(0, parser.CursorPos());
}

TEST(CKBufferParserTest, SkipLargePositiveOffsetClampsWithoutSignedWraparound) {
    char bytes[8] = {}; CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(3);
    parser.Skip(INT_MAX); EXPECT_EQ(8, parser.CursorPos());
    parser.Skip(INT_MIN); EXPECT_EQ(0, parser.CursorPos());
}

TEST(CKBufferParserTest, ComputeCrcUsesCurrentSliceAndSeedWithoutAdvancingCursor) {
    unsigned char bytes[] = {0xFA, 'a', 'b', 'c', 0xFB};
    CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    EXPECT_EQ(0x024A0126u, parser.ComputeCRC(3));
    const CKDWORD seed = 0x12345678u;
    EXPECT_EQ(IndependentAdler(bytes + 1, 3, seed), parser.ComputeCRC(3, seed));
    EXPECT_EQ(1, parser.CursorPos());
    EXPECT_EQ(seed, parser.ComputeCRC(0, seed)); EXPECT_EQ(1, parser.CursorPos());
}

TEST(CKBufferParserTest, ComputeCrcChainsAdjacentSlicesWithIndependentAdlerOracle) {
    unsigned char bytes[] = {0, 1, 127, 128, 255, 2};
    CKBufferParser parser(bytes, sizeof(bytes));
    const CKDWORD first = parser.ComputeCRC(2, 1); parser.Seek(2);
    EXPECT_EQ(IndependentAdler(bytes, sizeof(bytes), 1), parser.ComputeCRC(4, first));
    EXPECT_EQ(2, parser.CursorPos());
}

TEST(CKBufferParserTest, ExtractReturnsReadableOwnedCopyAndConsumesSourceSlice) {
    unsigned char bytes[] = {0x91, 10, 20, 30, 0x92};
    CKBufferParser source(bytes, sizeof(bytes)); source.Seek(1);
    CKBufferParser *copy = source.Extract(3); ASSERT_NE(nullptr, copy);
    EXPECT_TRUE(copy->IsValid()); EXPECT_EQ(3, copy->Size());
    EXPECT_EQ(0, copy->CursorPos()); EXPECT_EQ(4, source.CursorPos());
    EXPECT_NE(source.m_Buffer + 1, copy->m_Buffer);
    bytes[1] = 99;
    unsigned char result[3] = {}; EXPECT_TRUE(copy->Read(result, sizeof(result)));
    const unsigned char expected[] = {10, 20, 30};
    EXPECT_EQ(0, std::memcmp(expected, result, sizeof(result)));
    delete copy;
}

TEST(CKBufferParserTest, ExtractRejectsEmptyNegativeOrShortSliceWithoutMovingSource) {
    char bytes[3] = {}; CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    CKBufferParser *empty = parser.Extract(0);
    EXPECT_EQ(nullptr, empty); delete empty;
    EXPECT_EQ(nullptr, parser.Extract(-1)); EXPECT_EQ(nullptr, parser.Extract(3));
    EXPECT_EQ(1, parser.CursorPos());
}

TEST(CKBufferParserTest, ExtractDestructorReleasesBothOwnersAndNeverBorrowedBuffer) {
    char bytes[] = {11, 22, 33};
    StartTracking(bytes);
    { CKBufferParser parser(bytes, sizeof(bytes)); delete parser.Extract(2); }
    Tracking = false; ExpectReleased(2); EXPECT_EQ(11, bytes[0]);
}

TEST(CKBufferParserTest, BorrowedDestructorDoesNotFreeCallerStorage) {
    char bytes[] = {11, 22, 33}; StartTracking(bytes);
    { CKBufferParser parser(bytes, sizeof(bytes)); }
    Tracking = false; ExpectReleased(0); EXPECT_EQ(33, bytes[2]);
}

TEST(CKBufferParserTest, ExtractChunkDecodesIndependentModernRecordAndConsumesDeclaredFrame) {
    int words[] = {0x71717171, (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8) | 9,
                   2, 0x12345678, 0x23456789, 0x72727272};
    CKBufferParser parser(words, sizeof(words)); parser.Seek(4);
    CKStateChunk *chunk = parser.ExtractChunk(16, nullptr); ASSERT_NE(nullptr, chunk);
    EXPECT_EQ(20, parser.CursorPos()); EXPECT_EQ(CKCID_OBJECT, chunk->GetChunkClassID());
    EXPECT_EQ(9, chunk->GetDataVersion()); EXPECT_EQ(8, chunk->GetDataSize());
    words[3] = 0; chunk->StartRead(); EXPECT_EQ(0x12345678, chunk->ReadInt());
    EXPECT_EQ(0x23456789, chunk->ReadInt()); delete chunk;
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserTest, ExtractChunkDecodesIndependentLegacyRecordAndDeepCopiesLists) {
    int words[] = {(CHUNK_VERSION2 << 16) | 9, 0x123, 1, 0, 1, 1, 1,
                   0x12345678, 0, 0, 0, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    CKStateChunk *chunk = parser.ExtractChunk(44, nullptr); ASSERT_NE(nullptr, chunk);
    EXPECT_EQ(44, parser.CursorPos()); EXPECT_EQ(0x123, chunk->GetChunkClassID());
    ASSERT_NE(nullptr, chunk->GetIds()); EXPECT_EQ(1, chunk->GetIds()->Size);
    ASSERT_NE(nullptr, chunk->GetChunks()); EXPECT_EQ(1, chunk->GetChunks()->Size);
    ASSERT_NE(nullptr, chunk->GetManagers()); EXPECT_EQ(1, chunk->GetManagers()->Size);
    chunk->StartRead(); EXPECT_EQ(0x12345678, chunk->ReadInt());
    delete chunk; EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserTest, ExtractChunkInvalidVersionReturnsNullAndStillConsumesRecord) {
    int words[] = {(99 << 16) | 9, 0, 0x72727272};
    CKBufferParser parser(words, sizeof(words));
    EXPECT_EQ(nullptr, parser.ExtractChunk(8, nullptr)); EXPECT_EQ(8, parser.CursorPos());
    EXPECT_EQ(0x72727272, parser.ReadInt());
}

TEST(CKBufferParserTest, ExtractChunkDestructorReleasesIndependentPayload) {
    int words[] = {(CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), 2, 41, 43};
    StartTracking(words);
    { CKBufferParser parser(words, sizeof(words)); delete parser.ExtractChunk(sizeof(words), nullptr); }
    Tracking = false; ExpectReleased(2); EXPECT_EQ(41, words[2]);
}

TEST(CKBufferParserTest, InsertChunkWritesLiteralWireWithoutAddingAnotherSizePrefix) {
    int literal[] = {(CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8) | 9, 2, 41, 43};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); ASSERT_TRUE(chunk.ConvertFromBuffer(literal));
    int output[] = {0x71717171, 0, 0, 0, 0, 0x72727272};
    CKBufferParser parser(output, sizeof(output)); parser.Seek(4); parser.InsertChunk(&chunk);
    EXPECT_EQ(20, parser.CursorPos()); EXPECT_EQ(0, std::memcmp(literal, output + 1, sizeof(literal)));
    EXPECT_EQ(0x71717171, output[0]); EXPECT_EQ(0x72727272, output[5]);
    parser.InsertChunk(nullptr); EXPECT_EQ(20, parser.CursorPos());
}

TEST(CKBufferParserTest, UnPackReadsIndependentZlibGoldenAtCursorWithoutConsumingSource) {
    unsigned char source[sizeof(PackedText) + 2] = {}; source[0] = 0x91; source[sizeof(source) - 1] = 0x92;
    std::memcpy(source + 1, PackedText, sizeof(PackedText));
    CKBufferParser parser(source, sizeof(source)); parser.Seek(1);
    CKBufferParser *decoded = parser.UnPack(sizeof(LiteralText) - 1, sizeof(PackedText));
    ASSERT_NE(nullptr, decoded); EXPECT_TRUE(decoded->IsValid()); EXPECT_EQ(0, decoded->CursorPos());
    EXPECT_EQ(sizeof(LiteralText) - 1, decoded->Size()); EXPECT_EQ(1, parser.CursorPos());
    EXPECT_EQ(0, std::memcmp(LiteralText, decoded->m_Buffer, sizeof(LiteralText) - 1));
    delete decoded; EXPECT_EQ(0x91, source[0]); EXPECT_EQ(0x92, source[sizeof(source) - 1]);
}

TEST(CKBufferParserTest, UnPackCorruptOrWrongDecodedSizeReturnsNullWithoutConsumingSource) {
    unsigned char source[sizeof(PackedText) + 1] = {}; std::memcpy(source + 1, PackedText, sizeof(PackedText));
    CKBufferParser parser(source, sizeof(source)); parser.Seek(1);
    EXPECT_EQ(nullptr, parser.UnPack(sizeof(LiteralText), sizeof(PackedText)));
    EXPECT_EQ(nullptr, parser.UnPack(sizeof(LiteralText) - 2, sizeof(PackedText)));
    source[sizeof(source) - 1] ^= 1;
    EXPECT_EQ(nullptr, parser.UnPack(sizeof(LiteralText) - 1, sizeof(PackedText)));
    EXPECT_EQ(1, parser.CursorPos());
}

TEST(CKBufferParserTest, UnPackDestructorReleasesDecodedStorageAndParserOwner) {
    StartTracking(const_cast<unsigned char *>(PackedText));
    { CKBufferParser parser(const_cast<unsigned char *>(PackedText), sizeof(PackedText));
      delete parser.UnPack(sizeof(LiteralText) - 1, sizeof(PackedText)); }
    Tracking = false; ExpectReleased(2);
}

TEST(CKBufferParserTest, PackProducesIndependentZlibDecodeOfCurrentSliceAndPreservesSourceCursor) {
    char bytes[] = "#parser literal bytes!";
    CKBufferParser parser(bytes, sizeof(bytes) - 1); parser.Seek(1);
    for (int level = -1; level <= 9; ++level) {
        CKBufferParser *packed = parser.Pack(sizeof(LiteralText) - 1, level); ASSERT_NE(nullptr, packed);
        EXPECT_TRUE(packed->IsValid()); EXPECT_EQ(0, packed->CursorPos()); EXPECT_EQ(1, parser.CursorPos());
        unsigned char decoded[64] = {}; mz_ulong decodedSize = sizeof(decoded);
        EXPECT_EQ(MZ_OK, mz_uncompress(decoded, &decodedSize,
                                      reinterpret_cast<const unsigned char *>(packed->m_Buffer), packed->Size()));
        EXPECT_EQ(sizeof(LiteralText) - 1, decodedSize);
        EXPECT_EQ(0, std::memcmp(LiteralText, decoded, sizeof(LiteralText) - 1));
        delete packed; EXPECT_EQ('#', bytes[0]); EXPECT_EQ('!', bytes[sizeof(bytes) - 2]);
    }
}

TEST(CKBufferParserTest, PackRejectsNonpositiveSizeAndInvalidLevelWithoutConsumingSource) {
    char bytes[] = "data"; CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    EXPECT_EQ(nullptr, parser.Pack(0, 6)); EXPECT_EQ(nullptr, parser.Pack(-1, 6));
    CKBufferParser *invalid = parser.Pack(3, 99);
    EXPECT_EQ(nullptr, invalid); delete invalid; EXPECT_EQ(1, parser.CursorPos());
}

TEST(CKBufferParserTest, PackDestructorReleasesTemporaryFinalBufferAndParserOwner) {
    char bytes[] = "parser literal bytes"; StartTracking(bytes);
    { CKBufferParser parser(bytes, sizeof(bytes)); delete parser.Pack(sizeof(bytes), 6); }
    Tracking = false; ExpectReleased(3);
}

TEST(CKBufferParserTest, ExtractFileWritesExactCurrentSliceAndAdvancesOnlyThatSlice) {
    char bytes[] = {0x11, 0x22, 0x33, 0x44, 0x55}; CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    char path[128]; std::snprintf(path, sizeof(path), "CKBufferParser-pass35-%p.tmp", static_cast<void *>(&parser));
    ASSERT_TRUE(parser.ExtractFile(path, 3)); EXPECT_EQ(4, parser.CursorPos());
    FILE *file = std::fopen(path, "rb"); ASSERT_NE(nullptr, file);
    char result[4] = {}; const std::size_t count = std::fread(result, 1, sizeof(result), file);
    std::fclose(file); EXPECT_EQ(0, std::remove(path));
    EXPECT_EQ(3u, count); EXPECT_EQ(0, std::memcmp(bytes + 1, result, 3));
}

TEST(CKBufferParserTest, ExtractFileOpenAndSliceFailuresDoNotConsumeInput) {
    char bytes[] = {0x11, 0x22, 0x33}; CKBufferParser parser(bytes, sizeof(bytes)); parser.Seek(1);
    char path[] = "CKBufferParser-pass35-nonexistent-directory/child.bin";
    EXPECT_FALSE(parser.ExtractFile(path, 2)); EXPECT_EQ(1, parser.CursorPos());
    EXPECT_FALSE(parser.ExtractFile(path, -1)); EXPECT_FALSE(parser.ExtractFile(path, 3));
    EXPECT_EQ(1, parser.CursorPos());
}
