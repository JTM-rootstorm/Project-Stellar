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

Add that directory as a TrenchBroom game package for repo-local use, or install/copy it into
TrenchBroom's local games directory. For copied packages, point the package back at the checkout with
`STELLAR_REPO_ROOT` or with the package-local `.stellar_repo_root` file written by the helper.

Helper install examples:

```bash
tools/trenchbroom/install_stellar_game_package.sh --dest "$HOME/.TrenchBroom/games" --copy
tools/trenchbroom/install_stellar_game_package.sh --dest "$HOME/.TrenchBroom/games" --link
```

Manual copied-package setup:

```bash
cp -a tools/trenchbroom/Stellar "$HOME/.TrenchBroom/games/Stellar"
printf '%s\n' "$PWD" > "$HOME/.TrenchBroom/games/Stellar/.stellar_repo_root"
```

Then create a new map using the **Stellar** game configuration.

Manual editor checklist:

- Confirm the selected game is **Stellar**.
- Confirm the map format/profile is BSP30, not Source/VBSP.
- Confirm the grid and entity coordinates use X/Y for the floor plan and Z for height.
- Confirm the first room has floor `z = 0`, ceiling `z = 96`, and player origin `0 0 36`.
- Confirm gameplay metadata uses dotted keys or Stellar FGD underscore aliases.
- Confirm triggers/buttons use `target` to fire another entity's `targetname`; doors are opened by
  firing their `targetname`. Runtime movement and collision are server-authoritative and replicated to
  clients as presentation transforms.
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

The game package includes `GameConfig.cfg`, `CompilationProfiles.cfg`, `stellar_entities.fgd`,
`Icon.png`, package-local materials/WAD assets, and `bin/stellar_tb_compile.sh` /
`bin/stellar_tb_validate.sh` shims. Compilation profiles call those shims, which locate the checkout and
delegate to project wrappers in `tools/bsp/` so terminal and editor builds share the same BSP30 and
validation policy.

The shims resolve the repository root in this order: `STELLAR_REPO_ROOT`, package `.stellar_repo_root`,
walking upward for repo-local installs, then the current working directory if it is a Stellar checkout.
Compile profile parameters quote `${MAP_FULL_PATH}` and output paths so spaces in checkout or map paths
are handled by the shell wrapper path.

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

| Canonical runtime name | Source alias | Compiler/WAD alias | Intended scale cue |
| --- | --- | --- | --- |
| `stellar_dev_grid_12` | `dev/grid_12` | `dev_grid_12` | 12 inch / 1 foot grid tile. |
| `stellar_dev_grid_16` | `dev/grid_16` | `dev_grid_16` | 16 inch tile/checker. |
| `stellar_dev_grid_32` | `dev/grid_32` | `dev_grid_32` | 32 inch tile/checker. |
| `stellar_dev_grid_64` | `dev/grid_64` | `dev_grid_64` | 64 inch tile/checker. |
| `stellar_dev_player_72` | `dev/player_72` | `dev_player_72` | 72 inch player-height reference strip. |
| `stellar_dev_wall_96` | `dev/wall_96` | `dev_wall_96` | 96 inch / 8 foot wall-height reference strip. |

The official WAD pixels and deterministic runtime fallback pixels are 128x128 for every developer
texture. The names describe grid spacing or height reference cues, not bitmap dimensions.

The package includes editor-visible developer PNGs and `materials/stellar_dev.wad`. The WAD uses the
15-byte compiler-compatible `dev/...` and `dev_*` names; runtime also recognizes the longer
`stellar_dev_*` aliases as deterministic fallback names. Generate or verify it with:

```bash
python3 tools/bsp/create_stellar_dev_wad.py --out /tmp/stellar_dev.wad
python3 tools/bsp/create_stellar_dev_wad.py --list
python3 tools/bsp/create_stellar_dev_wad.py --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad
```

