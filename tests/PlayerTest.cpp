#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Player.h"

static bool DirectoryExistsForTest(const char *dir) {
#ifdef _WIN32
    if (!dir || dir[0] == '\0')
        return false;
    const DWORD attributes = ::GetFileAttributesA(dir);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    (void) dir;
    return false;
#endif
}

static bool FileExistsForTest(const char *file) {
#ifdef _WIN32
    if (!file || file[0] == '\0')
        return false;
    const DWORD attributes = ::GetFileAttributesA(file);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    (void) file;
    return false;
#endif
}

static bool HasPlayerRuntimeAssets() {
    // The Player sample expects these directories at runtime.
    return DirectoryExistsForTest("Plugins")
        && DirectoryExistsForTest("RenderEngines")
        && DirectoryExistsForTest("Managers")
        && DirectoryExistsForTest("BuildingBlocks");
}

class PlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        player_ = new Player();
    }

    void TearDown() override {
        delete player_;
    }

    Player *player_ = nullptr;
};

TEST_F(PlayerTest, Init) {
    ASSERT_NE(nullptr, player_);

    if (!HasPlayerRuntimeAssets()) {
        GTEST_SKIP() << "Player runtime assets (Plugins/RenderEngines/Managers/BuildingBlocks) not found in working directory.";
    }

    ASSERT_TRUE(player_->Init());
}

TEST_F(PlayerTest, Load) {
    ASSERT_NE(nullptr, player_);

    if (!HasPlayerRuntimeAssets()) {
        GTEST_SKIP() << "Player runtime assets (Plugins/RenderEngines/Managers/BuildingBlocks) not found in working directory.";
    }

    if (!FileExistsForTest("base.cmo")) {
        GTEST_SKIP() << "Test composition 'base.cmo' not found in working directory.";
    }

    ASSERT_TRUE(player_->Init());

    ASSERT_TRUE(player_->Load("base.cmo"));

    player_->Play();
    player_->Run();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}