#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "CKAll.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

class CKFileIoCompletionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(CK_OK, CKStartUp()); }
    static void TearDownTestSuite() { CKShutdown(); }
    void SetUp() override {
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        Context->SetFileWriteMode(CKFILE_UNCOMPRESSED);
        Directory = std::filesystem::path(::testing::TempDir()) /
            (std::string("CKFileIoCompletion-") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::create_directories(Directory);
    }
    void TearDown() override {
        CKCloseContext(Context);
        std::filesystem::remove_all(Directory);
    }
    void ExpectClosed(const CKFile &file) {
        EXPECT_FALSE(Context->m_Saving);
        EXPECT_EQ(nullptr, file.m_FileName);
        EXPECT_EQ(0, file.m_FileObjects.Size());
        EXPECT_EQ(0, file.m_IncludedFiles.Size());
        EXPECT_EQ(0u, file.m_Flags);
    }
    CKContext *Context = nullptr;
    std::filesystem::path Directory;
};

TEST_F(CKFileIoCompletionTest, EmptySavePublishesActualBytesAndCompletesSession) {
    std::string name = (Directory / "empty.cmo").string();
    CKFile file(Context);
    ASSERT_EQ(CK_OK, file.StartSave(name.data(), 0));
    ASSERT_EQ(CK_OK, file.EndSave());
    ExpectClosed(file);
    FILE *stream = std::fopen(name.c_str(), "rb");
    ASSERT_NE(nullptr, stream);
    unsigned char bytes[4096] = {};
    const size_t count = std::fread(bytes, 1, sizeof(bytes), stream);
    EXPECT_GE(count, 72u); // Built-in managers may also emit save data.
    EXPECT_EQ(0, std::fclose(stream));
    EXPECT_EQ(0, std::memcmp(bytes, "Nemo", 4));
    EXPECT_EQ(8, bytes[16]);
    EXPECT_EQ(count, file.m_FileInfo.FileSize);
}

#ifdef _WIN32
// StartSave first opens and closes the destination without writing. EndSave
// reconnects for the actual output; this peer disconnects without reading it.
struct SavePipe {
    char Name[128];
    HANDLE Pipe = INVALID_HANDLE_VALUE;
    HANDLE Thread = nullptr;
    HANDLE Ready = nullptr;
    int Connections = 0;

