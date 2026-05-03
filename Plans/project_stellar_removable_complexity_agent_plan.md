# Project Stellar Removable Complexity Plan

**Repository:** `JTM-rootstorm/Project-Stellar`  
**Target branch inspected:** `fixes`  
**Plan date:** 2026-05-03  
**Goal:** Identify complexity that can be removed, not merely moved, and provide an AI-agent-ready implementation plan that preserves behavior while reducing duplicated knowledge, oversized files, and repeated workflow definitions.

---

## 0. Inspection Scope and Confidence

This plan is based on static inspection of the `fixes` branch through the GitHub connector. I did **not** run a local build or test suite from this environment.

The branch already contains one meaningful complexity reduction: the root `CMakeLists.txt` is now focused on project setup, production libraries/executables, and a final `if(BUILD_TESTING) add_subdirectory(tests) endif()` gate, while test registration was moved into `tests/CMakeLists.txt`.

Remaining complexity is removable in two broad categories:

1. **Duplicated source-of-truth data**: FGD definitions, developer texture names/patterns, fixture matrices, authoring aliases, documentation tables, and validation allowlists are repeated across tools, code, tests, and docs.
2. **Oversized responsibility clusters**: `BspLevelBuilder.cpp`, `RenderLevel.cpp`, `tests/CMakeLists.txt`, `LevelAsset.hpp`, and BSP shell wrappers combine multiple responsibilities that can be split behind stable seams without changing behavior.

The highest-value theme is: **replace repeated hand-maintained knowledge with small manifests, generated/checkable outputs, and narrow modules.**

---

## 1. Agent Operating Contract

An AI agent implementing this plan must follow these constraints:

1. Work from branch `fixes`.
2. Create a working branch before edits, for example:
   ```bash
   git switch fixes
   git pull --ff-only
   git switch -c cleanup/remove-complexity-seams
   ```
3. Do not change gameplay, rendering, lighting, BSP import semantics, fixture paths, test names, or target names unless a specific task explicitly says to.
4. Prefer small behavior-preserving changes. Each task should be independently reviewable.
5. Keep default CI display-free and GPU-free.
6. Keep external BSP compiler/VHLT coverage optional and skippable with return code `77` where current behavior does so.
7. Do not commit unless explicitly instructed by the user.
8. Before each task, record the baseline commands and outputs. After each task, rerun the smallest relevant validation set.

---

## 2. Baseline Validation Before Any Refactor

Run this first and capture output in the final report.

```bash
cmake -S . -B build/complexity-baseline -DCMAKE_BUILD_TYPE=Debug
cmake --build build/complexity-baseline --target stellar-client stellar-server -j$(nproc)
ctest --test-dir build/complexity-baseline -N
```

Then run a focused smoke set:

```bash
ctest --test-dir build/complexity-baseline --output-on-failure \
  -R 'runtime_world|bsp_importer|bsp_materials|bsp_lightmaps|fgd_contract|trenchbroom_source_preflight|render_level_upload'
```

Expected acceptable skips:

- VHLT fixture matrix may skip with return code `77` if VHLT tools are absent.
- Display/OpenGL/Vulkan context tests should remain disabled unless explicitly configured.

---

## 3. Prioritized Removable Complexity Matrix

| ID | Priority | Complexity | Why removable | Primary win |
| --- | --- | --- | --- | --- |
| RC-01 | P0 | Duplicate FGD files | One file says it mirrors the other; lint is compensating for duplication | One source of truth for entity authoring contract |
| RC-02 | P0 | Developer texture definitions duplicated in C++/Python/docs/preflight | Same names, palette, patterns, aliases, and samples are repeated | One manifest drives runtime, WAD/PNG generation, docs, and validation |
| RC-03 | P1 | Fixture matrix duplicated in CMake, shell, README/docs | Positive/negative fixture lists and expected outcomes are hand-maintained in multiple places | Fixture manifest makes tests/tools/docs consistent |
| RC-04 | P1 | `BspLevelBuilder.cpp` combines entity parsing, texture resolution, geometry, lightmaps, diagnostics | It has many separable pure/helper domains | Smaller importer modules with clearer ownership |
| RC-05 | P1 | `RenderLevel` overload cluster plus debug/upload responsibilities | Render request state can be represented once; debug/upload can be extracted | Easier renderer evolution and fewer overload paths |
| RC-06 | P1 | Embedded Python inside BSP shell wrappers | Shell should orchestrate; Python snippets are reusable/testable tools | Testable map-rewrite utilities |
| RC-07 | P2 | `LevelAsset.hpp` contains data schema plus PVS/visibility algorithms | Asset contract can stay declarative; algorithms can move to query helpers | Lower header complexity and rebuild coupling |
| RC-08 | P2 | Docs repeat material tables/workflow details and have stale branch references | Docs should link or consume generated snippets; branch-specific text should be avoided | Less stale documentation |
| RC-09 | P2 | `tests/CMakeLists.txt` is now a large test registry | The root CMake split succeeded, but test registration remains monolithic | Smaller test modules and reusable fixture helpers |
| RC-10 | P3 | Temporary `Scene*` compatibility aliases | If no users remain, aliases can be deleted | Removes migration leftovers |

