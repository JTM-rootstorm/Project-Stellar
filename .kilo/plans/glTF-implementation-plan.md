# glTF Support Continuation Plan

## Status

Static glTF work has been split into implementation and validation milestones. The current
branch contains uncommitted work through Phase 2C.

Completed so far:
- Phase 1A: backend-neutral texture/sampler asset model, material texture slots, importer
  texture/sampler population, glTF build gating, and initial client asset-path seam.
- Phase 1B: runtime static scene loading through `--asset`, generic `SceneRenderer`,
  `RenderScene` material upload binding, OpenGL base-color texture rendering, alpha mode
  handling, double-sided culling, sampler propagation, and mock upload tests.
- Phase 1C: normal and metallic-roughness material upload bindings, lightweight material
  shader inputs, simple ambient/directional lighting, transformed normal/tangent handling,
  and expanded material upload/import regression tests.
- Phase 1D: representative validation coverage, `--validate-config`, backend-neutral client
  config validation, isolated asset validation smoke tests, static glTF fixture coverage, and
  minimal `.glb` smoke coverage.
- Phase 2A: static importer completeness, including authored tangent import, primitive bounds
  computation, stricter mesh accessor/index validation, documented unsupported-feature policy,
  graceful deferral for currently unsupported optional attributes, and generated `.glb` BIN
  payload regression coverage.
- Phase 2B: static material and transparency polish, including backend-neutral texture
  color-space metadata, normal texture scale, occlusion/emissive material inputs, and
  view-space sorted transparent primitive submission.
- Phase 2C: UV and vertex attribute expansion, including static `TEXCOORD_1` import/render
  routing, vertex color import/render modulation, and conservative tangent generation for
  normal-mapped primitives without authored tangents.

Known remaining limitations:
- Normal maps are only active for primitives with `MeshPrimitive::has_tangents = true`; importer
  tangent generation is intentionally disabled if any triangle has invalid UV0 tangent data.
- Renderer supports `TEXCOORD_0` and `TEXCOORD_1`; texture slots requesting texcoord sets above 1
  are treated as unsupported and are not bound.
- Metallic-roughness shading is lightweight and not full glTF PBR.
- Alpha blending uses primitive-center view-space sorting; intersecting or concave transparent
  geometry can still require asset-side splitting or later order-independent transparency.
- Reusing one texture for both color and non-color material slots keeps one upload color-space;
  color usage currently wins until per-use texture views are added.
- Vulkan backend/runtime parity is still absent.
- Animation, skinning, morph targets, cameras, lights, and extensions are not supported.

## Phase 2A: Static Importer Completeness - Completed

Goal: make common static glTF exports from Blender and similar DCC tools import with fewer
silent omissions or incorrect runtime fallbacks.

### Completion Notes

Phase 2A has been completed in the current branch.

- glTF `TANGENT` attributes are imported into `StaticVertex::tangent` when present and valid.
- `MeshPrimitive::has_tangents` is set only after valid tangent data is fully read.
- Primitive `bounds_min` and `bounds_max` are computed from all imported positions, including
  indexed primitives.
- Required draw data now fails hard when invalid, including missing/invalid `POSITION`,
  malformed present `NORMAL`, `TEXCOORD_0`, or `TANGENT`, invalid index accessors,
  out-of-range indices, and unsupported topology.
- Unsupported optional static features without runtime representation, including vertex colors
  and additional UV sets, are ignored predictably for now.
- The static import policy is documented in the importer implementation and covered by
  regression tests.
- Importer regression coverage now includes tangent values, `has_tangents`, non-default
  bounds, hard-failure cases, ignored optional attributes, and generated `.glb` files with
  a BIN chunk containing triangle mesh data.
- `TEXCOORD_1` runtime support was intentionally deferred to a later phase; material texCoord
  metadata remains preserved, while rendering remains limited to `TEXCOORD_0`.

### Tasks

1. Import glTF `TANGENT` attributes.
   - Populate the existing `StaticVertex::tangent` field.
   - Set `MeshPrimitive::has_tangents` only when tangent data is successfully read for the
     primitive.
   - Validate accessor type/count before treating tangents as available.
   - Add regression coverage for tangent values and `has_tangents`.

