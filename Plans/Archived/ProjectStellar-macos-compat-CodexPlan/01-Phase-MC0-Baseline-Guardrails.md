---
phase: MC-0
title: Baseline, Guardrails, and Branch Truth
depends_on: []
can_parallelize: false
---

# MC-0 — Baseline, Guardrails, and Branch Truth

## Goal

Establish the exact `macos-compat` baseline before touching build/platform/backend code.

## Files to inspect

```text
CMakeLists.txt
thirdparty/miniaudio/CMakeLists.txt
include/stellar/graphics/GraphicsBackend.hpp
src/graphics/GraphicsBackend.cpp
src/graphics/GraphicsDeviceFactory.cpp
src/graphics/opengl/OpenGLGraphicsDevice.cpp
src/client/Application.cpp
src/network/SocketTransport.cpp
tests/CMakeLists.txt
tests/cmake/GraphicsTests.cmake
tests/graphics/BackendSelection.cpp
Plans/NEXT.md
docs/ImplementationStatus.md
```

## Tasks

1. Confirm current branch:
   ```bash
   git status --short
   git branch --show-current
   ```
2. Confirm no active Metal files exist:
   ```bash
   git ls-files | grep -Ei '(^|/)metal|Metal|\.metal$' || true
   git grep -n -i 'SDL_WINDOW_METAL\|SDL_Metal\|CAMetalLayer\|MTLDevice\|MetalKit' -- include src tests CMakeLists.txt || true
   ```
3. Confirm OpenGL-only backend state:
   ```bash
   git grep -n 'GraphicsBackend::kOpenGL\|SDL_WINDOW_OPENGL\|OpenGLGraphicsDevice' -- include src tests CMakeLists.txt
   ```
4. Confirm no Vulkan regression:
   ```bash
   git grep -n -i -e 'vulkan' -e 'VK_' -e 'SDL_WINDOW_VULKAN' -- include src tests CMakeLists.txt docs Plans/NEXT.md ':!Plans/Archived/**' || true
   ```
5. Record baseline build/test result on the current platform.

## Acceptance criteria

- A short baseline note is added to the active macOS plan/status doc.
- No source behavior changes except documentation/status if desired.
- Known blockers are recorded:
  - OpenGL 4.5 request is incompatible with macOS as a support target.
  - Metal support is absent.
  - miniaudio CMake typo exists.
  - macOS socket send policy needs a fix.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
```
