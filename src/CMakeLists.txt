set(CK2_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

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

        # Misc
        CKDebugContext.cpp
        CKSynchroObject.cpp
        CKRenderContext.cpp
        CKBitmapData.cpp
        CKMemoryPool.cpp

        # Parameters
        CKParameter.cpp
        CKParameterIn.cpp
        CKParameterOut.cpp
        CKParameterLocal.cpp
        CKParameterOperation.cpp

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
        CKBeObject.cpp
        CKDependencies.cpp
        CKInterfaceObjectManager.cpp

        # Containers
        XObjectArray.cpp
        CKDataArray.cpp
        CKObjectArray.cpp
        )

add_library(CK2 SHARED ${CK2_SOURCES} ${CK2_PUBLIC_HEADERS} ${CK2_PRIVATE_HEADERS})
target_include_directories(CK2 PUBLIC $<BUILD_INTERFACE:${CK2_INCLUDE_DIR}>)
target_link_libraries(CK2 PRIVATE miniz "${CMAKE_CURRENT_SOURCE_DIR}/VxMath.lib")

install(TARGETS CK2 RUNTIME DESTINATION Bin)
