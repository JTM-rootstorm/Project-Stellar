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

find_package(Threads REQUIRED)

if(UNIX)
    target_link_libraries(miniaudio PUBLIC Threads::Threads m)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(miniaudio PUBLIC dl)
endif()

if(APPLE AND STELLAR_MINIAUDIO_NO_RUNTIME_LINKING)
    target_compile_definitions(miniaudio PUBLIC MA_NO_RUNTIME_LINKING)
    target_link_libraries(miniaudio PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

set_target_properties(miniaudio PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
