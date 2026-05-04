# Stellar TrenchBroom Package

This package provides Stellar's editor-facing BSP30 workflow for TrenchBroom.

## Install

Supported install modes:

1. **Repo-local:** add `tools/trenchbroom/Stellar/` from the checkout as the game package.
2. **Copied package:** copy or link this package into TrenchBroom's user games directory and point it
   at the checkout with `STELLAR_REPO_ROOT` or package-local `.stellar_repo_root`.

Helper install from the repository root:

```bash
tools/trenchbroom/install_stellar_game_package.sh --dest "$HOME/.TrenchBroom/games" --copy
tools/trenchbroom/install_stellar_game_package.sh --dest "$HOME/.TrenchBroom/games" --link
```

Manual copied-package setup:

```bash
cp -a tools/trenchbroom/Stellar "$HOME/.TrenchBroom/games/Stellar"
printf '%s\n' "$PWD" > "$HOME/.TrenchBroom/games/Stellar/.stellar_repo_root"
```

Then open TrenchBroom's game preferences, add or refresh the package directory, and select the
**Stellar** game profile. Keep map sources in `maps/src/` and compiled BSPs in `maps/compiled/` unless
your project overrides the profile paths.

## Compile tools

The compilation profiles call package-local shims:

- `bin/stellar_tb_compile.sh`
- `bin/stellar_tb_validate.sh`

Those shims locate the checkout and delegate to:

- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/validate_trenchbroom_bsp30.sh`

Repository root resolution order is `STELLAR_REPO_ROOT`, package `.stellar_repo_root`, walking upward
for repo-local installs, then the current working directory if it is a Stellar checkout. Paths passed by
the compile profiles are quoted for checkouts and maps with spaces.

Set `STELLAR_BSP30_COMPILER` to your BSP30-capable `.map` to `.bsp` compiler, or place a supported
compiler such as `qbsp`, `ericw-qbsp`, or `hqbsp` on `PATH`.

Set `STELLAR_CLIENT` and `STELLAR_SERVER` if your built binaries are not available as
`build/stellar-client` and `build/stellar-server`.

Copied packages inherit the same external tool environment as repo-local packages: `STELLAR_CLIENT`,
`STELLAR_SERVER`, `STELLAR_BSP30_TOOLCHAIN`, `STELLAR_BSP30_COMPILER`, `QBSP`, `STELLAR_VHLT_DIR`,
`HLCSG`, `HLBSP`, `HLVIS`, `HLRAD`, and `RIPENT`.

## Expected package files

- `GameConfig.cfg`
- `CompilationProfiles.cfg`
- `stellar_entities.fgd`
- `Icon.png`
- `materials/StellarDevMaterials.txt`
- `materials/stellar_dev.wad`
- `templates/minimal_zup_room.map`
- `templates/lit_zup_room.map`
- `bin/stellar_tb_compile.sh`
- `bin/stellar_tb_validate.sh`
- `bin/stellar_tb_env.sh`

## Troubleshooting

- Missing repository root: export `STELLAR_REPO_ROOT=/path/to/Stellar_Engine` or rerun the install
  helper in `--copy` mode so `.stellar_repo_root` is written.
- Spaces in paths: use the provided compilation profiles and shims; do not remove the quotes around
  `${MAP_FULL_PATH}` or `${WORK_DIR_PATH}` in `CompilationProfiles.cfg`.
- Missing VHLT tools: set `STELLAR_VHLT_DIR`, individual `HLCSG`/`HLBSP`/`HLVIS`/`HLRAD` variables, or
  use the generic `STELLAR_BSP30_COMPILER`/`QBSP` path.
- Missing Stellar binaries during validation: build the project or set `STELLAR_CLIENT` and
  `STELLAR_SERVER` to executable paths.

## Coordinates and scale

Stellar uses Z-up coordinates for this branch. One authored unit is one gameplay inch. Place the default
player start at `origin = "0 0 36"` when the floor is at `z = 0`.

## Entity keys

The TrenchBroom FGD intentionally uses importer-supported underscore aliases such as
`_stellar_script`, `_stellar_extents`, and `_stellar_collision`. These are imported as the same metadata
as dotted keys such as `stellar.script`, but are safer with classic map compilers.

Entity targeting uses the classic BSP contract: `classname` selects the entity type, `targetname` is the
entity's targetable Name, and `target` points to another entity's `targetname`. Triggers and
`func_button` can fire targets; `func_door` is opened by firing its `targetname` and does not expose door
output fields yet.

Do not author plain placeholder keys like `stellar_script`; they are not supported aliases.

## Compile-time lighting

Author `light_spot` and `light_environment` orientation in TrenchBroom degrees. `pitch` `90` points down,
`pitch` `270` or `-90` points up, `pitch` `0` is horizontal, and yaw/`angle` controls horizontal
direction. Classic `light_spot` and `light_environment` ignore roll in `angles "pitch yaw roll"`.

The VHLT compile wrapper converts TrenchBroom/editor-facing pitch to VHLT/GoldSrc pitch on the copied
work map before `hlrad`; authored source maps are not modified. If a `light_spot` has `target`, VHLT may
aim the spotlight at the target and ignore `angle`/`pitch`/`angles`.

## Materials

Use the package-local `materials/stellar_dev.wad`, PNG thumbnails, and names listed in
`materials/StellarDevMaterials.txt` for editor-visible developer materials. Runtime validation uses
deterministic fallback materials by material name.

Optional runtime `.stellar_material` sidecars can live under material search roots using BSP texture
names as keys, such as `materials/dev/wall_96.stellar_material` for `dev/wall_96`. Sidecars may bind
normal/specular textures for presentation shading only; missing sidecars keep existing BSP texture and
lightmap behavior, and gameplay/networking does not depend on them.

Verify the packaged WAD from the repository root with:

```bash
python3 tools/bsp/create_stellar_dev_wad.py --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad
```

Keep source-map `wad` keys relative. Configure extra runtime/import search roots with
`STELLAR_WAD_PATH` or `STELLAR_TEXTURE_PATH`; do not commit absolute local WAD paths or `..` escapes.

## Templates

`templates/minimal_zup_room.map` and `templates/lit_zup_room.map` are 192x192x96 Z-up starter rooms
with `info_player_start` at `0 0 36`. `GameConfig.cfg` uses the minimal room as the initial map when
TrenchBroom honors `initialmap`; otherwise open a template manually and save a copy under `maps/src/`.

The compile shims run source preflight before external compilers. Run it manually with:

```bash
python3 tools/bsp/validate_trenchbroom_map_source.py maps/src/your_map.map
```
