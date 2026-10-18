#include <gtest/gtest.h>

#include <climits>
#include <cstring>

#include "CKAll.h"

static void LoadPrimitiveWords(CKStateChunk &chunk, const CKDWORD *words, int count) {
    CKDWORD wire[40] = {};
    ASSERT_LE(count, 38);
    wire[0] = (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8);
    wire[1] = count;
    if (count) std::memcpy(wire + 2, words, count * sizeof(CKDWORD));
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
    chunk.StartRead();
}

static void ExpectPrimitiveWire(CKStateChunk &chunk, const CKDWORD *words, int count) {
    ASSERT_EQ(count * 4, chunk.GetDataSize());
    ASSERT_EQ((count + 2) * 4, chunk.ConvertToBuffer(nullptr));
    CKDWORD wire[40] = {};
    ASSERT_LE(count, 38);
    ASSERT_EQ((count + 2) * 4, chunk.ConvertToBuffer(wire));
    EXPECT_EQ((CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8), wire[0]);
    EXPECT_EQ(static_cast<CKDWORD>(count), wire[1]);
    for (int i = 0; i < count; ++i) EXPECT_EQ(words[i], wire[i + 2]) << i;
}

static CKDWORD FloatBits(float value) {
    CKDWORD bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const CKDWORD FloatWire[] = {0, 0x80000000u, 0x3f800000u, 0xc0200000u,
    0x7f800000u, 0xff800000u, 0x7fc12345u, 0x00000001u, 0x00800000u};
static const CKDWORD VectorWire[] = {0x3f800000u, 0xc0200000u, 0x80000000u};
static const CKDWORD MatrixWire[] = {
    0x3f800000u, 0x40000000u, 0x40400000u, 0x40800000u,
    0x40a00000u, 0x40c00000u, 0x40e00000u, 0x41000000u,
    0x41100000u, 0x41200000u, 0x41300000u, 0x41400000u,
    0x41500000u, 0x41600000u, 0x41700000u, 0x41800000u
};

static void FillVector(VxVector &value, const CKDWORD *words) {
    ASSERT_EQ(12u, sizeof(value));
    std::memcpy(&value, words, sizeof(value));
}

static void ExpectVectorWords(const VxVector &value, const CKDWORD *expected) {
    CKDWORD words[3] = {};
    ASSERT_EQ(sizeof(words), sizeof(value));
    std::memcpy(words, &value, sizeof(words));
    for (int i = 0; i < 3; ++i) EXPECT_EQ(expected[i], words[i]) << i;
}

static void FillMatrix(VxMatrix &value) {
    ASSERT_EQ(sizeof(MatrixWire), sizeof(value));
    std::memcpy(&value, MatrixWire, sizeof(MatrixWire));
}

static void ExpectMatrixWords(const VxMatrix &value) {
    CKDWORD words[16] = {};
    ASSERT_EQ(sizeof(words), sizeof(value));
    std::memcpy(words, &value, sizeof(words));
    for (int i = 0; i < 16; ++i) EXPECT_EQ(MatrixWire[i], words[i]) << i;
}

TEST(CKStateChunkPrimitiveTest, ByteWriterSignExtendsOneCharIntoEachDword) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteByte(0); chunk.WriteByte(0x7f);
    chunk.WriteByte(static_cast<CKCHAR>(0x80)); chunk.WriteByte(static_cast<CKCHAR>(0xff));
    EXPECT_EQ(4, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0, 0x7f, 0xffffff80u, 0xffffffffu};
    ExpectPrimitiveWire(chunk, expected, 4);
}

TEST(CKStateChunkPrimitiveTest, ByteReaderUsesLowByteAndConsumesWholeDword) {
    const CKDWORD words[] = {0x12345678u, 0xffffff80u, 0xffffffffu};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 3);
    EXPECT_EQ(0x78, chunk.ReadByte()); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(0x80, chunk.ReadByte()); EXPECT_EQ(0xff, chunk.ReadByte());
    EXPECT_EQ(0, chunk.ReadByte()); EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, WordWriterZeroExtendsOneWordIntoEachDword) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteWord(0); chunk.WriteWord(0x8001); chunk.WriteWord(0xffff);
    EXPECT_EQ(3, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0, 0x8001, 0xffff}; ExpectPrimitiveWire(chunk, expected, 3);
}

