#include "CKPluginManager.h"

#include <gtest/gtest.h>

TEST(CKPluginManagerPlatformTest, AcceptsCurrentPlatformPluginExtension)
{
#if defined(_WIN32)
    EXPECT_TRUE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.dll"));
    EXPECT_TRUE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.DLL"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.so"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.dylib"));
#elif defined(__APPLE__)
    EXPECT_TRUE(CKPluginManager::IsPluginLibraryFileName("libCK2_3D.dylib"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.dll"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.so"));
#else
    EXPECT_TRUE(CKPluginManager::IsPluginLibraryFileName("libCK2_3D.so"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.dll"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.dylib"));
#endif
}

TEST(CKPluginManagerPlatformTest, RejectsNonPluginFileNames)
{
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName(nullptr));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName(""));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D.ini"));
    EXPECT_FALSE(CKPluginManager::IsPluginLibraryFileName("CK2_3D"));
}
