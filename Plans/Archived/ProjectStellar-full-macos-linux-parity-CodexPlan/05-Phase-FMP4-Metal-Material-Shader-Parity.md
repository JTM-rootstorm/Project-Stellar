---
phase: FMP-4
title: Metal Material and Shader Parity
depends_on: [FMP-3]
can_parallelize: false
---

# FMP-4 — Metal Material and Shader Parity

## Goal

Bring Metal material rendering to parity with the current OpenGL shader/material contract.

## Required parity features

Implement or deliberately test every current OpenGL material behavior:

1. Base color factor.
2. Vertex color.
3. Base color texture.
4. Lightmap texture on secondary UVs.
5. Normal texture with tangent/bitangent basis.
6. Specular texture.
7. Specular factor and power.
8. Metallic/roughness texture and factors.
9. Occlusion texture and strength.
10. Emissive texture and factor.
11. Texture transforms for all relevant slots.
12. Texcoord set selection.
13. Alpha opaque/mask/blend.
14. Double-sided material culling.
15. Unlit materials.
16. Camera-world-position-dependent specular.
17. Global/key light constants matching OpenGL path.

## Tasks

1. Move embedded MSL to `src/graphics/metal/StellarShaders.metal` if feasible, or keep embedded MSL until CMake is stable.
2. Define a Metal uniform struct matching all OpenGL material uniforms.
3. Create a stable texture binding table for base, normal, metallic/roughness, occlusion, emissive, lightmap, and specular.
4. Preserve sRGB/linear pixel formats.
5. Implement sampler cache instead of per-draw sampler creation if straightforward.
6. Implement double-sided culling and alpha blend/mask.
7. Implement normal/specular/lightmap math equivalent to OpenGL.
8. Add debug output listing material slots bound for the first few frames.

## Acceptance criteria

- Metal shader consumes every material slot OpenGL consumes.
- Render tests prove material upload records are not ignored.
- No gameplay/authority/server code changes are required.
- OpenGL path remains unchanged except shared backend-neutral helpers if needed.

## Validation

```bash
ctest --test-dir build-macos-metal -R '^(render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps)' --output-on-failure
STELLAR_DEBUG_RENDER=1 build-macos-metal/stellar-client --map <material_sidecar_fixture.bsp> --renderer metal
STELLAR_DEBUG_RENDER=1 build-macos-metal/stellar-client --map <lit_fixture.bsp> --renderer metal
```