TEST(CKStateChunkPrimitiveTest, WordReaderUsesLowWordAndDoesNotConsumeAtEof) {
    const CKDWORD words[] = {0x12345678u, 0xffff8001u, 0xffffffffu};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 3);
    EXPECT_EQ(0x5678, chunk.ReadWord()); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(0x8001, chunk.ReadWord()); EXPECT_EQ(0xffff, chunk.ReadWord());
    EXPECT_EQ(0, chunk.ReadWord()); EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, IntWriterPreservesSignedExtremesAsLiteralWords) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteInt(INT_MIN); chunk.WriteInt(-1); chunk.WriteInt(0); chunk.WriteInt(INT_MAX);
    EXPECT_EQ(4, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0x80000000u, 0xffffffffu, 0, 0x7fffffffu};
    ExpectPrimitiveWire(chunk, expected, 4);
}

TEST(CKStateChunkPrimitiveTest, IntReaderInterpretsSignedLiteralWordsAndStopsAtEof) {
    const CKDWORD words[] = {0x80000000u, 0xffffffffu, 0, 0x7fffffffu};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 4);
    EXPECT_EQ(INT_MIN, chunk.ReadInt()); EXPECT_EQ(-1, chunk.ReadInt());
    EXPECT_EQ(0, chunk.ReadInt()); EXPECT_EQ(INT_MAX, chunk.ReadInt());
    EXPECT_EQ(0, chunk.ReadInt()); EXPECT_EQ(4, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, DwordWriterPreservesAllThirtyTwoBits) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteDword(0x89abcdefu); chunk.WriteDword(0xffffffffu);
    EXPECT_EQ(2, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0x89abcdefu, 0xffffffffu}; ExpectPrimitiveWire(chunk, expected, 2);
}

TEST(CKStateChunkPrimitiveTest, DwordReaderPreservesUnsignedLiteralBits) {
    const CKDWORD words[] = {0x89abcdefu, 0xffffffffu};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 2);
    EXPECT_EQ(words[0], chunk.ReadDword()); EXPECT_EQ(words[1], chunk.ReadDword());
    EXPECT_EQ(0u, chunk.ReadDword()); EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, DwordAsWordsWriterStillUsesOneDword) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteDwordAsWords(0x1234abcd); chunk.WriteDwordAsWords(0x80000001u);
    EXPECT_EQ(2, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0x1234abcdu, 0x80000001u}; ExpectPrimitiveWire(chunk, expected, 2);
}

TEST(CKStateChunkPrimitiveTest, DwordAsWordsReaderUsesOneLiteralDword) {
    const CKDWORD words[] = {0x1234abcdu, 0x80000001u};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 2);
    EXPECT_EQ(words[0], chunk.ReadDwordAsWords()); EXPECT_EQ(1, chunk.GetCurrentPos());
    EXPECT_EQ(words[1], chunk.ReadDwordAsWords());
    EXPECT_EQ(0u, chunk.ReadDwordAsWords()); EXPECT_EQ(2, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, FloatWriterStoresLiteralIeeeBitsWithoutArithmetic) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    for (int i = 0; i < 9; ++i) {
        float value; std::memcpy(&value, FloatWire + i, sizeof(value)); chunk.WriteFloat(value);
    }
    EXPECT_EQ(9, chunk.GetCurrentPos()); chunk.CloseChunk(); ExpectPrimitiveWire(chunk, FloatWire, 9);
}

TEST(CKStateChunkPrimitiveTest, FloatReaderPreservesFiniteZeroInfinityQuietNanAndSubnormalBits) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, FloatWire, 9);
    for (int i = 0; i < 9; ++i) EXPECT_EQ(FloatWire[i], FloatBits(chunk.ReadFloat())) << i;
    EXPECT_EQ(0u, FloatBits(chunk.ReadFloat())); EXPECT_EQ(9, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, GuidWriterUsesOrderedTwoDwordFields) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteGuid(CKGUID(0x89abcdefu, 0x12345678u)); chunk.WriteInt(73);
    EXPECT_EQ(3, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {0x89abcdefu, 0x12345678u, 73}; ExpectPrimitiveWire(chunk, expected, 3);
}