Runtime BSP import resolves embedded miptex pixels first, then safe external WAD3 textures from the
compiled `worldspawn` `wad` key, then deterministic developer fallback names. Relative WAD paths are
searched from the BSP/map directory, `STELLAR_WAD_PATH`/`STELLAR_TEXTURE_PATH` roots, and the packaged
developer materials path. Parent-directory escapes are rejected. Absolute WAD paths are ignored unless
`STELLAR_ALLOW_ABSOLUTE_WAD_PATHS` is explicitly set, so do not commit local absolute WAD paths.

## VHLT Linux toolchain

On Linux, the supported multi-stage external BSP30 path is the VHLT wrapper in `tools/bsp/`. Place
executable VHLT tools in one of these repository-local locations, or point `STELLAR_VHLT_DIR` at a
directory containing them:

```text
tools/bsp/hlcsg
tools/bsp/hlbsp
tools/bsp/hlvis
tools/bsp/hlrad
tools/bsp/ripent      optional
tools/bsp/vhlt/<tool> alternate layout
tools/bsp/bin/<tool>  alternate layout
```

The wrapper also accepts explicit per-tool overrides: `HLCSG`, `HLBSP`, `HLVIS`, `HLRAD`, and
`RIPENT`. Keep copied or vendored tools executable and out of generated build-output directories.

Compile one map directly with VHLT:

```bash
tools/bsp/compile_vhlt_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile full
```

Compile through the editor-facing wrapper while selecting VHLT explicitly:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile full \
  --toolchain vhlt
