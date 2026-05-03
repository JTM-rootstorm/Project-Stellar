# Project Stellar — Black View on Map Load Investigation and Fix Plan

Branch target: `fixes`

## Purpose

Investigate and fix the live-client symptom where a TrenchBroom-authored BSP loads but the view is entirely black. This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

## Current branch facts

- `fixes` is one commit ahead of `main`.
- The only diff from `main` is `tools/trenchbroom/Stellar/stellar_entities.fgd`.
- Therefore, the black-view problem is inherited from current `main`/runtime rendering code, not introduced by the FGD syntax fix.
- The branch already includes mouse/camera-input changes and `STELLAR_DEBUG_CAMERA=1` logging in `Application.cpp`.
- The FGD fix lets the user place light entities, but placing lights does not guarantee visible output if render geometry is culled, lightmaps are black, the camera has no usable snapshot/render view, or visibility/PVS culling rejects all surfaces.

## Most likely causes, ranked

### Cause A — OpenGL back-face culling hides all BSP interior faces

This is the highest-probability cause.

Evidence:
- `OpenGLGraphicsDevice::begin_frame()` clears the frame to black every frame.
- `OpenGLGraphicsDevice::draw_mesh()` enables `GL_CULL_FACE` and culls `GL_BACK` for any material not marked `double_sided`.
- Imported level materials are created with default `double_sided = false`.
- `BspLevelBuilder.cpp` emits triangle fans directly from BSP face polygon order:
  - vertices come from BSP surfedges,
  - primitive indices are `0, i, i + 1`,
  - normals are computed from that order.
- BSP/CSG compilers often emit face winding for solid-outward faces. From inside a sealed room, those can be back-facing. If the engine culls backfaces, the whole interior can disappear, leaving only the black clear color.

Fast isolation:
```bash
STELLAR_DISABLE_GL_CULLING=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If no such flag exists yet, temporarily add one or temporarily call `glDisable(GL_CULL_FACE)` before level draws. If the map becomes visible, implement a permanent BSP interior surface policy.

Preferred permanent fixes:
1. Make imported BSP level materials double-sided by default.
2. Or add `LevelSurfaceMaterial::double_sided` and set it for BSP-imported materials.
3. Or normalize BSP face winding so interior faces are front-facing, then keep culling.

Given BSP rooms are usually viewed from inside closed solids, the simplest robust solution is to make BSP level geometry double-sided until an explicit winding/culling policy exists.

Acceptance:
- The same map is visible without requiring lights.
- Fast compile maps remain visible/fullbright.
- Full/lit maps remain visible and show lightmap variation when present.

### Cause B — Lightmap path multiplies base color by black lightmap data

Evidence:
- The OpenGL shader multiplies base color by the lightmap when `u_has_lightmap_texture` is true.
- If imported lightmap bytes are all zero, the surface can render black unless `u_lightmap_min` is safely nonzero.
- If users compile with Full after adding lights but the compiler did not produce useful lighting, the importer can still bind lightmaps that sample black.

Fast isolation:
```bash
# Compare no-lightmap or debug-fullbright paths.
STELLAR_DISABLE_LIGHTMAPS=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
STELLAR_FORCE_FULLBRIGHT=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If these flags do not exist yet, add them as temporary diagnostics.

Preferred permanent fixes:
- Keep unlit/fullbright fallback for missing or invalid lightmaps.
- Add import diagnostics that print raw lighting lump size, lightmap count, min/max/average RGB, and all-black status.
- Warn when a lightmap is all black.
- Ensure `u_lightmap_min` is explicitly set to a sane value or document why it is zero.

Acceptance:
- Full/VHLT lit fixture has nonzero lightmap byte stats.
- Fast/no-lightmap compile remains visible.
- All-black lightmaps are distinguishable from "nothing rendered."

### Cause C — No latest snapshot or render view is set

Evidence:
- `Application.cpp` clears render view and presentation state when latest snapshot is absent.
- `LevelRenderer` falls back to a fitted camera if render view is absent, which should still render static geometry if geometry exists.
- If renderer initialization has no level geometry or the level asset is missing, the screen remains clear black.

