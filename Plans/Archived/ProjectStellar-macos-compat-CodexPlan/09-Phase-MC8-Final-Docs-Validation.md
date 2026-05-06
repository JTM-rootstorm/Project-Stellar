---
phase: MC-8
title: Documentation, Packaging, CI Notes, and Final Handoff
depends_on: [MC-2, MC-3, MC-7]
can_parallelize: false
---

# MC-8 — Documentation, Packaging, CI Notes, and Final Handoff

## Goal

Make the new macOS/Metal state clear to future agents and users without overstating unsupported pieces.

## Files likely changed

```text
README.md
docs/ImplementationStatus.md
docs/Design.md
Plans/NEXT.md
Plans/macos_compat_metal_plan/00-MASTER-MacOSCompatMetal-CodexPlan.md
tests/CMakeLists.txt
tests/cmake/GraphicsTests.cmake
```

## Tasks

1. Update docs with exact supported states:
   - macOS build supported.
   - Metal backend supported or experimental depending on actual validation.
   - OpenGL on macOS is deprecated/transitional/unsupported depending on MC-4 outcome.
   - Default validation remains display-free.
   - Opt-in Metal smoke requires macOS + display/GPU.
2. Add runbook:
   ```bash
   brew install cmake sdl2 glew glm
   cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-macos -j$(sysctl -n hw.ncpu)
   ctest --test-dir build-macos --output-on-failure
   ```
   Metal:
   ```bash
   cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
   cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
   build-macos-metal/stellar-client --validate-display --renderer metal
   ```
3. Add or document CI strategy:
   - Linux default CI unchanged.
   - macOS CI may run configure/build/default CTest.
   - Metal display smoke likely remains manual/local unless a display-capable runner is configured.
4. Archive/commit this plan bundle in `Plans/macos_compat_metal_plan/` only if the user wants it in the repo.
5. Final audits:
   ```bash
   git grep -n -i 'vulkan\|SDL_WINDOW_VULKAN\|Vulkan::Vulkan' -- include src tests CMakeLists.txt docs Plans/NEXT.md ':!Plans/Archived/**'
   git grep -n 'MSG_NOSIGNAL' -- src include tests
   git grep -n 'POSITION_INDEPENDIENT_CODE' -- .
   git grep -n 'renderer metal\|GraphicsBackend::kMetal\|SDL_WINDOW_METAL' -- include src tests docs
   tools/dev/check_target_boundaries.sh .
   ```

## Acceptance criteria

- Docs, status, and tests reflect actual support.
- Mac/Metal instructions are directly runnable.
- No stale claims remain from OpenGL-only or Vulkan-removal era.
- Final default CTest passes.
- Opt-in Metal smoke passes locally or is documented as skipped with a reason.

## Final validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
```

macOS Metal local:

```bash
cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```
