#include <gtest/gtest.h>

#include <cstring>

#include "CKAll.h"

// Literal v7 input: two identifier blocks, single/sequence ID entries, an
// embedded child and single/sequence manager entries. Offsets are DWORDs.
static const int kSource[] = {
    0x07073923, 27,
    0x1111, 4, 41, 0x5555,
    0x2222, 0, 2, 41, 42,
    9, 7, 0x00070003, 1, 0, 1, 0, 0, 41, 0,
    0x12345678, 0x23456789, 2,
    2, 0x12345678, 0x23456789, 1, 2,
    3, 2, -1, 6,
    1, 9,
    3, 19, -1, 22
};

static void ExpectWire(CKStateChunk &chunk, const int *expected, int words) {
    int actual[96] = {};
    ASSERT_EQ(words * static_cast<int>(sizeof(int)), chunk.ConvertToBuffer(nullptr));
    ASSERT_EQ(words * static_cast<int>(sizeof(int)), chunk.ConvertToBuffer(actual));
    for (int i = 0; i < words; ++i) EXPECT_EQ(expected[i], actual[i]) << "word " << i;
}

static void LoadSource(CKStateChunk &chunk) {
    int input[sizeof(kSource) / sizeof(kSource[0])];
    std::memcpy(input, kSource, sizeof(input));
    ASSERT_TRUE(chunk.ConvertFromBuffer(input));
}

TEST(CKStateChunkAppendTest, AppendRelocatesIdentifierLinksAndAllThreeLists) {
    CKStateChunk source(57, nullptr), destination(31, nullptr);
    LoadSource(source);
    destination.StartWrite();
    destination.WriteIdentifier(0xAAAA);
    destination.WriteInt(99);
    destination.AddChunk(&source);
    EXPECT_EQ(30, destination.GetCurrentPos());
    destination.CloseChunk();
    const int expected[] = {
        0x07071F23, 30,
        0xAAAA, 3, 99,
        0x1111, 7, 41, 0x5555,
        0x2222, 0, 2, 41, 42,
        9, 7, 0x00070003, 1, 0, 1, 0, 0, 41, 0,
        0x12345678, 0x23456789, 2,
        2, 0x12345678, 0x23456789, 1, 2,
        3, 5, -1, 9,
        1, 12,
        3, 22, -1, 25
    };
    ExpectWire(destination, expected, sizeof(expected) / sizeof(expected[0]));
    ExpectWire(source, kSource, sizeof(kSource) / sizeof(kSource[0]));
    destination.StartRead();
    ASSERT_TRUE(destination.SeekIdentifier(0x1111));
    EXPECT_EQ(41u, destination.ReadObjectID());
    ASSERT_TRUE(destination.SeekIdentifier(0x2222));
    EXPECT_EQ(2, destination.StartReadSequence());
    EXPECT_EQ(41u, destination.ReadObjectID());
    EXPECT_EQ(42u, destination.ReadObjectID());
    CKStateChunk *child = destination.ReadSubChunk();
    ASSERT_NE(nullptr, child);
    child->StartRead();
    EXPECT_EQ(41u, child->ReadObjectID());
    DeleteCKStateChunk(child);
}

TEST(CKStateChunkAppendTest, DeleteAppendIntoNonemptyDestinationPreservesIndependentSourceData) {
    CKStateChunk *source = CreateCKStateChunk(57, nullptr);
    LoadSource(*source);
    CKStateChunk destination(31, nullptr);
    destination.StartWrite();
    destination.WriteIdentifier(0xAAAA);
    destination.WriteInt(99);
    destination.AddChunkAndDelete(source);
    destination.CloseChunk();
    destination.StartRead();
    ASSERT_TRUE(destination.SeekIdentifier(0xAAAA));
    EXPECT_EQ(99, destination.ReadInt());
    ASSERT_TRUE(destination.SeekIdentifier(0x1111));
    EXPECT_EQ(41u, destination.ReadObjectID());
    EXPECT_EQ(0x5555, destination.ReadInt());
    ASSERT_TRUE(destination.SeekIdentifier(0x2222));
    EXPECT_EQ(2, destination.StartReadSequence());
    EXPECT_EQ(41u, destination.ReadObjectID());
    EXPECT_EQ(42u, destination.ReadObjectID());
}

TEST(CKStateChunkAppendTest, EmptyDestinationTakesClosedSourceListsAndPayload) {
    CKStateChunk *source = CreateCKStateChunk(57, nullptr);
    LoadSource(*source);
    CKStateChunk destination(57, nullptr);
    destination.StartWrite();
    destination.AddChunkAndDelete(source);
    EXPECT_EQ(0, destination.GetCurrentPos());
    EXPECT_EQ(27 * static_cast<int>(sizeof(int)), destination.GetDataSize());
    destination.CloseChunk();
    ExpectWire(destination, kSource, sizeof(kSource) / sizeof(kSource[0]));
}

