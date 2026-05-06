---
phase: MC-7
title: Metal Shader and Material Parity
depends_on: [MC-6]
can_parallelize: false
---

# MC-7 — Metal Shader and Material Parity

## Goal

Reach visual/material parity with the current OpenGL backend for BSP static rendering, lightmaps, normal/specular sidecars, alpha modes, and billboards where practical.

## Files likely changed

```text
src/graphics/metal/MetalGraphicsDevice.mm
src/graphics/metal/StellarShaders.metal       # or embedded MSL source in phase 1
include/stellar/graphics/MaterialUpload.hpp   # only if backend-neutral gaps are found
tests/graphics/RenderSceneUpload.cpp
tests/graphics/RenderSceneInspection.cpp
tests/fixtures/...                            # only if adding Metal-specific smoke fixture
docs/ImplementationStatus.md
```

## Tasks

1. Port OpenGL shader behavior to Metal Shading Language:
   - static vertex format,
   - MVP/world/normal matrices,
   - base color,
   - vertex color,
   - base texture,
   - lightmap,
   - normal map with tangents,
   - metallic/roughness if currently used,
   - occlusion/emissive/specular,
   - alpha mask/blend,
   - unlit fallback.
2. Decide shader delivery:
   - Early: `newLibraryWithSource` from embedded MSL string for easy CMake.
   - Later: compile `.metal` to `.metallib` with CMake custom command and copy beside executable.
3. Implement sampler state mapping:
   - wrap modes,
   - min/mag filter,
   - mip filter where supported.
4. Implement texture slots matching `MaterialUpload`.
5. Implement alpha/blending and culling:
   - double-sided disables culling,
   - alpha blend enables blend state,
   - alpha mask discards in shader.
6. Validate coordinate/projection behavior:
   - GLM matrices are column-major.
   - Metal NDC depth is different from OpenGL if projection math assumes GL conventions; adjust in `make_projection_for_backend()` or shader if necessary.
7. Add debugging env flag output analogous to OpenGL:
   ```text
   STELLAR_DEBUG_RENDER=1
   ```
8. Do not add gameplay dependencies.

## Acceptance criteria

- RenderLevel material upload/inspection tests pass for Metal where display is available.
- BSP fixtures with base textures, lightmaps, normal maps, and specular maps render without device errors.
- OpenGL tests still pass on non-Apple/default.
- No Vulkan references return.

## Validation

```bash
ctest --test-dir build-macos-metal -R '^(graphics_backend_selection|render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|client_map_validation)' --output-on-failure
STELLAR_DEBUG_RENDER=1 build-macos-metal/stellar-client --map <lit_or_material_fixture.bsp> --renderer metal
```
