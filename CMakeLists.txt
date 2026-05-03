cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

add_subdirectory(thirdparty/miniaudio)
add_subdirectory(thirdparty/lua)

add_library(stellar_assets INTERFACE)
target_include_directories(stellar_assets INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

add_library(stellar_physics
    src/physics/CollisionWorld.cpp
    src/physics/CharacterController.cpp
)

target_include_directories(stellar_physics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_physics PUBLIC
    stellar_assets
)

add_library(stellar_world
    src/world/RuntimeWorld.cpp
    src/world/RuntimeCollisionState.cpp
    src/world/SensorOverlapTracker.cpp
    src/world/TriggerSystem.cpp
    src/world/ObjectCollider.cpp
    src/world/CollisionValidation.cpp
    src/world/WorldMetadataValidation.cpp
)

target_include_directories(stellar_world PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_world PUBLIC
    stellar_assets
    stellar_physics
)

add_library(stellar_server_core
    src/server/GameplayWorld.cpp
    src/server/MovementSimulation.cpp
    src/server/MovementTriggerIntegration.cpp
    src/server/WorldSession.cpp
)

target_include_directories(stellar_server_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_server_core PUBLIC
    stellar_world
    stellar_physics
)

add_library(stellar_scripting
    src/scripting/LuaRuntime.cpp
    src/scripting/ScriptCommandProcessor.cpp
    src/scripting/ScriptHookDispatcher.cpp
    src/scripting/ScriptRegistry.cpp
    src/scripting/ScriptSandbox.cpp
    src/scripting/ScriptedWorldSession.cpp
    src/scripting/ObjectColliderScriptSystem.cpp
    src/scripting/TriggerScriptSystem.cpp
)

target_include_directories(stellar_scripting PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_scripting PUBLIC
    stellar_world
    stellar_server_core
)

target_link_libraries(stellar_scripting PRIVATE
    stellar_lua
)

add_library(stellar_network
    src/network/LoopbackTransport.cpp
    src/network/Messages.cpp
    src/network/Session.cpp
    src/network/SocketTransport.cpp
    src/network/SnapshotCodec.cpp
    src/network/SnapshotDelta.cpp
)

target_include_directories(stellar_network PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_network PUBLIC
    stellar_server_core
    stellar_scripting
)

add_library(stellar_dedicated_server
    src/server/DedicatedServer.cpp
)

target_include_directories(stellar_dedicated_server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_dedicated_server PUBLIC
    stellar_import_bsp
    stellar_network
    stellar_scripting
)

add_library(stellar_audio
    src/audio/AudioEventRouter.cpp
)

target_include_directories(stellar_audio PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_audio PUBLIC
    stellar_network
)

add_library(stellar_import_bsp
    src/import/bsp/Loader.cpp
    src/import/bsp/Validation.cpp
    src/import/bsp/BspParser.cpp
    src/import/bsp/BspEntities.cpp
    src/import/bsp/DeveloperTextures.cpp
    src/import/bsp/Wad3Reader.cpp
    src/import/bsp/BspLevelBuilder.cpp
)

target_include_directories(stellar_import_bsp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_import_bsp PUBLIC
    stellar_assets
)

add_library(stellar_platform
    src/platform/DisplayDiagnostics.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
)

target_include_directories(stellar_platform PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(stellar_platform PUBLIC
    ${SDL2_LIBRARIES}
)

add_library(stellar_client_runtime
    src/client/ClientWorldReceiver.cpp
    src/client/HudPresentation.cpp
    src/client/LocalServerBridge.cpp
    src/client/MovementInputMapper.cpp
    src/client/NetworkedClientRuntime.cpp
    src/client/LocalLoopbackRuntime.cpp
    src/client/PlayerPresentation.cpp
    src/client/GameplayPresentation.cpp
    src/client/ScriptRegistryLoader.cpp
)

target_include_directories(stellar_client_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_runtime PUBLIC
    stellar_platform
    stellar_server_core
    stellar_scripting
    stellar_network
    stellar_graphics
)

add_library(stellar_graphics
    src/graphics/GraphicsBackend.cpp
    src/graphics/RendererFactory.cpp
    src/graphics/GraphicsDeviceFactory.cpp
    src/graphics/BillboardSprite.cpp
    src/graphics/RenderLevel.cpp
    src/graphics/LevelRenderer.cpp
    src/graphics/opengl/OpenGLGraphicsDevice.cpp
    src/graphics/vulkan/VulkanGraphicsDevice.cpp
    src/graphics/vulkan/VulkanGraphicsDeviceCommon.cpp
    src/graphics/vulkan/VulkanGraphicsDeviceDevice.cpp
    src/graphics/vulkan/VulkanGraphicsDeviceSwapchain.cpp
    src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
    src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
    src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
)

target_include_directories(stellar_graphics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(stellar_graphics PUBLIC
    stellar_platform
    ${OPENGL_LIBRARIES}
    Vulkan::Vulkan
    PkgConfig::GLEW
    PkgConfig::GLM
)

target_link_libraries(stellar_graphics PUBLIC stellar_assets)

add_library(stellar_client_config
    src/client/ApplicationConfig.cpp
)

target_include_directories(stellar_client_config PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_config PUBLIC
    stellar_assets
    stellar_client_runtime
    stellar_import_bsp
    stellar_world
)

add_executable(stellar-client
    src/client/main.cpp
    src/client/Application.cpp
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-client PRIVATE
    stellar_client_config
    stellar_client_runtime
    stellar_graphics
    stellar_audio
    stellar_assets
    miniaudio
)

add_executable(stellar-server
    src/server/main.cpp
)

target_include_directories(stellar-server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-server PRIVATE
    stellar_dedicated_server
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
