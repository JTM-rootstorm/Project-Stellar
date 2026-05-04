# Project Stellar: Implementation Status

Status scope: completed Vulkan removal, completed lightweight BSP normal/specular material sidecars,
completed client/server decoupling handoff, and completed historical branch notes.

## Completed Scope — Vulkan Removal

Status: complete through KV-5 as of 2026-05-04.

Completed handoff plan:

- `Plans/Archived/kill_vulkan/00-MASTER-KillVulkan-Codex-AgentPlan.md`

### Current Renderer Summary

OpenGL is the current supported renderer. The active build no longer requires a Vulkan SDK, loader,
headers, libraries, CMake package, backend implementation, runtime CLI alias, or context smoke test.
The backend-neutral `GraphicsDevice` abstraction remains in place so future DirectX/Direct3D or Metal
support can be planned as explicit backend additions when real implementations exist.

## Completed Scope — Lightweight BSP Normal/Specular Material Sidecars

Status: complete through SNT-8 as of 2026-05-04.

Completed handoff plan:

- `Plans/spec_normal_textures_plan/00-MASTER-SpecNormalTextures-AgentPlan.md`

### Final Architecture Summary

BSP maps remain the canonical source for geometry, face texture names, lightmaps, visibility,
entities, and collision. Optional `.stellar_material` sidecars now enrich BSP surface presentation by
normalized BSP texture name without changing the BSP binary format or authoring gameplay semantics.
Missing sidecars preserve previous BSP texture/lightmap fallback behavior.

The importer resolves sidecars from configured material roots, rejects unsafe texture paths, loads
sidecar images into source-neutral `LevelAsset` image/texture/material data, and generates
texinfo-derived tangents where valid. `RenderLevel` uploads the resolved material slots through the
backend-neutral render scene path. OpenGL consumes the shared material contract for base color,
normal, specular, lightmap, and fallback texture bindings in display-free validated paths. The former
Vulkan parity path from SNT-7 was intentionally removed from active support on the kill-vulkan branch.

Material sidecars are presentation-only. Server-authoritative gameplay, collision, scripting,
transport, and networking do not depend on sidecar state.

### Material Authoring Summary

- Sidecars use `.stellar_material` files keyed by normalized BSP texture names, such as
  `materials/dev/wall_96.stellar_material` for `dev/wall_96`.
- Texture references are relative to the sidecar file directory; absolute paths, drive-letter paths,
  empty segments, and `..` escapes are rejected.
- Normal maps require valid BSP texinfo tangents. Degenerate texture axes fall back to regular
  non-normal-mapped shading.
- Specular maps, factors, and power are lightweight presentation shading, not full PBR. Full PBR,
  image-based lighting, environment maps, reflection probes, and physically based BRDF parity remain
  deferred.

### Phase Checklist

- [x] SNT-0 — Baseline validation.
- [x] SNT-1 — Material contracts.
- [x] SNT-2 — Sidecar parser.
- [x] SNT-3 — BSP material resolution and image loading.
- [x] SNT-4 — Tangent data.
- [x] SNT-5 — RenderLevel material upload.
- [x] SNT-6 — OpenGL normal/specular lighting.
- [x] SNT-7 — Former Vulkan material parity, retired from active support on kill-vulkan.
- [x] SNT-8 — Fixtures, docs, final validation, and handoff.

### Validation Commands