```

Or select it by environment for TrenchBroom profiles and terminal use:

```bash
export STELLAR_BSP30_TOOLCHAIN=vhlt
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile full
```

Use `maps/compiled/` for manually authored maps. The fixture matrix writes generated artifacts under
`build/tests/fixtures/trenchbroom/vhlt/compiled/`, logs under
`build/tests/fixtures/trenchbroom/vhlt/logs/`, and preserved wrapper work directories under
`build/tests/fixtures/trenchbroom/vhlt/work/` when requested by the matrix runner.

During VHLT compilation, the wrapper copies the source `.map` into an isolated work directory,
generates a temporary `stellar_dev.wad`, injects or replaces the copied map's `wad` key, and rewrites
compiler-facing developer texture aliases when required by VHLT. It also converts TrenchBroom/editor-
facing spotlight and environment-light pitch to VHLT/GoldSrc pitch on the copied work map before
`hlrad`. The authored source `.map` is not modified. The default rewrite preserves Valve 220 texture
axes, shifts, rotation, and scales, and injects `mapversion "220"` into the copied work map so repo-local
VHLT stays in Valve 220 parsing mode. Use `--classic-texture-axes` only as an explicit diagnostic fallback
for a legacy tool that cannot consume Valve 220 axes. Source-tree `.map` files remain clean authoring
references and should not receive local absolute WAD paths.

## Compile-time lighting and renderer lightmaps

Place `light`, `light_spot`, and `light_environment` entities from the Stellar FGD to bake static BSP
lighting. VHLT's full profile runs `hlrad`, which writes the BSP lighting lump consumed by Stellar's
importer. The renderer uploads those lightmaps as linear, clamp-to-edge textures and samples them from
secondary UVs (`uv1`), multiplying base material color/texture by the baked lightmap. Surfaces without
valid lightmap data keep the existing unlit/fullbright fallback behavior.

Spotlight orientation in authored TrenchBroom maps uses editor-facing degrees:

- `pitch` `90` points down.
- `pitch` `270` or `-90` points up.
- `pitch` `0` is horizontal.
- `yaw`/`angle` controls the horizontal direction.
- Roll in `angles "pitch yaw roll"` is ignored by classic `light_spot` and `light_environment`.

If a `light_spot` has `target`, VHLT may aim the spotlight at the target entity and ignore
`angle`/`pitch`/`angles`. Prefer targeted fixtures when validating target-driven behavior.

Fast compile profiles are geometry-iteration profiles and do not bake lighting. From TrenchBroom, use
`Stellar BSP30 Full Lighting` when you need baked lightmaps; it passes `--profile full --toolchain vhlt`
so `hlcsg`, `hlbsp`, `hlvis`, and `hlrad` all run. From a terminal, pass `--toolchain vhlt` explicitly
for the same behavior.

Useful runtime/import diagnostics:

- `build/stellar-client --validate-map <map.bsp>` prints imported lightmap summaries, per-lightmap RGB
  stats, all-black warnings, and material/lightmap bindings.
- `STELLAR_DEBUG_LIGHTMAPS=1` logs renderer-side lightmap upload/binding stats at startup.
- `STELLAR_DISABLE_LIGHTMAPS=1` or `STELLAR_FORCE_FULLBRIGHT=1` skips lightmap bindings and shows base
  textures/fullbright fallback.
- `STELLAR_FORCE_LIGHTMAP_VISUALIZATION=1` samples the lightmap texture directly through secondary UVs
  so imported lighting can be inspected without base textures.

Use `tests/fixtures/trenchbroom/src/lit_zup_room.map` or the package `templates/lit_zup_room.map` as the
manual lit-room reference. After VHLT compile and validation, launch with OpenGL or Vulkan and confirm
visible light/dark variation on the floor and walls. Nonzero classic BSP light styles currently use a
stable static multiplier of `1.0`; no dynamic realtime lights or client-side gameplay authority are
created by light entities.

## FGD key policy

The TrenchBroom-facing FGD uses importer-supported underscore aliases rather than plain placeholder
field names. This avoids compiler/editor ambiguity while preserving the same runtime metadata as dotted
keys.

Entity targeting uses the classic BSP contract: `classname` selects the entity type, `targetname` is the
entity's targetable Name, and `target` points to another entity's `targetname`. Multiple entities may
share a `targetname` for group firing.

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
- `trigger_stellar_point`, `trigger_multiple_point`, `trigger_once_point`
- `stellar_sprite`
- `env_sprite`
- `stellar_object_collider`
- `stellar_object_collider_point`
- `light`, `light_spot`, `light_environment`
- `func_wall`
- `func_illusionary`, `func_detail`
- `func_door`
- `func_button`

Script bindings are import-time metadata for the authoritative runtime. Import records them but never
executes Lua.
Object-collider sensors do not block movement. `func_door` and `func_button` are server-authoritative
runtime brush movers; clients receive only replicated presentation transforms.

## Compile to BSP30

Set a BSP30-capable compiler path if one is not on `PATH`:

```bash
export STELLAR_BSP30_COMPILER=/path/to/qbsp
```

The generic single-compiler path remains supported. It invokes the configured compiler as
`<compiler> <map> <out>`, so compilers with different command lines should be wrapped in a small adapter
and assigned to `STELLAR_BSP30_COMPILER`.

Compile and validate with the selected toolchain:

```bash
tools/bsp/compile_trenchbroom_bsp30.sh \
  --map maps/src/test_room.map \
  --out maps/compiled/test_room.bsp \
  --profile fast