Fast isolation:
```bash
STELLAR_DEBUG_CAMERA=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

Add one-time startup log:
- `validation.level.has_value`
- mesh count
- primitive/index count
- surface count
- material count
- image/texture/lightmap counts
- whether `networked_runtime` exists
- whether `latest_snapshot` appears within the first N frames

Acceptance:
- Logs distinguish no geometry, no snapshot, zero draw calls, and draw calls producing black pixels.

### Cause D — Visibility/PVS culling rejects every surface

Evidence:
- `RenderLevel::queue_static_draws()` computes a PVS-driven visible surface mask when camera position is present.
- If PVS data is imported and camera leaf is found, an all-false mask can skip every surface.

Fast isolation:
```bash
STELLAR_DISABLE_VISIBILITY_CULLING=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

Preferred permanent fixes:
- If PVS resolves to zero visible surfaces while the level has surfaces, fall back to all-visible and emit a debug warning.
- Add diagnostics for camera position, leaf index, leaf surface count, and visible surface count.
- Add tests for malformed/empty PVS rows falling back to visible rather than black.

### Cause E — Shader/context/draw errors are swallowed

Evidence:
- Default tests use mock/recording graphics devices and prove upload/draw dispatch, not actual GPU fragments.
- The optional OpenGL smoke test currently verifies black clear readback, not rendering a mesh.
- If VAO, shader, uniforms, texture binding, or draw path fails, the result can also be black.

Fast isolation:
- Add `glGetError()` checks behind `STELLAR_DEBUG_GL=1`.
- Add an opt-in OpenGL readback test that draws one known colored triangle and asserts a nonblack pixel.
- Add an opt-in BSP readback smoke that renders a tiny imported level and asserts a nonblack pixel.

## Immediate diagnostic sequence

### 1. Confirm map validates

```bash
build/stellar-client --validate-map maps/compiled/test_room.bsp
build/stellar-server --validate-config --map maps/compiled/test_room.bsp
```

### 2. Confirm display path

```bash
build/stellar-client --validate-display --renderer opengl
build/stellar-client --validate-display --renderer vulkan
```

Use OpenGL first unless the user explicitly tests Vulkan.

### 3. Run with camera diagnostics

```bash
STELLAR_DEBUG_CAMERA=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

Expected for `info_player_start origin "0 0 36"`:
- player center z near `36`
- camera eye z near `72`
- target not equal eye

### 4. Add `STELLAR_DEBUG_RENDER=1`

Log once after renderer initialization:
```text
level.source_uri
geometry.meshes.size
sum primitive count
sum index count
geometry.surfaces.size
geometry.materials.size
geometry.images.size
geometry.textures.size
geometry.lightmaps.size
geometry.raw_lighting.size
visibility.available
visibility.leaves.size
level_collision meshes/triangles
```

Log for first few frames:
```text
render_view present?
camera_world_position present?
visible surface count if PVS used
opaque draw count
blend draw count
```

### 5. Add and test culling override first

Implement one temporary toggle:
```text
STELLAR_DISABLE_GL_CULLING=1
```

Test:
```bash
STELLAR_DISABLE_GL_CULLING=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If visible, prioritize Cause A.

### 6. Add and test lightmap override second

Implement:
```text
STELLAR_DISABLE_LIGHTMAPS=1
STELLAR_FORCE_FULLBRIGHT=1
```

Test:
```bash
STELLAR_DISABLE_LIGHTMAPS=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
STELLAR_FORCE_FULLBRIGHT=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If visible only with these flags, prioritize Cause B.

### 7. Add and test PVS override third

Implement:
```text
STELLAR_DISABLE_VISIBILITY_CULLING=1
```

Test:
```bash
STELLAR_DISABLE_VISIBILITY_CULLING=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If visible, prioritize Cause D.

## Permanent implementation recommendation

### Phase 1 — Make black screen diagnosable

Implement:
- `STELLAR_DEBUG_RENDER=1`
- Optional `STELLAR_DEBUG_RENDER_FRAMES=N`
- Render stats and draw counts for first N frames
- Importer lightmap stats under `--validate-map` or a new `--inspect-map`

