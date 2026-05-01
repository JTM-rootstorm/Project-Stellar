# Project Stellar: Next Implementation Plans

Branch target: `collision-movement`

Prepared for: an implementation AI agent such as Codex/Kilo/other code-writing agent.

## Context snapshot

The branch already contains completion notes for the earlier Phase 6 world-authoring work:

- Phase 6A: static glTF collision extraction.
- Phase 6B: collision queries and minimal movement resolution.
- Phase 6C: billboard sprite rendering.
- Phase 6D: world metadata extraction.

Do **not** restart those phases from scratch. Treat them as implemented first passes and focus the next round on making the features coherent, usable at runtime, and harder to break.

## Current status

Phase 12E scripted object-collider smoke and documentation is complete as of 2026-05-01:

- Added display-free `scripted_object_collider_smoke` coverage with a generated glTF fixture that
  imports `COLLIDER_PickupGem`, builds deterministic runtime AABB object-collider data, enters it
  through authoritative `WorldSession` movement, invokes the bound Lua object-collider hook, emits
  `object_collider.set_enabled` plus `gameplay.pickup_collected`, applies the validated native
  disable command, and proves later ticks do not emit stay for the disabled collider.
- The smoke asserts Phase 12D synchronous exit policy: disabling an overlapped object collider emits
  the exit on the command result, not as a later snapshot/hook replay.
- Added CMake target `stellar_scripted_object_collider_smoke_test` and CTest name
  `scripted_object_collider_smoke` under the existing glTF + Lua enabled gates.
- Updated `docs/Design.md` for `COLLIDER_<Name>` object-collider authoring, sensor-only semantics,
  server-authoritative Lua hooks/commands, and deferred solid moving blockers.

Phase 12A-D authoritative object-collider integration is complete as of 2026-05-01:

- Phase 12A hardened `ObjectColliderSystem` lifecycle semantics with deterministic live
  enable/disable, upsert, remove, preserving-overlap replacement, duplicate-id diagnostics, and
  synchronous exit results for disabled/removed previously-overlapped colliders.
- Phase 12B integrated object-collider events into `WorldSession` snapshots using authoritative
  post-movement capsule overlap checks, runtime-world collider construction on reset, and explicit
  mutation APIs for script/native command application.
- Phase 12C added authored `COLLIDER_<Name>` metadata import and validation for sensor-style AABB
  runtime colliders, separate from `COL_*` static triangle collision extraction.
- Phase 12D added `ObjectColliderScriptSystem`, object-collider Lua hook loading/invocation, and
  native validation/application for `object_collider.set_enabled` by finite integer-like collider id.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target stellar_scripted_object_collider_smoke_test stellar_object_collider_test stellar_server_world_session_test stellar_scripted_world_session_test stellar_script_command_processor_test stellar_import_gltf_regression stellar_world_metadata_validation_test -j$(nproc)
ctest --test-dir build -R '^(scripted_object_collider_smoke|object_collider|server_world_session|scripted_world_session|script_command_processor|gltf_importer_regression|world_metadata_validation)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Result: targeted Phase 12E suite passed (7/7) and full CTest passed (31/31). Build linked
successfully; the vendored Lua `tmpnam` linker warning remains unchanged.

Phase 12D authoritative object-collider scripting is complete as of 2026-05-01:

- Added `ObjectColliderScriptSystem` with deterministic Lua hooks for
  `on_object_collider_enter`, `on_object_collider_stay`, and `on_object_collider_exit` using
  authoritative `WorldSnapshot` object-collider events.
- `ScriptedWorldSession` now loads unique trigger and object-collider scripts once, then processes
  native movement, trigger events, object-collider events, trigger script callbacks,
  object-collider script callbacks, and native command validation/application in that order.
- Added native validation/application for `stellar.emit_event("object_collider.set_enabled", ...)`
  by finite integer-like `uint32_t` id only; invalid fields are rejected, unknown ids report
  `not_found`, and disable-while-overlapped exits are surfaced synchronously on the command result.
