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

# Add executable
add_executable(stellar-client
    src/client/main.cpp
    src/client/Application.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/graphics/opengl/CubeRenderer.cpp
)

# Include directories
target_include_directories(stellar-client PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(stellar-client 
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    PkgConfig::GLEW
    PkgConfig::GLM
    miniaudio
)