Final SNT validation run on 2026-05-04:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh
ctest --test-dir build -R '^(bsp_materials|bsp_lightmaps|bsp_importer|render_level_upload|render_level_inspection|graphics_backend_selection|target_boundary)$' --output-on-failure
ctest --test-dir build -R '^(trenchbroom|bsp_|client_map_validation|client_cli|server_cli|render_level|world_axes|collision_world|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
```

Default validation is display-free and remains the pass/fail gate. OpenGL context smoke tests are
opt-in for systems with a GPU/display/driver context.

## Completed Scope — Client/Server Decoupling

Status: complete through Phase CS-9 as of 2026-05-03.

Completed handoff plan:

- `Plans/ClientServerSplit-AgentPlan.md`

Proposal source preserved at:

- `Plans/ProjectStellar-ClientServerDecoupling-AgentPlan.md`

### Final Architecture Summary

Client/server decoupling is complete. The implementation now separates pure protocol DTOs/codecs,
transport implementations, authority bootstrap/session ownership, reusable server runtime hosting,
single-player authority stepping, remote client networking, listen-server hosting, presentation-only
client helpers, and top-level client application composition. The completed split preserves the
existing socket/session lifecycle and TrenchBroom BSP30 gameplay foundations rather than restarting
them.

Remote client runtime is presentation/network-only and does not own authoritative BSP/script/session
state. Single-player runs authoritative simulation in-process without starting a server host, opening a
listener, or performing `ClientHello`/`ServerWelcome`. Listen-server and dedicated-server modes share
the authority bootstrap and server runtime path.

### Runtime Mode Summary

| Mode | Command | Authority Owner | Socket Listener | Script Loading | Lifetime |
|---|---|---|---:|---:|---|
| Single-player | `stellar-client --map path/to/map.bsp` | In-process single-player authority facade | no | authority-side only | client |
| Remote client | `stellar-client --connect HOST:PORT` | remote server/listen host | no | no | client |
| Listen server | `stellar-client --host --map path/to/map.bsp [--listen HOST:PORT]` | in-process server host | yes when listening | authority-side only | host client |
| Dedicated server | `stellar-server --map path/to/map.bsp --listen HOST:PORT` | server process | yes | authority-side only | server process |

### Module Boundary Summary

- `stellar_protocol`: pure protocol DTOs/codecs such as protocol/session ids, hello/welcome,
  commands, snapshots, gameplay entities, events, and delta/session codecs. It must not depend on
  server, scripting, client runtime, graphics, audio, or presentation platform targets.
- `stellar_transport`: transport interfaces/implementations such as client/server transports,
  loopback transport, TCP/POSIX socket transport, endpoint parsing, and packet envelopes. It should
  depend only on protocol plus low-level platform/system support.
- `stellar_authority`: backend-neutral authoritative gameplay runtime and bootstrap for BSP map
  validation/loading, sandboxed script-root resolution, script registry loading, authoritative session
  creation, and authority-side snapshot/event conversion. It must not depend on client runtime,
  graphics, audio, or window/input presentation.
- `stellar_server_runtime`: reusable server host/session lifecycle for dedicated and listen-server
  modes, including connection state, command ingestion, snapshot scheduling, and client assignment.
  It must not depend on client presentation, graphics, audio, or window/input.
- `stellar_dedicated_server`: thin dedicated-server wrapper/CLI helpers around server runtime and
  authority bootstrap. It must not link client runtime or presentation.
- `stellar_single_player`: client-process local authoritative session for single-player. It must not
  open sockets, create server transports, send `ClientHello`, wait for `ServerWelcome`, or start a
  dedicated/listen server host.
- `stellar_client_net`: remote/listen client runtime for socket connection, client session state,
  input command sequencing, and `ClientWorldReceiver`. It must not link authority, server runtime/core,
  or scripting.
- `stellar_client_presentation`: presentation-only helpers for player/gameplay/HUD/audio routing and
  render-view setup. It must not own authoritative runtime state.
- `stellar_client_app`: top-level mode selection, platform window/input, and presentation frame loop.
  It may compose single-player, remote-client, listen-server, and no-map fallback runtimes, but it
  should not contain map/script/server bootstrap logic directly.
- `stellar_listen_server`: client-hosted server mode whose server lifetime is tied to the host client
  and which reuses server runtime/transport rather than client-owned authority internals.

Compatibility shims may remain where required by public include or namespace transition, but new code
should target the split modules directly.

### Dependency Audit Guardrails and Results

- Protocol must not link server, scripting, client runtime, graphics, audio, or platform presentation
  targets.
- Transport must not link server runtime, scripting, or client presentation.
- Remote client runtime must not link authority, server runtime/core, or scripting.
- Dedicated server must not link client runtime or presentation.
- Authority-side Lua remains mandatory, sandboxed, and server-authoritative; remote client runtime must
  not load gameplay scripts.
- Keep broad TODOs tracked in completed plan/status docs. Do not scatter broad TODO comments through
  source.

Final audit commands:

```bash
git grep -n 'LocalServerBridge\|LocalLoopbackRuntime' -- include src tests ':!Plans/Archived/**'
git grep -n 'stellar/server/WorldSession.hpp\|stellar/scripting/ScriptedWorldSession.hpp' -- include/stellar/network src/network include/stellar/client src/client ':!Plans/Archived/**'
tools/dev/check_target_boundaries.sh
```

Expected source-level references to server/scripting types are permitted only in authority,
single-player, listen-host, server-runtime, dedicated-server, or top-level application composition paths
that intentionally link authority-side modules. `stellar_client_net` remains isolated by CMake link
assertions and the boundary audit.

### Phase Checklist

- [x] CS-0 — Architecture baseline and guardrails.
- [x] CS-1 — Extract protocol DTOs away from server/scripting implementation types.
- [x] CS-2 — Extract shared authority bootstrap.
- [x] CS-3 — Move `LocalServerBridge` out of the client and generalize it as server runtime.
- [x] CS-4 — Introduce a single client-facing runtime interface.
- [x] CS-5 — Add true single-player runtime without server startup.
- [x] CS-6 — Add listen-server host mode.
- [x] CS-7 — Remove legacy local loopback runtime and client-owned authority leftovers.
- [x] CS-8 — Build graph enforcement.
- [x] CS-9 — Documentation update and final handoff.

CS-7 deletion was gated on passing CS-5 and CS-6 replacement tests before removal.

### Final Validation Results

CS-0 baseline validation on 2026-05-03:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Result: passed. CMake configure succeeded, full debug build succeeded, and full default CTest passed
91/91.

CS-8 build-boundary validation from commit `8dce477c757e293fdb6f39cbca81b809b202b7e8`
(`Enforce client-server target boundaries`):

- CMake configure passed.
- Build targets passed: `stellar_protocol_test`, `stellar_transport_test`,
  `stellar_client_connect_test`, and `stellar_dedicated_server_test`.
- CTest regex passed 10/10 after protocol/transport aliases.
- `tools/dev/check_target_boundaries.sh` passed.

CS-9 final handoff validation on 2026-05-03:

- CMake configure passed: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.
- Full debug build passed: `cmake --build build -j$(nproc)`.
- Full default CTest passed: `ctest --test-dir build --output-on-failure` reported 97/97.
- Focused client/server final filter passed: 44/44.
- `tools/dev/check_target_boundaries.sh` passed.
- Final source audits for `LocalServerBridge`/`LocalLoopbackRuntime` and forbidden
  network/client server-scripting includes returned no active source matches.

### Historical Scope Guardrail

Completed socket transport/session lifecycle and completed TrenchBroom BSP30 compatibility work are
historical context and must not be restarted. This includes TCP `SocketTransport`, `ClientHello`,
`ServerWelcome`, `ClientWorldReceiver`, `NetworkWorldSnapshot` presentation, `stellar-server`,
`stellar-client --connect`, the Stellar TrenchBroom package, BSP30 compile/validation wrappers, Z-up
authoring/runtime migration, fixture coverage, lightmap support, and the implemented
server-authoritative `func_door`/`func_button` path.

## Completed Scope — Full Stellar TrenchBroom BSP30 Compatibility

Status: complete as of 2026-05-02.

Final supported Stellar TrenchBroom compatibility scope:

- The project-owned `tools/trenchbroom/Stellar/` game package is the supported editor package for
  repo-local use and copied-package installs that point back to the checkout with `STELLAR_REPO_ROOT`
  or package-local `.stellar_repo_root`.
- The supported editor compile target is classic BSP30 through the Stellar compile/validate shims,
  generic BSP30 compilers, or the optional VHLT Linux toolchain. Source/VBSP and arbitrary third-party
  game profiles are outside this Stellar BSP30 profile.
- Authoring uses Z-up coordinates and 1 editor unit = 1 Stellar gameplay inch. BSP30 coordinates are
  imported 1:1; the default 72 inch player capsule spawn center is authored at `z = 36` above a floor
  at `z = 0`.
- Materials use packaged developer PNGs plus `materials/stellar_dev.wad`, safe WAD3 lookup, and
  deterministic developer fallback aliases across `stellar_dev_*`, `dev/...`, and `dev_*` names.
- Compile-time `light`, `light_spot`, and `light_environment` entities are supported for BSP lightmap
  generation. Imported BSP lightmaps are uploaded and sampled by the renderer, multiplying static
  surface base material color/texture by baked lighting where valid.
- Runtime-supported FGD classes are `worldspawn`, `info_player_start`, `info_stellar_spawn`,
  `trigger_stellar`, `trigger_multiple`, `trigger_once`, point trigger variants, `stellar_sprite`,
  `env_sprite`, `stellar_object_collider`, `stellar_object_collider_point`, `light`, `light_spot`,
  `light_environment`, `func_wall`, `func_illusionary`, `func_detail`, `func_door`, and
  `func_button`.
- `func_door` and `func_button` are implemented server-authoritative brush movers with target routing,
  query-time collision overlay transforms, and replicated presentation transforms. Plats, trains,
  rotators, arbitrary Quake/Half-Life entity parity, and third-party physics are not part of this
  Stellar profile.
- Lua gameplay scripting remains mandatory, sandboxed, and server-authoritative. Import records script
  metadata but never executes scripts; runtime loads only asset-relative scripts.
- Default validation remains display-free and compiler-independent. External VHLT/display/GPU checks
  are optional manual or locally run validation, not CI requirements.
- Manual TrenchBroom GUI QA is user-performed and recorded with `docs/TrenchBroomManualQA.md`; GUI
  automation is intentionally not required in CI.

Final TB-FULL-07 validation runbook:

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

Optional VHLT validation when tools are installed:

```bash
tools/bsp/run_vhlt_fixture_matrix.sh --source-root . --build-root build --profile full --keep-going
```

Optional display/GPU smoke commands:

```bash
build/stellar-client --validate-display
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/lit_zup_room.bsp
build/stellar-client --map build/tests/fixtures/trenchbroom/vhlt/compiled/moving_door_button_zup.bsp
```

Final TB-FULL-07 audit commands:

```bash
git grep -n -i 'deferred\|metadata only\|unsupported' -- docs tools/trenchbroom tests/fixtures Plans/NEXT.md ':!Plans/Archived/**'
git grep -n 'Icon.png' -- tools/trenchbroom/Stellar
git ls-files 'tools/trenchbroom/Stellar/**'
git grep -n '_stellar_script\|stellar_script' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'func_door\|func_button\|light_spot\|light_environment' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'dev_grid_\|dev/grid_\|stellar_dev_' -- tools docs src tests ':!Plans/Archived/**'
git grep -n 'lightmap' -- include src tests docs ':!Plans/Archived/**'
```

Audit interpretation recorded for TB-FULL-07:

- Stale active wording for WAD generation, copied-package installs, metadata-only door/button support,
  and imported-but-unrendered lightmaps has been removed or narrowed to historical phase context.
- Remaining `unsupported` hits describe diagnostics or true non-goals such as Source/VBSP, unsafe paths,
  sprite script callbacks, and arbitrary non-Stellar entity parity.
- Remaining `metadata only` hits describe import-time script metadata before server-authoritative runtime
  loading, not missing runtime support for advertised FGD gameplay classes.
- Remaining `deferred` hits are scoped post-branch networking/presentation/audio/history notes or true
  non-goals outside the Stellar BSP30 profile.
- `Icon.png`, package shims, templates, FGD, materials, and WAD are expected package files.
- `_stellar_script` aliases are importer-supported FGD aliases; plain `stellar_script` appears only as a
  documented negative example.
- Door/button, light entity, developer material, and lightmap hits are expected implementation, fixture,
  documentation, and test coverage references.

## Completed Scope — TB-FULL-06 Fixtures, CI Gates, and Manual QA

Status: complete as of 2026-05-02.

- Completed positive TrenchBroom source fixture matrix for minimal, entity matrix, scripted
  interaction, lit, material/WAD, moving door/button, point volume, and illusionary/static behavior.
- Added deterministic negative source fixtures for script path escape, incomplete/malformed brushes,
  missing target references, and strict missing WAD texture handling.
- Expanded generated BSP fixture coverage for synthetic lightmap metadata, material/WAD entity data,
  door/button brush model metadata, point-volume classes, and illusionary/static brush metadata.
- Expanded VHLT fixture matrix with all positive fixtures, negative preflight expectations,
  `--fixture`/`--list`, script copying, and skip code `77` when VHLT tools are unavailable.
- Added copied-package compile smoke behavior that runs package-local shims and skips the compile
  sub-step when no external BSP30 compiler is available.
- Added full manual TrenchBroom QA checklist/reporting template in `docs/TrenchBroomManualQA.md` and
  regex-friendly CTest groups including `trenchbroom_*`, `bsp_lightmaps_*`, and `brush_mover_*`.

## Completed Scope — TB-FULL-05 Runtime Brush Entities and Targets

Status: complete as of 2026-05-02.

- Preserved imported brush model ownership in source-neutral level geometry metadata.
- Added query-time server-owned collision mesh translations without mutating immutable level assets.
- Added minimal server-authoritative `func_door`/`func_button` mover state and target firing diagnostics.
- Replicated moving brush transforms through authoritative snapshots/gameplay entity presentation state.
- Added source fixtures for moving door/button, point volumes, and static illusionary behavior.

## Completed Scope — TB-FULL-04 Lighting and Renderer Lightmaps

Status: complete as of 2026-05-02.

- Added a TrenchBroom lit room source fixture with `light` and `light_spot` entities.
- Expanded BSP lightmap/import and display-free RenderLevel material upload coverage.
- Wired static BSP lightmaps through linear/clamp texture upload, secondary-UV material bindings, and a
  stable lightstyle multiplier baseline of `1.0`.
- Updated OpenGL material sampling to multiply static surface base color/texture by imported
  lightmaps through the shared material contract.
- Moving brush runtime behavior was intentionally not introduced in TB-FULL-04; it was completed later
  by TB-FULL-05 for the supported `func_door`/`func_button` path.

## Completed Scope — TrenchBroom BSP30 Compatibility and Z-up Migration

Status: complete as of 2026-05-01.

Archived handoff docs:

- `Plans/Archived/trenchbroom_compat/TrenchBroomCompat-AgentPlan.md`
- `Plans/Archived/trenchbroom_compat/trenchbroom_compat_plans/00-MASTER-TrenchBroomCompat-AgentPlan.md`

Completed branch outcomes:

- Active gameplay/runtime/world authoring uses Z-up coordinates.
- 1 Stellar unit remains 1 authored inch.
- The primary editor workflow targets TrenchBroom-authored BSP30 maps imported 1:1.
- The default player capsule is 72 inches tall, so player spawn centers are authored at `z = 36`
  above a floor at `z = 0`.
- The project now owns a TrenchBroom game package, FGD aliases, developer material references,
  BSP30 compile/validation wrappers, source-map fixtures, and generated BSP30 test fixtures.
- Server authority, sandboxed Lua, BSP canonical runtime, import-never-executes-scripts behavior,
  and display-free validation remain mandatory.
- No-map and remote-without-presentation-map paths use a blank/static-less presentation fallback.
- Source/VBSP, moving brush classes beyond the implemented door/button path, dynamic rigid bodies,
  full PBR, client-side gameplay scripting, map transfer/caching, prediction, and reconciliation remain
  outside the completed Stellar BSP30 profile unless explicitly selected later.

Phase order:

1. Phase 0 — branch, handoff, and docs baseline.
2. Phase 0.5 — remove prototype cube renderer and switch no-map rendering to a static-less fallback.
3. Phase 1 — central Z-up world-axis contract.
4. Phase 2 — Z-up runtime, collision, movement, scripting metadata, and fixtures.
5. Phase 3 — Z-up presentation, camera, input mapping, snapshots, and network-adjacent tests.
6. Phase 4 — BSP30 import/validation lockdown.
7. Phase 5 — TrenchBroom package, FGD, material/editor assets, and compile wrappers.
8. Phase 6 — exported map fixtures and end-to-end validation.
9. Phase 7 — final documentation, audits, archival, and handoff.

Phase completion notes:

- Phase 0 — established the branch handoff and baseline audit. The audit identified axis, BSP wording,
  FGD alias, and prototype fallback cleanup targets without changing runtime behavior.
- Phase 0.5 — removed the prototype cube renderer/debug cube mesh path. No-map and
  remote-without-presentation-map modes now initialize a blank/static-less presentation frame until
  authored static geometry or authoritative dynamic snapshot data is available.
- Phase 1 — added the central Z-up world-axis contract in `stellar::core`, converted core
  character-controller and presentation default-up values to named constants, and added display-free
  `world_axes` coverage.
- Phase 2 — converted authoritative runtime movement, physics collision queries,
  trigger/object-collider capsule defaults, collision validation, script-facing event tests, and
  generated BSP gameplay fixtures to Z-up semantics. Gravity applies along `-Z`, horizontal movement
  uses the X/Y plane, and importer/entity parsing preserves authored `x y z` ordering without hidden
  axis swaps.
- Phase 3 — converted client-side presentation and network-adjacent expectations to Z-up semantics.
  Player camera eye offset is along +Z, default forward is +Y on the X/Y plane, yaw rotates around +Z,
  input commands map W/S to +/-Y and A/D to +/-X, and snapshots remain raw server-authored data
  carriers with no prediction, interpolation, reconciliation, or client authority.
- Phase 4 — locked importer validation around BSP30 as the TrenchBroom target while preserving the
  existing legacy BSP compatibility path. BSP30 tests cover Z-up player origins, dotted and underscore
  alias entity keys, deterministic developer material fallback, malformed lump rejection, and explicit
  Source/VBSP unsupported-version diagnostics without hidden axis conversion.
- Phase 5 — added the project-owned TrenchBroom package, underscore-alias FGD, developer material name
  reference, BSP30 compile/validation wrappers, and workflow documentation. Runtime dotted metadata
  remains supported and wrappers fail clearly when external compiler/runtime tools are unavailable.
- Phase 6 — added TrenchBroom-compatible `.map` source fixtures, Lua fixture scripts, deterministic
  generated BSP30 outputs under the build tree, fixture README/manual checklist coverage, and
  display-free client/server/importer/script validation for minimal room, entity matrix, scripted
  interaction, and invalid script escape cases.
- Phase 7 — finalized active docs and handoff, archived the completed plan package, reran final audits,
  reran full/focused CTest, and validated generated BSP30 fixtures with the wrapper.

Phase 0 validation:

```bash
git grep audits for retired axis wording, forward/up vector literals, FGD placeholder names,
legacy BSP wording, and prototype cube fallback symbols across active docs/source/tests/tools/plans.
```

Result: audit commands were run during Phase 0; findings are summarized above. No runtime behavior was
changed in Phase 0.

Phase 1 validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_core_world_axes_test stellar_character_controller_test stellar_collision_world_test stellar_render_level_inspection_test stellar_gameplay_presentation_test stellar_player_presentation_test -j$(nproc)
ctest --test-dir build -R '^(world_axes|character_controller|collision_world|collision_validation|render_level_inspection|gameplay_presentation|player_presentation)' --output-on-failure
```

