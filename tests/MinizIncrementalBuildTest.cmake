cmake_minimum_required(VERSION 3.16)

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/source/deps")
file(COPY "${DEPENDENCY_DIR}/" DESTINATION "${TEST_ROOT}/source/deps" PATTERN ".git" EXCLUDE)
file(WRITE "${TEST_ROOT}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16)
project(MinizIncrementalCheck C)
add_subdirectory(deps)
add_executable(miniz_incremental_consumer consumer.c)
target_link_libraries(miniz_incremental_consumer PRIVATE miniz)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-$<CONFIG>.path"
     CONTENT "$<TARGET_FILE:miniz_incremental_consumer>")
]=])
file(WRITE "${TEST_ROOT}/source/consumer.c" [=[
#include <miniz.h>
#include <string.h>
#ifndef CK2_MINIZ_REGEN_VALUE
#define CK2_MINIZ_REGEN_VALUE 0
#endif
int main(void) {
    const unsigned char input[] = "incremental miniz round trip";
    unsigned char compressed[128], output[128];
    mz_ulong compressed_size = sizeof(compressed), output_size = sizeof(output);
    if (mz_compress(compressed, &compressed_size, input, sizeof(input)) != MZ_OK
        || mz_uncompress(output, &output_size, compressed, compressed_size) != MZ_OK
        || output_size != sizeof(input) || memcmp(input, output, sizeof(input)) != 0)
        return 1;
    return CK2_MINIZ_REGEN_VALUE;
}
]=])

function(run_checked)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed (${result}): ${ARGN}\n${output}${error}")
    endif ()
endfunction()

set(configure "${CMAKE_COMMAND}" -S "${TEST_ROOT}/source" -B "${TEST_ROOT}/build"
        -G "${TEST_GENERATOR}" -DCMAKE_BUILD_TYPE=Release)
if (TEST_PLATFORM)
    list(APPEND configure -A "${TEST_PLATFORM}")
endif ()
if (TEST_TOOLSET)
    list(APPEND configure -T "${TEST_TOOLSET}")
endif ()
if (TEST_OSX_ARCHITECTURES)
    list(APPEND configure "-DCMAKE_OSX_ARCHITECTURES=${TEST_OSX_ARCHITECTURES}")
endif ()
set(build "${CMAKE_COMMAND}" --build "${TEST_ROOT}/build" --config Release --target miniz_incremental_consumer)
run_checked(${configure})
run_checked(${build})
file(READ "${TEST_ROOT}/build/consumer-Release.path" consumer)
run_checked("${consumer}")
file(TIMESTAMP "${consumer}" original_stamp "%s" UTC)

run_checked("${CMAKE_COMMAND}" -E sleep 1.1)
run_checked(${configure})
run_checked(${build})
file(TIMESTAMP "${consumer}" unchanged_stamp "%s" UTC)
if (NOT original_stamp STREQUAL unchanged_stamp)
    message(FATAL_ERROR "Reconfiguring unchanged miniz rebuilt its consumer")
endif ()

# A real vendor header edit must regenerate the amalgamation during the build.
run_checked("${CMAKE_COMMAND}" -E sleep 1.1)
file(APPEND "${TEST_ROOT}/source/deps/miniz/miniz_common.h" "\n#define CK2_MINIZ_REGEN_VALUE 7\n")
run_checked(${build})
execute_process(COMMAND "${consumer}" RESULT_VARIABLE result)
if (NOT result EQUAL 7)
    message(FATAL_ERROR "The build did not pick up a changed miniz header (exit ${result})")
endif ()
