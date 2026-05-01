cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.1.3 LANGUAGES C CXX)

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

option(STELLAR_ENABLE_GLTF "Enable the glTF importer seam" OFF)
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

function(stellar_add_gltf_world_test target_name test_name)
    stellar_add_test_executable(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE
        stellar_import_gltf
        stellar_world
        stellar_server_core
    )
endfunction()

function(stellar_add_scripted_gltf_world_test target_name test_name)
    stellar_add_gltf_world_test(${target_name} ${test_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE stellar_scripting)
endfunction()

add_subdirectory(thirdparty/miniaudio)
if(STELLAR_ENABLE_GLTF)
    add_subdirectory(thirdparty/cgltf)
endif()
add_subdirectory(thirdparty/lua)

add_library(stellar_scene
    src/scene/AnimationRuntime.cpp
)
target_include_directories(stellar_scene PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${GLM_INCLUDE_DIRS}
)
target_link_libraries(stellar_scene PUBLIC PkgConfig::GLM)

add_library(stellar_assets INTERFACE)
target_include_directories(stellar_assets INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(stellar_assets INTERFACE stellar_scene)

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

if(STELLAR_ENABLE_GLTF)
    add_library(stellar_import_gltf
        src/import/gltf/Importer.cpp
        src/import/gltf/Loader.cpp
        src/import/gltf/CgltfUtils.cpp
        src/import/gltf/DataUri.cpp
        src/import/gltf/ImageImport.cpp
        src/import/gltf/MaterialImport.cpp
        src/import/gltf/TextureImport.cpp
        src/import/gltf/MeshImport.cpp
        src/import/gltf/SkinImport.cpp
        src/import/gltf/AnimationImport.cpp
        src/import/gltf/NodeImport.cpp
        src/import/gltf/CollisionImport.cpp
        src/import/gltf/WorldMetadataImport.cpp
        src/import/gltf/SceneImport.cpp
    )

    target_include_directories(stellar_import_gltf PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/thirdparty/cgltf
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_import_gltf PUBLIC
        stellar_assets
        cgltf
    )

    target_compile_definitions(stellar_import_gltf PRIVATE STELLAR_ENABLE_GLTF=1)
endif()

add_library(stellar_import_bsp
    src/import/bsp/Loader.cpp
    src/import/bsp/BspParser.cpp
    src/import/bsp/BspEntities.cpp
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
    src/client/MovementInputMapper.cpp
    src/client/LocalLoopbackRuntime.cpp
    src/client/PlayerPresentation.cpp
)

target_include_directories(stellar_client_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_runtime PUBLIC
    stellar_platform
    stellar_server_core
)

add_library(stellar_graphics
    src/graphics/GraphicsBackend.cpp
    src/graphics/RendererFactory.cpp
    src/graphics/GraphicsDeviceFactory.cpp
    src/graphics/BillboardSprite.cpp
    src/graphics/DebugCubeMesh.cpp
    src/graphics/RenderScene.cpp
    src/graphics/SceneRenderer.cpp
    src/graphics/opengl/CubeRenderer.cpp
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
    stellar_scene
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

stellar_add_graphics_test(stellar_render_scene_upload_test render_scene_upload
    tests/graphics/RenderSceneUpload.cpp
)

add_executable(stellar_render_scene_inspection_test
    tests/graphics/RenderSceneInspection.cpp
)

target_include_directories(stellar_render_scene_inspection_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/graphics
)

target_link_libraries(stellar_render_scene_inspection_test PRIVATE
    stellar_graphics
)

add_test(NAME render_scene_inspection COMMAND stellar_render_scene_inspection_test)

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

stellar_add_test_executable(stellar_animation_runtime_test animation_runtime
    tests/scene/AnimationRuntime.cpp
)
target_link_libraries(stellar_animation_runtime_test PRIVATE stellar_scene)

stellar_add_test_executable(stellar_import_bsp_test bsp_importer
    tests/import/bsp/BspImporter.cpp
)
target_link_libraries(stellar_import_bsp_test PRIVATE stellar_import_bsp)

stellar_add_physics_test(stellar_collision_world_test collision_world
    tests/physics/CollisionWorld.cpp
)

stellar_add_physics_test(stellar_character_controller_test character_controller
    tests/physics/CharacterController.cpp
)

stellar_add_physics_test(stellar_collision_performance_regression_test collision_performance_regression
    tests/physics/CollisionPerformanceRegression.cpp
)

add_executable(stellar_client_asset_validation_smoke
    tests/client/AssetValidationSmoke.cpp
)

target_include_directories(stellar_client_asset_validation_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_asset_validation_smoke PRIVATE
    stellar_client_config
)

add_test(NAME client_asset_validation_smoke COMMAND stellar_client_asset_validation_smoke)

stellar_add_client_runtime_test(stellar_client_movement_input_mapper_test client_movement_input_mapper
    tests/client/MovementInputMapper.cpp
)

stellar_add_client_runtime_test(stellar_player_presentation_test player_presentation
    tests/client/PlayerPresentation.cpp
)

stellar_add_client_runtime_test(stellar_client_local_loopback_runtime_test client_local_loopback_runtime
    tests/client/LocalLoopbackRuntime.cpp
)

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

add_executable(stellar-client
    src/client/main.cpp
    src/client/Application.cpp
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-client PRIVATE
    stellar_client_config
    stellar_graphics
    stellar_assets
    stellar_scene
    miniaudio
)

set(STELLAR_EMPTY_GLTF_FIXTURE
    ${CMAKE_SOURCE_DIR}/tests/fixtures/gltf/empty_scene.gltf
)

add_test(NAME client_cli_asset_validation
    COMMAND stellar-client --validate-config --asset ${STELLAR_EMPTY_GLTF_FIXTURE}
)

if(STELLAR_ENABLE_GLTF)
    target_compile_definitions(stellar_client_config PRIVATE STELLAR_ENABLE_GLTF=1)
    target_compile_definitions(stellar-client PRIVATE STELLAR_ENABLE_GLTF=1)
    target_link_libraries(stellar_client_config PUBLIC stellar_import_gltf)
    target_link_libraries(stellar-client PRIVATE stellar_import_gltf)
    target_compile_definitions(stellar_client_asset_validation_smoke PRIVATE STELLAR_ENABLE_GLTF=1)

    add_executable(stellar_import_gltf_regression
        tests/import/gltf/ImporterRegression.cpp
    )

    target_include_directories(stellar_import_gltf_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(stellar_import_gltf_regression PRIVATE
        stellar_import_gltf
        stellar_world
    )

    add_test(NAME gltf_importer_regression COMMAND stellar_import_gltf_regression)

    stellar_add_gltf_world_test(stellar_playable_world_smoke_test playable_world_smoke
        tests/integration/PlayableWorldSmoke.cpp
    )

    stellar_add_scripted_gltf_world_test(stellar_scripted_playable_world_smoke_test
        scripted_playable_world_smoke
        tests/integration/ScriptedPlayableWorldSmoke.cpp
    )

    stellar_add_scripted_gltf_world_test(stellar_scripted_collision_smoke_test
        scripted_collision_smoke
        tests/integration/ScriptedCollisionSmoke.cpp
    )

    stellar_add_scripted_gltf_world_test(stellar_scripted_object_collider_smoke_test
        scripted_object_collider_smoke
        tests/integration/ScriptedObjectColliderSmoke.cpp
    )
else()
    set_tests_properties(client_cli_asset_validation PROPERTIES
        WILL_FAIL TRUE
    )
endif()
