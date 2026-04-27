# Remaining glTF Implementation Plan

## Summary

Further implementation is needed. The current branch has substantial glTF support: static mesh import, materials/textures, UV1, vertex colors, generated/authored tangents, skins, animation assets, runtime animation evaluation, OpenGL GPU skinning, backend selection, Vulkan upload-record skeleton, display-free render inspection tests, runtime viewer animation playback for imported scenes, and auto-fit viewer camera placement.

The work is not complete because several paths are still explicitly unsupported or not wired into runtime use:

- Vulkan rendering is still a no-op after initialization/upload recording.
- Phase 5C now drives `AnimationPlayer` for imported scenes and auto-fits the viewer camera to scene bounds.
- Deferred viewer controls remain play/pause defaults, manual camera distance/FOV, and scene index selection.
- Phase 5A preflight and Phase 5B accessor, animation, and skin joint palette validation are complete.
- Phase 4B test infrastructure is partially complete, but optional headless render tests and representative render fixtures remain.
- Morph targets, cameras, lights, UV2+, JOINTS_1/WEIGHTS_1, full glTF PBR, and most material extensions remain unsupported.

## Current Evidence

- `.kilo/plans/glTF-implementation-plan.md` lists Phase 4B as in progress and still has Phase 4B tasks for optional headless/context tests and representative assets.
- `.kilo/plans/glTF-implementation-plan.md` known limitations still include Vulkan no-op rendering, lightweight metallic-roughness shading, skinned Vulkan non-rendering, and unsupported morph targets/cameras/lights/extensions.
- `src/graphics/vulkan/VulkanGraphicsDevice.cpp` keeps `begin_frame`, `draw_mesh`, and `end_frame` as explicit Phase 4A no-ops.
- `src/graphics/SceneRenderer.cpp` now auto-fits camera placement from aggregate scene bounds and drives imported scene animation through `AnimationPlayer` when animations exist.
- `src/client/main.cpp` and `include/stellar/client/ApplicationConfig.hpp` expose animation selection/loop controls; play/pause defaults, manual camera distance/FOV, and selected-scene controls remain deferred.
- `src/import/gltf/Importer.cpp` now performs the Phase 5A unsupported `extensionsRequired` policy gate after `cgltf_validate`; Phase 5B accessor, animation, and skin joint palette validation is complete.
- `src/import/gltf/Importer.cpp` now rejects `weights` animation channels clearly because morph targets are not represented by the runtime asset model.

## Phase 0: Plan And Status Hygiene - Completed

Goal: make the roadmap match the code before adding more behavior.

### Completion Notes

Phase 0 has been completed as documentation/status hygiene only. The phased roadmap is
preserved, current OpenGL/Vulkan implementation status is called out explicitly, and remaining
Phase 4B test-infrastructure work stays visible rather than being treated as complete.

Further implementation remains. Phase 5A and Phase 5B are the recommended next implementation
slices because they are display-free, tighten importer correctness, and prevent silent acceptance
of unsupported or malformed glTF data before additional runtime rendering features are added.

Tasks:

1. Update `.kilo/plans/glTF-implementation-plan.md` status text so it no longer says the branch only contains work through Phase 3B.
2. Mark Phase 4B as partially completed or split remaining Phase 4B work into a follow-up phase.
3. Keep documented limitations explicit rather than silently implying full glTF support.

Validation:

- Documentation/status review only.

## Phase 5A: Importer Conformance Gate - Completed

Goal: fail clearly for unsupported required glTF features and document optional degradation behavior.

### Completion Notes

Phase 5A has been completed with a conservative `extensionsRequired` preflight that rejects all
required extensions after `cgltf_validate`, because no glTF extension is fully represented by the
runtime asset model. Regression coverage now verifies unsupported required extension names appear in
errors, and optional unsupported cameras, lights, morph targets, UV2+, JOINTS_1/WEIGHTS_1, and
material extensions import without creating unsupported runtime state.

