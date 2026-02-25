#include <gtest/gtest.h>

#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "CKAll.h"

namespace {

class CKRuntimeFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
        ASSERT_EQ(CK_OK, CKCreateContext(&context_, nullptr, 0, 0));
        ASSERT_NE(nullptr, context_);
    }

    static void TearDownTestSuite() {
        if (context_) {
            CKCloseContext(context_);
            context_ = nullptr;
        }
        CKShutdown();
    }

    static CKContext *context_;
};

CKContext *CKRuntimeFixture::context_ = nullptr;

} // namespace

TEST_F(CKRuntimeFixture, SetElementValueFromParameterClearsUpperBitsForObjectColumn) {
    if (sizeof(CKUINTPTR) <= sizeof(CKDWORD)) {
        GTEST_SKIP() << "64-bit specific regression";
    }

    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_OBJECT, "ObjectRef");
    array->AddRow();

    CKUINTPTR *element = array->GetElement(0, 0);
    ASSERT_NE(nullptr, element);
#if UINTPTR_MAX > UINT32_MAX
    *element = (static_cast<CKUINTPTR>(0x12345678u) << 32) | static_cast<CKUINTPTR>(0xFFFFFFFFu);
#else
    *element = static_cast<CKUINTPTR>(0xFFFFFFFFu);
#endif

    CKObject *target = context_->CreateObject(CKCID_OBJECT, "TargetObject", CK_OBJECTCREATION_DYNAMIC);
    ASSERT_NE(nullptr, target);
    const CK_ID targetId = target->GetID();

    CKParameterOut *objectParam = context_->CreateCKParameterOut("ObjectParam", CKPGUID_OBJECT, TRUE);
    ASSERT_NE(nullptr, objectParam);
    ASSERT_EQ(CK_OK, objectParam->SetValue((void *)&targetId));

    ASSERT_TRUE(array->SetElementValueFromParameter(0, 0, objectParam));

    EXPECT_EQ(static_cast<CKUINTPTR>(targetId), *element);
    EXPECT_EQ(target, array->GetElementObject(0, 0));
    EXPECT_TRUE(array->IsObjectUsed(target, CKCID_OBJECT));
}

TEST_F(CKRuntimeFixture, UniqueRemovesDuplicatesOnly) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayUniqueRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_INT, "Id");
    array->InsertColumn(-1, CKARRAYTYPE_STRING, "Label");

    array->AddRow();
    array->AddRow();
    array->AddRow();

    int id = 1;
    ASSERT_TRUE(array->SetElementValue(0, 0, &id));
    ASSERT_TRUE(array->SetElementStringValue(0, 1, "first"));

    id = 1;
    ASSERT_TRUE(array->SetElementValue(1, 0, &id));
    ASSERT_TRUE(array->SetElementStringValue(1, 1, "duplicate"));

    id = 2;
    ASSERT_TRUE(array->SetElementValue(2, 0, &id));
    ASSERT_TRUE(array->SetElementStringValue(2, 1, "second"));

    array->Unique(0);

    ASSERT_EQ(2, array->GetRowCount());
    int out = 0;
    ASSERT_TRUE(array->GetElementValue(0, 0, &out));
    EXPECT_EQ(1, out);
    ASSERT_TRUE(array->GetElementValue(1, 0, &out));
    EXPECT_EQ(2, out);
}

TEST_F(CKRuntimeFixture, SetElementStringValueDuplicateKeyPreservesExistingValue) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayStringKeyRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_STRING, "Name");
    array->SetKeyColumn(0);

    array->AddRow();
    array->AddRow();

    ASSERT_TRUE(array->SetElementStringValue(0, 0, "Alpha"));
    ASSERT_TRUE(array->SetElementStringValue(1, 0, "Beta"));

    EXPECT_FALSE(array->SetElementStringValue(0, 0, "Beta"));

    char buffer[64] = {};
    const int length = array->GetElementStringValue(0, 0, buffer, sizeof(buffer));
    ASSERT_GT(length, 0);
    EXPECT_STREQ("Alpha", buffer);
}

TEST_F(CKRuntimeFixture, InsertAndMoveColumnDoNotRemapKeyColumnIndex) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayKeyColumnRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_INT, "A");
    array->InsertColumn(-1, CKARRAYTYPE_INT, "B");
    array->InsertColumn(-1, CKARRAYTYPE_INT, "C");

    array->SetKeyColumn(1);
    EXPECT_EQ(1, array->GetKeyColumn());

    array->InsertColumn(0, CKARRAYTYPE_INT, "Inserted");
    EXPECT_EQ(1, array->GetKeyColumn());

    array->MoveColumn(2, 0);
    EXPECT_EQ(1, array->GetKeyColumn());

    array->MoveColumn(0, -1);
    EXPECT_EQ(1, array->GetKeyColumn());
}

