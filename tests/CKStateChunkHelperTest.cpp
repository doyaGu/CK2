#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

#include "CKAll.h"

namespace {

// Independent ABI description; inspect object bytes without redefining access
// control, modifying private state, or pretending Win64 has Win32 pointer widths.
struct ChunkLayout {
    int ClassId;
    int PublishedWords;
    int *Data;
    short DataVersion;
    short ChunkVersion;
    ChunkParser *Parser;
    IntListStruct *Ids;
    IntListStruct *Chunks;
    IntListStruct *Managers;
    CKFile *File;
    CKBOOL Dynamic;
};

ChunkLayout Describe(const CKStateChunk &chunk) {
    ChunkLayout description = {};
    static_assert(sizeof(ChunkLayout) == sizeof(CKStateChunk), "chunk ABI description size");
    std::memcpy(&description, &chunk, sizeof(description));
    return description;
}

void LoadOwnedLiteral(CKStateChunk &chunk) {
    int wire[] = {(7 << 24) | (CHUNK_VERSION4 << 16) | (CKCID_PARAMETER << 8) | 0x5A,
                  6, 0x1122, 3, 41, 0x3344, 0, 43, 2, 2, 5, 1, 4, 1, 2};
    ASSERT_TRUE(chunk.ConvertFromBuffer(wire));
}

TEST(CKStateChunkHelperTest, ListDefaultConstructorHasNoAllocationOrValues) {
    IntListStruct list;
    EXPECT_EQ(0, list.Size); EXPECT_EQ(0, list.AllocatedSize); EXPECT_EQ(nullptr, list.Data);
}

TEST(CKStateChunkHelperTest, SingleGrowthPreservesSignedValuesAndReportedCapacity) {
    IntListStruct list;
    int expectedCapacity = 0;
    for (int i = 0; i < 257; ++i) {
        if (i + 1 >= expectedCapacity) expectedCapacity = (expectedCapacity + 1) * 2;
        list.AddEntry(i % 2 ? -i : i);
        EXPECT_EQ(i + 1, list.Size); EXPECT_EQ(expectedCapacity, list.AllocatedSize);
        for (int j = 0; j <= i; ++j) EXPECT_EQ(j % 2 ? -j : j, list.Data[j]);
    }
}

TEST(CKStateChunkHelperTest, PairGrowthPreservesMarkersAndOffsets) {
    IntListStruct list;
    int expectedCapacity = 0;
    for (int i = 0; i < 129; ++i) {
        if (2 * i + 2 >= expectedCapacity) expectedCapacity = (expectedCapacity + 1) * 2;
        list.AddEntries(i * 3);
        EXPECT_EQ(2 * i + 2, list.Size); EXPECT_EQ(expectedCapacity, list.AllocatedSize);
        for (int j = 0; j <= i; ++j) {
            EXPECT_EQ(-1, list.Data[2 * j]); EXPECT_EQ(j * 3, list.Data[2 * j + 1]);
        }
    }
}

TEST(CKStateChunkHelperTest, InterleavedGrowthRetainsLiteralSequence) {
    IntListStruct list;
    int expected[300];
    int count = 0;
    for (int i = 0; i < 100; ++i) {
        list.AddEntry(i); expected[count++] = i;
        list.AddEntries(i + 400); expected[count++] = -1; expected[count++] = i + 400;
    }
    ASSERT_EQ(300, list.Size); EXPECT_GE(list.AllocatedSize, list.Size);
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], list.Data[i]);
}

TEST(CKStateChunkHelperTest, CopyConstructorUsesCompactIndependentStorage) {
    IntListStruct source;
    source.AddEntry(17); source.AddEntries(23); source.AddEntry(-29);
    IntListStruct copy(source);
    ASSERT_EQ(4, copy.Size); EXPECT_EQ(4, copy.AllocatedSize);
    ASSERT_NE(source.Data, copy.Data);
    const int expected[] = {17, -1, 23, -29};
    for (int i = 0; i < 4; ++i) EXPECT_EQ(expected[i], copy.Data[i]);
    source.Data[0] = 31; copy.Data[2] = 37;
    EXPECT_EQ(17, copy.Data[0]); EXPECT_EQ(23, source.Data[2]);
    copy.AddEntry(41); EXPECT_EQ(4, source.Size); EXPECT_EQ(5, copy.Size);
    EXPECT_EQ(41, copy.Data[4]);
}

