# TB-FULL-03 — Materials, WAD, Textures, and Source Map Preflight

Target branch: `trenchbroom-compat`

## Goal

Make material and texture authoring practical for real TrenchBroom maps, not just deterministic developer fallback fixtures. Add source `.map` preflight so malformed brushes and unsupported authoring patterns fail before compiler/runtime confusion.

## Key files to inspect first

- `tools/bsp/create_stellar_dev_wad.py`
- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/trenchbroom/Stellar/materials/StellarDevMaterials.txt`
- `src/import/bsp/DeveloperTextures.cpp`
- `src/import/bsp/BspLevelBuilder.cpp`
- `tests/import/bsp/BspMaterials.cpp`
- `tests/fixtures/trenchbroom/src/*.map`
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`

## Tasks

### TB-FULL-03.1 — Normalize all developer material aliases

Problem: source fixtures use slash aliases such as `dev/grid_32`; VHLT work maps may rewrite those to compiler-facing aliases such as `dev_grid_32`; runtime fallback currently recognizes slash aliases and `stellar_dev_*` aliases but should be made exhaustive and deterministic.

Implement canonical alias support for these names:

- `dev/grid_12`, `dev/grid_16`, `dev/grid_32`, `dev/grid_64`, `dev/player_72`, `dev/wall_96`
- `dev_grid_12`, `dev_grid_16`, `dev_grid_32`, `dev_grid_64`, `dev_player_72`, `dev_wall_96`
- `stellar_dev_grid_12`, `stellar_dev_grid_16`, `stellar_dev_grid_32`, `stellar_dev_grid_64`, `stellar_dev_player_72`, `stellar_dev_wall_96`

Rules:

- Preserve authored/source material name in `LevelSurfaceMaterial::source_name`.
- Generate canonical image/texture names consistently.
- Do not emit missing texture warnings for any supported developer alias.
- Keep VHLT copied work-map rewrite behavior source-tree clean.

Acceptance:

- Import tests cover all aliases.
- VHLT-compiled fixtures render deterministic developer textures even after alias rewrite.

### TB-FULL-03.2 — Make WAD generation official

Problem: docs still describe editor-visible WAD generation as deferred, but the branch now includes a generator and WAD file.

Implement:

- Treat `tools/bsp/create_stellar_dev_wad.py` as an official tool.
- Add `--list`, `--verify`, and optional `--png-source`/`--manifest` modes if helpful.
- Add deterministic validation of `tools/trenchbroom/Stellar/materials/stellar_dev.wad` against generated output.
- Add tests for WAD header, lump names, dimensions, palette, and deterministic byte-for-byte generation.
- Document when to use PNG thumbnails, WAD textures, and runtime fallback names.

Acceptance:

```bash
python3 tools/bsp/create_stellar_dev_wad.py --out /tmp/stellar_dev.wad
python3 tools/bsp/create_stellar_dev_wad.py --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad
ctest --test-dir build -R 'bsp_materials|trenchbroom|wad' --output-on-failure
```

### TB-FULL-03.3 — Implement runtime external WAD3 texture loading

Problem: real GoldSrc/BSP30 workflows often store texture names in BSP and pixels in external WAD files. The current importer can use embedded miptex pixels and developer fallbacks, but real user WADs need runtime loading.

Implement a small BSP/WAD texture resolver:

- New module suggestion:
  - `src/import/bsp/Wad3Reader.cpp`
  - `include` or detail header as appropriate.
- Parse WAD3 header, directory, miptex lumps, palette, and indexed pixels.
- Convert WAD texture data to `ImageAsset` RGBA.
- Resolve texture names case-insensitively where classic tooling does so, but preserve source names.
- Parse `worldspawn` `wad` key from BSP entity text.
- Resolve WAD paths safely:
  1. Map directory relative path.
  2. Configured texture roots/env var such as `STELLAR_WAD_PATH` or `STELLAR_TEXTURE_PATH`.
  3. Repository package materials path for developer WAD.
- Do not trust unsafe absolute paths by default unless explicitly allowed by env/config.
- Reject `..` escapes when resolving relative to configured roots.
- Cache opened WADs during import to avoid repeated reads.

Validation behavior:

- Missing external WAD should produce actionable diagnostics.
- Display-free validation can warn by default but provide a strict mode for full compatibility fixtures.
- Launch/runtime should render loaded WAD textures when available.

Acceptance:

- Test loads a generated WAD and resolves external texture pixels into `LevelGeometryAsset::images/textures`.
- Test verifies missing WAD diagnostic includes attempted safe paths.
- Test verifies unsafe WAD path is rejected unless explicitly allowed.
- TrenchBroom material fixture with external WAD validates and renders with real texture data.

### TB-FULL-03.4 — Add source `.map` preflight validator

Problem: malformed source brushes can reach VHLT and fail late with cryptic diagnostics. Full editor compatibility needs a project-owned preflight that catches common authored map mistakes before compile.

Implement:

- `tools/bsp/validate_trenchbroom_map_source.py`

Minimum supported source syntax:

- Standard Quake map brush planes.
- Valve 220 texture axes as used by current fixtures.
- Entity blocks with quoted key/value pairs.
- Nested brush blocks under worldspawn and brush entities.
- `//` comments.

Checks:

- Entity blocks are balanced.
- Brush blocks are balanced.
- Brush has at least 4 planes; normal convex boxes have 6.
- Face plane point triples parse as finite numbers.
- Texture names are present and within compiler-compatible constraints after rewrite.
- Valve texture axes parse as finite vectors/scales.
- Required `worldspawn` exists and is first entity.
- At least one player spawn exists unless `--allow-no-spawn`.
- FGD-known classes/keys are recognized or reported as custom properties.
- `_stellar_*` values parse where possible: vectors, booleans, script paths.
- Script path escapes fail preflight.
- Target references can be optionally checked after TB-FULL-05 target routing is implemented.

Integration:

- Call preflight from `compile_trenchbroom_bsp30.sh` before invoking selected compiler unless `--skip-source-preflight` is explicitly passed.
- Call preflight from VHLT matrix on source fixtures.
- Add `--format human|json` for future editor diagnostics.

Acceptance:

- All `tests/fixtures/trenchbroom/src/*.map` pass preflight except intentional negative source fixtures.
- A negative malformed/incomplete brush fixture fails preflight with line/column information.
- Compile wrappers fail early with a clear preflight message.

### TB-FULL-03.5 — Add source map templates

Implement:

- `tools/trenchbroom/Stellar/templates/minimal_zup_room.map`
- `tools/trenchbroom/Stellar/templates/lit_zup_room.map`
- Configure TrenchBroom `initialmap` if supported by `GameConfig.cfg`; otherwise document manual template open/copy workflow.

Template requirements:

- Valid 192x192x96 Z-up sealed room.
- Player start at `0 0 36`.
- Developer materials assigned.
- Lit template includes at least one `light` entity.
- No absolute WAD paths.

Acceptance:

- Templates pass source preflight.
- Templates compile through wrapper when a compiler is available.

## Documentation updates

Update:

- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `tools/trenchbroom/Stellar/materials/StellarDevMaterials.txt`
- `tools/trenchbroom/Stellar/README.md`

Required content:

- Official developer material alias table.
- How WAD loading works.
- How to configure external WAD search paths.
- Why source maps must not commit absolute local WAD paths.
- How preflight diagnostics map to editor fixes.

## Phase done checklist

- [ ] Developer alias normalization is complete.
- [ ] WAD generation is official and verified.
- [ ] Runtime external WAD3 loading exists.
- [ ] Source `.map` preflight validator exists and is integrated into wrappers.
- [ ] Templates exist and pass preflight.
- [ ] Docs no longer describe WAD generation as deferred.
