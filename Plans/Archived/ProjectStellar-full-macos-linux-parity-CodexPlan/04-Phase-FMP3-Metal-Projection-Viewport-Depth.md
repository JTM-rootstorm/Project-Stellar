---
phase: FMP-3
title: Metal Projection, Viewport, Drawable, and Depth Correctness
depends_on: [FMP-2]
can_parallelize: false
---

# FMP-3 — Metal Projection, Viewport, Drawable, and Depth Correctness

## Goal

Make Metal draw the same world geometry in the same screen-space orientation/depth behavior as Linux/OpenGL.

## Tasks

1. Implement backend-aware projection:
   - Use a Metal correction matrix or clearly documented GLM convention.
   - Validate near/far depth and handedness.
2. Add Metal viewport setup with drawable dimensions.
3. Use SDL Metal drawable size, not only window logical size.
4. Recreate depth target on drawable-size changes.
5. Acquire drawable late and release drawable/command buffer after commit.
6. Add `STELLAR_DEBUG_RENDER=1` logs for backend, drawable size, depth texture size, and projection convention.
7. Add a small opt-in Metal projection smoke test if practical.

## Acceptance criteria

- `--validate-display --renderer metal` succeeds on macOS.
- `STELLAR_DEBUG_RENDER=1` prints drawable/depth/projection diagnostics.
- Basic known geometry renders with correct orientation/depth.
- No Linux/OpenGL regression.

## Validation

```bash
STELLAR_DEBUG_RENDER=1 build-macos-metal/stellar-client --validate-display --renderer metal
STELLAR_RUN_METAL_CONTEXT_TESTS=1 ctest --test-dir build-macos-metal -R '^metal_context_smoke$' --output-on-failure
ctest --test-dir build -R '^(render_level_inspection|render_level_upload|graphics_backend_selection)$' --output-on-failure
```
