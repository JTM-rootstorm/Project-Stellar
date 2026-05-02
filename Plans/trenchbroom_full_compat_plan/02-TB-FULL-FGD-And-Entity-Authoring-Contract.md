# TB-FULL-02 — FGD and Entity Authoring Contract Alignment

Target branch: `trenchbroom-compat`

## Goal

Make the Stellar FGD a truthful editor contract: every exposed class/key must be importable, validated, and either implemented at runtime or explicitly final as metadata. No important FGD class should be editor-only fiction.

## Key files to inspect first

- `tools/trenchbroom/Stellar/stellar_entities.fgd`
- `tools/bsp/stellar_entities.fgd`
- `src/import/bsp/BspLevelBuilder.cpp`
- `tests/import/bsp/BspEntityAuthoring.cpp`
- `tests/import/bsp/BspImporter.cpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`
- `tests/fixtures/trenchbroom/src/*.map`

## Compatibility contract

The final Stellar profile must support these editor categories:

1. World setup: `worldspawn`.
2. Player spawns: `info_player_start`, `info_player_deathmatch`.
3. Generic spawns: `info_stellar_spawn`.
4. Lights for compiled BSP lighting: `light`, `light_spot`, `light_environment`.
5. Trigger volumes: brush-backed and point-authored variants.
6. Object-collider sensors: brush-backed and point-authored variants.
7. Presentation sprites: `stellar_sprite`, `env_sprite`.
8. Static brush entities: `func_wall`, `func_illusionary`, `func_detail` where compiler/runtime semantics apply.
9. Moving/interactable brush entities: `func_door`, `func_button`.
10. Optional target helpers needed for entity routing: `target_stellar_relay`, `info_null`, or equivalent minimal target metadata if chosen by implementation.

Do not expose classes in the FGD unless the importer and runtime/test contract are updated in the same phase or a dependent phase in this plan.

## Tasks

### TB-FULL-02.1 — Create one authoritative FGD source

Problem: there are two FGD files with overlapping but different coverage.

Implement:

- Make `tools/trenchbroom/Stellar/stellar_entities.fgd` the authoritative TrenchBroom FGD.
- Either generate `tools/bsp/stellar_entities.fgd` from the authoritative FGD or reduce it to a documented compatibility shim that includes the same classes/keys.
- Add a small checker script/test that compares class/key coverage between the two if both remain.

Acceptance:

- No class/key exists in one FGD but not the other unless explicitly marked as package-only and tested.

### TB-FULL-02.2 — Add light entity classes

Implement FGD classes:

- `light`
  - `targetname`
  - `origin`
  - `_light` / `light` color-intensity fields as compiler-compatible choices/string.
  - `style`, `pattern`, `spawnflags` if supported by VHLT/GoldSrc compile path.
- `light_spot`
  - `targetname`
  - `origin`
  - `angles` or `angle`/`pitch` fields compatible with VHLT.
  - `_light`, `_cone`, `_cone2`, `style`.
- `light_environment`
  - `targetname`
  - direction fields.
  - color/intensity fields.

Implementation notes:

- These are primarily compile-time entities for VHLT/lightmap generation.
- Preserve their key/value metadata in import reports when retained in BSP entity text.
- Do not require runtime dynamic lighting for these classes unless later code chooses to use them for diagnostics/presentation.

Acceptance:

- TrenchBroom smart edit shows light classes.
- VHLT lit fixture compiles with light entities and produces nonempty lighting data.
- Import preserves or safely ignores light entities without failing validation.

### TB-FULL-02.3 — Add point-authored volume classes

Problem: runtime/importer supports point-authored volumes with `origin + stellar.extents`, and fixtures use this style, but the FGD currently exposes trigger/object-collider classes only as solids.

Implement one of these designs; prefer Design A unless TrenchBroom rejects it:

Design A — Add explicit point variant classnames:

- `trigger_stellar_point`
- `trigger_multiple_point`
- `trigger_once_point`
- `stellar_object_collider_point`

Importer maps them to the same `WorldMarkerType` and runtime semantics as their brush-backed equivalents.

Design B — If TrenchBroom supports duplicate class definitions safely, expose both point and solid forms with the same classname and verify smart edit behavior manually.

Required keys for point volumes:

- `targetname`
- `origin`
- `_stellar_extents`
- `_stellar_script`, `_stellar_table`
- `_stellar_once` for triggers
- `_stellar_collider`, `_stellar_enabled`, `archetype` for object colliders

Acceptance:

- Point-volume class can be placed through TrenchBroom entity browser.
- Importer tests verify new point classnames map to expected marker types.
- Existing source fixtures are updated to use the final intended classnames or FGD form.

### TB-FULL-02.4 — Add missing keys to brush classes

Implement FGD keys and importer/runtime support for:

- `archetype` on `func_wall`, `func_door`, `func_button` when gameplay/presentation uses it.
- `target`, `killtarget`, `message`, `delay` where target routing is implemented.
- `angle`/`angles`, `speed`, `wait`, `lip`, `dmg`, `spawnflags` for `func_door`.
- `angle`/`angles`, `speed`, `wait`, `lip`, `target`, `spawnflags` for `func_button`.
- `_stellar_collision` on all brush classes that can affect collision extraction.

Acceptance:

- No fixture uses an entity key that smart edit hides unless deliberately using raw custom key mode.
- Importer preserves and validates all exposed keys.

### TB-FULL-02.5 — Add `info_player_deathmatch`

Implement:

- FGD class for `info_player_deathmatch` with same size/origin guidance as `info_player_start`.
- Importer mapping to player spawn marker or multiplayer spawn marker, preserving raw properties.
- Tests for Z-up origin preservation.

Acceptance:

- Maps with only `info_player_deathmatch` validate according to chosen spawn policy.
- Docs explain when to use start vs deathmatch spawn.

### TB-FULL-02.6 — Add FGD lint tests

Implement a display-free test/script that parses enough FGD syntax to assert:

- Every class exposed by the FGD has an importer classification path or compile-only classification.
- Every `_stellar_*` alias has a dotted runtime equivalent.
- No unsupported plain aliases such as `stellar_script` are introduced.
- Solid classes and point classes are intentionally categorized.
- Required keys documented in `docs/BspAuthoring.md` match FGD class properties.

Acceptance:

```bash
ctest --test-dir build -R 'fgd|bsp_entity_authoring|bsp_importer' --output-on-failure
```

## Documentation updates

Update:

- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`
- `tools/trenchbroom/Stellar/README.md`

Required content:

- Authoritative class table.
- Which classes are compile-time only (`light*`, possibly `func_detail`).
- Which classes are runtime gameplay entities.
- Point vs brush volume authoring.
- Key/alias table.
- Example maps for lights, point trigger, brush trigger, button-door interaction.

## Phase done checklist

- [ ] FGD class/key list is complete for final Stellar profile.
- [ ] Light classes exist.
- [ ] Point-volume workflow is editor-visible.
- [ ] `func_door`/`func_button` keys are exposed for later runtime phase.
- [ ] FGD lint tests exist.
- [ ] Docs match actual FGD and importer behavior.