TEST(CKStateChunkPrimitiveTest, GuidReaderConsumesExactlyTwoFieldsAndKeepsFollowingWord) {
    const CKDWORD words[] = {0x89abcdefu, 0x12345678u, 73};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 3);
    EXPECT_EQ(CKGUID(words[0], words[1]), chunk.ReadGuid()); EXPECT_EQ(2, chunk.GetCurrentPos());
    EXPECT_EQ(73, chunk.ReadInt()); EXPECT_EQ(CKGUID(), chunk.ReadGuid());
    EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, GuidReaderWithoutParserReturnsZeroWithoutCreatingReadState) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(CKGUID(), chunk.ReadGuid()); EXPECT_EQ(-1, chunk.GetCurrentPos());
    EXPECT_EQ(0, chunk.GetDataSize());
}

TEST(CKStateChunkPrimitiveTest, GuidReaderRejectsOneWordFrameWithoutConsumingIt) {
    const CKDWORD words[] = {0x89abcdefu};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 1);
    EXPECT_EQ(CKGUID(), chunk.ReadGuid()); EXPECT_EQ(0, chunk.GetCurrentPos());
    EXPECT_EQ(words[0], chunk.ReadDword()); EXPECT_EQ(CKGUID(), chunk.ReadGuid());
    EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, VectorReferenceWriterUsesXyzWordOrder) {
    VxVector value; FillVector(value, VectorWire);
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteVector(value);
    EXPECT_EQ(3, chunk.GetCurrentPos()); chunk.CloseChunk(); ExpectPrimitiveWire(chunk, VectorWire, 3);
}

TEST(CKStateChunkPrimitiveTest, VectorPointerWriterUsesSameThreeLiteralWords) {
    VxVector value; FillVector(value, VectorWire);
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteVector(&value);
    EXPECT_EQ(3, chunk.GetCurrentPos()); chunk.CloseChunk(); ExpectPrimitiveWire(chunk, VectorWire, 3);
}

TEST(CKStateChunkPrimitiveTest, NullVectorPointerWriterIsInertBeforeAndDuringWriting) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.WriteVector(static_cast<const VxVector *>(nullptr)); EXPECT_EQ(-1, chunk.GetCurrentPos());
    chunk.StartWrite(); chunk.WriteInt(73); chunk.WriteVector(static_cast<const VxVector *>(nullptr));
    EXPECT_EQ(1, chunk.GetCurrentPos()); chunk.CloseChunk();
    const CKDWORD expected[] = {73}; ExpectPrimitiveWire(chunk, expected, 1);
}

TEST(CKStateChunkPrimitiveTest, VectorReferenceReaderConsumesExactXyzFrame) {
    const CKDWORD words[] = {VectorWire[0], VectorWire[1], VectorWire[2], 73};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 4);
    VxVector value; chunk.ReadVector(value); ExpectVectorWords(value, VectorWire);
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(73, chunk.ReadInt());
}

