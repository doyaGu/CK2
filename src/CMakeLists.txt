set(CK2_PUBLIC_HEADERS
        ${CK2_INCLUDE_DIR}/CKAll.h

        # Defines and Global Functions
        ${CK2_INCLUDE_DIR}/CKDefines2.h
        ${CK2_INCLUDE_DIR}/CKGlobals.h
        ${CK2_INCLUDE_DIR}/CKBaseManager.h
        ${CK2_INCLUDE_DIR}/CKContext.h
        ${CK2_INCLUDE_DIR}/CKInterfaceObjectManager.h

        # Managers
        ${CK2_INCLUDE_DIR}/CKParameterManager.h
        ${CK2_INCLUDE_DIR}/CKTimeManager.h
        ${CK2_INCLUDE_DIR}/CKMessageManager.h
        ${CK2_INCLUDE_DIR}/CKRenderManager.h
        ${CK2_INCLUDE_DIR}/CKBehaviorManager.h
        ${CK2_INCLUDE_DIR}/CKAttributeManager.h
        ${CK2_INCLUDE_DIR}/CKPluginManager.h
        ${CK2_INCLUDE_DIR}/CKPathManager.h

        # External Managers
        ${CK2_INCLUDE_DIR}/CKFloorManager.h
        ${CK2_INCLUDE_DIR}/CKGridManager.h
        ${CK2_INCLUDE_DIR}/CKSoundManager.h
        ${CK2_INCLUDE_DIR}/CKMidiManager.h
        ${CK2_INCLUDE_DIR}/CKInputManager.h
        ${CK2_INCLUDE_DIR}/CKCollisionManager.h

        # Misc
        ${CK2_INCLUDE_DIR}/CKMaterial.h
        ${CK2_INCLUDE_DIR}/CKTexture.h
        ${CK2_INCLUDE_DIR}/CKRenderContext.h
        ${CK2_INCLUDE_DIR}/CKSynchroObject.h

        # Parameters
        ${CK2_INCLUDE_DIR}/CKParameter.h
        ${CK2_INCLUDE_DIR}/CKParameterIn.h
        ${CK2_INCLUDE_DIR}/CKParameterOut.h
        ${CK2_INCLUDE_DIR}/CKParameterLocal.h
        ${CK2_INCLUDE_DIR}/CKParameterOperation.h

        # Behaviors
        ${CK2_INCLUDE_DIR}/CKBehaviorIO.h
        ${CK2_INCLUDE_DIR}/CKBehaviorLink.h
        ${CK2_INCLUDE_DIR}/CKBehaviorPrototype.h
        ${CK2_INCLUDE_DIR}/CKBehavior.h
        ${CK2_INCLUDE_DIR}/CKMessage.h
        ${CK2_INCLUDE_DIR}/CKObjectDeclaration.h

        # Level/Scene/place
        ${CK2_INCLUDE_DIR}/CKLevel.h
        ${CK2_INCLUDE_DIR}/CKPlace.h
        ${CK2_INCLUDE_DIR}/CKGroup.h
        ${CK2_INCLUDE_DIR}/CKScene.h

        # Save/load
        ${CK2_INCLUDE_DIR}/CKStateChunk.h
        ${CK2_INCLUDE_DIR}/CKFile.h

        # Sound
        ${CK2_INCLUDE_DIR}/CKSound.h
        ${CK2_INCLUDE_DIR}/CKWaveSound.h
        ${CK2_INCLUDE_DIR}/CKMidiSound.h
        ${CK2_INCLUDE_DIR}/CKSoundReader.h

        # Curves
        ${CK2_INCLUDE_DIR}/CK2dCurve.h
        ${CK2_INCLUDE_DIR}/CK2dCurvePoint.h
        ${CK2_INCLUDE_DIR}/CKCurve.h
        ${CK2_INCLUDE_DIR}/CKCurvePoint.h

        # Character and Animation
        ${CK2_INCLUDE_DIR}/CKAnimation.h
        ${CK2_INCLUDE_DIR}/CKKeyedAnimation.h
        ${CK2_INCLUDE_DIR}/CKObjectAnimation.h
        ${CK2_INCLUDE_DIR}/CKKinematicChain.h
        ${CK2_INCLUDE_DIR}/CKCharacter.h

        # Base Objects
        ${CK2_INCLUDE_DIR}/CKObject.h
        ${CK2_INCLUDE_DIR}/CKSceneObject.h
        ${CK2_INCLUDE_DIR}/CKRenderObject.h
        ${CK2_INCLUDE_DIR}/CKBeObject.h
        ${CK2_INCLUDE_DIR}/CKDependencies.h

        # 2d Objects
        ${CK2_INCLUDE_DIR}/CK2dEntity.h
        ${CK2_INCLUDE_DIR}/CKSprite.h
        ${CK2_INCLUDE_DIR}/CKSpriteText.h

        # 3d Objects
        ${CK2_INCLUDE_DIR}/CKMesh.h
        ${CK2_INCLUDE_DIR}/CKPatchMesh.h
        ${CK2_INCLUDE_DIR}/CK3dEntity.h
        ${CK2_INCLUDE_DIR}/CKCamera.h
        ${CK2_INCLUDE_DIR}/CKTargetCamera.h
        ${CK2_INCLUDE_DIR}/CKSprite3D.h
        ${CK2_INCLUDE_DIR}/CKLight.h
        ${CK2_INCLUDE_DIR}/CKTargetLight.h
        ${CK2_INCLUDE_DIR}/CK3dObject.h
        ${CK2_INCLUDE_DIR}/CKBodyPart.h
        ${CK2_INCLUDE_DIR}/CKSkin.h

        # Containers
        ${CK2_INCLUDE_DIR}/XObjectArray.h
        ${CK2_INCLUDE_DIR}/CKDataArray.h
        ${CK2_INCLUDE_DIR}/CKDebugContext.h
        ${CK2_INCLUDE_DIR}/CKMemoryPool.h
)

