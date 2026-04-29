# Phase 5F: Targeted glTF Feature Expansion

## Goal

Implement a conservative, targeted expansion of glTF support after Phase 5E Vulkan render parity is complete.

Phase 5F should add only glTF features that fit Stellar Engine's current lightweight runtime scene model, preserve OpenGL/Vulkan parity, and keep default tests display-free. The agent should update this Phase 5F plan with completion notes after implementation. The existing `.kilo/plans/glTF-vulkan-parity.md` plan does **not** need to be edited after this implementation unless explicitly requested later.

## Design Constraints To Preserve

- Target platform remains Linux-first.
- Keep C++23 and CMake 3.20+.
- Preserve runtime-selectable OpenGL and Vulkan backends behind the common graphics abstraction.
- Preserve OpenGL/Vulkan behavior parity for any new runtime-visible feature.
- Keep the client presentation-focused; do not introduce game-server or ECS authority changes for glTF feature work.
- Keep default CTest display-free and deterministic.
- Keep GPU/display-dependent OpenGL/Vulkan tests opt-in.
- Preserve `std::expected<T, stellar::platform::Error>` for fallible importer/setup/upload paths; do not introduce exceptions.
- Keep frame functions `noexcept`.
- Public APIs touched by this work must retain Doxygen `@brief` comments.
- Do not broaden the project into full glTF viewer/editor scope unless a feature is explicitly selected below.
- Do not modify `AGENTS.md`.

## Agent Routing

Primary agent: `@miyamoto`

Use `@miyamoto` for glTF asset routing, graphics asset model updates, shaders, OpenGL/Vulkan parity, and graphics tests.

Use `@carmack` only if a backend-neutral asset/runtime interface change affects non-graphics ownership or core scene/animation model design.

Do not create subagents from non-director roles.

## Required Reading Before Editing

Read these first:

- `AGENTS.md`
- `docs/Design.md`
- `.kilo/plans/glTF-vulkan-parity.md`
- `.kilo/plans/1777257747393-clever-tiger.md`
- `CMakeLists.txt`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/assets/TextureAsset.hpp`
- `include/stellar/assets/MaterialAsset.hpp`
- `include/stellar/assets/MeshAsset.hpp`
- `include/stellar/assets/AnimationAsset.hpp`
- `include/stellar/assets/SkinAsset.hpp`
- `include/stellar/graphics/GraphicsDevice.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `include/stellar/graphics/SceneRenderer.hpp`
- `src/import/gltf/Importer.cpp`
- `src/import/gltf/MaterialImport.cpp`
- `src/import/gltf/TextureImport.cpp`
- `src/import/gltf/MeshImport.cpp`
- `src/import/gltf/SkinImport.cpp`
- `src/import/gltf/AnimationImport.cpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevice*.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- Existing optional OpenGL/Vulkan context smoke tests.

## Current Phase 5F Scope From The Parity Plan

The existing parity roadmap recommends this order:

1. `KHR_texture_transform`, because it affects common textured assets and can be represented backend-neutrally.
2. `KHR_materials_unlit`, because it is simple and useful for sprite-like/game assets.
3. Larger skin palettes using UBO/SSBO/texture buffers so OpenGL is not capped at 96 uniform joints and Vulkan can share the model.
4. Fuller metallic-roughness PBR only if the renderer direction calls for glTF visual fidelity rather than the current lightweight engine style.
5. Morph targets only if animation weight channels should become functional.
6. Cameras and lights only if runtime scene-authored camera/light selection is desired.

This plan keeps that order, but splits Phase 5F into explicit implementation slices and decision gates.

## Recommended Phase 5F Slice Order

### Phase 5F.0: Scope Lock And Baseline Audit

Goal: confirm Phase 5E is complete enough to safely expand features.

Tasks:

1. Confirm default display-free tests pass before changes.
2. Confirm glTF importer regression tests pass.
3. Confirm Phase 5E Vulkan parity tests either pass in an opt-in Vulkan environment or have documented environment limitations.
4. Audit current asset model for material texture slots, samplers, material flags, skin palette limits, unsupported required extension policy, and animation weight rejection.
5. Decide whether Phase 5F will implement all slices below or only the first two low-risk extensions in the current pass.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Optional backend validation if available:

```bash
cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Completion-note requirement:

