if (NOT DEFINED DESTINATION OR NOT DEFINED CK2_RUNTIME OR NOT DEFINED VXMATH_RUNTIME)
    message(FATAL_ERROR "Test runtime copying requires a destination and both runtime files")
endif ()

# Test executables share a directory. Protect its runtime files from concurrent
# post-build copies, including the compare-and-replace in copy_if_different.
file(MAKE_DIRECTORY "${DESTINATION}")
file(LOCK "${DESTINATION}/.ck2-test-runtime.lock" GUARD PROCESS RESULT_VARIABLE lock_result)
if (NOT lock_result STREQUAL "0")
    message(FATAL_ERROR "Cannot lock test runtime directory: ${lock_result}")
endif ()

set(runtime_files "${CK2_RUNTIME}" "${VXMATH_RUNTIME}")
if (DEFINED SDL_RUNTIME AND NOT SDL_RUNTIME STREQUAL "")
    list(APPEND runtime_files "${SDL_RUNTIME}")
endif ()
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_files} "${DESTINATION}"
        RESULT_VARIABLE result)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot copy test runtime dependencies: ${result}")
endif ()
