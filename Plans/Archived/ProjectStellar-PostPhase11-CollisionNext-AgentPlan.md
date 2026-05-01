# Project Stellar — Post-Phase-11 Collision Next-Step Agent Plan

**Repository:** `JTM-rootstorm/Project-Stellar`  
**Branch inspected:** `collision-movement`  
**Generated:** 2026-04-30  
**Prepared for:** AI implementation agents such as Codex, Kilo, or other code-writing agents  
**Output type:** planning handoff only; do **not** commit unless the user explicitly asks.

---

## 0. AI Agent Intake Summary

The branch has already moved past the archived "next collision after Lua" plan. Treat Phase 11 as implemented, not as the next task.

Current branch evidence says Phase 11A-F is complete:

- capsule-aware trigger overlap;
- `RuntimeCollisionState` static mesh enable/disable overlay;
- filtered collision queries and movement integration;
- native validation of `collision.set_mesh_enabled` script commands;
- backend-neutral `ObjectColliderSystem` foundation;
- display-free `scripted_collision_smoke` proving a Lua trigger can disable a named static collision mesh and let subsequent authoritative movement pass through it.

The next collision work should therefore **not** restart collision extraction, capsule triggers, static mesh runtime toggles, or the Lua collision command path.

Recommended next collision slice:

> **Phase 12 — Authoritative Object Collider Integration and Gameplay-Visible Collision Events**

The existing `ObjectColliderSystem` is currently a useful backend-neutral foundation, but it is not yet a gameplay/runtime feature. It is not integrated into `WorldSession`, not authored from glTF metadata, not present in `WorldSnapshot`, not script-visible, and not robust enough for lifecycle changes such as disabling/removing a collider while overlapped.

Implement Phase 12 in this order:

1. **Phase 12A — Harden ObjectCollider lifecycle semantics**
2. **Phase 12B — Integrate object-collider overlap events into `WorldSession`**
3. **Phase 12C — Add authored object-collider metadata and validation**
4. **Phase 12D — Add server-authoritative Lua hooks/commands for object colliders**
5. **Phase 12E — Add final scripted object-collider smoke coverage and docs**

Defer solid/moving blocker collision response, dynamic rigid bodies, third-party physics, navmesh, networking replication, broad ECS, renderer debug views, and client authority.

---

## 1. Current Branch Evidence Snapshot

### 1.1 Implementation status

`docs/ImplementationStatus.md` says the branch has completed the full Phase 11 collision-scripting slice:

- capsule-aware trigger overlap;
- server-owned runtime collision mesh enable state;
- filtered raycast/ground/sphere/triangle/character movement queries;
- native processing of script-emitted `collision.set_mesh_enabled`;
- backend-neutral `ObjectColliderSystem`;
- `scripted_collision_smoke` coverage for a glTF-triggered script disabling `DoorBlocker`;
- full CTest pass reported as **29/29** with glTF and Lua enabled.

Use this as the current source of truth. Archived Phase 8-11 plan files are historical unless you are reading them for context.

### 1.2 Design constraints to preserve

`docs/Design.md` and `AGENTS.md` reinforce these branch rules:

- server owns gameplay, collision, movement, validation, and scripts;
- client remains presentation;
- imported collision assets remain immutable;
- runtime collision mutations must be server-owned overlays/state, not asset rewrites;
- Lua emits primitive events/commands only; native C++ validates and applies;
- default tests remain display-free;
- no third-party physics engine yet;
- no dynamic rigid bodies yet;
- public APIs require Doxygen `@brief`.

### 1.3 Current implemented collision/runtime surfaces

Observed current files:

```text
include/stellar/physics/CollisionWorld.hpp
src/physics/CollisionWorld.cpp
include/stellar/physics/CharacterController.hpp
src/physics/CharacterController.cpp
include/stellar/world/RuntimeCollisionState.hpp
src/world/RuntimeCollisionState.cpp
include/stellar/world/TriggerSystem.hpp
src/world/TriggerSystem.cpp
include/stellar/world/ObjectCollider.hpp
src/world/ObjectCollider.cpp
include/stellar/server/MovementSimulation.hpp
src/server/MovementSimulation.cpp
include/stellar/server/MovementTriggerIntegration.hpp
src/server/MovementTriggerIntegration.cpp
include/stellar/server/WorldSession.hpp
src/server/WorldSession.cpp
include/stellar/scripting/ScriptCommandProcessor.hpp
src/scripting/ScriptCommandProcessor.cpp
include/stellar/scripting/ScriptedWorldSession.hpp
src/scripting/ScriptedWorldSession.cpp
tests/integration/ScriptedCollisionSmoke.cpp
```

Current static collision path:

```text
glTF collision-only nodes
  -> SceneAsset::level_collision
  -> RuntimeWorld::collision_world
  -> WorldSession-owned RuntimeCollisionState
  -> filtered CollisionWorld/CharacterController queries
  -> authoritative movement
```

Current scripted static collision path:

```text
authoritative movement tick
  -> capsule trigger events
  -> TriggerScriptSystem
  -> Lua script emits collision.set_mesh_enabled
  -> ScriptCommandProcessor validates
  -> WorldSession::set_collision_mesh_enabled
  -> next authoritative tick sees updated collision filter
```

Current object collider path:

```text
ObjectColliderSystem exists in stellar_world
  -> supports sphere/AABB/capsule overlap against player capsule
  -> emits enter/stay/exit events
  -> stores colliders in caller-provided order
  -> not owned by WorldSession
  -> not included in WorldSnapshot
  -> not loaded from glTF metadata
  -> not script-visible
  -> not a solid movement blocker
```

### 1.4 Specific gap found in current object collider tests

`tests/world/ObjectCollider.cpp` contains a test named:

```cpp
disabled_previously_overlapping_collider_emits_exit
```

But the test currently disables by rebuilding the collider list through `set_colliders`, and then asserts that the next update is empty. The implementation also clears previous overlap state on `set_colliders`.

This is a semantics gap to fix before object colliders become gameplay-visible:

- If `set_colliders` is intentionally a hard reset, rename the test and document reset behavior.
- If runtime enable/disable/removal should be gameplay-visible, add mutation APIs that preserve overlap state by stable collider id and produce deterministic exit events.

Recommendation: keep hard reset behavior for `set_colliders`, but add explicit lifecycle mutation APIs that preserve overlap state and emit exits.

---

## 2. Recommendation

Implement **Phase 12: Authoritative Object Collider Integration and Gameplay-Visible Collision Events**.

Why this is the right next collision work:

1. Static authored collision is already usable by server movement and Lua-triggered collision commands.
2. The remaining collision foundation that exists but is not usable at runtime is `ObjectColliderSystem`.
3. Gameplay needs pickups, hazards, interactables, props, and entity/object sensors before it needs rigid bodies.
4. Object collider overlaps are safer and lower-risk than solid dynamic collision response.
5. This keeps the branch aligned with server authority and display-free testing.

Do **not** make the next slice about:

- replacing the collision system with Bullet/PhysX/Jolt/Box2D;
- rigid body simulation;
- dynamic broadphase for physical bodies;
- exact continuous capsule CCD;
- full ECS object ownership;
- remote networking/prediction;
- client-side collision authority;
- renderer debug visualization;
- arbitrary Lua mutation of engine state.

---

## 3. Phase Order and Dependencies

```text
Required order:
  12A -> 12B -> 12D -> 12E

Authoring lane:
  12C can start after 12A, and can run partly in parallel with 12B if it does not touch WorldSession.

Final integration:
  12E depends on 12A, 12B, 12C, and 12D.
```

Safe parallelization:

- **12A and Design/ImplementationStatus drafting** can run together.
- **12C can run in parallel with 12B** if 12C only adds metadata parsing/build helpers/validation and does not modify `WorldSession`.
- **12B tests can initially use programmatic object colliders**, avoiding dependency on glTF authoring.
- **12D must wait for 12B** because script hooks need authoritative object-collider events.
- **12E must wait for all previous phases**.

Do not run in parallel:

- 12A and any other work that changes `ObjectColliderSystem` storage semantics.
- 12B and 12D if both modify `WorldSnapshot`, `ScriptedWorldSession`, or command processing.
- 12C and 12D if both change script-binding validation policies on marker types.

---

## 4. Global Implementation Rules

For all phases:

- Keep collision and world code backend-neutral.
- Keep default CTest display-free.
- Preserve server authority.
- Keep imported assets immutable.
- Keep runtime changes in server-owned state/overlays.
- Keep Lua isolated to `stellar_scripting`.
- Scripts must emit primitive events/commands; native C++ validates and applies.
- Use deterministic ordering and stable machine-readable result codes.
- Use Doxygen `@brief` for new public APIs.
- Keep source-compatible overloads where practical.
- Update `docs/ImplementationStatus.md` after implementation.
- Update `docs/Design.md` only when broad design or authoring conventions change.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.
- Do not add a third-party physics engine.
- Do not add dynamic rigid bodies in Phase 12.

Baseline validation at the end of each phase:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

OpenGL/Vulkan context tests remain opt-in only.

---

# Phase 12A — ObjectCollider Lifecycle Hardening

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: backend-neutral collision/runtime correctness
- Depends on: completed Phase 11E object collider foundation
- Can run in parallel with: docs drafting only
- Must finish before: Phase 12B, 12D, 12E
- Do not commit unless explicitly instructed by the user.

## Objective

Make `ObjectColliderSystem` safe for gameplay-visible runtime use.

The current system is fine as a first-pass overlap foundation, but it stores previous overlap state by vector index and clears state on `set_colliders`. That is acceptable for reset, but unsafe for live runtime changes.

Implement explicit lifecycle semantics:

- hard reset replaces colliders and clears overlap state;
- runtime enable/disable preserves overlap history by stable collider id;
- disabling/removing a currently-overlapped collider emits a deterministic exit event;
- reordering colliders does not silently corrupt previous overlap state;
- duplicate collider ids are diagnosed or rejected deterministically.

## Required Reading

```text
include/stellar/world/ObjectCollider.hpp
src/world/ObjectCollider.cpp
tests/world/ObjectCollider.cpp
include/stellar/world/TriggerSystem.hpp
src/world/TriggerSystem.cpp
docs/ImplementationStatus.md
docs/Design.md
```

## Recommended API Direction

Keep existing `set_colliders` as hard-reset behavior, but document it clearly.

