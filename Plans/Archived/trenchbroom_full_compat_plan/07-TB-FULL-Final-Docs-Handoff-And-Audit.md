# TB-FULL-07 — Final Docs, Handoff, and Compatibility Audit

Target branch: `trenchbroom-compat`

## Goal

Close the full compatibility project with accurate docs, explicit validation logs, and an audit proving that no important TrenchBroom-facing feature remains deferred.

## Key files to inspect first

- `docs/ImplementationStatus.md`
- `docs/TrenchBroom.md`
- `docs/BspAuthoring.md`
- `README.md`
- `Plans/NEXT.md`
- `tools/trenchbroom/Stellar/README.md`
- `tests/fixtures/trenchbroom/README.md`
- All files changed by TB-FULL-01 through TB-FULL-06.

## Tasks

### TB-FULL-07.1 — Define final compatibility statement

Update docs to state the final supported scope precisely:

- Stellar TrenchBroom game package.
- BSP30 compile path.
- Z-up, 1 inch units.
- Materials and WAD workflow.
- Light entities and rendered BSP lightmaps.
- Runtime-supported FGD class list.
- Moving doors/buttons and target routing.
- Server-authoritative scripting.
- Validation and launch workflow.

Do not use vague wording such as "mostly compatible" or "deferred" for important editor features that were implemented by this plan.

Allowed scope boundaries:

- Source/VBSP is outside the Stellar BSP30 profile target.
- Arbitrary Quake/Half-Life game entity parity is outside the Stellar profile target unless exposed by Stellar FGD.
- TrenchBroom GUI automation is not required in CI; manual QA is user-performed.

### TB-FULL-07.2 — Remove stale deferred wording

Audit and update docs for stale statements:

- "Editor WAD generation is deferred".
- "func_door/func_button are metadata only".
- "Moving brush simulation is deferred".
- Any statement that lightmaps are imported but not rendered.
- Any statement that copied package mode is unsupported.
- Any fixture list missing new final fixtures.

Acceptance:

```bash
git grep -n -i 'deferred\|metadata only\|unsupported' -- docs tools/trenchbroom tests/fixtures Plans/NEXT.md ':!Plans/Archived/**'
```

Every hit must be either removed, updated, or intentionally scoped as non-Stellar-profile behavior.

### TB-FULL-07.3 — Final validation runbook

Record final command set in `docs/ImplementationStatus.md` and `Plans/NEXT.md`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'trenchbroom|bsp_|client_map_validation|client_cli|server_cli|render_level|brush_mover|world_axes|collision_world|runtime_world|server_world_session|scripted_world_session|networked_client_runtime|dedicated_server' --output-on-failure
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
bash -n tools/bsp/run_vhlt_fixture_matrix.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_compile.sh
bash -n tools/trenchbroom/Stellar/bin/stellar_tb_validate.sh
python3 tools/bsp/create_stellar_dev_wad.py --verify tools/trenchbroom/Stellar/materials/stellar_dev.wad
python3 tools/bsp/validate_trenchbroom_map_source.py tests/fixtures/trenchbroom/src/*.map
```

Optional with VHLT tools:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full --keep-going
```

Optional with display/GPU:

```bash
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/moving_door_button_zup.bsp
```

### TB-FULL-07.4 — Manual QA handoff form

Add a form/checklist for user results:

```text
Manual TrenchBroom QA result
Date:
OS:
TrenchBroom version:
Compiler/toolchain:
Install mode: repo-local / copied package
Map tested:
Compile profile:
Validation result:
Launch result:
Materials visible: yes/no
Lightmaps visible: yes/no
Door/button behavior: yes/no
Trigger/script behavior: yes/no
Notes/screenshots:
```

### TB-FULL-07.5 — Final audit commands

Run or document commands that prove no stale active conflicts remain:

```bash
git grep -n 'Icon.png' -- tools/trenchbroom/Stellar
find tools/trenchbroom/Stellar -maxdepth 3 -type f | sort
git grep -n '_stellar_script\|stellar_script' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'func_door\|func_button\|light_spot\|light_environment' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'dev_grid_\|dev/grid_\|stellar_dev_' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'lightmap' -- include src tests docs ':!Plans/Archived/**'
```

Interpret results in the final handoff.

## Final deliverable text for `Plans/NEXT.md`

At completion, `Plans/NEXT.md` should say:

- Full Stellar TrenchBroom BSP30 compatibility is complete.
- List exact supported install modes, map formats, entity classes, lighting/material features, moving brush behavior, and validation commands.
- List manual QA status and any user-only manual verification not run by agents.
- List true non-goals outside the Stellar BSP30 profile, not deferred important features.

## Phase done checklist

- [ ] Docs accurately describe final compatibility.
- [ ] Stale deferred wording removed or scoped.
- [ ] Final validation commands recorded.
- [ ] Manual QA checklist/form exists.
- [ ] Audit command results are summarized.
- [ ] `Plans/NEXT.md` has a clean handoff.