---

# Task Packets

Each packet below is written so an AI agent can execute it independently.

---

## RC-01 — Make the FGD Contract Single-Source-of-Truth

### Objective

Remove the need to hand-edit both:

- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`

The TrenchBroom package FGD should be authoritative. The BSP compatibility path should either point to it, be generated from it, or be verified as an exact copy without manual drift.

### Evidence

Both FGD files contain the same entity contract and comments explicitly state that the compatibility copy mirrors the authoritative TrenchBroom package FGD.

### Recommended implementation

Prefer this low-risk approach first:

1. Keep `tools/trenchbroom/Stellar/stellar_entities.fgd` as the authoritative file.
2. Add `tools/bsp/sync_stellar_fgd.py` with two modes:
   - `--check`: fail if `tools/bsp/stellar_entities.fgd` differs from the authoritative file after normalizing the header comment if needed.
   - `--write`: overwrite `tools/bsp/stellar_entities.fgd` from the authoritative file.
3. Add a CTest entry, for example `trenchbroom_fgd_sync_check`, that runs `--check`.
4. Update the header comment in `tools/bsp/stellar_entities.fgd` to state that it is generated/copied by the sync tool.
5. Leave the compatibility path in place for now to avoid breaking external scripts or user workflows.

If the project accepts symlinks across all target platforms, an even simpler follow-up can replace the compatibility copy with a symlink. Do not start with symlinks unless Windows/package portability has been considered.

### Files to inspect/change

- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`
- `tests/CMakeLists.txt`
- `tests/import/bsp/FgdContract.cpp`
- `tools/trenchbroom/lint_stellar_compilation_profiles.py` if it already compares FGD coverage

### Validation

```bash
python3 tools/bsp/sync_stellar_fgd.py --check
cmake -S . -B build/fgd-sync-check -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build/fgd-sync-check -R 'fgd|trenchbroom_fgd' --output-on-failure
```

### Acceptance criteria

- Only one FGD file is considered authoritative.
- Editing the authoritative FGD and forgetting to sync/check the compatibility copy fails a test.
- Existing FGD contract tests still pass.
- Existing paths used by tools remain valid.

### Suggested commit message

```text
Add FGD sync check for Stellar authoring contract
```

---

## RC-02 — Introduce a Developer Texture Manifest

### Objective

Remove duplicated developer texture knowledge from:

- Runtime C++ fallback texture generation.
- Python WAD/PNG generator.
- Source preflight alias allowlist.
- Documentation material tables.
- Test/sample-color expectations.

### Current complexity

`src/import/bsp/DeveloperTextures.cpp` and `tools/bsp/create_stellar_dev_wad.py` both hardcode the developer texture names, aliases, palette, and semantic patterns. `validate_trenchbroom_map_source.py` hardcodes developer texture aliases again. Docs contain the material table again.

### Recommended implementation

Add a manifest at:

```text
tools/bsp/developer_textures.json
```

Suggested shape:

```json
{
  "texture_size": 128,
  "palette": [
    [0, 0, 0],
    [32, 32, 36]
  ],
  "textures": [
    {
      "canonical": "stellar_dev_grid_12",
      "source_alias": "dev/grid_12",
      "compiler_alias": "dev_grid_12",
      "kind": "grid",
      "spacing": 12,
      "intended_scale_cue": "12 inch / 1 foot grid tile",
      "samples": [[0, 0], [1, 1], [16, 16], [32, 32], [64, 64], [96, 96], [127, 127]]
    }
  ]
}
```