- Metadata validation now treats script bindings on object-collider markers as supported while
  keeping sprite, portal, player spawn, and entity-spawn script bindings unsupported.

Phase 12C authored object-collider metadata is complete as of 2026-05-01:

- Added `WorldMarkerType::kObjectCollider` and glTF `COLLIDER_<Name>` metadata import for
  sensor-style AABB object colliders; `COLLIDER_*` remains separate from static `COL_*` triangle
  collision extraction.
- Added builder helpers that convert copied world metadata into deterministic nonzero-id runtime
  AABB object colliders in metadata marker order.
- Extended metadata validation with deterministic object-collider name, duplicate-name, empty extent,
  large extent, and unsupported script-binding diagnostics. Lua hooks, solid collision, oriented
  boxes, ECS spawning, rendering, and networking remain deferred.

Phase 11A-F collision scripting slice is complete as of 2026-04-30:

- Added capsule-aware trigger overlap so authoritative trigger events use the same vertical capsule
  dimensions as character movement instead of the previous center-sphere approximation.
- Added `RuntimeCollisionState`, a server-owned enable/disable overlay for immutable imported static
  collision meshes. Empty and duplicate authored mesh names produce deterministic diagnostics;
  duplicate names are toggled together by name.
- Added filtered collision queries for raycasts, ground probes, sphere movement, triangle queries,
  and character movement. `WorldSession` now owns the runtime collision state, rebuilds it on reset,
  and routes authoritative movement through that state.
- Added native script command processing for `collision.set_mesh_enabled`. Lua scripts still emit
  primitive events only; native server code validates fields and applies approved collision changes
  after trigger callbacks, affecting subsequent authoritative ticks.
- Added a backend-neutral kinematic `ObjectColliderSystem` foundation for deterministic object
  overlap events without adding rigid bodies, ECS ownership, networking, rendering, or script hooks.
- Added display-free `scripted_collision_smoke` coverage proving an authored glTF trigger script can
  disable a named `DoorBlocker` collision mesh and allow the next authoritative movement tick to pass
  through it deterministically.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Result: full CTest passed (29/29). Build linked successfully; the vendored Lua `tmpnam` linker
warning remains unchanged.

Phase 10E and the overall Phase 10 Lua scripting slice are complete as of 2026-04-30:

- Added dedicated display-free `scripted_playable_world_smoke` coverage for the authored glTF ->
  collision validation -> metadata validation -> `RuntimeWorld` -> `ScriptedWorldSession` path.
- The scripted smoke fixture includes visible render geometry, collision-only floor/walls,
  `SPAWN_Player`, and `TRIGGER_DoorOpen` metadata with `stellar.script`/`stellar.table` extras.
- The smoke test uses an in-memory `ScriptRegistry`, verifies trigger script binding import, loads
  `scripts/door.lua`, emits `door_open_requested` only on the authoritative enter event, and checks
  deterministic repeat snapshots and script outputs without requiring display, GPU, or graphics
  context setup.
- `docs/Design.md` now reflects the `lua-scripting` branch direction, server-authoritative Lua
  scripting principles, dependency/build/test layout, and deferred-work alignment.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(scripted_playable_world_smoke|scripted_world_session|trigger_script|lua_runtime)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Result: all targeted tests passed (4/4) and full CTest passed (25/25). Build linked successfully;
the vendored Lua `tmpnam` linker warning remains unchanged.

Phase 10D is complete as of 2026-04-30:

- Added opt-in `ScriptRegistry` and `ScriptedWorldSession` APIs under `stellar_scripting`.
- `ScriptedWorldSession::create` wraps native `server::WorldSession`, scans current
  `RuntimeWorld` trigger script metadata, loads each unique script id into `LuaRuntime` once, and
  returns a deterministic `ScriptError` for missing script sources or load failures.
- Scripted ticks advance the native authoritative session first, then invoke Phase 10C
  `TriggerScriptSystem` callbacks and return a frame containing the native snapshot, script output
  events, and script errors. `latest_snapshot()` returns cached authoritative state without
  replaying script callbacks.
