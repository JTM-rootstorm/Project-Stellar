# Phase 5E.8 — Vulkan Skinning Descriptor Path

## Status

Completed on 2026-04-27.

Completion notes (2026-04-27):
- Added Vulkan skinned draw support for primitives with `JOINTS_0` / `WEIGHTS_0`, matching
  OpenGL's current 96-joint cap with Vulkan-private `kMaxSkinJoints = 96`.
- Added set `1` vertex-stage uniform-buffer descriptors backed by per-frame host-visible,
  host-coherent draw-uniform buffers. Each frame owns 256 descriptor-backed slots and resets an
  upload cursor at `begin_frame`, so multiple static or skinned draws can safely upload per-draw
  transform/skin data without push constants for joint matrices.
- Chose a single per-draw vertex uniform block containing `mvp`, `world`, padded normal matrix,
  `has_skinning`, `joint_count`, and a 96-`mat4` skin palette; material factors and material flags
  remain in the existing 128-byte push-constant block for fragment/material parity.
- Extended the Vulkan pipeline layout with material set `0` plus transform/skin set `1`, and added
  vertex attributes location `6` (`VK_FORMAT_R16G16B16A16_UINT` joints0) and location `7`
  (`VK_FORMAT_R32G32B32A32_SFLOAT` weights0).
- Regenerated the embedded Vulkan material vertex SPIR-V so skinned positions, normals, and tangents
  follow the OpenGL lightweight skinning math while preserving the existing fragment/material shader
  behavior.
- Removed the previous Vulkan skinned-primitive skip. Static primitives and skinned primitives with
  empty draw palettes use `has_skinning = false`; valid skinned draws upload and bind their palette;
  over-limit palettes or palettes smaller than referenced joint indices are skipped with clear
  Vulkan diagnostics.
- Added upload-time validation for skinned Vulkan primitives: joint indices must be below 96 and
  weights must be finite. Draw-time validation rejects palettes over 96 matrices and undersized
  palettes for the primitive's recorded max joint index.
- Extended `vulkan_context_smoke` with a generated skinned triangle using joints0/weights0, a
  non-identity one-matrix palette draw, and a 97-matrix over-limit safety draw while keeping Vulkan
  context coverage opt-in via `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.
- Validated default display-free coverage with `cmake --build build -j$(nproc)` and
  `ctest --test-dir build --output-on-failure`; both passed. Also validated the required subset with
  `cmake --build build --target stellar_graphics_backend_selection_test
  stellar_render_scene_inspection_test stellar_render_scene_upload_test
  stellar_animation_runtime_test -j$(nproc)` and `ctest --test-dir build -R
  '^(graphics_backend_selection|render_scene_inspection|render_scene_upload|animation_runtime)$'
  --output-on-failure`; both passed.
- Validated opt-in Vulkan coverage with `cmake -S . -B build-vulkan-tests
  -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON`, `cmake --build build-vulkan-tests --target
  stellar_vulkan_context_smoke_test -j$(nproc)`, and `ctest --test-dir build-vulkan-tests -R
  '^vulkan_context_smoke$' --output-on-failure`; all passed.
- Remaining limitations deferred: larger skin palettes / storage-buffer or multi-palette model remain
  Phase 5F work; resize/lifetime hardening remains Phase 5E.9.

## Objective

Implement Vulkan rendering for skinned glTF primitives with parity to the current OpenGL lightweight material path, using the existing backend-neutral `MeshPrimitiveDrawCommand::skin_joint_matrices` input and OpenGL's current 96-joint cap.

This phase should make Vulkan draw skinned primitives instead of skipping them, while preserving the existing static mesh/material behavior implemented in earlier Phase 5E work.

## Non-Goals

- Do not expand the skin palette limit beyond 96 joints in this phase.
- Do not introduce a new backend-specific skinning API.
- Do not move larger skin palette, storage-buffer, or multi-palette architecture work into this phase; leave that for Phase 5F.
- Do not pull Phase 5E.9 resize/lifetime hardening into this phase.
- Do not make Vulkan context tests part of the default display-free test suite.

## Design Constraints

- Preserve the backend-neutral `GraphicsDevice` interface.
- Continue routing skinning data through `MeshPrimitiveDrawCommand::skin_joint_matrices`.
- Keep default CTest display-free. Vulkan context rendering must remain opt-in through `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.
- Keep runtime renderer selection behind the existing OpenGL/Vulkan abstraction.
- Match the current OpenGL behavior:
  - `joints0` and `weights0` drive skinning.
  - skinning is enabled only when the primitive has skinning data and the draw command provides joint matrices.
  - the maximum supported joint matrix count is 96.
