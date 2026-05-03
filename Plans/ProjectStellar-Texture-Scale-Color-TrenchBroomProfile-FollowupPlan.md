# Project Stellar — Texture Scale/Color and TrenchBroom Profile Follow-up Plan

Target branch: `fixes`

## User-reported issues

1. Pure red/green/blue lights previously created RGB striping. Lighting is now mostly fixed, but keep regression coverage.
2. Runtime textures are still scaled/colored incorrectly compared to TrenchBroom.
   - Screenshot shows `dev/wall_96` on the walls rendering as a small repeated brown checker/brick texture rather than a full 96-inch wall-height reference.
   - Screenshot shows `dev/grid_64` on the floor rendering as a very dark blue/black grid rather than the expected editor-visible material.
3. TrenchBroom clears `CompilationProfiles.cfg` or fails to execute tool references such as `Stellar BSP30 compile wrapper`.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

## Current branch facts

`fixes` currently includes:
- double-sided BSP materials,
- lightmap/render debug flags,
- `STELLAR_DISABLE_LIGHTMAPS`,
- `STELLAR_FORCE_LIGHTMAP_VISUALIZATION`,
- VHLT texture-axis preservation by default,
- a 128x128 procedural developer texture fallback,
- a `Stellar BSP30 Full Lighting` profile that passes `--toolchain vhlt`.

Important files:
- `src/import/bsp/BspLevelBuilder.cpp`
- `src/import/bsp/DeveloperTextures.cpp`
- `src/import/bsp/Wad3Reader.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/RenderLevel.cpp`
- `tools/bsp/create_stellar_dev_wad.py`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/trenchbroom/Stellar/GameConfig.cfg`
- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/trenchbroom/Stellar/materials/StellarDevMaterials.txt`
- `tools/trenchbroom/test_stellar_package_paths.sh`

## Highest-priority findings

### Finding A — Texture patterns may now match WAD but not intended dev-texture semantics

`DeveloperTextures.cpp` now uses one 128x128 texture size for all dev aliases. That matches the WAD generator’s fixed `TEXTURE_SIZE = 128`, but it changes the meaning of names like `dev/wall_96` and `dev/grid_64`.

For `dev/wall_96`, the current fallback/WAD pattern is a generic 128x128 brick pattern. It is not a 96-inch wall-height reference with a full-height marker. The screenshot matches this: the walls show repeated small brown checker/brick cells, not a single 96-inch height cue.

For `dev/grid_64`, the current palette is intentionally very dark with blue grid lines. The screenshot floor looks like that generated palette, but this may not match the TrenchBroom PNG preview or desired Source-style developer material color.

Required decision:
- Decide whether dev texture names are semantic visual references or generic generated texture IDs.
- User expectation appears semantic:
  - `wall_96` should show a full 96-inch wall-height reference.
  - `grid_64` should visibly represent a 64-inch grid, likely closer to the editor preview.

Recommended fix:
1. Make the WAD generator, runtime fallback, and PNG previews share one authoritative generator.
2. For `dev/wall_96`, create a 128x128 or 64x128 texture that clearly encodes 96 inches:
   - vertical reference marker,
   - 12/16/32/64/96 labels or stripe ticks if possible,
   - visually distinct wall color.
3. For `dev/grid_64`, choose a readable floor grid palette:
   - avoid near-black base if this is meant for editor/runtime geometry validation,
   - make 64-inch major grid clearly visible,
   - optionally use orange/blue dev colors similar to common dev textures.
4. Regenerate:
   - `materials/stellar_dev.wad`
   - any PNG previews under `tools/trenchbroom/Stellar/materials/`
   - procedural fallback in `DeveloperTextures.cpp`
5. Add tests proving WAD and fallback pixels match for sample coordinates.

Acceptance:
- `dev/wall_96` on a 96-inch high wall shows the intended full wall-height reference.
- `dev/grid_64` on the floor is visibly the same color/pattern as TrenchBroom preview.
- The same material looks the same whether loaded from WAD or procedural fallback.

### Finding B — Texture scale may be wrong because UVs divide by 128 for semantic textures

`BspLevelBuilder.cpp` computes:
```cpp
uv0 = s / texture_width, t / texture_height
```