- Lua remains isolated to `stellar_scripting`; no Lua dependency was introduced into
  `stellar_server_core` or `stellar_world`.
- Added display-free `scripted_world_session` tests for create/load, missing script id, snapshot and
  script event output, deterministic repeatability, runtime script errors without crashes, and
  latest snapshot access without callback replay.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(scripted_world_session|trigger_script|lua_runtime)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Phase 10B is complete as of 2026-04-30:

- Added optional `WorldScriptBinding` metadata to authored world markers without introducing a Lua
  dependency into import, world, or server core code.
- glTF node `extras` now preserve existing `extras_json` behavior while narrowly extracting string
  `stellar.script` and `stellar.table` values into marker script bindings; scripts are not executed
  during import.
- Runtime world assembly continues to copy world metadata wholesale, including script bindings.
- Metadata validation now reports deterministic script binding diagnostics for empty script ids,
  empty table names, path traversal/absolute path escapes, and script bindings on marker types that
  do not yet have runtime invocation support.
- Extended display-free metadata validation and glTF importer regression tests for script binding
  extraction, validation, and RuntimeWorld copy behavior.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(world_metadata_validation|gltf_importer_regression|lua_runtime)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Phase 10A is complete as of 2026-04-30:

- Added optional `STELLAR_ENABLE_LUA_SCRIPTING` support with a new `stellar_scripting` target
  and vendored Lua 5.4.8 under `thirdparty/lua`.
- Added RAII Lua runtime ownership, expected-style script errors, protected script loading/calls,
  restricted standard library setup, `stellar.emit_event(name, fields)`, deterministic output
  draining, bytecode rejection, and instruction budget enforcement.
- Preserved the native server/world dependency boundary: Lua is linked only through
  `stellar_scripting`, not through `stellar_world` or `stellar_server_core`.
- Added display-free `lua_runtime` CTest coverage for construction, script load/call behavior,
  missing function no-op policy, syntax/runtime errors, event ordering, sandbox restrictions,
  primitive field validation, bytecode rejection, and budget failure behavior.

Validation run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R '^lua_runtime$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Phase 9F is complete as of 2026-04-30:

- Added a display-free playable-world smoke test that generates a small glTF fixture and validates the
  import -> collision validation -> metadata validation -> runtime world -> authoritative session path.
- The fixture covers visible render geometry, collision-only floor/walls, player spawn metadata,
  trigger metadata, sprite metadata, and collision-only render filtering.
- The scripted authoritative movement path checks initial spawn snapshots, wall collision, trigger
  events, deterministic repeat output, and no graphics/window context requirement.

Phase 9E is complete as of 2026-04-30:

- Added backend-neutral player presentation helpers that extract player pose from authoritative
  `WorldSnapshot` data without mutating server state.
- Added deterministic camera-follow frame generation with documented sanitization for non-finite
  position/offset and near/far clip-plane inputs.
- Deferred renderer camera override and player billboard/material binding until a later presentation
  slice.

Phase 9D is complete as of 2026-04-30:

- Added `stellar::client::LocalLoopbackRuntime`, an in-process authoritative runtime bridge over
  `stellar::server::WorldSession` for local/single-player client presentation.
- Client input is mapped to server commands, fixed-step time is accumulated deterministically, ticks
  are bounded by `max_ticks_per_frame`, and snapshots/trigger events are returned for presentation.
- `Application::run` integration remains deferred; the implemented bridge is display-free and tested
  independently.

Phase 9C is complete as of 2026-04-30:

- Added `stellar_client_runtime` and `MovementInputMapper` APIs for converting plain input state or
  `platform::Input` keyboard state into `server::MovementCommand` intent.
- The world-axis convention is documented and tested: forward `-Z`, backward `+Z`, left `-X`, right
  `+X`, with optional deterministic diagonal normalization and jump intent pass-through.
- The mapper does not apply movement locally and introduces no rendering or networking dependency.

Phase 9B is complete as of 2026-04-30:

- Added standalone backend-neutral `validate_world_metadata` diagnostics for player spawns, entity
  spawns, triggers, sprite markers, portal markers, non-finite transforms, duplicate names, empty
  names/extents, large trigger extents, and unparsed `extras_json`.
- Reports are deterministic and available for both `WorldMetadataAsset` and `RuntimeWorld` without
  making runtime world construction fail by default.

Phase 9A is complete as of 2026-04-30:

- Added `stellar::server::WorldSession`, a backend-neutral authoritative session around one local
  player slot with stable command and snapshot data shapes for future ECS/network expansion.
- The session owns movement state, fixed tick sequencing, and `MovementTriggerTracker` state while
  referencing caller-owned `RuntimeWorld` data with documented lifetime requirements.
- Missing commands produce zero movement, unknown player commands are ignored deterministically, ticks
  use the existing authoritative movement simulation, and pure snapshots do not replay trigger events.

Phase 8F is complete as of 2026-04-30:

- Added a display-free collision performance regression harness with large synthetic floor,
  corridor/wall, distant geometry, degenerate, empty, and single-triangle scenes.
- Tests assert BVH/candidate pruning through diagnostics using generous ratios instead of timing or
  exact node-count assumptions.
- Repeated raycast and character movement results are covered for deterministic output stability.

Phase 8E is complete as of 2026-04-30:

- Added backend-neutral authoritative movement-trigger integration in `stellar_server_core`.
- `MovementTriggerTracker` builds trigger volumes from `RuntimeWorld`, owns overlap state outside the
  runtime world, and emits deterministic enter/stay/exit movement trigger events.
- Added an explicit helper that simulates one authoritative movement tick and updates triggers from
  the server-owned final character position and configured radius.

Phase 8D is complete as of 2026-04-30:

- Added standalone backend-neutral `validate_level_collision` authoring diagnostics without changing
  `LevelCollisionAsset` layout.
- Validation reports deterministic warning/error findings for non-finite data, bad normals,
  zero-area triangles, bounds mismatches, empty assets/meshes, large bounds, duplicate mesh names,
  and missing walkable upward-facing surfaces.

Phase 8C is complete as of 2026-04-30:

- Added `stellar_server_core` with a small server-authoritative movement simulation seam over
  `RuntimeWorld` and `CharacterController`.
- Movement commands are intent-only and sanitized; authoritative state remains plain data with no
  renderer, audio, ECS, networking, or platform dependency.
- Tests cover spawn initialization, no-collision movement, floor/wall collision, optional collision,
  input/config sanitization, terminal fall clamping, and fixed-tick repeatability.

Phase 8B is complete as of 2026-04-30:

- Activated `CharacterControllerConfig::height` as total vertical capsule height including
  hemispherical ends, with position remaining the capsule center and effective height clamped to at
  least `2 * radius`.
- Character movement now uses conservative capsule overlap/recovery, sampled/refined capsule sweeps,
  lower-endpoint ground snap, and ceiling-checked step-up over Phase 8A BVH candidate queries.
- Sphere-like behavior is preserved when height collapses to two radii.
- Extended display-free character controller tests for capsule floor, wall/body height, ceiling,
  ground snap, step-up under low ceilings, too-tall tunnels, degenerate dimensions, and corner slide.

Phase 8A is complete as of 2026-04-30:

- Added deterministic BVH-backed static triangle candidate queries through
  `CollisionWorld::query_triangles` using stable mesh/triangle indices.
- Character controller recovery and sweep/slide now query pruned triangle candidates instead of
  brute-force scanning all static triangles while preserving deterministic behavior.
- Diagnostics now expose the most recent candidate count so tests can prove pruning without exposing
  BVH internals.

Phase 7E is complete as of 2026-04-30:

- Added a deterministic internal static BVH over imported collision triangle AABBs in
  `stellar::physics::CollisionWorld`.
- Raycast and low-level sphere movement now traverse BVH candidates before narrowphase triangle
  tests while preserving nearest-hit and sweep/slide observable behavior.