TEST(CKStateChunkHelperTest, CopyOfEmptyListCanGrowWithoutChangingSource) {
    IntListStruct source;
    IntListStruct copy(source);
    EXPECT_EQ(0, copy.Size); EXPECT_EQ(0, copy.AllocatedSize);
    copy.AddEntries(43);
    ASSERT_EQ(2, copy.Size); EXPECT_EQ(-1, copy.Data[0]); EXPECT_EQ(43, copy.Data[1]);
    EXPECT_EQ(0, source.Size); EXPECT_EQ(nullptr, source.Data);
}

TEST(CKStateChunkHelperTest, ListCopySurvivesSourceDestruction) {
    IntListStruct *copy;
    {
        IntListStruct source;
        source.AddEntry(47); source.AddEntries(53);
        copy = new IntListStruct(source);
    }
    ASSERT_EQ(3, copy->Size);
    EXPECT_EQ(47, copy->Data[0]); EXPECT_EQ(-1, copy->Data[1]); EXPECT_EQ(53, copy->Data[2]);
    copy->AddEntry(59); EXPECT_EQ(59, copy->Data[3]);
    delete copy;
}

TEST(CKStateChunkHelperTest, ParserDestructorDoesNotOwnOrModifyCallerStorage) {
    struct Storage { int Before; alignas(ChunkParser) unsigned char Bytes[sizeof(ChunkParser)]; int After; } storage;
    storage.Before = 61; storage.After = 67;
    ChunkParser *parser = new (storage.Bytes) ChunkParser;
    parser->CurrentPos = 3; parser->DataSize = 17; parser->PrevIdentifierPos = 7;
    parser->~ChunkParser();
    EXPECT_EQ(61, storage.Before); EXPECT_EQ(67, storage.After);
    parser = new (storage.Bytes) ChunkParser;
    EXPECT_EQ(0, parser->CurrentPos); EXPECT_EQ(0, parser->DataSize); EXPECT_EQ(0, parser->PrevIdentifierPos);
    parser->~ChunkParser();
}

TEST(CKStateChunkHelperTest, IteratorConstructorInitializesEveryFieldAndOnlyItsStorage) {
    struct Storage { int Before; alignas(ChunkIteratorData) unsigned char Bytes[sizeof(ChunkIteratorData)]; int After; } storage;
    storage.Before = 71; storage.After = 73;
    std::memset(storage.Bytes, 0xA5, sizeof(storage.Bytes));
    ChunkIteratorData *iterator = new (storage.Bytes) ChunkIteratorData;
    EXPECT_EQ(0, iterator->ChunkVersion); EXPECT_EQ(nullptr, iterator->Data);
    EXPECT_EQ(0, iterator->ChunkSize); EXPECT_EQ(FALSE, iterator->Flag);
    EXPECT_EQ(nullptr, iterator->Ids); EXPECT_EQ(0, iterator->IdCount);
    EXPECT_EQ(nullptr, iterator->Chunks); EXPECT_EQ(0, iterator->ChunkCount);
    EXPECT_EQ(nullptr, iterator->Managers); EXPECT_EQ(0, iterator->ManagerCount);
    EXPECT_EQ(CKGUID(0, 0), iterator->Guid); EXPECT_EQ(nullptr, iterator->ConversionTable);
    EXPECT_EQ(0, iterator->NbEntries); EXPECT_EQ(nullptr, iterator->DepContext); EXPECT_EQ(nullptr, iterator->Context);
    EXPECT_EQ(71, storage.Before); EXPECT_EQ(73, storage.After);
    iterator->~ChunkIteratorData();
}

TEST(CKStateChunkHelperTest, ListAndParserPlatformLayouts) {
    ASSERT_TRUE(std::is_standard_layout<IntListStruct>::value);
    ASSERT_TRUE(std::is_standard_layout<ChunkParser>::value);
    EXPECT_EQ(sizeof(void *) == 8 ? 16u : 12u, sizeof(IntListStruct));
    EXPECT_EQ(alignof(void *), alignof(IntListStruct));
    EXPECT_EQ(0u, offsetof(IntListStruct, Size)); EXPECT_EQ(4u, offsetof(IntListStruct, AllocatedSize));
    EXPECT_EQ(8u, offsetof(IntListStruct, Data));
    EXPECT_EQ(12u, sizeof(ChunkParser)); EXPECT_EQ(4u, alignof(ChunkParser));
    EXPECT_EQ(0u, offsetof(ChunkParser, CurrentPos)); EXPECT_EQ(4u, offsetof(ChunkParser, DataSize));
    EXPECT_EQ(8u, offsetof(ChunkParser, PrevIdentifierPos));
}