- Record the baseline commit/test state in this plan.

### Completion Notes (2026-04-27)

- Implemented: `Phase 5F.0` baseline audit only; no runtime feature code was changed in this
  slice. Baseline commit state was `30f08f5` with `Plans/Phase5F.md` present as an untracked
  planning file before these completion notes were added.
- Backend-neutral asset changes: none. Current audit findings: materials expose base-color,
  normal, metallic-roughness, occlusion, and emissive texture slots; `MaterialTextureSlot` carries
  a texture index, texcoord set, and scalar slot scale; samplers carry min/mag filters plus S/T wrap
  modes; material flags currently cover alpha mode, alpha cutoff, and double-sided rendering.
  There is not yet backend-neutral `KHR_texture_transform` or `KHR_materials_unlit` state.
- Importer behavior: current required-extension policy rejects every `extensionsRequired` entry
  with the extension name in the error. Optional unsupported cameras, lights, morph targets, UV2+,
  JOINTS_1/WEIGHTS_1, and optional material extensions degrade/ignore without creating runtime
  state. Skin joint indices are validated against the bound skin joint palette. Animation `weights`
  channels are rejected because morph targets are unsupported.
- OpenGL behavior: audited current path only. OpenGL samples the five material texture slots,
  applies UV0/UV1 routing, vertex color, alpha mask/blend, double-sided culling, lightweight
  metallic/roughness/occlusion/emissive lighting, normal maps when tangents exist, and uses a
  96-joint uniform-array skinning cap.
- Vulkan behavior: audited current path only. Vulkan opt-in smoke coverage is configured by
  `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`; it is not part of default CTest. Current Vulkan material
  path mirrors the lightweight OpenGL slots through descriptors and push constants, alpha/culling
  variants, and a matching 96-joint skinning cap.
- Tests added/updated: none for this audit slice.
- Default validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - `cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test stellar_animation_runtime_test stellar_graphics_backend_selection_test -j$(nproc)`
  - `ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload|animation_runtime|graphics_backend_selection)$' --output-on-failure`
  - Result: pass; full default suite passed 7/7 tests, focused suite passed 5/5 tests.
- Optional backend validation:
  - `cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`
  - `cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)`
  - `ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure`
  - Result: pass; Vulkan smoke passed 1/1 tests in the available hidden-window Vulkan environment.
- Manual validation:
  - Not run for representative glTF visual comparison; this slice was limited to baseline audit and
    automated default/opt-in smoke validation.
- Deferred follow-up:
  - Selected current implementation scope is Phase 5F.1 `KHR_texture_transform` and Phase 5F.2
    `KHR_materials_unlit` only. Larger skin palettes, full PBR, morph targets/animation weights,
    authored cameras, and authored lights remain deferred to later Phase 5F decision gates.

### Phase 5F.1: `KHR_texture_transform`

Goal: support glTF texture coordinate transforms for common material texture slots.

Rationale:

This is the best first Phase 5F feature because it is common in real assets, is material/texture-slot local, can be represented backend-neutrally, and does not require a new scene or animation model.

Tasks:

1. Add a backend-neutral texture transform model.

   Recommended API shape in `include/stellar/assets/TextureAsset.hpp`:

   ```cpp
   /**
    * @brief KHR_texture_transform state for a material texture slot.
    */
   struct TextureTransform {
       std::array<float, 2> offset{0.0f, 0.0f};
       float rotation = 0.0f;
       std::array<float, 2> scale{1.0f, 1.0f};
       std::optional<std::uint32_t> texcoord_set;
       bool enabled = false;
   };
   ```

   Then add `TextureTransform transform;` to `MaterialTextureSlot`.

   Avoid reusing the existing scalar `MaterialTextureSlot::scale`, because current code likely uses that scalar for normal texture scale or similar glTF texture-info scalar behavior. Keep transform scale as a distinct `std::array<float, 2>`.