Add result and diagnostic types:

```cpp
namespace stellar::world {

enum class ObjectColliderDiagnosticSeverity {
    kWarning,
    kError,
};

struct ObjectColliderDiagnostic {
    ObjectColliderDiagnosticSeverity severity = ObjectColliderDiagnosticSeverity::kWarning;
    std::string code;
    std::string message;
    std::uint32_t collider_id = 0;
    std::size_t collider_index = 0;
};

struct ObjectColliderMutationResult {
    bool applied = false;
    std::string code;
    std::string message;
};

}
```

Add lifecycle APIs:

```cpp
class ObjectColliderSystem {
public:
    /** @brief Replace runtime colliders and clear previous overlap state. */
    void set_colliders(std::span<const ObjectCollider> colliders);

    /** @brief Replace colliders while preserving overlap state by stable collider id. */
    [[nodiscard]] std::vector<ObjectColliderOverlapEvent>
    replace_colliders_preserving_overlaps(std::span<const ObjectCollider> colliders) noexcept;

    /** @brief Enable or disable one collider by stable id and queue an exit if needed. */
    [[nodiscard]] ObjectColliderMutationResult set_collider_enabled(
        std::uint32_t collider_id,
        bool enabled) noexcept;

    /** @brief Insert or replace one collider by stable id without clearing unrelated overlap state. */
    [[nodiscard]] ObjectColliderMutationResult upsert_collider(
        const ObjectCollider& collider) noexcept;

    /** @brief Remove one collider by stable id and queue an exit if it was overlapped. */
    [[nodiscard]] ObjectColliderMutationResult remove_collider(
        std::uint32_t collider_id) noexcept;

    /** @brief Return deterministic diagnostics for duplicate ids, suspicious ids, or invalid shapes. */
    [[nodiscard]] std::vector<ObjectColliderDiagnostic> diagnostics() const;
};
```

Acceptable alternative: if pending-exit queues are too invasive, make mutation APIs return exit events directly. Do not silently drop exits for runtime mutations.

## Implementation Notes

1. Preserve `set_colliders` as a reset.
   - Existing callers that want reset behavior should remain simple.
   - Rename misleading tests if reset behavior intentionally emits no exit.

2. Store overlap state by `collider_id`, not by vector index, for runtime mutations.
   - Preserve deterministic output order by current collider order for current overlaps.
   - For removed colliders, queue exits in removal order or previous order, documented and tested.

3. Add pending exit handling.
   - If an overlapped collider is disabled or removed, emit exactly one exit event.
   - The exit event should include the last known `collider_id`, `name`, and eventually `archetype`.
   - Do not emit stay after a queued exit.

4. Add duplicate-id policy.
   - Recommended: duplicate ids are errors for runtime mutation APIs.
   - `set_colliders` may accept duplicates only with diagnostics, but `update_player_capsule` must remain deterministic.
   - Prefer making duplicate ids impossible in authored/runtime helpers.

5. Preserve duplicate names.
   - Duplicate names are allowed by current public docs.
   - Do not use names for overlap-state identity.

6. Keep shape overlap functions unchanged unless tests expose a correctness bug.

## Non-Goals

Do not implement:

- `WorldSession` integration;
- glTF authoring conventions;
- Lua hooks;
- solid collision response;
- ECS ownership;
- rigid bodies;
- dynamic broadphase.

## Required Tests

Extend `tests/world/ObjectCollider.cpp`.

Add or update tests:

1. `set_colliders_is_hard_reset_and_does_not_emit_exit`
2. `disabled_previously_overlapping_collider_emits_exit_with_mutation_api`
3. `removed_previously_overlapping_collider_emits_exit_once`
4. `reordered_colliders_preserve_overlap_state_by_id`
5. `upsert_preserves_unrelated_overlap_state`
6. `duplicate_collider_ids_report_diagnostic`
7. `duplicate_names_remain_allowed_but_do_not_affect_identity`
8. `queued_exit_order_is_deterministic`
9. `disabled_collider_can_reenter_after_reenabled`
10. `non_finite_shape_diagnostics_are_deterministic`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_object_collider_test -j$(nproc)
ctest --test-dir build -R '^object_collider$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Runtime collider mutation semantics are explicit.
- Hard-reset and live-mutation behavior are both tested.
- Disabling/removing an overlapped runtime collider can produce an exit event.
- Reordering colliders does not corrupt overlap state.
- Duplicate id policy is deterministic.
- Existing shape overlap tests still pass.
- Default CTest remains display-free.

---

