---
project: Project Stellar
branch: macos-compat
plan_family: macos_compat_metal
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-05
---

# Project Stellar — macOS Compatibility and Metal Backend Plan Bundle

## Intake summary

Target branch: `macos-compat`.

Goal: make Project Stellar build and run cleanly on macOS, then add an explicit Metal backend through the existing backend-neutral graphics abstraction.

The smallest correct architecture is:

```text
macOS compile hygiene + POSIX socket portability
  -> optional/minimal presentation-only miniaudio runtime sink
  -> keep OpenGL as current non-Mac/default fallback where supported
  -> add Metal as an Apple-gated backend target
  -> route backend selection through GraphicsBackend + GraphicsDeviceFactory
  -> validate display-free by default, with opt-in display/GPU/audio smoke tests
```

Do not advertise Metal support by only adding enum values or CLI aliases. Metal becomes supported only after a real `GraphicsDevice` implementation exists and passes an opt-in display smoke test.

## Current branch findings

### Build graph

- Root `CMakeLists.txt` still declares `LANGUAGES C CXX`, finds SDL2/OpenGL, requires pkg-config GLEW/GLM, directly includes `src/graphics/opengl/OpenGLGraphicsDevice.cpp` in `stellar_graphics`, and links `stellar-client` directly to `miniaudio`.
- No Apple-specific CMake branch, no Objective-C++ language enablement, no Metal/QuartzCore/MetalKit framework links, no Metal source files, and no macOS CI preset are present.
- `thirdparty/miniaudio/CMakeLists.txt` has a typo: `POSITION_INDEPENDIENT_CODE`; this should be `POSITION_INDEPENDENT_CODE`.
- `thirdparty/miniaudio/CMakeLists.txt` only handles Linux extra links. It does not express macOS command-line `pthread`/`m` links or the optional notarization-safe `MA_NO_RUNTIME_LINKING` framework path.

### Platform and networking

- `SocketTransport.cpp` includes `__APPLE__` in the POSIX socket path, but `send()` uses `MSG_NOSIGNAL`. macOS does not define Linux `MSG_NOSIGNAL`; use a guarded send flag and set `SO_NOSIGPIPE` on Apple sockets, or otherwise define a reliable SIGPIPE policy.
- `Window` is a minimal SDL wrapper exposing `SDL_Window*`. This is enough for a Metal backend to call SDL Metal APIs from the graphics device, but it currently has no backend-specific window capability helpers.

### Audio

- `AudioEventRouter` and tests are display/audio-device-free.
- The client still uses `NoOpAudioRequestSink`, so footstep/gameplay audio routes to stable sound IDs but does not play audibly at runtime.
- Keep default validation no-device and no-display. Add real playback behind a presentation-only sink and optional tests.

### Graphics

- `GraphicsBackend` only has `kOpenGL`; parser accepts `opengl`, `gl`, and `OpenGL`; all other backend strings fail.
- `GraphicsDeviceFactory` always creates `OpenGLGraphicsDevice`.
- `Application.cpp` always returns `SDL_WINDOW_OPENGL` from `backend_window_flags()`.
- `OpenGLGraphicsDevice` requests an OpenGL 4.5 core context and contains a vertex shader using `#version 430 core`. macOS OpenGL is deprecated and Apple-facing APIs expose OpenGL 4.1 Core at most, so the current OpenGL path is not a reliable macOS renderer target.
- Existing tests include `graphics_backend_selection`, `render_level_upload`, `render_level_inspection`, and optional OpenGL context/display startup tests. There is no Metal context/smoke test.

## External references to preserve in implementation notes

- Apple documents OpenGL/OpenCL as deprecated in macOS 10.14 and recommends transitioning graphics-intensive apps to Metal.
- Apple `MTKView` documentation describes Metal view/device/drawable/render-pass behavior. It may be used as an architectural reference, but this project already uses SDL windows; direct SDL Metal view/layer integration is likely the smaller step.
- SDL2 exposes `SDL_WINDOW_METAL`, `SDL_Metal_CreateView`, `SDL_Metal_GetLayer`, and `SDL_Metal_DestroyView`. `SDL_Metal_CreateView` creates a CAMetalLayer-backed view; on macOS, engine code must associate an `MTLDevice` with the layer.
- miniaudio documents macOS Core Audio support, command-line `-lpthread -lm`, and a notarization-safe `MA_NO_RUNTIME_LINKING` path that links `CoreFoundation`, `CoreAudio`, and `AudioToolbox`.

## Phase dependency graph

```text
MC-0 Baseline and guardrails
  -> MC-1 macOS build/toolchain hygiene
      -> MC-2 POSIX socket portability
      -> MC-3 miniaudio CMake + optional playback sink
      -> MC-4 OpenGL/macOS fallback decision and diagnostics
          -> MC-5 Metal backend scaffold
              -> MC-6 Metal resource upload + frame lifecycle
                  -> MC-7 Metal shader/material parity
                      -> MC-8 docs, validation, and final handoff
```

## Parallelization map

Safe parallel after MC-1:
- MC-2 networking portability can run independently.
- MC-3 audio sink can run independently if it only touches `stellar_audio`/client presentation and CMake.
- MC-4 diagnostics/tests can run while Metal scaffolding design is drafted.

Do not parallelize:
- MC-5 backend enum/factory/window flags with MC-6/MC-7 implementation unless the scaffold API is already landed.
- Metal shader/material parity with resource upload unless texture, sampler, and material binding structs are fixed.
- Runtime audio integration with authority/server changes. Audio remains presentation-only.

## Global invariants

- Target branch is `macos-compat`.
- Do not push. This plan is for local Codex implementation.
- Default tests must remain display-free and audio-device-free.
- Server authority, scripting, transport, and collision remain backend-neutral.
- Rendering, HUD, audio, and UI are presentation only and never gameplay truth.
- Do not reintroduce Vulkan.
- Do not make `.stellar_material` sidecars gameplay truth.
- Do not add `GraphicsBackend::kMetal`, `--renderer metal`, or docs claiming Metal support until a real Metal graphics device exists.
- Keep OpenGL supported only where it actually builds/runs; for macOS, prefer Metal as the supported renderer direction.
- Add public `@brief` comments for public headers.
- Use platform gates instead of breaking Linux/default builds.
- Default CTest should pass without a GPU/display/audio device.
- Opt-in display/audio tests should skip cleanly with return code `77` when unsupported.

## Standard local checkpoint

At the end of each phase:

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
git status --short
```

For macOS display/Metal phases, also run:

```bash
cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```

Use the Metal smoke only on a local macOS machine with a display/GPU.
