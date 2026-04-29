# Stellar Engine Implementation Status

_Last updated: 2026-04-28_

This document is the current, branch-facing status reference for implemented engine behavior. Historical roadmap files under `Plans/` and `.kilo/plans/` are archival and may contain stale intermediate phase descriptions.

## Rendering backend status

- OpenGL and Vulkan are runtime-selectable through the shared graphics abstraction.
- OpenGL is render-capable for the current lightweight glTF path.
- Vulkan is no longer an upload-only or no-op backend on this branch. It initializes, creates swapchain resources, records draw commands, submits frames, presents, and has opt-in context smoke coverage.
- Default tests remain display-free; GPU/display-dependent OpenGL and Vulkan context tests are opt-in.
- The current renderer targets lightweight material parity across OpenGL and Vulkan, not full glTF PBR compliance.

## glTF support status

Implemented on this branch:

- Static glTF mesh import for triangle primitives, bounds, tangents, UV0/UV1, vertex colors, textures, samplers, and materials.
- `.gltf` and `.glb` importer coverage through generated fixtures.
- Runtime scene loading through the client asset path.
- Base color, normal, metallic-roughness, occlusion, emissive, alpha mask/blend, double-sided materials, UV routing, vertex color modulation, and conservative tangent generation.
- `KHR_texture_transform` for supported material texture slots.
- `KHR_materials_unlit`.
- Skin and animation asset import, runtime animation evaluation, scene pose generation, and GPU skinning.
- Shared OpenGL/Vulkan skin palette behavior with a conservative runtime cap of 256 joints per draw.
- Vulkan material, alpha/culling, texture, skinning, resize/recreate, and resource lifetime hardening sufficient for the current lightweight parity target.

Unsupported or intentionally deferred:

- Full metallic-roughness PBR. The current shading model is intentionally lightweight.
- Morph target deformation and animation `weights` channels.
- glTF-authored cameras and lights. The Phase 5F decision-gate work has treated these as deferred rather than implemented; the current asset model has no camera or light asset storage.
- Broad glTF extension coverage beyond the supported material extensions listed above.
- Deterministic GPU readback validation for rendered pixel output.

## Extension policy

- Supported required extensions are accepted only after importer and both render backends can represent and render them consistently.
- Unknown or unsupported `extensionsRequired` entries should fail clearly with the extension name in the error.
- Optional unsupported data should degrade predictably without creating partial runtime state that implies support.
- The current required-extension allowlist is limited to `KHR_texture_transform` and `KHR_materials_unlit`.

## Recommended next status-aligned work

The previous recommended next step, the authored cameras/lights decision gate, has already been completed as a deferral in the Phase 5F archive. There is no longer a single pre-approved next glTF implementation slice in the current docs.

If continuing glTF implementation, pick the next slice from an actual asset or product requirement rather than reopening a completed decision gate. Practical candidates are:

1. **Authored cameras**, if imported scenes should drive the viewer camera. This would require a backend-neutral camera asset model, camera import, node camera references, and client/viewer selection.
2. **Morph targets**, if assets need facial animation or animated morph weights. This would require mesh morph target storage, animation `weights` channel support, render submission changes, and OpenGL/Vulkan shader parity.
3. **`KHR_lights_punctual` or fuller PBR**, only if the renderer direction expands beyond the current lightweight material model.
4. **Asset compatibility pass**, using representative external Blender/glTF assets to identify the next real blocker before expanding the runtime model.

## Notes for documentation readers

`docs/Design.md` remains the broad architecture/design document. When it conflicts with this file on current implementation status, prefer this file for the `glTF-vulkan` branch status.