# Phase 12B — WorldSession Object Collider Integration

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` if snapshot shape or gameplay event semantics need policy decisions
- Type: authoritative runtime/session integration
- Depends on: Phase 12A
- Can run in parallel with: Phase 12C authoring helpers, if no shared files are edited
- Must finish before: Phase 12D
- Do not commit unless explicitly instructed by the user.

## Objective

Make object collider overlap events part of the authoritative world session.

After this phase, `WorldSession` should own object collider runtime state and emit object overlap transitions after authoritative movement resolves, using the same sanitized character capsule dimensions as movement and trigger overlap.

Programmatic colliders are enough for this phase. glTF authoring comes later.

## Required Reading

```text
include/stellar/world/ObjectCollider.hpp
src/world/ObjectCollider.cpp
include/stellar/server/WorldSession.hpp
src/server/WorldSession.cpp
include/stellar/server/MovementTriggerIntegration.hpp
src/server/MovementTriggerIntegration.cpp
include/stellar/server/MovementSimulation.hpp
src/server/MovementSimulation.cpp
include/stellar/client/LocalLoopbackRuntime.hpp
src/client/LocalLoopbackRuntime.cpp
tests/server/WorldSession.cpp
tests/client/LocalLoopbackRuntime.cpp
```

## Recommended API Direction

Add server-level event data rather than exposing world-level overlap events directly in snapshots.

```cpp
namespace stellar::server {

struct ObjectColliderEvent {
    PlayerId player_id = 0;
    std::uint32_t collider_id = 0;
    std::string name;
    std::string archetype;
    bool entered = false;
    bool stayed = false;
    bool exited = false;
};

struct WorldSnapshot {
    std::uint64_t tick = 0;
    std::vector<PlayerSnapshot> players;
    std::vector<MovementTriggerEvent> trigger_events;
    std::vector<ObjectColliderEvent> object_collider_events;
};

}
```

Add `WorldSession` APIs:

```cpp
class WorldSession {
public:
    /** @brief Replace runtime object colliders and reset object overlap state. */
    void set_object_colliders(std::span<const stellar::world::ObjectCollider> colliders);

    /** @brief Replace runtime object colliders while preserving overlap state by id. */
    [[nodiscard]] std::vector<ObjectColliderEvent>
    replace_object_colliders_preserving_overlaps(
        std::span<const stellar::world::ObjectCollider> colliders) noexcept;

    /** @brief Authoritatively enable or disable one object collider for future overlap updates. */
    [[nodiscard]] stellar::world::ObjectColliderMutationResult set_object_collider_enabled(
        std::uint32_t collider_id,
        bool enabled) noexcept;

    /** @brief Add or replace one object collider by stable id. */
    [[nodiscard]] stellar::world::ObjectColliderMutationResult upsert_object_collider(
        const stellar::world::ObjectCollider& collider) noexcept;

    /** @brief Remove one object collider by stable id. */
    [[nodiscard]] stellar::world::ObjectColliderMutationResult remove_object_collider(
        std::uint32_t collider_id) noexcept;
};
```

Add `WorldSessionConfig` input only if it is cleaner for tests:

```cpp
struct WorldSessionConfig {
    MovementSimulationConfig movement{};
    PlayerId local_player_id = 1;
    std::vector<stellar::world::ObjectCollider> object_colliders;
};
```

## Update Order Policy

For each tick:

1. Select and sanitize local player movement command.
2. Run authoritative movement.
3. Increment tick index.
4. Update trigger overlap from final authoritative position.
5. Update object collider overlap from final authoritative position.
6. Return `WorldSnapshot` with both event streams.

This keeps object collider events based on the same authoritative position as triggers.

If scripts later need both streams, `ScriptedWorldSession` can process them in deterministic order:

```text
trigger events first, object collider events second
```

Document that order in Phase 12D.

## Implementation Steps

1. Add object-collider event data to server headers.
2. Add `ObjectColliderSystem object_colliders_` to `WorldSession`.
3. Reset object colliders on `WorldSession::reset`.
   - If using `WorldSessionConfig::object_colliders`, load them here.
   - If not, reset to empty until caller sets colliders.
4. Add capsule construction helper shared with trigger integration if possible.
   - Use `sanitize_movement_simulation_config(config).value.character`.
   - Use final authoritative player position.
   - Use world-Y up.
5. Convert `ObjectColliderOverlapEvent` into `server::ObjectColliderEvent`.
6. Include events in snapshots from ticks only.
   - `snapshot()` should not replay events.
   - `latest_snapshot()` in local loopback remains event-safe.
7. Update `LocalLoopbackRuntime` tests if snapshot equality assumptions change.
8. Keep everything display-free.

## Non-Goals

Do not implement:

- glTF-authored object colliders;
- Lua hooks;
- object collider command processing;
- solid collision response;
- multi-player object collider state;
- networking replication;
- ECS entity ownership.

## Required Tests

Add or extend `tests/server/WorldSession.cpp`.

Required coverage:

1. `world_session_with_no_object_colliders_emits_no_object_events`
2. `world_session_emits_object_collider_enter_after_authoritative_movement`
3. `world_session_emits_object_collider_stay_and_exit`
4. `world_session_object_events_use_sanitized_character_capsule_height`
5. `snapshot_does_not_replay_object_collider_events`
6. `reset_clears_object_collider_overlap_state`
7. `set_object_collider_enabled_emits_exit_when_overlapped`
8. `same_inputs_and_colliders_produce_same_object_event_sequence`
9. `local_loopback_runtime_preserves_object_events_from_authoritative_snapshot`

Optional new focused test target if `WorldSession.cpp` becomes too large:

```text
tests/server/ObjectColliderIntegration.cpp
CTest name: server_object_collider_integration
```

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target \
  stellar_object_collider_test \
  stellar_server_world_session_test \
  stellar_client_local_loopback_runtime_test \
  -j$(nproc)
ctest --test-dir build -R '^(object_collider|server_world_session|client_local_loopback_runtime)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- `WorldSession` owns deterministic object collider overlap state.
- `WorldSnapshot` carries object collider event transitions from the latest tick.
- Object collider overlap uses the same character capsule dimensions as authoritative movement.
- Pure snapshots do not replay events.
- Runtime enable/disable/removal semantics are deterministic.
- No graphics/audio/platform dependency is introduced.
- Default CTest remains display-free.

---

# Phase 12C — Authored Object Collider Metadata and Validation

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director`; coordinate with `@miyamoto` only if import/render conventions conflict
- Type: asset/import/world authoring
- Depends on: Phase 12A preferred; can start before 12B if kept isolated
- Must finish before: final authored/scripting smoke
- Do not commit unless explicitly instructed by the user.