- Keep `VulkanGraphicsDevice::draw_mesh` `noexcept`; invalid or unsupported draw commands should skip and/or log safely rather than throwing.
- Continue embedding Vulkan SPIR-V so normal configure/build/test flows do not require a shader compiler.

## Context / Existing State

- `StaticVertex` already includes `joints0` and `weights0` fields.
- `MeshPrimitive` already records `has_skinning`.
- `MeshPrimitiveDrawCommand` already provides `skin_joint_matrices`.
- OpenGL already supports the lightweight skinning path with a 96-joint uniform array.
- Vulkan currently stores `has_skinning` in `MeshPrimitiveRecord`, but `draw_mesh` skips primitives when `primitive.has_skinning` is true or when the draw command has non-empty `skin_joint_matrices`.
- `VulkanDrawPushConstants` is already 128 bytes, so Phase 5E.8 must not attempt to push joint matrices through push constants.

## Files to Inspect Before Editing

- `include/stellar/graphics/GraphicsDevice.hpp`
- `include/stellar/assets/MeshAsset.hpp`
- `include/stellar/graphics/vulkan/VulkanGraphicsDevice.hpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePrivate.hpp`
- `src/graphics/vulkan/VulkanGraphicsDevicePipeline.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceFrame.cpp`
- `src/graphics/vulkan/VulkanGraphicsDeviceResources.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `tests/graphics/VulkanContextSmoke.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `.kilo/plans/1777257747393-clever-tiger.md`
- `Phase5E8.md`

## Implementation Plan

### 1. Add Vulkan Skinning Constants

Add a Vulkan-side constant matching OpenGL:

```cpp
constexpr std::size_t kMaxSkinJoints = 96;
```

Place it near the Vulkan draw/shader constants or another Vulkan-private location where `draw_mesh`, descriptor setup, and shader-layout code can share it as needed.

### 2. Add a GPU-Friendly Skin Matrix Upload Path

Implement a Vulkan descriptor-backed path for joint matrices.

Preferred model:

- Add a vertex-stage descriptor set layout for skinning data.
- Use binding `0` with `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`.
- Make the descriptor visible to `VK_SHADER_STAGE_VERTEX_BIT`.
- Allocate enough storage for 96 `mat4` joint matrices.
- Use per-frame skin uniform buffers to avoid overwriting data used by in-flight frames.
- Use host-visible, host-coherent memory for this phase to match the current simple upload style.
- Track a per-frame skin upload cursor so multiple skinned draw commands in one frame are safe.

At minimum, the implementation must safely handle one or more skinned draw commands per frame and must never let the shader index uninitialized or undersized joint data.

Do not use push constants for joint matrices.

### 3. Add a Transform / Skin Flag Data Path Without Exceeding Push Constants

The existing Vulkan push constants are already 128 bytes. Vulkan skinning also needs the equivalent of OpenGL's:

- `u_mvp`
- `u_model`
- `u_normal_matrix`
- `u_has_skinning`
- `u_joint_matrices[96]`

Do not expand `VulkanDrawPushConstants` beyond portable push-constant limits.

Preferred model:

- Move per-draw transform data into a vertex-stage uniform buffer descriptor.
- Include:
  - `mat4 mvp`
  - `mat4 world`
  - normal transform data, padded according to Vulkan layout rules
  - `uint hasSkinning`
- Keep material factors and material flags in the existing push constants if that path remains valid.

Acceptable alternative:

- Keep `mvp` in push constants.
- Add a separate transform descriptor for `world`, `normal`, and `hasSkinning`.

Whichever model is chosen, ensure the C++ struct layout exactly matches the GLSL/SPIR-V layout.

### 4. Extend Vulkan Pipeline Layout

Update Vulkan pipeline layout creation so the material shader can access both material and skinning/transform data.

Expected descriptor sets:

- set `0`: existing material combined-image-sampler descriptor set
- set `1`: new vertex-stage transform/skinning descriptor set, or separate descriptor sets if the implementation keeps transform and skin data distinct

Static primitives must remain valid.

Preferred static behavior:

- Bind a default identity/no-skin descriptor for static draws.
- Set `hasSkinning = false` for static draws.

Alternative static behavior is acceptable only if it is safe and clearly documented.

### 5. Extend Vulkan Vertex Input Attributes

The Vulkan pipeline must consume the full `StaticVertex` skinning layout.

Keep existing vertex attribute locations unchanged:

| Location | Field | Expected Format |
| --- | --- | --- |
| 0 | `position` | existing position format |
| 1 | `normal` | existing normal format |
| 2 | `uv0` | existing uv0 format |
| 3 | `tangent` | existing tangent format |
| 4 | `uv1` | existing uv1 format |
| 5 | `color` | existing color format |

