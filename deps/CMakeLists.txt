# miniz - configure as static library
set(AMALGAMATE_SOURCES ON CACHE BOOL "" FORCE)
set(BUILD_HEADER_ONLY OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
add_subdirectory(miniz EXCLUDE_FROM_ALL)

# miniz rewrites its amalgamation on every configure. Compile stable copies so
# an unchanged configure does not rebuild CK2 and relink all of its consumers.
set(_ck2_miniz_stable_dir "${CMAKE_CURRENT_BINARY_DIR}/miniz/stable-amalgamation")
foreach (_extension c h)
    configure_file("${CMAKE_CURRENT_BINARY_DIR}/miniz/amalgamation/miniz.${_extension}"
            "${_ck2_miniz_stable_dir}/miniz.${_extension}" COPYONLY)
endforeach ()
set_property(TARGET miniz PROPERTY SOURCES
        "${_ck2_miniz_stable_dir}/miniz.c" "${_ck2_miniz_stable_dir}/miniz.h")
target_include_directories(miniz BEFORE PUBLIC "$<BUILD_INTERFACE:${_ck2_miniz_stable_dir}>")

# Regenerate the amalgamation when vendor sources change during a normal build.
file(GLOB _ck2_miniz_inputs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/miniz/miniz*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_ck2_miniz_inputs})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CK2_BUILD_SHARED AND TARGET miniz)
    set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()
