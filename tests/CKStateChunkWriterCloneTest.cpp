#include <gtest/gtest.h>

#include "CKAll.h"

namespace {

void ExpectOpenCloneWords(CKStateChunk &chunk, const int *expected, int count) {
    ASSERT_EQ(count * 4, chunk.GetDataSize());
    chunk.StartRead();
    for (int i = 0; i < count; ++i) EXPECT_EQ(expected[i], chunk.ReadInt()) << i;
}

TEST(CKStateChunkWriterCloneTest, MethodCopiesUnpublishedWriterAndBothCanContinue) {
    CKStateChunk source(CKCID_PARAMETER, nullptr), copy(CKCID_OBJECT, nullptr);
    source.StartWrite(); source.WriteInt(41); source.WriteInt(43); source.WriteInt(47);
    ASSERT_EQ(0, source.GetDataSize());
    copy.Clone(&source);
    EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(3, copy.GetCurrentPos());
    copy.WriteInt(53); copy.CloseChunk(); source.WriteInt(59); source.CloseChunk();
    const int expectedCopy[] = {41, 43, 47, 53}, expectedSource[] = {41, 43, 47, 59};
    ExpectOpenCloneWords(copy, expectedCopy, 4); ExpectOpenCloneWords(source, expectedSource, 4);
}

TEST(CKStateChunkWriterCloneTest, PointerConstructorDoesNotPublishUnpublishedSource) {
    CKStateChunk source(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.SetDataVersion(17); source.WriteInt(41); source.WriteInt(43);
    CKStateChunk copy(&source);
    EXPECT_EQ(CKCID_PARAMETER, copy.GetChunkClassID()); EXPECT_EQ(17, copy.GetDataVersion());
    EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(2, copy.GetCurrentPos());
    copy.WriteInt(47); copy.CloseChunk();
    EXPECT_EQ(0, source.GetDataSize()); EXPECT_EQ(2, source.GetCurrentPos());
    const int expected[] = {41, 43, 47}; ExpectOpenCloneWords(copy, expected, 3);
    source.CloseChunk(); const int original[] = {41, 43}; ExpectOpenCloneWords(source, original, 2);
}

TEST(CKStateChunkWriterCloneTest, FactoryUnpublishedCopySurvivesSourceDeletion) {
    CKStateChunk *source = CreateCKStateChunk(CKCID_OBJECT, nullptr);
    ASSERT_NE(nullptr, source);
    source->StartWrite(); source->WriteInt(41); source->WriteInt(43);
    CKStateChunk *copy = CreateCKStateChunk(source);
    ASSERT_NE(nullptr, copy); EXPECT_EQ(0, copy->GetDataSize()); EXPECT_EQ(2, copy->GetCurrentPos());
    DeleteCKStateChunk(source);
    copy->WriteInt(47); copy->CloseChunk();
    const int expected[] = {41, 43, 47}; ExpectOpenCloneWords(*copy, expected, 3);
    DeleteCKStateChunk(copy);
}

TEST(CKStateChunkWriterCloneTest, PartialPublicationKeepsSizeWhileCopyingLaterWrittenWords) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.WriteInt(41); source.UpdateDataSize();
    source.WriteInt(43); source.WriteInt(47);
    ASSERT_EQ(4, source.GetDataSize());
    copy.Clone(&source);
    EXPECT_EQ(4, copy.GetDataSize()); EXPECT_EQ(3, copy.GetCurrentPos());
    copy.WriteInt(53); copy.CloseChunk();
    EXPECT_EQ(4, source.GetDataSize()); EXPECT_EQ(3, source.GetCurrentPos());
    const int expected[] = {41, 43, 47, 53}; ExpectOpenCloneWords(copy, expected, 4);
    source.CloseChunk(); const int original[] = {41, 43, 47}; ExpectOpenCloneWords(source, original, 3);
}

TEST(CKStateChunkWriterCloneTest, LargeUnpublishedWriterRetainsPrefixAcrossGrowth) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite();
    for (int i = 0; i < 1201; ++i) source.WriteInt(0x10000 + i);
    ASSERT_EQ(0, source.GetDataSize());
    copy.Clone(&source); EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(1201, copy.GetCurrentPos());
    copy.WriteInt(73); copy.CloseChunk();
    ASSERT_EQ(1202 * 4, copy.GetDataSize()); copy.StartRead();
    for (int i = 0; i < 1201; ++i) EXPECT_EQ(0x10000 + i, copy.ReadInt()) << i;
    EXPECT_EQ(73, copy.ReadInt());
    EXPECT_EQ(0, source.GetDataSize()); EXPECT_EQ(1201, source.GetCurrentPos());
}

