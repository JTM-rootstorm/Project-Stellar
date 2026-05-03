# Phase 7 — Final Documentation, Audits, Archival, and Handoff

Branch target: `trenchbroom-compat`

## Goal

Finalize the branch so documentation, plans, tests, and handoff accurately describe the implemented TrenchBroom BSP30 Z-up workflow.

## Tasks

### 7.1 Finalize `docs/ImplementationStatus.md`

Add completion notes for each phase:

- Phase 0 — branch/handoff baseline.
- Phase 0.5 — prototype cube renderer/debug cube fallback removal.
- Phase 1 — Z-up axis contract.
- Phase 2 — runtime/collision/movement Z-up conversion.
- Phase 3 — presentation/network-adjacent Z-up conversion.
- Phase 4 — BSP30 import/validation lockdown.
- Phase 5 — TrenchBroom toolchain/FGD/materials/wrappers.
- Phase 6 — fixtures/end-to-end validation.
- Phase 7 — final docs/audits/handoff.

Include exact validation commands and results.

### 7.2 Finalize `Plans/NEXT.md`

When implementation is complete:

- Mark `trenchbroom-compat` scope complete.
- Point to archived plan package.
- List recommended next scopes, such as:
  - richer map editor workflow polish;
  - room/portal semantics beyond sealed brush rooms;
  - map distribution/caching for remote clients;
  - HUD/UI/VFX presentation;
  - miniaudio playback;
  - sprite atlas/sheet animation;
  - multiplayer expansion;
  - prediction/reconciliation;
  - moving brush simulation if explicitly selected later.

### 7.3 Archive plans

Move completed branch plans to:

```text
Plans/Archived/trenchbroom_compat/
```

Keep a short root pointer only if the repo convention requires it.

### 7.4 Finalize `docs/Design.md`

Update broad architecture:

- current branch status and/or completed scope;
- Z-up convention;
- BSP30 TrenchBroom workflow;
- 1 unit = 1 inch;
- player spawn center at z=36;
- BSP canonical runtime remains;
- server authority remains;
- no-map and remote-without-presentation-map modes use blank/static-less fallback instead of a prototype cube;
- Lua sandbox/import-never-executes-scripts remains;
- Source/VBSP and moving brush simulation remain deferred.

### 7.5 Finalize `docs/BspAuthoring.md`

Rewrite examples to Z-up/BSP30:

- minimal workflow is TrenchBroom + BSP30.
- room example:
  - floor at `z = 0`;
  - ceiling at `z = 96`;
  - X/Y horizontal footprint;
  - `info_player_start origin = "0 0 36"`.
- developer material notes.
- FGD key aliases.
- validation commands.
- unsupported/deferred list.

Remove or explicitly mark old Y-up examples as historical only. Prefer not to keep old Y-up examples in active docs.

### 7.6 Finalize `docs/TrenchBroom.md`

Make the document complete and procedural:

- install/setup;
- copy or link Stellar game profile;
- select BSP30 map format/profile;
- entity placement;
- texture selection;
- compile;
- validate;
- launch;
- troubleshooting.

### 7.7 Final grep audits

Run:

```bash
git grep -n -i 'Y is up\|Y-up\|floor at y = 0\|floor at `y = 0`\|0 36 0\|x/z spanning' -- docs include src tests tools Plans ':!Plans/Archived/**' ':!build*/**'
git grep -n '0\.0F, 1\.0F, 0\.0F\|0, 1, 0' -- include src tests ':!build*/**'
git grep -n 'stellar_script\|stellar_extents\|stellar_sprite\|stellar_collision' -- tools docs tests include src ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'BSP29/BSP30-style\|BSP29-style\|classic BSP29' -- docs tools Plans ':!Plans/Archived/**' ':!build*/**'
git grep -n 'CubeRenderer\|DebugCubeMesh\|create_debug_cube_mesh\|debug:cube\|debug_cube\|debug_cube_surface\|debug_red\|debug_green\|debug_blue' -- include src tests CMakeLists.txt docs Plans ':!Plans/Archived/**' ':!build*/**'
```

Interpretation:

- Active docs/source should not describe Y-up as current.
- `{0, 1, 0}` may remain only when it means forward +Y, not up.
- Plain FGD placeholder names should not be required by active TrenchBroom workflow.
- BSP29 mentions should be legacy-only, not the TrenchBroom target.
- Cube prototype references should be absent from active source/docs/tests except completed historical notes.

### 7.8 Final validation

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(world_axes|character_controller|collision_validation|runtime_world|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_|client_map_validation_smoke|client_cli_map_validation|dedicated_server|player_presentation|gameplay_presentation|render_level|graphics_backend_selection|client_world_receiver|networked_client_runtime|snapshot_|hud_presentation|audio_event_router)' --output-on-failure
```

Also run wrapper validation on checked-in BSP30 fixtures:

```bash
tools/bsp/validate_trenchbroom_bsp30.sh tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
tools/bsp/validate_trenchbroom_bsp30.sh tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp
tools/bsp/validate_trenchbroom_bsp30.sh tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp
```

## Expected files changed

- `docs/ImplementationStatus.md`
- `docs/Design.md`
- `docs/BspAuthoring.md`
- `docs/TrenchBroom.md`
- `Plans/NEXT.md`
- `Plans/Archived/trenchbroom_compat/**`
- possibly root plan pointers

## Acceptance criteria

- [ ] Active docs present Z-up as current.
- [ ] Active docs present BSP30 as TrenchBroom target.
- [ ] Active docs and tools no longer depend on plain non-alias FGD placeholder keys.
- [ ] Active source/docs/tests no longer contain prototype cube renderer/debug cube fallback references.
- [ ] Plans are archived or clearly marked complete.
- [ ] Full CTest passes.
- [ ] Focused TrenchBroom/BSP30/Z-up validation passes.
- [ ] Final audit results are documented in `docs/ImplementationStatus.md`.
- [ ] Branch is ready for PR/review.

## Parallelization

Safe workstreams:

- Agent A: ImplementationStatus and NEXT.
- Agent B: Design and BspAuthoring.
- Agent C: TrenchBroom docs.
- Agent D: audit and validation runner.

Do not archive plans until all final docs and validations are complete.