TEST(CKStateChunkHelperTest, IteratorPlatformFieldOffsets) {
    ASSERT_TRUE(std::is_standard_layout<ChunkIteratorData>::value);
    const bool wide = sizeof(void *) == 8;
    EXPECT_EQ(wide ? 112u : 64u, sizeof(ChunkIteratorData));
    EXPECT_EQ(alignof(void *), alignof(ChunkIteratorData));
    EXPECT_EQ(0u, offsetof(ChunkIteratorData, ChunkVersion));
    EXPECT_EQ(wide ? 8u : 4u, offsetof(ChunkIteratorData, Data));
    EXPECT_EQ(wide ? 16u : 8u, offsetof(ChunkIteratorData, ChunkSize));
    EXPECT_EQ(wide ? 20u : 12u, offsetof(ChunkIteratorData, Flag));
    EXPECT_EQ(wide ? 24u : 16u, offsetof(ChunkIteratorData, Ids));
    EXPECT_EQ(wide ? 32u : 20u, offsetof(ChunkIteratorData, IdCount));
    EXPECT_EQ(wide ? 40u : 24u, offsetof(ChunkIteratorData, Chunks));
    EXPECT_EQ(wide ? 48u : 28u, offsetof(ChunkIteratorData, ChunkCount));
    EXPECT_EQ(wide ? 56u : 32u, offsetof(ChunkIteratorData, Managers));
    EXPECT_EQ(wide ? 64u : 36u, offsetof(ChunkIteratorData, ManagerCount));
    EXPECT_EQ(wide ? 68u : 40u, offsetof(ChunkIteratorData, Guid));
    EXPECT_EQ(wide ? 80u : 48u, offsetof(ChunkIteratorData, ConversionTable));
    EXPECT_EQ(wide ? 88u : 52u, offsetof(ChunkIteratorData, NbEntries));
    EXPECT_EQ(wide ? 96u : 56u, offsetof(ChunkIteratorData, DepContext));
    EXPECT_EQ(wide ? 104u : 60u, offsetof(ChunkIteratorData, Context));
}

TEST(CKStateChunkHelperTest, ChunkPlatformLayoutAndEmptyPrivateRepresentation) {
    ASSERT_TRUE(std::is_standard_layout<CKStateChunk>::value);
    const bool wide = sizeof(void *) == 8;
    EXPECT_EQ(wide ? 72u : 40u, sizeof(CKStateChunk)); EXPECT_EQ(alignof(void *), alignof(CKStateChunk));
    EXPECT_EQ(0u, offsetof(ChunkLayout, ClassId)); EXPECT_EQ(4u, offsetof(ChunkLayout, PublishedWords));
    EXPECT_EQ(8u, offsetof(ChunkLayout, Data));
    EXPECT_EQ(wide ? 16u : 12u, offsetof(ChunkLayout, DataVersion));
    EXPECT_EQ(wide ? 18u : 14u, offsetof(ChunkLayout, ChunkVersion));
    EXPECT_EQ(wide ? 24u : 16u, offsetof(ChunkLayout, Parser));
    EXPECT_EQ(wide ? 32u : 20u, offsetof(ChunkLayout, Ids));
    EXPECT_EQ(wide ? 40u : 24u, offsetof(ChunkLayout, Chunks));
    EXPECT_EQ(wide ? 48u : 28u, offsetof(ChunkLayout, Managers));
    EXPECT_EQ(wide ? 56u : 32u, offsetof(ChunkLayout, File));
    EXPECT_EQ(wide ? 64u : 36u, offsetof(ChunkLayout, Dynamic));
    CKStateChunk chunk(CKCID_PARAMETER, nullptr);
    const ChunkLayout layout = Describe(chunk);
    EXPECT_EQ(CKCID_PARAMETER, layout.ClassId); EXPECT_EQ(0, layout.PublishedWords);
    EXPECT_EQ(nullptr, layout.Data); EXPECT_EQ(0, layout.DataVersion); EXPECT_EQ(0, layout.ChunkVersion);
    EXPECT_EQ(nullptr, layout.Parser); EXPECT_EQ(nullptr, layout.Ids); EXPECT_EQ(nullptr, layout.Chunks);
    EXPECT_EQ(nullptr, layout.Managers); EXPECT_EQ(nullptr, layout.File); EXPECT_EQ(TRUE, layout.Dynamic);
}