TEST(CKStateChunkWriterCloneTest, BackpatchedUnpublishedHeadersAndHiddenTailSurviveClone) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite();
    source.WriteIdentifier(0x1111); source.WriteInt(41); source.WriteInt(43);
    source.WriteIdentifier(0x2222); source.WriteInt(47);
    // Source has seven genuinely written words. Cursor now moves behind most
    // of them while extent remains unpublished. Copy max(extent,cursor) loses
    // the tail, so the allocation-capacity copy is necessary.
    source.Goto(2); source.WriteInt(59);
    ASSERT_EQ(0, source.GetDataSize()); ASSERT_EQ(3, source.GetCurrentPos());
    copy.Clone(&source); EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(3, copy.GetCurrentPos());
    copy.Goto(7); copy.WriteIdentifier(0x3333); copy.WriteInt(61); copy.CloseChunk();
    source.Goto(7); source.CloseChunk();
    copy.StartRead(); ASSERT_TRUE(copy.SeekIdentifier(0x1111));
    EXPECT_EQ(59, copy.ReadInt()); EXPECT_EQ(43, copy.ReadInt());
    ASSERT_TRUE(copy.SeekIdentifier(0x2222)); EXPECT_EQ(47, copy.ReadInt());
    ASSERT_TRUE(copy.SeekIdentifier(0x3333)); EXPECT_EQ(61, copy.ReadInt());
    source.StartRead(); ASSERT_TRUE(source.SeekIdentifier(0x1111)); EXPECT_EQ(59, source.ReadInt());
    ASSERT_TRUE(source.SeekIdentifier(0x2222)); EXPECT_EQ(47, source.ReadInt());
    EXPECT_FALSE(source.SeekIdentifier(0x3333));
}

TEST(CKStateChunkWriterCloneTest, PublishedPrefixAndUnpublishedBackpatchedTailRemainIndependent) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.WriteInt(41); source.WriteInt(43); source.UpdateDataSize();
    source.WriteInt(47); source.WriteInt(53); source.WriteInt(59);
    source.Goto(0); source.WriteInt(61);
    ASSERT_EQ(8, source.GetDataSize()); ASSERT_EQ(1, source.GetCurrentPos());
    copy.Clone(&source); EXPECT_EQ(8, copy.GetDataSize()); EXPECT_EQ(1, copy.GetCurrentPos());
    copy.Goto(4); copy.WriteInt(67); copy.CloseChunk();
    source.Goto(5); source.CloseChunk();
    const int expectedCopy[] = {61, 43, 47, 53, 67}, expectedSource[] = {61, 43, 47, 53, 59};
    ExpectOpenCloneWords(copy, expectedCopy, 5); ExpectOpenCloneWords(source, expectedSource, 5);
}

TEST(CKStateChunkWriterCloneTest, FilledWriteReservationCopiesBeforeCursorAdvancement) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite();
    int *reserved = static_cast<int *>(source.LockWriteBuffer(3)); ASSERT_NE(nullptr, reserved);
    reserved[0] = 41; reserved[1] = 43; reserved[2] = 47;
    ASSERT_EQ(0, source.GetDataSize()); ASSERT_EQ(0, source.GetCurrentPos());
    copy.Clone(&source); EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(0, copy.GetCurrentPos());
    // Obtain capacity through the public reservation API before access.
    // Expect only the three caller-initialized values, never unused padding.
    int *copied = static_cast<int *>(copy.LockWriteBuffer(3)); ASSERT_NE(nullptr, copied);
    EXPECT_EQ(41, copied[0]); EXPECT_EQ(43, copied[1]); EXPECT_EQ(47, copied[2]);
    copied[1] = 53; copy.Skip(3); copy.CloseChunk();
    source.Skip(3); source.CloseChunk();
    const int expectedCopy[] = {41, 53, 47}, expectedSource[] = {41, 43, 47};
    ExpectOpenCloneWords(copy, expectedCopy, 3); ExpectOpenCloneWords(source, expectedSource, 3);
}

TEST(CKStateChunkWriterCloneTest, CloneDoesNotPromoteUnusedReservationIntoSerializedPayload) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.CheckSize(8000); source.WriteInt(41); source.WriteInt(43);
    copy.Clone(&source); EXPECT_EQ(0, copy.GetDataSize()); EXPECT_EQ(2, copy.GetCurrentPos());
    copy.CloseChunk(); ASSERT_EQ(8, copy.GetDataSize());
    int wire[4] = {}; ASSERT_EQ(16, copy.ConvertToBuffer(nullptr));
    ASSERT_EQ(16, copy.ConvertToBuffer(wire));
    EXPECT_EQ(2, wire[1]); EXPECT_EQ(41, wire[2]); EXPECT_EQ(43, wire[3]);
    EXPECT_EQ(0, source.GetDataSize());
}

