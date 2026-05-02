cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Find packages
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

option(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS
       "Build optional OpenGL context/readback smoke tests" OFF)
option(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS
       "Build optional Vulkan context/init smoke tests" OFF)

function(stellar_add_test_executable target_name test_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

function(stellar_add_world_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_world)
endfunction()

function(stellar_add_server_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_server_core)
endfunction()

function(stellar_add_physics_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_physics)
endfunction()

function(stellar_add_scripting_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_scripting)
endfunction()

function(stellar_add_graphics_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_graphics)
endfunction()

function(stellar_add_client_runtime_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_client_runtime)
endfunction()

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
    src/import/bsp/BspLevelBuilder.cpp
)

target_include_directories(stellar_import_bsp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_import_bsp PUBLIC
    stellar_assets
)

add_library(stellar_platform
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

stellar_add_world_test(stellar_runtime_world_test runtime_world
    tests/world/RuntimeWorld.cpp
)

stellar_add_world_test(stellar_runtime_collision_state_test runtime_collision_state
    tests/world/RuntimeCollisionState.cpp
)

stellar_add_world_test(stellar_trigger_system_test trigger_system
    tests/world/TriggerSystem.cpp
)

stellar_add_world_test(stellar_object_collider_test object_collider
    tests/world/ObjectCollider.cpp
)

stellar_add_world_test(stellar_collision_validation_test collision_validation
    tests/world/CollisionValidation.cpp
)

stellar_add_world_test(stellar_world_metadata_validation_test world_metadata_validation
    tests/world/WorldMetadataValidation.cpp
)

stellar_add_server_test(stellar_server_movement_simulation_test server_movement_simulation
    tests/server/MovementSimulation.cpp
)

stellar_add_server_test(stellar_movement_trigger_integration_test movement_trigger_integration
    tests/server/MovementTriggerIntegration.cpp
)

stellar_add_server_test(stellar_server_world_session_test server_world_session
    tests/server/WorldSession.cpp
)

stellar_add_server_test(stellar_server_gameplay_world_test server_gameplay_world
    tests/server/GameplayWorld.cpp
)

stellar_add_test_executable(stellar_dedicated_server_test dedicated_server
    tests/server/DedicatedServer.cpp
)
target_include_directories(stellar_dedicated_server_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)
target_link_libraries(stellar_dedicated_server_test PRIVATE stellar_dedicated_server)

stellar_add_graphics_test(stellar_render_level_upload_test render_level_upload
    tests/graphics/RenderSceneUpload.cpp
)

add_executable(stellar_render_level_inspection_test
    tests/graphics/RenderSceneInspection.cpp
)

target_include_directories(stellar_render_level_inspection_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/graphics
)

target_link_libraries(stellar_render_level_inspection_test PRIVATE
    stellar_graphics
)

add_test(NAME render_level_inspection COMMAND stellar_render_level_inspection_test)

stellar_add_graphics_test(stellar_graphics_backend_selection_test graphics_backend_selection
    tests/graphics/BackendSelection.cpp
)

if(STELLAR_ENABLE_OPENGL_CONTEXT_TESTS)
    add_executable(stellar_opengl_context_smoke_test
        tests/graphics/OpenGLContextSmoke.cpp
    )

    target_include_directories(stellar_opengl_context_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${OPENGL_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_opengl_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME opengl_context_smoke COMMAND stellar_opengl_context_smoke_test)
    set_tests_properties(opengl_context_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

if(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS)
    add_executable(stellar_vulkan_context_smoke_test
        tests/graphics/VulkanContextSmoke.cpp
    )

    target_include_directories(stellar_vulkan_context_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_vulkan_context_smoke_test PRIVATE
        stellar_graphics
    )

    add_test(NAME vulkan_context_smoke COMMAND stellar_vulkan_context_smoke_test)
endif()

stellar_add_test_executable(stellar_import_bsp_test bsp_importer
    tests/import/bsp/BspImporter.cpp
)
target_link_libraries(stellar_import_bsp_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_entity_authoring_test bsp_entity_authoring
    tests/import/bsp/BspEntityAuthoring.cpp
)
target_link_libraries(stellar_bsp_entity_authoring_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_visibility_test bsp_visibility
    tests/import/bsp/BspVisibility.cpp
)
target_link_libraries(stellar_bsp_visibility_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_materials_test bsp_materials
    tests/import/bsp/BspMaterials.cpp
)
target_link_libraries(stellar_bsp_materials_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_lightmaps_test bsp_lightmaps
    tests/import/bsp/BspLightmaps.cpp
)
target_link_libraries(stellar_bsp_lightmaps_test PRIVATE stellar_import_bsp)

stellar_add_test_executable(stellar_bsp_validation_test bsp_validation
    tests/import/bsp/BspValidation.cpp
)
target_include_directories(stellar_bsp_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/fixtures
    ${CMAKE_SOURCE_DIR}/tests/import/bsp
)
target_link_libraries(stellar_bsp_validation_test PRIVATE stellar_import_bsp)

stellar_add_physics_test(stellar_collision_world_test collision_world
    tests/physics/CollisionWorld.cpp
)

stellar_add_physics_test(stellar_character_controller_test character_controller
    tests/physics/CharacterController.cpp
)

stellar_add_physics_test(stellar_collision_performance_regression_test collision_performance_regression
    tests/physics/CollisionPerformanceRegression.cpp
)

add_executable(stellar_client_map_validation_smoke
    tests/client/AssetValidationSmoke.cpp
)

target_include_directories(stellar_client_map_validation_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)

target_link_libraries(stellar_client_map_validation_smoke PRIVATE
    stellar_client_config
)

add_test(NAME client_map_validation_smoke COMMAND stellar_client_map_validation_smoke)

stellar_add_client_runtime_test(stellar_client_movement_input_mapper_test client_movement_input_mapper
    tests/client/MovementInputMapper.cpp
)

stellar_add_client_runtime_test(stellar_player_presentation_test player_presentation
    tests/client/PlayerPresentation.cpp
)

stellar_add_client_runtime_test(stellar_gameplay_presentation_test gameplay_presentation
    tests/client/GameplayPresentation.cpp
)

stellar_add_client_runtime_test(stellar_client_local_loopback_runtime_test client_local_loopback_runtime
    tests/client/LocalLoopbackRuntime.cpp
)

stellar_add_client_runtime_test(stellar_client_world_receiver_test client_world_receiver
    tests/client/ClientWorldReceiver.cpp
)

stellar_add_client_runtime_test(stellar_networked_client_runtime_test networked_client_runtime
    tests/client/NetworkedClientRuntime.cpp
)

stellar_add_test_executable(stellar_client_connect_test client_connect
    tests/client/ClientConnect.cpp
)
target_include_directories(stellar_client_connect_test PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)
target_link_libraries(stellar_client_connect_test PRIVATE
    stellar_client_config
    stellar_dedicated_server
)

stellar_add_client_runtime_test(stellar_hud_presentation_test hud_presentation
    tests/client/HudPresentation.cpp
)

stellar_add_test_executable(stellar_audio_event_router_test audio_event_router
    tests/audio/AudioEventRouter.cpp
)
target_link_libraries(stellar_audio_event_router_test PRIVATE stellar_audio)

stellar_add_scripting_test(stellar_lua_runtime_test lua_runtime
    tests/scripting/LuaRuntime.cpp
)

stellar_add_scripting_test(stellar_trigger_script_system_test trigger_script
    tests/scripting/TriggerScriptSystem.cpp
)

stellar_add_scripting_test(stellar_object_collider_script_system_test object_collider_script
    tests/scripting/ObjectColliderScriptSystem.cpp
)

stellar_add_scripting_test(stellar_script_command_processor_test script_command_processor
    tests/scripting/ScriptCommandProcessor.cpp
)

stellar_add_scripting_test(stellar_scripted_world_session_test scripted_world_session
    tests/scripting/ScriptedWorldSession.cpp
)

stellar_add_test_executable(stellar_snapshot_codec_test snapshot_codec
    tests/network/SnapshotCodec.cpp
)
target_link_libraries(stellar_snapshot_codec_test PRIVATE stellar_network)

stellar_add_test_executable(stellar_network_session_test network_session
    tests/network/Session.cpp
)
target_link_libraries(stellar_network_session_test PRIVATE stellar_network stellar_client_runtime)

stellar_add_test_executable(stellar_loopback_transport_test loopback_transport
    tests/network/LoopbackTransport.cpp
)
target_link_libraries(stellar_loopback_transport_test PRIVATE stellar_network)

stellar_add_test_executable(stellar_socket_transport_test socket_transport
    tests/network/SocketTransport.cpp
)
target_link_libraries(stellar_socket_transport_test PRIVATE stellar_network stellar_client_runtime)

stellar_add_test_executable(stellar_snapshot_delta_test snapshot_delta
    tests/network/SnapshotDelta.cpp
)
target_link_libraries(stellar_snapshot_delta_test PRIVATE stellar_network)

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

add_executable(stellar_bsp_fixture_writer
    tests/fixtures/BspFixtureWriter.cpp
)

target_include_directories(stellar_bsp_fixture_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)

set(STELLAR_CLI_BSP_FIXTURE
    ${CMAKE_BINARY_DIR}/tests/fixtures/bsp/client_cli_map_validation.bsp
)
set(STELLAR_CLI_BAD_BSP_FIXTURE
    ${CMAKE_BINARY_DIR}/tests/fixtures/bsp/client_cli_bad_map_validation.bsp
)

add_test(NAME bsp_fixture_writer
    COMMAND stellar_bsp_fixture_writer ${STELLAR_CLI_BSP_FIXTURE} gameplay_room
)
set_tests_properties(bsp_fixture_writer PROPERTIES FIXTURES_SETUP bsp_cli_fixture)

add_test(NAME bsp_bad_fixture_writer
    COMMAND stellar_bsp_fixture_writer ${STELLAR_CLI_BAD_BSP_FIXTURE} invalid_script_path
)
set_tests_properties(bsp_bad_fixture_writer PROPERTIES FIXTURES_SETUP bsp_bad_cli_fixture)

add_test(NAME client_cli_map_validation
    COMMAND stellar-client --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(client_cli_map_validation PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME client_cli_validate_map
    COMMAND stellar-client --validate-map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(client_cli_validate_map PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME client_cli_map_validation_rejects_bad_map
    COMMAND stellar-client --validate-map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)
set_tests_properties(client_cli_map_validation_rejects_bad_map PROPERTIES
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    WILL_FAIL TRUE
)

add_test(NAME server_cli_validate_config
    COMMAND stellar-server --validate-config --map ${STELLAR_CLI_BSP_FIXTURE}
)
set_tests_properties(server_cli_validate_config PROPERTIES FIXTURES_REQUIRED bsp_cli_fixture)

add_test(NAME server_cli_rejects_bad_map
    COMMAND stellar-server --validate-config --map ${STELLAR_CLI_BAD_BSP_FIXTURE}
)
set_tests_properties(server_cli_rejects_bad_map PROPERTIES
    FIXTURES_REQUIRED bsp_bad_cli_fixture
    WILL_FAIL TRUE
)

add_executable(stellar_bsp_playable_world_smoke_test
    tests/integration/BspPlayableWorldSmoke.cpp
)

target_include_directories(stellar_bsp_playable_world_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)

target_link_libraries(stellar_bsp_playable_world_smoke_test PRIVATE
    stellar_import_bsp
    stellar_client_runtime
    stellar_scripting
)

add_test(NAME bsp_playable_world_smoke COMMAND stellar_bsp_playable_world_smoke_test)
add_test(NAME bsp_scripted_playable_world_smoke COMMAND stellar_bsp_playable_world_smoke_test)
add_test(NAME bsp_scripted_collision_smoke COMMAND stellar_bsp_playable_world_smoke_test)
add_test(NAME bsp_scripted_object_collider_smoke COMMAND stellar_bsp_playable_world_smoke_test)

add_executable(stellar_bsp_authoring_smoke_test
    tests/integration/BspAuthoringSmoke.cpp
)

target_include_directories(stellar_bsp_authoring_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/fixtures
)

target_link_libraries(stellar_bsp_authoring_smoke_test PRIVATE
    stellar_client_config
)

add_test(NAME bsp_authoring_smoke COMMAND stellar_bsp_authoring_smoke_test)