2. Compute primitive bounds during import.
   - Fill `MeshPrimitive::bounds_min` and `bounds_max` from imported positions.
   - Support indexed and non-indexed primitives.
   - Add regression tests for non-default bounds.

3. Define unsupported-feature policy for static import.
   - Fail hard for data required to draw correctly, such as missing `POSITION`, invalid
     index accessors, invalid component types, and non-triangle topology when no conversion
     exists.
   - Degrade gracefully for currently unsupported optional data such as vertex colors,
     additional UV sets, skins, animations, morph targets, cameras, lights, and extensions.
   - Document the policy in tests or a small implementation note if a suitable code location
     exists.

4. Decide `TEXCOORD_1` handling.
   - Preferred minimal choice: keep runtime support limited to `TEXCOORD_0` for now and add
     explicit importer/render validation that materials using `texCoord > 0` degrade
     predictably.
   - Alternative: extend `StaticVertex` and shader inputs with `uv1`, then route material
     texture slots to the requested coordinate set.
   - This decision should be made before Phase 2B.

5. Add stronger `.glb` regression coverage.
   - Add a generated `.glb` with a BIN chunk containing at least one simple triangle mesh.
   - Verify mesh, primitive, vertex, index, scene, and material import.
   - Keep the fixture generated in test code unless a checked-in binary asset is explicitly
     preferred.

### Validation

- Build/test with `STELLAR_ENABLE_GLTF=ON`.
- Build/test with `STELLAR_ENABLE_GLTF=OFF` to preserve gate behavior.
- Run importer regression and render-scene upload tests.

### Expected Outcome

Static mesh import handles tangents and bounds correctly, has a clear unsupported-feature
policy, and has meaningful `.glb` binary-payload coverage.

## Phase 2B: Static Material and Transparency Polish - Completed

Goal: reduce visible artifacts and improve static asset fidelity without entering animation,
skinning, or full renderer parity work.

### Completion Notes

Phase 2B has been completed in the current branch.

- `RenderScene` now submits opaque/mask primitives before alpha-blend primitives and sorts
  alpha-blend draws back-to-front by primitive bounds center in view space.
- Texture uploads carry backend-neutral color-space metadata: base-color/emissive usage is
  sRGB, while normal, metallic-roughness, and occlusion data remain linear.
- glTF normal texture scale is imported, preserved through material upload, and applied in the
  OpenGL shader when tangent-space normal mapping is active.
- Occlusion and emissive material slots/factors are imported, uploaded, covered by tests, and
  applied with modest shader complexity.
- Display-free validation covers material upload color-space metadata, alpha draw ordering,
  normal scale, and occlusion/emissive import.
- Remaining limitations: transparent sorting is center-depth based and not order-independent;
  one texture reused for color and non-color slots has only one upload color-space; runtime
  sampling remains limited to `TEXCOORD_0`.

### Tasks

1. Decide and implement alpha blend ordering strategy.
   - Minimal option: split opaque/mask and blend primitives into separate submission groups.
   - Sort blend draws back-to-front using camera-space or view-space depth.
   - Keep limitations documented for intersecting transparent geometry.

2. Improve texture color-space handling.
   - Treat base-color textures as sRGB where appropriate.
   - Keep non-color data such as normal and metallic-roughness textures linear.
   - Add backend-neutral metadata if needed, but avoid OpenGL-specific fields in assets.

3. Add normal texture scale support if available in imported material data.
   - Preserve glTF normal texture scale in the material slot or material asset.
   - Apply it in shader when normal mapping is enabled.

4. Add occlusion/emissive material inputs only if they fit the static-material scope.
   - Import and store the data backend-neutrally.
   - Prefer upload/test coverage before visual shader complexity.

5. Expand representative asset validation.
   - Add tests for alpha sorting inputs where possible with mock draw-command inspection.
   - Add importer/material tests for sRGB-vs-linear texture classification if implemented.

### Validation

- Build/test with glTF enabled and disabled.
- Prefer display-free tests for render queue/material classification.
- Keep OpenGL visual tests optional unless CI/runtime environment can guarantee a context.

### Expected Outcome

Static textured assets render with fewer transparency and color-space surprises, and the
material path remains backend-neutral.

