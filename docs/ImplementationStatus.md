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
