# Stellar Engine Implementation Status

_Last updated: 2026-04-28_

This document is the current, branch-facing status reference for implemented engine behavior. Historical roadmap files under `Plans/` and `.kilo/plans/` are archival and may contain stale intermediate phase descriptions. The Phase 6 plan files under `Plans/` are current implementation handoff plans for the collision/world-authoring direction unless their completion notes say otherwise.

## Branch focus

Current branch: `collision-movement`.

Primary near-term goal: move from "glTF renders as a scene" toward "glTF can define playable static world geometry" for a 3D world with 2D billboard entities.

The active planned slice is `Plans/Phase6A-LevelCollisionExtraction.md`:

- Add backend-neutral static level collision data.
- Extract collision triangles from selected glTF nodes/meshes.
- Use simple node-name conventions such as `COL_*`, `Collision_*`, and a parent node named `Collision`.
- Transform collision triangles into scene/world space.
- Keep collision data independent of OpenGL/Vulkan.
- Keep default tests display-free.

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

Planned for the collision/world-authoring direction:

- **Phase 6A:** Static level collision extraction from glTF. This is the active next implementation slice.
- **Phase 6B:** Collision queries and minimal movement resolution against Phase 6A static collision data.
- **Phase 6C:** Billboard sprite rendering for 2D entities/objects in 3D world space.
- **Phase 6D:** World metadata extraction from glTF node conventions for spawns, triggers, sprite markers, and similar gameplay markers.

Unsupported or intentionally deferred:

- Full metallic-roughness PBR. The current shading model is intentionally lightweight.
- Morph target deformation and animation `weights` channels.
- glTF-authored cameras and lights. The Phase 5F decision-gate work treated these as deferred rather than implemented; the current asset model has no camera or light asset storage.
- Broad glTF extension coverage beyond the supported material extensions listed above.
- Deterministic GPU readback validation for rendered pixel output.
- Full physics engine integration, dynamic rigid bodies, navigation mesh/pathfinding, runtime trigger behavior, and ECS/server spawning from glTF metadata. These are future work beyond Phase 6A unless specifically added by later plans.

## Extension policy

- Supported required extensions are accepted only after importer and both render backends can represent and render them consistently.
- Unknown or unsupported `extensionsRequired` entries should fail clearly with the extension name in the error.
- Optional unsupported data should degrade predictably without creating partial runtime state that implies support.
- The current required-extension allowlist is limited to `KHR_texture_transform` and `KHR_materials_unlit`.
- Static level collision is intentionally planned as an engine node-convention/import policy, not a glTF extension.

## Recommended next status-aligned work

Implement Phase 6A first.

Recommended execution order:

1. `Plans/Phase6A-LevelCollisionExtraction.md`
   - Build the collision asset model and glTF extraction path.
   - Add display-free importer tests for collision node naming, transforms, ordinary render-node exclusion, normals, and bounds.
2. `Plans/Phase6B-CollisionQueriesAndMovement.md`
   - Add ray/segment queries, ground probe, and minimal sweep/slide movement over static collision.
3. `Plans/Phase6C-BillboardSpriteRendering.md`
   - Add 2D billboard sprite rendering in 3D world space with OpenGL/Vulkan parity.
4. `Plans/Phase6D-WorldMetadataFromGltf.md`
   - Add glTF node-convention metadata for player/entity spawns, triggers, sprite markers, and optional portal markers.

Do not spend the next implementation slice on full PBR, morph targets, glTF cameras, or glTF lights unless a concrete asset requirement appears. For the current engine goal, playable level collision and sprite/world metadata are higher-value than deeper glTF visual fidelity.

## Notes for documentation readers

`docs/Design.md` remains the broad architecture/design document. When it conflicts with this file on current implementation status, prefer this file for the `collision-movement` branch status.