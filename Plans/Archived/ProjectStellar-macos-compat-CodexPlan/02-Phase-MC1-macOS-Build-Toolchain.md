---
phase: MC-1
title: macOS Build and Toolchain Hygiene
depends_on: [MC-0]
can_parallelize: false
---

# MC-1 — macOS Build and Toolchain Hygiene

## Goal

Make the project configure cleanly on macOS without yet claiming Metal renderer support.

## Files likely changed

```text
CMakeLists.txt
thirdparty/miniaudio/CMakeLists.txt
cmake/StellarPlatform.cmake       # optional new helper
CMakePresets.json                 # optional if the repo uses/accepts presets
docs/ImplementationStatus.md
Plans/NEXT.md
```

## Tasks

1. Fix miniaudio property typo:
   ```cmake
   POSITION_INDEPENDENT_CODE ON
   ```
2. Add Apple CMake gates without disrupting Linux:
   ```cmake
   if(APPLE)
       # Use framework links only where required by a target.
   endif()
   ```
3. Add an option:
   ```cmake
   option(STELLAR_ENABLE_METAL "Build the experimental Metal graphics backend on Apple platforms" OFF)
   ```
4. If `STELLAR_ENABLE_METAL` is ON:
   - Require `APPLE`.
   - Enable Objective-C++:
     ```cmake
     enable_language(OBJCXX)
     ```
   - Find/link frameworks later on the Metal target:
     ```cmake
     find_library(METAL_FRAMEWORK Metal REQUIRED)
     find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
     find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
     ```
     Add `MetalKit` only if using `MTKView`, which is probably not needed for the SDL Metal path.
5. Keep `find_package(OpenGL)` and GLEW behavior gated to the OpenGL backend rather than globally required once Metal is introduced. Do not break current Linux builds in this phase.
6. Add a macOS configure note for Homebrew dependencies:
   ```text
   brew install cmake sdl2 glew glm
   ```
   Do not hardcode Homebrew paths unless absolutely necessary.

## Acceptance criteria

- Default configure/build still works on Linux/non-Apple.
- macOS configure reaches compiler checks before renderer-specific failures.
- `STELLAR_ENABLE_METAL=ON` on non-Apple fails clearly and early.
- No `GraphicsBackend::kMetal` is added yet.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build -R '^(target_boundary|graphics_backend_selection|audio_event_router|socket_transport|transport|protocol)' --output-on-failure
tools/dev/check_target_boundaries.sh .
```