## Objective

Allow glTF/world metadata to define backend-neutral object colliders for pickups, hazards, interactables, and other gameplay sensors.

This phase should add authoring support for **sensor-style object colliders**, not solid movement blockers.

## Required Reading

```text
include/stellar/assets/WorldMetadataAsset.hpp
src/import/gltf/WorldMetadataImport.cpp
include/stellar/world/WorldMetadataValidation.hpp
src/world/WorldMetadataValidation.cpp
include/stellar/world/RuntimeWorld.hpp
src/world/RuntimeWorld.cpp
include/stellar/world/ObjectCollider.hpp
tests/import/gltf/ImporterRegression.cpp
tests/world/WorldMetadataValidation.cpp
tests/world/RuntimeWorld.cpp
docs/Design.md
```

## Recommended Authoring Convention

Add a new marker type:

```cpp
enum class WorldMarkerType {
    kPlayerSpawn,
    kEntitySpawn,
    kTrigger,
    kSprite,
    kPortal,
    kObjectCollider,
};
```

Initial glTF node convention:

```text
COLLIDER_<Name>
```

Interpretation:

- node translation = collider center;
- absolute node scale = AABB half extents;
- node rotation is ignored initially, matching trigger AABB simplicity;
- marker name is `<Name>` after the prefix;
- marker archetype remains optional plain data;
- script binding may be preserved but should warn as unsupported until Phase 12D.

Why `COLLIDER_` instead of `COL_`:

- `COL_*` is already static collision mesh extraction.
- `COLLIDER_*` is metadata/sensor authoring, not static triangle extraction.
- Make sure importer logic does not accidentally classify `COLLIDER_*` as a collision-only mesh.

Initial shape policy:

- First authored shape: AABB only.
- Runtime APIs may still support sphere/capsule programmatically.
- Do not add a general JSON parser just to author sphere/capsule in this phase.
- Future extras may add shape type/radius/height after a parser policy is chosen.

## Recommended Builder API

Add helper functions in `ObjectCollider.hpp` or a new `ObjectColliderAuthoring.hpp` if the file grows too broad:

```cpp
namespace stellar::world {

/** @brief Build runtime object colliders from authored world metadata collider markers. */
[[nodiscard]] std::vector<ObjectCollider> build_object_colliders(
    const stellar::assets::WorldMetadataAsset& metadata);

/** @brief Build runtime object colliders from a RuntimeWorld's copied metadata. */
[[nodiscard]] std::vector<ObjectCollider> build_object_colliders(
    const RuntimeWorld& world);

}
```

ID policy:

- Assign deterministic nonzero ids in metadata marker order.
- Recommended: `id = marker_index + 1`.
- If future saved games/networking need stable cross-version ids, add explicit authored ids later.
- Do not use `std::hash`.

## Validation Policy

Extend metadata validation with deterministic findings:

- `empty_object_collider_name` warning or error; recommendation: warning if the collider can still get an id, but script lookup by name will be poor.
- `duplicate_object_collider_name` warning.
- `empty_object_collider_extents` warning.
- `large_object_collider_extents` warning, reuse trigger threshold or add documented collider threshold.
- `non_finite_position/scale` existing errors should already apply.
- `script_binding_unsupported_marker_type` should still warn for object colliders until Phase 12D.
- `object_collider_runtime_deferred` is **not** needed once Phase 12B exists.

## Implementation Steps

1. Add `kObjectCollider` to `WorldMarkerType`.
2. Extend glTF metadata import to detect `COLLIDER_<Name>` nodes.
3. Ensure `COLLIDER_*` nodes are not treated as static collision-only mesh nodes.
4. Preserve existing raw `extras_json` behavior.
5. Add object collider builder helpers.
6. Add metadata validation coverage.
7. Optionally extend `RuntimeWorldDiagnostics` with `object_collider_marker_count`.
8. Update `docs/Design.md` authoring conventions.
9. Update `docs/ImplementationStatus.md`.

## Non-Goals

Do not implement:

- object collider script hooks;
- solid object collision response;
- oriented boxes;
- shape extras parser;
- ECS entity spawning from colliders;
- renderer debug views;
- network serialization.

## Required Tests

Update or add display-free tests.

Importer regression:

1. `gltf_importer_extracts_collider_marker_from_COLLIDER_prefix`
2. `COLLIDER_prefix_does_not_create_static_collision_mesh`
3. `COLLIDER_marker_preserves_script_binding_but_does_not_execute_script`
4. `collider_marker_transform_position_and_scale_are_preserved`

World/object tests:

