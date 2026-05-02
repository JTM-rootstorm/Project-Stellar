# TrenchBroom BSP30 Fixtures

These fixtures are editor-facing references for the `trenchbroom-compat` branch. The `.map` sources use Z-up coordinates, 1 unit = 1 inch, developer materials, and Phase 5 underscore metadata aliases (`_stellar_*`) where TrenchBroom exposes smart properties.

## Layout

- `src/*.map` — TrenchBroom source maps.
- `scripts/*.lua` — valid asset-relative Lua scripts used by scripted fixtures.
- `compiled/` — reserved for externally compiled BSP30 outputs. Default CI does not require checked-in BSP binaries; CTest generates deterministic BSP30 equivalents under `build/tests/fixtures/trenchbroom/compiled/` with `stellar_bsp_fixture_writer`.

## Fixture matrix

| Fixture | Source | Generated/compiled BSP | Expected BSP version | Expected outcome |
| --- | --- | --- | --- | --- |
| `minimal_zup_room` | `src/minimal_zup_room.map` | `build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp` | 30 | Client/server display-free validation succeeds. |
| `entity_matrix_zup` | `src/entity_matrix_zup.map` | `build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp` | 30 | Import succeeds and metadata includes all supported FGD classes. |
| `scripted_interaction_zup` | `src/scripted_interaction_zup.map` | `build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp` | 30 | Scripted authoritative runtime loads `scripts/gate.lua` and `scripts/pickup.lua`. |
| `invalid_script_escape_zup` | `src/invalid_script_escape_zup.map` | `build/tests/fixtures/trenchbroom/compiled/invalid_script_escape_zup.bsp` | 30 | Validation fails because `_stellar_script` uses `../escape.lua`. |

## Expected entities

- `minimal_zup_room`: `worldspawn`, `info_player_start origin "0 0 36"`; room bounds are `-96..96` on X/Y, floor `z=0`, ceiling `z=96`.
- `entity_matrix_zup`: `info_player_start`, `info_stellar_spawn`, `trigger_stellar`, `trigger_multiple`, `trigger_once`, `stellar_sprite`, `env_sprite`, `stellar_object_collider`, `func_wall`, `func_door`, `func_button`.
- `scripted_interaction_zup`: `trigger_stellar` bound to `scripts/gate.lua`, `stellar_object_collider` bound to `scripts/pickup.lua`, named static blocker `GateDoor`.
- `invalid_script_escape_zup`: `trigger_stellar` with invalid `_stellar_script "../escape.lua"`.

## Manual editor checklist

1. Open TrenchBroom.
2. Select the project-owned Stellar game config from `tools/trenchbroom/Stellar/`.
3. Open `tests/fixtures/trenchbroom/src/minimal_zup_room.map`.
4. Confirm `dev/grid_32`, `dev/grid_64`, and `dev/wall_96` materials are visible/selectable or named consistently if using a local WAD.
5. Confirm Stellar entity classes appear with smart properties, especially underscore aliases such as `_stellar_script`, `_stellar_extents`, and `_stellar_sprite`.
6. Compile with the BSP30 profile or run `tools/bsp/compile_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/src/minimal_zup_room.map --out tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp --profile fast`.
7. Run `tools/bsp/validate_trenchbroom_bsp30.sh --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp`.
8. Optionally launch `build/stellar-client --map tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp` on a machine with display/GPU access.

Default CI must remain display-free and must not require an external BSP compiler. The compile wrapper smoke test skips gracefully when no compiler is available.
