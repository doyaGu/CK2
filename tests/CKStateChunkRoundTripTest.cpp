#include <gtest/gtest.h>

#include <array>
#include <climits>
#include <memory>
#include <vector>

#include "CKAll.h"

namespace {

struct CKStateChunkDeleter {
    void operator()(CKStateChunk *chunk) const {
        if (chunk) {
            DeleteCKStateChunk(chunk);
        }
    }
};

using CKStateChunkPtr = std::unique_ptr<CKStateChunk, CKStateChunkDeleter>;

CKStateChunkPtr CreateEmptyChunk() {
    return CKStateChunkPtr(CreateCKStateChunk(CKCID_OBJECT, nullptr));
}

} // namespace

TEST(CKStateChunkRoundTripTest, GuidRoundTrip) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const CKGUID expected(0x13572468u, 0x24681357u);

    chunk->StartWrite();
    chunk->WriteGuid(expected);
    chunk->CloseChunk();

    chunk->StartRead();
    const CKGUID actual = chunk->ReadGuid();

    EXPECT_TRUE(actual == expected);
}

TEST(CKStateChunkRoundTripTest, BufferRoundTripPreservesBytes) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const std::array<CKBYTE, 7> source = {{0x01, 0xFE, 0x10, 0x00, 0xAB, 0x7C, 0x99}};
    std::array<CKBYTE, 7> target = {{0, 0, 0, 0, 0, 0, 0}};

    chunk->StartWrite();
    chunk->WriteBuffer(static_cast<int>(source.size()), const_cast<CKBYTE *>(source.data()));
    chunk->CloseChunk();

    chunk->StartRead();
    chunk->ReadAndFillBuffer(target.data());

    EXPECT_EQ(source, target);
}

TEST(CKStateChunkRoundTripTest, NullBufferWriteDoesNotOverwriteTarget) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    std::array<CKBYTE, 8> target = {{0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}};
    const std::array<CKBYTE, 8> original = target;

    chunk->StartWrite();
    chunk->WriteBuffer(64, nullptr);
    chunk->CloseChunk();

    chunk->StartRead();
    chunk->ReadAndFillBuffer(target.data());

    EXPECT_EQ(original, target);
}

TEST(CKStateChunkRoundTripTest, MixedPrimitiveRoundTrip) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const int expectedInt = -1024;
    const CKDWORD expectedDword = 0xDEADBEEFu;
    const float expectedFloat = 3.5f;

    chunk->StartWrite();
    chunk->WriteInt(expectedInt);
    chunk->WriteDword(expectedDword);
    chunk->WriteFloat(expectedFloat);
    chunk->CloseChunk();

    chunk->StartRead();

    EXPECT_EQ(expectedInt, chunk->ReadInt());
    EXPECT_EQ(expectedDword, chunk->ReadDword());
    EXPECT_FLOAT_EQ(expectedFloat, chunk->ReadFloat());
}

TEST(CKStateChunkRoundTripTest, PrimitiveTypeMatrixRoundTrip) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const std::array<CKBYTE, 4> bytes = {{0x00, 0x01, 0x7F, 0xFF}};
    const std::array<CKWORD, 4> words = {{0x0000u, 0x0001u, 0x7FFFu, 0xFFFFu}};
    const std::array<int, 5> ints = {{INT_MIN, -1, 0, 1, INT_MAX}};
    const std::array<CKDWORD, 4> dwords = {{0x00000000u, 0x00000001u, 0x7FFFFFFFu, 0xFFFFFFFFu}};
    const std::array<float, 5> floats = {{-100.25f, -0.0f, 0.0f, 1.5f, 12345.875f}};

    chunk->StartWrite();
    for (size_t i = 0; i < bytes.size(); ++i) chunk->WriteByte(static_cast<CKCHAR>(bytes[i]));
    for (size_t i = 0; i < words.size(); ++i) chunk->WriteWord(words[i]);
    for (size_t i = 0; i < ints.size(); ++i) chunk->WriteInt(ints[i]);
    for (size_t i = 0; i < dwords.size(); ++i) chunk->WriteDword(dwords[i]);
    for (size_t i = 0; i < dwords.size(); ++i) chunk->WriteDwordAsWords(dwords[i]);
    for (size_t i = 0; i < floats.size(); ++i) chunk->WriteFloat(floats[i]);
    chunk->CloseChunk();

    chunk->StartRead();
    for (size_t i = 0; i < bytes.size(); ++i) EXPECT_EQ(bytes[i], chunk->ReadByte());
    for (size_t i = 0; i < words.size(); ++i) EXPECT_EQ(words[i], chunk->ReadWord());
    for (size_t i = 0; i < ints.size(); ++i) EXPECT_EQ(ints[i], chunk->ReadInt());
    for (size_t i = 0; i < dwords.size(); ++i) EXPECT_EQ(dwords[i], chunk->ReadDword());
    for (size_t i = 0; i < dwords.size(); ++i) EXPECT_EQ(dwords[i], chunk->ReadDwordAsWords());
    for (size_t i = 0; i < floats.size(); ++i) EXPECT_FLOAT_EQ(floats[i], chunk->ReadFloat());
}

TEST(CKStateChunkRoundTripTest, StringMatrixRoundTrip) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const char *values[] = {
        "",
        "simple",
        "with spaces and symbols !@#$%^&*()",
        "line1\\nline2\\nline3",
    };

    chunk->StartWrite();
    for (int i = 0; i < static_cast<int>(sizeof(values) / sizeof(values[0])); ++i) {
        chunk->WriteString(const_cast<char *>(values[i]));
    }
    chunk->CloseChunk();

    chunk->StartRead();
    for (int i = 0; i < static_cast<int>(sizeof(values) / sizeof(values[0])); ++i) {
        CKSTRING loaded = nullptr;
        const int size = chunk->ReadString(&loaded);
        ASSERT_GT(size, 0);
        ASSERT_NE(nullptr, loaded);
        EXPECT_STREQ(values[i], loaded);
        CKDeletePointer(loaded);
    }
}

TEST(CKStateChunkRoundTripTest, ReadBufferAllocatedPathRoundTrip) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    const std::array<CKBYTE, 11> source = {{0xDE, 0xAD, 0xBE, 0xEF, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70}};

    chunk->StartWrite();
    chunk->WriteBuffer(static_cast<int>(source.size()), const_cast<CKBYTE *>(source.data()));
    chunk->CloseChunk();

    chunk->StartRead();
    void *allocated = nullptr;
    const int readSize = chunk->ReadBuffer(&allocated);
    ASSERT_EQ(static_cast<int>(source.size()), readSize);
    ASSERT_NE(nullptr, allocated);

    const CKBYTE *readBytes = static_cast<const CKBYTE *>(allocated);
    for (int i = 0; i < readSize; ++i) {
        EXPECT_EQ(source[static_cast<size_t>(i)], readBytes[i]);
    }

    CKDeletePointer(allocated);
}

TEST(CKStateChunkRoundTripTest, ReadGuidOutOfDataReturnsZeroGuid) {
    CKStateChunkPtr chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.get());

    chunk->StartWrite();
    chunk->CloseChunk();

    chunk->StartRead();
    const CKGUID guid = chunk->ReadGuid();
    EXPECT_TRUE(guid == CKGUID(0u, 0u));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
