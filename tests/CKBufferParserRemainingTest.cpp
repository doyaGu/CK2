#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include "CKAll.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct CKTestTemporaryFile {
    char Name[160];
    CKTestTemporaryFile() {
        std::snprintf(Name, sizeof(Name), "CKBufferParser-pass36-%p.tmp", static_cast<void *>(this));
    }
    ~CKTestTemporaryFile() { std::remove(Name); }
};

TEST(CKBufferParserRemainingTest, ExtractFilePreservesBinarySliceAndFollowingSourceBytes) {
    unsigned char bytes[] = {0xaa, 0x00, 0xff, 0x1a, 0x0a, 0x0d, 0x55};
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(1);
    CKTestTemporaryFile output;
    ASSERT_TRUE(parser.ExtractFile(output.Name, 5));
    EXPECT_EQ(6, parser.CursorPos());
    EXPECT_EQ(0x55, bytes[6]);
    FILE *file = std::fopen(output.Name, "rb");
    ASSERT_NE(nullptr, file);
    unsigned char saved[8] = {};
    const size_t count = std::fread(saved, 1, sizeof(saved), file);
    EXPECT_EQ(0, std::fclose(file));
    EXPECT_EQ(5u, count);
    EXPECT_EQ(0, std::memcmp(bytes + 1, saved, 5));
}

TEST(CKBufferParserRemainingTest, ExtractFileZeroLengthCreatesEmptyFileWithoutConsumingSource) {
    char bytes[] = "abc";
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(2);
    CKTestTemporaryFile output;
    ASSERT_TRUE(parser.ExtractFile(output.Name, 0));
    EXPECT_EQ(2, parser.CursorPos());
    FILE *file = std::fopen(output.Name, "rb");
    ASSERT_NE(nullptr, file);
    EXPECT_EQ(EOF, std::fgetc(file));
    EXPECT_EQ(0, std::fclose(file));
}

TEST(CKBufferParserRemainingTest, ExtractFileOpenFailurePreservesSourceForRetry) {
    char bytes[] = "#retry";
    CKBufferParser parser(bytes, sizeof(bytes));
    parser.Seek(1);
    char missing[] = "CKBufferParser-pass36-missing-directory/output.bin";
    ASSERT_FALSE(parser.ExtractFile(missing, 5));
    EXPECT_EQ(1, parser.CursorPos());
    CKTestTemporaryFile output;
    EXPECT_TRUE(parser.ExtractFile(output.Name, 5));
    EXPECT_EQ(6, parser.CursorPos());
}

#ifdef _WIN32
struct CKTestClosedPipe {
    HANDLE Pipe;
    HANDLE Thread;
    char Name[160];
    bool Connected;

    static DWORD WINAPI CloseAfterConnection(void *data) {
        CKTestClosedPipe *pipe = static_cast<CKTestClosedPipe *>(data);
        pipe->Connected = ConnectNamedPipe(pipe->Pipe, nullptr) != FALSE
            || GetLastError() == ERROR_PIPE_CONNECTED;
        CloseHandle(pipe->Pipe);
        return 0;
    }

    CKTestClosedPipe() : Pipe(INVALID_HANDLE_VALUE), Thread(nullptr), Connected(false) {
        std::snprintf(Name, sizeof(Name), "\\\\.\\pipe\\BallancedPass36Parser-%lu-%p",
            GetCurrentProcessId(), static_cast<void *>(this));
        Pipe = CreateNamedPipeA(Name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
            1, 1, 1, 0, nullptr);
        if (Pipe != INVALID_HANDLE_VALUE)
            Thread = CreateThread(nullptr, 0, CloseAfterConnection, this, 0, nullptr);
    }

    ~CKTestClosedPipe() {
        if (Thread) {
            // Also releases the server when a failed assertion prevented a client connection.
            if (WaitForSingleObject(Thread, 0) == WAIT_TIMEOUT) {
                HANDLE client = CreateFileA(Name, GENERIC_WRITE, 0, nullptr,
                    OPEN_EXISTING, 0, nullptr);
                if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
            }
            WaitForSingleObject(Thread, 5000);
            CloseHandle(Thread);
        } else if (Pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(Pipe);
        }
    }
};

class CKBufferParserPipeFailureTest : public ::testing::TestWithParam<int> {};

TEST_P(CKBufferParserPipeFailureTest, ExtractFileReportsRealBrokenPipeAndKeepsSliceForRetry) {
    const int count = GetParam();
    unsigned char *bytes = new unsigned char[count + 2];
    for (int i = 0; i < count + 2; ++i) bytes[i] = static_cast<unsigned char>(i * 73 + 19);
    CKBufferParser parser(bytes, count + 2);
    parser.Seek(1);
    {
        CKTestClosedPipe pipe;
        ASSERT_NE(INVALID_HANDLE_VALUE, pipe.Pipe);
        ASSERT_NE(nullptr, pipe.Thread);
        // The peer never reads and closes after connecting. This exceeds the pipe
        // quota, so a race cannot turn the entire write into a successful transfer.
        EXPECT_FALSE(parser.ExtractFile(pipe.Name, count));
        EXPECT_EQ(1, parser.CursorPos());
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Thread, 5000));
        EXPECT_TRUE(pipe.Connected); // fopen succeeded; the failure happened during I/O.
    }
    CKTestTemporaryFile retry;
    ASSERT_TRUE(parser.ExtractFile(retry.Name, count));
    EXPECT_EQ(count + 1, parser.CursorPos());
    FILE *file = std::fopen(retry.Name, "rb");
    ASSERT_NE(nullptr, file);
    unsigned char *saved = new unsigned char[count + 1];
    const size_t size = std::fread(saved, 1, count + 1, file);
    EXPECT_EQ(0, std::fclose(file));
    EXPECT_EQ(static_cast<size_t>(count), size);
    EXPECT_EQ(0, std::memcmp(bytes + 1, saved, count));
    delete[] saved;
    delete[] bytes;
}

INSTANTIATE_TEST_SUITE_P(ActualPipeWrites, CKBufferParserPipeFailureTest,
    ::testing::Values(65536, 262144, 1048576));
#endif
