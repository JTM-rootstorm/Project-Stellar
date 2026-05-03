# Project Stellar — Texture Mismatch and Lighting Not Working Plan

Target branch: `fixes`

## User report

- The room now displays after the black-screen/culling fix.
- Runtime textures do not match TrenchBroom.
- Lighting does not visibly work in either Fast or Full compile profiles.

## Working diagnosis

These are probably two separate issues:

1. **Texture mismatch is likely caused by the VHLT wrapper stripping Valve 220 texture axes.**
   - `compile_vhlt_bsp30.sh` rewrites TrenchBroom Valve 220 face syntax from:
     `( p1 ) ( p2 ) ( p3 ) texture [ u-axis ] [ v-axis ] rotation scale_x scale_y`
     to:
     `( p1 ) ( p2 ) ( p3 ) texture 0 0 rotation scale_x scale_y`
   - This destroys TrenchBroom-authored texture axes, shifts, and wall vertical mapping.
   - Texture names may need alias rewrite, but texture axes should not be discarded by default.

2. **Fast compile is not expected to bake lighting.**
   - Fast is useful for geometry iteration.
   - Full only bakes lighting if the selected toolchain actually runs VHLT `hlvis` and `hlrad`.

3. **The current TrenchBroom Full profile does not force VHLT.**
   - `CompilationProfiles.cfg` passes only `--profile full`.
   - `compile_trenchbroom_bsp30.sh` defaults to `--toolchain auto`.
   - Auto can choose a single compiler first when `STELLAR_BSP30_COMPILER` or `QBSP` is set.
   - If a single compiler is selected, `hlrad` will not run and lights will not bake.

4. **Developer texture sources may be inconsistent.**
   - `create_stellar_dev_wad.py` generates all WAD textures as 128x128.
   - `DeveloperTextures.cpp` fallback textures use semantic sizes like 32x32, 64x64, 24x96.
   - Runtime appearance can differ depending on whether WAD pixels or fallback pixels are used.

## Files to inspect first

- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/bsp/create_stellar_dev_wad.py`
- `tools/trenchbroom/Stellar/materials/StellarDevMaterials.txt`
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/import/bsp/DeveloperTextures.cpp`
- `src/import/bsp/Wad3Reader.cpp`
- `src/graphics/RenderLevel.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `tests/import/bsp/BspMaterials.cpp`
- `tests/import/bsp/BspLightmaps.cpp`
- `tests/fixtures/trenchbroom/src/lit_zup_room.map`
- `docs/TrenchBroom.md`

## Phase 0 — Capture exact compile behavior

Run and keep logs:

```bash
env | grep -E 'STELLAR_BSP30|QBSP|VHLT|HLCSG|HLBSP|HLVIS|HLRAD|STELLAR_CLIENT|STELLAR_SERVER' | sort

bash -x tools/bsp/compile_trenchbroom_bsp30.sh   --map maps/src/test_room.map   --out maps/compiled/test_room_fast.bsp   --profile fast 2>&1 | tee build/test_room_fast_compile.log

bash -x tools/bsp/compile_trenchbroom_bsp30.sh   --map maps/src/test_room.map   --out maps/compiled/test_room_full_auto.bsp   --profile full 2>&1 | tee build/test_room_full_auto_compile.log