Result: focused Phase 1 builds and tests passed during implementation. Full CTest remains part of
phase-close validation before commit.

Phase 2 validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(character_controller|collision_world|collision_performance_regression|collision_validation|runtime_world|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_playable_world_smoke|movement_simulation|movement_trigger_integration|trigger_system|object_collider|runtime_collision_state)' --output-on-failure
```

Result: focused Phase 2 physics/runtime/gameplay tests passed during implementation. Full CTest remains
part of phase-close validation before commit.

Phase 3 validation:

```bash
cmake --build build --target stellar_player_presentation_test stellar_gameplay_presentation_test stellar_render_level_inspection_test stellar_client_movement_input_mapper_test stellar_client_world_receiver_test stellar_networked_client_runtime_test stellar_client_connect_test stellar_snapshot_delta_test stellar_snapshot_codec_test -j$(nproc)
ctest --test-dir build -R '^(player_presentation|gameplay_presentation|render_level_inspection|client_movement_input_mapper|client_world_receiver|networked_client_runtime|client_connect|snapshot_)' --output-on-failure
```

Result: focused Phase 3 presentation, input, and network-adjacent tests passed during implementation.
Full CTest remains part of phase-close validation before commit.

Phase 4 validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_validation|bsp_importer|bsp_authoring_smoke|client_map_validation_smoke|client_cli_map_validation|dedicated_server)' --output-on-failure
```

Result: focused BSP30 importer/validation tests passed during implementation. Full CTest remains part
of phase-close validation before commit.

Phase 5 validation:

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
git grep audits for TrenchBroom FGD aliases and runtime dotted metadata keys across active
tools/docs/tests.
```

Result: wrapper syntax checks and FGD/key audits passed during implementation. External BSP compiler
smoke testing remains optional because no compiler is vendored.

Phase 6 validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_authoring_smoke|bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|dedicated_server|runtime_world|server_world_session|scripted_world_session)' --output-on-failure
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
```

Result: focused Phase 6 fixture and end-to-end validation passed during implementation. Source-tree
fixture maps are human/editor references; deterministic compiled BSP30 fixtures are generated under
`build/tests/fixtures/trenchbroom/compiled/` for display-free validation.

