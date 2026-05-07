cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.3.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STELLAR_ENABLE_OPENGL_BACKEND "Build the OpenGL graphics backend" ON)
option(STELLAR_ENABLE_METAL "Build the experimental Metal graphics backend on Apple platforms" OFF)
option(STELLAR_ENABLE_VULKAN_BACKEND "Build the Vulkan graphics backend on Linux" OFF)
option(STELLAR_ENABLE_VULKAN_VALIDATION
       "Enable Vulkan validation layer diagnostics in dev builds" ON)
option(STELLAR_MINIAUDIO_NO_RUNTIME_LINKING
       "Use explicit CoreAudio framework links for miniaudio on Apple platforms" OFF)

if(STELLAR_ENABLE_METAL AND NOT APPLE)
    message(FATAL_ERROR "STELLAR_ENABLE_METAL requires an Apple platform")
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR
        "STELLAR_ENABLE_VULKAN_BACKEND is Linux-only in Project Stellar; use Metal on macOS")
endif()

if(STELLAR_ENABLE_METAL)
    enable_language(OBJCXX)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
endif()

include(CTest)
include(cmake/StellarDocs.cmake)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)

pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

if(STELLAR_ENABLE_OPENGL_BACKEND)
    find_package(OpenGL REQUIRED)
    pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
    find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin")
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc is required for STELLAR_ENABLE_VULKAN_BACKEND")
    endif()
    set(STELLAR_VULKAN_SHADER_OUTPUT_DIR
        "${CMAKE_BINARY_DIR}/generated/shaders/vulkan")
    set(STELLAR_VULKAN_VERTEX_SHADER
        "${CMAKE_SOURCE_DIR}/assets/shaders/vulkan/static_level.vert")
    set(STELLAR_VULKAN_FRAGMENT_SHADER
        "${CMAKE_SOURCE_DIR}/assets/shaders/vulkan/static_level.frag")
    set(STELLAR_VULKAN_VERTEX_SPV
        "${STELLAR_VULKAN_SHADER_OUTPUT_DIR}/static_level.vert.spv")
    set(STELLAR_VULKAN_FRAGMENT_SPV
        "${STELLAR_VULKAN_SHADER_OUTPUT_DIR}/static_level.frag.spv")
    add_custom_command(
        OUTPUT "${STELLAR_VULKAN_VERTEX_SPV}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${STELLAR_VULKAN_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" -fshader-stage=vert
                "${STELLAR_VULKAN_VERTEX_SHADER}" -o "${STELLAR_VULKAN_VERTEX_SPV}"
        DEPENDS "${STELLAR_VULKAN_VERTEX_SHADER}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${STELLAR_VULKAN_FRAGMENT_SPV}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${STELLAR_VULKAN_SHADER_OUTPUT_DIR}"
        COMMAND "${GLSLC_EXECUTABLE}" -fshader-stage=frag
                "${STELLAR_VULKAN_FRAGMENT_SHADER}" -o "${STELLAR_VULKAN_FRAGMENT_SPV}"
        DEPENDS "${STELLAR_VULKAN_FRAGMENT_SHADER}"
        VERBATIM
    )
    add_custom_target(stellar_vulkan_shaders ALL
        DEPENDS "${STELLAR_VULKAN_VERTEX_SPV}" "${STELLAR_VULKAN_FRAGMENT_SPV}"
    )
endif()

add_subdirectory(thirdparty/miniaudio)
add_subdirectory(thirdparty/lua)

add_library(stellar_stb INTERFACE)
target_include_directories(stellar_stb INTERFACE
    ${CMAKE_SOURCE_DIR}/thirdparty/stb
)

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
    src/server/FootstepTracker.cpp
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

add_library(stellar_protocol
    src/network/Session.cpp
    src/network/SnapshotCodec.cpp
    src/network/SnapshotDelta.cpp
)

