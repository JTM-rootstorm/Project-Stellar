# Project Stellar — Next Collision Plan After Lua Scripting

**Branch inspected:** `collision-movement`  
**Repository:** `JTM-rootstorm/Project-Stellar`  
**Generated:** 2026-04-30  
**Prepared for:** AI implementation agents such as Codex, Kilo, or other code-writing agents  
**Output type:** planning handoff only; do not commit unless the user explicitly asks

---

## 0. AI Agent Intake Summary

Lua scripting is now implemented on top of the existing authoritative movement/session seam. Do **not** restart the earlier collision, world metadata, trigger, playable loop, or Lua phases.

The next collision work should focus on making collision usable by authored gameplay and scripts:

1. Make trigger overlap match the capsule-based character controller instead of the current center-sphere approximation.
2. Add an authoritative runtime collision-state overlay so named static collision meshes can be enabled/disabled without rebuilding imported assets.
3. Route authoritative movement, raycasts, ground probes, and character queries through that collision state.
4. Apply script outputs through native validation to collision state, enabling the first real scripted collision behavior such as doors, gates, traps, and one-way blockers.
5. Defer full dynamic rigid bodies, third-party physics, navmesh, and broad ECS integration until entity ownership is concrete.

The highest-value outcome is a display-free scripted collision smoke test where:

- glTF imports visible geometry, collision-only floor/walls, a named collision blocker, player spawn, and trigger metadata.
- A trigger script emits a validated request such as `set_collision_enabled`.
- Native server code validates and applies the request.
- The next authoritative movement tick can pass through the disabled blocker.
- The repeat run produces identical snapshots, script events, collision command results, and trigger state.

---

## 1. Current Branch Evidence Snapshot

Treat these as already implemented first passes:

### Collision and movement

- Static glTF collision extraction exists through `src/import/gltf/CollisionImport.cpp`.
- `SceneAsset` carries backend-neutral `LevelCollisionAsset`.
- `CollisionWorld` exposes:
  - `raycast`
  - `probe_ground`
  - `move_sphere`
  - `query_triangles`
  - immutable `asset()`
  - `stats()`
- `CollisionWorld` has deterministic BVH/candidate diagnostics through `CollisionWorldStats`.
- `CharacterController` is capsule-aware:
  - `radius`
  - total capsule `height`
  - `skin_width`
  - `max_slope_degrees`
  - `step_height`
  - `ground_snap_distance`
  - fixed max slide iterations
- Character movement uses recovery, conservative sampled/refined capsule sweep, slide, ground snap, and step-up.
- Server authoritative movement exists through `stellar::server::simulate_movement_tick`.
- Movement command `jump` is currently accepted as intent but is not applied by the deterministic movement seam.

### Runtime world and triggers

- `RuntimeWorld` is built from imported `SceneAsset`.
- `RuntimeWorld` owns optional `CollisionWorld`, copied world metadata, and diagnostics.
- `TriggerSystem` builds runtime AABB triggers from `WorldMarkerType::kTrigger`.
- `MovementTriggerTracker` converts authoritative movement into enter/stay/exit trigger events.
- Important limitation: `MovementTriggerTracker` currently uses the character radius as a sphere centered at the authoritative movement position. Capsule-height trigger checks are explicitly deferred.

### Authoritative session and local loopback

- `WorldSession` owns one local authoritative player slot, movement state, trigger tracking, tick index, and snapshot output.
- `LocalLoopbackRuntime` maps client input into server movement commands and advances fixed authoritative ticks without making the client authoritative.
- Current runtime integration remains display-free by default.

### Lua scripting

- `stellar_scripting` is opt-in and wraps Lua behind engine-owned APIs.
- `ScriptedWorldSession` wraps `WorldSession`, advances native authoritative movement first, and then invokes scripts from authoritative trigger events.
- `TriggerScriptSystem` invokes trigger hooks in deterministic order.
- Scripts emit primitive `ScriptOutputEvent`s. They do **not** directly mutate engine state.

### Documentation status

- `docs/ImplementationStatus.md` reports Phase 10 complete and full CTest passing.
- `docs/Design.md` preserves the architecture:
  - server authority owns gameplay, collision, movement, physics decisions, and validation;
  - client remains presentation;
  - collision and metadata stay backend-neutral;
  - default tests remain display-free;
  - full physics engine, dynamic rigid bodies, and navmesh remain deferred.

---

## 2. Recommendation

The next collision slice should be **Phase 11: Authoritative Runtime Collision State and Scripted Collision Behavior**.

Do not spend the next branch slice on:

- replacing the custom static collision system with a third-party physics engine;
- dynamic rigid bodies;
- broad ECS;
- remote networking;
- client-side prediction;
- navmesh/pathfinding;
- renderer debug views;
- client-side collision authority;
- Lua directly mutating C++ objects.

Instead, build the minimum authoritative path that lets collision participate in gameplay:

```text
current:
  imported static collision -> immutable CollisionWorld -> character movement
  trigger events -> Lua script outputs -> buffered events only

next:
  imported static collision -> immutable asset + authoritative runtime state overlay
  filtered collision queries -> authoritative character movement
  trigger scripts -> validated native collision commands -> runtime collision state
```

This keeps server authority intact while unlocking real gameplay interactions.

---

## 3. Phase Order