Implementation sequence:

1. Add the manifest with all current developer textures:
   - `grid_12`
   - `grid_16`
   - `grid_32`
   - `grid_64`
   - `player_72`
   - `wall_96`
2. Update `tools/bsp/create_stellar_dev_wad.py` to load names, aliases, palette, sample coordinates, texture size, and scale-cue metadata from the manifest.
3. Add `--emit-cpp-header <path>` to generate a checked-in or build-generated header such as:
   ```text
   src/import/bsp/DeveloperTextureSpecs.generated.hpp
   ```
4. Update `DeveloperTextures.cpp` to consume the generated table for aliases/canonical names/pattern kind/spacing.
5. Update `validate_trenchbroom_map_source.py` to load developer aliases from the manifest or from a generated Python module.
6. Add `--emit-doc-table` to generate a Markdown material table. Use that table in docs or verify docs with a check script.

### Conservative first patch

If full C++ code generation feels too large, do this in two patches:

Patch A:
- Add manifest.
- Update Python generator and preflight validator to read it.
- Add a check that C++ hardcoded aliases still match manifest.

Patch B:
- Generate and consume C++ spec table.

### Files to inspect/change

- `src/import/bsp/DeveloperTextures.cpp`
- `src/import/bsp/DeveloperTextures.hpp`
- `tools/bsp/create_stellar_dev_wad.py`
- `tools/bsp/validate_trenchbroom_map_source.py`
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`
- `tests/import/bsp/BspMaterials.cpp`
- `tests/graphics/RenderSceneInspection.cpp` if it asserts material names/samples

### Validation

```bash
python3 tools/bsp/create_stellar_dev_wad.py --list
python3 tools/bsp/create_stellar_dev_wad.py --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad
python3 tools/bsp/create_stellar_dev_wad.py --verify-png tools/trenchbroom/Stellar/materials
python3 tools/bsp/validate_trenchbroom_map_source.py tests/fixtures/trenchbroom/src/minimal_zup_room.map
cmake -S . -B build/dev-texture-manifest -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev-texture-manifest --target stellar_import_bsp_test stellar_bsp_materials_test -j$(nproc)
ctest --test-dir build/dev-texture-manifest -R 'bsp_materials|trenchbroom_wad|trenchbroom_dev_png|trenchbroom_source_preflight' --output-on-failure
```

### Acceptance criteria

- Adding/removing a developer texture requires editing one manifest.
- WAD/PNG generation, runtime fallback, preflight validation, and docs all agree.
- Generated WAD and PNG outputs remain deterministic.
- Existing texture names and aliases remain accepted.

### Suggested commit message

```text
Centralize developer texture definitions in a manifest
```

---

## RC-03 — Add a TrenchBroom Fixture Manifest

### Objective

Remove duplicated fixture matrix knowledge from:

- `tests/CMakeLists.txt`
- `tools/bsp/run_vhlt_fixture_matrix.sh`
- `tests/fixtures/trenchbroom/README.md`
- `docs/TrenchBroom.md`
- possibly integration tests that hardcode generated fixture paths

### Recommended manifest

Add:

```text
tests/fixtures/trenchbroom/fixtures.json
```

Suggested shape:

```json
{
  "fixtures": [
    {
      "name": "minimal_zup_room",
      "source": "src/minimal_zup_room.map",
      "kind": "positive",
      "expected_bsp_version": 30,
      "generated_bsp": true,
      "vhlt": true,
      "scripts_to_copy": []
    },
    {
      "name": "invalid_incomplete_brush",
      "source": "src/invalid_incomplete_brush.map",
      "kind": "negative",
      "preflight_expected_regex": "fewer than 4 planes|unclosed|expected Valve 220",
      "strict_textures": false,
      "compile_after_preflight_failure": false
    }
  ]
}
```

### Implementation sequence

1. Add the manifest with all current positive and negative fixtures.
2. Add `tools/bsp/list_trenchbroom_fixtures.py` with output modes:
   - `--positive-names`
   - `--negative-names`
   - `--shell-arrays`
   - `--cmake-fragments` or `--json` for CMake parsing
   - `--markdown-table`
3. Update `run_vhlt_fixture_matrix.sh` to use generated shell arrays or call the Python tool directly.
4. Add a CTest check that validates the shell runner and README/docs fixture lists are in sync with the manifest.
5. In a follow-up patch, reduce `tests/CMakeLists.txt` fixture duplication by iterating over manifest entries using CMake `string(JSON ...)` or a generated CMake include.
6. Generate or verify the README fixture matrix table from the manifest.

### Files to inspect/change

- `tests/fixtures/trenchbroom/README.md`
- `tests/CMakeLists.txt`
- `tools/bsp/run_vhlt_fixture_matrix.sh`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`

