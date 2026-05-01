# Project Stellar: Implementation Status

Branch target: `bsp-gameplay-loop`

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

- `Plans/BspGameplayLoop-AgentPlan.md` — concise active agent handoff.
- `Plans/ProjectStellar-BSP-GameplayLoop-AgentPlan.md` — detailed master plan.

Branch gameplay unit policy: 1 Stellar gameplay world unit equals 1 inch, Y is up, BSP authored
coordinates import without scale conversion, and player capsule center spawns should be half the
capsule height above the floor.

Follow-up implementation should use `Plans/NEXT.md` for the next recommended scope after this branch
and this file as the source of truth for branch completion notes. Archived phase plans under
`Plans/Archived/` are historical context unless this file explicitly names one as active.

Do not add Source/VBSP support, dynamic rigid bodies, full PBR, client-side gameplay scripting,
renderer/audio gameplay authority, or retired importer functionality unless explicitly requested.

## BSP Gameplay Loop — Active Phase Status

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

- Added `Plans/BspGameplayLoop-AgentPlan.md` as the concise active handoff derived from the detailed
  master plan.
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
- The fixture is a 192x192 inch room from roughly `x/z = -96..96`, with floor `y = 0`, ceiling
  `y = 96`, a center `info_player_start` at `0 36 0`, static collision floor/walls/ceiling, one
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
  `GraphicsDevice` abstraction with no OpenGL/Vulkan-specific BSP API.
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
  lightmap modulation remain deferred; current support records/uploads stable source-neutral data
  without full PBR or backend-specific APIs.

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

- Added mandatory `stellar_import_bsp` loader for the classic BSP29/BSP30 family.
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
- Preserved OpenGL/Vulkan backend parity through the shared `GraphicsDevice` abstraction; no
  BSP-specific raw backend API was added.
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
- OpenGL and Vulkan remain runtime-selectable through shared abstractions.
- Source/VBSP, moving brush simulation, third-party physics, dynamic rigid bodies, full PBR,
  model/animation systems, and client-side gameplay scripting remain out of scope unless explicitly
  requested.
