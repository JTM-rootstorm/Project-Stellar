# TrenchBroom BSP30 Fixtures

These fixtures are editor-facing references for the Stellar BSP30 workflow. The `.map` sources use Z-up coordinates, 1 unit = 1 inch, developer materials, and underscore metadata aliases (`_stellar_*`) where TrenchBroom exposes smart properties.

## Layout

- `src/*.map` — TrenchBroom source maps.
- `scripts/*.lua` — valid asset-relative Lua scripts used by scripted fixtures.
- `compiled/` — reserved for externally compiled BSP30 outputs. Default CI does not require checked-in BSP binaries; CTest generates deterministic BSP30 equivalents under `build/tests/fixtures/trenchbroom/compiled/` with `stellar_bsp_fixture_writer`.

The editor package used by these fixtures lives at `tools/trenchbroom/Stellar/`. It supports repo-local
use and copied-package use through `STELLAR_REPO_ROOT` or package `.stellar_repo_root`. The helper
`tools/trenchbroom/install_stellar_game_package.sh` can copy or link the package into a TrenchBroom
games directory and validates `GameConfig.cfg`, `CompilationProfiles.cfg`, FGD, icon, materials/WAD,
and package-local compile/validate shims.

## Fixture matrix

<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_BEGIN -->
| Fixture | Source | Generated/compiled BSP | Expected BSP version | Expected outcome |
| --- | --- | --- | --- | --- |
| `minimal_zup_room` | `src/minimal_zup_room.map` | `build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp` | 30 | Client/server display-free validation succeeds. |
| `entity_matrix_zup` | `src/entity_matrix_zup.map` | `build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp` | 30 | Import succeeds and metadata includes all supported FGD classes. |
| `scripted_interaction_zup` | `src/scripted_interaction_zup.map` | `build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp` | 30 | Scripted authoritative runtime loads scripts/gate.lua and scripts/pickup.lua. |
| `lit_zup_room` | `src/lit_zup_room.map` | `build/tests/fixtures/trenchbroom/compiled/lit_zup_room.bsp` | 30 | Generated fixture imports synthetic lightmap metadata; VHLT full profile bakes real lighting. |
| `spotlight_pitch_down_zup` | `src/spotlight_pitch_down_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/spotlight_pitch_down_zup.bsp` | 30 | VHLT full profile preserves editor-facing downward spotlight pitch after work-map normalization. |
| `spotlight_pitch_up_zup` | `src/spotlight_pitch_up_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/spotlight_pitch_up_zup.bsp` | 30 | VHLT full profile preserves editor-facing upward spotlight pitch after work-map normalization. |
| `spotlight_yaw_walls_zup` | `src/spotlight_yaw_walls_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/spotlight_yaw_walls_zup.bsp` | 30 | VHLT full profile preserves spotlight yaw coverage across room walls. |
| `spotlight_targeted_zup` | `src/spotlight_targeted_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/spotlight_targeted_zup.bsp` | 30 | VHLT full profile accepts target-driven spotlight orientation. |
| `light_environment_pitch_zup` | `src/light_environment_pitch_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/light_environment_pitch_zup.bsp` | 30 | VHLT full profile preserves editor-facing light_environment pitch after work-map normalization. |
| `texture_axes_zup` | `src/texture_axes_zup.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/texture_axes_zup.bsp` | 30 | Valve 220 texture axes, shifts, and scales are preserved by the VHLT wrapper. |
| `material_wad_zup` | `src/material_wad_zup.map` | `build/tests/fixtures/trenchbroom/compiled/material_wad_zup.bsp` | 30 | Relative WAD material workflow resolves safely or falls back deterministically. |
| `moving_door_button_zup` | `src/moving_door_button_zup.map` | `build/tests/fixtures/trenchbroom/compiled/moving_door_button_zup.bsp` | 30 | func_button targets server-authoritative func_door DoorA; collision and presentation transforms are snapshot-owned. |
| `point_volume_zup` | `src/point_volume_zup.map` | `build/tests/fixtures/trenchbroom/compiled/point_volume_zup.bsp` | 30 | Point trigger/object collider aliases preserve Z-up extents without brush solids. |
| `illusionary_static_zup` | `src/illusionary_static_zup.map` | `build/tests/fixtures/trenchbroom/compiled/illusionary_static_zup.bsp` | 30 | func_wall is solid/static and func_illusionary is visible/nonblocking. |
| `alias_target_zup` | `src/alias_target_zup.map` | `none` | n/a | Alias target authoring preflight accepts supported targetname/target routing metadata. |
| `invalid_script_escape_zup` | `src/invalid_script_escape_zup.map` | `build/tests/fixtures/trenchbroom/compiled/invalid_script_escape_zup.bsp` | 30 | Validation fails because _stellar_script uses ../escape.lua. |
| `invalid_incomplete_brush` | `src/invalid_incomplete_brush.map` | `none` | n/a | Source preflight fails for fewer than four brush planes. |
| `invalid_malformed_brush` | `src/invalid_malformed_brush.map` | `none` | n/a | Source preflight fails for malformed/unclosed brush syntax. |
| `invalid_missing_target` | `src/invalid_missing_target.map` | `none` | n/a | Source preflight fails deterministically for unmatched target. |
| `invalid_missing_wad_texture` | `src/invalid_missing_wad_texture.map` | `none` | n/a | Strict source preflight fails for unresolved WAD texture. |
<!-- STELLAR_TRENCHBROOM_FIXTURE_TABLE_END -->

