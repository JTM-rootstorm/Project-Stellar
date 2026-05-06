---
phase: MC-6
title: Metal Resource Upload and Frame Lifecycle
depends_on: [MC-5]
can_parallelize: false
---

# MC-6 — Metal Resource Upload and Frame Lifecycle

## Goal

Implement enough of `MetalGraphicsDevice` to upload meshes/textures/material records and draw basic geometry.

## Files likely changed

```text
src/graphics/metal/MetalGraphicsDevice.mm
include/stellar/graphics/metal/MetalGraphicsDevice.hpp
src/graphics/RenderLevel.cpp       # only if backend-specific projection/depth correction is needed
tests/graphics/RenderSceneUpload.cpp
tests/graphics/MetalContextSmoke.cpp
```

## Tasks

1. Implement records mirroring OpenGL backend concepts:
   ```cpp
   struct MeshRecord { id<MTLBuffer> vertex_buffer; id<MTLBuffer> index_buffer; ... };
   struct TextureRecord { id<MTLTexture> texture; ... };
   struct MaterialRecord { MaterialUpload upload; };
   ```
2. Use `newBufferWithBytes:length:options:` for static mesh data initially.
3. Create textures for `kR8G8B8` and `kR8G8B8A8` images:
   - Map sRGB/linear color-space metadata.
   - Handle row bytes explicitly.
   - Generate mipmaps later if not needed immediately.
4. Set up render pass:
   - Acquire drawable as late as possible.
   - Configure color clear/store.
   - Add a depth texture sized to drawable/window.
   - Use less-depth compare matching OpenGL behavior.
5. Implement `begin_frame`, `draw_mesh`, `end_frame`:
   - `begin_frame`: create command buffer and render command encoder.
   - `draw_mesh`: bind pipeline/buffers and issue indexed draws.
   - `end_frame`: end encoding, present drawable, commit.
6. For the first pass, draw with fallback color/material only. Texture/material parity comes in MC-7.
7. Handle resize/drawable size changes.
8. Return/print diagnostics when drawable is unavailable instead of crashing.

## Acceptance criteria

- Metal display validation clears/presents.
- A simple render-level upload test can instantiate/upload through the Metal backend in an opt-in context.
- No default tests require a Metal device.
- Resource destruction is deterministic and leak-safe enough for repeated smoke tests.

## Validation

```bash
ctest --test-dir build-macos-metal -R '^(metal_context_smoke|render_level_upload|render_level_inspection)$' --output-on-failure
build-macos-metal/stellar-client --validate-display --renderer metal
```
