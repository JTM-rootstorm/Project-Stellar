# TrenchBroom BSP30 Fixtures

These fixtures are editor-facing references for the `trenchbroom-compat` branch. The `.map` sources use Z-up coordinates, 1 unit = 1 inch, developer materials, and Phase 5 underscore metadata aliases (`_stellar_*`) where TrenchBroom exposes smart properties.

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

| Fixture | Source | Generated/compiled BSP | Expected BSP version | Expected outcome |
| --- | --- | --- | --- | --- |
| `minimal_zup_room` | `src/minimal_zup_room.map` | `build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp` | 30 | Client/server display-free validation succeeds. |
| `lit_zup_room` | `src/lit_zup_room.map` | `build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp` | 30 | VHLT full profile bakes a nonempty lighting lump and Stellar imports at least one lightmap. |
| `entity_matrix_zup` | `src/entity_matrix_zup.map` | `build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp` | 30 | Import succeeds and metadata includes all supported FGD classes. |
| `scripted_interaction_zup` | `src/scripted_interaction_zup.map` | `build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp` | 30 | Scripted authoritative runtime loads `scripts/gate.lua` and `scripts/pickup.lua`. |
| `invalid_script_escape_zup` | `src/invalid_script_escape_zup.map` | `build/tests/fixtures/trenchbroom/compiled/invalid_script_escape_zup.bsp` | 30 | Validation fails because `_stellar_script` uses `../escape.lua`. |

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
| `entity_matrix_zup` | Succeeds. | Succeeds with supported entity/FGD metadata preserved. | Positive fixture passes. |
| `scripted_interaction_zup` | Succeeds. | Succeeds when `scripts/gate.lua` and `scripts/pickup.lua` are copied beside compiled outputs. | Positive scripted fixture passes. |
| `invalid_script_escape_zup` | Succeeds. | Fails for `_stellar_script "../escape.lua"` / `kScriptPathEscape`. | Negative fixture passes only when validation fails for the expected reason. |

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
- `entity_matrix_zup`: `info_player_start`, `info_stellar_spawn`, `trigger_stellar`, `trigger_multiple`, `trigger_once`, `stellar_sprite`, `env_sprite`, `stellar_object_collider`, `func_wall`, `func_door`, `func_button`.
- `scripted_interaction_zup`: `trigger_stellar` bound to `scripts/gate.lua`, `stellar_object_collider` bound to `scripts/pickup.lua`, named static blocker `GateDoor`.
- `invalid_script_escape_zup`: `trigger_stellar` with invalid `_stellar_script "../escape.lua"`.

## Manual editor checklist

1. Open TrenchBroom.
2. Select the project-owned Stellar game config from `tools/trenchbroom/Stellar/`.
3. Open `tests/fixtures/trenchbroom/src/minimal_zup_room.map`.
4. Confirm `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` materials are visible/selectable or named consistently if using a local WAD.
5. Confirm Stellar entity classes appear with smart properties, especially underscore aliases such as `_stellar_script`, `_stellar_extents`, and `_stellar_sprite`.
6. Compile with the BSP30 profile. The profile invokes `bin/stellar_tb_compile.sh` with quoted map and
   output paths, so copied packages and paths with spaces work when `STELLAR_REPO_ROOT` or
   `.stellar_repo_root` points at the checkout.
7. Or run `tools/bsp/compile_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/src/minimal_zup_room.map --out tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp --profile fast`.
8. Run `tools/bsp/validate_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp`.
9. For lighting, compile `src/lit_zup_room.map` with the VHLT full profile and confirm validation reports
   a BSP30 with lighting data; runtime import should create one or more `LevelLightmap` records.
10. Optionally launch `build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp` on a machine with display/GPU access and verify light/dark variation on static surfaces in OpenGL or Vulkan.

Default CI must remain display-free and must not require an external BSP compiler. The compile wrapper
smoke test skips gracefully when no compiler is available, and `trenchbroom_package_path_smoke` checks
package-local shims, copied-package root resolution, expected package files, and quoted compile profile
parameters without launching TrenchBroom.
