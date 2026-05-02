# Stellar TrenchBroom Package

This package provides Stellar's editor-facing BSP30 workflow for TrenchBroom.

## Install

1. Open TrenchBroom's game preferences.
2. Add this directory (`tools/trenchbroom/Stellar`) as a game package, or copy it into your local TrenchBroom games directory.
3. Select the **Stellar** game profile.
4. Keep map sources in `maps/src/` and compiled BSPs in `maps/compiled/` unless your project overrides the profile paths.

## Compile tools

The compilation profiles call:

- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/validate_trenchbroom_bsp30.sh`

Set `STELLAR_BSP30_COMPILER` to your BSP30-capable `.map` to `.bsp` compiler, or place a supported compiler such as `qbsp`, `ericw-qbsp`, or `hqbsp` on `PATH`.

Set `STELLAR_CLIENT` and `STELLAR_SERVER` if your built binaries are not available as `build/stellar-client` and `build/stellar-server`.

## Coordinates and scale

Stellar uses Z-up coordinates for this branch. One authored unit is one gameplay inch. Place the default player start at `origin = "0 0 36"` when the floor is at `z = 0`.

## Entity keys

The TrenchBroom FGD intentionally uses importer-supported underscore aliases such as `_stellar_script`, `_stellar_extents`, and `_stellar_collision`. These are imported as the same metadata as dotted keys such as `stellar.script`, but are safer with classic map compilers.

Do not author plain placeholder keys like `stellar_script`; they are not supported aliases.

## Materials

The runtime validates deterministic fallback materials by material name. Editor-visible WAD3 generation is deferred; use the listed names from `materials/StellarDevMaterials.txt` or make a local editor WAD with matching texture names if your TrenchBroom setup requires texture thumbnails.