2. Parse `KHR_texture_transform` on every supported glTF `textureInfo`-like slot:
   - base color texture
   - normal texture
   - metallic-roughness texture
   - occlusion texture
   - emissive texture

3. If a transform extension supplies its own `texCoord`, route that value through `TextureTransform::texcoord_set` or resolve it into the final slot texcoord during import. Make the choice explicit and test it.

4. Update `extensionsRequired` policy:
   - Allow `KHR_texture_transform` in `extensionsRequired` only after the importer and both render backends support it.
   - Keep unknown required extensions rejected with clear errors.

5. Update backend-neutral draw/upload structs if needed:
   - Ensure texture transform data reaches `GraphicsDevice::create_material` and `draw_mesh`.
   - Keep the common graphics abstraction stable if possible. Prefer extending existing `MaterialUpload`/material records over changing public draw APIs.

6. Update OpenGL shader/material path:
   - Apply texture transform before sampling.
   - Respect per-slot texcoord routing.
   - Apply transform consistently for all supported slots.
   - Formula should match glTF extension semantics: transform UV by offset, rotation, and scale in the expected order.

7. Update Vulkan shader/material path:
   - Add compact push constants or descriptor/uniform data for transforms.
   - Preserve push-constant size limits; use uniform/storage buffer if the existing push-constant block cannot safely grow.
   - Match OpenGL behavior exactly.

8. Add display-free importer regression tests:
   - default transform omitted
   - offset only
   - rotation only
   - non-uniform scale
   - extension `texCoord` override
   - required extension accepted after support is complete
   - unknown required extension still rejected

9. Add display-free render inspection tests:
   - material slot records carry transform data to the recording device
   - UV0/UV1 routing remains correct

10. Add optional backend smoke coverage where feasible:
   - simple transformed base-color texture fixture
   - transform on a secondary slot such as normal or emissive if smoke complexity remains small

Validation:

```bash
cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)
ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload)$' --output-on-failure
```

Optional:

```bash
ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Acceptance criteria:

- `KHR_texture_transform` is accepted when optional or required.
- Unknown required extensions remain rejected.
- Texture transform data is represented backend-neutrally.
- OpenGL and Vulkan sample with matching transformed UVs.
- Default tests remain display-free.

### Completion Notes (2026-04-27)

- Implemented: `Phase 5F.1` `KHR_texture_transform` only. Base color, normal,
  metallic-roughness, occlusion, and emissive material texture slots now carry offset,
  rotation, scale, and extension `texCoord` override data through import, upload, and rendering.
- Backend-neutral asset changes: added `assets::TextureTransform` to `MaterialTextureSlot` and
  preserved the existing scalar slot `scale` for normal scale/occlusion strength semantics. Added
  the same transform payload to resolved `graphics::MaterialTextureBinding`; `RenderScene` resolves
  the effective texcoord set from the extension override while retaining original material slot data.
- Importer behavior: `MaterialImport` parses `KHR_texture_transform` from all supported
  textureInfo-like slots. `extensionsRequired` now accepts `KHR_texture_transform` because importer,
  OpenGL, and Vulkan routing are implemented; unknown required extensions still fail with the
  extension name in the error.
- OpenGL behavior: the material fragment shader applies glTF transform order `T * R * S` before
  sampling every supported texture slot and uses per-slot texcoord routing after extension override
  resolution.
- Vulkan behavior: added a per-material transform uniform buffer at material descriptor binding 5 so
  push constants stay at 128 bytes. Regenerated embedded SPIR-V material shaders to sample all five
  slots with the same transformed UV and texcoord routing behavior as OpenGL.
- Tests added/updated: importer regression coverage for omitted/default transform, offset,
  rotation, non-uniform scale, `texCoord` override, required-extension acceptance, and unknown
  required-extension rejection. Render upload/inspection tests now verify transform data and UV0/UV1
  override routing reach recording devices.
- Default validation:
  - `cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)`
  - `ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: pass; focused suite passed 3/3 tests and full default suite passed 7/7 tests.