If `dev/wall_96` is now 128 pixels high but the semantic wall reference is 96 inches high, the wall will not map as expected unless either:
- the texture really is 96 pixels high, or
- the TrenchBroom texture scale accounts for 128 pixels, or
- the runtime applies semantic scale metadata.

Potential causes:
- TrenchBroom preview PNG and WAD texture dimensions differ.
- The compiled BSP uses WAD dimension 128x128 while the original editor face alignment expected a different image size.
- The test BSP was not recompiled after texture-axis preservation.
- Runtime fallback dimensions differ from the actual compiled miptex dimension.

Required diagnostics:
1. Add/finish `STELLAR_DEBUG_TEXTURES=1`.
2. For every surface, log:
   - source texture name,
   - loaded image dimensions,
   - texture source: embedded miptex, WAD, procedural fallback,
   - texinfo s/t vectors,
   - uv0 min/max,
   - material index.
3. Run:
   ```bash
   STELLAR_DEBUG_TEXTURES=1 build/stellar-client --validate-map maps/compiled/test_room.bsp
   STELLAR_DISABLE_LIGHTMAPS=1 STELLAR_DEBUG_RENDER=1 build/stellar-client --renderer opengl --map maps/compiled/test_room.bsp
   ```

Acceptance:
- `wall_96` image dimensions and UV repeats explain the rendered wall scale.
- If wall height is 96 inches and the intended texture is one wall-height repeat, uv0/uv1 should reflect exactly one intended repeat over height.

### Finding C — Runtime may still be loading WAD/fallback while TrenchBroom displays PNG

TrenchBroom may show PNG previews from `materials/dev/...`, while runtime loads:
1. embedded miptex pixels,
2. external WAD pixels,
3. procedural fallback pixels.

If these are generated independently, they can differ.

Required fix:
- Make one generator produce all three outputs:
  - WAD miptex data,
  - PNG preview files,
  - C++ fallback pixel data or a generated manifest consumed by C++ tests.
- Add a test that reads/generated compares sample colors for each dev texture across WAD and fallback.
- If PNG verification in CI is not practical, at least generate a manifest with dimensions and sample coordinate colors.

Acceptance:
- TrenchBroom preview and runtime pixels are derived from the same definitions.

### Finding D — OpenGL RGB unpack alignment fix should remain covered

The reported lightmap striping strongly indicated RGB row unpack alignment. Even if lighting is now fixed, keep/verify the fix.

Required regression test:
- Upload an RGB texture whose `width * 3` is not divisible by 4, such as 3x2 or 5x2.
- Read it back or render/sample it in an opt-in OpenGL test.
- Assert rows/channels are not shifted.

Acceptance:
- Pure RGB lightmaps remain channel-correct.

### Finding E — TrenchBroom compilation profile schema is likely still invalid

Current package uses tool names with spaces:
```json
"tool": "Stellar BSP30 compile wrapper"
```

TrenchBroom clearing profiles suggests schema/tool substitution mismatch. It likely expects variable-style tool references, for example:
```json
"tool": "${STELLAR_BSP30_COMPILE}"
```

Required fix:
1. Change `GameConfig.cfg` compilation tool names to variable-safe identifiers:
   ```json
   "compilationTools": [
     { "name": "STELLAR_BSP30_COMPILE", "path": "bin/stellar_tb_compile.sh" },
     { "name": "STELLAR_BSP30_VALIDATE", "path": "bin/stellar_tb_validate.sh" }
   ]
   ```
2. Change `CompilationProfiles.cfg`:
   - add top-level `"version": 1`,
   - use `"tool": "${STELLAR_BSP30_COMPILE}"`,
   - use `"tool": "${STELLAR_BSP30_VALIDATE}"`,
   - add `"workdir": "${WORK_DIR_PATH}"` if required by the installed TrenchBroom version.
3. Update package tests so the old display-name style fails.

