# =============================================================================
# CK2 - Virtools CK Engine Library
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(CK2
        VERSION 0.1.0
        DESCRIPTION "Virtools CK Engine Library"
        HOMEPAGE_URL "https://github.com/doyaGu/CK2"
        LANGUAGES CXX
)

# =============================================================================
# Determine if top-level project
# =============================================================================
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    if (PROJECT_IS_TOP_LEVEL)
        set(CK2_IS_TOP_LEVEL ON)
    else ()
        set(CK2_IS_TOP_LEVEL OFF)
    endif ()
else ()
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(CK2_IS_TOP_LEVEL ON)
    else ()
        set(CK2_IS_TOP_LEVEL OFF)
    endif ()
endif ()

# =============================================================================
# Platform and build checks
# =============================================================================
if (CK2_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Run CMake from a separate directory: cmake -B build")
endif ()

if (NOT WIN32)
    message(FATAL_ERROR "CK2 only supports Windows.")
endif ()

# =============================================================================
# C++ standard
# =============================================================================
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif ()
if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED NO)
endif ()
if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS YES)
endif ()

# =============================================================================
# Project options
# =============================================================================
option(CK2_BUILD_TESTS "Build the test suite" OFF)
option(CK2_BUILD_SHARED "Build CK2 as a shared library" ON)
option(CK2_BUILD_STATIC "Build CK2 as a static library" OFF)
option(CK2_INSTALL "Generate install target" ${CK2_IS_TOP_LEVEL})

# =============================================================================
# CMake modules
# =============================================================================
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# =============================================================================
# Top-level project settings
# =============================================================================
if (CK2_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
    set(CMAKE_USE_RELATIVE_PATHS TRUE)
    set(CMAKE_SUPPRESS_REGENERATION TRUE)

    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
        message(STATUS "[CK2] Setting build type to 'Release'")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif ()

    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
        message(STATUS "[CK2] Setting install directory to ${CMAKE_INSTALL_PREFIX}")
    endif ()

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

# =============================================================================
# Compiler settings
# =============================================================================
if (MSVC)
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
    )
    add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:strictStrings->
    )
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>
    )
endif ()

# =============================================================================
# Cache internal variables
# =============================================================================
set(CK2_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "CK2 include directory")
set(CK2_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "CK2 source directory")

# Find dependencies
# Prefer an already-installed VxMath package (Config or Module mode), but fall back to fetching it.
if (NOT TARGET VxMath)
    find_package(VxMath QUIET)
endif ()

# Prefer a sibling workspace checkout (e.g. ../VxMath) before using FetchContent.
if (NOT TARGET VxMath)
    get_filename_component(_ck2_repo_root "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
    get_filename_component(_vxmath_local_dir "${_ck2_repo_root}/../VxMath" ABSOLUTE)
    if (EXISTS "${_vxmath_local_dir}/CMakeLists.txt")
        message(STATUS "[CK2] Using local VxMath checkout: ${_vxmath_local_dir}")
        add_subdirectory("${_vxmath_local_dir}" "${CMAKE_BINARY_DIR}/_deps/vxmath-local-build" EXCLUDE_FROM_ALL)
    endif ()
endif ()

if (NOT TARGET VxMath)
    include(FetchContent)
    FetchContent_Declare(
            VxMath
            GIT_REPOSITORY https://github.com/doyaGu/VxMath
            GIT_TAG main
    )

    if (COMMAND FetchContent_MakeAvailable)
        FetchContent_MakeAvailable(VxMath)
    else ()
        FetchContent_GetProperties(VxMath)
        if (NOT vxmath_POPULATED)
            FetchContent_Populate(VxMath)
            add_subdirectory("${vxmath_SOURCE_DIR}" "${vxmath_BINARY_DIR}" EXCLUDE_FROM_ALL)
        endif ()
    endif ()
endif ()

if (NOT TARGET VxMath)
    message(FATAL_ERROR "VxMath was not found and could not be fetched. Set VxMath_DIR / CMAKE_PREFIX_PATH, or provide an installed VxMath package.")
endif ()

# Add subdirectories
add_subdirectory(deps)
add_subdirectory(src)
if (CK2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# Export and packaging
if (CK2_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Export targets for build tree (only those that exist)
    if (CK2_TARGETS)
        export(TARGETS ${CK2_TARGETS} miniz
                FILE "${CMAKE_CURRENT_BINARY_DIR}/CK2Targets.cmake"
        )
    endif ()

    # Install export for install tree
    install(EXPORT CK2Targets
            FILE CK2Targets.cmake
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

# =============================================================================
# Configuration summary
# =============================================================================
if (CK2_IS_TOP_LEVEL)
    message(STATUS "")
    message(STATUS "============================================================")
    message(STATUS "CK2 Configuration Summary")
    message(STATUS "============================================================")
    message(STATUS "  Version:              ${PROJECT_VERSION}")
    message(STATUS "  Build Type:           ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Build Shared:         ${CK2_BUILD_SHARED}")
    message(STATUS "  Build Static:         ${CK2_BUILD_STATIC}")
    message(STATUS "  Build Tests:          ${CK2_BUILD_TESTS}")
    message(STATUS "  Install:              ${CK2_INSTALL}")
    message(STATUS "  Install Prefix:       ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "============================================================")
    message(STATUS "")
endif ()