- Optional backend validation:
  - `cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)`
  - `ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure`
  - `cmake --build build-opengl-context --target stellar_opengl_context_smoke_test -j$(nproc) && ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure`
  - Result: Vulkan smoke passed 1/1 tests. OpenGL optional smoke was not run because
    `build-opengl-context` was not configured in this workspace.
- Manual validation:
  - Not run; no representative display/GPU visual comparison asset was exercised in this pass.
- Deferred follow-up:
  - No optional backend readback fixture was added for this slice. Phase 5F.2
    `KHR_materials_unlit`, larger skin palettes, full PBR, morph targets, cameras, and lights remain
    deferred to their planned slices/decision gates.

### Phase 5F.2: `KHR_materials_unlit`

Goal: support unlit glTF materials for sprite-like/game assets.

Rationale:

The design document describes sprite-like game rendering in a 3D environment. Unlit materials are useful for authored sprites, UI-like meshes, and simple stylized assets without requiring full PBR.

Tasks:

1. Add `bool unlit = false;` to `MaterialAsset` with Doxygen if needed.
2. Parse `KHR_materials_unlit` in material import.
3. Update `extensionsRequired` policy:
   - Allow `KHR_materials_unlit` only after importer and both render backends support it.
4. Update OpenGL material shader path:
   - If unlit, output base color / base color texture / vertex color / alpha behavior without directional lighting, normal mapping contribution, metallic/roughness lighting, or occlusion darkening unless the current lightweight design explicitly wants occlusion on unlit. Prefer matching glTF unlit expectations: color is not affected by lights.
   - Preserve alpha mask/blend behavior.
5. Update Vulkan shader path to match OpenGL.
6. Ensure unlit materials still respect:
   - base color factor
   - base color texture
   - vertex colors
   - texture transform from Phase 5F.1 if implemented
   - alpha mode/cutoff/blend
   - double-sided culling
7. Add importer tests:
   - optional `KHR_materials_unlit`
   - required `KHR_materials_unlit`
   - unknown required extension still rejected
8. Add render inspection tests:
   - material upload records unlit flag
   - draw routing preserves unlit material state
9. Add optional backend smoke fixtures:
   - unlit textured material
   - unlit alpha-mask material if convenient

Validation:

```bash
cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)
ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload)$' --output-on-failure
```

Acceptance criteria:

- Unlit materials import and render consistently in OpenGL and Vulkan.
- Required `KHR_materials_unlit` is accepted only when fully supported.
- Lighting-dependent material behavior remains unchanged for non-unlit materials.

### Completion Notes (2026-04-28)

- Implemented: `Phase 5F.2` `KHR_materials_unlit` only. Materials now carry a
  backend-neutral unlit flag from glTF import through material upload and OpenGL/Vulkan rendering.
- Backend-neutral asset changes: added `MaterialAsset::unlit` while preserving existing alpha mode,
  alpha cutoff, double-sided, base color, texture transform, vertex color, and texture slot state.
- Importer behavior: `MaterialImport` parses `KHR_materials_unlit`; `extensionsRequired` now accepts
  `KHR_materials_unlit` and `KHR_texture_transform`, while unknown required extensions still fail
  with the extension name in the error.
- OpenGL behavior: the material shader bypasses directional lighting, normal-map lighting,
  metallic/roughness attenuation, and occlusion darkening for unlit materials, while retaining base
  color factor/texture, vertex color, texture transforms, emissive contribution, alpha mask/blend,
  and double-sided culling behavior.
- Vulkan behavior: material push flags now include the unlit state, and the embedded fragment SPIR-V
  was regenerated to match OpenGL's unlit branch while preserving existing descriptor, transform,
  alpha, culling, and skinning paths.
- Tests added/updated: importer regression coverage for optional and required
  `KHR_materials_unlit`, render upload/inspection checks for the unlit material flag, and opt-in
  Vulkan smoke coverage for an unlit textured alpha-mask/double-sided material.