### Validation

```bash
python3 tools/bsp/list_trenchbroom_fixtures.py tests/fixtures/trenchbroom/fixtures.json --positive-names
python3 tools/bsp/list_trenchbroom_fixtures.py tests/fixtures/trenchbroom/fixtures.json --negative-names
bash tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build/fixture-manifest --list
cmake -S . -B build/fixture-manifest -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build/fixture-manifest -R 'trenchbroom_source_preflight|trenchbroom_package|fixture' --output-on-failure
```

### Acceptance criteria

- Fixture identity and expected outcome are declared once.
- Shell matrix, CMake tests, and docs agree with the manifest.
- Current positive/negative coverage remains unchanged.
- Existing generated output paths remain unchanged.

### Suggested commit message

```text
Add manifest for TrenchBroom fixture matrix
```

---

## RC-04 — Split `BspLevelBuilder.cpp` by Responsibility

### Objective

Reduce the conceptual size of BSP import by extracting focused modules. This is not just moving code; it creates stable seams so future map-editor, lighting, texture, collision, and entity work can be changed independently.

### Current responsibility cluster

`BspLevelBuilder.cpp` currently mixes:

- entity key lookup and alias resolution,
- vector/float/bool parsing,
- canonical entity naming,
- lighting mode/global ambient parsing,
- texture lookup and fallback resolution,
- embedded miptex/WAD/developer texture path,
- material index creation,
- lightmap stats and diagnostics,
- face vertex extraction,
- geometric helpers,
- import report diagnostics,
- mesh/surface construction.

### Target module layout

Create these internal modules under `src/import/bsp/`:

```text
BspEntitySchema.hpp/.cpp
BspGeometryBuilder.hpp/.cpp
BspTextureResolver.hpp/.cpp
BspLightmapBuilder.hpp/.cpp
BspImportDiagnostics.hpp/.cpp
BspAssetDebugNames.hpp/.cpp
```

Suggested ownership:

#### `BspEntitySchema`

- `value_for`
- `value_for_alias`
- `string_or`
- `string_or_alias`
- canonical entity name key list
- `canonical_entity_name_for`
- `parse_vec2`
- `parse_vec3`
- `parse_bool_like`
- `parse_float_value`
- name/key aliases used by importer

#### `BspTextureResolver`

- texture lookup key normalization
- `texture_name_for`
- `texture_for_face`
- embedded miptex handling
- WAD texture fallback
- developer texture fallback
- material index key construction

#### `BspLightmapBuilder`

- lightmap mode inference
- face lightmap reference counts
- lightmap image extraction
- lightmap UV bounds/extent logic
- lightmap RGB stats
- lightmap diagnostics

#### `BspGeometryBuilder`

- `Vec3` helpers
- `subtract`
- `cross`
- `normalize`
- `include_point`
- `face_vertices`
- polygon/primitive conversion helpers

#### `BspImportDiagnostics`

- `add_warning`
- `add_info`
- `add_entity_warning`
- helper builders for repeated diagnostic strings

#### `BspAssetDebugNames`

- stringification helpers shared with renderer if practical:
  - texture filter names,
  - wrap names,
  - image format names,
  - texture source kind.

### Implementation rules

1. Do not redesign algorithms in the same patch as extraction.
2. Move one helper group at a time and run focused tests after each group.
3. Keep namespaces internal: `stellar::import::bsp::detail`.
4. Avoid adding public headers unless required by tests or another module.
5. Keep CMake target `stellar_import_bsp` updated with the new `.cpp` files.
6. Preserve diagnostic text unless a test update is explicitly justified.