bash -x tools/bsp/compile_trenchbroom_bsp30.sh   --map maps/src/test_room.map   --out maps/compiled/test_room_full_vhlt.bsp   --profile full   --toolchain vhlt 2>&1 | tee build/test_room_full_vhlt_compile.log
```

Expected:
- Fast should not be considered a lighting profile.
- Full auto may not run `hlrad`.
- Full VHLT must run `hlcsg`, `hlbsp`, `hlvis`, and `hlrad`.

## Phase 1 — Fix TrenchBroom profile ambiguity

Update `tools/trenchbroom/Stellar/CompilationProfiles.cfg`.

Add explicit profile:

```json
{
  "name": "Stellar BSP30 Full Lighting",
  "tasks": [
    {
      "target": "${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp",
      "type": "tool",
      "tool": "Stellar BSP30 compile wrapper",
      "parameters": "--map \"${MAP_FULL_PATH}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile full --toolchain vhlt"
    }
  ]
}
```

Recommended profile set:
- `Stellar BSP30 Fast` — quick no-light iteration.
- `Stellar BSP30 Full Lighting` — VHLT + `hlrad`, for baked lights.
- `Stellar Validate Existing BSP30`.

Acceptance:
- Running the Full Lighting profile creates logs for `hlcsg`, `hlbsp`, `hlvis`, and `hlrad`.
- Docs state Fast does not bake lighting.

## Phase 2 — Preserve Valve 220 texture axes

In `tools/bsp/compile_vhlt_bsp30.sh`, split the current rewrite behavior.

Current behavior to remove from default path:
- converting `[u-axis] [v-axis]` to `0 0`.

Keep only texture-name alias rewrite:
- `dev/grid_32` -> `dev_grid_32`
- `dev/wall_96` -> `dev_wall_96`
- etc.

Recommended functions:
- `rewrite_vhlt_texture_names_only`
- optional `rewrite_valve220_to_classic_texture_axes` behind an explicit non-default flag.

Acceptance:
- Preserved VHLT work map still contains `[` and `]` Valve 220 axis vectors.
- VHLT still compiles.
- Runtime wall/floor texture alignment matches TrenchBroom better.
- No default path silently discards texture axes.

## Phase 3 — Add texture-axis regression fixture

Create a fixture map with:
- floor using X/Y axes
- north/south wall using X/Z axes
- east/west wall using Y/Z axes
- nonzero texture shift
- non-1 texture scale

Tests:
- preflight passes.
- wrapper preserved work map keeps Valve 220 axes.
- compiled BSP texinfo produces expected UVs.
- importer `uv0` min/max matches expected values.
- test fails if work-map face lines become `texture 0 0 rotation scale scale`.

## Phase 4 — Unify developer texture sources

Decide one official visual source.

Recommended:
- Make procedural fallback in `DeveloperTextures.cpp` match the generated WAD output exactly, or vice versa.
- Add manifest tests so WAD decoded texture dimensions and fallback dimensions match.
- Update `BspMaterials.cpp` expectations.

Current concern:
- WAD generator uses 128x128 for all dev textures.
- fallback grid/wall textures use semantic dimensions such as 32x32, 64x64, 24x96.
- Runtime can differ from TrenchBroom depending on WAD resolution and fallback path.

Acceptance:
- `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` look the same whether loaded from WAD or fallback.
- TrenchBroom-visible material previews match runtime intent.
- Docs clearly define whether names describe texture dimensions or grid spacing.

## Phase 5 — Make lighting diagnosable

Add diagnostics to map validation/import:
- raw lighting lump byte count
- lightmap count
- per-lightmap dimensions
- min/max/average RGB
- all-black flag
- material-to-lightmap bindings

If helpers already exist in `BspLevelBuilder.cpp`, verify they are actually called before returning the level asset.

Add runtime debug flags:
- `STELLAR_DEBUG_LIGHTMAPS=1`
- `STELLAR_DISABLE_LIGHTMAPS=1`
- `STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1`
- `STELLAR_FORCE_FULLBRIGHT=1`

Acceptance:
- `build/stellar-client --validate-map <full-vhlt.bsp>` can prove whether lightmaps were imported.
- Full Lighting profile produces nonzero lightmap stats for the lit fixture.
- If lightmaps are missing or all black, the user gets a clear diagnostic instead of guessing.

## Phase 6 — Verify renderer lightmap usage

Audit:
- `RenderLevel` uploads lightmap textures as linear.
- `lightmap_texture.texcoord_set == 1`.
- shader binds `u_lightmap_texture`.
- shader sets `u_lightmap_intensity`.
- shader sets `u_lightmap_min`.
- materials without lightmap remain fullbright/unlit.

Add tests:
- fake level with base texture + lightmap reaches `MaterialUpload`.
- `STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1` renders lightmap RGB directly in optional GPU smoke.
- `STELLAR_DISABLE_LIGHTMAPS=1` shows base textures only.

## Phase 7 — Check VHLT light entity compatibility

Inspect `hlrad.log` and entity text:

```bash
strings maps/compiled/test_room_full_vhlt.bsp | grep -E '"classname"|"light"|"_light"|"origin"'
sed -n '1,220p' build/tests/fixtures/trenchbroom/vhlt/logs/test_room_full/hlrad.log
```

Confirm:
- `light` and/or `light_spot` are present before `hlrad`.
- `hlrad` reports lights found or direct lighting work.
- If VHLT expects `_light` over `light`, update FGD/templates to use `_light "255 255 255 300"` while keeping `light` compatibility.

## Validation checklist

Default:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'bsp_materials|bsp_lightmaps|bsp_importer|render_level|trenchbroom|fgd|client_map_validation' --output-on-failure
```

VHLT matrix:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh   --source-root .   --build-root build   --profile full   --keep-going
```

Manual:

```bash
STELLAR_VHLT_KEEP_WORK=1 tools/bsp/compile_trenchbroom_bsp30.sh   --map tools/trenchbroom/Stellar/templates/lit_zup_room.map   --out maps/compiled/lit_zup_room_full.bsp   --profile full   --toolchain vhlt

build/stellar-client --validate-map maps/compiled/lit_zup_room_full.bsp

STELLAR_DEBUG_CAMERA=1 STELLAR_DEBUG_TEXTURES=1 STELLAR_DEBUG_LIGHTMAPS=1 build/stellar-client --renderer opengl --map maps/compiled/lit_zup_room_full.bsp
```

Manual acceptance:
- runtime texture orientation and scale match TrenchBroom.
- `dev/wall_96` maps vertically on walls.
- Full Lighting profile produces nonzero lightmap stats.
- lit room shows visible light/dark variation.
- Fast profile remains visible but is documented as unlit/no-bake.

## Likely first patch sequence

1. Add explicit `Stellar BSP30 Full Lighting` profile with `--toolchain vhlt`.
2. Modify `compile_vhlt_bsp30.sh` to preserve Valve 220 texture axes by default.
3. Add texture and lightmap diagnostics.
4. Unify WAD/fallback texture dimensions and patterns.
5. Add texture-axis fixture tests.
6. Add nonzero-lightmap validation for Full Lighting fixture.
7. Update docs.

## Do not do

- Do not remove Fast profile.
- Do not claim Fast bakes lights.
- Do not silently strip Valve 220 texture axes.
- Do not add GPU/display dependencies to default CI.
- Do not remove deterministic fallback textures.
- Do not change gameplay authority or camera controls while fixing textures/lighting.