```

Select VHLT with either `--toolchain vhlt` or `STELLAR_BSP30_TOOLCHAIN=vhlt`. The default `auto`
selection prefers an explicit single compiler, then repository-local VHLT tools, then compatible
single-compilers found on `PATH`.

Profiles:

- `Stellar BSP30 Fast` / `--profile fast`: quick no-light CSG/BSP iteration through the selected
  compiler/toolchain.
- `Stellar BSP30 Full Lighting` / `--profile full --toolchain vhlt`: full VHLT compile with `hlcsg`,
  `hlbsp`, `hlvis`, and `hlrad` for baked lightmaps.
- `validate-only`: skip compile and validate an existing BSP output.

The wrapper fails clearly when no compiler is configured, the output is missing or empty, the BSP header
is not version 30, required gameplay entity text is absent, or display-free Stellar validation fails.
Before compiling, the wrapper runs `tools/bsp/validate_trenchbroom_map_source.py` over source `.map`
files. Use `--skip-source-preflight` only for diagnosing compiler behavior directly. Preflight catches
unbalanced entity/brush blocks, incomplete brushes, malformed Valve 220 texture axes, unsafe script/WAD
paths, missing worldspawn/player start, and malformed `_stellar_*` values with line/column diagnostics.

If TrenchBroom reports that the compile tool cannot locate the repository, export
`STELLAR_REPO_ROOT=/path/to/Stellar_Engine` before launching TrenchBroom or reinstall the package with
`tools/trenchbroom/install_stellar_game_package.sh --copy` so `.stellar_repo_root` is written. If
validation reports missing binaries, build the project or set `STELLAR_CLIENT` and `STELLAR_SERVER`.

## Validate and launch

Always run the validation wrapper after compile, including after a successful VHLT build. Compiler
success only proves a BSP was produced; Stellar validation proves the BSP30 header, entity text,
importer constraints, client map validation, server config validation, and Lua script path sandbox
policy all pass.

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
build/stellar-client --validate-display
build/stellar-client --map maps/compiled/test_room.bsp
build/stellar-server --map maps/compiled/test_room.bsp --listen 127.0.0.1:7777
build/stellar-client --connect 127.0.0.1:7777
```

Live play uses FPS-style controls. `W`/`A`/`S`/`D` move relative to the current camera yaw on the X/Y
floor plane, `Space` submits jump intent, mouse motion turns and looks up/down, arrow keys provide
keyboard look fallback, `F1` toggles relative mouse capture, left-click recaptures the mouse, and `Esc`
first releases capture before a second `Esc` exits.

For quick camera diagnostics, set `STELLAR_DEBUG_CAMERA=1` before launching. The client logs the first
few authoritative camera frames, including runtime mode, player center, snapshot rotation, camera eye,
camera target, and near/far planes.

`--validate-map` and `stellar-server --validate-config --map` are display-free. `--validate-display`
is intentionally GUI/display-backed: it creates a small SDL window, initializes the selected graphics
backend, and exits. Use it to separate map/import validity from Linux display-server authorization or
OpenGL/Vulkan context startup issues.

## End-to-end fixtures

Project-owned TrenchBroom compatibility fixtures live in:

```text
tests/fixtures/trenchbroom/
```

Use `src/minimal_zup_room.map` as the first manual editor smoke map and `src/lit_zup_room.map` as the
lighting smoke map. Both are 192x192x96 Z-up rooms with floor `z = 0`, ceiling `z = 96`,
`info_player_start origin "0 0 36"`, and developer grid/wall materials. The lit fixture also includes
`light` and `light_spot` entities for VHLT `hlrad` lightmap generation. The fixture README documents the
complete fixture matrix, expected entities, expected validation outcomes, and the manual
open/compile/validate checklist.

The complete positive source fixture matrix is `minimal_zup_room`, `entity_matrix_zup`,
`scripted_interaction_zup`, `lit_zup_room`, `texture_axes_zup`, `material_wad_zup`,
`moving_door_button_zup`,
`point_volume_zup`, and `illusionary_static_zup`. Negative source fixtures cover script escapes,
incomplete/malformed brushes, missing targets, and strict unresolved WAD textures. A full manual QA
checklist/reporting template is available in [`docs/TrenchBroomManualQA.md`](TrenchBroomManualQA.md).

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

Use CTest group names such as `trenchbroom_package_*`, `trenchbroom_fgd_*`,
`trenchbroom_source_preflight_*`, `trenchbroom_vhlt_fixture_matrix`, `bsp_lightmaps_*`, and
`brush_mover_*` for focused CI runs.

## Troubleshooting