5. `build_object_colliders_from_metadata_aabb`
6. `build_object_colliders_assigns_deterministic_ids`
7. `build_object_colliders_preserves_order`
8. `runtime_world_copies_object_collider_markers`

Validation tests:

9. `empty_object_collider_name_warns`
10. `duplicate_object_collider_name_warns`
11. `empty_object_collider_extents_warns`
12. `object_collider_script_binding_warns_until_script_hooks_exist`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_import_gltf_regression \
  stellar_world_metadata_validation_test \
  stellar_runtime_world_test \
  stellar_object_collider_test \
  -j$(nproc)
ctest --test-dir build -R '^(gltf_importer_regression|world_metadata_validation|runtime_world|object_collider)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Authored glTF/world metadata can define object collider markers.
- The new convention does not conflict with static `COL_*` collision extraction.
- Runtime object colliders can be built deterministically from metadata.
- Validation diagnostics are deterministic.
- Design docs describe the convention.
- Default CTest remains display-free.

---

# Phase 12D — Object Collider Script Hooks and Commands

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` because this crosses scripting, server authority, collision, and gameplay semantics
- Type: authoritative scripting/event integration
- Depends on: Phase 12B; Phase 12C for authored script bindings
- Must finish before: final scripted object-collider smoke
- Do not commit unless explicitly instructed by the user.

## Objective

Make object collider overlap events script-visible through server-authoritative Lua hooks, while preserving the existing script safety model.

Scripts should observe authoritative object overlap events and emit primitive output commands/events. Native C++ should validate and apply only narrow supported commands.

## Required Reading

```text
include/stellar/scripting/LuaRuntime.hpp
src/scripting/LuaRuntime.cpp
include/stellar/scripting/TriggerScriptSystem.hpp
src/scripting/TriggerScriptSystem.cpp
include/stellar/scripting/ScriptedWorldSession.hpp
src/scripting/ScriptedWorldSession.cpp
include/stellar/scripting/ScriptCommandProcessor.hpp
src/scripting/ScriptCommandProcessor.cpp
include/stellar/server/WorldSession.hpp
src/server/WorldSession.cpp
include/stellar/world/ObjectCollider.hpp
src/world/ObjectCollider.cpp
tests/scripting/TriggerScriptSystem.cpp
tests/scripting/ScriptedWorldSession.cpp
tests/scripting/ScriptCommandProcessor.cpp
```

## Hook Policy

Add object collider hooks:

```lua
function Item.on_object_collider_enter(event) end
function Item.on_object_collider_stay(event) end
function Item.on_object_collider_exit(event) end
```

Initial event fields:

```lua
event.tick
event.player_id
event.collider_id
event.collider_name
event.archetype
```

Ordering policy:

```text
native movement tick
  -> trigger events
  -> object collider events
  -> trigger script callbacks
  -> object collider script callbacks
  -> native command validation/application
```

Alternative acceptable order:

```text
process trigger and object scripts in the exact order their event streams are stored in WorldSnapshot
```

Whichever order is chosen must be documented and tested.

## Script Binding Policy

Currently script binding validation warns when script bindings are attached to non-trigger markers.

After Phase 12D:

- trigger markers support trigger hooks;
- object collider markers support object collider hooks;
- script binding on object colliders should no longer emit `script_binding_unsupported_marker_type`;
- script binding on sprites/portals/entity spawns should still warn unless supported later.

## Recommended C++ Additions

New files:

```text
include/stellar/scripting/ObjectColliderScriptSystem.hpp
src/scripting/ObjectColliderScriptSystem.cpp
tests/scripting/ObjectColliderScriptSystem.cpp
```

API direction:

```cpp
namespace stellar::scripting {

struct ObjectColliderScriptResult {
    std::vector<ScriptOutputEvent> output_events;
    std::vector<ScriptError> errors;
};

/** @brief Invokes Lua hooks for server-authoritative object collider events. */
class ObjectColliderScriptSystem {
public:
    /** @brief Build script bindings from RuntimeWorld object collider metadata. */
    explicit ObjectColliderScriptSystem(const stellar::world::RuntimeWorld& world);

