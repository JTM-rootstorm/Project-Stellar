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

Do not author plain placeholder keys like `stellar_script`; they are not supported aliases.

## Materials

Use the package-local `materials/stellar_dev.wad`, PNG thumbnails, and names listed in
`materials/StellarDevMaterials.txt` for editor-visible developer materials. Runtime validation uses
deterministic fallback materials by material name.