```text
Safety/correctness lane:
  Phase 11A -> Phase 11F

Runtime collision-state lane:
  Phase 11B -> Phase 11C -> Phase 11D -> Phase 11F

Future object-collision lane:
  Phase 11E, after Phase 11B; may wait until after 11D if scripted doors are priority
```

Recommended order:

1. **Phase 11A — Capsule-Aware Trigger Overlap**
2. **Phase 11B — Runtime Collision State Overlay**
3. **Phase 11C — Filtered Collision Queries and Movement Integration**
4. **Phase 11D — Validated Script Collision Commands**
5. **Phase 11E — Kinematic Object Collider Registry** *(optional next slice; defer if ECS ownership is still unsettled)*
6. **Phase 11F — Scripted Collision Authoring Smoke and Documentation**

---

## 4. Parallelization Guidance

Safe parallel work:

- **Phase 11A and Phase 11B can run in parallel** if Phase 11A stays in trigger overlap code and Phase 11B stays in collision mesh state/lookup code.
- **Phase 11E can prototype after Phase 11B** if it does not alter `WorldSession` or `ScriptedWorldSession`.
- Documentation drafting for Phase 11F can start at any time, but the smoke test should wait for Phases 11A through 11D.

Do not run in parallel:

- **Phase 11B and Phase 11C** if both modify collision query APIs.
- **Phase 11C and Phase 11D** if both modify `WorldSession` or `ScriptedWorldSession`.
- **Phase 11D and Phase 11F smoke assertions**, because the smoke test depends on the final command-application semantics.

---

## 5. Global Implementation Rules

For every phase:

- Keep server authority intact.
- Keep Lua as a producer of validated requests/events, not a direct mutator of engine state.
- Keep collision/backend-neutral code independent of graphics, audio, platform windows, OpenGL, and Vulkan.
- Keep default CTest display-free.
- Use small explicit C++ APIs with Doxygen `@brief` comments.
- Preserve deterministic ordering.
- Preserve existing tests unless the plan explicitly says behavior changes.
- Add or update completion notes in the phase plan if the plan is copied into the repo.
- Update `docs/ImplementationStatus.md` after each implemented phase.
- Update `docs/Design.md` only when broad architecture or authoring conventions change.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.
- Do not add a third-party physics engine.
- Do not add dynamic rigid bodies in Phase 11.

Baseline validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

OpenGL/Vulkan context tests remain opt-in only.

---

# Phase 11A — Capsule-Aware Trigger Overlap

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: collision correctness / authoritative trigger hardening
- Depends on: Phase 8B, Phase 8E, Phase 9A, Phase 10C
- Can run in parallel with: Phase 11B if no shared files are edited
- Must finish before: final scripted collision smoke
- Do not commit unless explicitly instructed by the user.

## Objective

Make runtime trigger overlap match the character controller’s capsule shape instead of the current center-sphere approximation.

Current `MovementTriggerTracker` uses a sphere centered at the authoritative movement position with the character radius. That was acceptable before scripts, but Lua trigger hooks now make trigger events gameplay-visible. Capsule-height mismatch can cause scripts to miss tall/low triggers or fire inconsistently relative to physical movement.

## Required Reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/world/TriggerSystem.hpp`
- `src/world/TriggerSystem.cpp`
- `include/stellar/server/MovementTriggerIntegration.hpp`
- `src/server/MovementTriggerIntegration.cpp`
- `include/stellar/physics/CharacterController.hpp`
- `include/stellar/server/MovementSimulation.hpp`
- `tests/world/TriggerSystem.cpp`
- `tests/server/MovementTriggerIntegration.cpp`
- `tests/scripting/TriggerScriptSystem.cpp`

## Scope

Add capsule-vs-AABB trigger overlap while keeping existing sphere overlap behavior available.

Preferred API additions:

```cpp
namespace stellar::world {

/** @brief Vertical capsule used for backend-neutral trigger overlap queries. */
struct TriggerCapsule {
    std::array<float, 3> center{};
    std::array<float, 3> up{0.0F, 1.0F, 0.0F};
    float radius = 0.0F;
    float height = 0.0F;
};

/** @brief Update one capsule against all triggers and return enter/stay/exit transitions. */
[[nodiscard]] std::vector<TriggerOverlap> update_capsule(const TriggerCapsule& capsule) noexcept;

}
```

Acceptable alternative: keep `TriggerCapsule` internal if `MovementTriggerTracker` gets a clean public API that accepts character config.

Update server trigger integration to call capsule overlap using:

- authoritative movement position;
- sanitized `CharacterControllerConfig::radius`;
- sanitized `CharacterControllerConfig::height`;
- up vector `{0, 1, 0}` for now.

## Implementation Steps

1. Add deterministic capsule-vs-AABB helper.
   - Treat capsule as vertical along `up`.
   - Clamp height to at least `2 * radius`.
   - Normalize nonzero `up`; fallback to world Y.
   - Use inclusive touch policy matching existing sphere-vs-AABB tests.
   - Preserve stable trigger iteration order.

2. Preserve existing sphere API.
   - Keep `update_sphere` tests passing.
   - `update_capsule` should reduce to sphere-like behavior when height collapses to two radii.

3. Update `MovementTriggerTracker`.
   - Pass radius and height, or pass `MovementSimulationConfig`.
   - Prefer an API that prevents call sites from accidentally using stale radius-only behavior.

