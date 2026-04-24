cmake_minimum_required(VERSION 3.20)

add_library(miniaudio STATIC
    miniaudio.c
    miniaudio.h
)

target_include_directories(miniaudio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(miniaudio PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# Linux requires -ldl and -lpthread and -lm
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(miniaudio PUBLIC -ldl -lpthread -lm)
endif()

set_target_properties(miniaudio PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDIENT_CODE ON
)