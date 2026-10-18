#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

namespace {

const int Sentinel = 0x13572468;

void LoadBufferWords(CKStateChunk &chunk, const int *words, int count) {
    int envelope[64] = {};
    envelope[0] = (CHUNK_VERSION4 << 16) | (CKCID_OBJECT << 8);
    envelope[1] = count;
    ASSERT_LE(count, 62);
    if (count) std::memcpy(envelope + 2, words, static_cast<size_t>(count) * sizeof(int));
    ASSERT_TRUE(chunk.ConvertFromBuffer(envelope));
    chunk.StartRead();
}

typedef void (CKStateChunk::*BufferWriter)(int, void *);
typedef void (CKStateChunk::*StoredFill)(void *);
typedef void (CKStateChunk::*ExplicitFill)(int, void *);
typedef int (CKStateChunk::*ArrayReader)(void **);

const BufferWriter SizedWriters[] = {
    &CKStateChunk::WriteBuffer, &CKStateChunk::WriteBuffer_LEndian, &CKStateChunk::WriteBuffer_LEndian16
};
const BufferWriter RawWriters[] = {
    &CKStateChunk::WriteBufferNoSize, &CKStateChunk::WriteBufferNoSize_LEndian, &CKStateChunk::WriteBufferNoSize_LEndian16
};
const StoredFill StoredReaders[] = {
    static_cast<StoredFill>(&CKStateChunk::ReadAndFillBuffer),
    static_cast<StoredFill>(&CKStateChunk::ReadAndFillBuffer_LEndian),
    static_cast<StoredFill>(&CKStateChunk::ReadAndFillBuffer_LEndian16)
};
const ExplicitFill ExplicitReaders[] = {
    static_cast<ExplicitFill>(&CKStateChunk::ReadAndFillBuffer),
    static_cast<ExplicitFill>(&CKStateChunk::ReadAndFillBuffer_LEndian),
    static_cast<ExplicitFill>(&CKStateChunk::ReadAndFillBuffer_LEndian16)
};
const ArrayReader ArrayReaders[] = {&CKStateChunk::ReadArray_LEndian, &CKStateChunk::ReadArray_LEndian16};

void ExpectPayload(const unsigned char *actual) {
    const unsigned char expected[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    EXPECT_EQ(0, std::memcmp(actual, expected, sizeof(expected)));
}

} // namespace

TEST(CKStateChunkBufferTest, SizedWriterAliasesEmitLengthBytesAndRoundedCursor) {
    unsigned char bytes[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        (chunk.*SizedWriters[i])(5, bytes); EXPECT_EQ(3, chunk.GetCurrentPos());
        chunk.WriteInt(Sentinel); chunk.CloseChunk();
        int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
        EXPECT_EQ(4, wire[1]); EXPECT_EQ(5, wire[2]);
        ExpectPayload(reinterpret_cast<unsigned char *>(wire + 3)); EXPECT_EQ(Sentinel, wire[5]);
    }
}

TEST(CKStateChunkBufferTest, RawWriterAliasesOmitLengthAndRoundToWords) {
    unsigned char bytes[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        (chunk.*RawWriters[i])(5, bytes); EXPECT_EQ(2, chunk.GetCurrentPos());
        chunk.WriteInt(Sentinel); chunk.CloseChunk();
        int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
        EXPECT_EQ(3, wire[1]); ExpectPayload(reinterpret_cast<unsigned char *>(wire + 2));
        EXPECT_EQ(Sentinel, wire[4]);
    }
}

TEST(CKStateChunkBufferTest, SizedWriterNullAndZeroProduceOneEmptyHeader) {
    unsigned char byte = 0xA5;
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        (chunk.*SizedWriters[i])(17, nullptr); (chunk.*SizedWriters[i])(0, &byte);
        EXPECT_EQ(2, chunk.GetCurrentPos()); chunk.CloseChunk(); chunk.StartRead();
        EXPECT_EQ(0, chunk.ReadInt()); EXPECT_EQ(0, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, RawWriterNullAndZeroDoNotAdvance) {
    unsigned char byte = 0xA5;
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        (chunk.*RawWriters[i])(17, nullptr); (chunk.*RawWriters[i])(0, &byte);
        EXPECT_EQ(0, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, AllocatedBufferOwnsExactUnalignedPayload) {
    const int words[] = {5, 0x78563412, 0x6B6B6B9A, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
    void *buffer = nullptr; ASSERT_EQ(5, chunk.ReadBuffer(&buffer)); ASSERT_NE(nullptr, buffer);
    ExpectPayload(static_cast<unsigned char *>(buffer)); EXPECT_EQ(3, chunk.GetCurrentPos());
    static_cast<unsigned char *>(buffer)[0] = 0xFF;
    CKDeletePointer(buffer); EXPECT_EQ(Sentinel, chunk.ReadInt());
    chunk.StartRead(); ASSERT_EQ(5, chunk.ReadBuffer(&buffer));
    ExpectPayload(static_cast<unsigned char *>(buffer)); CKDeletePointer(buffer);
}

TEST(CKStateChunkBufferTest, EmptyAllocatedBufferClearsOutputAndConsumesHeader) {
    const int words[] = {0, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    int retained = 1; void *buffer = &retained;
    EXPECT_EQ(0, chunk.ReadBuffer(&buffer)); EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, AllocatedBufferEofAndMissingParserRetainOutput) {
    int retained = 1; void *buffer = &retained;
    CKStateChunk fresh(CKCID_OBJECT, nullptr); EXPECT_EQ(0, fresh.ReadBuffer(&buffer)); EXPECT_EQ(&retained, buffer);
    const int words[] = {Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 1); chunk.ReadInt();
    EXPECT_EQ(0, chunk.ReadBuffer(&buffer)); EXPECT_EQ(&retained, buffer); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, AllocatedBufferRejectsNegativeLengthWithoutReturningNegativeSize) {
    const int words[] = {-1, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    void *buffer = nullptr; EXPECT_EQ(0, chunk.ReadBuffer(&buffer)); EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, AllocatedBufferRejectsTruncatedPayloadBeforeAllocation) {
    const int words[] = {5, 0x78563412};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    int retained = 1; void *buffer = &retained;
    EXPECT_EQ(0, chunk.ReadBuffer(&buffer)); EXPECT_EQ(nullptr, buffer); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, StoredFillAliasesCopyOnlyDeclaredBytes) {
    const int words[] = {5, 0x78563412, 0x6B6B6B9A, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
        unsigned char guarded[] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
        (chunk.*StoredReaders[i])(guarded + 1); ExpectPayload(guarded + 1);
        EXPECT_EQ(0xCC, guarded[0]); EXPECT_EQ(0xCC, guarded[6]);
        EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, StoredFillNullDestinationSkipsWholePayload) {
    const int words[] = {5, 0x78563412, 0x6B6B6B9A, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
        (chunk.*StoredReaders[i])(nullptr); EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, StoredFillEmptyHeaderLeavesDestinationIntact) {
    const int words[] = {0, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
        unsigned char byte = 0xCC; (chunk.*StoredReaders[i])(&byte);
        EXPECT_EQ(0xCC, byte); EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, StoredFillTruncatedPayloadStopsWithoutCopy) {
    const int words[] = {5, 0x78563412};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
        unsigned char bytes[5] = {}; (chunk.*StoredReaders[i])(bytes);
        const unsigned char zero[5] = {}; EXPECT_EQ(0, std::memcmp(bytes, zero, 5)); EXPECT_EQ(2, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, StoredFillNegativeLengthStopsWithoutCopy) {
    const int words[] = {-1, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
        unsigned char byte = 0xCC; (chunk.*StoredReaders[i])(&byte);
        EXPECT_EQ(0xCC, byte); EXPECT_EQ(2, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, ExplicitFillAliasesDoNotReadLengthHeader) {
    const int words[] = {0x78563412, 0x6B6B6B9A, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
        unsigned char guarded[7]; std::memset(guarded, 0xCC, 7);
        (chunk.*ExplicitReaders[i])(5, guarded + 1); ExpectPayload(guarded + 1);
        EXPECT_EQ(0xCC, guarded[0]); EXPECT_EQ(0xCC, guarded[6]); EXPECT_EQ(2, chunk.GetCurrentPos());
        EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, ExplicitFillNullDestinationSkipsRoundedWords) {
    const int words[] = {0x78563412, 0x6B6B6B9A, Sentinel};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
        (chunk.*ExplicitReaders[i])(0, nullptr); EXPECT_EQ(0, chunk.GetCurrentPos());
        (chunk.*ExplicitReaders[i])(5, nullptr); EXPECT_EQ(2, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, ExplicitFillRejectsShortPayloadAndNegativeCount) {
    const int words[] = {0x78563412};
    for (int i = 0; i < 3; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 1);
        unsigned char bytes[5] = {}; (chunk.*ExplicitReaders[i])(-1, bytes); EXPECT_EQ(0, chunk.GetCurrentPos());
        (chunk.*ExplicitReaders[i])(5, bytes); const unsigned char zero[5] = {};
        EXPECT_EQ(0, std::memcmp(bytes, zero, 5)); EXPECT_EQ(1, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, ArrayWriterAliasesEmitByteLengthThenElementCount) {
    unsigned char bytes[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        if (i) chunk.WriteArray_LEndian16(2, 3, bytes); else chunk.WriteArray_LEndian(2, 3, bytes);
        EXPECT_EQ(4, chunk.GetCurrentPos()); chunk.WriteInt(Sentinel); chunk.CloseChunk();
        int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
        EXPECT_EQ(6, wire[2]); EXPECT_EQ(2, wire[3]);
        EXPECT_EQ(0, std::memcmp(wire + 4, bytes, 6)); EXPECT_EQ(Sentinel, wire[6]);
    }
}

TEST(CKStateChunkBufferTest, ArrayWritersUseTwoZeroWordsForEmptyInput) {
    unsigned char byte = 0xCC;
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        if (i) { chunk.WriteArray_LEndian16(3, 1, nullptr); chunk.WriteArray_LEndian16(0, 1, &byte); }
        else { chunk.WriteArray_LEndian(3, 1, nullptr); chunk.WriteArray_LEndian(0, 1, &byte); }
        chunk.CloseChunk(); chunk.StartRead(); for (int j = 0; j < 4; ++j) EXPECT_EQ(0, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, ArrayReaderAliasesOwnPayloadAndReturnIndependentElementCount) {
    // A legacy reader returns the explicit count, not byte-length / sizeof(T).
    const int words[] = {5, 7, 0x78563412, 0x6B6B6B9A, Sentinel};
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 5);
        void *buffer = nullptr; ASSERT_EQ(7, (chunk.*ArrayReaders[i])(&buffer)); ASSERT_NE(nullptr, buffer);
        ExpectPayload(static_cast<unsigned char *>(buffer)); CKDeletePointer(buffer);
        EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, ArrayEmptyAndNonPositiveCountConsumeOnlyTwoWordHeader) {
    const int words[] = {0, 0, 5, -1, Sentinel};
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 5);
        int retained = 1; void *buffer = &retained;
        EXPECT_EQ(0, (chunk.*ArrayReaders[i])(&buffer)); EXPECT_EQ(nullptr, buffer); EXPECT_EQ(2, chunk.GetCurrentPos());
        buffer = &retained; EXPECT_EQ(0, (chunk.*ArrayReaders[i])(&buffer)); EXPECT_EQ(nullptr, buffer);
        EXPECT_EQ(4, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}

TEST(CKStateChunkBufferTest, ArrayShortPayloadIsRejectedBeforeAllocating) {
    const int words[] = {5, 7, 0x78563412};
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
        void *buffer = nullptr; EXPECT_EQ(0, (chunk.*ArrayReaders[i])(&buffer));
        EXPECT_EQ(nullptr, buffer); EXPECT_EQ(2, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, ArrayIncompleteHeaderCannotAdvanceBeyondChunk) {
    const int words[] = {4};
    for (int i = 0; i < 2; ++i) {
        CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 1);
        void *buffer = nullptr; EXPECT_EQ(0, (chunk.*ArrayReaders[i])(&buffer));
        EXPECT_EQ(nullptr, buffer); EXPECT_EQ(1, chunk.GetCurrentPos());
    }
}

TEST(CKStateChunkBufferTest, ArrayEofAndMissingParserRetainOutput) {
    int retained = 1;
    for (int i = 0; i < 2; ++i) {
        void *buffer = &retained; CKStateChunk fresh(CKCID_OBJECT, nullptr);
        EXPECT_EQ(0, (fresh.*ArrayReaders[i])(&buffer)); EXPECT_EQ(&retained, buffer);
        const int words[] = {Sentinel}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
        LoadBufferWords(chunk, words, 1); chunk.ReadInt();
        EXPECT_EQ(0, (chunk.*ArrayReaders[i])(&buffer)); EXPECT_EQ(&retained, buffer);
    }
}

TEST(CKStateChunkBufferTest, StringWriterDistinguishesNullEmptyAndTerminatedUtf8Bytes) {
    char empty[] = ""; char text[] = "a\xC3\xA9";
    CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
    chunk.WriteString(nullptr); chunk.WriteString(empty); chunk.WriteString(text);
    EXPECT_EQ(5, chunk.GetCurrentPos()); chunk.WriteInt(Sentinel); chunk.CloseChunk();
    int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
    EXPECT_EQ(0, wire[2]); EXPECT_EQ(1, wire[3]); EXPECT_EQ(0, reinterpret_cast<unsigned char *>(wire + 4)[0]);
    EXPECT_EQ(4, wire[5]); EXPECT_EQ(0, std::memcmp(wire + 6, text, 4)); EXPECT_EQ(Sentinel, wire[7]);
}

TEST(CKStateChunkBufferTest, RawStringReaderOwnsFrameIncludingEmbeddedTerminator) {
    const int words[] = {5, 0x42410061, 0x7F7F7F00, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
    char *text = nullptr; ASSERT_EQ(5, chunk.ReadString(&text)); ASSERT_NE(nullptr, text);
    const char expected[] = {'a', 0, 'A', 'B', 0}; EXPECT_EQ(0, std::memcmp(text, expected, 5));
    CKDeletePointer(text); EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, RawStringNullAndEmptyHaveDifferentWireLengths) {
    const int words[] = {0, 1, 0x7F7F7F00, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
    char *text = nullptr; EXPECT_EQ(0, chunk.ReadString(&text)); EXPECT_EQ(nullptr, text);
    ASSERT_EQ(1, chunk.ReadString(&text)); ASSERT_NE(nullptr, text); EXPECT_EQ(0, text[0]);
    CKDeletePointer(text); EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, RawStringEofAndMissingParserRetainOutput) {
    char retained[] = "old"; char *text = retained; CKStateChunk fresh(CKCID_OBJECT, nullptr);
    EXPECT_EQ(0, fresh.ReadString(&text)); EXPECT_EQ(retained, text);
    const int words[] = {Sentinel}; CKStateChunk chunk(CKCID_OBJECT, nullptr);
    LoadBufferWords(chunk, words, 1); chunk.ReadInt(); EXPECT_EQ(0, chunk.ReadString(&text)); EXPECT_EQ(retained, text);
}

TEST(CKStateChunkBufferTest, RawStringNegativeLengthIsFailureNotNegativeReturn) {
    const int words[] = {-1, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    char *text = nullptr; EXPECT_EQ(0, chunk.ReadString(&text)); EXPECT_EQ(nullptr, text);
    EXPECT_EQ(1, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, RawStringTruncatedFrameIsRejectedBeforeAllocation) {
    const int words[] = {5, 0x00636261};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    char retained[] = "old"; char *text = retained; EXPECT_EQ(0, chunk.ReadString(&text));
    EXPECT_EQ(nullptr, text); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, XStringReadsLiteralUtf8AndConsumesTerminatorPadding) {
    const int words[] = {4, 0x00A9C361, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
    XString text; chunk.ReadString(text); EXPECT_STREQ("a\xC3\xA9", text.CStr());
    EXPECT_EQ(2, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, XStringEmptyAndEofClearPreviousValue) {
    const int words[] = {0}; CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 1);
    XString text("old"); chunk.ReadString(text); EXPECT_EQ(0, text.Length()); EXPECT_EQ(1, chunk.GetCurrentPos());
    text = "old"; chunk.ReadString(text); EXPECT_EQ(0, text.Length());
    CKStateChunk fresh(CKCID_OBJECT, nullptr); text = "old"; fresh.ReadString(text); EXPECT_EQ(0, text.Length());
}

TEST(CKStateChunkBufferTest, XStringUsesFirstNullButConsumesFullDeclaredFrame) {
    const int words[] = {5, 0x42410061, 0x7F7F7F00, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 4);
    XString text; chunk.ReadString(text); EXPECT_STREQ("a", text.CStr());
    EXPECT_EQ(3, chunk.GetCurrentPos()); EXPECT_EQ(Sentinel, chunk.ReadInt());
}

TEST(CKStateChunkBufferTest, XStringCannotUseTerminatorInNextRecord) {
    const int words[] = {4, 0x44434241, 0};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
    XString text("old"); chunk.ReadString(text); EXPECT_EQ(0, text.Length()); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, XStringNegativeLengthCannotReadFollowingPayload) {
    const int words[] = {-1, 0x00646142, Sentinel};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 3);
    XString text("old"); chunk.ReadString(text); EXPECT_EQ(0, text.Length()); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, XStringRejectsShortDeclaredFrameEvenWithEarlyNull) {
    const int words[] = {5, 0x00636261};
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadBufferWords(chunk, words, 2);
    XString text("old"); chunk.ReadString(text); EXPECT_EQ(0, text.Length()); EXPECT_EQ(1, chunk.GetCurrentPos());
}

TEST(CKStateChunkBufferTest, BufferWritersClearOnlyAlignmentPaddingInReusedStorage) {
    unsigned char bytes[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    for (int sized = 0; sized < 2; ++sized) {
        for (int alias = 0; alias < 3; ++alias) {
            for (int size = 1; size <= 8; ++size) {
                CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
                void *storage = chunk.LockWriteBuffer(16); ASSERT_NE(nullptr, storage);
                std::memset(storage, 0xA5, 64);
                (chunk.*(sized ? SizedWriters[alias] : RawWriters[alias]))(size, bytes);
                chunk.WriteInt(Sentinel); chunk.CloseChunk();
                int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
                const unsigned char *payload = reinterpret_cast<unsigned char *>(wire + 2 + sized);
                EXPECT_EQ(0, std::memcmp(bytes, payload, size));
                const int padded = ((size + 3) / 4) * 4;
                for (int i = size; i < padded; ++i) EXPECT_EQ(0, payload[i]);
                EXPECT_EQ(Sentinel, wire[2 + sized + padded / 4]);
            }
        }
    }
}

TEST(CKStateChunkBufferTest, StringWriterPreservesAnsiBytesAndClearsAlignmentPadding) {
    const char bytes[] = {'A', char(0xD6), char(0xD0), char(0xE9), char(0x81), 'B', 'C'};
    for (int length = 0; length <= 7; ++length) {
        char text[8] = {}; std::memcpy(text, bytes, length);
        CKStateChunk chunk(CKCID_OBJECT, nullptr); chunk.StartWrite();
        void *storage = chunk.LockWriteBuffer(16); ASSERT_NE(nullptr, storage);
        std::memset(storage, 0xA5, 64);
        chunk.WriteString(text); chunk.WriteInt(Sentinel); chunk.CloseChunk();
        int wire[64] = {}; ASSERT_GT(chunk.ConvertToBuffer(wire), 0);
        const unsigned char *payload = reinterpret_cast<unsigned char *>(wire + 3);
        EXPECT_EQ(length + 1, wire[2]);
        EXPECT_EQ(0, std::memcmp(text, payload, length + 1));
        for (int i = length + 1; i < ((length + 4) / 4) * 4; ++i) EXPECT_EQ(0, payload[i]);
        chunk.StartRead(); char *loaded = nullptr;
        ASSERT_EQ(length + 1, chunk.ReadString(&loaded)); ASSERT_NE(nullptr, loaded);
        EXPECT_EQ(0, std::memcmp(text, loaded, length + 1)); CKDeletePointer(loaded);
        EXPECT_EQ(Sentinel, chunk.ReadInt());
    }
}
