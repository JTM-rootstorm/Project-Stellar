# TrenchBroom Workflow

Branch target: `trenchbroom-compat`

This is the supported editor-facing workflow for authoring Stellar BSP30 maps with TrenchBroom.
Stellar imports authored BSP coordinates 1:1: one editor unit is one gameplay inch.

## Install or locate TrenchBroom

Install a current TrenchBroom release from your distro or from the upstream project. The Stellar package
is project-owned and lives at:

```text
tools/trenchbroom/Stellar/
```

Add that directory as a TrenchBroom game package, or copy it into TrenchBroom's local games directory.
Then create a new map using the **Stellar** game configuration.

Manual editor checklist:

- Confirm the selected game is **Stellar**.
- Confirm the map format/profile is BSP30, not Source/VBSP.
- Confirm the grid and entity coordinates use X/Y for the floor plan and Z for height.
- Confirm the first room has floor `z = 0`, ceiling `z = 96`, and player origin `0 0 36`.
- Confirm gameplay metadata uses dotted keys or Stellar FGD underscore aliases.
- Confirm compile output is written outside the source `.map` directory, normally under
  `maps/compiled/`.

## BSP30 workflow

Author `.map` sources under:

```text
maps/src/
```

Compile BSP output under:

```text
maps/compiled/
```

The game package includes `GameConfig.cfg`, `CompilationProfiles.cfg`, and `stellar_entities.fgd`.
Compilation profiles call project wrappers in `tools/bsp/` so terminal and editor builds share the same
BSP30 and validation policy.

## Coordinates, scale, and first room

Stellar uses Z-up coordinates on this branch. Keep the floor at `z = 0`, use X/Y for horizontal floor
layout, and place the default player capsule center at:

```text
origin = "0 0 36"
```

A practical first test room is 192x192x96 inches:

- floor footprint: `-96..96` on X and Y;
- floor height: `z = 0`;
- ceiling height: `z = 96`;
- player start: `info_player_start` at `0 0 36`.

## Developer materials

Use these runtime-recognized material names for deterministic fallback rendering and display-free
validation:

| Material name | Slash alias | Intended scale cue |
| --- | --- | --- |
| `stellar_dev_grid_12` | `dev/grid_12` | 12 inch / 1 foot grid tile. |
| `stellar_dev_grid_16` | `dev/grid_16` | 16 inch tile/checker. |
| `stellar_dev_grid_32` | `dev/grid_32` | 32 inch tile/checker. |
| `stellar_dev_grid_64` | `dev/grid_64` | 64 inch tile/checker. |
| `stellar_dev_player_72` | `dev/player_72` | 72 inch player-height reference strip. |
| `stellar_dev_wall_96` | `dev/wall_96` | 96 inch / 8 foot wall-height reference strip. |

Editor-visible WAD3 generation is deferred. If TrenchBroom requires texture thumbnails, create a local
WAD with these exact names. Runtime validation does not require the WAD; it uses deterministic fallback
material data by name.

## FGD key policy

The TrenchBroom-facing FGD uses importer-supported underscore aliases rather than plain placeholder
field names. This avoids compiler/editor ambiguity while preserving the same runtime metadata as dotted
keys.

| FGD alias | Runtime metadata key |
| --- | --- |
| `_stellar_script` | `stellar.script` |
| `_stellar_table` | `stellar.table` |
| `_stellar_extents` | `stellar.extents` |
| `_stellar_once` | `stellar.once` |
| `_stellar_sprite` | `stellar.sprite` |
| `_stellar_texture` | `stellar.texture` |
| `_stellar_size` | `stellar.size` |
| `_stellar_alpha` | `stellar.alpha` |
| `_stellar_collider` | `stellar.collider` |
| `_stellar_enabled` | `stellar.enabled` |
| `_stellar_collision` | `stellar.collision` |

Do not author plain placeholder names such as `stellar_script`; they are not importer aliases.

## Supported FGD classes

The Stellar package defines:

- `worldspawn`
- `info_player_start`
- `info_stellar_spawn`
- `trigger_stellar`
- `trigger_multiple`
- `trigger_once`
- `stellar_sprite`
- `env_sprite`
- `stellar_object_collider`
- `func_wall`
- `func_door`
- `func_button`

Script bindings are authoritative-runtime metadata only. Import records them but never executes Lua.
Object-collider sensors do not block movement. `func_door` and `func_button` are named static collision
metadata only in this branch; moving brush simulation is deferred.

## Compile to BSP30

Set a BSP30-capable compiler path if one is not on `PATH`:

```bash
export STELLAR_BSP30_COMPILER=/path/to/qbsp
```

Compile and validate:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile fast
```

Profiles:

- `fast`: quick CSG/BSP iteration through the selected compiler.
- `full`: full compile profile placeholder through the selected compiler.
- `validate-only`: skip compile and validate an existing BSP output.

The wrapper fails clearly when no compiler is configured, the output is missing or empty, the BSP header
is not version 30, required gameplay entity text is absent, or display-free Stellar validation fails.

## Validate and launch

Validate an existing BSP30 without compiling:

```bash
tools/bsp/validate_trenchbroom_bsp30.sh --map maps/compiled/test_room.bsp
```

The validation wrapper checks the BSP30 header and runs:

```bash
stellar-client --validate-map maps/compiled/test_room.bsp
stellar-server --validate-config --map maps/compiled/test_room.bsp
```

If built binaries are not in `build/`, set:

```bash
export STELLAR_CLIENT=/path/to/stellar-client
export STELLAR_SERVER=/path/to/stellar-server
```

Launch commands remain outside this package's compile wrapper. Use the normal client/server commands
for the current branch after display-free validation passes:

```bash
build/stellar-client --map maps/compiled/test_room.bsp
build/stellar-server --map maps/compiled/test_room.bsp --listen 127.0.0.1:7777
build/stellar-client --connect 127.0.0.1:7777
```

## End-to-end fixtures

Project-owned TrenchBroom compatibility fixtures live in:

```text
tests/fixtures/trenchbroom/
```

Use `src/minimal_zup_room.map` as the first manual editor smoke map. It is a 192x192x96 Z-up room with
floor `z = 0`, ceiling `z = 96`, `info_player_start origin "0 0 36"`, and developer grid/wall
materials. The fixture README documents the complete fixture matrix, expected entities, expected
validation outcomes, and the manual open/compile/validate checklist.

Generated fixture policy:

- Source-tree `.map` files are human/editor references and reviewable authoring examples.
- Lua fixture scripts live next to the source-map fixture set when needed for scripted maps.
- Binary BSP30 outputs are generated under `build/tests/fixtures/trenchbroom/compiled/` by tests.
- Generated BSPs are deterministic validation artifacts and are not required to be checked in.
- CI/default CTest must not require an external BSP compiler, a display, or a GPU.
- External compiler smoke coverage is optional and skipped clearly when no BSP30 compiler is configured.

Validate generated fixtures after a build with:

```bash
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp
```

## Troubleshooting

- `No BSP30 compiler configured`: set `STELLAR_BSP30_COMPILER` or use `--profile validate-only` for an
  existing BSP.
- `BSP header version is not 30`: use the Stellar BSP30 profile and avoid Source/VBSP compilers.
- Missing texture thumbnails in TrenchBroom: create a local WAD with the documented developer material
  names; runtime validation still uses deterministic fallback materials by name.
- Missing player spawn: add `info_player_start` at `origin = "0 0 36"` for the first room.
- Script path rejected: keep script ids asset-relative and do not use absolute paths, drive-letter
  paths, or `..` parent traversal.
- Runtime script missing: place referenced Lua scripts next to the map or configure the runtime script
  root explicitly.

## Unsupported or deferred

- Moving brush simulation for doors, buttons, plats, trains, or rotating entities.
- Client-side gameplay scripting or renderer/audio script authority.
- Editor WAD generation in the project package; runtime fallback by material name is used instead.
- Source/VBSP formats and Source-specific entities.
- Dynamic rigid bodies, third-party physics, PBR materials, and model animation systems.