TEST(CKStateChunkWriterCloneTest, BackpatchedWriterGrowthPreservesPreviouslyWrittenTail) {
    CKStateChunk chunk(CKCID_OBJECT, nullptr);
    chunk.StartWrite(); chunk.WriteInt(41); chunk.WriteInt(43); chunk.UpdateDataSize();
    chunk.WriteInt(47); chunk.WriteInt(53); chunk.WriteInt(59);
    chunk.Goto(0); chunk.WriteInt(61);
    ASSERT_EQ(8, chunk.GetDataSize()); ASSERT_EQ(1, chunk.GetCurrentPos());
    // The normal small writer allocation cannot fit this request. Reserve
    // through the SDK without inspecting private capacity or filling gaps.
    int *reserved = static_cast<int *>(chunk.LockWriteBuffer(1200));
    ASSERT_NE(nullptr, reserved);
    EXPECT_EQ(8, chunk.GetDataSize()); EXPECT_EQ(1, chunk.GetCurrentPos());
    reserved[0] = 67;
    chunk.Goto(5); chunk.CloseChunk();
    const int expected[] = {61, 67, 47, 53, 59};
    ExpectOpenCloneWords(chunk, expected, 5);
}

TEST(CKStateChunkWriterCloneTest, BackpatchedCloneGrowthPreservesTailWithoutMutatingSource) {
    CKStateChunk source(CKCID_OBJECT, nullptr), copy(CKCID_PARAMETER, nullptr);
    source.StartWrite(); source.WriteInt(41); source.WriteInt(43); source.WriteInt(47);
    source.WriteInt(53); source.WriteInt(59); source.UpdateDataSize();
    copy.Clone(&source);
    copy.Goto(0); copy.WriteInt(61);
    ASSERT_EQ(20, copy.GetDataSize()); ASSERT_EQ(1, copy.GetCurrentPos());
    int *reserved = static_cast<int *>(copy.LockWriteBuffer(1200));
    ASSERT_NE(nullptr, reserved);
    EXPECT_EQ(20, copy.GetDataSize()); EXPECT_EQ(1, copy.GetCurrentPos());
    reserved[0] = 67;
    EXPECT_EQ(20, source.GetDataSize()); EXPECT_EQ(5, source.GetCurrentPos());
    source.Goto(4); source.WriteInt(73); source.CloseChunk();
    copy.Goto(5); copy.CloseChunk();
    const int expectedCopy[] = {61, 67, 47, 53, 59};
    const int expectedSource[] = {41, 43, 47, 53, 73};
    ExpectOpenCloneWords(copy, expectedCopy, 5);
    ExpectOpenCloneWords(source, expectedSource, 5);
}

class CKStateChunkWriterCloneContextTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context, nullptr, 0, 0));
    }
    static void TearDownTestSuite() {
        if (context) CKCloseContext(context);
        context = nullptr;
        CKShutdown();
    }
    static CKContext *context;
};

CKContext *CKStateChunkWriterCloneContextTest::context = nullptr;

TEST_F(CKStateChunkWriterCloneContextTest, StateChunkParameterCopyInvokesCloneForAnOpenWriter) {
    CKParameterLocal *sourceParam = context->CreateCKParameterLocal(nullptr, CKPGUID_STATECHUNK);
    CKParameterLocal *copyParam = context->CreateCKParameterLocal(nullptr, CKPGUID_STATECHUNK);
    ASSERT_NE(nullptr, sourceParam); ASSERT_NE(nullptr, copyParam);
    CKStateChunk *source = nullptr, *copy = nullptr;
    ASSERT_EQ(CK_OK, sourceParam->GetValue(&source, FALSE));
    ASSERT_EQ(CK_OK, copyParam->GetValue(&copy, FALSE));
    ASSERT_NE(nullptr, source); ASSERT_NE(nullptr, copy); EXPECT_NE(source, copy);
    source->StartWrite(); source->WriteInt(41); source->WriteInt(43); source->WriteInt(47);
    ASSERT_EQ(CK_OK, copyParam->CopyValue(sourceParam, FALSE));
    ASSERT_EQ(CK_OK, copyParam->GetValue(&copy, FALSE));
    EXPECT_EQ(0, copy->GetDataSize()); EXPECT_EQ(3, copy->GetCurrentPos());
    EXPECT_EQ(0, source->GetDataSize()); EXPECT_EQ(3, source->GetCurrentPos());
    copy->WriteInt(53); copy->CloseChunk();
    const int expected[] = {41, 43, 47, 53}; ExpectOpenCloneWords(*copy, expected, 4);
    source->WriteInt(59); source->CloseChunk();
    const int original[] = {41, 43, 47, 59}; ExpectOpenCloneWords(*source, original, 4);
    context->DestroyObject(sourceParam); context->DestroyObject(copyParam);
}

} // namespace