## Phase 2C: UV and Vertex Attribute Expansion - Completed

Goal: support additional common static glTF vertex attributes when they materially affect
rendered output.

### Completion Notes

Phase 2C has been completed in the current branch.

- `StaticVertex` now carries backend-neutral `uv1` and RGBA `color` data, and `MeshPrimitive`
  records whether authored vertex colors are present.
- glTF `TEXCOORD_1` is imported and uploaded to OpenGL as a second UV attribute; material texture
  slots with texcoord set 0 or 1 sample UV0/UV1 respectively, while texcoord sets above 1 remain
  predictably unsupported and unbound.
- glTF `COLOR_0` is imported for VEC3/VEC4 float-compatible data; VEC3 colors default alpha to 1,
  and the OpenGL shader multiplies base color by vertex color only for primitives marked with
  `has_colors`.
- If a primitive references a normal texture and has no authored tangents, the importer now
  conservatively generates tangents from positions, normals, indices, and UV0. Tangents are enabled
  only when generation succeeds for the entire primitive.
- Regression coverage now verifies UV1 values, vertex colors, material texcoord routing through
  mock upload, successful generated tangents, and degenerate UVs disabling generated tangents.

### Tasks

1. Add `TEXCOORD_1` support if approved in Phase 2A.
   - Extend `StaticVertex` or introduce a richer static vertex format.
   - Import `TEXCOORD_1`.
   - Update OpenGL vertex layout and shader inputs.
   - Route `MaterialTextureSlot::texcoord_set` to UV0 or UV1.
   - Add regression tests for a material using texCoord 1.

2. Add vertex color support.
   - Decide supported formats, likely `COLOR_0` as vec3/vec4 float-compatible data.
   - Extend vertex asset representation.
   - Multiply base color by vertex color in shader if present.
   - Add regression tests.

3. Revisit tangent generation.
   - If normal maps are common without authored tangents, add tangent generation from
     positions and UVs.
   - Keep this optional and well-tested because incorrect generated tangents can be worse
     than disabling normal maps.

### Validation

- Importer regression for UV1 and vertex colors.
- Render-scene upload or mock rendering tests for material texcoord routing.
- glTF enabled/disabled build verification.

### Expected Outcome

Common static assets using secondary UVs or vertex colors have a correct import and render
path.

## Phase 3A: Animation and Skin Asset Model - Completed

Goal: represent skeletal animation data in CPU-side assets without implementing playback yet.

### Completion Notes

Phase 3A has been completed in the current branch.

- Backend-neutral `SkinAsset` and `AnimationAsset` structures now preserve glTF skins,
  skeleton roots, inverse bind matrices, animation samplers, channels, target paths, and
  interpolation modes without renderer-specific fields.
- `StaticVertex` carries first-set skinning inputs (`JOINTS_0` and normalized `WEIGHTS_0`), and
  `MeshPrimitive::has_skinning` records when paired skin attributes were imported successfully.
- The glTF importer now populates node `skin_index`, skin assets, animation assets, and
  CPU-side keyframe input/output arrays. Missing inverse bind matrices default to identity as
  specified by glTF.
- Skin attribute validation fails clearly for unpaired `JOINTS_0`/`WEIGHTS_0`, unsupported joint
  or weight component types, non-normalized integer weights, zero/negative/non-finite weight
  sums, and malformed inverse bind matrix data.
- Regression coverage includes a generated minimal skinned and animated asset plus invalid skin
  attribute cases. No runtime playback, skin evaluation, CPU/GPU deformation, shader changes, or
  render-path skinning were added.

### Tasks

1. Extend asset model with skin data.
   - Skins, joints, skeleton root, inverse bind matrices.
   - Node `skin_index` assignment during import.

2. Extend mesh vertex data for skinning.
   - Import `JOINTS_0` and `WEIGHTS_0`.
   - Decide whether to add a skinned vertex format or extend `StaticVertex`.
   - Validate normalized weights and supported component types.

3. Add animation asset types.
   - Animation samplers.
   - Channels.
   - Target paths: translation, rotation, scale, weights if morph targets are later allowed.
   - Interpolation modes: linear, step, cubic spline if feasible.

