---
phase: FMP-9
title: Documentation, CI Gates, and Final Handoff
depends_on: [FMP-5, FMP-6, FMP-7, FMP-8]
can_parallelize: false
---

# FMP-9 — Documentation, CI Gates, and Final Handoff

## Goal

Only now mark full macOS compatibility / Linux parity complete.

## Tasks

1. Update status:
   - `macOS compatibility: complete`.
   - `Metal renderer parity: complete`.
   - exact OpenGL-on-macOS support state.
   - Linux OpenGL remains supported.
2. Include final runbooks for Linux default, macOS default, macOS Metal, macOS Metal-only if supported, and optional display/audio smoke.
3. Archive or retain the phase bundle under `Plans/full_macos_linux_parity_plan/`.
4. Run final audits:
   ```bash
   git grep -n 'TODO\|FIXME' -- include src tests docs Plans/NEXT.md ':!Plans/Archived/**'
   git grep -n -i 'vulkan\|SDL_WINDOW_VULKAN\|Vulkan::Vulkan' -- include src tests CMakeLists.txt docs Plans/NEXT.md ':!Plans/Archived/**'
   git grep -n 'MSG_NOSIGNAL' -- src include tests
   git grep -n 'POSITION_INDEPENDIENT_CODE' -- .
   git grep -n 'normal/specular/lightmap parity remains follow-up\|experimental Metal' -- docs Plans/NEXT.md README.md
   ```
5. Final acceptance matrix must show `PASS` or `SKIP_EXPECTED` for every required row.

## Acceptance criteria

- Full parity definition is satisfied.
- Docs no longer underclaim or overclaim.
- CI/local runbooks are complete.
- Known optional limitations are explicit.
- Target boundaries pass.

## Final validation

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j$(nproc)
ctest --test-dir build-linux --output-on-failure
tools/dev/check_target_boundaries.sh .

cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Debug
cmake --build build-macos -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos --output-on-failure

cmake -S . -B build-macos-metal -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_METAL=ON -DSTELLAR_ENABLE_METAL_CONTEXT_TESTS=ON
cmake --build build-macos-metal -j$(sysctl -n hw.ncpu)
ctest --test-dir build-macos-metal --output-on-failure

STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal -R '^metal_context_smoke$' --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
STELLAR_ENABLE_AUDIO=1 build-macos-metal/stellar-client --map <walking_fixture> --renderer metal
```
