---
repo: "JTM-rootstorm/Project-Stellar"
branch: "kill-vulkan"
created_for: "Codex implementation handoff"
created_on: "2026-05-04"
status: "ready-for-implementation"
---

# KV-1 — Remove Vulkan Build and Dependency Requirements

## Goal

Make the project configure and build without Vulkan development packages. This phase removes the hard build dependency first so downstream cleanup can be validated on OpenGL-only systems.

## Files To Edit

Primary:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/cmake/GraphicsTests.cmake`

Possibly:

- CI/build scripts, if active grep finds Vulkan-specific configure flags outside archived plans.
- Package/dependency docs, but broad docs updates are mainly KV-4.

## Tasks

### 1. Remove mandatory Vulkan discovery

In `CMakeLists.txt`, delete:

```cmake
find_package(Vulkan REQUIRED)
```

Do not replace it with an option. The goal is not optional Vulkan; the goal is no active Vulkan support.

### 2. Remove Vulkan source files from `stellar_graphics`

In the `add_library(stellar_graphics ...)` source list, delete all active Vulkan implementation files:

```cmake
src/graphics/vulkan/VulkanGraphicsDevice.cpp
src/graphics/vulkan/VulkanGraphicsDeviceCommon.cpp
src/graphics/vulkan/VulkanGraphicsDeviceDevice.cpp
src/graphics/vulkan/VulkanGraphicsDeviceSwapchain.cpp
src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp
src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp
src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp
```

Do not delete the files in this phase unless KV-3 is being performed in the same patch. This phase is about unhooking CMake first.

### 3. Remove Vulkan include directories and link libraries

Delete from `target_include_directories(stellar_graphics PUBLIC ...)`:

```cmake
${Vulkan_INCLUDE_DIRS}
```

Delete from `target_link_libraries(stellar_graphics PUBLIC ...)`:

```cmake
Vulkan::Vulkan
```

Keep OpenGL, GLEW, GLM, SDL, platform, and assets links unchanged.

### 4. Remove Vulkan context test option and test target

In `tests/CMakeLists.txt`, delete:

```cmake
option(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS
       "Build optional Vulkan context/init smoke tests" OFF)
```

In `tests/cmake/GraphicsTests.cmake`, delete the full block:

```cmake
if(STELLAR_ENABLE_VULKAN_CONTEXT_TESTS)
    add_executable(stellar_vulkan_context_smoke_test
        ${STELLAR_TEST_SOURCE_DIR}/graphics/VulkanContextSmoke.cpp
    )
    ...
    add_test(NAME vulkan_context_smoke COMMAND $<TARGET_FILE:stellar_vulkan_context_smoke_test>)
endif()
```

Keep OpenGL optional context tests unchanged.

### 5. Check no CMake references remain

Run:

```bash
git grep -n -i \
  -e 'Vulkan' \
  -e 'STELLAR_ENABLE_VULKAN' \
  -e 'Vulkan::Vulkan' \
  -e 'Vulkan_INCLUDE_DIRS' \
  -- CMakeLists.txt tests/CMakeLists.txt tests/cmake ':!Plans/Archived/**'
```

Expected after this phase:

- No active CMake/test-config matches.
- Archived plans may still match if included accidentally; exclude them.

## Validation

Run:

```bash
cmake -S . -B build-kv1 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kv1 -j$(nproc)
```

If build fails because code still references `VulkanGraphicsDevice` through the factory, proceed to KV-2 in the same working tree. That is expected if phases are applied strictly one at a time.

If configure still requires Vulkan, KV-1 is incomplete.

## Acceptance Criteria

- No active CMake path calls `find_package(Vulkan REQUIRED)`.
- `stellar_graphics` no longer compiles or links Vulkan backend files.
- `stellar_graphics` no longer exposes Vulkan include directories or links `Vulkan::Vulkan`.
- Vulkan context smoke test target and CMake option are removed.
- Default OpenGL context test option remains available and opt-in.