target_include_directories(stellar_protocol PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

add_library(stellar_transport
    src/network/LoopbackTransport.cpp
    src/network/SocketTransport.cpp
)

target_include_directories(stellar_transport PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_transport PUBLIC
    stellar_protocol
)

add_library(stellar_network INTERFACE)
target_link_libraries(stellar_network INTERFACE
    stellar_protocol
    stellar_transport
)

add_library(stellar_authority
    src/authority/AuthorityBootstrap.cpp
    src/authority/NetworkConversion.cpp
)

target_include_directories(stellar_authority PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_authority PUBLIC
    stellar_server_core
    stellar_scripting
    stellar_protocol
)

add_library(stellar_dedicated_server
    src/server/DedicatedServer.cpp
)

target_include_directories(stellar_dedicated_server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_dedicated_server PUBLIC
    stellar_import_bsp
    stellar_authority
    stellar_transport
)

add_library(stellar_audio
    src/audio/AudioEventRouter.cpp
)

target_include_directories(stellar_audio PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_audio PUBLIC
    stellar_protocol
)

add_library(stellar_audio_miniaudio
    src/audio/MiniaudioSink.cpp
)

target_include_directories(stellar_audio_miniaudio PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(stellar_audio_miniaudio PRIVATE
    STELLAR_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(stellar_audio_miniaudio PUBLIC
    stellar_audio
)

target_link_libraries(stellar_audio_miniaudio PRIVATE
    miniaudio
)

add_library(stellar_import_bsp
    src/import/bsp/Loader.cpp
    src/import/bsp/Validation.cpp
    src/import/bsp/BspParser.cpp
    src/import/bsp/BspEntities.cpp
    src/import/bsp/BspAssetDebugNames.cpp
    src/import/bsp/BspEntitySchema.cpp
    src/import/bsp/BspGeometryBuilder.cpp
    src/import/bsp/BspImportDiagnostics.cpp
    src/import/bsp/BspLightmapBuilder.cpp
    src/import/bsp/BspTextureResolver.cpp
    src/import/bsp/DeveloperTextures.cpp
    src/import/bsp/ImageFileLoader.cpp
    src/import/bsp/MaterialSidecar.cpp
    src/import/bsp/Wad3Reader.cpp
    src/import/bsp/BspLevelBuilder.cpp
)

target_include_directories(stellar_import_bsp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_import_bsp PUBLIC
    stellar_assets
    stellar_stb
)

target_link_libraries(stellar_authority PUBLIC
    stellar_import_bsp
)

add_library(stellar_server_runtime
    src/server/ServerRuntime.cpp
)

target_include_directories(stellar_server_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_server_runtime PUBLIC
    stellar_authority
    stellar_transport
    stellar_server_core
    stellar_scripting
)

target_link_libraries(stellar_dedicated_server PUBLIC
    stellar_server_runtime
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

add_library(stellar_client_net
    src/client/ClientWorldReceiver.cpp
    src/client/MovementInputMapper.cpp
    src/client/RemoteClientRuntime.cpp
)

target_include_directories(stellar_client_net PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_net PUBLIC
    stellar_transport
    stellar_platform
)

add_library(stellar_single_player
    src/client/SinglePlayerRuntime.cpp
)

target_include_directories(stellar_single_player PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_single_player PUBLIC
    stellar_authority
    stellar_client_net
)

add_library(stellar_listen_server
    src/client/ListenHostRuntime.cpp
)

target_include_directories(stellar_listen_server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_listen_server PUBLIC
    stellar_authority
    stellar_server_runtime
    stellar_client_net
)

add_library(stellar_client_presentation
    src/client/HudPresentation.cpp
    src/client/PlayerPresentation.cpp
    src/client/GameplayPresentation.cpp
)

target_include_directories(stellar_client_presentation PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_presentation PUBLIC
    stellar_protocol
    stellar_assets
    PkgConfig::GLM
)

add_library(stellar_client_runtime INTERFACE)
target_link_libraries(stellar_client_runtime INTERFACE
    stellar_client_net
    stellar_single_player
    stellar_listen_server
    stellar_client_presentation
)

set(STELLAR_GRAPHICS_SOURCES
    src/graphics/GraphicsBackend.cpp
    src/graphics/RendererFactory.cpp
    src/graphics/GraphicsDeviceFactory.cpp
    src/graphics/BillboardSprite.cpp
    src/graphics/RenderLevel.cpp
    src/graphics/LevelRenderer.cpp
)

if(STELLAR_ENABLE_OPENGL_BACKEND)
    list(APPEND STELLAR_GRAPHICS_SOURCES
        src/graphics/opengl/OpenGLGraphicsDevice.cpp
    )
endif()

if(STELLAR_ENABLE_METAL)
    list(APPEND STELLAR_GRAPHICS_SOURCES
        src/graphics/metal/MetalGraphicsDevice.mm
    )
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND)
    list(APPEND STELLAR_GRAPHICS_SOURCES
        src/graphics/vulkan/VulkanGraphicsDevice.cpp
    )
endif()

add_library(stellar_graphics
    ${STELLAR_GRAPHICS_SOURCES}
)

target_include_directories(stellar_graphics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(stellar_graphics PUBLIC
    stellar_platform
    PkgConfig::GLM
)

if(STELLAR_ENABLE_OPENGL_BACKEND)
    target_compile_definitions(stellar_graphics PUBLIC STELLAR_ENABLE_OPENGL_BACKEND=1)
    target_include_directories(stellar_graphics PUBLIC
        ${OPENGL_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
    )
    target_link_libraries(stellar_graphics PUBLIC
        ${OPENGL_LIBRARIES}
        PkgConfig::GLEW
    )
endif()

if(STELLAR_ENABLE_METAL)
    target_compile_definitions(stellar_graphics PUBLIC STELLAR_ENABLE_METAL_BACKEND=1)
    target_link_libraries(stellar_graphics PRIVATE
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND)
    add_dependencies(stellar_graphics stellar_vulkan_shaders)
    target_compile_definitions(stellar_graphics PUBLIC
        STELLAR_ENABLE_VULKAN_BACKEND=1
    )
    if(STELLAR_ENABLE_VULKAN_VALIDATION)
        target_compile_definitions(stellar_graphics PUBLIC
            STELLAR_ENABLE_VULKAN_VALIDATION=1
        )
    endif()
    target_link_libraries(stellar_graphics PUBLIC Vulkan::Vulkan)
endif()

target_link_libraries(stellar_graphics PUBLIC stellar_assets)

add_library(stellar_client_config
    src/client/ApplicationConfig.cpp
)

target_include_directories(stellar_client_config PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_config PUBLIC
    stellar_assets
    stellar_client_net
    stellar_single_player
    stellar_listen_server
    stellar_graphics
    stellar_import_bsp
    stellar_world
)

add_library(stellar_client_app
    src/client/Application.cpp
)

target_include_directories(stellar_client_app PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_app PUBLIC
    stellar_client_config
    stellar_client_runtime
    stellar_client_presentation
    stellar_graphics
    stellar_audio
    stellar_audio_miniaudio
    stellar_assets
)

add_executable(stellar-client
    src/client/main.cpp
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-client PRIVATE
    stellar_client_app
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

add_executable(stellar-audio-smoke
    tools/audio/AudioSmokeMain.cpp
)

target_include_directories(stellar-audio-smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-audio-smoke PRIVATE
    stellar_audio_miniaudio
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

function(stellar_assert_no_direct_links target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Boundary check target does not exist: ${target_name}")
    endif()

    set(forbidden_links ${ARGN})
    get_target_property(public_links ${target_name} LINK_LIBRARIES)
    get_target_property(interface_links ${target_name} INTERFACE_LINK_LIBRARIES)
    set(all_links ${public_links} ${interface_links})

    foreach(link_name IN LISTS all_links)
        foreach(forbidden IN LISTS forbidden_links)
            if(link_name STREQUAL forbidden)
                message(FATAL_ERROR
                    "Target boundary violation: ${target_name} directly links ${forbidden}")
            endif()
        endforeach()
    endforeach()
endfunction()

stellar_assert_no_direct_links(stellar_protocol
    stellar_authority stellar_server_core stellar_server_runtime stellar_dedicated_server
    stellar_scripting stellar_client_runtime stellar_client_net stellar_single_player
    stellar_listen_server stellar_client_presentation stellar_client_app stellar_graphics
    stellar_audio stellar_audio_miniaudio miniaudio stellar_platform Vulkan::Vulkan
)
stellar_assert_no_direct_links(stellar_transport
    stellar_authority stellar_server_core stellar_server_runtime stellar_dedicated_server
    stellar_scripting stellar_client_runtime stellar_client_presentation stellar_client_app
)
stellar_assert_no_direct_links(stellar_client_net
    stellar_authority stellar_server_core stellar_server_runtime stellar_dedicated_server
    stellar_scripting
)
stellar_assert_no_direct_links(stellar_dedicated_server
    stellar_client_runtime stellar_client_net stellar_single_player stellar_listen_server
    stellar_client_presentation stellar_client_app stellar_graphics stellar_audio
    stellar_audio_miniaudio miniaudio Vulkan::Vulkan
)
stellar_assert_no_direct_links(stellar_server_runtime
    stellar_client_runtime stellar_client_net stellar_single_player stellar_listen_server
    stellar_client_presentation stellar_client_app stellar_graphics stellar_audio
    stellar_audio_miniaudio miniaudio Vulkan::Vulkan
)

add_custom_target(stellar_target_boundary_check
    COMMAND ${CMAKE_SOURCE_DIR}/tools/dev/check_target_boundaries.sh ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking Stellar CMake target boundaries"
)

if(BUILD_TESTING)
    add_test(NAME target_boundary
        COMMAND ${CMAKE_SOURCE_DIR}/tools/dev/check_target_boundaries.sh ${CMAKE_SOURCE_DIR}
    )
endif()