### Files to inspect/change

- `src/import/bsp/BspLevelBuilder.cpp`
- `src/import/bsp/BspBinary.hpp`
- `src/import/bsp/DeveloperTextures.cpp/.hpp`
- `src/import/bsp/Wad3Reader.cpp/.hpp`
- `include/stellar/import/bsp/Diagnostics.hpp`
- `CMakeLists.txt`
- `tests/import/bsp/*`

### Validation after each extraction group

```bash
cmake -S . -B build/bsp-builder-split -DCMAKE_BUILD_TYPE=Debug
cmake --build build/bsp-builder-split --target \
  stellar_import_bsp_test \
  stellar_bsp_entity_authoring_test \
  stellar_bsp_materials_test \
  stellar_bsp_lightmaps_test \
  stellar_bsp_validation_test \
  -j$(nproc)
ctest --test-dir build/bsp-builder-split --output-on-failure \
  -R 'bsp_importer|bsp_entity_authoring|bsp_materials|bsp_lightmaps|bsp_validation|fgd_contract'
```

### Acceptance criteria

- `BspLevelBuilder.cpp` becomes orchestration rather than a helper dump.
- Each extracted module has a narrow name and purpose.
- BSP importer tests pass with unchanged behavior.
- No public gameplay/rendering contracts change.

### Suggested commit sequence

```text
Extract BSP entity schema helpers
Extract BSP texture resolver helpers
Extract BSP lightmap builder helpers
Extract BSP geometry builder helpers
Extract BSP import diagnostic helpers
```

---

## RC-05 — Collapse `RenderLevel` Overloads into a Render Request

### Objective

Remove render-call complexity by representing a frame once instead of adding overloads for each combination of view, visibility culling, and billboards.

### Current complexity

`RenderLevel` exposes several overloads:

- static level render with view-projection and view,
- static level render with optional camera position,
- static plus billboards,
- static plus billboards plus visibility culling,
- compatibility fallback with only view-projection.

Internally, these already converge on one private render method. Make that convergence explicit in the public API.

### Recommended implementation

Add a request struct to `include/stellar/graphics/RenderLevel.hpp`:

```cpp
struct RenderLevelFrame {
  int width = 0;
  int height = 0;
  std::array<float, 16> view_projection{};
  std::array<float, 16> view{};
  std::optional<std::array<float, 3>> camera_world_position;
  const BillboardView *billboard_view = nullptr;
  std::span<const BillboardSprite> sprites{};
};
```

Add:

```cpp
void render(const RenderLevelFrame &frame) noexcept;
```

Then make all existing overloads forward to this one method. Do not remove old overloads until all call sites are migrated.

### Follow-up extraction

After the request struct is stable, extract from `RenderLevel.cpp`:

```text
src/graphics/RenderDebugOptions.hpp/.cpp
src/graphics/RenderLevelDebugLog.hpp/.cpp
src/graphics/RenderLevelResourceUpload.hpp/.cpp
src/graphics/AssetDebugNames.hpp/.cpp
```

Good candidates:

- environment flag parsing,
- frame limit parsing,
- lightmap/texture debug logging,
- texture/image/material stringification,
- lightmap upload and black-fallback upload logic,
- material upload assembly.

### Files to inspect/change

- `include/stellar/graphics/RenderLevel.hpp`
- `src/graphics/RenderLevel.cpp`
- `src/graphics/LevelRenderer.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- backend files if material upload expectations change

### Validation

```bash
cmake -S . -B build/render-request -DCMAKE_BUILD_TYPE=Debug
cmake --build build/render-request --target \
  stellar_render_level_upload_test \
  stellar_render_level_inspection_test \
  stellar-client \
  -j$(nproc)
ctest --test-dir build/render-request --output-on-failure \
  -R 'render_level_upload|render_level_inspection|graphics_backend_selection|client_map_validation_smoke'