set(CK2_PRIVATE_HEADERS

)

set(CK2_SOURCES
        CK2Win32.cpp

        # Global Functions and Base Classes
        CKGlobals.cpp
        CKBaseManager.cpp
        CKContext.cpp

        # Managers
        CKObjectManager.cpp
        CKParameterManager.cpp
        CKTimeManager.cpp
        CKMessageManager.cpp
        CKBehaviorManager.cpp
        CKAttributeManager.cpp
        CKPluginManager.cpp
        CKPathManager.cpp
        CKGridManager.cpp

        # Misc
        CKDebugContext.cpp
        CKSynchroObject.cpp
        CKRenderContext.cpp
        CKBitmapData.cpp
        CKMemoryPool.cpp
        CKJpegDecoder.cpp

        # Parameters
        CKParameter.cpp
        CKParameterIn.cpp
        CKParameterOut.cpp
        CKParameterLocal.cpp
        CKParameterOperation.cpp
        CKCallbackFunctions.cpp

        # Behaviors
        CKBehaviorIO.cpp
        CKBehaviorLink.cpp
        CKBehaviorPrototype.cpp
        CKBehavior.cpp
        CKMessage.cpp
        CKObjectDeclaration.cpp

        # Level/Scene/place
        CKLevel.cpp
        CKGroup.cpp
        CKScene.cpp

        # Save/load
        CKStateChunk.cpp
        CKFile.cpp

        # Sound
        CKSound.cpp
        CKSoundManager.cpp
        CKWaveSound.cpp
        CKMidiSound.cpp

        # Curves
        CK2dCurve.cpp
        CK2dCurvePoint.cpp

        # Base Objects
        CKObject.cpp
        CKSceneObject.cpp
        CKSceneObjectDesc.cpp
        CKBeObject.cpp
        CKDependencies.cpp
        CKInterfaceObjectManager.cpp

        CKRenderContext.cpp

        # Containers
        XObjectArray.cpp
        CKDataArray.cpp
        CKObjectArray.cpp
)