Acceptance:
- A black screen can be classified as no geometry, no snapshot, no draw calls, all surfaces culled, all-black lightmaps, or GPU draw failure.

### Phase 2 — Fix BSP interior culling

Preferred minimal permanent fix in `src/graphics/RenderLevel.cpp`:
```cpp
stellar::assets::MaterialAsset material_asset_for_level_surface(
    const stellar::assets::LevelSurfaceMaterial& surface_material) {
  stellar::assets::MaterialAsset material;
  ...
  material.unlit = true;
  material.double_sided = true; // BSP interiors must be visible from inside.
  return material;
}
```

Add comment:
```cpp
// Classic BSP rooms are often viewed from inside closed solids. Until the importer
// normalizes face winding for an explicit interior-rendering convention, render
// BSP level surfaces double-sided to avoid black interiors caused by back-face culling.
```

Add tests:
- Display-free: RenderLevel material upload for level surfaces has `double_sided == true`.
- Optional OpenGL readback: inside a box renders nonblack with culling on because material disables culling.

### Phase 3 — Guard against black lightmaps

Implement:
- Lightmap min/max/average/all-black stats.
- Warning on all-black lightmap data.
- `STELLAR_DISABLE_LIGHTMAPS=1`.
- Consider a small `u_lightmap_min` visibility floor or keep zero but expose diagnostics.

Tests:
- Synthetic all-black lightmap emits warning/diagnostic.
- Nonzero lightmap remains bound and sampled.
- Materials with no lightmap remain fullbright/unlit.

### Phase 4 — PVS safety fallback

Implement:
- Count true bits in `visible_surfaces`.
- If the level has surfaces and visible count is zero, clear the mask and draw all surfaces, with debug warning.
- Add `STELLAR_DISABLE_VISIBILITY_CULLING=1`.

Tests:
- Malformed/all-zero PVS does not black out the map.
- Valid PVS still culls.

### Phase 5 — Add GPU readback tests

Opt-in only:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build -j$(nproc)
STELLAR_RUN_OPENGL_CONTEXT_TESTS=1 ctest --test-dir build -R opengl --output-on-failure
```

Extend tests:
- clear black pixel still passes.
- known colored triangle produces nonblack pixel.
- minimal imported level produces nonblack pixel.

## Acceptance criteria

### Default automated

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'bsp_|render_level|player_presentation|client_map_validation|client_connect|networked_client_runtime|fgd|trenchbroom' --output-on-failure
```

Required:
- FGD still loads in TrenchBroom.
- Existing map validation passes.
- RenderLevel material upload proves BSP surfaces are double-sided or culling-safe.
- Lightmap diagnostics tests pass.
- No default CI display/GPU dependency is added.

### Manual

Compile and run a known map:
```bash
tools/bsp/compile_trenchbroom_bsp30.sh   --map tools/trenchbroom/Stellar/templates/lit_zup_room.map   --out maps/compiled/lit_zup_room.bsp   --profile full   --toolchain vhlt

build/stellar-client --validate-map maps/compiled/lit_zup_room.bsp

STELLAR_DEBUG_CAMERA=1 STELLAR_DEBUG_RENDER=1   build/stellar-client --renderer opengl --map maps/compiled/lit_zup_room.bsp
```

Expected:
- logs show nonempty geometry and draw calls.
- camera eye is near `z = 72`.
- room is visible.
- mouse/look controls still work.
- full lighting shows visible variation if VHLT/hlrad produced nonzero lightmaps.

## Quick likely fix to try first

Patch `material_asset_for_level_surface()` to set:

```cpp
material.double_sided = true;
```

Then rebuild and test:
```bash
cmake --build build -j$(nproc)
build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
```

If this fixes black view, still complete diagnostics and tests so the regression is caught.

## Do not do in this slice

- Do not change gameplay authority.
- Do not change TrenchBroom spawn convention.
- Do not remove lightmap support entirely.
- Do not require GPU/display for default CI.
- Do not silently swallow render failures; add diagnostics.
- Do not make PVS culling authoritative for gameplay.