```

### Acceptance criteria

- One canonical public render request exists.
- Existing overloads remain behavior-preserving wrappers.
- Render tests pass.
- Future render options can be added as fields, not as overload combinations.

### Suggested commit message

```text
Add canonical RenderLevel frame request
```

---

## RC-06 — Extract Embedded Python from BSP Shell Wrappers

### Objective

Remove inline Python scripts from Bash wrappers so map/BSP rewrite logic is reusable and unit-testable.

### Current complexity

`tools/bsp/compile_vhlt_bsp30.sh` embeds Python for several transformations:

- normalizing BSP entity lump length,
- injecting/replacing worldspawn keys,
- injecting `wad` and `mapversion`,
- rewriting developer texture aliases,
- rewriting Valve 220 texture axes to classic texture axes.

### Recommended implementation

Create:

```text
tools/bsp/map_rewrite.py
```

Subcommands:

```bash
python3 tools/bsp/map_rewrite.py normalize-bsp-entity-lump --bsp path/to/map.bsp
python3 tools/bsp/map_rewrite.py inject-worldspawn-key --map path/to/map.map --key wad --value path/to/stellar_dev.wad
python3 tools/bsp/map_rewrite.py inject-mapversion --map path/to/map.map --value 220
python3 tools/bsp/map_rewrite.py rewrite-dev-texture-names --map path/to/map.map
python3 tools/bsp/map_rewrite.py rewrite-valve220-to-classic --map path/to/map.map
```

Then update `compile_vhlt_bsp30.sh` to call the Python utility.

### Files to inspect/change

- `tools/bsp/compile_vhlt_bsp30.sh`
- `tools/bsp/normalize_vhlt_light_angles.py`
- `tools/bsp/test_normalize_vhlt_light_angles.py`
- Add `tools/bsp/test_map_rewrite.py`
- `tests/CMakeLists.txt` for the new test entry

### Validation

```bash
python3 tools/bsp/test_map_rewrite.py
python3 tools/bsp/test_normalize_vhlt_light_angles.py
bash tools/bsp/compile_vhlt_bsp30.sh --help
cmake -S . -B build/map-rewrite -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build/map-rewrite -R 'trenchbroom_vhlt_light_angle_normalizer|map_rewrite|trenchbroom_vhlt_preserves_valve220_axes' --output-on-failure
```

If VHLT tools are absent, ensure related external-tool tests still skip cleanly.

### Acceptance criteria

- Bash wrapper no longer contains substantial inline Python logic.
- Map/BSP rewrite behavior is covered by direct Python tests.
- VHLT wrapper command-line behavior remains unchanged.

### Suggested commit message

```text
Extract BSP map rewrite helpers from VHLT shell wrapper
```

---

## RC-07 — Move Visibility/PVS Queries Out of `LevelAsset.hpp`

### Objective

Keep `LevelAsset.hpp` as a data contract, not a mixed data-and-algorithm header.

### Current complexity

`LevelAsset.hpp` defines level geometry, lighting, brush entity, leaf, and visibility data structures. It also implements point-to-leaf lookup, PVS decompression, and surface visibility mask generation inline.

### Recommended implementation

Create:

```text
include/stellar/assets/LevelVisibilityQueries.hpp
```

Move these functions there:

- `find_level_leaf_for_point`
- `visible_level_surfaces_from_leaf`
- PVS decompression helper

Keep the functions header-only at first to avoid turning `stellar_assets` from an interface target into a compiled library. This still removes conceptual clutter from `LevelAsset.hpp` without forcing a build-system change.

Later, if the project accepts a compiled `stellar_assets` target, move the function bodies into:

```text
src/assets/LevelVisibilityQueries.cpp
```

### Files to inspect/change

- `include/stellar/assets/LevelAsset.hpp`
- New `include/stellar/assets/LevelVisibilityQueries.hpp`
- `src/graphics/RenderLevel.cpp`
- any tests using visibility helpers

### Validation

```bash
cmake -S . -B build/visibility-queries -DCMAKE_BUILD_TYPE=Debug
cmake --build build/visibility-queries --target stellar_render_level_inspection_test stellar_bsp_visibility_test -j$(nproc)
ctest --test-dir build/visibility-queries -R 'bsp_visibility|render_level_inspection' --output-on-failure
```

### Acceptance criteria

- `LevelAsset.hpp` primarily contains asset data structures.
- Visibility query call sites include `LevelVisibilityQueries.hpp` explicitly.
- Tests pass with no behavior changes.

### Suggested commit message

```text
Move level visibility queries out of LevelAsset header
```

---

## RC-08 — Consolidate BSP/TrenchBroom Documentation

### Objective

Remove duplicated and stale documentation. Keep docs accurate by making each page own one purpose.

### Current issues

Documentation currently repeats:

- developer texture table,
- workflow and validation commands,
- FGD alias policy,
- fixture matrix details,
- compile wrapper behavior,
- non-goals.

Some current branch references still name `trenchbroom-compat`, which is stale when reading the `fixes` branch.

### Recommended ownership split

Use this documentation ownership model:

- `docs/TrenchBroom.md`: editor workflow, install/configuration, compile/validate/launch, troubleshooting.
- `docs/BspAuthoring.md`: runtime entity/key semantics, import diagnostics, script/collision/presentation notes.
- `tests/fixtures/trenchbroom/README.md`: fixture matrix and manual fixture QA only.
- Generated snippets or manifest-derived tables: developer texture table and fixture matrix where possible.

### Implementation steps

1. Replace branch-specific prose like `Branch target: trenchbroom-compat` with branch-neutral wording such as `Supported BSP30 editor workflow`.
2. Replace duplicated developer texture tables with a generated snippet from `developer_textures.json`, or link to one canonical table until RC-02 is complete.
3. Replace duplicated fixture lists with either a generated table from `fixtures.json` or a link to `tests/fixtures/trenchbroom/README.md`.
4. Add a docs lint/check script:
   ```bash
   python3 tools/docs/check_docs_consistency.py
   ```
   Initial checks:
   - no stale `trenchbroom-compat` outside archived plans unless intentionally allowed,
   - material names match developer texture manifest,
   - fixture names match fixture manifest.

### Files to inspect/change

- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `tests/fixtures/trenchbroom/README.md`
- `docs/TrenchBroomManualQA.md` if present
- Add `tools/docs/check_docs_consistency.py`

### Validation

```bash
python3 tools/docs/check_docs_consistency.py
cmake -S . -B build/docs-consistency -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build/docs-consistency -R 'docs|trenchbroom_package|trenchbroom_source_preflight' --output-on-failure
```

### Acceptance criteria

- No stale branch name appears in active docs unless intentionally documented.
- Developer texture and fixture docs do not drift from manifests.
- Docs link rather than duplicate where a page does not own the detail.

### Suggested commit message

```text
Consolidate TrenchBroom and BSP authoring docs
```

---

## RC-09 — Split and De-duplicate `tests/CMakeLists.txt`

### Objective

Finish the complexity reduction started by moving tests out of root CMake. The current `tests/CMakeLists.txt` is better than the old root file, but it is still a large hand-maintained registry.

### Recommended structure

```text
tests/
├── CMakeLists.txt
└── cmake/
    ├── TestHelpers.cmake
    ├── WorldTests.cmake
    ├── ServerTests.cmake
    ├── ClientTests.cmake
    ├── GraphicsTests.cmake
    ├── BspTests.cmake
    ├── TrenchBroomTests.cmake
    ├── ScriptingTests.cmake
    ├── NetworkTests.cmake
    └── AudioPlatformTests.cmake