- Default validation:
  - `cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)`
  - `ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: pass; focused suite passed 3/3 tests and full default suite passed 7/7 tests.
- Optional backend validation:
  - `cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`
  - `cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)`
  - `ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure`
  - `cmake -S . -B build-opengl-context -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON`
  - `cmake --build build-opengl-context --target stellar_opengl_context_smoke_test -j$(nproc)`
  - `ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure`
  - `STELLAR_RUN_OPENGL_CONTEXT_TESTS=1 ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure`
  - Result: Vulkan smoke passed 1/1 tests; OpenGL skip path was validated, then explicit OpenGL
    context smoke passed 1/1 tests with the runtime environment gate enabled.
- Manual validation:
  - Not run; no representative authored glTF visual comparison asset was exercised in this pass.
- Deferred follow-up:
  - No deterministic GPU readback fixture was added for unlit visible color. Larger skin palettes,
    full PBR, morph targets, cameras, and lights remain deferred to later Phase 5F slices/decision
    gates.

### Phase 5F.3: Larger Skin Palettes

Goal: remove or raise the current 96-joint rendering cap using a shared backend-neutral model that OpenGL and Vulkan can both implement.

Rationale:

The current 96-joint cap was appropriate for Phase 5E parity, but larger real glTF characters may exceed it. This slice should be tackled after low-risk material extensions because it touches shaders, per-draw buffers, validation policy, and possibly backend-neutral limits.

Recommended approach:

- Prefer a storage-buffer model for both OpenGL 4.5 and Vulkan 1.2.
- Use SSBOs in OpenGL and storage buffers in Vulkan where available.
- Keep a conservative maximum, for example 256 or 512 joints per draw, unless the renderer can support arbitrary palettes cleanly.
- Keep importer validation separate from renderer capability validation:
  - Import may allow larger skins.
  - Runtime upload/draw should fail clearly if a skin exceeds the configured runtime cap.

Tasks:

1. Define a backend-neutral skin palette capacity policy:
   - constant name
   - value
   - error behavior
   - documentation
2. Update `MeshAsset`, `SkinAsset`, or draw structures only if required. Prefer avoiding asset-format churn unless necessary.
3. Update `RenderScene` skin draw preparation to pass larger palettes without truncation.
4. Update OpenGL skinning path:
   - replace uniform array joint matrices with SSBO or other scalable buffer
   - preserve static/no-skin path
5. Update Vulkan skinning path:
   - replace or extend current per-draw UBO slot model with storage buffers or another scalable mechanism
   - preserve per-frame synchronization and resource lifetime safety from Phase 5E.9
6. Update shader code for both backends.
7. Update validation:
   - valid palette at old limit
   - valid palette above old 96 limit
   - over new cap fails or skips clearly
   - undersized palette for referenced joint index still fails/skips clearly
8. Add generated tests:
   - importer/runtime test for >96 joints
   - render inspection forwarding of >96-joint palette
   - optional backend smoke for >96 if feasible

Validation:

```bash
cmake --build build --target stellar_animation_runtime_test stellar_render_scene_inspection_test stellar_render_scene_upload_test -j$(nproc)
ctest --test-dir build -R '^(animation_runtime|render_scene_inspection|render_scene_upload)$' --output-on-failure
```

Optional backend validation:

```bash
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure
```

Acceptance criteria:

- Current skinned assets still render.
- Assets with more than 96 joints render or fail clearly according to the new documented cap.
- OpenGL and Vulkan use the same effective skin palette limit.
- No regression in static or unskinned draw paths.

### Phase 5F.4: Full Metallic-Roughness PBR Decision Gate

Goal: decide whether to implement fuller glTF PBR or explicitly defer it.

Recommendation:

Do **not** implement full PBR by default in this phase unless the renderer direction has changed from lightweight engine style toward visual glTF fidelity.

Tasks if deferring:

1. Add a note to this plan explaining that the current lightweight metallic-roughness approximation remains intentional.
2. Keep importer accepting core metallic-roughness material fields already represented.
3. Add follow-up notes for future PBR requirements:
   - image-based lighting
   - environment maps
   - tone mapping
   - color management
   - physically based BRDF parity
   - normal/occlusion/emissive interactions
   - OpenGL/Vulkan shader parity

Tasks if implementing:

1. Define exact visual target and lighting model.
2. Add required environment/lighting inputs to the renderer design.
3. Preserve OpenGL/Vulkan parity.
4. Add generated material fixtures and optional backend visual/readback tests where deterministic.

Acceptance criteria if deferred:

- Clear documentation in this plan that full PBR remains future work.
- No false claim of full glTF PBR compliance.

### Phase 5F.5: Morph Targets Decision Gate

Goal: decide whether to represent morph targets and make animation `weights` channels functional.

Recommendation:

Defer unless there is an immediate asset requirement. Morph targets affect mesh asset layout, animation runtime, importer validation, renderer vertex processing, GPU buffer formats, and draw submission.

Tasks if deferring:

1. Keep importer rejection of animation `weights` channels unless morph target support is implemented.
2. Keep optional morph target data degraded or ignored according to current policy.
3. Record morph target support as future work.

Tasks if implementing:

1. Extend `MeshAsset` to store morph target deltas for positions, normals, and tangents where supported.
2. Extend `AnimationAsset` and runtime evaluation to support `weights` channels.
3. Update `RenderScene` to pass morph weights per draw.
4. Update OpenGL/Vulkan shaders and buffers.
5. Add importer, animation runtime, render inspection, and optional backend tests.
6. Define caps for morph target count and fail clearly over cap.

Acceptance criteria if deferred:

- Existing unsupported-feature behavior remains clear and tested.
- No partial morph runtime path silently produces incorrect animation.

### Phase 5F.6: Cameras And Lights Decision Gate

Goal: decide whether authored glTF cameras/lights should affect runtime scene selection and rendering.

Recommendation:

Defer by default. The current client already auto-fits a viewer camera, and the design document’s long-term game camera/lighting model should stay engine-driven unless authored glTF scene controls become a clear requirement.

Tasks if deferring:

1. Keep optional cameras/lights ignored/degraded according to current policy.
2. Keep required camera/light extensions rejected unless fully represented.
3. Record future requirements:
   - scene-authored camera selection
   - CLI/config camera selection
   - punctual lights or extension-specific light import
   - shader/light uniform model
   - OpenGL/Vulkan parity

Tasks if implementing cameras:

1. Add backend-neutral camera asset structures.
2. Parse glTF cameras.
3. Add CLI/config scene camera selection.
4. Keep auto-fit fallback.

Tasks if implementing lights:

1. Choose supported light extension(s), likely `KHR_lights_punctual`.
2. Add backend-neutral light asset structures.
3. Update shader/light data for OpenGL and Vulkan.
4. Add tests and manual validation.

Acceptance criteria if deferred:

- Current auto-fit camera and lightweight lighting behavior remain stable.
- No claim of scene-authored camera/light support.

## Implementation Guidance

### Keep Phase 5F Conservative

Suggested implementation pass:

1. Implement Phase 5F.0.
2. Implement Phase 5F.1 `KHR_texture_transform`.
3. Implement Phase 5F.2 `KHR_materials_unlit`.
4. Stop and update this plan unless there is an explicit need for larger skin palettes or other features.

This gives the best value/risk ratio:
- both extensions are common,
- both are representable in the current material model,
- both can be tested display-free,
- both preserve the lightweight renderer direction.

### Avoid Broad API Churn

Prefer extending existing backend-neutral asset/upload structures over adding new render APIs.

Do not change `GraphicsDevice` draw signatures unless unavoidable.

### Required Extension Policy

The importer currently rejects unsupported `extensionsRequired`. For each newly supported extension:

1. Parse and represent the feature backend-neutrally.
2. Implement OpenGL behavior.
3. Implement Vulkan behavior.
4. Add importer and render routing tests.
5. Only then allow the extension in `extensionsRequired`.

### Testing Strategy

Default tests:

- importer regression
- animation runtime where relevant
- render scene upload
- render scene inspection
- backend selection
- client validation smoke

Optional tests:

- OpenGL context smoke behind existing opt-in gates
- Vulkan context smoke behind existing opt-in gates

Do not make optional GPU/display tests mandatory.

## Files Likely To Change

Likely for Phase 5F.1 and 5F.2:

- `include/stellar/assets/TextureAsset.hpp`
- `include/stellar/assets/MaterialAsset.hpp`
- `src/import/gltf/MaterialImport.cpp`
- `src/import/gltf/TextureImport.cpp`
- `src/import/gltf/Importer.cpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- optional context smoke tests

