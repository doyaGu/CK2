# miniz - configure as static library
set(AMALGAMATE_SOURCES ON CACHE BOOL "" FORCE)
set(BUILD_HEADER_ONLY OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
add_subdirectory(miniz EXCLUDE_FROM_ALL)

# STB
set(STB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb" CACHE INTERNAL "STB include directory")
if(NOT EXISTS "${STB_INCLUDE_DIR}")
    message(WARNING "STB headers not found at ${STB_INCLUDE_DIR}. Please ensure STB is properly downloaded.")
endif()