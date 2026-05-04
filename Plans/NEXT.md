# Stellar Engine - Next Scope Handoff

Status scope: completed Stellar TrenchBroom BSP30 compatibility follow-up notes.

## Current entry point

`docs/ImplementationStatus.md` is the source of truth for branch status. The active scope is
complete TrenchBroom BSP30 compatibility and the Z-up migration. The completed phase package is
archived under:

- `Plans/Archived/trenchbroom_compat/TrenchBroomCompat-AgentPlan.md`
- `Plans/Archived/trenchbroom_compat/trenchbroom_compat_plans/00-MASTER-TrenchBroomCompat-AgentPlan.md`

## Completed TrenchBroom BSP30 Scope

Full Stellar TrenchBroom BSP30 compatibility is complete for the project-owned Stellar game package.
The branch uses Z-up runtime and authoring conventions while preserving the 1 Stellar gameplay unit =
1 authored inch policy. The supported TrenchBroom workflow targets BSP30-authored maps with imported
coordinates preserved 1:1.

Completed branch outcomes:

- Runtime, movement, collision, presentation, and BSP fixture assumptions use Z-up; default player
  spawn centers are authored at `z = 36` above a floor at `z = 0` for the 72 inch capsule.
- The supported editor workflow is the Stellar TrenchBroom package in repo-local or copied-package
  mode, with copied packages resolving the checkout through `STELLAR_REPO_ROOT` or `.stellar_repo_root`.
- TrenchBroom-authored BSP30 maps are the primary authoring workflow. Source/VBSP and arbitrary
  non-Stellar entity/profile parity are outside the Stellar BSP30 profile.
- No-map and remote-without-local-map presentation use a blank/static-less fallback.
- The repo includes a TrenchBroom game config, FGD aliases, developer material references, BSP30
  compile/validation wrappers, source-map fixtures, generated BSP30 test fixtures, package-local
  compile/validate shims, `Icon.png`, template maps, and manual QA documentation.
- Materials use packaged developer PNGs and `materials/stellar_dev.wad`; runtime import resolves safe
  WAD3 textures and deterministic developer fallbacks.
- `light`, `light_spot`, and `light_environment` feed BSP lightmap generation; imported lightmaps are
  rendered by multiplying static surface base materials/textures by baked lighting where valid.
- Runtime-supported Stellar FGD classes are `worldspawn`, `info_player_start`, `info_stellar_spawn`,
  trigger brush and point variants, `stellar_sprite`, `env_sprite`, brush and point object colliders,
  compile-time light classes, `func_wall`, `func_illusionary`, `func_detail`, `func_door`, and
  `func_button`.
- `func_door` and `func_button` have server-authoritative target routing, movement state, collision
  overlay transforms, and snapshot-owned presentation transforms.
- Preserve server authority, mandatory sandboxed Lua, BSP canonical runtime, and display-free
  validation.
- Manual TrenchBroom GUI QA is user-performed using `docs/TrenchBroomManualQA.md`; GUI automation is
  not required in CI.

## Final validation and audit runbook

Default/focused validation:

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
for map in \
  tests/fixtures/trenchbroom/src/minimal_zup_room.map \
  tests/fixtures/trenchbroom/src/entity_matrix_zup.map \
  tests/fixtures/trenchbroom/src/scripted_interaction_zup.map \
  tests/fixtures/trenchbroom/src/lit_zup_room.map \
  tests/fixtures/trenchbroom/src/material_wad_zup.map \
  tests/fixtures/trenchbroom/src/moving_door_button_zup.map \
  tests/fixtures/trenchbroom/src/point_volume_zup.map \
  tests/fixtures/trenchbroom/src/illusionary_static_zup.map; do
  python3 tools/bsp/validate_trenchbroom_map_source.py "$map"
done
```

Optional VHLT validation when tools are present:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full --keep-going
```

Optional display/GPU smoke commands:

```bash
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/moving_door_button_zup.bsp
```

Final audit commands:

```bash
git grep -n -i 'deferred\|metadata only\|unsupported' -- docs tools/trenchbroom tests/fixtures Plans/NEXT.md ':!Plans/Archived/**'
git grep -n 'Icon.png' -- tools/trenchbroom/Stellar
git ls-files 'tools/trenchbroom/Stellar/**'
git grep -n '_stellar_script\|stellar_script' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'func_door\|func_button\|light_spot\|light_environment' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'dev_grid_\|dev/grid_\|stellar_dev_' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'lightmap' -- include src tests docs ':!Plans/Archived/**'
```

