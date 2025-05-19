cmake_minimum_required(VERSION 3.12)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-tree builds are not supported. Run CMake from a separate directory: cmake -B build")
endif ()

project(CK2 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED NO)
set(CMAKE_CXX_EXTENSIONS YES)

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

if (NOT WIN32)
    message(FATAL_ERROR "Only support Windows.")
endif ()

# Use relative paths
# This is mostly to reduce path size for command-line limits on windows
if (WIN32)
    set(CMAKE_USE_RELATIVE_PATHS true)
    set(CMAKE_SUPPRESS_REGENERATION true)
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as no build type was specified")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type (Debug/Release)" FORCE)
endif ()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "The root directory of the installation" FORCE)
    message(STATUS "Setting default install directory to ${CMAKE_INSTALL_PREFIX} as no install directory was specified")
endif ()

# Generate a CompilationDatabase (compile_commands.json)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Disable strict const-qualification conformance for pointers initialized by using string literals
add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->>
        $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:GNU>:-fpermissive>>
        $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:GNU>:-Wno-write-strings>>
)

# Disable msvc unsafe warnings
add_compile_definitions(
		$<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
		$<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
)

set(CK2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(CK2_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(deps)
add_subdirectory(src)

