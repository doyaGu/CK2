#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "CKAll.h"

#ifdef _WIN32
// The peer connects but never reads. This small write remains in the CRT
// buffer: an independent control below proves the error occurs at fclose.
struct CKTestDisconnectedOutputPipe {
    HANDLE Pipe;
    HANDLE Thread;
    char Name[160];
    bool Connected;

    static DWORD WINAPI Disconnect(void *data) {
        CKTestDisconnectedOutputPipe *pipe = static_cast<CKTestDisconnectedOutputPipe *>(data);
        pipe->Connected = ConnectNamedPipe(pipe->Pipe, nullptr) != FALSE
            || GetLastError() == ERROR_PIPE_CONNECTED;
        CloseHandle(pipe->Pipe);
        return 0;
    }

    CKTestDisconnectedOutputPipe() : Pipe(INVALID_HANDLE_VALUE), Thread(nullptr), Connected(false) {
        std::snprintf(Name, sizeof(Name), "\\\\.\\pipe\\Ballanced-ParserFlush-%lu-%p",
            GetCurrentProcessId(), static_cast<void *>(this));
        Pipe = CreateNamedPipeA(Name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
            1, 1, 1, 0, nullptr);
        if (Pipe != INVALID_HANDLE_VALUE)
            Thread = CreateThread(nullptr, 0, Disconnect, this, 0, nullptr);
    }

    ~CKTestDisconnectedOutputPipe() {
        if (Thread) {
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
#endif

struct CKTestRetryFile {
#ifdef _WIN32
    char Name[MAX_PATH];
#else
    char Name[PATH_MAX];
#endif
    CKTestRetryFile() {
#ifdef _WIN32
        char directory[MAX_PATH];
        const DWORD length = GetTempPathA(MAX_PATH, directory);
        Name[0] = '\0';
        if (length && length < MAX_PATH)
            GetTempFileNameA(directory, "ckp", 0, Name);
#else
        const char *directory = getenv("TMPDIR");
        if (!directory || !directory[0]) directory = "/tmp";
        const int length = std::snprintf(Name, sizeof(Name), "%s/ck-parser-XXXXXX", directory);
        if (length < 0 || length >= static_cast<int>(sizeof(Name))) {
            Name[0] = '\0';
            return;
        }
        const int descriptor = mkstemp(Name);
        if (descriptor < 0) Name[0] = '\0';
        else EXPECT_EQ(0, close(descriptor));
#endif
    }
    ~CKTestRetryFile() { if (Name[0]) std::remove(Name); }
};

#ifndef _WIN32
struct CKTestFileSizeLimit {
    struct rlimit Previous;
    struct sigaction PreviousSignal;
    bool Ready;
    bool SignalChanged;

    CKTestFileSizeLimit() : Ready(false), SignalChanged(false) {
        if (getrlimit(RLIMIT_FSIZE, &Previous) != 0) return;
        struct sigaction ignored = {};
        ignored.sa_handler = SIG_IGN;
        if (sigemptyset(&ignored.sa_mask) != 0) return;
        if (sigaction(SIGXFSZ, &ignored, &PreviousSignal) != 0) return;
        SignalChanged = true;
        struct rlimit limited = Previous;
        limited.rlim_cur = 0;
        Ready = setrlimit(RLIMIT_FSIZE, &limited) == 0;
    }
    ~CKTestFileSizeLimit() {
        if (Ready) EXPECT_EQ(0, setrlimit(RLIMIT_FSIZE, &Previous));
        if (SignalChanged) EXPECT_EQ(0, sigaction(SIGXFSZ, &PreviousSignal, nullptr));
    }
};
#endif

TEST(CKBufferParserFlushTest, ExtractFileReportsBufferedCloseFailureAndRetainsSliceForRetry) {
    unsigned char source[258];
    for (int i = 0; i < 258; ++i) source[i] = static_cast<unsigned char>(i * 73 + 19);
    source[0] = 0xa7;
    source[257] = 0xb9;
    const unsigned char firstPayloadByte = source[1];
    unsigned char original[sizeof(source)];
    std::memcpy(original, source, sizeof(source));
#ifdef _WIN32
    {
        CKTestDisconnectedOutputPipe control;
        ASSERT_NE(INVALID_HANDLE_VALUE, control.Pipe);
        ASSERT_NE(nullptr, control.Thread);
        FILE *stream = std::fopen(control.Name, "wb");
        ASSERT_NE(nullptr, stream);
        // Match the real parser's fwrite element size/count. If this does not
        // succeed fully, the fixture has not isolated the close-only path.
        const size_t written = std::fwrite(source + 1, 1, 256, stream);
        const int closed = std::fclose(stream);
        EXPECT_EQ(256u, written);
        EXPECT_NE(0, closed);
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(control.Thread, 5000));
        EXPECT_TRUE(control.Connected);
    }
#else
    {
        CKTestRetryFile control;
        ASSERT_NE('\0', control.Name[0]);
        CKTestFileSizeLimit limit;
        ASSERT_TRUE(limit.Ready);
        FILE *stream = std::fopen(control.Name, "wb");
        ASSERT_NE(nullptr, stream);
        const size_t written = std::fwrite(source + 1, 1, 256, stream);
        const int closed = std::fclose(stream);
        EXPECT_EQ(256u, written);
        EXPECT_NE(0, closed);
    }
#endif
    CKBufferParser parser(source, sizeof(source));
    parser.Seek(1);
#ifdef _WIN32
    {
        CKTestDisconnectedOutputPipe pipe;
        ASSERT_NE(INVALID_HANDLE_VALUE, pipe.Pipe);
        ASSERT_NE(nullptr, pipe.Thread);
        EXPECT_FALSE(parser.ExtractFile(pipe.Name, 256));
        EXPECT_EQ(1, parser.CursorPos());
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Thread, 5000));
        EXPECT_TRUE(pipe.Connected);
    }
#else
    {
        CKTestRetryFile rejected;
        ASSERT_NE('\0', rejected.Name[0]);
        CKTestFileSizeLimit limit;
        ASSERT_TRUE(limit.Ready);
        EXPECT_FALSE(parser.ExtractFile(rejected.Name, 256));
        EXPECT_EQ(1, parser.CursorPos());
    }
#endif
    EXPECT_EQ(0, std::memcmp(original, source, sizeof(source)));
    CKTestRetryFile retry;
    ASSERT_NE('\0', retry.Name[0]);
    ASSERT_TRUE(parser.ExtractFile(retry.Name, 256));
    EXPECT_EQ(257, parser.CursorPos());
    EXPECT_EQ(0xa7, source[0]);
    EXPECT_EQ(firstPayloadByte, source[1]);
    EXPECT_EQ(0xb9, source[257]);
    FILE *stream = std::fopen(retry.Name, "rb");
    ASSERT_NE(nullptr, stream);
    unsigned char saved[257] = {};
    const size_t count = std::fread(saved, 1, sizeof(saved), stream);
    EXPECT_EQ(0, std::fclose(stream));
    EXPECT_EQ(256u, count);
    EXPECT_EQ(0, std::memcmp(source + 1, saved, 256));
    unsigned char next = 0;
    ASSERT_TRUE(parser.Read(&next, 1));
    EXPECT_EQ(0xb9, next);
}