TEST_F(CKRuntimeFixture, SaveLoadInMemoryHandlesNullParameterCell) {
    CKDataArray *source = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArraySaveNullParamRegressionSrc", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, source);

    source->InsertColumn(-1, CKARRAYTYPE_PARAMETER, "ParamCol", CKPGUID_FLOAT);
    source->AddRow();

    CKUINTPTR *cell = source->GetElement(0, 0);
    ASSERT_NE(nullptr, cell);
    *cell = 0;

    CKStateChunk *chunk = source->Save(nullptr, 0xFFFFFFFFu);
    ASSERT_NE(nullptr, chunk);

    CKDataArray *loaded = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArraySaveNullParamRegressionDst", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, loaded);

    EXPECT_EQ(CK_OK, loaded->Load(chunk, nullptr));
    EXPECT_EQ(1, loaded->GetRowCount());
    EXPECT_NE(nullptr, loaded->GetElement(0, 0));

    delete chunk;
}

TEST_F(CKRuntimeFixture, SetColumnTypeParameterToParameterKeepsUsableCell) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayParamToParamRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_PARAMETER, "ParamCol", CKPGUID_INT);
    array->AddRow();

    int intValue = 42;
    CKParameterOut *intParam = context_->CreateCKParameterOut("IntParam", CKPGUID_INT, TRUE);
    ASSERT_NE(nullptr, intParam);
    ASSERT_EQ(CK_OK, intParam->SetValue(&intValue, sizeof(intValue)));
    ASSERT_TRUE(array->SetElementValueFromParameter(0, 0, intParam));

    array->SetColumnType(0, CKARRAYTYPE_PARAMETER, CKPGUID_FLOAT);

    CKUINTPTR *cell = array->GetElement(0, 0);
    ASSERT_NE(nullptr, cell);
    ASSERT_NE(static_cast<CKUINTPTR>(0), *cell);

    CKParameterOut *converted = reinterpret_cast<CKParameterOut *>(*cell);
    ASSERT_NE(nullptr, converted);
    EXPECT_TRUE(converted->GetGUID() == CKPGUID_FLOAT);

    float floatValue = 3.5f;
    CKParameterOut *floatParam = context_->CreateCKParameterOut("FloatParam", CKPGUID_FLOAT, TRUE);
    ASSERT_NE(nullptr, floatParam);
    ASSERT_EQ(CK_OK, floatParam->SetValue(&floatValue, sizeof(floatValue)));
    ASSERT_TRUE(array->SetElementValueFromParameter(0, 0, floatParam));

    float out = 0.0f;
    ASSERT_TRUE(array->GetElementValue(0, 0, &out));
    EXPECT_FLOAT_EQ(floatValue, out);
}

TEST_F(CKRuntimeFixture, LoadElementsNegativeColumnDoesNotModifyRows) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayNegativeColumnRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_INT, "Value");

    const char *filename = "CKDataArray_LoadElements_NegativeColumn.tmp";
    FILE *file = fopen(filename, "wb");
    ASSERT_NE(nullptr, file);
    fputs("123\n", file);
    fclose(file);

    EXPECT_TRUE(array->LoadElements(filename, FALSE, -1));
    EXPECT_EQ(0, array->GetRowCount());

    remove(filename);
}

TEST_F(CKRuntimeFixture, SortHandlesExtremeIntegersWithoutComparatorOverflow) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArraySortOverflowRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_INT, "Value");
    array->AddRow();
    array->AddRow();
    array->AddRow();

    int value = INT_MAX;
    ASSERT_TRUE(array->SetElementValue(0, 0, &value));
    value = INT_MIN;
    ASSERT_TRUE(array->SetElementValue(1, 0, &value));
    value = 0;
    ASSERT_TRUE(array->SetElementValue(2, 0, &value));

    array->Sort(0, TRUE);

    int out = 0;
    ASSERT_TRUE(array->GetElementValue(0, 0, &out));
    EXPECT_EQ(INT_MIN, out);
    ASSERT_TRUE(array->GetElementValue(1, 0, &out));
    EXPECT_EQ(0, out);
    ASSERT_TRUE(array->GetElementValue(2, 0, &out));
    EXPECT_EQ(INT_MAX, out);
}

TEST_F(CKRuntimeFixture, GetElementStringValueRespectsDestinationCapacity) {
    CKDataArray *array = static_cast<CKDataArray *>(
        context_->CreateObject(CKCID_DATAARRAY, "DataArrayStringBufferRegression", CK_OBJECTCREATION_DYNAMIC));
    ASSERT_NE(nullptr, array);

    array->InsertColumn(-1, CKARRAYTYPE_STRING, "Text");
    array->AddRow();
    ASSERT_TRUE(array->SetElementStringValue(0, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"));

    char small[8] = {};
    const int needed = array->GetElementStringValue(0, 0, small, sizeof(small));
    ASSERT_EQ(static_cast<int>(std::strlen("ABCDEFGHIJKLMNOPQRSTUVWXYZ")) + 1, needed);
    EXPECT_STREQ("ABCDEFG", small);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
