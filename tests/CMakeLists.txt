include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_ck2_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${CK2_INCLUDE_DIR}"
            "${CK2_SOURCE_DIR}"
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE "CK2_EXPORTS")

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")
endfunction()

add_executable(Player Player.cpp)

set_target_properties(Player PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_compile_definitions(Player PRIVATE
        PLAYER_STANDALONE
)

target_include_directories(Player PRIVATE
        ${CK2_INCLUDE_DIR}
        ${CK2_SOURCE_DIR}
)

target_link_libraries(Player PRIVATE
        CK2
        ${CK2_SOURCE_DIR}/VxMath.lib
)

add_ck2_test(PlayerTest
        SOURCES
        PlayerTest.cpp
        Player.cpp
        DEPENDENCIES
        CK2 ${CK2_SOURCE_DIR}/VxMath.lib
)