4. Import glTF skins and animations.
   - Populate the new asset structures.
   - Add regression tests for a minimal skinned/animated asset.

### Validation

- CPU importer tests only; no runtime playback required in this phase.
- Verify invalid/missing skin data fails clearly or degrades predictably.

### Expected Outcome

The engine can represent glTF skin and animation data after import, but does not yet animate
or skin meshes at runtime.

## Phase 3B: Runtime Animation Evaluation

Goal: evaluate glTF animations and update scene node transforms over time.

### Tasks

1. Add animation player/evaluator.
   - Sample animation channels by time.
   - Apply interpolation.
   - Write results into mutable runtime node transforms.

2. Add playback controls.
   - Start/stop/loop.
   - Animation selection by index/name.
   - Time advancement independent from rendering frame rate where possible.

3. Add CPU-side skeleton pose update.
   - Compute joint world transforms.
   - Combine with inverse bind matrices.
   - Store final joint matrices for skinning.

4. Add tests.
   - Deterministic CPU tests for interpolation.
   - Node transform update tests.
   - Skeleton pose matrix tests.

### Validation

- Unit tests without GPU/display.
- Ensure static scenes still render unchanged when no animation is active.

### Expected Outcome

Runtime can evaluate glTF animations and compute skeleton poses, but mesh deformation may
still be deferred to Phase 3C.

## Phase 3C: Skinning Render Path

Goal: render skinned glTF meshes.

### Tasks

1. Choose CPU or GPU skinning for first implementation.
   - Recommended: GPU skinning for the render path if shader/uniform limits are acceptable.
   - CPU skinning may be simpler for validation but risks performance debt.

2. Add skinning data upload.
   - Joint matrices per skinned draw.
   - Skinned vertex attributes for joints and weights.

3. Update OpenGL shader path.
   - Apply joint matrices before model/view/projection.
   - Keep static mesh path working.

4. Add tests/validation scenes.
   - CPU-side matrix tests.
   - Minimal runtime validation scene.
   - Optional visual/manual test asset.

### Validation

- glTF enabled build/tests.
- Static rendering regression tests.
- Animation/skinning CPU tests.

### Expected Outcome

Skeletal animated glTF assets can be represented, evaluated, and rendered.

## Phase 4A: Renderer Backend Parity Decision

Goal: reconcile current OpenGL-only runtime implementation with the project design goal of
OpenGL/Vulkan runtime-selectable parity.

### Tasks

1. Decide near-term project stance.
   - Option A: explicitly document glTF runtime rendering as OpenGL-only until Vulkan exists.
   - Option B: start Vulkan backend implementation for the same `GraphicsDevice`,
     `RenderScene`, and `MaterialUpload` model.

2. If documenting OpenGL-only status:
   - Update relevant design/status docs.
   - Keep public interfaces backend-neutral.
   - Ensure CMake/runtime errors are clear when Vulkan is requested in the future.

3. If implementing Vulkan:
   - Add Vulkan graphics device skeleton.
   - Implement mesh, texture, sampler, material, and draw upload parity.
   - Add backend selection via config/CLI.
   - Add parity tests where feasible.

### Validation

- If OpenGL-only: documentation/status review plus build/tests.
- If Vulkan: backend-specific build, runtime initialization, and parity validation.

### Expected Outcome

The project status matches documented renderer promises instead of leaving parity ambiguous.

## Phase 4B: Renderer Quality and Test Infrastructure

Goal: make graphics validation stronger and less dependent on manual visual inspection.

### Tasks

1. Add optional headless/context-capable render tests if environment support exists.
   - Keep disabled or opt-in if CI/display support is not guaranteed.

2. Add draw-command inspection hooks or mockable render queues.
   - Validate alpha grouping, material state, and texture binding decisions without a GPU.

3. Add representative sample assets.
   - Keep them small and license-clean.
   - Prefer generated fixtures for binary edge cases when practical.

4. Add performance sanity tests for static scene upload/render traversal.

### Validation

- Mock tests for deterministic behavior.
- Optional OpenGL/Vulkan runtime tests when available.

### Expected Outcome

Future renderer and glTF work can be validated with less risk of silent visual regressions.