```

### Helper functions to add

Add higher-level helpers for repeated patterns:

```cmake
stellar_add_fixture_writer_test(
    TEST_NAME <name>
    OUTPUT <path>
    FIXTURE_ID <fixture>
    FIXTURES_SETUP <setup-name>
)

stellar_add_validate_map_test(
    TEST_NAME <name>
    MAP <path>
    FIXTURES_REQUIRED <setup-name>
    COMMAND_TARGET stellar-client
    ARGS --validate-map
)

stellar_add_expected_failure_test(
    TEST_NAME <name>
    COMMAND ...
)
```

### Implementation rules

1. Preserve all existing CTest names.
2. Preserve all existing CMake target names.
3. Preserve fixture output locations under `${CMAKE_BINARY_DIR}/tests/fixtures/...`.
4. Do not combine this with fixture manifest consumption unless RC-03 is already merged and validated.
5. Make `tests/CMakeLists.txt` an include hub only.

### Validation

```bash
cmake -S . -B build/tests-cmake-split -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build/tests-cmake-split -N > /tmp/tests-after.txt
cmake --build build/tests-cmake-split --target \
  stellar_runtime_world_test \
  stellar_import_bsp_test \
  stellar_bsp_fixture_writer \
  stellar_client_connect_test \
  stellar_lua_runtime_test \
  -j$(nproc)
