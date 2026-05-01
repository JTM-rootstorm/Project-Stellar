Prepared: 2026-05-01  
Branch target: `bsp-integration`  
Audience: Kilo Code agents / Codex-style implementation agents  
Objective: implement the next required BSP support hardening after the canonical BSP migration.

# Phase 5 — BSP Validation Tooling and Regression Fixtures

## Purpose

Make BSP support practical to iterate on by adding stronger validation diagnostics, generated-map
fixtures, and CLI-facing map validation behavior.

## Primary agents

- `@carmack`: validation core and CLI/client validation.
- `@kojima`: authoring-rule diagnostics.
- `@miyamoto`: presentation diagnostics for materials/lightmaps/visibility.
- `@director`: keeps diagnostics user-facing and docs aligned.

## Parallelism

Can run after Phases 2/3/4 have landed. Some validation work can be staged earlier, but final
diagnostic coverage should wait for all feature data to exist.

## Files likely touched

- `include/stellar/import/bsp/Loader.hpp`
- new `include/stellar/import/bsp/Validation.hpp`
- `src/import/bsp/*`
- `src/client/ApplicationConfig.cpp`
- `src/client/main.cpp`
- `tests/client/AssetValidationSmoke.cpp`
- `tests/fixtures/BspFixture.hpp`
- `tests/fixtures/BspFixtureWriter.cpp`
- new `tests/import/bsp/BspValidation.cpp`
- new `tests/integration/BspAuthoringSmoke.cpp`
- `CMakeLists.txt`
- `docs/BspAuthoring.md`
- `docs/ImplementationStatus.md`

## Validation categories

Add deterministic diagnostics for these categories.

### Binary structure

- unsupported BSP version;
- lump offset/size out of file bounds;
- lump size not a multiple of expected record size;
- face references invalid plane/texinfo/edge/surfedge/vertex;
- model references invalid face ranges;
- leaf/marksurface references invalid faces;
- clipnode references invalid planes/children.

### Geometry/collision

- no worldspawn geometry;
- no collision triangles;
- degenerate face polygons;
- duplicate collision mesh names;
- empty target names for script-addressable collision;
- brush model with no faces.

### Visibility

- visibility lump present but invalid;
- PVS offset out of range;
- decompressed PVS shorter/longer than expected;
- leaf references invalid marksurfaces.

### Materials/lightmaps

- missing texture name;
- unsupported embedded texture;
- missing external WAD texture;
- invalid light offset;
- lightmap dimensions unsupported;
- material fallback used.

### Entity authoring

- missing player spawn;
- malformed origin/vector/extent fields;
- unsupported script binding;
- script path escape;
- object collider with no model and no extents;
- trigger with no model and no extents;
- unsupported moving brush behavior.

## CLI/client requirements

Extend `stellar-client --validate-config --map path.bsp` or add `--validate-map` if cleaner.

Expected behavior:

- No window or graphics context.
- Loads BSP and builds `RuntimeWorld`.
- Runs import, collision, metadata, and BSP authoring diagnostics.
- Prints or returns deterministic errors through existing `platform::Error`/validation surfaces.
- Tests can validate failure/success without brittle human-output matching where possible.

## Fixture requirements

Extend generated BSP fixtures to cover:

- minimal valid map;
- map with visibility/PVS;
- map with lightmaps and embedded/missing textures;
- map with player spawn, trigger, sprite, object collider, collision blocker;
- malformed lump bounds;
- invalid face reference;
- invalid entity vector;
- invalid script path escape;
- missing player spawn warning;
- no collision warning/error policy.

Keep generated fixtures small and display-free. Prefer byte builders over committed binary blobs unless
a real-map fixture is explicitly approved.

## Acceptance criteria

- Validation diagnostics are structured and deterministic.
- CLI/client validation catches malformed maps without GPU/display.
- Fixture coverage exists for success and failure cases.
- Full default CTest passes.
- `docs/BspAuthoring.md` includes validation command examples and common diagnostic explanations.

## Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|bsp_authoring_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Stop conditions

Stop if validation requires loading renderer resources or WAD files by default. Validation must remain
headless and deterministic.