Phase 7 validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(world_axes|character_controller|collision_validation|runtime_world|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_|client_map_validation_smoke|client_cli_map_validation|dedicated_server|player_presentation|gameplay_presentation|render_level|graphics_backend_selection|client_world_receiver|networked_client_runtime|snapshot_|hud_presentation|audio_event_router)' --output-on-failure
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/minimal_zup_room.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/entity_matrix_zup.bsp
tools/bsp/validate_trenchbroom_bsp30.sh build/tests/fixtures/trenchbroom/compiled/scripted_interaction_zup.bsp
```

Result: final Phase 7 validation passed on 2026-05-01. Full debug build succeeded with no work to do,
full default CTest passed 59/59, focused TrenchBroom/BSP30/Z-up CTest passed 40/40, and wrapper
validation passed for all three generated BSP30 fixtures. The two script-bound generated fixtures
required the existing fixture Lua scripts to be present beside the generated BSPs under
`build/tests/fixtures/trenchbroom/compiled/scripts/`, matching runtime script-root behavior.

Final audit interpretation:

- Active docs present Z-up as current and do not describe the retired axis convention as active.
- Remaining `{0, 1, 0}` source/test hits are forward +Y vectors, triangle vertices, colors, or generic
  math fallbacks; they are not up-axis contracts.
- Active TrenchBroom workflow uses dotted runtime keys or underscore aliases; plain placeholder field
  names are mentioned only as unsupported negative examples.
- BSP30 is the TrenchBroom target; legacy BSP support remains importer compatibility only, and
  Source/VBSP remains outside the Stellar BSP30 profile.
- Prototype cube renderer/debug cube fallback symbols are absent from active source/docs/tests/plans
  outside archived historical plans.

## Completed Follow-up Scope — VHLT BSP Toolchain Testing

Status: complete as of 2026-05-02.

- Added VHLT-aware BSP30 compile routing over `hlcsg`, `hlbsp`, `hlvis`, and `hlrad` while preserving
  the generic single-compiler wrapper path.
- Added deterministic developer WAD generation and transient build/work-map WAD injection for
  TrenchBroom source-map compilation.
- Added display-free VHLT fixture matrix coverage for the positive TrenchBroom fixtures and the invalid
  script path negative fixture.
- Updated TrenchBroom workflow, BSP authoring, and fixture documentation for VHLT tool discovery,
  generated artifact locations, and client/server validation requirements.
- Default tests skip clearly when VHLT tools are unavailable and remain display-free.

Validation summary:

```bash
bash -n tools/bsp/compile_trenchbroom_bsp30.sh
bash -n tools/bsp/compile_vhlt_bsp30.sh
bash -n tools/bsp/validate_trenchbroom_bsp30.sh
bash -n tools/bsp/run_vhlt_fixture_matrix.sh
```

Result: wrapper syntax checks passed. Documentation checks confirmed VHLT wrapper commands are present
and the updated documentation does not introduce absolute local paths.

## Completed Scope — Socket Transport and Networked Session Lifecycle

Branch target: `socket-transport`

Status: complete as of 2026-05-01.

Archived handoff docs:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`
- Source plan package archive: `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

The branch completed TCP-first socket transport and networked session lifecycle on top of the
completed BSP gameplay-loop and BSP presentation/networking polish scopes. Those earlier scopes
remain archived and must not be restarted.

Current phase status:

- Phase ST-2 — Live client over local networked transport path: complete as of 2026-05-01.
- Phase ST-3 — Connection and session lifecycle: complete as of 2026-05-01.
- Phase ST-4 — Remote socket transport: complete as of 2026-05-01.
- Phase ST-5 — Dedicated server entry point: complete as of 2026-05-01.
- Phase ST-6 — Client connect mode: complete as of 2026-05-01.
- Phase ST-7 — Hardening, documentation, validation, and archival: complete as of 2026-05-01.

Phase ST-2 completion notes:

- Added `NetworkedClientRuntime` for mapped local play over
  `LoopbackTransportPair + LocalServerBridge + ClientWorldReceiver` with monotonic
  `NetworkPlayerCommand` sequencing and authoritative `NetworkWorldSnapshot` presentation.
- Added presentation-safe `NetworkWorldSnapshot` overloads for player/camera and gameplay billboard
  helpers without GPU handles or client authority in snapshots.
- `prepare_application_runtime()` now creates `networked_runtime` for mapped play by default while
  preserving `LocalLoopbackRuntime` in the codebase for low-level tests and fallback.
- Live mapped client rendering now reads from latest authoritative network snapshots. No-map debug
  fallback remains unchanged.
- Scripted maps still load Lua through the existing sandboxed server-authoritative registry path and
  fail deterministically on script load errors.

ST-2 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar_networked_client_runtime_test stellar_client_map_validation_smoke stellar_gameplay_presentation_test stellar_player_presentation_test -j$(nproc)
ctest --test-dir build -R '^(networked_client_runtime|client_map_validation_smoke|client_cli_map_validation|gameplay_presentation|player_presentation|client_world_receiver|loopback_transport|snapshot_)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused ST-2 targets built, focused ST-2 CTest passed 12/12, full
debug build succeeded, and full default CTest passed 49/49 on 2026-05-01.

Phase ST-3 completion notes:

- Added deterministic session protocol contracts in `stellar::network` for `ProtocolVersion`,
  `SessionId`, `SessionState`, `MapIdentity`, `ClientHello`, and `ServerWelcome`, including a small
  non-cryptographic deterministic map identity hash helper.
- Extended the binary codec with bounded little-endian hello/welcome encode/decode paths and clean
  `std::expected` failures for malformed, truncated, oversized, and unknown packet data.
- `LocalServerBridge` now requires an accepted hello before input/snapshots, rejects protocol and map
  mismatches with deterministic `ServerWelcome` diagnostics, and overwrites requested command player
  ids with the server-assigned authoritative player slot.
- `NetworkedClientRuntime` sends `ClientHello` on creation, pumps the local bridge until accepted,
  stores the assigned player id, uses it for commands and presentation lookup, and exposes session
  state/diagnostics in frame results.
- Display-free coverage was added for session round trips, accepted welcome/player assignment,
  protocol mismatch, map mismatch, input before welcome, malformed packets, and assigned-player
  presentation behavior. Remote sockets remain deferred; there is still no prediction or
  reconciliation.

ST-3 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_network_session_test stellar_snapshot_codec_test stellar_networked_client_runtime_test stellar_client_world_receiver_test -j$(nproc)
ctest --test-dir build -R '^(network_session|snapshot_codec|networked_client_runtime|client_world_receiver|loopback_transport)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: focused ST-3 targets built, focused ST-3 CTest passed 5/5, full debug build succeeded, and
full default CTest passed 50/50 on 2026-05-01.

Phase ST-4 completion notes:

- Added Linux/POSIX TCP socket transport behind the existing transport-neutral
  `ClientTransport`/`ServerTransport` seam without changing gameplay/message contracts.
- Added deterministic `host:port` endpoint parsing/formatting and ephemeral bind reporting for
  localhost tests and future dedicated server startup.
- Added a compact TCP packet envelope with `STCP` magic, version byte, channel byte, little-endian
  `uint32` payload length, configurable max payload bound, complete-packet FIFO draining, and bounded
  partial read/write handling.
- Implemented a socket client transport plus a single-client socket server transport. Multi-client
  simulation, UDP, prediction, reconciliation, authentication, encryption, and public Internet
  discovery remain deferred.
- Display-free localhost loopback coverage now exercises endpoint parsing, client/server connection,
  FIFO ordering, packet boundaries, oversized payload rejection, disconnect send diagnostics,
  hello/welcome round trip, and snapshot delivery into `ClientWorldReceiver`.

ST-4 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_socket_transport_test stellar_network_session_test stellar_snapshot_codec_test -j$(nproc)
ctest --test-dir build -R '^(socket_transport|network_session|snapshot_codec|loopback_transport)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused ST-4 targets built, focused ST-4 CTest passed 4/4, full debug
build succeeded, and full default CTest passed 51/51 on 2026-05-01.

Phase ST-5 completion notes:

- Added `stellar::server::DedicatedServer` with `DedicatedServerConfig`, bounded `pump_once()` for
  tests, validate-only startup, and a `stellar-server` CLI target.
- The dedicated server owns BSP map validation/loading, backend-neutral runtime-world construction,
  sandboxed authoritative Lua script loading when map metadata references scripts, single player id
  assignment, authoritative fixed ticks, snapshots/deltas, and server-approved gameplay events.
- The socket lifecycle accepts one TCP client, waits for `ClientHello`, validates protocol and map
  identity, sends `ServerWelcome`, rejects/ignores input before welcome, sends the first full snapshot,
  emits deltas after the baseline, and survives client disconnect without crashing. This is not a true
  simultaneous multiplayer simulation claim.
- Added display-free coverage for CLI/config parsing failures, validate-only generated BSP fixtures,
  missing/non-BSP/invalid map failures, missing script source failures, socket hello/welcome, first full
  snapshot, input-driven authoritative snapshot updates, and disconnect handling.

ST-5 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-server stellar_dedicated_server_test stellar_socket_transport_test stellar_network_session_test -j$(nproc)
ctest --test-dir build -R '^(dedicated_server|socket_transport|network_session|snapshot_|scripted_world_session|bsp_)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused ST-5 targets built, focused ST-5 CTest passed 21/21, full debug
build succeeded, and full default CTest passed 54/54 on 2026-05-01.

Phase ST-6 completion notes:

- Added `stellar-client --connect HOST:PORT` and `--client-name` configuration parsing for remote
  presentation mode. `--map` without `--connect` remains local networked runtime mode; `--connect`
  without `--map` is remote mode; `--map` plus `--connect` is rejected as ambiguous; `--script-root` is
  invalid in remote mode.
- Added `RemoteClientRuntime`, which owns a socket `ClientTransport`, sends `ClientHello`, accepts
  `ServerWelcome`, stores the server-assigned player id, sends input commands only after accepted
  welcome, drains `ClientWorldReceiver`, and exposes latest authoritative snapshots/events/session
  diagnostics without prediction, reconciliation, interpolation, local authority, or script loading.
- The live application now renders remote connect mode from `NetworkWorldSnapshot` camera/billboard
  presentation and clears safely until a snapshot arrives. Received `GameplayEvent` records are routed
  to the existing HUD presentation cache and no-op audio event route.
- Static level rendering policy is intentionally minimal for ST-6: remote mode without `--map` renders
  network dynamic state/fallback only, and no map transfer/download or presentation-map option was
  added. The dedicated server accepts an empty requested map id from clients without a local map
  expectation and still rejects explicit mismatches.
- Added display-free coverage for CLI connect parsing/conflicts, validate-only remote preparation with
  no local authority/scripts, remote hello/welcome/input gating, snapshot/player/event exposure, and a
  bounded in-process localhost `DedicatedServer` integration.

ST-6 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar-server stellar_client_connect_test stellar_dedicated_server_test stellar_socket_transport_test -j$(nproc)
ctest --test-dir build -R '^(client_connect|dedicated_server|socket_transport|network_session|networked_client_runtime|client_world_receiver|gameplay_presentation|player_presentation)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused ST-6 targets built, focused ST-6 CTest passed 8/8, full debug
build succeeded, and full default CTest passed 55/55 on 2026-05-01.

Phase ST-7 completion notes:

- Completed protocol and authority audit for remote client mode, dedicated server session lifecycle,
  local bridge compatibility, and active docs/tests. No active source path was found where a remote
  client creates or mutates authoritative gameplay state, loads gameplay scripts, trusts renderer/audio
  state as authority, sends input before accepted welcome, or assigns its own authoritative player id.
- Completed network robustness audit for the TCP envelope, codec/session handling, client receiver,
  dedicated server, and socket tests. Existing coverage verifies malformed packet rejection, oversized
  payload rejection before allocation, partial envelope handling, bounded nonblocking socket loops,
  disconnect handling, server-side player id overwrite, full snapshot baseline reset, delta baseline
  requirements, one-shot gameplay event draining, RAII socket closure, and bounded localhost tests.
- Finalized branch docs for the implemented Linux/POSIX TCP-first transport, `stellar-server`,
  `stellar-client --connect HOST:PORT`, single-client/single-active-player limitation, and remote
  dynamic/fallback rendering policy. No prediction, reconciliation, interpolation, map transfer,
  authentication, encryption, UDP, or true simultaneous multiplayer implementation is claimed.
- Archived active socket transport plans under `Plans/Archived/socket_transport/`; root `Plans/` now
  keeps `NEXT.md` as the current handoff and refers to archived ST material for history.

ST-7 final validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(networked_client_runtime|network_session|socket_transport|dedicated_server|client_connect|client_world_receiver|loopback_transport|snapshot_|bsp_|runtime_world|server_world_session|scripted_world_session|script_command_processor|gameplay_presentation|player_presentation|hud_presentation|audio_event_router)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'TODO.*prediction\|TODO.*reconciliation\|predict' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'LocalLoopbackRuntime' -- src/client include/stellar/client tests/client ':!build*/**'
git grep -n 'SocketTransport-AgentPlan\|ProjectStellar-SocketTransport-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Result: configure succeeded, full debug build succeeded, full default CTest passed 55/55, and the
focused ST/BSP/runtime/client/server CTest regex passed 30/30 on 2026-05-01. Retired importer audit
hits were absent from active source and limited to documentation/history references. Authority and
prediction audit hits were documentation/test prohibitions or explicit deferred-work statements, not
active client authority. `LocalLoopbackRuntime` hits were limited to local mapped fallback/tests and
not used by remote `--connect` mode. After archival, socket plan filename hits outside archives were
limited to this status file and `Plans/NEXT.md` archive references.

Known post-ST deferred work:

- Client interpolation, prediction/reconciliation, and any presentation smoothing that is explicitly
  scoped and reconciled against server authority.
- True simultaneous multiplayer simulation beyond the current single accepted TCP client and one
  active authoritative player slot.
- UDP/unreliable transport, transport selection, public Internet deployment, authentication,
  encryption, matchmaking, and reconnect/resume semantics.
- Map distribution/caching or a presentation-map workflow for remote clients; remote mode currently
  renders received dynamic network state/fallback only.
- Richer HUD/UI/VFX, miniaudio-backed playback integration, sprite atlas/sheet animation, and manual
  LAN/cross-platform socket validation.

Manual validation and platform limitations: validation was display-free and localhost-only on the
Linux/POSIX TCP backend. No GPU/display run, public LAN/Internet run, non-POSIX socket backend,
multi-client soak, authentication/encryption review, or long-duration dedicated-server soak was run.

## Completed Follow-up Scope — BSP Presentation and Networking Polish

Status: complete as of 2026-05-01.

This run builds on the completed BSP gameplay-loop branch. The completed gameplay-loop plan package
was archived under `Plans/Archived/bsp_gameplay_loop/`. The completed presentation/networking polish
plan package is archived under `Plans/Archived/bsp_presentation_networking_polish/`.

Completed phases:

- Phase PN-0 — Plan archival and follow-up handoff: complete as of 2026-05-01.
- Phase PN-1 — Live scripted authoritative runtime integration: complete as of 2026-05-01.
- Phase PN-2 — Authoritative gameplay snapshot presentation: complete as of 2026-05-01.
- Phase PN-3 — Snapshot/delta/event transport contracts: complete as of 2026-05-01.
- Phase PN-4 — Local transport bridge and remote-ready contracts: complete as of 2026-05-01.
- Phase PN-5 — HUD/audio/toolchain polish: complete as of 2026-05-01.
- Phase PN-6 — Final docs, validation, and handoff: complete as of 2026-05-01.

Phase PN-0 completion notes:

- Moved the completed BSP gameplay-loop handoff plans from root `Plans/` into
  `Plans/Archived/bsp_gameplay_loop/` without content changes.
- Added new follow-up handoff files for BSP presentation and networking polish.
- Updated `Plans/NEXT.md`, `docs/Design.md`, and `docs/BspAuthoring.md` so the then-current PN scope
  was scripted live-runtime integration, authoritative gameplay snapshot presentation, remote-ready
  snapshot/delta/event contracts, local bridge work, and presentation polish over completed BSP
  gameplay-loop foundations.
- No source or build behavior changes were introduced.

Validation run:

```bash
git status --short
git diff -- Plans docs
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Result: documentation-only Phase PN-0 completed on 2026-05-01. The active grep was expected to return
only historical/archive references in `docs/ImplementationStatus.md`, `Plans/NEXT.md`, and PN plan
text; the old gameplay-loop files are no longer root-level active handoffs. Validation was limited to
repository inspection because no code changed.