    static DWORD WINAPI Serve(void *data) {
        SavePipe *pipe = static_cast<SavePipe *>(data);
        const BOOL firstConnected = ConnectNamedPipe(pipe->Pipe, nullptr);
        const DWORD firstError = firstConnected ? ERROR_SUCCESS : GetLastError();
        // The first client can finish its zero-byte open/close before this
        // thread runs; NO_DATA still proves that it connected and departed.
        if (!firstConnected && firstError != ERROR_PIPE_CONNECTED && firstError != ERROR_NO_DATA)
            return 1;
        ++pipe->Connections;
        unsigned char byte;
        DWORD read = 0;
        ReadFile(pipe->Pipe, &byte, 1, &read, nullptr);
        // A reused disconnected instance rejects opens until ConnectNamedPipe
        // starts listening. Publish a fresh instance before signalling Ready:
        // it also accepts a client that arrives before ConnectNamedPipe runs.
        CloseHandle(pipe->Pipe);
        pipe->Pipe = CreateNamedPipeA(pipe->Name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
            1, 1, 1, 0, nullptr);
        SetEvent(pipe->Ready);
        if (pipe->Pipe == INVALID_HANDLE_VALUE)
            return 1;
        if (ConnectNamedPipe(pipe->Pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED)
            ++pipe->Connections;
        CloseHandle(pipe->Pipe);
        return 0;
    }
    SavePipe() {
        std::snprintf(Name, sizeof(Name), "\\\\.\\pipe\\BallancedPass37Save-%lu-%p",
            GetCurrentProcessId(), static_cast<void *>(this));
        Ready = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        Pipe = CreateNamedPipeA(Name, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
            1, 1, 1, 0, nullptr);
        if (Ready && Pipe != INVALID_HANDLE_VALUE)
            Thread = CreateThread(nullptr, 0, Serve, this, 0, nullptr);
    }
    ~SavePipe() {
        if (Thread) {
            for (int i = 0; i < 2 && WaitForSingleObject(Thread, 0) == WAIT_TIMEOUT; ++i) {
                HANDLE client = CreateFileA(Name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
                WaitForSingleObject(Thread, 100);
            }
            WaitForSingleObject(Thread, 5000);
            CloseHandle(Thread);
        } else if (Pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(Pipe);
        }
        if (Ready) CloseHandle(Ready);
    }
};

TEST_F(CKFileIoCompletionTest, FinalFlushFailureIsReportedAndSaveSessionCanBeReused) {
    // Independent CRT control distinguishes a buffered close failure from an
    // open error or a short fwrite. No runtime stream or CRT function is mocked.
    {
        SavePipe control;
        ASSERT_NE(nullptr, control.Thread);
        FILE *first = std::fopen(control.Name, "ab");
        ASSERT_NE(nullptr, first);
        ASSERT_EQ(0, std::fclose(first));
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(control.Ready, 5000));
        FILE *output = std::fopen(control.Name, "wb");
        ASSERT_NE(nullptr, output);
        unsigned char bytes[256] = {};
        EXPECT_EQ(1u, std::fwrite(bytes, sizeof(bytes), 1, output));
        EXPECT_NE(0, std::fclose(output));
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(control.Thread, 5000));
        EXPECT_EQ(2, control.Connections);
    }
    CKFile file(Context);
    {
        SavePipe pipe;
        ASSERT_NE(INVALID_HANDLE_VALUE, pipe.Pipe);
        ASSERT_NE(nullptr, pipe.Thread);
        ASSERT_EQ(CK_OK, file.StartSave(pipe.Name, 0));
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Ready, 5000));
        // The empty frame fits in the CRT's stream buffer. The actual final
        // flush meets a disconnected peer with a one-byte pipe quota.
        EXPECT_EQ(CKERR_NOTENOUGHDISKPLACE, file.EndSave());
        ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Thread, 5000));
        EXPECT_EQ(2, pipe.Connections);
        ExpectClosed(file);
    }
    std::string retry = (Directory / "retry.cmo").string();
    ASSERT_EQ(CK_OK, file.StartSave(retry.data(), 0));
    EXPECT_EQ(CK_OK, file.EndSave());
    ExpectClosed(file);
    EXPECT_EQ(CK_OK, file.Load(retry.data(), nullptr));
}

TEST_F(CKFileIoCompletionTest, IncludedResourceWriteFailureIsReportedAndSessionCloses) {
    std::string resource = (Directory / "payload.bin").string();
    FILE *stream = std::fopen(resource.c_str(), "wb");
    ASSERT_NE(nullptr, stream);
    unsigned char block[4096];
    for (int i = 0; i < 4096; ++i) block[i] = static_cast<unsigned char>(i * 73 + 19);
    for (int i = 0; i < 64; ++i)
        ASSERT_EQ(sizeof(block), std::fwrite(block, 1, sizeof(block), stream));
    ASSERT_EQ(0, std::fclose(stream));
    CKFile file(Context);
    SavePipe pipe;
    ASSERT_NE(INVALID_HANDLE_VALUE, pipe.Pipe);
    ASSERT_NE(nullptr, pipe.Thread);
    ASSERT_EQ(CK_OK, file.StartSave(pipe.Name, 0));
    ASSERT_TRUE(file.IncludeFile(resource.data()));
    ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Ready, 5000));
    EXPECT_EQ(CKERR_NOTENOUGHDISKPLACE, file.EndSave());
    ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(pipe.Thread, 5000));
    EXPECT_EQ(2, pipe.Connections);
    ExpectClosed(file);
}
#endif

} // namespace