- Added `CollisionWorldStats` diagnostics for mesh count, triangle count, broadphase node count, and
  the most recent query's narrowphase triangle-test count.
- Extended display-free `collision_world` tests for empty/single/many triangles, deterministic equal
  hit ordering, movement stop/slide preservation, degenerate triangles, diagnostics, and pruning.

## Previous status

Phase 7D is complete as of 2026-04-30:

- Added backend-neutral `stellar::world::TriggerVolume`, `TriggerOverlap`, and `TriggerSystem`
  runtime trigger support.
- Trigger metadata now converts into deterministic axis-aligned trigger volumes through
  `build_trigger_volumes` overloads for `WorldMetadataAsset` and `RuntimeWorld`.
- Runtime overlap supports sphere-vs-AABB queries with an inclusive touch policy and deterministic
  enter/stay/exit transition state, without ECS, scripts, networking, rendering, or physics response.
- Added display-free `trigger_system` tests covering metadata conversion, ignored marker types,
  outside/touching/inside overlap behavior, enter/stay/exit/re-enter transitions, deterministic
  ordering, empty systems, and RuntimeWorld metadata hooks.

Phase 7C is complete as of 2026-04-30:

- Added a backend-neutral `stellar::world::RuntimeWorld` assembly layer over imported
  `SceneAsset` data.
- Runtime world assembly keeps an explicit source scene pointer, creates `CollisionWorld` only for
  non-empty `SceneAsset::level_collision`, copies world metadata for stable pure marker lookups,
  and emits diagnostics for collision, markers, sprite markers, player spawn presence, and warnings.
- Added helper lookups for player spawns, typed markers, entity spawn archetypes, sprite markers,
  and trigger markers without adding texture/material binding or gameplay behavior.
- Client asset validation now records display-free runtime world diagnostics after glTF import.
- Added display-free `runtime_world` tests covering empty scenes, collision creation/absence,
  empty collision assets, marker lookups, diagnostics, and metadata lookup lifetime behavior.

Phase 7B is complete as of 2026-04-30:

- Added a small `stellar::physics::CharacterController` layer over the existing static
  `CollisionWorld` API.
- Preserved existing `CollisionWorld` raycast, ground probe, and low-level `move_sphere` behavior
  while exposing immutable collision asset access for higher-level static queries.
- Character movement now covers deterministic start-overlap recovery, conservative sphere
  sweep/slide, walkable slope classification, ground snap, and simple step-up fallback.
- Added display-free `character_controller` tests covering empty movement, grounding, shallow
  recovery, wall stop/slide, corner stop, edge/vertex contacts, slope limits, snap, step-up, and
  finite degenerate/zero-displacement output.

Phase 7A is complete as of 2026-04-30:

- Status documentation now treats Phase 6A/6B/6C/6D as implemented first passes.
- glTF collision-only nodes are still extracted into `SceneAsset::level_collision`.
- Collision-only nodes are excluded from normal imported render scene membership by clearing
  mesh instances during scene import after collision extraction.
- Collision-only render filtering applies to nodes named `COL_*`, nodes named `Collision_*`,
  and mesh descendants of an exact node named `Collision`.
- Metadata markers remain independent: `SPAWN_*`, `TRIGGER_*`, and `SPRITE_*` do not become
  collision meshes, and collision nodes do not become world metadata markers.

## Strong recommendation

Implement only one phase per pull request/branch slice. Keep every phase display-free by default, with optional graphics context tests remaining opt-in.

## Shared implementation rules

- Prefer small public APIs with Doxygen `@brief` comments.
- Keep collision and metadata backend-neutral.
- Do not add a third-party physics engine yet.
- Keep default CTest display-free.
- Preserve OpenGL/Vulkan parity assumptions where rendering is touched.
- Do not expand glTF extension support unless directly required.
- Add completion notes to every plan file after implementation.
- Update `docs/ImplementationStatus.md` after each phase.

## Shared validation baseline

Run this at the end of each phase unless the phase plan says otherwise:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

When graphics context code is touched, keep context tests opt-in and run the relevant optional path separately.
