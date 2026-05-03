# TB-FULL-04 — BSP Lighting and Renderer Lightmap Completion

Target branch: `trenchbroom-compat`

## Goal

Make TrenchBroom-authored lights produce visible lighting in the engine. This requires FGD light classes, VHLT lightmap generation fixtures, BSP lightmap import validation, and OpenGL/Vulkan lightmap sampling.

## Key files to inspect first

- `tests/import/bsp/BspLightmaps.cpp`
- `include/stellar/assets/LevelAsset.hpp`
- `include/stellar/assets/MeshAsset.hpp`
- `include/stellar/graphics/MaterialUpload.hpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/graphics/RenderLevel.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/vulkan/*`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/RecordingGraphicsDevice.hpp`
- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/run_vhlt_fixture_matrix.sh`

## Current expected state

The repo already has source-neutral lightmap structures and tests that verify valid lighting lump data creates `LevelLightmap` metadata, secondary UVs, and material `lightmap_index`. Complete this pipeline through actual render backend sampling and TrenchBroom/VHLT fixtures.

## Tasks

### TB-FULL-04.1 — Finalize light entity FGD and compile fixture

Dependency: TB-FULL-02 light FGD classes.

Implement or update source fixture:

- `tests/fixtures/trenchbroom/src/lit_zup_room.map`

Fixture requirements:

- Sealed 192x192x96 Z-up room.
- Developer materials.
- `info_player_start` at `0 0 36`.
- At least one `light` entity.
- At least one `light_spot` entity if VHLT supports it in the selected config.
- No absolute WAD paths.

Acceptance:

- Source preflight passes.
- VHLT matrix compiles fixture when tools are available.
- Compiled BSP has nonempty lighting lump and at least one imported `LevelLightmap`.

### TB-FULL-04.2 — Audit and complete BSP lightmap import

Implement or verify:

- Correct lightmap dimensions from BSP face extents and texinfo.
- Correct `uv1` generation for every vertex.
- Correct handling of faces with no lighting or invalid offsets.
- Correct image format and color space for lightmaps.
- Correct `LevelSurfaceMaterial::lightmap_index` assignment.
- Correct behavior when multiple faces share material texture but have distinct lightmaps.
- Preserve classic BSP light style index.

Add tests:

- Multiple faces with multiple lightmaps.
- Face with texture + lightmap uses both `uv0` and `uv1`.
- Invalid light offset warning does not crash and falls back safely.
- VHLT compiled lit fixture imports at least one lightmap.

Acceptance:

```bash
ctest --test-dir build -R 'bsp_lightmaps|bsp_importer|bsp_materials' --output-on-failure
```

### TB-FULL-04.3 — Wire lightmap materials through RenderLevel

Current `RenderLevel` resolves `MaterialUpload.lightmap_texture`. Ensure this remains correct after material/import changes.

Implement/verify:

- Lightmap image upload uses linear color space.
- Lightmap binding uses texture coordinate set 1.
- Lightmap sampler clamps and uses linear filtering.
- Materials with both base color texture and lightmap texture upload both bindings.
- Materials without lightmap keep fullbright/unlit fallback behavior.

Add display-free RecordingGraphicsDevice test:

- Build a fake `LevelAsset` with base texture + lightmap.
- Initialize `RenderLevel`.
- Assert two textures uploaded.
- Assert material upload contains `base_color_texture` and `lightmap_texture`.
- Assert `lightmap_texture.texcoord_set == 1`.

Acceptance:

```bash
ctest --test-dir build -R 'render_level_inspection' --output-on-failure
```

### TB-FULL-04.4 — Implement OpenGL lightmap shader sampling

Implement:

- Add `u_lightmap_texture`, `u_has_lightmap_texture`, and `u_lightmap_texcoord_set` uniforms.
- Bind lightmap texture to a stable texture unit after existing material textures.
- Sample lightmap from `uv1` by default.
- Classic BSP behavior: final static surface color should multiply base color/texture by lightmap intensity.
- Preserve alpha mode behavior.
- Preserve vertex color behavior.
- For materials without lightmaps, keep existing fallback visual behavior.
- Avoid making renderer-owned state gameplay authority.

Recommended shader behavior:

```glsl
vec3 lit_rgb = color.rgb;
if (u_has_lightmap_texture) {
    vec3 lm = texture(u_lightmap_texture, uv_for_set(u_lightmap_texcoord_set)).rgb;
    lit_rgb *= max(lm * u_lightmap_intensity, vec3(u_lightmap_min));
} else if (!u_unlit) {
    // Existing directional fallback path.
}
frag_color = vec4(lit_rgb + emissive, color.a);
```

Tune constants to match current engine style, not physically based rendering.

Acceptance:

- OpenGL compile succeeds.
- Existing OpenGL context smoke remains opt-in.
- Display-free material upload tests prove lightmap bindings reach the device.
- Manual lit fixture visually shows light/dark variation in OpenGL.

### TB-FULL-04.5 — Implement Vulkan lightmap sampling parity

Implement Vulkan parity for the same material contract:

- Descriptor layout includes lightmap texture/sampler or a safe default texture.
- Pipeline/shader code samples secondary UVs.
- Material upload records and draw binding logic pass lightmap binding.
- Behavior matches OpenGL for base texture * lightmap.
- Existing Vulkan context tests remain opt-in.

Acceptance:

- Vulkan code builds.
- Display-free backend selection/render tests pass.
- Optional Vulkan context smoke passes when enabled on capable system.
- Manual lit fixture visually shows light/dark variation in Vulkan if user runs it.

### TB-FULL-04.6 — Add lightstyle baseline support

Implement minimal but complete static support:

- Import classic style index already stored on `LevelLightmap`.
- Add a render-time lightstyle multiplier table initialized to `1.0` for all styles.
- Style 0 renders normally.
- Nonzero styles render with stable multiplier unless/until game logic animates them.
- Document that compiled lightmap texture data is used; no dynamic realtime lights are required for TrenchBroom compatibility.

Acceptance:

- Nonzero style fixture imports and renders deterministically.
- No crash or missing binding on nonzero light styles.

## Documentation updates

Update:

- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `docs/ImplementationStatus.md`
- Fixture README

Required content:

- How to place `light`, `light_spot`, `light_environment`.
- How VHLT produces BSP lighting.
- How to validate lit maps.
- How renderer handles lightmaps and fallback unlit surfaces.
- Manual visual checklist.

## Phase done checklist

- [ ] Lit source fixture exists.
- [ ] VHLT lit compile works when tools are available.
- [ ] BSP lightmap import handles multi-face/missing/invalid cases.
- [ ] RenderLevel uploads and binds lightmaps.
- [ ] OpenGL samples lightmaps.
- [ ] Vulkan samples lightmaps with parity.
- [ ] Docs and tests prove lit maps are supported.
