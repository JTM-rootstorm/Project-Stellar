---
phase: "VK-5"
title: "Resource Upload And Descriptors"
repo: "JTM-rootstorm/Project-Stellar"
branch: "GL-to-vulkan"
platform_scope: "Linux Vulkan only"
status: "ready"
---

# Phase VK-5 — Resource Upload And Descriptors

## Objective

Implement Vulkan mesh, texture, sampler, and material upload behind the existing `GraphicsDevice` API.

## Existing Contract To Match

`RenderLevel` uploads:

- meshes through `create_mesh`
- images through `create_texture`
- materials through `create_material`
- draw commands through `draw_mesh`

Do not change importer or gameplay contracts.

## Mesh Upload

Implement:

- staging buffer
- device-local vertex buffer
- device-local index buffer
- one `MeshPrimitiveRecord` per primitive
- support only triangle topology initially
- preserve:
  - index count
  - `has_tangents`
  - `has_colors`

Return clear errors for unsupported topology or empty primitive data.

## Texture Upload

Support:

- `ImageFormat::kR8G8B8`
- `ImageFormat::kR8G8B8A8`

For RGB, expand CPU-side to RGBA8 before upload.

Format mapping:

| Engine format/color space | Vulkan format |
|---|---|
| RGB/RGBA linear | `VK_FORMAT_R8G8B8A8_UNORM` |
| RGB/RGBA sRGB | `VK_FORMAT_R8G8B8A8_SRGB` |

Implement:

- staging buffer
- optimal tiled image
- image memory
- image view
- layout transitions
- upload copy
- sampler-compatible final layout

Mipmap policy:

- Start with one mip level.
- Do not require mip generation for first parity.
- Respect sampler min/mag/wrap behavior even if mip filters collapse to linear/nearest.

## Samplers

Cache samplers by:

- min filter
- mag filter
- wrap S
- wrap T

Map existing engine sampler modes to Vulkan equivalents.

## Materials And Descriptors

Store `MaterialUpload` plus descriptor resources.

Suggested descriptor layout:

- set 0: per-draw uniforms
- set 1: material textures/samplers

Texture slots:

| Binding | Slot |
|---:|---|
| 0 | base color |
| 1 | normal |
| 2 | metallic/roughness |
| 3 | occlusion |
| 4 | emissive |
| 5 | lightmap |
| 6 | specular |

Use fallback textures for missing slots:

- white RGBA8
- black RGBA8
- flat normal map if normal slot missing

## Memory Management

For first implementation, hand-roll minimal helpers:

- `find_memory_type`
- `create_buffer`
- `copy_buffer`
- `create_image`
- `transition_image_layout`
- `copy_buffer_to_image`

Do not introduce VMA unless explicitly approved later.

## Validation

```bash
cmake --preset linux-vulkan
cmake --build --preset linux-vulkan --parallel $(nproc)
ctest --test-dir build-linux-vulkan -R '^(render_level_upload|render_level_inspection|graphics_backend_selection)$' --output-on-failure
```

Opt-in display:

```bash
build-linux-vulkan/stellar-client --validate-display --renderer vulkan
```

## Acceptance Criteria

- Vulkan `create_mesh`, `create_texture`, and `create_material` return real handles.
- Existing display-free render upload/inspection tests pass.
- Missing texture/material slots use deterministic fallbacks.
- Destroy methods free Vulkan resources safely.
- No Vulkan resources are exposed outside the backend.
