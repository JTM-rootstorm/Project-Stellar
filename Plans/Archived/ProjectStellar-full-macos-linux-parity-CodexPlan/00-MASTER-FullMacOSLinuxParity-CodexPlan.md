---
project: Project Stellar
branch: macos-compat
plan_family: full_macos_linux_parity
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-05
---

# Project Stellar — Full macOS Compatibility and Linux Parity Plan Bundle

## Purpose

Close the remaining gap between the current `macos-compat` branch and a true "macOS is a first-class target" state.

The current branch has macOS build hygiene, Apple-gated Metal enablement, POSIX socket fixes, optional miniaudio playback, and an initial Metal rendering path. It does **not** yet have full parity with Linux/OpenGL rendering because Metal currently covers only the first base-color/base-texture path and does not reproduce OpenGL's full normal/specular/lightmap/material behavior.

## Current branch state to assume

- CMake exposes `STELLAR_ENABLE_OPENGL_BACKEND`, `STELLAR_ENABLE_METAL`, and `STELLAR_MINIAUDIO_NO_RUNTIME_LINKING`.
- Metal is Apple-only and uses Objective-C++ plus Metal/QuartzCore/Foundation framework discovery.
- `GraphicsBackend::kMetal` is compiled only with `STELLAR_ENABLE_METAL_BACKEND`.
- Parser accepts `metal` / `mtl` only in Metal-enabled builds.
- `GraphicsDeviceFactory` can create `MetalGraphicsDevice`.
- Client window flags switch to `SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI` for Metal.
- `MetalGraphicsDevice.mm` creates an SDL Metal view, obtains a CAMetalLayer, creates a MTLDevice/command queue, creates pipelines, uploads static mesh buffers, uploads RGB/RGBA textures as RGBA, stores material records, draws indexed triangles, handles depth, base color, vertex color, base color texture, alpha discard/blend, and basic sampler state.
- Metal does **not** yet implement full OpenGL material parity: lightmaps, normal maps, specular maps, metallic/roughness, occlusion, emissive, texture transforms for all slots, full alpha/culling behavior, camera-dependent specular, or backend-specific projection/depth validation.
- OpenGL on macOS remains experimental/unsupported; full macOS support should be Metal-first, not OpenGL 4.5 on Apple platforms.
- Default CTest must remain display-free and audio-device-free.

## Definition of "full macOS compatibility and Linux parity"

Do not mark this bundle complete until all of these are true:

1. **Build parity**
   - Default Linux build passes.
   - Default macOS build passes.
   - macOS Metal build passes with `STELLAR_ENABLE_METAL=ON`.
   - macOS Metal-only build passes where supported.
2. **CLI/runtime parity**
   - `stellar-client`, `stellar-server`, map validation, server mode, single-player mode, listen-host mode, and remote-client mode behave the same on macOS as Linux, except renderer choice.
   - `--renderer metal` works on macOS Metal builds.
   - Unsupported backend messages remain clear.
3. **Rendering parity**
   - Metal supports the same active material contract that OpenGL supports: base color, vertex color, base color texture, lightmaps / secondary UVs, normal maps using tangents, specular texture/factor/power, metallic/roughness, occlusion, emissive, texture transforms, alpha mask/blend, double-sided/culling behavior, unlit behavior, and camera-dependent specular path.
   - Metal projection/depth/viewport behavior is validated against OpenGL expectations.
4. **Display and HiDPI parity**
   - Metal drawable size uses SDL Metal drawable size, handles resize, and renders correctly on Retina/HiDPI.
5. **Audio parity**
   - Default tests remain no-device.
   - Optional audible smoke works with generated footstep WAVs on macOS.
   - Audio failures remain presentation diagnostics, never gameplay failures.
6. **Tooling parity**
   - Python/bash tooling used by validation works on macOS or documents known optional Linux-only components.
   - TrenchBroom/BSP authoring wrappers either work on macOS or clearly skip external compiler paths when unavailable.
7. **Validation parity**
   - CI or documented local runbooks cover Linux default, macOS default, and macOS Metal.
   - Opt-in display/GPU/audio tests skip cleanly when not available.

## Phase dependency graph

```text
FMP-0 Gap audit and acceptance matrix
  -> FMP-1 Build matrix and CI parity
      -> FMP-2 Backend selection/defaults and OpenGL-on-macOS policy
      -> FMP-3 Metal projection, viewport, drawable, and depth correctness
          -> FMP-4 Metal material/shader parity
              -> FMP-5 Render fixture parity and readback/smoke validation
                  -> FMP-6 Client/server runtime parity on macOS
                  -> FMP-7 Audio parity and runtime smoke
                  -> FMP-8 Tooling/editor/script parity
                      -> FMP-9 Documentation, CI, release gates, and final handoff
```

## Global invariants

- Target branch: `macos-compat`.
- Do not push.
- Do not reintroduce Vulkan.
- Default CTest remains display-free and audio-device-free.
- GPU/display/audio tests must be opt-in and skip with code `77` when unsupported.
- Rendering, HUD, audio, and UI remain presentation-only.
- Server authority, scripting, transport, collision, and BSP import remain backend-neutral.
- OpenGL remains the Linux/default renderer path; Metal is the macOS first-class renderer path.
- Use clear platform gates instead of breaking Linux.
- Public headers need Doxygen `@brief` comments.
- Preserve existing client/server target-boundary rules.

## Standard validation checkpoint

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
```

macOS Metal checkpoint:

```bash
cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON -DSTELLAR_ENABLE_METAL_CONTEXT_TESTS=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure
STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal -R '^metal_context_smoke$' --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```