4. Update `simulate_movement_tick_and_update_triggers`.
   - Trigger state must use the same sanitized character shape as movement.
   - Do not duplicate divergent sanitization if avoidable.

5. Keep scripting unchanged.
   - `TriggerScriptSystem` continues consuming authoritative `MovementTriggerEvent`s.
   - Script hook behavior changes only because trigger detection is more accurate.

## Non-Goals

Do not implement:

- dynamic triggers;
- oriented trigger boxes;
- trigger scripting changes;
- collision response against triggers;
- ECS;
- renderer debug drawing;
- client authority.

## Required Tests

Update or add display-free tests.

Trigger system tests:

1. `capsule_overlap_matches_sphere_when_height_is_two_radii`
2. `capsule_top_enters_high_trigger_where_center_sphere_does_not`
3. `capsule_bottom_enters_low_trigger_where_center_sphere_does_not`
4. `capsule_touch_policy_is_inclusive`
5. `capsule_non_finite_inputs_are_sanitized_or_noop_deterministically`
6. `capsule_trigger_order_is_deterministic`

Movement trigger integration tests:

7. `movement_trigger_tracker_uses_character_capsule_height`
8. `movement_trigger_tracker_reset_preserves_capsule_behavior`
9. `script_trigger_enter_uses_capsule_accurate_authoritative_event`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_trigger_system_test \
  stellar_movement_trigger_integration_test \
  stellar_trigger_script_system_test \
  -j$(nproc)
ctest --test-dir build -R '^(trigger_system|movement_trigger_integration|trigger_script)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Authoritative trigger events use capsule geometry consistent with character movement.
- Existing sphere trigger API remains available and tested.
- Trigger ordering remains deterministic.
- Lua trigger scripts require no API change.
- Default CTest remains display-free.

---

# Phase 11B — Runtime Collision State Overlay

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` if authoring conventions need policy decisions
- Type: authoritative runtime collision state
- Depends on: existing `RuntimeWorld`, `CollisionWorld`, and collision validation
- Can run in parallel with: Phase 11A
- Must finish before: Phase 11C and Phase 11D
- Do not commit unless explicitly instructed by the user.

## Objective

Add an authoritative runtime state overlay for static collision meshes so gameplay can enable/disable named collision without mutating imported assets or rebuilding `CollisionWorld`.

This is the foundation for scripted doors, gates, traps, and one-way blockers.

## Required Reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/world/RuntimeWorld.hpp`
- `src/world/RuntimeWorld.cpp`
- `include/stellar/world/CollisionValidation.hpp`
- `src/world/CollisionValidation.cpp`
- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `tests/world/RuntimeWorld.cpp`
- `tests/world/CollisionValidation.cpp`
- `tests/physics/CollisionWorld.cpp`

## Scope

Add a small backend-neutral runtime state object for mesh-level static collision toggles.

Preferred files:

```text
include/stellar/world/RuntimeCollisionState.hpp
src/world/RuntimeCollisionState.cpp
tests/world/RuntimeCollisionState.cpp
```

Preferred public API:

```cpp
namespace stellar::world {

/** @brief Stable runtime reference to one imported static collision mesh. */
struct RuntimeCollisionMeshRef {
    std::size_t mesh_index = 0;
    std::string name;
};

/** @brief Result of applying an authoritative collision-state change. */
struct RuntimeCollisionStateResult {
    bool applied = false;
    std::string code;
    std::string message;
};

/** @brief Authoritative enable/disable overlay for immutable imported static collision meshes. */
class RuntimeCollisionState {
public:
    /** @brief Build enabled state and name lookup from a runtime world's collision asset. */
    static RuntimeCollisionState from_world(const RuntimeWorld& world);

    /** @brief Return true when a collision mesh index should participate in collision queries. */
    [[nodiscard]] bool is_mesh_enabled(std::size_t mesh_index) const noexcept;

    /** @brief Enable or disable all collision meshes with the supplied authored name. */
    [[nodiscard]] RuntimeCollisionStateResult set_mesh_enabled(std::string_view name,
                                                               bool enabled) noexcept;

    /** @brief Return authored collision mesh names and indices. */
    [[nodiscard]] std::vector<RuntimeCollisionMeshRef> meshes() const;

    /** @brief Return deterministic diagnostics for duplicate or empty collision mesh names. */
    [[nodiscard]] std::vector<std::string> diagnostics() const;

private:
    std::vector<bool> enabled_meshes_;
    std::vector<RuntimeCollisionMeshRef> mesh_refs_;
};

}
```

Naming policy:

- Mesh names come from `CollisionMesh::name`.
- Empty names are allowed but cannot be targeted by script/native commands.
- Duplicate names are allowed but produce a warning and are toggled together by name unless Phase 11B chooses to make duplicates an error.
- Prefer warning over error to avoid breaking imported content prematurely.

## Implementation Steps

1. Add runtime state object.
   - Initialize all meshes enabled.
   - Handle worlds with no collision.
   - Keep data plain and deterministic.
   - Do not mutate `CollisionWorld` or `LevelCollisionAsset`.

2. Add mesh name lookup.
   - Preserve mesh index ordering.
   - Return deterministic diagnostics for empty names and duplicates.
   - Add helper to find all mesh indices for a name.