TEST(CKStateChunkPrimitiveTest, VectorPointerReaderConsumesSameLiteralFrame) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, VectorWire, 3);
    VxVector value; chunk.ReadVector(&value); ExpectVectorWords(value, VectorWire);
    EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, VectorReferenceReaderPreservesDestinationForShortFrames) {
    for (int count = 1; count < 3; ++count) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, VectorWire, count);
        const CKDWORD sentinel[] = {0x41000000u, 0x41100000u, 0x41200000u};
        VxVector value; FillVector(value, sentinel); chunk.ReadVector(value);
        ExpectVectorWords(value, sentinel); EXPECT_EQ(0, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkPrimitiveTest, VectorPointerReaderPreservesDestinationForShortFrames) {
    for (int count = 1; count < 3; ++count) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, VectorWire, count);
        const CKDWORD sentinel[] = {0x41000000u, 0x41100000u, 0x41200000u};
        VxVector value; FillVector(value, sentinel); chunk.ReadVector(&value);
        ExpectVectorWords(value, sentinel); EXPECT_EQ(0, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkPrimitiveTest, NullVectorPointerReaderDoesNotConsumeAValidFrame) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, VectorWire, 3);
    chunk.ReadVector(static_cast<VxVector *>(nullptr)); EXPECT_EQ(0, chunk.GetCurrentPos());
    VxVector value; chunk.ReadVector(value); ExpectVectorWords(value, VectorWire);
    chunk.ReadVector(static_cast<VxVector *>(nullptr)); EXPECT_EQ(3, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, MatrixWriterUsesSixteenRowMajorLiteralWords) {
    VxMatrix value; FillMatrix(value);
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite(); chunk.WriteMatrix(value);
    EXPECT_EQ(16, chunk.GetCurrentPos()); chunk.CloseChunk(); ExpectPrimitiveWire(chunk, MatrixWire, 16);
}

TEST(CKStateChunkPrimitiveTest, MatrixReaderConsumesExactlySixteenWordsAndKeepsSentinel) {
    CKDWORD words[17]; std::memcpy(words, MatrixWire, sizeof(MatrixWire)); words[16] = 73;
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, 17);
    VxMatrix value; chunk.ReadMatrix(value); ExpectMatrixWords(value);
    EXPECT_EQ(16, chunk.GetCurrentPos()); EXPECT_EQ(73, chunk.ReadInt());
}

TEST(CKStateChunkPrimitiveTest, MatrixReaderPreservesDestinationForIncompleteFrames) {
    const int counts[] = {1, 8, 15};
    for (int i = 0; i < 3; ++i) {
        CKDWORD words[16] = {}; for (int j = 0; j < 16; ++j) words[j] = 0x12340000u + j;
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, words, counts[i]);
        VxMatrix value; FillMatrix(value); chunk.ReadMatrix(value); ExpectMatrixWords(value);
        EXPECT_EQ(0, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkPrimitiveTest, ColdScalarReadersReturnZeroAndKeepParserAbsent) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    EXPECT_EQ(0, chunk.ReadByte()); EXPECT_EQ(0, chunk.ReadWord()); EXPECT_EQ(0, chunk.ReadInt());
    EXPECT_EQ(0u, chunk.ReadDword()); EXPECT_EQ(0u, chunk.ReadDwordAsWords());
    EXPECT_EQ(0u, FloatBits(chunk.ReadFloat())); EXPECT_EQ(-1, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, ColdVectorAndMatrixReadersPreserveCallerValues) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    VxVector vector; FillVector(vector, VectorWire);
    VxMatrix matrix; FillMatrix(matrix);
    chunk.ReadVector(vector); ExpectVectorWords(vector, VectorWire);
    chunk.ReadVector(&vector); ExpectVectorWords(vector, VectorWire);
    chunk.ReadMatrix(matrix); ExpectMatrixWords(matrix); EXPECT_EQ(-1, chunk.GetCurrentPos());
}

TEST(CKStateChunkPrimitiveTest, EofReadersKeepCallerValuesAndCursorStable) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadPrimitiveWords(chunk, nullptr, 0);
    VxVector vector; FillVector(vector, VectorWire); VxMatrix matrix; FillMatrix(matrix);
    EXPECT_EQ(0, chunk.ReadByte()); EXPECT_EQ(0, chunk.ReadWord()); EXPECT_EQ(0, chunk.ReadInt());
    EXPECT_EQ(0u, chunk.ReadDword()); EXPECT_EQ(0u, chunk.ReadDwordAsWords());
    EXPECT_EQ(0u, FloatBits(chunk.ReadFloat())); EXPECT_EQ(CKGUID(), chunk.ReadGuid());
    chunk.ReadVector(vector); ExpectVectorWords(vector, VectorWire);
    chunk.ReadVector(&vector); ExpectVectorWords(vector, VectorWire);
    chunk.ReadMatrix(matrix); ExpectMatrixWords(matrix); EXPECT_EQ(0, chunk.GetCurrentPos());
}