- `No BSP30 compiler configured`: set `STELLAR_BSP30_COMPILER` or use `--profile validate-only` for an
  existing BSP.
- Missing VHLT tools: install or copy executable `hlcsg`, `hlbsp`, `hlvis`, and `hlrad` under
  `tools/bsp/`, `tools/bsp/vhlt/`, `tools/bsp/bin/`, or set `STELLAR_VHLT_DIR`. The fixture matrix exits
  with skip code `77` when required tools are unavailable.
- `BSP header version is not 30`: use the Stellar BSP30 profile and avoid Source/VBSP compilers.
- WAD generation failures: verify `python3` is available and the build/work output directory is
  writable. VHLT needs a temporary WAD reference in the copied work map; do not add absolute WAD paths
  to source-tree `.map` files.
- Slash texture alias handling: source fixtures may use aliases such as `dev/grid_32`; the VHLT wrapper
  rewrites copied work maps to compiler-facing aliases for tools that reject slash texture names.
- `hlcsg` brush diagnostics: inspect the copied work map and stage logs under
  `build/tests/fixtures/trenchbroom/vhlt/logs/<fixture>/`. Leaks, malformed brush planes, or invalid
  texture axes can stop the VHLT pipeline before Stellar validation runs.
- Missing texture thumbnails in TrenchBroom: use the packaged PNGs/WAD and verify it with
  `create_stellar_dev_wad.py --verify`; runtime validation still uses deterministic fallback materials
  by name if external pixels are unavailable.
- Missing player spawn: add `info_player_start` at `origin = "0 0 36"` for the first room.
- Script path rejected: keep script ids asset-relative and do not use absolute paths, drive-letter
  paths, or `..` parent traversal.
- VHLT compile succeeds but validation fails with a script path escape: this is expected for negative
  fixtures such as `invalid_script_escape_zup`; fix authored `_stellar_script` or `stellar.script`
  values before launching the map.
- Runtime script missing: place referenced Lua scripts next to the map or configure the runtime script
  root explicitly.
- Client launch fails with `Authorization required` or an SDL display/startup error: this is a display
  server authorization issue, not a BSP compile issue, when display-free validation already passes. First
  confirm the map without opening a window:

  ```bash
  build/stellar-client --validate-map maps/compiled/test_room.bsp
  build/stellar-server --validate-config --map maps/compiled/test_room.bsp
  ```

  Then launch from a desktop terminal as the same user that owns the GUI session; avoid `sudo` for the
  game client. Capture the display environment and, when diagnosing Wayland/Xwayland selection, try an
  explicit SDL video driver:

  ```bash
  whoami
  echo "$DISPLAY"
  echo "$WAYLAND_DISPLAY"
  echo "$XAUTHORITY"
  echo "$SDL_VIDEODRIVER"
  build/stellar-client --validate-display
  SDL_VIDEODRIVER=x11 build/stellar-client --validate-display
  SDL_VIDEODRIVER=wayland build/stellar-client --validate-display
  ```

- VS Code integrated terminals can inherit different display/session variables than a desktop terminal.
  If launch works in a normal terminal but not in VS Code, compare the runtime environment and consider
  starting VS Code from the working desktop terminal with `code .`:

  ```bash
  echo "$DISPLAY"
  echo "$WAYLAND_DISPLAY"
  echo "$XAUTHORITY"
  echo "$XDG_RUNTIME_DIR"
  build/stellar-client --validate-map maps/compiled/test_room.bsp
  build/stellar-client --map maps/compiled/test_room.bsp
  ```

## Non-Goals Outside The Stellar BSP30 Profile

- Moving brush classes for plats, trains, or rotating entities beyond the implemented door/button path.
- Client-side gameplay scripting or renderer/audio script authority.
- Arbitrary unsafe WAD paths. Keep WAD keys relative to the map or configured search roots.
- Source/VBSP formats and Source-specific entities.
- Dynamic rigid bodies, third-party physics, PBR materials, and model animation systems.