Add:

| Location | Field | Expected Format |
| --- | --- | --- |
| 6 | `joints0` | `VK_FORMAT_R16G16B16A16_UINT` |
| 7 | `weights0` | `VK_FORMAT_R32G32B32A32_SFLOAT` |

Use `offsetof(stellar::assets::StaticVertex, joints0)` and `offsetof(stellar::assets::StaticVertex, weights0)` for offsets.

### 6. Update / Regenerate Embedded Vulkan Material Vertex SPIR-V

Update the Vulkan material vertex shader logic to match the OpenGL lightweight skinning path.

The vertex shader should implement equivalent logic to:

```glsl
mat4 skin = mat4(1.0);
if (hasSkinning) {
    skin =
        weights0.x * jointMatrices[joints0.x] +
        weights0.y * jointMatrices[joints0.y] +
        weights0.z * jointMatrices[joints0.z] +
        weights0.w * jointMatrices[joints0.w];
}

vec4 localPosition = skin * vec4(position, 1.0);
mat3 skinnedNormalMatrix = mat3(skin);

v_uv0 = uv0;
v_uv1 = uv1;
v_normal = normalize(normalMatrix * skinnedNormalMatrix * normal);
v_tangent = vec4(mat3(model) * skinnedNormalMatrix * tangent.xyz, tangent.w);
v_color = color;
gl_Position = mvp * localPosition;
```

Retain the current fragment shader behavior for material sampling, alpha mask, alpha blend, UV set selection, vertex color, normal map, metallic/roughness, occlusion, and emissive handling.

Continue embedding generated SPIR-V in `VulkanGraphicsDevicePipeline.cpp` unless the repository already has an explicit shader-generation workflow that should be updated.

### 7. Remove the Vulkan Skinned-Primitive Skip

Update `VulkanGraphicsDevice::draw_mesh` so skinned primitives are handled rather than skipped.

Current behavior to replace:

- skip if `primitive.has_skinning` is true
- skip if `command.skin_joint_matrices` is non-empty

New behavior:

#### Static Path

Use when either:

- `primitive.has_skinning == false`, or
- `command.skin_joint_matrices.empty()`

Behavior:

- set `hasSkinning = false`
- bind an identity/no-skin descriptor or otherwise provide safe shader data
- draw normally through the existing material pipeline

#### Skinned Path

Use when:

- `primitive.has_skinning == true`
- `command.skin_joint_matrices` is non-empty
- `command.skin_joint_matrices.size() <= 96`

Behavior:

- upload the joint matrices for the current draw
- bind the skin/transform descriptor data
- set `hasSkinning = true`
- preserve existing material descriptor binding and push constants
- draw indexed as usual

#### Over-Limit Path

Use when:

- `command.skin_joint_matrices.size() > 96`

Preferred behavior:

- skip the draw
- log a clear Vulkan diagnostic such as: `Vulkan skin joint count exceeds 96; skipping skinned primitive`

Do not silently truncate unless the behavior is explicitly documented in completion notes. Do not let the shader index beyond uploaded descriptor data.

### 8. Add Skinning Validation

Add deterministic validation around skinning data.

Recommended checks:

- During `create_mesh`, for primitives with `has_skinning`, validate that joint indices are within the supported 96-joint Vulkan/OpenGL cap.
- Validate that skinning weights are finite.
- Optionally validate that each skinned vertex has a non-zero total weight, but do not reject valid imported assets merely because a vertex is effectively unweighted unless the rest of the project already enforces that.
- During draw, reject or skip if the draw command provides more than 96 matrices.
- If feasible, reject or skip if uploaded joint indices require a matrix beyond the draw command's joint count.

Any failure should return a clear `stellar::platform::Error` from upload-time validation or log/skip safely from draw-time validation.

### 9. Preserve Existing Material Behavior

Skinned draws must continue to use the material path added in earlier Phase 5E work.

Do not regress support for:

- base color factors
- base color textures
- vertex colors
- UV0 / UV1 routing
- normal maps when tangents exist
- metallic/roughness textures and factors
- occlusion textures and strength
- emissive textures and factors
- alpha mask
- alpha blend
- double-sided materials
- static primitive rendering

If the implementation introduces any temporary skinned-material limitation, record it explicitly in completion notes and keep the draw path safe.

### 10. Update Vulkan Context Smoke Test

Update `tests/graphics/VulkanContextSmoke.cpp`.

Add generated coverage for a skinned Vulkan draw:

