# TB-FULL-06 — End-to-End Fixtures, CI Gates, and Manual TrenchBroom QA

Target branch: `trenchbroom-compat`

## Goal

Prove the final TrenchBroom workflow end-to-end with source maps, generated fixtures, optional VHLT-compiled BSPs, display-free runtime validation, and a manual editor checklist for the user.

## Key files to inspect first

- `tests/fixtures/trenchbroom/README.md`
- `tests/fixtures/trenchbroom/src/*.map`
- `tests/fixtures/BspFixture.hpp`
- `tests/fixtures/BspFixtureWriter.cpp`
- `tools/bsp/run_vhlt_fixture_matrix.sh`
- `tools/bsp/compile_trenchbroom_bsp30.sh`
- `tools/bsp/validate_trenchbroom_bsp30.sh`
- `CMakeLists.txt`
- `docs/TrenchBroom.md`

## Required final fixture matrix

Positive source fixtures:

1. `minimal_zup_room.map`
   - Basic sealed room, player spawn, developer materials.
2. `entity_matrix_zup.map`
   - All final FGD classes that can appear in one map without behavior conflicts.
3. `scripted_interaction_zup.map`
   - Server-authoritative Lua trigger and object collider.
4. `lit_zup_room.map`
   - `light`, `light_spot`, and compiled lightmaps.
5. `material_wad_zup.map`
   - External WAD texture resolution and developer alias coverage.
6. `moving_door_button_zup.map`
   - Trigger/button opens moving door with collision and presentation updates.
7. `point_volume_zup.map`
   - Final point trigger/object-collider classes.
8. `illusionary_static_zup.map`
   - Static solid vs visible nonblocking brush behavior.

Negative fixtures:

1. `invalid_script_escape_zup.map`
   - Fails validation for script path escape.
2. `invalid_incomplete_brush.map`
   - Fails source preflight before compiler.
3. `invalid_missing_target.map`
   - Warning or failure according to final target policy; must be deterministic.
4. `invalid_missing_wad_texture.map`
   - Fails strict texture validation or warns in non-strict mode according to final policy.

## Tasks

### TB-FULL-06.1 — Expand source fixture set

Implement all required source fixtures under:

```text
tests/fixtures/trenchbroom/src/
```

Rules:

- Human-readable, TrenchBroom-openable `.map` files.
- No absolute WAD paths.
- Z-up, 1 inch units.
- Complete convex brushes.
- Use final FGD classnames and aliases.
- Include comments that explain intended behavior.

Acceptance:

- Source preflight passes all positive fixtures.
- Negative fixtures fail for expected diagnostics only.

### TB-FULL-06.2 — Expand generated BSP fixture writer

If external compilers are absent in default CI, generated BSP fixture writer must create deterministic BSP30 equivalents for new behavior where feasible.

Implement generated fixture coverage for:

- Lit import metadata, using synthetic lighting lump if full VHLT output is unavailable.
- Moving door/button metadata and brush model separation.
- Point-volume classnames.
- WAD material resolution using generated WAD input or synthetic embedded miptex.

Acceptance:

- Default CTest validates runtime/import behavior without requiring VHLT.
- Generated fixtures match source fixture intent.

### TB-FULL-06.3 — Expand VHLT fixture matrix

Update `tools/bsp/run_vhlt_fixture_matrix.sh`:

- Include all positive fixtures.
- Include all negative fixtures with expected failure reasons.
- Copy required scripts beside compiled outputs.
- Configure WAD paths safely.
- Preserve logs/work maps on failure.
- Add `--fixture <name>` and `--list` options if useful for iteration.
- Ensure skip code `77` when VHLT tools are missing.

Acceptance:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full --keep-going
```

Expected with tools present:

- Positive fixtures compile and validate.
- Negative fixtures fail for expected diagnostics only.
- Summary log clearly identifies pass/fail/skip.

### TB-FULL-06.4 — Add copied-package compile smoke

Implement an automated smoke that does not require GUI:

1. Copy `tools/trenchbroom/Stellar` to a temp directory.
2. Set `STELLAR_REPO_ROOT` to checkout root.
3. Run package-local compile/validate shims with `--help`.
4. If compiler is available, compile `minimal_zup_room.map` through the copied package shim.
5. Validate output.

Acceptance:

- CTest includes this smoke.
- Skips compile sub-step cleanly if no compiler is available.

### TB-FULL-06.5 — Add full manual QA checklist

Create or update:

- `docs/TrenchBroomManualQA.md` or a dedicated section in `docs/TrenchBroom.md`.

Checklist must be written for the user to perform in TrenchBroom:

1. Install repo-local package.
2. Install copied package with `STELLAR_REPO_ROOT`.
3. Create new Stellar map.
4. Confirm coordinate/grid policy: X/Y floor, Z height, player at `0 0 36`.
5. Confirm material browser shows developer materials.
6. Confirm WAD texture workflow.
7. Place light and compile lit map.
8. Place point trigger and point object collider.
9. Place brush trigger.
10. Place sprite marker.
11. Place `func_wall`, `func_illusionary`, `func_door`, `func_button`.
12. Compile through Fast and Full profiles.
13. Validate through Stellar wrapper.
14. Launch local client and verify materials, lighting, collision, triggers, pickup, door/button, and sprite presentation.
15. Launch server/client local socket path if desired.
16. Record manual result, OS, TrenchBroom version, compiler/toolchain, and notes.

Acceptance:

- Checklist is complete enough for non-agent manual execution.
- Every manual item maps to automated coverage where possible.

### TB-FULL-06.6 — Add final CTest grouping

Add stable test names or regex-friendly naming:

- `trenchbroom_package_*`
- `trenchbroom_fgd_*`
- `trenchbroom_source_preflight_*`
- `trenchbroom_vhlt_fixture_matrix`
- `bsp_lightmaps_*`
- `brush_mover_*`

Acceptance:

```bash
ctest --test-dir build -R 'trenchbroom|bsp_lightmaps|brush_mover|bsp_authoring|client_cli|server_cli' --output-on-failure
```

## Documentation updates

Update:

- Fixture README.
- `docs/TrenchBroom.md`.
- `docs/BspAuthoring.md`.
- `docs/ImplementationStatus.md`.

Required content:

- Fixture matrix table.
- Expected positive and negative results.
- Generated vs VHLT compiled artifact policy.
- Manual QA checklist and reporting template.

## Phase done checklist

- [ ] Positive fixture set covers materials, lighting, FGD matrix, scripts, doors/buttons, point volumes.
- [ ] Negative fixture set covers script escape, malformed brush, target policy, texture policy.
- [ ] Default CI remains display-free and compiler-independent.
- [ ] Optional VHLT matrix covers real compiler output.
- [ ] Copied-package smoke exists.
- [ ] Manual QA checklist exists.