    /** @brief Process object collider events from an authoritative WorldSnapshot. */
    [[nodiscard]] ObjectColliderScriptResult process_object_collider_events(
        LuaRuntime& runtime,
        const stellar::server::WorldSnapshot& snapshot) noexcept;
};

}
```

Update `ScriptedWorldSession`:

```cpp
class ScriptedWorldSession {
    TriggerScriptSystem trigger_scripts_;
    ObjectColliderScriptSystem object_collider_scripts_;
};
```

Update script loading:

- Load unique trigger script ids.
- Load unique object collider script ids.
- Load each script source only once.
- Missing object collider script source should fail create deterministically, same as trigger scripts.

## Command Vocabulary

Add one narrow command:

```lua
stellar.emit_event("object_collider.set_enabled", {
    id = 7,
    enabled = false
})
```

Validation policy:

- `id` must be a finite integer-like number and within `uint32_t`.
- `enabled` must be boolean.
- Unknown id returns `not_found`.
- Invalid fields return `invalid_field`.
- Unsupported command returns `unsupported_event`.
- Applying a disable to an overlapped collider should produce deterministic exit behavior through Phase 12A/B lifecycle semantics.

Do not support name-based object collider commands initially unless a deterministic duplicate-name policy is added.

## Implementation Steps

1. Add `ObjectColliderScriptSystem`.
2. Update `ScriptedWorldSession::create` to load object collider scripts.
3. Update `ScriptedWorldSession::tick` to process object collider events.
4. Preserve existing trigger script behavior and tests.
5. Extend `ScriptCommandProcessor` with `object_collider.set_enabled`.
6. Add `WorldSession::set_object_collider_enabled` command application.
7. Update `WorldMetadataValidation` script binding policy for object collider markers.
8. Add deterministic tests for hook order, command order, errors, and repeatability.

## Non-Goals

Do not implement:

- arbitrary Lua access to object collider internals;
- Lua-owned collider creation/destruction;
- direct C++ pointer exposure;
- solid collision blockers;
- ECS entity callbacks;
- renderer/audio side effects;
- network replication.

## Required Tests

New `tests/scripting/ObjectColliderScriptSystem.cpp`:

1. `object_collider_enter_invokes_bound_script`
2. `object_collider_stay_and_exit_invoke_hooks`
3. `unbound_object_collider_event_is_ignored`
4. `script_errors_are_reported_without_crash`
5. `object_collider_script_event_fields_are_deterministic`
6. `event_order_is_deterministic`

Extend `tests/scripting/ScriptCommandProcessor.cpp`:

7. `object_collider_set_enabled_valid_event_applies`
8. `object_collider_set_enabled_missing_id_reports_invalid_field`
9. `object_collider_set_enabled_non_boolean_enabled_reports_invalid_field`
10. `object_collider_set_enabled_unknown_id_reports_not_found`
11. `object_collider_set_enabled_duplicate_or_ambiguous_policy_is_tested_if_relevant`

Extend `tests/scripting/ScriptedWorldSession.cpp`:

12. `scripted_object_collider_enter_can_disable_own_collider`
13. `latest_snapshot_does_not_replay_object_collider_scripts`
14. `repeat_run_produces_same_object_script_events_and_command_results`
15. `trigger_scripts_and_object_collider_scripts_have_documented_order`

Extend `tests/world/WorldMetadataValidation.cpp`:

16. `object_collider_script_binding_is_supported_after_phase_12d`
17. `sprite_or_portal_script_binding_still_warns_unsupported`

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_object_collider_script_system_test \
  stellar_script_command_processor_test \
  stellar_scripted_world_session_test \
  stellar_world_metadata_validation_test \
  -j$(nproc)
ctest --test-dir build -R '^(object_collider_script|script_command_processor|scripted_world_session|world_metadata_validation)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If the CTest name differs, use the exact name added in CMake.

## Acceptance Criteria

- Object collider events are script-visible through authoritative snapshots.
- Object collider scripts load from metadata bindings.
- Scripts emit primitive events only.
- Native C++ validates and applies `object_collider.set_enabled`.
- Invalid commands produce deterministic results, not crashes.
- Trigger script behavior remains unchanged.
- Lua remains isolated to `stellar_scripting`.
- Default CTest remains display-free.

---

# Phase 12E — Scripted Object Collider Smoke and Documentation

## Task Card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director`
- Type: integration smoke, docs, status update
- Depends on: Phases 12A-D
- Do not commit unless explicitly instructed by the user.

## Objective

Add a display-free authored integration smoke proving that object collider authoring, session integration, scripting, and command processing work together deterministically.

This should mirror the current `scripted_collision_smoke`, but for object colliders rather than static collision mesh blockers.

## Required Reading

```text
tests/integration/ScriptedCollisionSmoke.cpp
tests/integration/ScriptedPlayableWorldSmoke.cpp
docs/ImplementationStatus.md
docs/Design.md
CMakeLists.txt
```

## Recommended Smoke Scenario

Create a generated glTF fixture with:

- visible floor/room geometry;
- static collision floor/walls so movement is authoritative;
- `SPAWN_Player`;
- `COLLIDER_PickupGem` object collider marker;
- script binding on `COLLIDER_PickupGem`, for example:
  - `stellar.script = "scripts/pickup.lua"`
  - `stellar.table = "PickupGem"`

Lua script:

```lua
PickupGem = {}

function PickupGem.on_object_collider_enter(event)
  stellar.emit_event("object_collider.set_enabled", {
    id = event.collider_id,
    enabled = false
  })
  stellar.emit_event("gameplay.pickup_collected", {
    name = event.collider_name
  })
end
```

Expected behavior:

1. Player starts outside the object collider.
2. Movement enters the collider.
3. Object collider enter event is emitted in snapshot.
4. Script hook runs.
5. Script emits `object_collider.set_enabled`.
6. Native command disables the collider.
7. Subsequent ticks do not emit stay.
8. If lifecycle semantics queue exit, one deterministic exit appears according to documented policy.
9. Repeat run produces identical snapshots, script events, command results, and object-collider events.

## New Test Target

Add:

```text
tests/integration/ScriptedObjectColliderSmoke.cpp
CTest name: scripted_object_collider_smoke
```

CMake target:

