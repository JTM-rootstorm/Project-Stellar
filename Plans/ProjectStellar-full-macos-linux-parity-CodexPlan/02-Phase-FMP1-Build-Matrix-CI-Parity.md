---
phase: FMP-1
title: Build Matrix and CI Parity
depends_on: [FMP-0]
can_parallelize: false
---

# FMP-1 — Build Matrix and CI Parity

## Goal

Make the build graph prove platform parity before deeper rendering work.

## Tasks

1. Define supported build variants:
   - `linux-default`: OpenGL ON, Metal OFF.
   - `macos-default`: OpenGL ON, Metal OFF.
   - `macos-metal`: OpenGL ON, Metal ON.
   - `macos-metal-only`: OpenGL OFF, Metal ON.
2. Ensure `macos-metal-only` does not require OpenGL/GLEW.
3. Ensure `stellar_graphics` compiles with OpenGL only, Metal only, and OpenGL + Metal.
4. Add CMake presets or CI scripts for each matrix entry.
5. Verify no server/authority/protocol target links graphics/audio/miniaudio.
6. Keep `STELLAR_ENABLE_METAL=ON` fatal on non-Apple.

## Acceptance criteria

- Linux default build still passes.
- macOS default build passes.
- macOS Metal build passes.
- macOS Metal-only build passes or is explicitly marked unsupported with a reason.
- Build matrix commands are documented and runnable by Codex/CI.

## Validation

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j$(nproc)
ctest --test-dir build-linux --output-on-failure

cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Debug
cmake --build build-macos -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos --output-on-failure

cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure

cmake -S . -B build-macos-metal-only -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON -DSTELLAR_ENABLE_OPENGL_BACKEND=OFF
cmake --build build-macos-metal-only -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal-only --output-on-failure
```
