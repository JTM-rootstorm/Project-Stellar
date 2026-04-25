cmake_minimum_required(VERSION 3.20)
project(StellarEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLM REQUIRED)

# Add executable
add_executable(stellar_demo 
    src/main.cpp
    src/graphics/window.cpp
)

# Include directories
target_include_directories(stellar_demo PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    include
)

# Link libraries
target_link_libraries(stellar_demo 
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    GLEW::GLEW
    GLM::GLM
)

# Compiler warnings
if(MSVC)
    target_compile_options(stellar_demo PRIVATE /W4)
else()
    target_compile_options(stellar_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()