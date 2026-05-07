---
phase: "VK-1"
title: "Linux Vulkan Build, Dependency Gates, And Presets"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only; macOS Metal preserved"
status: "ready"
---

# Phase VK-1 — Linux Vulkan Build, Dependency Gates, And Presets

## Objective

Add Vulkan to the build graph only for Linux/non-Apple platforms. Keep macOS Vulkan-free and Metal-preserving.

## Design Rules

- `find_package(Vulkan)` must not run on Apple platforms.
- Vulkan source files must not compile on Apple platforms.
- No MoltenVK references.
- No macOS Vulkan presets.
- Metal CMake logic remains intact.
- OpenGL remains available during migration unless this phase explicitly creates a Vulkan-only Linux preset.

## CMake Changes

In `CMakeLists.txt`, add:

```cmake
option(STELLAR_ENABLE_VULKAN_BACKEND "Build the Vulkan graphics backend on Linux" ON)
option(STELLAR_ENABLE_VULKAN_VALIDATION "Enable Vulkan validation layer diagnostics in dev builds" ON)
```

Then gate Vulkan:

```cmake
if(STELLAR_ENABLE_VULKAN_BACKEND AND APPLE)
    message(FATAL_ERROR "STELLAR_ENABLE_VULKAN_BACKEND is Linux-only in Project Stellar; use Metal on macOS")
endif()

if(STELLAR_ENABLE_VULKAN_BACKEND)
    find_package(Vulkan REQUIRED)
endif()
```

If the project later wants BSD/Windows, add a separate plan. For this phase, assume Linux/non-Apple Unix only.

## Source List Changes

Add a Vulkan source block:

```cmake
if(STELLAR_ENABLE_VULKAN_BACKEND)
    list(APPEND STELLAR_GRAPHICS_SOURCES
        src/graphics/vulkan/VulkanGraphicsDevice.cpp
    )
endif()
```

Add definitions and links only in the Vulkan block:

```cmake
if(STELLAR_ENABLE_VULKAN_BACKEND)
    target_compile_definitions(stellar_graphics PUBLIC STELLAR_ENABLE_VULKAN_BACKEND=1)
    target_link_libraries(stellar_graphics PUBLIC Vulkan::Vulkan)
endif()
```

## Presets

Add Linux presets only:

```json
{
  "name": "linux-vulkan",
  "displayName": "Linux Vulkan build",
  "description": "Linux build with Vulkan enabled and OpenGL temporarily enabled during migration.",
  "generator": "Unix Makefiles",
  "binaryDir": "${sourceDir}/build-linux-vulkan",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "STELLAR_ENABLE_VULKAN_BACKEND": "ON",
    "STELLAR_ENABLE_OPENGL_BACKEND": "ON",
    "STELLAR_ENABLE_METAL": "OFF"
  }
}
```

```json
{
  "name": "linux-vulkan-only",
  "displayName": "Linux Vulkan-only build",
  "description": "Linux build with Vulkan enabled and OpenGL/GLEW disabled.",
  "generator": "Unix Makefiles",
  "binaryDir": "${sourceDir}/build-linux-vulkan-only",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "STELLAR_ENABLE_VULKAN_BACKEND": "ON",
    "STELLAR_ENABLE_OPENGL_BACKEND": "OFF",
    "STELLAR_ENABLE_METAL": "OFF"
  }
}
```

Do not add:

- `macos-vulkan`
- `macos-vulkan-only`
- MoltenVK presets

## Tests CMake

Add an opt-in test option:

```cmake
option(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS
       "Build optional Linux Vulkan context/readback smoke tests" OFF)
```

Gate it:

```cmake
if(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS AND APPLE)
    message(FATAL_ERROR "Vulkan context tests are Linux-only; use Metal tests on macOS")
endif()
```

## Validation

Linux:

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
ctest --preset linux-vulkan --output-on-failure

cmake --preset linux-vulkan-only
cmake --build --preset linux-vulkan-only --parallel $(nproc)
```

macOS guard:

```bash
cmake --preset macos-metal
cmake --build --preset macos-metal --parallel $(sysctl -n hw.ncpu)
ctest --preset macos-metal --output-on-failure
```

Negative Apple check if available:

```bash
cmake -S . -B build-apple-vulkan-negative -DSTELLAR_ENABLE_VULKAN_BACKEND=ON
```

Expected on Apple: clear fatal error saying Vulkan is Linux-only and Metal should be used.

## Acceptance Criteria

- Linux Vulkan configure reaches Vulkan discovery.
- Linux Vulkan-only configure does not require OpenGL/GLEW.
- macOS Metal configure does not require Vulkan.
- No MoltenVK references exist.
- No server target links Vulkan or graphics.
- Target boundary check still passes.