Phase PN-1 completion notes:

- `prepare_application_runtime()` now detects trigger/object-collider script bindings in the loaded BSP
  `RuntimeWorld` and constructs a sandboxed server-authoritative `ScriptedWorldSession` for those maps;
  maps without script bindings still construct the plain `WorldSession` loopback runtime.
- Added a narrow script registry loader for live runtime preparation. Script ids resolve relative to
  `ApplicationConfig::script_root` when provided, otherwise the map directory; absolute ids, drive
  paths, parent escapes, missing source files, and root escapes fail preparation deterministically.
- `LocalLoopbackRuntime` keeps its public `latest_snapshot()`/`update(input, delta_seconds)` shape while
  internally supporting either plain or scripted sessions. Frame results now preserve `script_events`,
  `script_errors`, native script `command_results`, and whether the frame used scripted mode.
- Scripted session forwarding preserves existing object-collider mutation APIs needed by runtime tests.
- Display-free tests cover no-script plain preparation, scripted preparation diagnostics, missing script
  source failure, import-time script path escape rejection, scripted frame events/errors, and scripted
  BSP pickup/door state through authoritative snapshots.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client -j$(nproc)
cmake --build build --target stellar_client_local_loopback_runtime_test stellar_client_map_validation_smoke stellar_scripted_world_session_test stellar_script_command_processor_test stellar_bsp_playable_world_smoke_test -j$(nproc)
ctest --test-dir build -R '^(client_local_loopback_runtime|client_map_validation_smoke|client_cli_map_validation|scripted_world_session|script_command_processor|bsp_playable_world_smoke|bsp_scripted_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, `stellar-client` built, focused PN-1 targets built, focused PN-1 CTest
passed 10/10, and full default CTest passed 41/41 on 2026-05-01.

Phase PN-2 completion notes:

- Added a client-side presentation adapter that converts server-owned `WorldSnapshot` /
  `GameplayWorldSnapshot` data into backend-neutral `graphics::BillboardSprite` draw data without GPU
  handles or gameplay ownership in authoritative snapshots.
- Active sprite entities and active pickup entities now present as color-only billboards; inactive or
  collected pickups are filtered, trigger/object-collider/player entities remain hidden by default, and
  deterministic door/gate debug markers are available only through an explicit presentation flag.
- `LevelRenderer` now retains backend-neutral `LevelPresentationState`, derives `BillboardView` from
  the active `LevelRenderState`, and submits gameplay billboards after static level geometry through
  the existing `RenderLevel` abstraction for OpenGL rendering.
- The live client frame loop feeds presentation state after authoritative loopback runtime updates and
  clears presentation state for no-runtime/no-map fallback frames.
- Display-free tests cover snapshot conversion behavior, inactive filtering, debug marker gating,
  finite defaults, billboard view derivation, retained-state clearing, and static-then-billboard draw
  ordering.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_player_presentation_test stellar_render_level_inspection_test stellar_graphics_backend_selection_test stellar_gameplay_presentation_test stellar-client -j$(nproc)
ctest --test-dir build -R '^(player_presentation|render_level_inspection|graphics_backend_selection|gameplay_presentation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused PN-2 targets built, focused PN-2 CTest passed 4/4, and full
default CTest passed 42/42 on 2026-05-01.

Phase PN-3 completion notes:

- Added `stellar_network` remote-ready transport contracts for client input commands, deterministic
  authoritative `NetworkWorldSnapshot` state, gameplay entities, server-approved `GameplayEvent`
  records, and structural `SnapshotDelta` baselines/targets.
- Server `WorldSnapshot` conversion preserves players and server-owned gameplay entities with stable
  id ordering; script command/error conversion emits pickup, door-state, script-command, and
  script-error presentation approvals without adding client authority.
- Implemented a narrow deterministic binary codec with explicit little-endian integers, bounded
  strings/vectors, finite-float validation, clean `std::expected` decode failures, and no socket,
  prediction, or reconciliation behavior.
- Display-free tests cover snapshot/event round trips, sprite/pickup/door entity data, delta
  round-trip/apply, invalid/truncated data, oversized strings/vectors, non-finite float rejection, and
  deterministic byte output.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_snapshot_codec_test stellar_snapshot_delta_test stellar_server_world_session_test stellar_scripted_world_session_test -j$(nproc)
ctest --test-dir build -R '^(snapshot_codec|snapshot_delta|server_world_session|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused PN-3 targets built, focused PN-3 CTest passed 4/4, and full
default CTest passed 44/44 on 2026-05-01.

Phase PN-4 completion notes:

- Added a transport-neutral `ClientTransport`/`ServerTransport` seam with in-memory local loopback
  endpoints that preserve reliable FIFO packet order and carry opaque encoded message payloads.
- Added a local authoritative server bridge that decodes client movement command requests, keeps the
  configured local server player slot authoritative, ticks plain or scripted sessions, and emits encoded
  full snapshots, structural deltas, and server-approved gameplay events over the same local transport.
- Added a client world receiver that accepts full snapshots, applies deltas to its current baseline,
  exposes the latest authoritative snapshot, queues gameplay events for presentation, and rejects
  malformed packets without prediction or reconciliation.
- Remote sockets remain deferred; the implemented bridge is in-memory/local and transport-neutral.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_loopback_transport_test stellar_client_world_receiver_test stellar_snapshot_codec_test stellar_snapshot_delta_test -j$(nproc)
ctest --test-dir build -R '^(loopback_transport|client_world_receiver|snapshot_codec|snapshot_delta)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, focused PN-4 targets built, focused PN-4 CTest passed 4/4, and full
default CTest passed 46/46 on 2026-05-01.

Phase PN-5 completion notes:

- Added a client HUD presentation cache driven only by server-approved gameplay events, including
  pickup messages, door-state messages, script diagnostics, bounded history, and deterministic reset
  behavior.
- Added an audio event router with an abstract request sink, production `NoOpAudioRequestSink`, and
  test-only fake sink over server-approved gameplay events so pickup/door plus optional script-error
  diagnostics/audio can be observed without making audio a gameplay authority.
- Added BSP FGD/toolchain helper coverage and updated `docs/BspAuthoring.md` authoring guidance for
  the PN-5 pickup/door presentation path, including the requirement that dotted Stellar keys reach BSP
  as dotted keys or importer-supported aliases.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(hud_presentation|audio_event_router|client_world_receiver|gameplay_presentation|bsp_authoring_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure succeeded, full debug build succeeded, focused PN-5 CTest passed 5/5, and full
default CTest passed 48/48 on 2026-05-01.

Phase PN-6 completion notes:

- Marked BSP presentation/networking polish complete and archived the completed PN plan package under
  `Plans/Archived/bsp_presentation_networking_polish/`.
- Updated `Plans/NEXT.md` to point at post-PN options instead of presenting PN first slices as active
  work.
- Updated `docs/Design.md` to describe current live scripted authoritative local runtime behavior,
  non-authoritative gameplay snapshot presentation, deterministic snapshot/delta/event contracts, and
  the local in-memory transport bridge without claiming remote sockets or real multiplayer lifecycle.
- Tightened HUD/audio documentation so HUD and audio remain presentation caches/routes only. Production
  audio feedback currently has an abstract sink plus `NoOpAudioRequestSink`; fake sinks are test-only,
  miniaudio playback/assets/spatial audio remain deferred, and missing sound diagnostics belong to the
  sink contract/test fake rather than a production local asset implementation.
- Clarified BSP authoring/tooling guidance so dotted Stellar keys such as `stellar.script` must reach
  BSP entity text unchanged or through importer-supported aliases; underscore FGD field names are
  editor-facing placeholders unless the editor/toolchain remaps them before export.

Final PN-6 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_|player_presentation|gameplay_presentation|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|snapshot_|loopback_transport|client_world_receiver|world_metadata_validation|collision_validation|character_controller|hud_presentation|audio_event_router)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n 'BspGameplayLoop-AgentPlan\|ProjectStellar-BSP-GameplayLoop-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Result: final validation passed on 2026-05-01. Configure succeeded, full debug build completed with
`ninja: no work to do`, full default CTest passed 48/48, and focused PN/BSP/runtime/render/client/
snapshot/HUD/audio CTest passed 38/38.

Audit interpretation:

- Retired importer search returned only documentation/audit-command references in
  `Plans/kilo_bsp_gameplay_followup_plans/07-Phase-PN6-Final-Docs-Validation.md` and
  `docs/ImplementationStatus.md`; no active source, build, or runtime implementation references were
  found.