3. Add validation integration.
   - Extend `validate_level_collision` with optional warnings:
     - empty collision mesh names;
     - duplicate collision mesh names;
     - suspicious names that cannot be targeted safely.
   - Do not make these fatal by default.

4. Add `RuntimeWorld` convenience only if clean.
   - Option A: `WorldSession` owns `RuntimeCollisionState`.
   - Option B: `RuntimeWorld` diagnostics expose collision names.
   - Avoid making `RuntimeWorld` mutable if possible.

5. Keep public API documented.
   - Doxygen `@brief` for all new public types/functions.

## Non-Goals

Do not implement:

- filtered collision queries yet;
- script output application;
- rebuilding BVHs per toggle;
- per-triangle enable flags;
- dynamic rigid bodies;
- collider transforms;
- broad ECS.

## Required Tests

Create `tests/world/RuntimeCollisionState.cpp`.

Required coverage:

1. `empty_world_has_empty_collision_state`
2. `all_collision_meshes_enabled_by_default`
3. `set_mesh_enabled_disables_named_mesh`
4. `set_mesh_enabled_enables_named_mesh`
5. `unknown_mesh_name_reports_deterministic_not_found`
6. `duplicate_mesh_names_toggle_together_or_report_policy`
7. `empty_mesh_names_warn_and_are_not_targetable`
8. `mesh_iteration_order_is_deterministic`
9. `runtime_world_lifetime_requirements_remain_unchanged`

Extend collision validation tests for:

10. `duplicate_collision_mesh_names_warn`
11. `empty_collision_mesh_name_warn`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target \
  stellar_runtime_collision_state_test \
  stellar_collision_validation_test \
  stellar_runtime_world_test \
  -j$(nproc)
ctest --test-dir build -R '^(runtime_collision_state|collision_validation|runtime_world)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Runtime collision mesh enable/disable state exists.
- Imported collision assets remain immutable.
- Empty/duplicate collision names produce deterministic diagnostics.
- No physics engine or dynamic body system is added.
- No rendering/audio/platform dependency is introduced.
- Default CTest remains display-free.

---

# Phase 11C — Filtered Collision Queries and Movement Integration

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: collision query API and authoritative movement integration
- Depends on: Phase 11B
- Can run in parallel with: none if query APIs are being modified
- Must finish before: Phase 11D scripted collision commands
- Do not commit unless explicitly instructed by the user.

## Objective

Make collision runtime state actually affect authoritative movement and queries.

After Phase 11B, collision meshes can be enabled/disabled in state, but existing `CollisionWorld`, `CharacterController`, and movement functions still query all static triangles. Phase 11C adds a filtered query path and routes authoritative movement through it.

## Required Reading

- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `include/stellar/physics/CharacterController.hpp`
- `src/physics/CharacterController.cpp`
- `include/stellar/server/MovementSimulation.hpp`
- `src/server/MovementSimulation.cpp`
- `include/stellar/server/WorldSession.hpp`
- `src/server/WorldSession.cpp`
- `include/stellar/world/RuntimeCollisionState.hpp`
- `tests/physics/CollisionWorld.cpp`
- `tests/physics/CharacterController.cpp`
- `tests/server/MovementSimulation.cpp`
- `tests/server/WorldSession.cpp`

## Scope

Add collision filters that can exclude disabled meshes without rebuilding the BVH.

Preferred public API direction:

```cpp
namespace stellar::physics {

/** @brief Optional static collision mesh filter used by query and movement APIs. */
struct CollisionQueryFilter {
    /** @brief Optional enabled mask indexed by collision mesh index. Null means all enabled. */
    const std::vector<bool>* enabled_meshes = nullptr;
};

/** @brief Return true when the supplied mesh index passes this filter. */
[[nodiscard]] bool collision_mesh_passes_filter(const CollisionQueryFilter& filter,
                                                std::size_t mesh_index) noexcept;

}
```

Then add overloads:

```cpp
RaycastHit raycast(origin, delta, CollisionQueryFilter filter) const noexcept;
GroundProbeHit probe_ground(origin, max_distance, min_floor_normal_y, CollisionQueryFilter filter) const noexcept;
MoveResult move_sphere(position, displacement, radius, max_iterations, CollisionQueryFilter filter) const noexcept;
std::vector<CollisionTriangleCandidate> query_triangles(bounds, CollisionQueryFilter filter) const;
```

For `CharacterController`, prefer either:

```cpp
CharacterMoveResult move(input, config, CollisionQueryFilter filter) const noexcept;
```

or a construction-time query context if cleaner:

```cpp
CharacterController controller(world, filter);
```

Recommended: use method-level filter overload to preserve existing construction behavior.

Server integration:

- `WorldSession` should own `RuntimeCollisionState`.
- `simulate_movement_tick` can remain pure over `RuntimeWorld` if a new overload accepts state.
- Prefer adding `simulate_movement_tick(..., const RuntimeCollisionState* collision_state)` and updating `WorldSession` to pass its owned state.
- Preserve old overload behavior as “all collision enabled.”

## Implementation Steps

1. Add filter representation.
   - No ownership of runtime world.
   - Null/empty filter means all meshes enabled.
   - Out-of-range masks should fail closed or open by explicit policy. Recommended: fail closed for safety and add tests.

