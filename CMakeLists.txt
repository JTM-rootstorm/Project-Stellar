cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.1.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

add_subdirectory(thirdparty/miniaudio)
add_subdirectory(thirdparty/cgltf)

option(STELLAR_ENABLE_GLTF "Enable the glTF importer seam" OFF)

add_library(stellar_scene INTERFACE)
target_include_directories(stellar_scene INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

add_library(stellar_assets INTERFACE)
target_include_directories(stellar_assets INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(stellar_assets INTERFACE stellar_scene)

add_library(stellar_import_gltf
    src/import/gltf/Importer.cpp
    src/import/gltf/Loader.cpp
)

target_include_directories(stellar_import_gltf PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty/cgltf
    ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(stellar_import_gltf PUBLIC
    stellar_assets
    cgltf
)

target_compile_definitions(stellar_import_gltf PRIVATE
    $<$<BOOL:${STELLAR_ENABLE_GLTF}>:STELLAR_ENABLE_GLTF=1>
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

add_library(stellar_graphics
    src/graphics/RendererFactory.cpp
    src/graphics/GraphicsDeviceFactory.cpp
    src/graphics/RenderScene.cpp
    src/graphics/opengl/CubeRenderer.cpp
    src/graphics/opengl/OpenGLGraphicsDevice.cpp
)

target_include_directories(stellar_graphics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(stellar_graphics PUBLIC
    stellar_platform
    ${OPENGL_LIBRARIES}
    PkgConfig::GLEW
    PkgConfig::GLM
)

target_link_libraries(stellar_graphics PUBLIC stellar_assets)

add_executable(stellar-client
    src/client/main.cpp
    src/client/Application.cpp
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-client PRIVATE
    stellar_graphics
    stellar_assets
    stellar_scene
    stellar_import_gltf
    miniaudio
)