- Authority-violation wording search returned only explicit prohibitions/deferrals in
  `docs/BspAuthoring.md`, `docs/Design.md`, and `docs/ImplementationStatus.md`, plus the documented
  audit command; no active implementation or authority violation was found.
- Old BSP gameplay-loop plan filename search returned historical PN source-plan references,
  documented audit commands, and archived-plan references in `docs/ImplementationStatus.md`; no
  root-level active handoff to the completed gameplay-loop plan remains.

Known deferred post-PN items:

- Remote socket transport and real multiplayer connection/session lifecycle.
- Client prediction, reconciliation, and interpolation.
- Richer HUD rendering, UI, inventory presentation, and VFX.
- miniaudio playback integration, local audio assets, and spatial audio/listener updates.
- Sprite atlas packing and sprite sheet animation.
- BSP editor/export remapping automation for dotted Stellar keys and FGD placeholder fields.
- Moving brushes, dynamic bodies, Source/VBSP, full PBR, model/animation systems, and client-side
  gameplay scripting remain out of scope unless explicitly requested.

## Branch Scope — Gameplay Loop Expansion over BSP Maps

This branch begins after `collision-movement` merges to `main`. Treat collision, movement,
trigger, object-collider, Lua scripting, BSP canonical migration, BSP rendering, and BSP hardening as
completed foundations, not as active work to restart.

The selected branch scope is gameplay loop expansion over BSP maps while preserving server authority
and display-free default validation.

Completed focus areas:

- ECS/entity spawn from BSP metadata.
- Player presentation from authoritative snapshots.
- Sprite, animation, and interaction loop.
- Item pickup and scripted doors/gates using the existing Lua command path.

Completed implementation plan:

- `Plans/Archived/bsp_gameplay_loop/BspGameplayLoop-AgentPlan.md` — concise historical handoff.
- `Plans/Archived/bsp_gameplay_loop/ProjectStellar-BSP-GameplayLoop-AgentPlan.md` — detailed
  historical master plan.

Archived branch gameplay unit policy: 1 Stellar gameplay world unit equaled 1 inch, authored BSP
coordinates imported without scale conversion, and player capsule center spawns were authored half the
capsule height above the floor. Current active authoring policy is Z-up and documented in the completed
Stellar TrenchBroom BSP30 compatibility section above.

Follow-up implementation should use `Plans/NEXT.md` for the next recommended post-PN options and this
file as the source of truth for branch completion notes. Archived phase plans under `Plans/Archived/`
are historical context unless this file explicitly names one as active.

Do not add Source/VBSP support, dynamic rigid bodies, full PBR, client-side gameplay scripting,
renderer/audio gameplay authority, or retired importer functionality unless explicitly requested.

## BSP Gameplay Loop — Completed Phase Status

- Phase 0 — Active gameplay-loop handoff lock-in: complete as of 2026-05-01.
- Phase 1 — Inch-based world scale and gameplay tuning: complete as of 2026-05-01.
- Phase 2 — Procedural developer textures for inch-scale BSP authoring: complete as of 2026-05-01.
- Phase 3 — Load the configured BSP map into the live client path: complete as of 2026-05-01.
- Phase 4 — Authoritative player camera drives level rendering: complete as of 2026-05-01.
- Phase 5 — Minimal ECS/entity spawn from BSP metadata: complete as of 2026-05-01.
- Phase 6 — Single-room controllable player loop: complete as of 2026-05-01.
- Phase 7 — First interaction loop, pickup and scripted door/gate: complete as of 2026-05-01.
- Phase 8 — Final branch hardening and documentation: complete as of 2026-05-01.

Phase 8 completion notes:

- Finalized the branch-facing handoff docs after Phases 0-7 and recorded the BSP gameplay-loop branch
  as complete.
- Confirmed active design documentation covers the inch-scale unit policy, live BSP client loop,
  metadata-driven server entity spawn direction, and pickup plus scripted door/gate interaction loop.
- Confirmed BSP authoring documentation covers inch-scale gameplay-room examples and all procedural
  developer texture names and aliases.
- Updated `Plans/NEXT.md` to point at the next recommended post-branch scope instead of presenting the
  completed gameplay-loop phases as active work.
- Active retired-importer references are limited to documented audit commands and historical notes in
  this status file; archived plans and build outputs remain excluded from active audits.

Final Phase 8 validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_|player_presentation|server_world_session|scripted_world_session|trigger_script|object_collider_script|script_command_processor|world_metadata_validation|collision_validation|character_controller)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'meter\|metre\|1\.8F\|0\.35F\|0\.8F\|6\.0F' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
```

Result: final validation passed on 2026-05-01. Configure/build succeeded, full default CTest passed
41/41, and focused BSP/runtime/render/client/player/server/script/collision validation passed 31/31.
The active retired-importer audit returned only documented audit command references outside archived
plans and ignored build outputs. The unit audit found no active prose reverting gameplay defaults to an
old scale policy; remaining matches are the documented audit command plus numeric literals used for
inch-scale constants, OpenGL texture enum names, aspect ratios, generated BSP room coordinates, and
synthetic test geometry that intentionally overrides local values.

Known deferred post-branch items:

- Remote networking and snapshot/delta expansion.
- Client prediction and reconciliation.
- Client presentation polish for sprites, animation, UI/HUD, inventory, VFX, and audio events.
- BSP toolchain/editor workflow polish beyond deterministic procedural developer texture fallback.
- Rich animation/model systems, moving brush simulation, Source/VBSP, dynamic rigid bodies, full PBR,
  and client-side gameplay scripting remain out of scope unless explicitly requested.

Phase 0 completion notes:

- Added the original root-level gameplay-loop handoff files, now archived under
  `Plans/Archived/bsp_gameplay_loop/`, as the concise handoff derived from the detailed master plan.
- Updated `Plans/NEXT.md`, `docs/ImplementationStatus.md`, `docs/Design.md`, and
  `docs/BspAuthoring.md` to point agents at the gameplay-loop plan and record the inch-scale unit
  policy.
- No source behavior changes were introduced.

Phase 1 completion notes:

- Added `include/stellar/core/WorldUnits.hpp` as the code source for inch-scale gameplay constants
  and trivial inch/foot conversion helpers.
- Updated default authoritative player capsule, movement simulation, and player camera presentation
  tuning to inch-scale values: 72 inch height, 16 inch radius, 160 inches/second walk speed, and a
  4096 unit debug far plane.
- Tiny synthetic movement and controller tests continue to override their local geometry and tuning
  explicitly, while default-value assertions now cover inch-scale controller, movement, and camera
  settings.
- No BSP importer scale conversion was introduced; authored BSP coordinates remain imported 1:1.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_character_controller_test stellar_server_movement_simulation_test stellar_player_presentation_test -j$(nproc)
ctest --test-dir build -R '^(character_controller|server_movement_simulation|player_presentation)$' --output-on-failure
```

Result: configure/build and focused CTest passed on 2026-05-01.

Phase 2 completion notes:

- Added deterministic procedural BSP developer texture fallback for `stellar_dev_grid_12`,
  `stellar_dev_grid_16`, `stellar_dev_grid_32`, `stellar_dev_grid_64`,
  `stellar_dev_player_72`, `stellar_dev_wall_96`, and safe `dev/...` slash aliases.
- Known developer textures now generate source-neutral `ImageAsset`, `TextureAsset`, sampler, and
  material bindings without WAD files, using nearest filtering and repeat wrapping for crisp inch-scale
  authoring marks.
- BSP base texture coordinates are normalized by resolved texture dimensions when image data is
  available, so standard BSP texture axes treat one texel as one authored world inch unless authors
  change texture scaling.
- Unknown missing or external textures continue to use existing deterministic fallback material
  behavior and missing-texture diagnostics.

Validation run:

```bash
cmake --build build --target stellar_bsp_materials_test stellar_render_level_upload_test stellar_render_level_inspection_test -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|render_level_upload|render_level_inspection)$' --output-on-failure
```

Result: focused Phase 2 build and CTest passed on 2026-05-01.

Phase 3 completion notes:

- Added display-free client runtime preparation that keeps the `LevelAsset` loaded during startup
  validation alive for the prepared `RuntimeWorld`, `WorldSession`, and `LocalLoopbackRuntime` chain.
- The live client path now reuses the validated BSP level instead of re-importing it, passes that
  loaded level into renderer creation, and preserves no-map debug cube fallback behavior.
- Configured BSP maps now instantiate an in-process authoritative `LocalLoopbackRuntime` using the
  default inch-scale movement/session tuning; no networking, prediction, or client scripting was
  added.
- Extended the client map validation smoke test to assert prepared runtime diagnostics, stable
  `RuntimeWorld` backing level lifetime, optional loopback state, and no-map fallback preparation.
- Linked `stellar-client` and client startup validation support with `stellar_client_runtime` so the
  live client bootstrap can own local loopback runtime state.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar_client_map_validation_smoke stellar_client_local_loopback_runtime_test stellar_render_level_upload_test -j$(nproc)
