#include "AnsiPathTest.h"

class CKDataArrayAnsiPathTest : public CKAnsiPathTest {
protected:
    CKDataArray *MakeArray(const char *name) {
        CKDataArray *array = (CKDataArray *)Context->CreateObject(CKCID_DATAARRAY, (char *)name);
        if (array) {
            array->InsertColumn(-1, CKARRAYTYPE_STRING, (char *)"First");
            array->InsertColumn(-1, CKARRAYTYPE_STRING, (char *)"Last");
        }
        return array;
    }
    static void ExpectValue(CKDataArray *array, int row, int column, const char *expected) {
        char value[64] = {};
        EXPECT_GT(array->GetElementStringValue(row, column, value, sizeof(value)), 0);
        EXPECT_STREQ(expected, value);
    }
};

TEST_F(CKDataArrayAnsiPathTest, ChineseFilenameLoadsAnsiFieldsWithEachLineEnding) {
    if (!PrepareDirectory()) return;
    CreateContext(); ASSERT_NE(nullptr, Context);
    CKDataArray *array = MakeArray("Imported"); ASSERT_NE(nullptr, array);
    XString native = NativeDirectory; native << u8"/\u8868\u683C.txt";
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    const char value[] = "\xD6\xD0\xCE\xC4";
    const char *endings[] = {"\r\n", "\n", ""};
    for (int i = 0; i < 3; ++i) {
        XString text = "first\t"; text << value << endings[i];
        WriteNative(native.CStr(), text.CStr(), text.Length());
        ASSERT_TRUE(array->LoadElements(filename.Str(), TRUE, 0));
        ASSERT_EQ(i + 1, array->GetRowCount());
        ExpectValue(array, i, 0, "first");
        ExpectValue(array, i, 1, value);
    }
}

TEST_F(CKDataArrayAnsiPathTest, SaveAppendAndReopenKeepAnsiFieldBytes) {
    if (!PrepareDirectory()) return;
    CreateContext(); ASSERT_NE(nullptr, Context);
    CKDataArray *array = MakeArray("Exported"); ASSERT_NE(nullptr, array);
    const char value[] = "\xD6\xD0\xCE\xC4";
    array->AddRow();
    ASSERT_TRUE(array->SetElementStringValue(0, 0, (char *)"first"));
    ASSERT_TRUE(array->SetElementStringValue(0, 1, (char *)value));
    XString native = NativeDirectory; native << u8"/\u5BFC\u51FA.txt";
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    ASSERT_TRUE(array->WriteElements(filename.Str(), 0, 2, FALSE));
    ASSERT_TRUE(array->WriteElements(filename.Str(), 0, 2, TRUE));
#ifdef _WIN32
    const char expected[] = "first\t\xD6\xD0\xCE\xC4\r\nfirst\t\xD6\xD0\xCE\xC4\r\n";
#else
    const char expected[] = "first\t\xD6\xD0\xCE\xC4\nfirst\t\xD6\xD0\xCE\xC4\n";
#endif
    ExpectNativeBytes(native.CStr(), expected, sizeof(expected) - 1);
    CKDataArray *reopened = MakeArray("Reopened"); ASSERT_NE(nullptr, reopened);
    ASSERT_TRUE(reopened->LoadElements(filename.Str(), TRUE, 0));
    ASSERT_EQ(2, reopened->GetRowCount());
    for (int row = 0; row < 2; ++row) {
        ExpectValue(reopened, row, 0, "first");
        ExpectValue(reopened, row, 1, value);
    }
}

TEST_F(CKDataArrayAnsiPathTest, AsciiFilenamePreservesFieldsWithEachLineEnding) {
    ASSERT_TRUE(PrepareDirectory("ascii"));
    CreateContext(); ASSERT_NE(nullptr, Context);
    CKDataArray *array = MakeArray("Imported"); ASSERT_NE(nullptr, array);
    XString native = NativeDirectory; native << "/fields.txt";
    XString filename; ASSERT_TRUE(EncodeAnsiTestPath(native.CStr(), filename));
    const char *values[] = {"last", "caf\xE9", "\xD6\xD0\xCE\xC4"};
    const char *endings[] = {"\r\n", "\n", ""};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            XString text = "first\t"; text << values[i] << endings[j];
            WriteNative(native.CStr(), text.CStr(), text.Length());
            ASSERT_TRUE(array->LoadElements(filename.Str(), TRUE, 0));
            ASSERT_EQ(i * 3 + j + 1, array->GetRowCount());
            ExpectValue(array, i * 3 + j, 0, "first");
            ExpectValue(array, i * 3 + j, 1, values[i]);
        }
    }
    ASSERT_TRUE(array->WriteElements(filename.Str(), 0, 2, FALSE));
    CKDataArray *reopened = MakeArray("Reopened"); ASSERT_NE(nullptr, reopened);
    ASSERT_TRUE(reopened->LoadElements(filename.Str(), TRUE, 0));
    ASSERT_EQ(9, reopened->GetRowCount());
    for (int i = 0; i < 9; ++i) {
        ExpectValue(reopened, i, 0, "first");
        ExpectValue(reopened, i, 1, values[i / 3]);
    }
}