2. Update BVH candidate query.
   - Traverse same BVH.
   - Filter candidates by mesh index before returning them.
   - Preserve deterministic mesh/triangle order.
   - Update stats to report filtered candidate count.

3. Update raycast/probe/move_sphere.
   - All must honor the filter.
   - Existing overloads call filtered version with no filter.

4. Update `CharacterController`.
   - Recovery, sweep/slide, ground snap, and step-up must all honor filters.
   - Keep old behavior unchanged when no filter is provided.

5. Update server movement.
   - `WorldSession` owns a runtime collision state built from the world.
   - Each tick passes collision state to movement.
   - `snapshot()` does not mutate collision state.
   - `reset()` resets collision state from the new world.

6. Expose controlled mutation entry points.
   - Add `WorldSession::set_collision_mesh_enabled(name, enabled)` or an internal helper used by Phase 11D.
   - If public, return deterministic result data and document server authority.

## Non-Goals

Do not implement:

- script command parsing;
- per-triangle toggles;
- moving colliders;
- dynamic body simulation;
- broad ECS;
- renderer debug views.

## Required Tests

CollisionWorld tests:

1. `query_triangles_filter_excludes_disabled_mesh`
2. `raycast_ignores_disabled_mesh`
3. `probe_ground_ignores_disabled_floor`
4. `move_sphere_ignores_disabled_wall`
5. `filter_preserves_deterministic_order`
6. `filter_out_of_range_policy_is_tested`

CharacterController tests:

7. `disabled_wall_no_longer_blocks_character`
8. `disabled_floor_no_longer_grounds_character`
9. `step_up_ignores_disabled_step`
10. `all_enabled_filter_matches_existing_behavior`

Server/session tests:

11. `world_session_collision_state_resets_with_world`
12. `world_session_disabled_mesh_affects_next_tick`
13. `snapshot_does_not_apply_or_replay_collision_mutation`
14. `same_collision_state_and_inputs_produce_same_snapshots`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target \
  stellar_collision_world_test \
  stellar_character_controller_test \
  stellar_server_movement_simulation_test \
  stellar_server_world_session_test \
  -j$(nproc)
ctest --test-dir build -R '^(collision_world|character_controller|server_movement_simulation|server_world_session)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Collision mesh enable/disable state affects authoritative movement and queries.
- Existing all-enabled behavior is preserved.
- Collision filtering is deterministic.
- `WorldSession` owns collision state as authoritative runtime state.
- Runtime world and imported asset data remain immutable.
- Default CTest remains display-free.

---