TEST(CKStateChunkHelperTest, ChunkPrivateOffsetsMatchActualPayloadListsAndParser) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr); LoadOwnedLiteral(chunk); chunk.StartRead();
    const ChunkLayout layout = Describe(chunk);
    EXPECT_EQ(CKCID_PARAMETER, layout.ClassId); EXPECT_EQ(6, layout.PublishedWords);
    EXPECT_EQ(0x5A, layout.DataVersion); EXPECT_EQ(CHUNK_VERSION4, layout.ChunkVersion);
    EXPECT_EQ(chunk.LockReadBuffer(), layout.Data);
    EXPECT_EQ(chunk.GetIds(), layout.Ids); EXPECT_EQ(chunk.GetChunks(), layout.Chunks);
    EXPECT_EQ(chunk.GetManagers(), layout.Managers);
    ASSERT_NE(nullptr, layout.Parser);
    EXPECT_EQ(0, layout.Parser->CurrentPos); EXPECT_EQ(6, layout.Parser->DataSize);
    EXPECT_EQ(0, layout.Parser->PrevIdentifierPos);
    chunk.SetDynamic(FALSE); EXPECT_EQ(FALSE, Describe(chunk).Dynamic);
    EXPECT_EQ(0x1122, chunk.ReadInt()); EXPECT_EQ(1, layout.Parser->CurrentPos);
}

TEST(CKStateChunkHelperTest, WriterCapacityIsSeparateFromPublishedWords) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite(); chunk.WriteInt(79);
    ChunkLayout layout = Describe(chunk);
    ASSERT_NE(nullptr, layout.Parser); EXPECT_EQ(0, layout.PublishedWords);
    EXPECT_EQ(1, layout.Parser->CurrentPos); EXPECT_GE(layout.Parser->DataSize, 1);
    chunk.UpdateDataSize(); EXPECT_EQ(1, Describe(chunk).PublishedWords);
    chunk.CloseChunk(); layout = Describe(chunk);
    EXPECT_EQ(1, layout.PublishedWords); EXPECT_EQ(nullptr, layout.Parser);
    ASSERT_NE(nullptr, layout.Data); EXPECT_EQ(79, layout.Data[0]);
}

TEST(CKStateChunkHelperTest, EmbeddedFileChunkLayoutDoesNotInventDefaultConstruction) {
    ASSERT_TRUE(std::is_standard_layout<CKFileChunk>::value);
    EXPECT_FALSE(std::is_default_constructible<CKFileChunk>::value);
    EXPECT_EQ(0u, offsetof(CKFileChunk, chunk));
    EXPECT_EQ(sizeof(CKStateChunk), offsetof(CKFileChunk, parser));
    EXPECT_EQ(sizeof(void *) == 8 ? 88u : 52u, sizeof(CKFileChunk));
    EXPECT_EQ(alignof(CKStateChunk), alignof(CKFileChunk));
    CKFileChunk combined = {CKStateChunk(CKCID_PARAMETER, nullptr), ChunkParser()};
    combined.parser.CurrentPos = 7; combined.parser.DataSize = 11; combined.parser.PrevIdentifierPos = 3;
    combined.chunk.StartWrite(); combined.chunk.WriteInt(89); combined.chunk.CloseChunk(); combined.chunk.StartRead();
    EXPECT_EQ(89, combined.chunk.ReadInt());
    EXPECT_EQ(7, combined.parser.CurrentPos); EXPECT_EQ(11, combined.parser.DataSize);
    EXPECT_EQ(3, combined.parser.PrevIdentifierPos);
}

TEST(CKStateChunkHelperTest, ExplicitPointerCloneSurvivesAllSourceOwnerDestruction) {
    CKStateChunk *copy;
    {
        CKStateChunk source(CKCID_OBJECT, nullptr); LoadOwnedLiteral(source); source.StartRead();
        copy = CreateCKStateChunk(&source);
    }
    ASSERT_NE(nullptr, copy); ASSERT_NE(nullptr, copy->GetIds()); ASSERT_NE(nullptr, copy->GetChunks());
    ASSERT_NE(nullptr, copy->GetManagers());
    EXPECT_EQ(0x1122, copy->ReadInt()); EXPECT_EQ(2, copy->GetIds()->Size);
    EXPECT_EQ(5, copy->GetIds()->Data[1]); EXPECT_EQ(4, copy->GetChunks()->Data[0]);
    EXPECT_EQ(2, copy->GetManagers()->Data[0]);
    copy->GetIds()->AddEntry(83); EXPECT_EQ(83, copy->GetIds()->Data[2]);
    DeleteCKStateChunk(copy);
}

} // namespace
