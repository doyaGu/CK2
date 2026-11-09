include(CTest)
enable_testing()
include(GoogleTest)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if (NOT TARGET CK2 AND TARGET CK2Static)
    add_library(CK2 ALIAS CK2Static)
endif ()

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
    get_target_property(_ck2_test_runtime_type CK2 TYPE)
    if (_ck2_test_runtime_type STREQUAL "STATIC_LIBRARY" AND test_name STREQUAL "CKMessageDeliveryTest")
        target_compile_definitions(${test_name} PRIVATE CK2_TEST_STATIC_RUNTIME)
    endif ()

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")

    set(_ck2_test_runtime_arguments
            "-DCK2_RUNTIME=$<TARGET_FILE:CK2>"
            "-DVXMATH_RUNTIME=$<TARGET_FILE:VxMath>")
    if (WIN32 AND TARGET SDL3::SDL3)
        get_target_property(_ck2_test_sdl_type SDL3::SDL3 TYPE)
        if (_ck2_test_sdl_type STREQUAL "SHARED_LIBRARY")
            list(APPEND _ck2_test_runtime_arguments "-DSDL_RUNTIME=$<TARGET_FILE:SDL3::SDL3>")
        endif ()
    endif ()
    add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} ${_ck2_test_runtime_arguments}
                    "-DDESTINATION=$<TARGET_FILE_DIR:${test_name}>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/CopyTestRuntime.cmake"
            VERBATIM)

            add_test(NAME ${test_name}
                    COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:CK2>;$<TARGET_FILE_DIR:VxMath>;$ENV{PATH}"
                    $<TARGET_FILE:${test_name}>
            )
endfunction()

