cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# Common compile options
add_compile_options(
    -Wall -Wextra -Wpedantic
    -fno-exceptions
)

# Client executable
add_executable(stellar-client
    src/client/main.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/graphics/GraphicsDevice.cpp
    src/graphics/OpenGLDevice.cpp
    src/graphics/Shader.cpp
    src/graphics/Texture.cpp
    src/graphics/Renderer.cpp
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(stellar-client PRIVATE
    SDL2::SDL2
    OpenGL::GL
)

set_target_properties(stellar-client PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