TEST(CKStateChunkAppendTest, EmptyDestinationTakesOpenSourceParserAndAllowsFurtherWriting) {
    CKStateChunk *source = CreateCKStateChunk(57, nullptr);
    source->StartWrite();
    source->SetDataVersion(35);
    source->WriteIdentifier(0x1111);
    source->WriteInt(41);
    source->UpdateDataSize();
    CKStateChunk destination(31, nullptr);
    destination.StartWrite();
    destination.AddChunkAndDelete(source);
    EXPECT_EQ(3, destination.GetCurrentPos());
    destination.WriteIdentifier(0x2222);
    destination.WriteInt(42);
    destination.CloseChunk();
    const int expected[] = {0x00071F23, 6, 0x1111, 3, 41, 0x2222, 0, 42};
    ExpectWire(destination, expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(CKStateChunkAppendTest, AppendRequiresDestinationParserAndUsableSource) {
    CKStateChunk source(57, nullptr), destination(31, nullptr), empty(57, nullptr);
    LoadSource(source);
    destination.AddChunk(&source);
    EXPECT_EQ(0, destination.GetDataSize());
    destination.StartWrite();
    destination.WriteIdentifier(0xAAAA);
    destination.WriteInt(99);
    destination.AddChunk(nullptr);
    destination.AddChunk(&empty);
    destination.CloseChunk();
    const int expected[] = {0x00071F00, 3, 0xAAAA, 0, 99};
    ExpectWire(destination, expected, sizeof(expected) / sizeof(expected[0]));
    ExpectWire(source, kSource, sizeof(kSource) / sizeof(kSource[0]));
}

TEST(CKStateChunkAppendTest, DeleteAppendDoesNotConsumeUnusableSource) {
    CKStateChunk destination(31, nullptr);
    CKStateChunk *source = CreateCKStateChunk(57, nullptr);
    LoadSource(*source);
    destination.AddChunkAndDelete(source);
    ExpectWire(*source, kSource, sizeof(kSource) / sizeof(kSource[0]));
    DeleteCKStateChunk(source);
    destination.StartWrite();
    CKStateChunk *empty = CreateCKStateChunk(57, nullptr);
    destination.AddChunkAndDelete(empty);
    // A source with no payload is not consumed by this API.
    EXPECT_EQ(57, empty->GetChunkClassID());
    DeleteCKStateChunk(empty);
    destination.AddChunkAndDelete(nullptr);
    destination.CloseChunk();
    EXPECT_EQ(0, destination.GetDataSize());
}

TEST(CKStateChunkAppendTest, IntListAppendRelocatesNonnegativeOffsetsAndPreservesMarkers) {
    IntListStruct destination, source;
    destination.AddEntry(2);
    source.AddEntry(-1);
    source.AddEntry(0);
    source.AddEntry(9);
    source.AddEntry(-7);
    destination.Append(&source, 12);
    const int expected[] = {2, -1, 12, 21, -7};
    ASSERT_EQ(5, destination.Size);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(expected[i], destination.Data[i]);
    ASSERT_EQ(4, source.Size);
    EXPECT_EQ(0, source.Data[1]);
    EXPECT_EQ(9, source.Data[2]);
    destination.Append(nullptr, 99);
    EXPECT_EQ(5, destination.Size);
    IntListStruct empty;
    destination.Append(&empty, 99);
    EXPECT_EQ(5, destination.Size);
}

TEST(CKStateChunkAppendTest, DeleteAppendCopiesDynamicPolicyWhilePlainAppendKeepsDestinationPolicy) {
    ASSERT_EQ(CK_OK, CKStartUp());
    CKContext *context = nullptr;
    ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    {
        CKObject object(context, "DynamicAppendProbe");
        object.ModifyObjectFlags(CK_OBJECT_DYNAMIC, 0);
        for (int consume = 0; consume < 2; ++consume) {
            CKStateChunk *source = CreateCKStateChunk(57, nullptr);
            source->StartWrite();
            source->SetDynamic(FALSE);
            source->WriteIdentifier(0x1111);
            source->WriteInt(41);
            source->UpdateDataSize();
            CKStateChunk destination(31, nullptr);
            destination.StartWrite();
            if (consume) {
                destination.AddChunkAndDelete(source);
            } else {
                destination.AddChunk(source);
                DeleteCKStateChunk(source);
            }
            destination.WriteIdentifier(0x2222);
            destination.WriteObject(&object);
            destination.CloseChunk();
            destination.StartRead();
            ASSERT_TRUE(destination.SeekIdentifier(0x2222));
            EXPECT_EQ(consume ? 0u : object.GetID(), destination.ReadObjectID());
        }
    }
    CKCloseContext(context);
    CKShutdown();
}

TEST(CKStateChunkAppendTest, BothAppendPathsAdoptSourceFileAssociation) {
    ASSERT_EQ(CK_OK, CKStartUp());
    CKContext *context = nullptr;
    ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    {
        CKFile file(context);
        for (int consume = 0; consume < 2; ++consume) {
            int input[sizeof(kSource) / sizeof(kSource[0])];
            std::memcpy(input, kSource, sizeof(input));
            input[0] = 0x0F073923;
            CKStateChunk *source = CreateCKStateChunk(57, &file);
            ASSERT_TRUE(source->ConvertFromBuffer(input));
            CKStateChunk destination(57, nullptr);
            destination.StartWrite();
            if (consume) {
                destination.AddChunkAndDelete(source);
            } else {
                destination.AddChunk(source);
                DeleteCKStateChunk(source);
            }
            destination.CloseChunk();
            ExpectWire(destination, input, sizeof(input) / sizeof(input[0]));
        }
    }
    CKCloseContext(context);
    CKShutdown();
}
