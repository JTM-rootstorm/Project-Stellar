---
phase: MC-5
title: Metal Backend Scaffold
depends_on: [MC-1, MC-4]
can_parallelize: false
---

# MC-5 — Metal Backend Scaffold

## Goal

Introduce Metal as an explicit Apple-gated backend with a real `GraphicsDevice` class skeleton and a clear unsupported-path behavior on non-Apple platforms.

## Files likely changed

```text
include/stellar/graphics/GraphicsBackend.hpp
src/graphics/GraphicsBackend.cpp
src/graphics/GraphicsDeviceFactory.cpp
include/stellar/graphics/metal/MetalGraphicsDevice.hpp
src/graphics/metal/MetalGraphicsDevice.mm
src/client/Application.cpp
CMakeLists.txt
tests/graphics/BackendSelection.cpp
tests/cmake/GraphicsTests.cmake
tests/graphics/MetalContextSmoke.cpp
docs/ImplementationStatus.md
Plans/NEXT.md
```

## Tasks

1. Add backend enum only with implementation files:
   ```cpp
   enum class GraphicsBackend {
       kOpenGL,
   #if defined(STELLAR_ENABLE_METAL_BACKEND)
       kMetal,
   #endif
   };
   ```
   Prefer a compile definition from CMake over raw `__APPLE__` in public API if Linux builds need stable tests.
2. Parser:
   - Accept `metal` and `mtl` only when Metal is compiled in.
   - Otherwise fail with `Unsupported graphics backend: metal (Metal backend not built)`.
3. Factory:
   - Return `MetalGraphicsDevice` for `kMetal`.
   - Keep OpenGL as default unless on Apple with Metal enabled and project decides to make Metal default.
4. Client window flags:
   ```cpp
   switch (backend) {
   case kMetal: return SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI;
   case kOpenGL: return SDL_WINDOW_OPENGL;
   }
   ```
5. `MetalGraphicsDevice::initialize(Window&)` scaffold:
   - Include `<SDL2/SDL_metal.h>`.
   - Create `SDL_MetalView`.
   - Get `CAMetalLayer`.
   - Create default `MTLDevice`.
   - Associate device with the layer.
   - Create `MTLCommandQueue`.
   - Return clear diagnostics on failure.
6. Destructor:
   - Release Metal objects.
   - Destroy the SDL Metal view before SDL window destruction.
7. Add `metal_context_smoke` opt-in test under `STELLAR_ENABLE_METAL_CONTEXT_TESTS` or reuse a display startup option. It should skip with return code `77` when not Apple or no display/device exists.

## Acceptance criteria

- `--renderer metal --validate-config` parses only when Metal is built.
- `--validate-display --renderer metal` can create a device/layer/command queue locally on macOS.
- Default Linux builds still pass.
- No render-level material parity is required yet; blank clear-only frames are acceptable in this phase.

## Validation

```bash
cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal --target stellar_graphics_backend_selection_test stellar_metal_context_smoke_test -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal -R '^(graphics_backend_selection|metal_context_smoke)$' --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```
