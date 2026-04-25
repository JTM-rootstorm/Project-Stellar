cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)

# Use pkg-config for SFML (SFML 3.0 CMake config has compatibility issues)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SFML_WINDOW REQUIRED IMPORTED_TARGET sfml-window)
pkg_check_modules(SFML_GRAPHICS REQUIRED IMPORTED_TARGET sfml-graphics)
pkg_check_modules(SFML_SYSTEM REQUIRED IMPORTED_TARGET sfml-system)
pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

# Thirdparty libraries
add_subdirectory(thirdparty/miniaudio)

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
)

target_include_directories(stellar-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-client PRIVATE
    PkgConfig::SFML_WINDOW
    PkgConfig::SFML_GRAPHICS
    PkgConfig::SFML_SYSTEM
    PkgConfig::GLEW
    PkgConfig::GLM
    OpenGL::GL
    miniaudio
)

set_target_properties(stellar-client PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)