add_test(NAME CKTestRuntimeCopy COMMAND "${CMAKE_COMMAND}"
        "-DCOPY_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/CopyTestRuntime.cmake"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runtime-copy-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeCopyTest.cmake")
set_tests_properties(CKTestRuntimeCopy PROPERTIES TIMEOUT 120)

add_ck2_test(CKBehaviorAccessTest
        SOURCES CKBehaviorAccessTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKDeclarationGenerationTest
        SOURCES CKDeclarationGenerationTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKGuidComparisonTest
        SOURCES
        CKGuidComparisonTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CK2dCurveEvaluationTest
        SOURCES CK2dCurveEvaluationTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CK2dCurveTangentsTest
        SOURCES CK2dCurveTangentsTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CK2dCurveLifecycleTest
        SOURCES CK2dCurveLifecycleTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CK2dCurveSerializationTest
        SOURCES CK2dCurveSerializationTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CK2dCurveLayoutTest
        SOURCES CK2dCurveLayoutTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CK2dCurvePointTest
        SOURCES CK2dCurvePointTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKMessageDeliveryTest
        SOURCES CKMessageDeliveryTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKMemoryPoolTest
        SOURCES CKMemoryPoolTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBaseManagerTest
        SOURCES CKBaseManagerTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapDataTransparencyTest
        SOURCES CKBitmapDataTransparencyTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapSlotTest
        SOURCES CKBitmapSlotTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapPropertiesTest
        SOURCES CKBitmapPropertiesTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapPropertyHelpersTest
        SOURCES CKBitmapPropertyHelpersTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapReaderTest
        SOURCES CKBitmapReaderTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKBitmapReaderTest PRIVATE CK_LIB)

add_ck2_test(CKDataReaderTest
        SOURCES CKDataReaderTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKDataReaderTest PRIVATE CK_LIB)

add_ck2_test(CKImageReaderPropertyLifecycleTest
        SOURCES CKImageReaderPropertyLifecycleTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKImageReaderPropertyLifecycleTest PRIVATE CK_LIB)
target_compile_features(CKImageReaderPropertyLifecycleTest PRIVATE cxx_std_17)

add_ck2_test(CKPcxDecodeTest
        SOURCES CKPcxDecodeTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKPcxDecodeTest PRIVATE CK_LIB)
target_compile_features(CKPcxDecodeTest PRIVATE cxx_std_17)

add_ck2_test(CKBmpTgaDecodeTest
        SOURCES CKBmpTgaDecodeTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKBmpTgaDecodeTest PRIVATE CK_LIB)
target_compile_features(CKBmpTgaDecodeTest PRIVATE cxx_std_17)

add_ck2_test(CKBmpTgaSaveTest
        SOURCES CKBmpTgaSaveTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKBmpTgaSaveTest PRIVATE CK_LIB)
target_compile_features(CKBmpTgaSaveTest PRIVATE cxx_std_17)

add_ck2_test(CKBmpTgaSaveArithmeticTest
        SOURCES CKBmpTgaSaveArithmeticTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKBmpTgaSaveArithmeticTest PRIVATE CK_LIB)
target_compile_features(CKBmpTgaSaveArithmeticTest PRIVATE cxx_std_17)

add_ck2_test(CKBmpPaletteSaveTest
        SOURCES CKBmpPaletteSaveTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKBmpPaletteSaveTest PRIVATE CK_LIB)
target_compile_features(CKBmpPaletteSaveTest PRIVATE cxx_std_17)

add_ck2_test(CKImageSaveSignedPitchTest
        SOURCES CKImageSaveSignedPitchTest.cpp
                ../../Plugins/ImageReader/ImageReader.cpp
                ../../Plugins/ImageReader/BmpReader.cpp
                ../../Plugins/ImageReader/TgaReader.cpp
                ../../Plugins/ImageReader/PcxReader.cpp
        DEPENDENCIES CK2 VxMath)
target_compile_definitions(CKImageSaveSignedPitchTest PRIVATE CK_LIB)
target_compile_features(CKImageSaveSignedPitchTest PRIVATE cxx_std_17)

add_ck2_test(CKMovieReaderTest
        SOURCES CKMovieReaderTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(CKMovieReaderTest PRIVATE CK_LIB)
target_compile_features(CKMovieReaderTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderTimelineTest
        SOURCES AVIReaderTimelineTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderTimelineTest PRIVATE CK_LIB)
target_compile_features(AVIReaderTimelineTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderPaletteCodecTest
        SOURCES AVIReaderPaletteCodecTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderPaletteCodecTest PRIVATE CK_LIB)
target_compile_features(AVIReaderPaletteCodecTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderContinuationTest
        SOURCES AVIReaderContinuationTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderContinuationTest PRIVATE CK_LIB)
target_compile_features(AVIReaderContinuationTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderInterlacedTest
        SOURCES AVIReaderInterlacedTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderInterlacedTest PRIVATE CK_LIB)
target_compile_features(AVIReaderInterlacedTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderVprpTest
        SOURCES AVIReaderVprpTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderVprpTest PRIVATE CK_LIB)
target_compile_features(AVIReaderVprpTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderApertureTest
        SOURCES AVIReaderApertureTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderApertureTest PRIVATE CK_LIB)
target_compile_features(AVIReaderApertureTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderBroadcastTest
        SOURCES AVIReaderBroadcastTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderBroadcastTest PRIVATE CK_LIB)
target_compile_features(AVIReaderBroadcastTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderNtscOriginTest
        SOURCES AVIReaderNtscOriginTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderNtscOriginTest PRIVATE CK_LIB)
target_compile_features(AVIReaderNtscOriginTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderMixedIndexTest
        SOURCES AVIReaderMixedIndexTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderMixedIndexTest PRIVATE CK_LIB)
target_compile_features(AVIReaderMixedIndexTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderVirtualPaletteTest
        SOURCES AVIReaderVirtualPaletteTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderVirtualPaletteTest PRIVATE CK_LIB)
target_compile_features(AVIReaderVirtualPaletteTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderOpenDmlPaletteTest
        SOURCES AVIReaderOpenDmlPaletteTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderOpenDmlPaletteTest PRIVATE CK_LIB)
target_compile_features(AVIReaderOpenDmlPaletteTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderTimedPaletteTest
        SOURCES AVIReaderTimedPaletteTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderTimedPaletteTest PRIVATE CK_LIB)
target_compile_features(AVIReaderTimedPaletteTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderTimingTest
        SOURCES AVIReaderTimingTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderTimingTest PRIVATE CK_LIB)
target_compile_features(AVIReaderTimingTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderPaletteAuthorityTest
        SOURCES AVIReaderPaletteAuthorityTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderPaletteAuthorityTest PRIVATE CK_LIB)
target_compile_features(AVIReaderPaletteAuthorityTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderVideo1DimensionsTest
        SOURCES AVIReaderVideo1DimensionsTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderVideo1DimensionsTest PRIVATE CK_LIB)
target_compile_features(AVIReaderVideo1DimensionsTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderYvyuTest
        SOURCES AVIReaderYvyuTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderYvyuTest PRIVATE CK_LIB)
target_compile_features(AVIReaderYvyuTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderSeekTest
        SOURCES AVIReaderSeekTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderSeekTest PRIVATE CK_LIB)
target_compile_features(AVIReaderSeekTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderStreamHeaderTest
        SOURCES AVIReaderStreamHeaderTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderStreamHeaderTest PRIVATE CK_LIB)
target_compile_features(AVIReaderStreamHeaderTest PRIVATE cxx_std_17)

add_ck2_test(AVIReaderFormatSupportTest
        SOURCES AVIReaderFormatSupportTest.cpp
                ../../Plugins/AVIReader/AVIReader.cpp
                ../../Plugins/AVIReader/AviDemuxer.cpp
                ../../Plugins/AVIReader/FrameDecoder.cpp
                ../../Plugins/AVIReader/RiffReader.cpp
                ../../Plugins/AVIReader/StbImageImpl.cpp
        DEPENDENCIES CK2 VxMath stb)
target_compile_definitions(AVIReaderFormatSupportTest PRIVATE CK_LIB)
target_compile_features(AVIReaderFormatSupportTest PRIVATE cxx_std_17)

add_ck2_test(CKBitmapDataSlotsTest
        SOURCES CKBitmapDataSlotsTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapDataImageIoTest
        SOURCES CKBitmapDataImageIoTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapDataMovieTest
        SOURCES CKBitmapDataMovieTest.cpp
        DEPENDENCIES CK2 VxMath)

add_ck2_test(CKBitmapDataChunkTest
        SOURCES CKBitmapDataChunkTest.cpp
        DEPENDENCIES CK2 VxMath)

set(_ck2_ansi_test_target CK2)
if (TARGET CK2Static)
    set(_ck2_ansi_test_target CK2Static)
endif ()
add_ck2_test(CKFileAnsiPathTest SOURCES CKFileAnsiPathTest.cpp DEPENDENCIES ${_ck2_ansi_test_target} VxMath)
add_ck2_test(CKDataArrayAnsiPathTest SOURCES CKDataArrayAnsiPathTest.cpp DEPENDENCIES CK2 VxMath)
add_ck2_test(CKPathManagerAnsiPathTest SOURCES CKPathManagerAnsiPathTest.cpp DEPENDENCIES CK2 VxMath)

add_ck2_test(CKDeclarationLookupTest
        SOURCES CKDeclarationLookupTest.cpp
        DEPENDENCIES CK2 VxMath
)

add_ck2_test(CKBitmapDataStateTest
        SOURCES
        CKBitmapDataStateTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKParameterStringValueTest
        SOURCES CKParameterStringValueTest.cpp
        DEPENDENCIES CK2 VxMath
)

add_ck2_test(CKStateChunkCopyUseTest
        SOURCES CKStateChunkCopyUseTest.cpp
        DEPENDENCIES CK2 VxMath
)

add_ck2_test(CKStateChunkRoundTripTest
        SOURCES
        CKStateChunkRoundTripTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkIdentifierTest
        SOURCES
        CKStateChunkIdentifierTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkObjectReadTest
        SOURCES
        CKStateChunkObjectReadTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkConversionTest
        SOURCES
        CKStateChunkConversionTest.cpp
        DEPENDENCIES
        CK2 VxMath miniz
)

add_ck2_test(CKStateChunkSubChunkTest
        SOURCES
        CKStateChunkSubChunkTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkRemapTest
        SOURCES
        CKStateChunkRemapTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkAppendTest
        SOURCES
        CKStateChunkAppendTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkLifecycleTest
        SOURCES
        CKStateChunkLifecycleTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkCloneTest
        SOURCES
        CKStateChunkCloneTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkBufferTest
        SOURCES
        CKStateChunkBufferTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkCursorTest
        SOURCES
        CKStateChunkCursorTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKJpegDecoderRegressionTest
        SOURCES
        CKJpegDecoderRegressionTest.cpp
        "${CK2_SOURCE_DIR}/CKJpegDecoder.cpp"
        DEPENDENCIES
        CK2 VxMath stb
)

add_ck2_test(CKParameterInRegressionTest
        SOURCES
        CKParameterInRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKParameterOperationRegressionTest
        SOURCES
        CKParameterOperationRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorHierarchyPostLoadRegressionTest
        SOURCES
        CKBehaviorHierarchyPostLoadRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorIOParameterMatrixTest
        SOURCES
        CKBehaviorIOParameterMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKParameterOutDestinationMatrixTest
        SOURCES
        CKParameterOutDestinationMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorLinkRegressionTest
        SOURCES
        CKBehaviorLinkRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorExecutionLinkMatrixTest
        SOURCES
        CKBehaviorExecutionLinkMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorSchedulerEdgeRegressionTest
        SOURCES
        CKBehaviorSchedulerEdgeRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKDataArrayRegressionTest
        SOURCES
        CKDataArrayRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKContextRegressionTest
        SOURCES
        CKContextRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKSceneRegressionTest
        SOURCES
        CKSceneRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKPathManagerRegressionTest
        SOURCES
        CKPathManagerRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKPluginManagerPlatformTest
        SOURCES
        CKPluginManagerPlatformTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKFileCRCTest
        SOURCES
        CKFileCRCTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkWriterCloneTest
        SOURCES
        CKStateChunkWriterCloneTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkPackedCloneTest
        SOURCES
        CKStateChunkPackedCloneTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkPrimitiveTest
        SOURCES
        CKStateChunkPrimitiveTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkObjectSequenceTest
        SOURCES
        CKStateChunkObjectSequenceTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkManagerSequenceTest
        SOURCES
        CKStateChunkManagerSequenceTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkAncillaryTest
        SOURCES
        CKStateChunkAncillaryTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkBitmapTest
        SOURCES
        CKStateChunkBitmapTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkHelperTest
        SOURCES
        CKStateChunkHelperTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

if (TARGET CK2Static)
    add_ck2_test(CKStateChunkOwnershipTest
            SOURCES
            CKStateChunkOwnershipTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    add_ck2_test(CKBufferParserTest
            SOURCES
            CKBufferParserTest.cpp
            DEPENDENCIES
            CK2Static VxMath miniz
    )
    add_ck2_test(CKBufferParserRemainingTest
            SOURCES
            CKBufferParserRemainingTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    add_ck2_test(CKFileRecordTypesTest
            SOURCES
            CKFileRecordTypesTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    add_ck2_test(CKBufferParserFlushTest
            SOURCES
            CKBufferParserFlushTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    target_compile_features(CKBufferParserFlushTest PRIVATE cxx_std_17)
    add_ck2_test(CKBufferParserEncodeTest
            SOURCES
            CKBufferParserEncodeTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    add_ck2_test(CKBufferParserDecodedSliceTest
            SOURCES
            CKBufferParserDecodedSliceTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    add_ck2_test(CKBufferParserChunkOutputTest
            SOURCES
            CKBufferParserChunkOutputTest.cpp
            DEPENDENCIES
            CK2Static VxMath
    )
    target_compile_features(CKBufferParserEncodeTest PRIVATE cxx_std_17)
    target_compile_features(CKBufferParserDecodedSliceTest PRIVATE cxx_std_17)
    target_compile_features(CKBufferParserChunkOutputTest PRIVATE cxx_std_17)
endif ()

add_ck2_test(CKFileSaveLifecycleTest
        SOURCES
        CKFileSaveLifecycleTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKFileLoadLifecycleTest
        SOURCES
        CKFileLoadLifecycleTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKFileReuseLifecycleTest
        SOURCES
        CKFileReuseLifecycleTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileReuseLifecycleTest PRIVATE cxx_std_17)

add_ck2_test(CKFileLegacyFormatTest
        SOURCES
        CKFileLegacyFormatTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKFileLegacyRemainingTest
        SOURCES
        CKFileLegacyRemainingTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileLegacyRemainingTest PRIVATE cxx_std_17)

add_ck2_test(CKFilePluginDependenciesTest
        SOURCES
        CKFilePluginDependenciesTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFilePluginDependenciesTest PRIVATE cxx_std_17)

add_ck2_test(CKFileLayoutStateTest
        SOURCES
        CKFileLayoutStateTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileLayoutStateTest PRIVATE cxx_std_17)

add_ck2_test(CKFileIoTest
        SOURCES
        CKFileIoTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileIoTest PRIVATE cxx_std_17)

add_ck2_test(CKFileLoadSessionTest
        SOURCES
        CKFileLoadSessionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileLoadSessionTest PRIVATE cxx_std_17)

add_ck2_test(CKFileDependencyComparisonTest
        SOURCES
        CKFileDependencyComparisonTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
target_compile_features(CKFileDependencyComparisonTest PRIVATE cxx_std_17)

# Existing-API differential probe. Its fixtures predate lifetime handles.
add_executable(CKObjectBehaviorProbe CKObjectBehaviorProbe.cpp)
target_link_libraries(CKObjectBehaviorProbe PRIVATE CK2 VxMath)
target_compile_features(CKObjectBehaviorProbe PRIVATE cxx_std_17)
add_test(NAME CKObjectBehaviorDifferentialTest COMMAND ${CMAKE_COMMAND}
        -DPROBE=$<TARGET_FILE:CKObjectBehaviorProbe>
        -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/object-identity
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CKObjectBehaviorDifferentialTest.cmake)

add_ck2_test(CKObjectIdentityTest SOURCES CKObjectIdentityTest.cpp DEPENDENCIES CK2 VxMath)

if (WIN32 AND MSVC)
    target_sources(CKPathManagerRegressionTest PRIVATE LongPaths.manifest)
endif ()