## VHLT fixture matrix

Run the Linux VHLT matrix manually with:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full
```

The matrix skips with exit code `77` when required VHLT tools are unavailable. It expects executable
`hlcsg`, `hlbsp`, `hlvis`, and `hlrad` under `tools/bsp/`, `tools/bsp/vhlt/`, `tools/bsp/bin/`, or the
directory named by `STELLAR_VHLT_DIR`.

| Fixture | VHLT compile outcome | Stellar validation outcome | Expected result |
| --- | --- | --- | --- |
| `minimal_zup_room` | Succeeds. | Succeeds. | Positive fixture passes. |
| `lit_zup_room` | Succeeds with `hlrad` light data. | Succeeds and should import lightmaps. | Positive lighting fixture passes. |
| `texture_axes_zup` | Succeeds with Valve 220 axes preserved in the copied work map. | Succeeds. | Positive texture-axis fixture passes. |
| `entity_matrix_zup` | Succeeds. | Succeeds with supported entity/FGD metadata preserved. | Positive fixture passes. |
| `scripted_interaction_zup` | Succeeds. | Succeeds when `scripts/gate.lua` and `scripts/pickup.lua` are copied beside compiled outputs. | Positive scripted fixture passes. |
| `material_wad_zup` | Succeeds with relative WAD handling. | Succeeds. | Positive material/WAD fixture passes. |
| `moving_door_button_zup` | Succeeds. | Succeeds with moving brush metadata. | Positive door/button fixture passes. |
| `point_volume_zup` | Succeeds. | Succeeds with point trigger/object metadata. | Positive point-volume fixture passes. |
| `illusionary_static_zup` | Succeeds. | Succeeds with static vs illusionary metadata. | Positive static/illusionary fixture passes. |
| `invalid_script_escape_zup` | Succeeds. | Fails for `_stellar_script "../escape.lua"` / `kScriptPathEscape`. | Negative fixture passes only when validation fails for the expected reason. |
| `invalid_incomplete_brush` | Source preflight fails before VHLT. | Not run. | Negative fixture passes only for expected brush diagnostic. |
| `invalid_malformed_brush` | Source preflight fails before VHLT. | Not run. | Negative fixture passes only for expected malformed brush diagnostic. |
| `invalid_missing_target` | Source preflight fails before VHLT. | Not run. | Negative fixture passes only for expected missing target diagnostic. |
| `invalid_missing_wad_texture` | Strict source preflight fails before VHLT. | Not run. | Negative fixture passes only for expected unresolved texture diagnostic. |

VHLT matrix outputs are generated under the build tree and should not be committed:

| Artifact | Location |
| --- | --- |
| Compiled BSPs | `build/tests/fixtures/trenchbroom/vhlt/compiled/` |
| Per-fixture logs and copied VHLT stage outputs | `build/tests/fixtures/trenchbroom/vhlt/logs/<fixture>/` |
| Matrix summary | `build/tests/fixtures/trenchbroom/vhlt/logs/matrix_summary.log` |
| Preserved wrapper work maps/WADs | `build/tests/fixtures/trenchbroom/vhlt/work/<fixture>/` |

Source `.map` fixtures remain clean authoring references. The VHLT wrapper injects temporary WAD
references and compiler-facing texture alias rewrites only into copied work maps under the build tree.

## Expected entities

- `minimal_zup_room`: `worldspawn`, `info_player_start origin "0 0 36"`; room bounds are `-96..96` on X/Y, floor `z=0`, ceiling `z=96`.
- `lit_zup_room`: `worldspawn`, `info_player_start origin "0 0 36"`, `light`, `light_spot`; room bounds match `minimal_zup_room` and are intended for VHLT lightmap generation.
- `texture_axes_zup`: room-shaped brush fixture with floor X/Y axes, north/south X/Z axes, east/west
  Y/Z axes, nonzero texture shifts, and non-1 texture scales.
- `entity_matrix_zup`: `info_player_start`, `info_stellar_spawn`, `trigger_stellar`, `trigger_multiple`, `trigger_once`, `stellar_sprite`, `env_sprite`, `stellar_object_collider`, `func_wall`, `func_door`, `func_button`.
- `scripted_interaction_zup`: `trigger_stellar` bound to `scripts/gate.lua`, `stellar_object_collider` bound to `scripts/pickup.lua`, named static blocker `GateDoor`.
- `moving_door_button_zup`: `func_door targetname "DoorA"`, `func_button target "DoorA"`, and baked light entity for TB-FULL-04 compatibility.
- `point_volume_zup`: `trigger_multiple_point` and `stellar_object_collider_point` with `_stellar_extents`/`stellar.extents`-compatible metadata.
- `illusionary_static_zup`: paired `func_wall`/`func_illusionary` brush entities for solid vs visible-nonblocking behavior.
- `invalid_script_escape_zup`: `trigger_stellar` with invalid `_stellar_script "../escape.lua"`.

Full manual QA is tracked in `docs/TrenchBroomManualQA.md`; this README keeps the fixture-specific quick checklist only.

## Manual editor checklist

1. Open TrenchBroom.
2. Select the project-owned Stellar game config from `tools/trenchbroom/Stellar/`.
3. Open `tests/fixtures/trenchbroom/src/minimal_zup_room.map`.
4. Confirm `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` materials are visible/selectable or named consistently if using a local WAD.
5. Confirm Stellar entity classes appear with smart properties, especially underscore aliases such as `_stellar_script`, `_stellar_extents`, and `_stellar_sprite`.
6. Compile with `Stellar BSP30 Fast` for quick no-light iteration or `Stellar BSP30 Full Lighting` for
   VHLT baked lighting. The profiles invoke `bin/stellar_tb_compile.sh` with quoted map and output paths,
   so copied packages and paths with spaces work when `STELLAR_REPO_ROOT` or `.stellar_repo_root` points
   at the checkout.
7. Or run `tools/bsp/compile_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/src/minimal_zup_room.map --out tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp --profile fast`.
8. Run `tools/bsp/validate_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp`.
9. For lighting, compile `src/lit_zup_room.map` with the VHLT full profile and confirm validation reports
   a BSP30 with lighting data; runtime import should create one or more `LevelLightmap` records.
10. Optionally launch `build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp` on a machine with display/GPU access and verify light/dark variation on static surfaces in OpenGL or Vulkan.

Default CI must remain display-free and must not require an external BSP compiler. The compile wrapper
smoke test skips gracefully when no compiler is available, and `trenchbroom_package_path_smoke` checks
package-local shims, copied-package root resolution, expected package files, and quoted compile profile
parameters without launching TrenchBroom.