ctest --test-dir build -R '^(client_map_validation_smoke|client_cli_map_validation|client_cli_validate_map|client_local_loopback_runtime|render_level_upload)$' --output-on-failure
```

Result: configure/build and focused Phase 3 CTest passed on 2026-05-01.

Phase 4 completion notes:

- Added a backend-neutral graphics-facing `LevelRenderView`/`LevelRenderState` path so the client can
  drive BSP rendering from authoritative player presentation without making `stellar_graphics` depend
  on `stellar_client`.
- `LevelRenderer` now supports a focused `set_render_view`/`clear_render_view` API; live client frames
  process input, advance `LocalLoopbackRuntime`, extract the local authoritative player snapshot,
  build a `PlayerCameraFrame`, and convert it into level render state before drawing.
- Player-camera frames pass the camera world position through to `RenderLevel` for optional BSP/PVS
  visibility culling. The automatic bounds-fit camera remains the fallback for no-map or missing
  player-presentation state and does not force visibility culling.
- Display-free render inspection coverage now exercises camera override state, fallback culling
  disablement, and existing camera-position visibility culling behavior.

Validation run:

```bash
cmake --build build --target stellar_player_presentation_test stellar_render_level_inspection_test stellar_graphics_backend_selection_test stellar-client -j$(nproc)
ctest --test-dir build -R '^(player_presentation|render_level_inspection|graphics_backend_selection)$' --output-on-failure
```

Result: focused Phase 4 build targets and CTest regex passed on 2026-05-01.

Phase 5 completion notes:

- Added a minimal `server::GameplayWorld` entity model without a full ECS rewrite: deterministic
  `EntityId` allocation, `EntityKind`, transform data, inert metadata, player/entity bindings, and
  display-free snapshot/query APIs.
- `WorldSession` now owns the spawned gameplay world, binds the configured local `PlayerId` to the
  first player spawn marker when available, and preserves existing movement/player snapshot behavior.
- Gameplay entity spawn covers player spawn markers, sprite markers with copied sprite/alpha/size
  metadata, pickup candidates from object-collider or `info_stellar_spawn`-style entity markers with
  pickup/item archetypes, trigger/object-collider markers, and door/gate metadata from trigger/entity
  markers or named collision meshes.
- Spawn/import remains server-owned and inert: no renderer handles, audio handles, or script execution
  are introduced during metadata import or gameplay-world assembly.

Validation run:

```bash
cmake --build build --target stellar_server_gameplay_world_test stellar_server_world_session_test stellar_runtime_world_test stellar_world_metadata_validation_test -j$(nproc)
ctest --test-dir build -R '^(server_gameplay_world|server_world_session|runtime_world|world_metadata_validation)$' --output-on-failure
```

Result: focused Phase 5 build targets and CTest regex passed on 2026-05-01.

Phase 6 completion notes:

- Added the `gameplay_room` generated BSP fixture as an inch-scale single-room map while preserving
  the existing tiny playable fixtures for focused importer/script tests.
- The fixture is a 192x192 inch room from roughly `x/y = -96..96`, with floor `z = 0`, ceiling
  `z = 96`, a center `info_player_start` at `0 0 36`, static collision floor/walls/ceiling, one
  sprite marker, one future pickup object-collider marker, and one future door/gate trigger marker.
- Display-free smoke coverage now imports the room BSP, builds `RuntimeWorld`, advances a
  `LocalLoopbackRuntime` with forward/right authoritative input, verifies movement direction and room
  wall containment, and checks identical input produces deterministic authoritative snapshots.
- The live client no-map fallback remains unchanged; CLI map validation now exercises the generated
  inch-scale gameplay room fixture.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_bsp_fixture_writer stellar_bsp_playable_world_smoke_test stellar_client_local_loopback_runtime_test stellar-client -j$(nproc)
ctest --test-dir build -R '^(bsp_fixture_writer|bsp_playable_world_smoke|client_local_loopback_runtime|client_cli_validate_map|client_cli_map_validation)$' --output-on-failure
```

Result: focused Phase 6 build targets and CTest regex passed on 2026-05-01. Manual renderer/display
validation was skipped because the requested validation scope is display-free and no GPU/display
manual check was run.

Phase 7 completion notes:

- Added minimal server-owned interaction state to `GameplayWorld`: pickup entities can become
  inactive after collection, and door/gate entities mirror named collision mesh open/closed state for
  presentation snapshots without renderer ownership.
- Object-collider pickup enter events now emit native `gameplay.collect_pickup` commands through the
  existing script-command processing path; native code validates active pickup state, disables the
  collider, and prevents repeated collection on later enters/stays.
- Scripted trigger enter/stay/exit behavior continues to use the sandboxed Lua hook path; scripts can
  emit `collision.set_mesh_enabled` for named gate meshes, and accepted native commands update both
  authoritative collision state and gameplay door/gate metadata.
- Added display-free coverage for collect-once pickup behavior, collider disablement preventing
  repeated enters, gate collision toggling through Lua command output, deterministic invalid command
  failure, and gameplay snapshot state updates.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_scripted_world_session_test stellar_trigger_script_system_test stellar_object_collider_script_system_test stellar_script_command_processor_test stellar_bsp_playable_world_smoke_test -j$(nproc)
ctest --test-dir build -R '^(scripted_world_session|trigger_script|object_collider_script|script_command_processor|bsp_playable_world_smoke|bsp_scripted_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke)$' --output-on-failure
cmake --build build --target stellar_server_gameplay_world_test stellar_server_world_session_test -j$(nproc)
ctest --test-dir build -R '^(server_gameplay_world|server_world_session)$' --output-on-failure
```

Result: focused Phase 7 validation passed on 2026-05-01.

## BSP Authoring and Presentation Hardening — Complete

BSP authoring and presentation hardening is complete as of 2026-05-01:

- Added structured BSP import, authoring, and validation diagnostics.
- Added BSP leaf/PVS visibility data and optional presentation-only culling.
- Added BSP lightmap metadata, material/texture import, and deterministic fallback behavior.
- Documented and validated BSP entity authoring conventions for spawns, triggers, sprites, object
  colliders, static collision brushes, and script metadata.
- Added generated BSP fixtures plus display-free/headless validation coverage, including client map
  validation before window or graphics context creation.
- Archived the completed hardening plan at
  `Plans/Archived/project_stellar_bsp_hardening_plan/`.
- `Plans/NEXT.md` now points to gameplay loop expansion over BSP maps as the next active scope.

Final phase status:

- Phase 0 — Active handoff and `NEXT.md` lock-in: complete as of 2026-05-01.
- Phase 1 — BSP diagnostics and `LevelAsset` contract foundation: complete as of 2026-05-01.
- Phase 2 — BSP PVS, leaf visibility, and render culling: complete as of 2026-05-01.
- Phase 3 — BSP lightmaps, textures, materials, and WAD fallback: complete as of 2026-05-01.
- Phase 4 — BSP entity, sprite, trigger, and object authoring conventions: complete as of 2026-05-01.
- Phase 5 — BSP validation tooling and regression fixtures: complete as of 2026-05-01.
- Phase 6 — Final hardening, documentation, archive, and `NEXT.md` completion: complete as of
  2026-05-01.

Final validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|render_level|runtime_world|client_map_validation|client_cli_map_validation|world_metadata_validation|collision_validation|scripted_world_session|bsp_playable_world_smoke)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
```

Result: final validation passed on 2026-05-01. Configure/build succeeded, full default CTest passed
40/40, and focused BSP/runtime/render/client/script validation passed 22/22. Active retired-importer
reference search returned only this documented validation command outside archived plans and ignored
build outputs; no active retired-importer functionality remains.

Known deferred post-hardening items:

- Gameplay loop expansion over BSP maps.
- BSP toolchain/editor workflow polish.
- Networking/snapshot expansion.
- Source/VBSP, moving brush simulation, dynamic rigid bodies, full PBR, model/animation systems,
  and client-side gameplay scripting remain out of scope unless explicitly requested.

Phase 1 completion notes:

- Added structured BSP import diagnostics and additive load-with-report APIs while preserving the
  existing `std::expected<LevelAsset, Error>` load path for fatal parse errors.
- Expanded source-neutral `LevelAsset` visibility/lightmap contracts with classic BSP leaf records,
  compressed PVS bytes, raw lighting bytes, and minimal lightmap metadata placeholders.
- Parser internals now retain nodes, leaves, marksurfaces, visibility bytes, and lighting bytes for
  later PVS/lightmap hardening without adding renderer handles or gameplay authority.
- Added display-free importer coverage for non-fatal missing player spawn warnings, fatal unexpected
  errors, no-vis placeholders, and raw visibility/lighting lumps.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_importer|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded. Targeted BSP/runtime/collision tests passed 5/5, and full
default CTest passed 31/31.

Phase 2 completion notes:

- Added source-neutral BSP leaf/PVS visibility helpers for leaf lookup, classic compressed PVS
  decompression, and per-surface visibility masks with all-visible fallback behavior.
- BSP import now maps leaf marksurfaces through constructed `LevelSurface` indices, preserves leaf
  bounds/contents/PVS offsets, and diagnoses invalid marksurface or PVS data without changing
  authoritative collision, movement, triggers, object colliders, or scripting.
- `RenderLevel` can optionally accept a camera world position and cull static level surfaces through
  BSP visibility data while falling back to the previous all-surface submission path when visibility
  is unavailable or invalid.
- Billboard submission remains after static level geometry, and rendering remains behind the shared
  `GraphicsDevice` abstraction with no raw backend-specific BSP API.
- Added display-free `bsp_visibility` coverage plus render-level synthetic culling coverage.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_visibility|bsp_importer|render_level_upload|render_level_inspection|bsp_playable_world_smoke)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded. Focused Phase 2 suite passed 5/5, and full default CTest
passed 32/32.

Phase 3 completion notes:

- Added BSP miptex metadata parsing, safe embedded miptex extraction into source-neutral
  `ImageAsset`/`TextureAsset` records, and deterministic fallback materials for external or missing
  WAD-backed textures.
- BSP import now preserves texture source names, emits non-fatal missing-texture diagnostics for
  external fallback, imports per-face lightmap image metadata from valid lighting offsets/styles,
  assigns material lightmap indices, and populates secondary `uv1` coordinates where lightmap data is
  available.
- Invalid or out-of-range lighting offsets are diagnosed and fall back to unlit material behavior.
- `RenderLevel` uploads represented base textures and lightmap textures through `GraphicsDevice`,
  binds lightmaps as backend-neutral material texture bindings on texcoord set 1, and preserves
  deterministic fallback behavior for missing resources.
- External WAD decoding, palette-accurate miptex color, lightmap atlasing, and shader-side visible
  lightmap modulation were not completed in this historical hardening phase. Later TB-FULL work added
  safe WAD3 lookup and renderer lightmap sampling for the Stellar BSP30 profile without full PBR or
  backend-specific BSP APIs.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_materials|bsp_lightmaps|bsp_importer|render_level_upload|render_level_inspection)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded. Focused Phase 3 suite passed 5/5, and full default CTest
passed 34/34.

Phase 4 completion notes:

- Added `docs/BspAuthoring.md` with concrete BSP entity key conventions for player spawns,
  gameplay spawns, trigger volumes, sprite billboards, object-collider sensors, and static collision
  brush entities.