Candidate profile:
```json
{
  "version": 1,
  "profiles": [
    {
      "name": "Stellar BSP30 Fast",
      "workdir": "${WORK_DIR_PATH}",
      "tasks": [
        {
          "type": "tool",
          "tool": "${STELLAR_BSP30_COMPILE}",
          "parameters": "--map \"${MAP_FULL_PATH}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile fast"
        }
      ]
    },
    {
      "name": "Stellar BSP30 Full Lighting",
      "workdir": "${WORK_DIR_PATH}",
      "tasks": [
        {
          "type": "tool",
          "tool": "${STELLAR_BSP30_COMPILE}",
          "parameters": "--map \"${MAP_FULL_PATH}\" --out \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\" --profile full --toolchain vhlt"
        }
      ]
    },
    {
      "name": "Stellar Validate Existing BSP30",
      "workdir": "${WORK_DIR_PATH}",
      "tasks": [
        {
          "type": "tool",
          "tool": "${STELLAR_BSP30_VALIDATE}",
          "parameters": "--map \"${WORK_DIR_PATH}/${MAP_BASE_NAME}.bsp\""
        }
      ]
    }
  ]
}
```

Acceptance:
- TrenchBroom no longer clears profiles.
- Profiles appear after package reload/restart.
- Fast and Full Lighting execute the package-local shims.
- Copied and linked packages both work.

## Immediate debug commands

### Texture source and scale

```bash
# Recompile after current fixes; avoid stale BSPs.
STELLAR_VHLT_KEEP_WORK=1 tools/bsp/compile_trenchbroom_bsp30.sh   --map maps/src/test_room.map   --out maps/compiled/test_room_full_lighting.bsp   --profile full   --toolchain vhlt

# Inspect texture names in BSP.
strings maps/compiled/test_room_full_lighting.bsp | grep -E 'dev_|dev/|stellar_dev'

# Inspect runtime texture import.
STELLAR_DEBUG_TEXTURES=1 build/stellar-client --validate-map maps/compiled/test_room_full_lighting.bsp

# Render without lightmaps to isolate base texture appearance.
STELLAR_DISABLE_LIGHTMAPS=1 STELLAR_DEBUG_RENDER=1 build/stellar-client --renderer opengl --map maps/compiled/test_room_full_lighting.bsp
```

### Lightmap regression check

```bash
STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1 build/stellar-client --renderer opengl --map maps/compiled/test_room_full_lighting.bsp
```

### TrenchBroom package/profile check

```bash
rm -rf "$HOME/.TrenchBroom/games/Stellar"
tools/trenchbroom/install_stellar_game_package.sh   --dest "$HOME/.TrenchBroom/games"   --link
```

Restart TrenchBroom, reload Stellar, and inspect whether profiles persist.

## Implementation order

1. Implement/verify OpenGL RGB unpack alignment regression test.
2. Add `STELLAR_DEBUG_TEXTURES=1` output if missing.
3. Compare runtime dev texture pixels/dimensions against WAD and TrenchBroom PNG previews.
4. Redesign `dev/wall_96` and `dev/grid_64` generated visuals to match the intended editor/runtime appearance.
5. Regenerate WAD and PNG previews from the same definitions.
6. Fix TrenchBroom compilation profile schema and tool identifiers.
7. Add a profile linter to CI.
8. Recompile a fresh BSP and manually validate texture scale/color and lighting.

## Automated validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

ctest --test-dir build --output-on-failure

ctest --test-dir build -R 'bsp_lightmaps|bsp_materials|render_level|render_level_inspection|trenchbroom|fgd|client_map_validation' --output-on-failure
```

Optional GPU/display:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build -j$(nproc)
STELLAR_RUN_OPENGL_CONTEXT_TESTS=1 ctest --test-dir build -R opengl --output-on-failure
```

## Manual acceptance

- `dev/wall_96` displays as a clear 96-inch wall-height reference in runtime and TrenchBroom.
- `dev/grid_64` displays as the expected floor grid in runtime and TrenchBroom.
- Pure red/green/blue/white lightmaps render without channel striping or colored shadow artifacts.
- Full Lighting profile runs VHLT and `hlrad`.
- TrenchBroom profiles persist and execute successfully.

## Do not do

- Do not hide texture issues by changing lighting.
- Do not disable lightmaps permanently.
- Do not keep independently generated PNG/WAD/fallback textures.
- Do not add GPU/display requirements to default CI.
- Do not keep compilation tool identifiers with spaces if TrenchBroom clears profiles.