ctest --test-dir build/tests-cmake-split --output-on-failure \
  -R 'runtime_world|bsp_importer|lua_runtime|client_connect|trenchbroom_source_preflight'
```

Compare the `ctest -N` output to the baseline captured before this task. Differences must be intentional and explained.

### Acceptance criteria

- `tests/CMakeLists.txt` becomes a small include hub.
- Domain test files are easier to navigate.
- Test names, targets, and fixture paths remain stable.

### Suggested commit message

```text
Split test CMake registrations by domain
```

---

## RC-10 — Remove Temporary `Scene*` Compatibility Aliases If Unused

### Objective

Remove migration leftovers after confirming there are no remaining users.

### Current candidates

`include/stellar/graphics/LevelRenderer.hpp` contains temporary aliases:

- `SceneBounds = LevelBounds`
- `SceneCameraFit = LevelCameraFit`
- `SceneRenderer = LevelRenderer`

### Implementation steps

1. Search the repository:
   ```bash
   git grep -n '\bSceneBounds\b\|\bSceneCameraFit\b\|\bSceneRenderer\b'
   ```
2. If only the alias definitions remain, delete them.
3. If references remain, migrate them to `Level*` names and then delete aliases.

### Validation

```bash
cmake -S . -B build/remove-scene-aliases -DCMAKE_BUILD_TYPE=Debug
cmake --build build/remove-scene-aliases --target stellar-client stellar_render_level_inspection_test -j$(nproc)
ctest --test-dir build/remove-scene-aliases -R 'render_level|client' --output-on-failure
```

### Acceptance criteria

- No `Scene*` graphics compatibility aliases remain unless an external API reason is documented.
- All internal references use `Level*` naming.

### Suggested commit message

```text
Remove obsolete Scene renderer compatibility aliases
```

---

# Recommended Execution Order

Use this order to remove complexity while minimizing regression risk:

1. **RC-01 FGD single source** — low risk, immediate duplicate removal.
2. **RC-06 shell/Python extraction** — localized, improves testability.
3. **RC-02 developer texture manifest** — high value, touches tools/runtime/docs; do in conservative patches.
4. **RC-03 fixture manifest** — high value, enables later CMake/docs simplification.
5. **RC-08 docs consolidation** — best after RC-02/RC-03 manifests exist.
6. **RC-09 tests CMake split** — easier after fixture manifest or helper functions exist.
7. **RC-04 BSP builder split** — largest code refactor; do mechanically with tests after each moved group.
8. **RC-05 RenderLevel request struct/extractions** — moderate renderer risk; keep wrappers initially.
9. **RC-07 Level visibility queries** — simple but touches public includes; do after render/import smoke is stable.
10. **RC-10 compatibility aliases** — cleanup once grep confirms safety.

---

# Final Reporting Template for the Agent

When handing the work back, report this exact structure:

```markdown
## Summary
- Removed/simplified:
- Behavior intentionally preserved:
- Files changed:

## Validation run
- Configure:
- Build targets:
- CTest filters:
- Skipped tests and why:
- Tests not run and why:

## Risk notes
- Potential compatibility concerns:
- Follow-up tasks:

## Diff map
- <file>: <why changed>
```

---

# Non-Goals

Do not perform these changes as part of this cleanup unless separately requested:

- Do not change BSP coordinate scale or Z-up policy.
- Do not alter gameplay authority or client/server ownership rules.
- Do not remove VHLT or generic compiler support.
- Do not require external BSP tools for default CI.
- Do not convert the project to a different build system.
- Do not introduce a third-party manifest parser into runtime C++ unless the dependency is already accepted.
- Do not rewrite the BSP importer architecture and change behavior in the same patch.

---

# One-Sentence Answer

Yes: the most removable complexity is repeated project knowledge—FGD classes, developer texture definitions, fixture matrices, aliases, and docs—plus oversized importer/renderer/test-wrapper files that can be reduced once those duplicated sources of truth are centralized.