Validation completed:

- `cmake --build build --target stellar_import_gltf_regression -j$(nproc)`
- `ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure`

Tasks:

1. Add an importer preflight after `cgltf_validate` that rejects unsupported `extensionsRequired`.
2. Decide the supported extension allowlist. Initial likely allowlist: none, or only extensions already handled by cgltf/core paths.
3. Add explicit negative tests for unsupported required extensions.
4. Add generated tests for optional unsupported data that should degrade predictably: cameras, lights, morph targets, UV2+, JOINTS_1/WEIGHTS_1, and optional material extensions.
5. Ensure error messages identify the unsupported feature clearly.

Validation:

- `cmake --build build --target stellar_import_gltf_regression -j$(nproc)`
- `ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure`

## Phase 5B: Accessor And Cross-Reference Validation - Completed

Goal: tighten correctness for valid glTF data and catch bad data earlier.

### Completion Notes

Phase 5B has been completed with backend-neutral importer validation. `TEXCOORD_0`,
`TEXCOORD_1`, and `COLOR_0` now accept float data or normalized unsigned byte/short data and
reject non-normalized integer encodings. Animation input times are required to be strictly
increasing, channel output semantics are validated by target path, and weight animation channels now
fail clearly because morph targets are not represented by the runtime asset model. Skinned mesh nodes
now validate enabled `JOINTS_0` data against the node's bound skin joint palette.

Validation completed:

- `cmake --build build --target stellar_import_gltf_regression -j$(nproc)`
- `ctest --test-dir build -R '^gltf_importer_regression$' --output-on-failure`
- `ctest --test-dir build -R '^animation_runtime$' --output-on-failure`

Tasks:

1. Support normalized integer `TEXCOORD_0` where glTF permits it, matching existing float-compatible handling for `TEXCOORD_1`.
2. Require normalized integer semantics for texcoord/color formats where required by glTF.
3. Add tests for normalized unsigned byte/short texcoords and colors.
4. Reject non-normalized integer texcoords/colors when invalid.
5. Validate animation input times are strictly increasing.
6. Validate animation output component counts by target path: translation/scale `VEC3`, rotation `VEC4`, and weights only after a morph-target policy is chosen.
7. Add a scene-level validation pass for skinned meshes so `JOINTS_0` indices are checked against the bound skin joint palette.
8. Add tests for out-of-range skin joint indices and malformed animation samplers/channels.

Validation:

- Importer regression tests.
- Animation runtime tests for any changed sampler assumptions.
- glTF enabled and disabled builds to preserve feature-gate behavior.

## Phase 5C: Runtime Viewer Usability - Completed

Goal: make imported glTF scenes viewable and animated in the client, not just importable/renderable through lower-level APIs.

### Completion Notes

Phase 5C has been completed with backend-neutral aggregate scene bounds, stable empty-scene
fallback bounds, auto-fit viewer camera placement, and runtime animation playback for imported
scenes. `SceneRenderer` now binds `AnimationPlayer` when animations exist, selects animation 0 by
default, supports CLI selection by `--animation-index` or `--animation-name`, honors
`--animation-loop`, advances by frame delta time, and renders animated scenes through the pose-aware
`RenderScene::render` path. Deferred controls: play/pause defaults, manual camera distance/FOV, and
scene index selection remain future viewer usability work rather than part of this minimal slice.

Validation completed:

- `cmake --build build --target stellar_render_scene_inspection_test stellar_render_scene_upload_test stellar_animation_runtime_test stellar_client_asset_validation_smoke stellar-client -j$(nproc)`
- `ctest --test-dir build -R '^(render_scene_inspection|render_scene_upload|animation_runtime|client_asset_validation_smoke|client_cli_asset_validation)$' --output-on-failure`

Tasks:

1. Add aggregate scene bounds computation from node transforms and primitive bounds.
2. Auto-fit the viewer camera to loaded scene bounds instead of always using `(0, 0, 3)` looking at the origin.
3. Bind `AnimationPlayer` inside `SceneRenderer` when the loaded scene has animations.
4. Auto-select animation 0 by default, with looping enabled unless configured otherwise.
5. Advance animation using frame delta time and call `RenderScene::render(..., pose)` for animated scenes.
6. Add CLI/config options for animation index/name, play/pause default, loop, camera distance/FOV, and optionally scene index.
7. Add display-free tests for bounds/camera calculation and animation pose forwarding using `RecordingGraphicsDevice` where feasible.

Validation:

- `render_scene_inspection`
- `render_scene_upload`
- `animation_runtime`
- client validation smoke tests
- Manual OpenGL run with a small static and animated glTF asset when a display is available.

## Phase 5D: Complete Phase 4B Test Infrastructure

Goal: reduce reliance on manual visual testing while keeping CI stable.

Tasks:

1. Add small generated representative render fixtures for textured, alpha mask/blend, UV1, vertex color, normal map, and skinned scenes.
2. Add optional OpenGL context smoke tests behind a CMake option or environment gate.
3. Add framebuffer readback tests only for deterministic basics such as clear color or simple material output.
4. Keep Vulkan render tests skipped or disabled until Vulkan presents frames.
5. Keep performance sanity non-flaky: validate traversal/upload/draw counts by default, and make timing thresholds opt-in only.

Validation:

- Default CTest remains display-free and deterministic.
- Optional context tests run only when explicitly enabled.

## Phase 5E: Vulkan Render Parity

Goal: make `--renderer vulkan` truthful for glTF runtime rendering rather than an upload-only no-op.

Tasks:

1. Implement Vulkan swapchain creation, image views, frame acquisition, presentation, and resize handling.
2. Add command pool/buffers and frame synchronization.
3. Upload mesh vertex/index buffers for the existing `StaticVertex` layout.
4. Upload textures/images with sRGB vs linear formats and create Vulkan samplers from backend-neutral sampler state.
5. Add descriptor layouts/sets for material textures, material factors, transforms, and skin matrices.
6. Port the current OpenGL lightweight material shader behavior to Vulkan shaders.
7. Implement alpha mask/blend, double-sided culling, UV0/UV1 routing, vertex colors, normal mapping, occlusion/emissive, and skinning parity.
8. Add Vulkan parity tests where possible, with GPU/context tests opt-in.

Validation:

- Backend selection tests.
- Vulkan initialization tests on systems with Vulkan support.
- Optional Vulkan render smoke tests.
- Manual run of representative static and skinned glTF scenes using `--renderer vulkan`.

## Phase 5F: Targeted Feature Expansion

Goal: decide which non-core glTF features are worth supporting after the core pipeline is robust.

Recommended order:

1. `KHR_texture_transform`, because it affects common textured assets and can be represented backend-neutrally.
2. `KHR_materials_unlit`, because it is simple and useful for sprite-like/game assets.
3. Larger skin palettes using UBO/SSBO/texture buffers so OpenGL is not capped at 96 uniform joints and Vulkan can share the model.
4. Fuller metallic-roughness PBR only if the renderer direction calls for glTF visual fidelity rather than the current lightweight engine style.
5. Morph targets only if animation weight channels should become functional.
6. Cameras and lights only if runtime scene-authored camera/light selection is desired.

Validation:

- Generated importer tests per feature.
- RenderScene upload/inspection tests for backend-neutral data routing.
- Optional backend render tests for visible behavior.

## Recommended Next Implementation Slice

Continue with Phase 5B before adding more rendering features. It is display-free and tightens remaining accessor, animation, and cross-reference validation gaps after the completed Phase 5A required-extension preflight. After that, Phase 5C gives the biggest user-visible improvement because imported animated assets will actually animate in the client and arbitrary-sized scenes will be framed correctly.