```cmake
add_executable(stellar_scripted_object_collider_smoke_test
    tests/integration/ScriptedObjectColliderSmoke.cpp
)

target_include_directories(stellar_scripted_object_collider_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_scripted_object_collider_smoke_test PRIVATE
    stellar_import_gltf
    stellar_world
    stellar_server_core
    stellar_scripting
)

add_test(NAME scripted_object_collider_smoke
         COMMAND stellar_scripted_object_collider_smoke_test)
```

Only add this inside the existing `if(STELLAR_ENABLE_GLTF)` and `if(STELLAR_ENABLE_LUA_SCRIPTING)` blocks.

## Documentation Updates

Update `docs/ImplementationStatus.md`:

- Add Phase 12A-E completion notes.
- Include focused and full validation commands.
- Record whether full CTest passed.

Update `docs/Design.md`:

- Add object collider authoring convention under world metadata.
- Clarify object colliders are sensor/overlap gameplay volumes, not rigid bodies.
- Clarify object collider scripts are server-authoritative and native-validated.
- Keep solid moving blockers deferred unless Phase 12 explicitly added them.

Do not update `AGENTS.md` unless explicitly requested.

## Required Tests

1. `scripted_object_collider_smoke`
2. Full related targeted suite:
   - `object_collider`
   - `server_world_session`
   - `object_collider_script`
   - `scripted_world_session`
   - `script_command_processor`
   - `gltf_importer_regression`
   - `world_metadata_validation`
3. Full CTest with glTF and Lua enabled.

## Focused Validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_scripted_object_collider_smoke_test \
  stellar_object_collider_test \
  stellar_server_world_session_test \
  stellar_scripted_world_session_test \
  stellar_script_command_processor_test \
  stellar_import_gltf_regression \
  stellar_world_metadata_validation_test \
  -j$(nproc)
ctest --test-dir build -R '^(scripted_object_collider_smoke|object_collider|server_world_session|scripted_world_session|script_command_processor|gltf_importer_regression|world_metadata_validation)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- Authored glTF object collider marker imports correctly.
- Runtime object collider is built deterministically.
- `WorldSession` emits object collider events from authoritative movement.
- Scripted object collider hook runs.
- Native command disables the object collider.
- Repeat runs are deterministic.
- Docs and implementation status reflect the completed work.
- Default CTest remains display-free.

---

# Deferred Collision Work After Phase 12

These are important but should not be mixed into Phase 12 unless the user explicitly changes scope.

## Solid kinematic object blockers

Object collider sensors are not movement blockers. If moving platforms, moving doors, pushable props, or kinematic blockers become the next priority, create a separate Phase 13 plan.

Likely requirements:

- `ObjectColliderResponseMode::kSensor` vs `kSolid`;
- filtered solid object-collider query context;
- character movement against shape blockers;
- deterministic order when static mesh and object blockers both collide;
- careful tests for tunneling, step-up, floor support, and exits when blockers move/disable.

This is higher risk than sensor events because `CharacterController` currently queries static triangles through `CollisionWorld`, not arbitrary dynamic shapes.

## Jump and gameplay movement abilities

`MovementCommand::jump` is currently accepted as intent but not applied by the deterministic movement seam. This is a movement/gameplay phase, not a collision foundation phase. Implement later with `@kojima` design review if gameplay feel is in scope.

## Dynamic rigid bodies

Do not add rigid bodies until ECS ownership, serialization, server authority, and networking implications are concrete.

## Navmesh/pathfinding

Do not add navmesh/pathfinding until movement/collision gameplay needs require it.

## Renderer/debug collision views

Useful later, but not required for backend-neutral collision correctness. Keep default tests display-free.

---

# Recommended First PR / Branch Slice

If the implementation agent only does one slice next, do this:

> **Phase 12A + targeted tests only.**

Reason:

- It closes a real semantic gap in the object collider foundation.
- It is backend-neutral and low risk.
- It makes later `WorldSession` integration safe.
- It does not require glTF, Lua, or snapshot shape changes.

Suggested first-slice validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_object_collider_test -j$(nproc)
ctest --test-dir build -R '^object_collider$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

# Completion Notes Template for Agents

Append completion notes to the plan file copied into the repo, or to `docs/ImplementationStatus.md`, after each implemented phase.

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 12X — <phase name>.
- Public/API changes:
  - <new/changed public types and functions>
- Runtime behavior:
  - <behavior change summary>
- Determinism:
  - <ordering/tie/lifecycle policy>
- Tests added/updated:
  - <list files and test cases>
- Validation:
  - `<command>`
  - `<command>`
  - Result: <pass/fail and relevant notes>
- Deferred follow-up:
  - <explicit non-goals not implemented>
```

---

# Final Agent Checklist

Before marking any Phase 12 slice complete:

- [ ] New public APIs have Doxygen `@brief`.
- [ ] Existing tests still pass.
- [ ] New behavior has display-free unit or integration coverage.
- [ ] Server authority is preserved.
- [ ] Lua does not directly mutate C++ state.
- [ ] Imported assets remain immutable.
- [ ] Event ordering is documented and tested.
- [ ] Reset vs live mutation behavior is explicit and tested.
- [ ] `docs/ImplementationStatus.md` is updated if implementation was done.
- [ ] `docs/Design.md` is updated if authoring conventions changed.
- [ ] No GPU/display requirement was added to default CTest.
- [ ] No third-party physics engine or dynamic rigid body system was added.
