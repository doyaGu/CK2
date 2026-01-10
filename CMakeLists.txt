cmake_minimum_required(VERSION 3.12)

project(CK2
        VERSION 0.1.0
        LANGUAGES CXX
)

# Determine if CK2 is being used as a subproject
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    # Use the built-in variable for CMake 3.21+
    if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        set(CK2_IS_TOP_LEVEL ON)
    else ()
        set(CK2_IS_TOP_LEVEL OFF)
    endif ()
else ()
    # Fallback for older CMake versions
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(CK2_IS_TOP_LEVEL ON)
    else ()
        set(CK2_IS_TOP_LEVEL OFF)
    endif ()
endif ()

# Only enforce out-of-source builds when CK2 is the top-level project
if (CK2_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-tree builds are not supported. Run CMake from a separate directory: cmake -B build")
endif ()

# Set C++ standard only if not already set by parent project
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif ()
if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED NO)
endif ()
if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS YES)
endif ()

# Project options - defaults depend on whether this is a top-level project
option(CK2_BUILD_TESTS "Build the test suite" ${CK2_IS_TOP_LEVEL})
option(CK2_INSTALL "Generate install target" ${CK2_IS_TOP_LEVEL})

# Library type
option(CK2_BUILD_SHARED "Build CK2 as a shared library" ON)

# Add path for custom modules (if needed in the future)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Standard CMake modules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE (only when top-level)
if (CK2_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
endif ()

# Platform checks
if (NOT WIN32)
    message(FATAL_ERROR "Only support Windows.")
endif ()

# Use relative paths (only when top-level on Windows)
if (CK2_IS_TOP_LEVEL AND WIN32)
    set(CMAKE_USE_RELATIVE_PATHS true)
    set(CMAKE_SUPPRESS_REGENERATION true)
endif ()

# Build type handling (only when top-level)
if (CK2_IS_TOP_LEVEL)
    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as no build type was specified")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type (Debug/Release)" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif ()
endif ()

# Install prefix (only when top-level)
if (CK2_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "The root directory of the installation" FORCE)
    message(STATUS "Setting default install directory to ${CMAKE_INSTALL_PREFIX} as no install directory was specified")
endif ()

# Generate compile commands for IDEs/tools (only when top-level)
if (CK2_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
endif ()

# Configure compiler-specific settings
# MSVC specific settings
if (MSVC)
    # Disable MSVC unsafe warnings
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
    )

    # MSVC specific compile options
    add_compile_options(
            # Disable strict const-qualification conformance for pointers initialized by string literals
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:strictStrings->
    )
endif ()

# GCC specific settings
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>
    )
endif ()

# Cache internal variables
set(CK2_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(CK2_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

# Find dependencies
# Prefer an already-installed VxMath package (Config or Module mode), but fall back to fetching it.
if(NOT TARGET VxMath::VxMath)
    find_package(VxMath QUIET)
endif()

if(NOT TARGET VxMath::VxMath)
    include(FetchContent)
    FetchContent_Declare(
            VxMath
            GIT_REPOSITORY https://github.com/doyaGu/VxMath
            GIT_TAG main
    )

    if(COMMAND FetchContent_MakeAvailable)
        FetchContent_MakeAvailable(VxMath)
    else()
        FetchContent_GetProperties(VxMath)
        if(NOT vxmath_POPULATED)
            FetchContent_Populate(VxMath)
            add_subdirectory("${vxmath_SOURCE_DIR}" "${vxmath_BINARY_DIR}" EXCLUDE_FROM_ALL)
        endif()
    endif()

    if(NOT TARGET VxMath::VxMath)
        if(TARGET VxMath)
            add_library(VxMath::VxMath ALIAS VxMath)
        endif()
    endif()
endif()

if(NOT TARGET VxMath::VxMath)
    message(FATAL_ERROR "VxMath was not found and could not be fetched. Set VxMath_DIR / CMAKE_PREFIX_PATH, or provide an installed VxMath package.")
endif()

# Add subdirectories
add_subdirectory(deps)
add_subdirectory(src)
if (CK2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# Export and packaging
if (CK2_INSTALL)
    # Export targets for build tree
    export(TARGETS CK2 miniz
            FILE "${CMAKE_CURRENT_BINARY_DIR}/CK2Targets.cmake"
            NAMESPACE CK2::
    )

    # Install targets and export for install tree
    install(EXPORT CK2Targets
            FILE CK2Targets.cmake
            NAMESPACE CK2::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CK2
    )

    # Configure package config file (if config file template exists)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CK2Config.cmake.in")
        configure_package_config_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CK2Config.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/CK2Config.cmake"
                INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CK2
        )
    else ()
        # Create a simple config file if template doesn't exist
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CK2Config.cmake"
                "include(CMakeFindDependencyMacro)\n"
                "find_dependency(VxMath REQUIRED)\n"
                "include(\"\${CMAKE_CURRENT_LIST_DIR}/CK2Targets.cmake\")\n"
        )
    endif ()

    # Configure package version file
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/CK2ConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/CK2Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/CK2ConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CK2
    )

    # Add package to CMake package registry (only when top-level)
    if (CK2_IS_TOP_LEVEL)
        export(PACKAGE CK2)
    endif ()
endif ()