# Phase 11D — Validated Script Collision Commands

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` because this crosses scripting, server authority, and collision state
- Type: server-authoritative script output application
- Depends on: Phase 10D, Phase 11B, Phase 11C
- Must finish before: final scripted collision smoke
- Do not commit unless explicitly instructed by the user.

## Objective

Allow Lua scripts to request collision changes through buffered events, while native server code validates and applies those requests.

Lua must not directly mutate collision state. Scripts should emit primitive events; native server code interprets a narrow command vocabulary and applies approved changes after authoritative native movement and script callbacks.

## Required Reading

- `include/stellar/scripting/ScriptValue.hpp`
- `include/stellar/scripting/ScriptedWorldSession.hpp`
- `src/scripting/ScriptedWorldSession.cpp`
- `include/stellar/scripting/TriggerScriptSystem.hpp`
- `src/scripting/TriggerScriptSystem.cpp`
- `include/stellar/server/WorldSession.hpp`
- `src/server/WorldSession.cpp`
- `include/stellar/world/RuntimeCollisionState.hpp`
- `tests/scripting/ScriptedWorldSession.cpp`
- `tests/integration/ScriptedPlayableWorldSmoke.cpp`

## Scope

Add a native command processor for script-emitted collision requests.

Preferred new files:

```text
include/stellar/scripting/ScriptCommandProcessor.hpp
src/scripting/ScriptCommandProcessor.cpp
tests/scripting/ScriptCommandProcessor.cpp
```

Preferred event vocabulary:

```lua
stellar.emit_event("collision.set_mesh_enabled", {
    mesh = "DoorBlocker",
    enabled = false
})
```

Alternative event name:

```lua
stellar.emit_event("set_collision_enabled", {
    target = "DoorBlocker",
    enabled = false
})
```

Recommended: use the namespaced form `collision.set_mesh_enabled`.

Preferred C++ command result:

```cpp
namespace stellar::scripting {

/** @brief Result of applying one script-emitted native command. */
struct ScriptCommandResult {
    std::string event_name;
    bool applied = false;
    std::string code;
    std::string message;
};

/** @brief Summary of native application of script output events. */
struct ScriptCommandApplication {
    std::vector<ScriptCommandResult> results;
};

/** @brief Validate and apply supported script output events to an authoritative world session. */
[[nodiscard]] ScriptCommandApplication apply_script_commands(
    stellar::server::WorldSession& session,
    std::span<const ScriptOutputEvent> events) noexcept;

}
```

Update `ScriptedWorldFrame` to include command application results:

```cpp
struct ScriptedWorldFrame {
    stellar::server::WorldSnapshot snapshot;
    std::vector<ScriptOutputEvent> script_events;
    std::vector<ScriptError> script_errors;
    std::vector<ScriptCommandResult> command_results;
};
```

Ordering policy:

- Native movement tick runs first.
- Trigger scripts run from that tick’s authoritative trigger events.
- Script output commands are validated and applied after script callbacks.
- Collision state changes affect the **next** authoritative movement tick, not the tick that already completed.
- Command application order follows script output event order.
- Unknown events are ignored or reported by policy. Recommended: report `unsupported_event` but do not treat as script error.

Validation policy:

- `mesh` must be a non-empty string.
- `enabled` must be boolean.
- Unknown mesh name returns deterministic `not_found`.
- Duplicate names follow Phase 11B policy.
- Missing/invalid fields return deterministic `invalid_field`.
- Command application must not throw and must not crash the session.

## Implementation Steps

1. Add command processor.
   - Convert `ScriptOutputEvent` fields by primitive type.
   - Recognize only `collision.set_mesh_enabled` initially.
   - Return deterministic result records.

2. Add `WorldSession` mutation API if not already public.
   - `set_collision_mesh_enabled(name, enabled)`
   - Return deterministic result data.
   - Do not expose imported asset mutation.

3. Update `ScriptedWorldSession::tick`.
   - Run native tick.
   - Run trigger scripts.
   - Apply script commands to the owned session.
   - Return command results in frame.
   - Preserve `latest_snapshot()` no-replay behavior.

4. Keep unsupported script outputs available.
   - Do not delete script output events after applying commands.
   - Higher-level gameplay may still inspect events.
   - Command results merely record what native server logic applied.

5. Add diagnostics.
   - Invalid command fields become command results, not Lua runtime errors.
   - Lua runtime errors remain `ScriptError`.

## Non-Goals

Do not implement:

- arbitrary script access to collision world;
- script-exposed C++ pointers;
- file-backed command schemas;
- dynamic rigid bodies;
- animation/render door changes;
- entity spawning;
- networking replication;
- rollback/prediction.

## Required Tests

Script command processor tests:

1. `collision_set_mesh_enabled_valid_event_applies`
2. `missing_mesh_field_reports_invalid_field`
3. `non_boolean_enabled_reports_invalid_field`
4. `unknown_collision_mesh_reports_not_found`
5. `unsupported_event_reports_or_ignores_by_policy`
6. `multiple_events_apply_in_deterministic_order`

Scripted session tests:

7. `scripted_trigger_can_disable_named_collision_mesh`
8. `collision_disable_affects_next_tick_not_current_tick`
9. `scripted_invalid_collision_command_does_not_crash`
10. `latest_snapshot_does_not_reapply_commands`
11. `repeat_run_produces_same_script_events_and_command_results`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_script_command_processor_test \
  stellar_scripted_world_session_test \
  stellar_server_world_session_test \
  -j$(nproc)
ctest --test-dir build -R '^(script_command_processor|scripted_world_session|server_world_session)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Scripts can request collision mesh enable/disable through buffered events.
- Native server code validates and applies only supported commands.
- Collision changes affect subsequent authoritative ticks.
- Invalid commands produce deterministic command results, not crashes.
- Lua remains isolated to `stellar_scripting`.
- `stellar_server_core` does not link Lua directly.
- Default CTest remains display-free.

---

# Phase 11E — Kinematic Object Collider Registry

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Gameplay review: `@kojima` only through `@director` if object semantics need gameplay decisions
- Type: future object/entity collision foundation
- Depends on: Phase 11B
- Can be deferred until after Phase 11D if scripted doors are the priority
- Do not commit unless explicitly instructed by the user.

## Objective

Add a small backend-neutral registry of kinematic object colliders and overlap events without introducing dynamic rigid bodies or full ECS.

This is the bridge from “player vs static world” to “player/object/script interactions” while preserving the current architecture.

## Why This Is Optional

The branch does not yet have concrete ECS runtime entity ownership. A full entity-collision system should wait. However, a plain data registry can support:

- pickups;
- hurt volumes;
- interact volumes;
- scripted object zones;
- projectile hit query prototypes;
- future ECS integration.

If the user wants immediate gameplay from Lua, do Phase 11D first and defer 11E.

## Required Reading

- `include/stellar/world/RuntimeWorld.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `include/stellar/world/TriggerSystem.hpp`
- `include/stellar/server/WorldSession.hpp`
- `docs/Design.md`
- future ECS docs if added later

## Scope

Add shape primitives and deterministic overlap queries for runtime object colliders.

Preferred files:

```text
include/stellar/world/ObjectCollider.hpp
src/world/ObjectCollider.cpp
tests/world/ObjectCollider.cpp
```

Preferred API direction:

```cpp
namespace stellar::world {

/** @brief Backend-neutral runtime collider shape kind for kinematic objects. */
enum class ObjectColliderShapeType {
    kSphere,
    kAabb,
    kCapsule,
};

/** @brief Backend-neutral runtime collider shape data. */
struct ObjectColliderShape {
    ObjectColliderShapeType type = ObjectColliderShapeType::kSphere;
    std::array<float, 3> center{};
    std::array<float, 3> half_extents{0.5F, 0.5F, 0.5F};
    float radius = 0.5F;
    float height = 1.0F;
};

/** @brief Stable object collider owned by runtime/server code. */
struct ObjectCollider {
    std::uint32_t id = 0;
    std::string name;
    std::string archetype;
    ObjectColliderShape shape{};
    bool enabled = true;
};

/** @brief Deterministic overlap event between the player capsule and one object collider. */
struct ObjectColliderOverlapEvent {
    std::uint32_t collider_id = 0;
    std::string name;
    bool entered = false;
    bool stayed = false;
    bool exited = false;
};

/** @brief Backend-neutral state machine for kinematic object collider overlaps. */
class ObjectColliderSystem {
public:
    void set_colliders(std::span<const ObjectCollider> colliders);
    [[nodiscard]] std::vector<ObjectColliderOverlapEvent> update_player_capsule(
        const TriggerCapsule& player_capsule) noexcept;
};

}
```

