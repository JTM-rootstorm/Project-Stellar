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
