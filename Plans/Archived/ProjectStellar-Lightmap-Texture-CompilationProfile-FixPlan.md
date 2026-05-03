# Project Stellar — Lightmap Color, Texture Loading, and TrenchBroom Compilation Profile Fix Plan

Target branch: `fixes`

## User-reported issues

1. Pure red/green/blue lights produce red/green/blue striping in lightmaps instead of solid colored lighting.
2. White lights mostly work, but shadows/corners take on a red hue.
3. Runtime textures are still wrong or not loading correctly.
4. TrenchBroom clears the compilation profiles or the profile tools do not execute. Tool references such as `Stellar BSP30 compile wrapper` fail.

This plan is optimized for AI implementation agents. Do not commit unless explicitly instructed by the user.

## Current branch observations

`fixes` now includes render/lightmap diagnostics, double-sided BSP materials, VHLT texture-axis preservation, unified procedural developer textures, and a `Stellar BSP30 Full Lighting` profile.

Important files:

- `src/import/bsp/BspLevelBuilder.cpp`
- `src/graphics/opengl/OpenGLGraphicsDevice.cpp`
- `src/graphics/RenderLevel.cpp`
- `src/import/bsp/DeveloperTextures.cpp`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/trenchbroom/Stellar/GameConfig.cfg`
- `tools/trenchbroom/Stellar/CompilationProfiles.cfg`
- `tools/trenchbroom/test_stellar_package_paths.sh`

## Highest-probability root causes

### Root cause A — OpenGL RGB unpack alignment is corrupting lightmap rows

This is the most likely cause of the colored stripes.

Facts:

- BSP lightmaps are imported as `ImageFormat::kR8G8B8` in `BspLevelBuilder.cpp`.
- `OpenGLGraphicsDevice::create_texture()` uploads textures with `glTexImage2D`.
- The OpenGL upload path does not set `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)`.
- OpenGL default unpack alignment is 4 bytes.
- RGB rows have `width * 3` bytes. If that row size is not divisible by 4, OpenGL assumes row padding that the CPU pixel vector does not contain.
- The result is classic channel/row misalignment: pure red light can appear as red/green/blue stripes, and white light can produce colored artifacts near corners or rows.

Required fix:

1. In `OpenGLGraphicsDevice::create_texture()`, set unpack alignment to 1 before `glTexImage2D`.
2. Restore previous unpack alignment after upload.
3. Apply this to all texture uploads, not only lightmaps, because future RGB images may have the same issue.

Suggested code:

```cpp
GLint previous_unpack_alignment = 4;
glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

glTexImage2D(...);

glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
```

Alternative:

- Convert RGB lightmaps to RGBA before upload. This is more memory, but avoids RGB row alignment. The unpack-alignment fix is still recommended because it is correct for arbitrary image formats.

Acceptance:

- Pure red light produces red-only lightmap visualization, not RGB stripes.
- Pure green produces green-only, pure blue produces blue-only.
- White light shadows no longer show red-channel row artifacts.
- Add a unit or opt-in GPU readback test with a 3x2 or 5x2 RGB image to catch row alignment regressions.

### Root cause B — Lightmap channel order may also need verification

After fixing unpack alignment, verify whether the BSP compiler emits RGB or BGR.

Tasks:

1. Build a synthetic BSP fixture with known lightmap bytes:
   - one texel red `(255, 0, 0)`
   - one green `(0, 255, 0)`
   - one blue `(0, 0, 255)`
2. Import and inspect `geometry.images[lightmap.image_index].pixels`.
3. Use `STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1` in a controlled scene to visually verify channel order.

If channel order is wrong:

- Swap channels during BSP lightmap import or expose a compiler-specific conversion option.
- Do not apply channel swaps until unpack alignment is fixed, because row misalignment can look like channel-order corruption.

Acceptance:

- CPU importer stats and GPU visualization agree on red/green/blue channel order.
- Tests distinguish row alignment from BGR/RGB order.

### Root cause C — Runtime texture source may still differ from TrenchBroom's texture source

The branch now unifies procedural dev textures to 128x128, but runtime can still differ if:

- the compiled BSP references `dev_grid_*` while TrenchBroom shows `dev/grid_*`;
- external WAD paths injected by VHLT are absolute temp paths and get rejected by the runtime WAD reader;
- VHLT embeds texture pixels differently than expected;
- runtime falls back to procedural textures while TrenchBroom displays PNGs or WAD textures;
- the generated package installed into TrenchBroom is stale;
- the map was compiled before the texture-axis fix and still contains old texinfo.

Tasks:

1. Add or enable texture diagnostics:

   ```bash
   STELLAR_DEBUG_TEXTURES=1 build/stellar-client --validate-map maps/compiled/test_room.bsp
   ```

2. Log per material:
   - material name
   - source texture name
   - image source URI
   - image dimensions
   - whether pixels came from embedded miptex, external WAD, or procedural fallback
   - texture sampler
   - uv0 min/max per surface

3. For VHLT output, inspect preserved work map:

   ```bash
   STELLAR_VHLT_KEEP_WORK=1 tools/bsp/compile_trenchbroom_bsp30.sh ...
   grep -n '\[' build/tests/fixtures/trenchbroom/vhlt/work/<fixture>/*.map
   ```

   The work map should preserve Valve 220 texture axes.

4. Inspect compiled BSP texture names:

   ```bash
   strings maps/compiled/test_room.bsp | grep -E 'dev_|dev/|stellar_dev'
   ```

5. Compare runtime loaded texture dimensions with TrenchBroom package WAD/PNG dimensions.

Required fixes:

- Ensure `DeveloperTextures.cpp`, generated WAD, and PNG material previews use the same pattern semantics.
- Prefer safe runtime WAD resolution for package-local `stellar_dev.wad`.
- Avoid relying on absolute temp WAD paths inside compiled BSPs.
- If VHLT requires an absolute WAD at compile time, post-process or document that runtime must resolve by texture name/fallback, not that temp path.

Acceptance:

- `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` look the same in runtime as in TrenchBroom.
- Runtime diagnostics show the source of every texture.
- Recompiled maps after the Valve 220-axis fix have expected uv0 values.
- Old stale BSPs are not used for manual validation.

### Root cause D — TrenchBroom CompilationProfiles.cfg schema is likely invalid

The current package has:

- `GameConfig.cfg` `compilationTools` named with spaces:
  - `Stellar BSP30 compile wrapper`
  - `Stellar BSP30 validation wrapper`
- `CompilationProfiles.cfg` uses:
  - `"tool": "Stellar BSP30 compile wrapper"`
  - `"tool": "Stellar BSP30 validation wrapper"`
- `CompilationProfiles.cfg` currently has no top-level `"version"` field.

TrenchBroom commonly expects compilation profile tool references to use variable expansion backed by compilation tool identifiers, such as:

```json
"tool": "${QBSP}"
```

not arbitrary display strings with spaces.

Required investigation:

1. Create a minimal throwaway package with one known-good TrenchBroom profile and confirm schema.
2. Validate whether TrenchBroom requires:
   - top-level `"version": 1`
   - variable-style tool references: `"${TOOL_NAME}"`
   - tool names without spaces
   - `workdir` field
   - a different key name for tools or tasks
3. Inspect TrenchBroom logs/preferences after reload to see exact parser reason for clearing profiles.

Recommended fix:

1. Rename compilation tool identifiers in `GameConfig.cfg` to variable-safe names:
   - `STELLAR_BSP30_COMPILE`
   - `STELLAR_BSP30_VALIDATE`
2. Add display descriptions if supported, but keep the `name` identifier simple.
3. Update profiles to use variable expansion:
   - `"tool": "${STELLAR_BSP30_COMPILE}"`
   - `"tool": "${STELLAR_BSP30_VALIDATE}"`
4. Add top-level version:

   ```json
   {
     "version": 1,
     "profiles": []
   }
   ```

5. Add `workdir` if TrenchBroom expects one:

   ```json
   "workdir": "${WORK_DIR_PATH}"
   ```

6. Avoid spaces in tool identifiers.

Candidate revised `GameConfig.cfg` block:

```json
"compilationTools": [
  {
    "name": "STELLAR_BSP30_COMPILE",
    "path": "bin/stellar_tb_compile.sh"
  },
  {
    "name": "STELLAR_BSP30_VALIDATE",
    "path": "bin/stellar_tb_validate.sh"
  }
]
```

Candidate revised `CompilationProfiles.cfg`:

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

- TrenchBroom no longer clears profiles on reload.
- Profiles appear in the UI.
- Fast profile executes package-local compile shim.
- Full Lighting profile executes package-local compile shim with `--toolchain vhlt`.
- Validate profile executes package-local validate shim.
- Paths with spaces still work.

### Root cause E — Package smoke tests are insufficient for real TrenchBroom profile validity

The current package path test checks token existence and shim execution, but it does not parse `CompilationProfiles.cfg` with TrenchBroom's parser or simulate tool substitution.

Required tests:

1. Add a structural JSON-ish parser/linter for our profile shape:
   - top-level version exists
   - tool names are variable-safe
   - profile tool references use `${...}`
   - every referenced tool is declared in `GameConfig.cfg`
   - no referenced tool name contains spaces
2. Add a script `tools/trenchbroom/lint_stellar_compilation_profiles.py`.
3. Update `tools/trenchbroom/test_stellar_package_paths.sh` to run this linter.
4. Optionally add a manual/optional test that invokes TrenchBroom headlessly if available.

Acceptance:

- CI catches invalid profile schema before the user sees TrenchBroom clearing it.
- The linter fails on the old `"tool": "Stellar BSP30 compile wrapper"` style.

## Immediate debug commands for agents

### Lightmap artifact isolation

```bash
STELLAR_VHLT_KEEP_WORK=1 \
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room_full_lighting.bsp \
  --profile full \
  --toolchain vhlt

STELLAR_DEBUG_LIGHTMAPS=1 \
build/stellar-client --validate-map maps/compiled/test_room_full_lighting.bsp

STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1 \
build/stellar-client --renderer opengl --map maps/compiled/test_room_full_lighting.bsp

STELLAR_DISABLE_LIGHTMAPS=1 \
build/stellar-client --renderer opengl --map maps/compiled/test_room_full_lighting.bsp
```

Expected:

- If visualization shows RGB stripes, fix unpack alignment first.
- If disabling lightmaps fixes color artifacts but not textures, texture issue is separate.
- If force visualization becomes correct after unpack alignment, lightmap upload was the issue.

### Texture isolation

```bash
STELLAR_DEBUG_TEXTURES=1 \
build/stellar-client --validate-map maps/compiled/test_room_full_lighting.bsp

STELLAR_DISABLE_LIGHTMAPS=1 \
STELLAR_DEBUG_RENDER=1 \
build/stellar-client --renderer opengl --map maps/compiled/test_room_full_lighting.bsp
```

Expected:

- Base textures should be inspected without lightmap multiplication.
- Runtime should report whether dev textures are WAD, embedded, or fallback.

### TrenchBroom profile isolation

1. Reinstall/link package:

   ```bash
   rm -rf "$HOME/.TrenchBroom/games/Stellar"
   tools/trenchbroom/install_stellar_game_package.sh \
     --dest "$HOME/.TrenchBroom/games" \
     --link
   ```

2. Restart TrenchBroom.
3. Reload game package/entity definitions.
4. Check whether profiles remain.
5. If still cleared, inspect TrenchBroom logs and update profile schema.

## Implementation order

1. Fix OpenGL unpack alignment.
2. Add RGB lightmap/channel tests.
3. Add lightmap visualization verification.
4. Fix TrenchBroom profile schema with variable-safe tool identifiers and top-level version.
5. Add compilation profile linter.
6. Add or finish texture diagnostics.
7. Confirm texture source consistency and WAD/fallback matching.
8. Re-run manual lighting and texture smoke.

## Automated validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

ctest --test-dir build --output-on-failure

ctest --test-dir build -R 'bsp_lightmaps|bsp_materials|render_level|render_level_inspection|trenchbroom|fgd|client_map_validation|opengl' --output-on-failure
```

Optional GPU/display:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_OPENGL_CONTEXT_TESTS=ON
cmake --build build -j$(nproc)
STELLAR_RUN_OPENGL_CONTEXT_TESTS=1 ctest --test-dir build -R opengl --output-on-failure
```

## Manual acceptance

- Pure red light produces red lightmap illumination with no green/blue striping.
- Pure green produces green only.
- Pure blue produces blue only.
- White light shadows are neutral, not red-tinted.
- Textures match TrenchBroom previews and alignment.
- Fast profile is visible and documented as no-bake/no-lightmap.
- Full Lighting profile runs VHLT + `hlrad`.
- TrenchBroom profiles persist after reload and execute successfully.

## Do not do

- Do not hide the problem with ambient lighting alone.
- Do not disable lightmaps permanently.
- Do not change gameplay authority or camera controls in this slice.
- Do not add GPU/display requirements to default CI.
- Do not keep TrenchBroom tool identifiers with spaces if the UI clears profiles.
