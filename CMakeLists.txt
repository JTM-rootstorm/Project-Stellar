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

add_subdirectory(thirdparty/miniaudio)
if(STELLAR_ENABLE_GLTF)
    add_subdirectory(thirdparty/cgltf)
endif()

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
)

target_include_directories(stellar_physics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_physics PUBLIC
    stellar_assets
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
)

add_executable(stellar_render_scene_upload_test
    tests/graphics/RenderSceneUpload.cpp
)

target_include_directories(stellar_render_scene_upload_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_render_scene_upload_test PRIVATE
    stellar_graphics
)

add_test(NAME render_scene_upload COMMAND stellar_render_scene_upload_test)

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

add_executable(stellar_graphics_backend_selection_test
    tests/graphics/BackendSelection.cpp
)

target_include_directories(stellar_graphics_backend_selection_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_graphics_backend_selection_test PRIVATE
    stellar_graphics
)

add_test(NAME graphics_backend_selection COMMAND stellar_graphics_backend_selection_test)

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

add_executable(stellar_animation_runtime_test
    tests/scene/AnimationRuntime.cpp
)

target_include_directories(stellar_animation_runtime_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_animation_runtime_test PRIVATE
    stellar_scene
)

add_test(NAME animation_runtime COMMAND stellar_animation_runtime_test)

add_executable(stellar_collision_world_test
    tests/physics/CollisionWorld.cpp
)

target_include_directories(stellar_collision_world_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_collision_world_test PRIVATE
    stellar_physics
)

add_test(NAME collision_world COMMAND stellar_collision_world_test)

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
    )

    add_test(NAME gltf_importer_regression COMMAND stellar_import_gltf_regression)
else()
    set_tests_properties(client_cli_asset_validation PROPERTIES
        WILL_FAIL TRUE
    )
endif()