If `TriggerCapsule` lives in `TriggerSystem`, reuse it to avoid duplicate shape definitions.

## Implementation Steps

1. Add primitive overlap helpers.
   - capsule-vs-sphere;
   - capsule-vs-AABB;
   - capsule-vs-capsule if cheap and deterministic.
   - Keep math small and display-free.

2. Add collider system.
   - Stable collider list.
   - Enabled flag.
   - Previous-overlap state.
   - Deterministic ordering by collider list order.

3. Add marker import bridge only if policy is clear.
   - Option A: derive object colliders from `WorldMarkerType::kEntitySpawn` scale.
   - Option B: add future `COLLIDER_<Name>` metadata convention in `docs/Design.md`.
   - Do not silently reinterpret existing markers without explicit docs.

4. Keep ECS integration deferred.
   - Registry can later be hydrated from ECS components.
   - For now, keep it plain data.

5. Keep script integration optional.
   - Do not add object script hooks in this phase unless scoped.
   - Later hooks may mirror trigger hooks:
     - `on_collider_enter`
     - `on_collider_stay`
     - `on_collider_exit`

## Non-Goals

Do not implement:

- rigid bodies;
- mass, forces, impulses;
- solver/contact manifolds;
- object-object collision response;
- network replication;
- full ECS ownership;
- renderer object binding.

## Required Tests

1. `empty_object_collider_system_returns_no_events`
2. `capsule_enters_sphere_collider`
3. `capsule_enters_aabb_collider`
4. `capsule_enters_capsule_collider_if_supported`
5. `disabled_collider_does_not_emit_events`
6. `enter_stay_exit_are_deterministic`
7. `duplicate_names_do_not_break_id_based_events`
8. `non_finite_shape_data_is_sanitized_or_ignored_by_policy`
9. `object_collider_order_is_stable`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_object_collider_test -j$(nproc)
ctest --test-dir build -R '^object_collider$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- A backend-neutral kinematic object collider registry exists.
- It supports deterministic overlap events.
- It does not add full physics, ECS, rendering, or networking.
- It is ready to be wired into `WorldSession` or future ECS later.
- Default CTest remains display-free.

---

# Phase 11F — Scripted Collision Authoring Smoke and Documentation

## Task Card

- Branch: `collision-movement`
- Primary agent: `@director` for integration planning; implementation likely `@carmack`
- Type: integration smoke / documentation
- Depends on: Phases 11A, 11B, 11C, 11D
- Can draft docs earlier; smoke test should run last
- Do not commit unless explicitly instructed by the user.

## Objective

Prove the new collision path with one end-to-end display-free scripted collision scenario and update docs so future agents know the new runtime collision rules.

## Required Reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `tests/integration/PlayableWorldSmoke.cpp`
- `tests/integration/ScriptedPlayableWorldSmoke.cpp`
- `include/stellar/scripting/ScriptedWorldSession.hpp`
- `include/stellar/scripting/ScriptCommandProcessor.hpp`
- `include/stellar/world/RuntimeCollisionState.hpp`
- all Phase 11 completion notes

## Integration Scenario

Create or extend a generated glTF fixture with:

- visible render floor/walls;
- collision-only floor;
- collision-only wall blocker named `DoorBlocker`;
- `SPAWN_Player`;
- `TRIGGER_DoorOpen`;
- trigger script binding:
  - `stellar.script = "scripts/door.lua"`
  - `stellar.table = "Door"`

Script:

```lua
Door = {}

function Door.on_trigger_enter(event)
    stellar.emit_event("collision.set_mesh_enabled", {
        mesh = "DoorBlocker",
        enabled = false
    })
end
```

Test behavior:

1. Import succeeds.
2. Collision validation succeeds or reports only expected non-fatal warnings.
3. Metadata validation succeeds.
4. Runtime world contains static collision and trigger metadata.
5. Collision state contains `DoorBlocker` enabled by default.
6. Before entering the trigger, movement into `DoorBlocker` is blocked.
7. Entering `TRIGGER_DoorOpen` invokes Lua.
8. Lua emits `collision.set_mesh_enabled`.
9. Native command processor applies the command.
10. On the next tick, movement can pass through the disabled `DoorBlocker`.
11. Repeat run produces identical snapshots, script events, command results, and final position.
12. Test requires no display, GPU, OpenGL, Vulkan, or SDL window.

Preferred test file:

```text
tests/integration/ScriptedCollisionSmoke.cpp
```

Preferred CTest name:

```text
scripted_collision_smoke
```

## Documentation Updates

### `docs/ImplementationStatus.md`

Add a current Phase 11 section:

```md
Phase 11A-F planned or active:

- Capsule-aware trigger overlap now aligns trigger events with capsule character movement.
- Runtime collision state overlays immutable imported static collision so named collision meshes can be enabled/disabled authoritatively.
- Filtered collision queries allow movement, raycasts, ground probes, and character controller sweeps to respect runtime collision state.
- Lua scripts still do not mutate C++ state directly. Scripted collision behavior is applied through native validated command processing.
- The first scripted collision smoke covers a trigger opening a named collision blocker without display/GPU context.
```

Update this after each phase with completion notes and validation commands.

### `docs/Design.md`

Add or update sections:

- Static collision remains imported immutable asset data.
- Runtime collision state is server-owned and may enable/disable named static collision meshes.
- Scripts emit collision requests through events; native server code validates and applies them.
- Collision state changes are authoritative and affect subsequent ticks.
- Full dynamic rigid bodies remain deferred.
- Kinematic object collider registry is optional/future and not a rigid body physics engine.
- Trigger overlap uses the authoritative character capsule where available.

### Plan Files

If copied into the repo, use:

```text
Plans/Phase11A-CapsuleAwareTriggerOverlap.md
Plans/Phase11B-RuntimeCollisionStateOverlay.md
Plans/Phase11C-FilteredCollisionQueriesAndMovement.md
Plans/Phase11D-ScriptCollisionCommands.md
Plans/Phase11E-KinematicObjectColliderRegistry.md
Plans/Phase11F-ScriptedCollisionSmokeAndDocs.md
```

Archive only after completion.

## Required Tests

1. `scripted_collision_smoke`
2. Existing `scripted_playable_world_smoke` still passes.
3. Existing `playable_world_smoke` still passes.
4. Existing collision, world, server, and scripting tests still pass.

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_scripted_collision_smoke_test \
  stellar_scripted_playable_world_smoke_test \
  stellar_playable_world_smoke_test \
  -j$(nproc)
ctest --test-dir build -R '^(scripted_collision_smoke|scripted_playable_world_smoke|playable_world_smoke)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- End-to-end display-free scripted collision behavior works.
- Native server code validates script collision requests.
- Runtime collision mesh toggles affect subsequent authoritative movement.
- Repeat run is deterministic.
- Documentation explains the collision state and scripting boundary.
- Full CTest passes with GLTF and Lua enabled.

---

## 6. Suggested Pull Request Boundaries

Use one PR per phase:

1. `phase11a-capsule-aware-triggers`
2. `phase11b-runtime-collision-state`
3. `phase11c-filtered-collision-movement`
4. `phase11d-script-collision-commands`
5. `phase11e-object-collider-registry` *(optional/deferable)*
6. `phase11f-scripted-collision-smoke-docs`

Each PR should include:

- implementation;
- focused tests;
- completion notes;
- `docs/ImplementationStatus.md` update;
- full validation result.

---

## 7. Risk Register

### Risk: Query API churn breaks existing movement tests

Mitigation:

- Keep old overloads.
- Add filtered overloads.
- Existing tests should exercise all-enabled default behavior.

### Risk: Script outputs become an unbounded command surface

Mitigation:

- Only support namespaced, explicitly parsed events.
- Unknown events produce deterministic command results.
- Do not expose raw C++ state or object pointers.

### Risk: Runtime collision state duplicates imported asset state

Mitigation:

- Asset remains immutable source.
- Runtime state stores only enabled masks and name lookup.
- `WorldSession::reset()` rebuilds state from the current world.

### Risk: Duplicate collision mesh names cause ambiguous script behavior

Mitigation:

- Warn deterministically.
- Choose one explicit policy:
  - toggle all matching names, or
  - reject duplicate target names.
- Recommended initial policy: toggle all matching names, because authored door blockers may be multi-mesh.

### Risk: Capsule trigger changes break existing trigger tests

Mitigation:

- Preserve `update_sphere`.
- Add `update_capsule`.
- Movement trigger integration should explicitly opt into capsule behavior.
- Existing pure trigger sphere tests should still pass.

### Risk: Door blocker visual state does not match collision state

Mitigation:

- Phase 11 only changes authoritative collision.
- Rendering/animation state can consume command results later.
- Do not add renderer mutation in this collision phase.

---

## 8. Explicit Deferred Work

Do not include in Phase 11 unless the user explicitly asks:

- third-party physics engine;
- dynamic rigid bodies;
- rigid-body solver;
- object-object collision response;
- network replication of collision state;
- client prediction/reconciliation;
- navmesh/pathfinding;
- moving platforms;
- collision debug renderer;
- ECS component migration;
- script callbacks for every object/entity type;
- animation or visual door-opening behavior.

---

## 9. Completion Notes Template for Future Agents

Append to each copied plan after implementation:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: <phase name>.
- Public/API changes: <headers/functions/classes>.
- Runtime behavior: <what now happens>.
- Determinism: <ordering and repeatability rules>.
- Tests added/updated: <test files and CTest names>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - <items intentionally left for later>.
```

---

## 10. Recommended Immediate Next Step

Start with **Phase 11A** and **Phase 11B**.

Reason:

- Phase 11A fixes a known correctness mismatch made more important by Lua trigger hooks.
- Phase 11B creates the runtime collision state foundation needed for scripted doors/gates.
- Both are relatively self-contained and can be developed in parallel if agents avoid shared files.

After those land, do **Phase 11C** and **Phase 11D** to make the state affect movement and to let Lua scripts request collision changes through validated native commands.