- Add a skinned triangle primitive or convert one existing primitive into a skinned primitive.
- Set `primitive.has_skinning = true`.
- Fill each skinned vertex with valid values:
  - `joints0 = {0, 0, 0, 0}`
  - `weights0 = {1.0F, 0.0F, 0.0F, 0.0F}`
- Issue a draw command with `skin_joint_matrices` containing one non-identity matrix.
- Keep the test display/Vulkan opt-in only through `STELLAR_ENABLE_VULKAN_CONTEXT_TESTS`.

Recommended additional coverage:

- Submit a draw command with 97 joint matrices and confirm the test does not crash.
- Because `draw_mesh` is `noexcept` and there is no visual readback yet, this can be a smoke-path safety check rather than an assertion-heavy validation.

### 11. Keep Default Tests Green

Run the default build and test suite:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

At minimum, ensure these display-free tests remain green:

```bash
cmake --build build --target \
  stellar_graphics_backend_selection_test \
  stellar_render_scene_inspection_test \
  stellar_render_scene_upload_test \
  stellar_animation_runtime_test \
  -j$(nproc)

ctest --test-dir build -R \
  '^(graphics_backend_selection|render_scene_inspection|render_scene_upload|animation_runtime)$' \
  --output-on-failure
```

### 12. Validate Opt-In Vulkan Path

With a Vulkan-capable test environment:

```bash
cmake -S . -B build-vulkan-tests -DSTELLAR_ENABLE_VULKAN_CONTEXT_TESTS=ON
cmake --build build-vulkan-tests --target stellar_vulkan_context_smoke_test -j$(nproc)
ctest --test-dir build-vulkan-tests -R '^vulkan_context_smoke$' --output-on-failure
```

Manual validation, if representative assets are available:

```bash
./build/stellar-client --renderer vulkan <representative static glTF>
./build/stellar-client --renderer vulkan <representative skinned-or-animated glTF>
```

Expected result:

- static Vulkan scenes remain unchanged from Phase 5E.7 behavior
- skinned Vulkan primitives render instead of being skipped
- animated/skinned poses visibly update
- material, alpha, culling, and texture behavior remains consistent with OpenGL for the lightweight renderer

## Acceptance Criteria

- Vulkan no longer skips valid skinned primitives.
- Vulkan supports `JOINTS_0` / `WEIGHTS_0` through `StaticVertex` locations 6 and 7.
- Vulkan uses a safe descriptor/uniform-buffer path for skin matrices and does not use push constants for the 96-matrix palette.
- Vulkan enforces the same 96-joint cap as OpenGL.
- Static primitives still render through the existing material path.
- Over-limit skin palettes are rejected or skipped safely with a clear diagnostic.
- Default display-free tests still pass.
- Opt-in Vulkan context smoke test covers a generated skinned primitive and passes in a Vulkan-capable environment.
- The implementation updates both this draft plan and `.kilo/plans/1777257747393-clever-tiger.md` with completion notes.

## Completion Notes Required From Codex Agent

When implementation is complete, update both:

1. `Phase5E8.md`
2. `.kilo/plans/1777257747393-clever-tiger.md`

Add completion notes under Phase 5E.8 describing:

- Vulkan skin descriptor/uniform-buffer model chosen
- transform data model chosen
- shader changes made for `joints0` / `weights0`
- how the 96-joint cap is enforced
- how static and skinned primitives are handled
- how over-limit skin palettes behave
- smoke-test updates
- default test command results
- opt-in Vulkan test command results
- any remaining limitations deferred to Phase 5E.9 or Phase 5F

## Suggested Completion Note Template

```markdown
Completion notes (YYYY-MM-DD):
- Added Vulkan skinned draw support for primitives with `JOINTS_0` / `WEIGHTS_0`, matching the current OpenGL 96-joint cap.
- Added vertex-stage skin/transform descriptor resources and per-frame upload handling for joint matrices while keeping material descriptors and existing alpha/culling pipeline variants intact.
- Updated embedded Vulkan material vertex SPIR-V so skinned positions, normals, and tangents follow the same skinning math as the OpenGL lightweight shader.
- Removed the Vulkan skinned-primitive draw skip; static primitives use the no-skin path, skinned primitives bind uploaded joint matrices, and over-limit palettes are rejected/skipped with a clear diagnostic rather than reading invalid descriptor data.
- Extended `vulkan_context_smoke` with a generated skinned primitive and joint matrix draw path while keeping Vulkan context tests opt-in.
- Validated default display-free coverage with: `<commands/results>`.
- Validated opt-in Vulkan coverage with: `<commands/results>`.
- Remaining limitations deferred: larger skin palettes / alternate buffer model for Phase 5F; resize/lifetime hardening remains Phase 5E.9.
```
