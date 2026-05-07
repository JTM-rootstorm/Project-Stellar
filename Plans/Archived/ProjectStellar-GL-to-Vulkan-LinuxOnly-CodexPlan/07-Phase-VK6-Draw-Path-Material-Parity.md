---
phase: "VK-6"
title: "Draw Path And Material Parity"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only"
status: "ready"
---

# Phase VK-6 — Draw Path And Material Parity

## Objective

Render static BSP level geometry in Vulkan with parity against the current active material contract.

## Required Material Contract

The Vulkan shader/draw path must support:

- base color factor
- vertex color
- base color texture
- lightmap texture with secondary UVs
- normal map when tangents exist
- specular texture/factor/power
- metallic factor
- roughness factor
- metallic/roughness texture
- occlusion texture/strength
- emissive texture/factor
- texture transform offset/rotation/scale
- texcoord set selection
- alpha opaque/mask/blend
- double-sided culling
- unlit materials
- global/key light constants
- camera-dependent specular

Do not expand into full PBR.

## Draw Flow

For each frame:

1. Acquire swapchain image.
2. Begin command buffer.
3. Begin render pass/dynamic rendering.
4. Bind pipeline.
5. Bind per-draw uniforms.
6. Bind material descriptors.
7. Bind vertex/index buffers.
8. Draw indexed primitives.
9. End render pass.
10. Submit/present.

## Pipeline Strategy

Minimum set:

- opaque/mask pipeline with depth write enabled
- alpha-blend pipeline with blending enabled

Culling:

- default: back-face culling
- double-sided: culling disabled

If Vulkan pipeline permutations become too large, cache by small `PipelineKey`:

```cpp
struct PipelineKey {
    bool alpha_blend = false;
    bool double_sided = false;
};
```

## Projection

Add Vulkan to backend projection convention in `LevelRenderer`.

Vulkan uses zero-to-one clip depth, matching the existing Metal-style convention more than OpenGL.

Suggested convention string:

```text
vulkan_ndc_z_zero_to_one
```

Do not use OpenGL's minus-one-to-one depth convention for Vulkan.

## Debug Logging

Under `STELLAR_DEBUG_RENDER=1`, log limited first-frame facts:

- backend=vulkan
- swapchain format
- depth format
- drawable extent
- projection convention
- material slot presence for first material
- primitive count
- descriptor pool/set allocation failures

## Validation

Display-free:

```bash
ctest --test-dir build-linux-vulkan -R \
'^(render_level_upload|render_level_inspection|bsp_materials|bsp_lightmaps|graphics_backend_selection)$' \
--output-on-failure
```

Opt-in display:

```bash
STELLAR_DEBUG_RENDER=1 \
build-linux-vulkan/stellar-client --validate-display --renderer vulkan
```

Fixture smoke:

```bash
STELLAR_DEBUG_RENDER=1 \
build-linux-vulkan/stellar-client \
  --validate-display \
  --renderer vulkan \
  --map tests/fixtures/trenchbroom/out/lit_zup_room.bsp
```

## Acceptance Criteria

- Vulkan renders static level geometry.
- Lightmaps appear when present.
- Normal/specular sidecar material fixture paths produce expected slot coverage.
- Alpha mask/blend/double-sided behavior matches existing tests.
- Metal behavior remains unchanged on macOS.
