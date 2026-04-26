# Next Implementation Plan

## Goal
Finish the first practical glTF importer pass by hardening the importer path, closing the remaining texture gap, and adding minimal validation so the engine can load a real static `.gltf` scene cleanly.

## Current State
The engine now has the major architectural seams in place:

- backend-neutral `GraphicsDevice` and `Renderer` interfaces exist
- `RenderScene` uploads CPU-side `SceneAsset` data and renders through backend handles
- the spinning cube prototype still runs through the scene runtime
- CPU-side asset and scene types exist for meshes, materials, images, nodes, and scenes
- cgltf is vendored and wired into `stellar_import_gltf`
- the importer currently parses `.gltf` / `.glb`, loads external buffers, and maps geometry/material/scene data into `SceneAsset`

## What Still Needs To Be Done

### 1. Add embedded image support to the importer
The current importer only supports external image URIs.

Required work:
- support cgltf images backed by `buffer_view`
- support data-URI images if they appear in the source asset
- decode embedded image bytes into `ImageAsset` using `stb_image`
- keep the external-image path working

Definition of done:
- importer can load a glTF scene with embedded texture data without failing

### 2. Clean up importer correctness and ownership
The importer should be tightened so it is easier to maintain and less brittle.

Required work:
- audit cgltf index translation and scene mapping for correctness
- remove any remaining ad hoc helpers that are no longer needed
- keep importer ownership strictly CPU-side
- preserve clear error messages for unsupported glTF features

Definition of done:
- importer code is straightforward, validated, and does not depend on renderer state

### 3. Add a small importer fixture/test pass
The importer needs a minimal regression test so future refactors do not break static scene loading.

Required work:
- add one tiny `.gltf` test asset or a generated fixture
- verify mesh, material, node, and scene counts are mapped correctly
- verify external image loading still works

Definition of done:
- at least one importer test proves a static scene can be loaded into `SceneAsset`

## Recommended Implementation Order
1. Implement embedded image loading in `stellar_import_gltf`
2. Clean up importer mapping and unsupported feature handling
3. Add a minimal importer test/fixture
4. Re-verify the spinning cube prototype still builds and runs

## Explicit Non-Goals For This Pass
Do not implement these yet:

- animation playback
- skinning
- morph targets
- runtime editor tooling
- mesh compression pipelines
- Vulkan renderer parity work beyond the existing abstraction layer

## Done Criteria
This implementation pass is complete when:

- `.gltf` and `.glb` static scenes load into `SceneAsset`
- external and embedded texture data are handled for the supported path
- the spinning cube prototype still runs
- a minimal importer regression test exists