Likely for larger skin palettes:

- `include/stellar/assets/SkinAsset.hpp`
- `include/stellar/scene/AnimationRuntime.hpp`
- `src/scene/AnimationRuntime.cpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/*`
- shader sources or embedded SPIR-V generation path
- related tests

Avoid:

- `.kilo/plans/glTF-vulkan-parity.md`, unless later explicitly requested.
- `AGENTS.md`.
- Mandatory dependency additions.
- Default GPU/display test requirements.

## Build And Validation Commands

Baseline/default:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Focused default validation:

```bash
cmake --build build --target stellar_import_gltf_regression stellar_render_scene_inspection_test stellar_render_scene_upload_test stellar_animation_runtime_test stellar_graphics_backend_selection_test -j$(nproc)
ctest --test-dir build -R '^(gltf_importer_regression|render_scene_inspection|render_scene_upload|animation_runtime|graphics_backend_selection)$' --output-on-failure
```

Optional OpenGL context validation:

```bash
cmake -S . -B build-opengl-context -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build-opengl-context --target stellar_opengl_context_smoke_test -j$(nproc)
ctest --test-dir build-opengl-context -R '^opengl_context_smoke$' --output-on-failure
```

Optional Vulkan context validation:

```bash
cmake -S . -B build-vulkan-tests -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Manual validation, when display/GPU/assets are available:

```bash
./build/stellar-client --renderer opengl --asset <representative-gltf>
./build/stellar-client --renderer vulkan --asset <representative-gltf>
```

Manual checks for Phase 5F.1 and 5F.2:

- base color texture transform offset/scale/rotation
- transform `texCoord` override
- transformed normal/emissive or secondary slot if supported
- unlit textured material
- unlit alpha mask/blend
- OpenGL/Vulkan visual parity at a smoke-test level

## Completion Notes Required

After implementation and validation, update **this plan only** with completion notes.

Do **not** update `.kilo/plans/glTF-vulkan-parity.md` for this task unless explicitly requested later.

Use this format under each completed slice:

```md
### Completion Notes (YYYY-MM-DD)

- Implemented: `<features>`
- Backend-neutral asset changes: `<summary>`
- Importer behavior: `<required/optional extension handling and validation>`
- OpenGL behavior: `<summary>`
- Vulkan behavior: `<summary>`
- Tests added/updated: `<summary>`
- Default validation:
  - `<commands>`
  - Result: `<pass/fail>`
- Optional backend validation:
  - `<commands or not run with reason>`
  - Result: `<pass/fail/skip>`
- Manual validation:
  - `<commands/assets/results, or not run with reason>`
- Deferred follow-up:
  - `<specific items moved to later Phase 5F slice or Phase 5G>`
```

## Phase 5F Acceptance Criteria

Phase 5F is complete for the selected implementation scope only when:

- The selected feature set is explicitly listed in this plan.
- Backend-neutral asset representation exists for each selected feature.
- Importer accepts supported required extensions only after full runtime support exists.
- Unsupported required extensions still fail clearly.
- OpenGL and Vulkan behavior are both updated for runtime-visible features.
- Display-free importer/render inspection tests cover data routing and policy behavior.
- Optional backend smoke tests are updated where feasible without making them default.
- Default CTest remains display-free.
- Completion notes are added to this plan with honest test results.
- Deferred features are explicitly documented rather than implied as supported.
