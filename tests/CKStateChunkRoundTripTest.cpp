#include <gtest/gtest.h>

#include <climits>
#include <cstring>

#include "CKAll.h"

class ScopedStateChunk {
public:
    explicit ScopedStateChunk(CKStateChunk *chunk) : chunk_(chunk) {}
    ~ScopedStateChunk() {
        if (chunk_) {
            DeleteCKStateChunk(chunk_);
        }
    }

    CKStateChunk *Get() const {
        return chunk_;
    }

    CKStateChunk *operator->() const {
        return chunk_;
    }

private:
    ScopedStateChunk(const ScopedStateChunk &);
    ScopedStateChunk &operator=(const ScopedStateChunk &);

    CKStateChunk *chunk_;
};

static ScopedStateChunk CreateEmptyChunk() {
    return ScopedStateChunk(CreateCKStateChunk(CKCID_OBJECT, nullptr));
}

TEST(CKStateChunkRoundTripTest, GuidRoundTrip) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    const CKGUID expected(0x13572468u, 0x24681357u);

    chunk->StartWrite();
    chunk->WriteGuid(expected);
    chunk->CloseChunk();

    chunk->StartRead();
    const CKGUID actual = chunk->ReadGuid();

    EXPECT_TRUE(actual == expected);
}

TEST(CKStateChunkRoundTripTest, BufferRoundTripPreservesBytes) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    const CKBYTE source[] = {0x01, 0xFE, 0x10, 0x00, 0xAB, 0x7C, 0x99};
    CKBYTE target[] = {0, 0, 0, 0, 0, 0, 0};

    chunk->StartWrite();
    chunk->WriteBuffer(static_cast<int>(sizeof(source)), const_cast<CKBYTE *>(source));
    chunk->CloseChunk();

    chunk->StartRead();
    chunk->ReadAndFillBuffer(target);

    EXPECT_EQ(0, memcmp(source, target, sizeof(source)));
}

TEST(CKStateChunkRoundTripTest, NullBufferWriteDoesNotOverwriteTarget) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    CKBYTE target[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    CKBYTE original[sizeof(target)];
    memcpy(original, target, sizeof(target));

    chunk->StartWrite();
    chunk->WriteBuffer(64, nullptr);
    chunk->CloseChunk();

    chunk->StartRead();
    chunk->ReadAndFillBuffer(target);

    EXPECT_EQ(0, memcmp(original, target, sizeof(target)));
}

TEST(CKStateChunkRoundTripTest, MixedPrimitiveRoundTrip) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

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
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    const CKBYTE bytes[] = {0x00, 0x01, 0x7F, 0xFF};
    const CKWORD words[] = {0x0000u, 0x0001u, 0x7FFFu, 0xFFFFu};
    const int ints[] = {INT_MIN, -1, 0, 1, INT_MAX};
    const CKDWORD dwords[] = {0x00000000u, 0x00000001u, 0x7FFFFFFFu, 0xFFFFFFFFu};
    const float floats[] = {-100.25f, -0.0f, 0.0f, 1.5f, 12345.875f};

    chunk->StartWrite();
    for (int i = 0; i < static_cast<int>(sizeof(bytes) / sizeof(bytes[0])); ++i) chunk->WriteByte(static_cast<CKCHAR>(bytes[i]));
    for (int i = 0; i < static_cast<int>(sizeof(words) / sizeof(words[0])); ++i) chunk->WriteWord(words[i]);
    for (int i = 0; i < static_cast<int>(sizeof(ints) / sizeof(ints[0])); ++i) chunk->WriteInt(ints[i]);
    for (int i = 0; i < static_cast<int>(sizeof(dwords) / sizeof(dwords[0])); ++i) chunk->WriteDword(dwords[i]);
    for (int i = 0; i < static_cast<int>(sizeof(dwords) / sizeof(dwords[0])); ++i) chunk->WriteDwordAsWords(dwords[i]);
    for (int i = 0; i < static_cast<int>(sizeof(floats) / sizeof(floats[0])); ++i) chunk->WriteFloat(floats[i]);
    chunk->CloseChunk();

    chunk->StartRead();
    for (int i = 0; i < static_cast<int>(sizeof(bytes) / sizeof(bytes[0])); ++i) EXPECT_EQ(bytes[i], chunk->ReadByte());
    for (int i = 0; i < static_cast<int>(sizeof(words) / sizeof(words[0])); ++i) EXPECT_EQ(words[i], chunk->ReadWord());
    for (int i = 0; i < static_cast<int>(sizeof(ints) / sizeof(ints[0])); ++i) EXPECT_EQ(ints[i], chunk->ReadInt());
    for (int i = 0; i < static_cast<int>(sizeof(dwords) / sizeof(dwords[0])); ++i) EXPECT_EQ(dwords[i], chunk->ReadDword());
    for (int i = 0; i < static_cast<int>(sizeof(dwords) / sizeof(dwords[0])); ++i) EXPECT_EQ(dwords[i], chunk->ReadDwordAsWords());
    for (int i = 0; i < static_cast<int>(sizeof(floats) / sizeof(floats[0])); ++i) EXPECT_FLOAT_EQ(floats[i], chunk->ReadFloat());
}

TEST(CKStateChunkRoundTripTest, StringMatrixRoundTrip) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

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
        char *loaded = nullptr;
        const int size = chunk->ReadString(&loaded);
        ASSERT_GT(size, 0);
        ASSERT_NE(nullptr, loaded);
        EXPECT_STREQ(values[i], loaded);
        CKDeletePointer(loaded);
    }
}

TEST(CKStateChunkRoundTripTest, ReadBufferAllocatedPathRoundTrip) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    const CKBYTE source[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70};

    chunk->StartWrite();
    chunk->WriteBuffer(static_cast<int>(sizeof(source)), const_cast<CKBYTE *>(source));
    chunk->CloseChunk();

    chunk->StartRead();
    void *allocated = nullptr;
    const int readSize = chunk->ReadBuffer(&allocated);
    ASSERT_EQ(static_cast<int>(sizeof(source)), readSize);
    ASSERT_NE(nullptr, allocated);

    const CKBYTE *readBytes = static_cast<const CKBYTE *>(allocated);
    for (int i = 0; i < readSize; ++i) {
        EXPECT_EQ(source[i], readBytes[i]);
    }

    CKDeletePointer(allocated);
}

TEST(CKStateChunkRoundTripTest, ReadGuidOutOfDataReturnsZeroGuid) {
    ScopedStateChunk chunk = CreateEmptyChunk();
    ASSERT_NE(nullptr, chunk.Get());

    chunk->StartWrite();
    chunk->CloseChunk();

    chunk->StartRead();
    const CKGUID guid = chunk->ReadGuid();
    EXPECT_TRUE(guid == CKGUID(0u, 0u));
}

TEST(CKStateChunkRoundTripTest, CreateContextRejectsNullOutputPointer) {
    EXPECT_EQ(CKERR_INVALIDPARAMETER, CKCreateContext(nullptr, NULL, 0, 0));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