- BSP import now validates deterministic vector and bool-like authoring fields, preserves raw entity
  key/value metadata, derives brush-model bounds for trigger/object-collider markers, supports
  `origin` + `stellar.extents` fallback volumes, and keeps script path escape checks fatal.
- Sprite script bindings remain unsupported and are ignored with diagnostics instead of creating
  client-side or presentation-owned gameplay behavior.
- Added display-free `bsp_entity_authoring` coverage for documented conventions, malformed
  vectors/booleans, script path rejection, brush bounds, fallback volumes, and sprite script
  diagnostics.

Validation run:

```bash
cmake -S . -B build-phase4-kojima -DCMAKE_BUILD_TYPE=Debug
cmake --build build-phase4-kojima -j$(nproc)
ctest --test-dir build-phase4-kojima -R '^(bsp_entity_authoring|bsp_importer|world_metadata_validation|bsp_playable_world_smoke|scripted_world_session)$' --output-on-failure
ctest --test-dir build-phase4-kojima --output-on-failure
```

Result: configure and build succeeded. Focused Phase 4 suite passed 5/5, and full default CTest
passed 35/35.

Phase 5 completion notes:

- Added a display-free BSP validation API that preserves existing load APIs while returning
  deterministic structured diagnostics, including fatal parse failures as error-severity report
  entries.
- Validation now covers binary/lump failures, invalid face polygon references, PVS and marksurface
  warnings, material fallback and lightmap diagnostics, entity authoring warnings, script path escape
  rejection, missing spawn warnings, and no-collision policy warnings without renderer or WAD
  requirements.
- Extended client validation with `--validate-map` alongside `--validate-config --map`; both paths
  validate maps headlessly before any window or graphics context is created.
- Expanded generated BSP fixtures and regression coverage for valid maps, PVS, textures/lightmaps,
  malformed lump bounds, invalid face references, malformed entity vectors, script path escapes,
  missing player spawn, no-collision policy, and authoring/client smoke behavior.
- Updated BSP authoring docs with validation command examples and common diagnostic explanations.

Validation run:

```bash
cmake -S . -B build-phase5-carmack -DCMAKE_BUILD_TYPE=Debug
cmake --build build-phase5-carmack -j$(nproc)
ctest --test-dir build-phase5-carmack -R '^(bsp_validation|bsp_importer|client_map_validation_smoke|client_cli_map_validation|bsp_authoring_smoke)$' --output-on-failure
ctest --test-dir build-phase5-carmack --output-on-failure
```

Result: configure and build succeeded. Focused Phase 5 suite passed 6/6, and full default CTest
passed 40/40.

Phase 0 completion notes:

- Locked `Plans/NEXT.md` to the BSP hardening plan package as the active implementation entry point.
- Recorded this hardening work as the current active branch scope without restarting BSP migration.
- Updated `docs/Design.md` to describe current BSP hardening rather than the completed migration as
  the near-term goal.
- No source or build behavior changes were introduced.

## BSP Canonical Migration — Complete

BSP canonical migration is complete as of 2026-05-01:

- BSP maps are now the canonical playable level format.
- Retired importer functionality, parser dependency, feature gates, startup paths, tests, and active
  docs have been removed.
- Runtime world assembly, static collision, server-authoritative movement, triggers,
  object-collider sensors, Lua hooks, client map validation, and level rendering operate from
  BSP-backed `LevelAsset` data.
- `Plans/NEXT.md` now points to BSP authoring and presentation hardening as the next recommended
  active scope.
- The completed migration plan is archived at
  `Plans/Archived/project_stellar_bsp_canonical_plan/`.

Final validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|character_controller|server_world_session|scripted_world_session|bsp_playable_world_smoke|bsp_scripted_collision_smoke|bsp_scripted_object_collider_smoke|billboard|render_level)' --output-on-failure
```

Result: configure/build succeeded, full CTest passed 31/31, and focused BSP/runtime/render/script
CTest passed 13/13.

Active retired-importer reference search: no active hits. `git grep` was used because `rg` is
unavailable in this environment.

Known deferred post-BSP items:

- BSP PVS/leaf visibility culling hardening.
- BSP lightmap presentation.
- External texture/WAD/material resolver polish.
- BSP editor/toolchain and map-authoring documentation.
- Sprite/entity authoring conventions.
- Gameplay loop expansion.

## BSP Canonical Migration — Phase Notes

The user selected BSP maps as the canonical playable level format. BSP replaces the retired
pre-BSP model-import path as the active level pipeline rather than being kept beside it.

Archived implementation entry points:

- `Plans/NEXT.md`
- `Plans/Archived/project_stellar_bsp_canonical_plan/00-MASTER-KILO-BSP-CANONICAL-PLAN.md`

Required migration outcome:

- BSP is the default and canonical playable level source.
- Retired importer code, startup validation, CMake options, fixtures, parser dependency, runtime
  assumptions, and active docs are removed during this migration.
- Existing server-authoritative movement, collision, Lua scripting, trigger, object-collider,
  billboard sprite, and display-free validation boundaries are preserved.

Phase BSP-0 is complete as of 2026-05-01:

- Locked active branch direction to BSP maps as canonical playable level format.
- Marked retired importer functionality for removal rather than side-by-side support.
- Replaced `Plans/NEXT.md` with the BSP migration handoff.
- Updated active design/agent guidance so new work no longer follows retired level assumptions.

Validation: documentation review and active-reference search.

Phase BSP-1 is complete as of 2026-05-01:

- Added source-neutral `LevelAsset` as the canonical runtime level input.
- Moved runtime world assembly off the retired scene-asset contract.
- Preserved existing backend-neutral collision, trigger, object-collider, scripting, and
  server-authority seams.
- Updated synthetic display-free tests to construct runtime worlds from `LevelAsset`.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(runtime_world|collision_world|character_controller|trigger_system|object_collider|server_world_session|scripted_world_session)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. Targeted Phase BSP-1 suite passed 7/7, and full
default CTest passed 26/26.

Phase BSP-2 is complete as of 2026-05-01:

- Added mandatory `stellar_import_bsp` loader for the legacy id Software BSP family.
- Added safe lump parsing, entity key/value parsing, and BSP-to-`LevelAsset` conversion.
- Built static geometry, named collision meshes, world metadata markers, and script bindings from
  BSP data.
- Added display-free BSP parser/importer regression tests using generated binary fixtures.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|runtime_world|collision_world|world_metadata_validation|collision_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted command completed
successfully for the matching runtime/collision tests; a corrected CTest regex also ran the new
`bsp_importer` test for 5/5 targeted passes. Full default CTest passed 27/27.

Phase BSP-3 is complete as of 2026-05-01:

- Runtime world assembly consumes source-neutral BSP-backed `LevelAsset` data and exposes object
  collider marker lookup alongside existing spawn, trigger, and sprite marker helpers.
- Server movement, static collision filters, triggers, object-collider sensors, and Lua script hooks
  are covered from generated BSP metadata through authoritative `ScriptedWorldSession` ticks.
- Client startup validation now uses `--map`/`map_path`, loads `.bsp` maps through
  `stellar_import_bsp`, and no longer has a retired importer feature-gate validation failure path.
- Added display-free BSP playable-world/scripted integration smoke coverage plus BSP-backed client
  map validation and CLI validation tests.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_|playable_world_smoke|scripted_playable_world_smoke|scripted_collision_smoke|scripted_object_collider_smoke|server_world_session|scripted_world_session|client_map_validation_smoke|client_cli_map_validation)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted command passed 9/9 selected
tests in this CTest regex mode, including server/session, scripted session, client map validation,
CLI map validation, and BSP-backed playable/scripted smoke aliases. A corrected CTest regex also ran
`bsp_importer` and `bsp_playable_world_smoke` for 11/11 targeted passes. Full default CTest passed
33/33.

Phase BSP-4 is complete as of 2026-05-01:

- Refactored active graphics presentation to consume BSP/`LevelAsset` static geometry through
  `RenderLevel`/`LevelRenderer` instead of retired scene data.
- Preserved the shared `GraphicsDevice` abstraction for OpenGL rendering; no BSP-specific raw backend
  API was added.
- Preserved billboard sprite rendering after static level geometry and updated display-free
  upload/submission tests for BSP geometry, default material fallback, surface material indices,
  and billboard ordering.
- Removed active renderer assumptions around scene graph pose, skinning, animation, and PBR-style
  materials; retained temporary compatibility aliases for old `RenderScene`/`SceneRenderer` names
  while active APIs are LevelAsset-focused.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(render_level|render_scene|billboard|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build -R '^(render_level_.*|graphics_backend_selection)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: default configure and build succeeded. The phase-plan targeted regex matched and passed
`graphics_backend_selection`; the corrected graphics regex ran `render_level_upload`,
`render_level_inspection`, and `graphics_backend_selection` for 3/3 passes. Full default CTest passed
33/33.

Phase BSP-5 is complete as of 2026-05-01:

- Removed the retired model-importer code path, parser dependency, feature gate, fixtures, and
  integration tests from active source, build configuration, and default validation.
- Removed retired scene, skin, animation asset/runtime types and active graphics draw plumbing that
  only served the retired model path.
- Kept BSP-backed level import/runtime/render tests and renamed scripted integration smoke
  registrations to BSP names.
- Cleaned active docs and agent guidance so BSP is the canonical level format and the retired model
  importer is no longer an active feature.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Result: configure and build succeeded; full CTest passed 31/31.

Active retired-importer reference search: no active hits. `git grep` was used because `rg` is
unavailable in this environment.

## Historical Context

Earlier collision, movement, billboard, metadata, Lua scripting, trigger, and object-collider work
remains useful historical context, but it is not the active branch plan. The active branch direction
is now BSP-first level import, runtime assembly, server-authoritative gameplay seams, and rendering.

Preserve these invariants in follow-up work:

- Server authority remains mandatory.
- Lua scripting remains mandatory and sandboxed.
- Default tests remain display-free.
- OpenGL remains the current renderer through shared graphics abstractions.
- Source/VBSP, moving brush simulation, third-party physics, dynamic rigid bodies, full PBR,
  model/animation systems, and client-side gameplay scripting remain out of scope unless explicitly
  requested.