# Function to configure common target settings
function(ck2_configure_target TARGET_NAME)
    target_include_directories(${TARGET_NAME}
            PUBLIC
            $<BUILD_INTERFACE:${CK2_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(${TARGET_NAME}
            PUBLIC
            VxMath
            PRIVATE
            miniz
            stb
    )

    # Set library properties
    set_target_properties(${TARGET_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endfunction()

# Track created targets for export
set(CK2_TARGETS)

if (CK2_BUILD_SHARED)
    add_library(CK2 SHARED ${CK2_SOURCES} ${CK2_PUBLIC_HEADERS} ${CK2_PRIVATE_HEADERS})
    ck2_configure_target(CK2)

    # Shared library specific settings
    set_target_properties(CK2 PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    # Header ABI macros (see include/CKDefines.h)
    # CK2 itself defines CK_API to export symbols; consumers get dllimport by default.
    target_compile_definitions(CK2 PRIVATE CK_API)
    target_compile_options(CK2 PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UCK_LIB>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UCK_LIB>
    )

    list(APPEND CK2_TARGETS CK2)
endif ()

if (CK2_BUILD_STATIC)
    set(CK2_STATIC_SOURCES ${CK2_SOURCES})
    list(FILTER CK2_STATIC_SOURCES EXCLUDE REGEX "\\.rc$")

    add_library(CK2Static STATIC ${CK2_STATIC_SOURCES} ${CK2_PUBLIC_HEADERS} ${CK2_PRIVATE_HEADERS})
    ck2_configure_target(CK2Static)

    # Static library specific settings
    set_target_properties(CK2Static PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            POSITION_INDEPENDENT_CODE ON
    )

    # Static build: consumers must define CK_LIB to disable dllimport/dllexport.
    target_compile_definitions(CK2Static PUBLIC CK_LIB)

    list(APPEND CK2_TARGETS CK2Static)
endif ()

# Installation rules (only if CK2_INSTALL is enabled)
if (CK2_INSTALL)
    # Install library to Bin directory (shared) or lib directory (static)
    if (TARGET CK2)
        install(TARGETS CK2
                EXPORT CK2Targets
                RUNTIME DESTINATION Bin
                LIBRARY DESTINATION Bin
                ARCHIVE DESTINATION lib
        )
    endif ()

    if (TARGET CK2Static)
        install(TARGETS CK2Static
                EXPORT CK2Targets
                ARCHIVE DESTINATION lib
        )
    endif ()

    install(TARGETS miniz
            EXPORT CK2Targets
            ARCHIVE DESTINATION lib
    )

    # If VxMath is built in-tree (FetchContent), export/install it alongside CK2.
    # If it is an imported target from an installed package, do not re-install it.
    if (TARGET VxMath)
        get_target_property(_vxmath_imported VxMath IMPORTED)
        if (NOT _vxmath_imported)
            install(TARGETS VxMath
                    EXPORT CK2Targets
                    RUNTIME DESTINATION Bin
                    LIBRARY DESTINATION Bin
                    ARCHIVE DESTINATION lib
            )
        endif ()
    endif ()

    if (TARGET VxMathStatic)
        get_target_property(_vxmathstatic_imported VxMathStatic IMPORTED)
        if (NOT _vxmathstatic_imported)
            install(TARGETS VxMathStatic
                    EXPORT CK2Targets
                    ARCHIVE DESTINATION lib
            )
        endif ()
    endif ()

    # Install public headers to include root
    install(DIRECTORY ${CK2_INCLUDE_DIR}/
            DESTINATION include
            FILES_MATCHING PATTERN "*.h"
    )
endif ()