Audit interpretation: remaining stale-wording hits are scoped to true non-goals, diagnostics,
historical phase notes, or documented negative examples. Package, FGD alias, material/WAD, lightmap,
door/button, fixture, and manual QA hits are expected coverage references rather than missing work.

## Completed phase status

- Phase 0 — branch, handoff, and docs baseline: completed.
- Phase 0.5 — prototype cube renderer removal and static-less no-map fallback: completed.
- Phase 1 — central Z-up world-axis contract: completed.
- Phase 2 — Z-up runtime, collision, movement, scripting metadata, and fixtures: completed.
- Phase 3 — Z-up presentation, camera, input mapping, snapshots, and network-adjacent tests:
  completed.
- Phase 4 — BSP30 import/validation lockdown: completed.
- Phase 5 — TrenchBroom package, FGD, material/editor assets, and compile wrappers: completed.
- Phase 6 — exported map fixtures and end-to-end validation: completed.
- Phase 7 — final documentation, audits, archival, and handoff: completed.

## Completed post-socket context

Socket transport and networked session lifecycle are complete context, not active TrenchBroom
compatibility work. Completed socket transport handoffs are archived under:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

## Completed socket-transport scope

The branch now has:

`stellar-server + stellar-client --connect HOST:PORT + Linux/POSIX TCP SocketTransport + ClientHello + ServerWelcome + ClientWorldReceiver + NetworkWorldSnapshot presentation`

Implemented limits remain explicit: one accepted TCP client, one active authoritative player slot, remote dynamic/fallback rendering only, and no client prediction, interpolation, reconciliation, map transfer, UDP, authentication, encryption, matchmaking, or public Internet deployment.

## Completed phase status

- ST-2 — Live client over local networked transport path: completed.
- ST-3 — Connection and session lifecycle: completed.
- ST-4 — Remote socket transport: completed.
- ST-5 — Dedicated server entry point: completed.
- ST-6 — Client connect mode: completed.
- ST-7 — Hardening, documentation, validation, and archival: completed.

## Next follow-up options

Pick one focused follow-up before implementation starts:

- Richer map editor workflow polish, including editor-visible WAD/material preview generation.
- Room/portal semantics beyond sealed brush rooms.
- Map distribution/caching or remote presentation-map workflow.
- Client interpolation/presentation smoothing over authoritative snapshots.
- Client prediction/reconciliation, explicitly scoped against server authority.
- True multi-player simulation beyond the current single-client/single-active-player limit.
- UDP/unreliable transport or transport-selection work.
- Richer HUD/UI/VFX presentation.
- miniaudio-backed playback integration for server-approved events.
- Sprite atlas/sheet animation.
- Moving brush classes beyond the implemented `func_door`/`func_button` path only if explicitly
  selected later.

## Completed historical scope

Completed plan packages remain archived and must not be restarted:

- BSP gameplay loop: `Plans/Archived/bsp_gameplay_loop/`.
- BSP presentation/networking polish: `Plans/Archived/bsp_presentation_networking_polish/`.
- Socket transport: `Plans/Archived/socket_transport/`.
- TrenchBroom BSP30 compatibility and Z-up migration: `Plans/Archived/trenchbroom_compat/`.

Do not restart completed collision, movement, Lua scripting, object-collider, BSP migration, BSP hardening, BSP gameplay-loop foundation work, BSP presentation/networking polish work, or socket transport/session lifecycle work.

## Invariants

- BSP remains the canonical playable level format.
- The active world-axis convention is Z-up.
- TrenchBroom authoring targets BSP30, with 1 editor unit = 1 gameplay inch.
- Default player spawn centers are authored 36 inches above the floor for the default capsule.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI are presentation only and never sources of gameplay truth.
- No client prediction, interpolation, map transfer, or reconciliation is active in the completed ST scope.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush classes beyond the implemented
  `func_door`/`func_button` path, full PBR, client-side gameplay scripting, model/animation systems,
  third-party physics, arbitrary non-Stellar entity parity, or retired importer functionality unless
  explicitly requested.
