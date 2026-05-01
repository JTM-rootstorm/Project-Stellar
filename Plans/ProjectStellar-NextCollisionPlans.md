# Project Stellar Next Collision Plan Pack

<!-- README-NextCollisionRoadmap.md -->

# Project Stellar `collision-movement`: Next Collision Implementation Plan Pack

Prepared for implementation AI agents working in `JTM-rootstorm/Project-Stellar` on the
`collision-movement` branch.

Do not commit directly to the branch unless the user explicitly asks. These files are planning
handoffs only.

## Why these are the next steps

Current branch evidence from `Plans/Archived` and `docs/ImplementationStatus.md`:

- Phase 6A first pass is complete: glTF static collision extraction into backend-neutral
  `SceneAsset::level_collision`.
- Phase 6B first pass is complete: `CollisionWorld` supports segment raycasts, ground probing,
  and minimal static sphere movement.
- Phase 6C/6D first passes are complete: billboard data and world metadata markers exist.
- Phase 7A is complete: collision-only glTF nodes are extracted as collision and filtered from
  normal render scene membership.
- Phase 7B is complete: `CharacterController` exists and handles deterministic recovery,
  sweep/slide, ground snap, slope classification, and conservative step-up over static collision.
- Phase 7C is complete: `RuntimeWorld` assembles imported scene data, optional `CollisionWorld`,
  metadata, and diagnostics.
- Phase 7D is complete: `TriggerSystem` converts trigger markers into deterministic AABB triggers.
- Phase 7E is complete: `CollisionWorld` has an immutable BVH and query diagnostics.

Current code evidence checked on the branch:

- `include/stellar/physics/CollisionWorld.hpp` exposes `raycast`, `probe_ground`, `move_sphere`,
  immutable `asset()`, and `stats()`.
- `include/stellar/physics/CharacterController.hpp` keeps `height` as a reserved capsule value, but
  states that the current implementation uses a sphere at the supplied position.
- `src/physics/CharacterController.cpp` currently loops over `world.asset().meshes` and triangle
  vectors directly for recovery and slide movement. That means the robust controller does not yet
  benefit from the Phase 7E BVH.
- `include/stellar/world/RuntimeWorld.hpp` already exposes optional static collision and metadata
  lookup helpers.
- `include/stellar/world/TriggerSystem.hpp` already exposes deterministic trigger overlap events.
- `CMakeLists.txt` has libraries for `stellar_physics`, `stellar_world`, `stellar_client_config`,
  and `stellar-client`, but no real `stellar-server` target yet.

Design constraints from `docs/Design.md` and `AGENTS.md`:

- Server/gameplay must be authoritative for collision and movement.
- Client remains presentation-only unless prediction is explicitly scoped.
- Collision, world metadata, and movement logic must stay backend-neutral.
- Default tests must stay display-free.
- Do not add a full third-party physics engine in this slice.
- Do not add dynamic rigid bodies, navmesh/pathfinding, broad networking, scripts, or full ECS unless
  a specific phase below asks for a small seam.
- Public APIs need Doxygen `@brief` documentation.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

## Recommended phase order and parallel lanes

```text
Physics lane:
  Phase 8A -> Phase 8B

Runtime/server-authority lane:
  Phase 8C -> Phase 8E

Authoring/validation lane:
  Phase 8D

Optional diagnostics lane:
  Phase 8F
```

Safe parallelization:

- **Phase 8A and Phase 8D can run in parallel.** They touch different surfaces when Phase 8D keeps
  validation as a standalone API and does not reshape `CollisionAsset`.
- **Phase 8C can start in parallel with Phase 8A only if it treats `CharacterController` as a
  black-box dependency and does not edit physics internals.** Merge Phase 8A first if API changes
  are needed.
- **Phase 8F can run in parallel with Phase 8D after Phase 8A has landed.**
- **Do not run Phase 8A and Phase 8B in parallel.** Both touch `CharacterController` and collision
  query internals.
- **Do not run Phase 8C and Phase 8E in parallel.** Phase 8E depends on the authoritative movement
  seam created by Phase 8C.

## Plan files

1. `Phase8A-BroadphaseBackedCharacterQueries.md`
   - Make high-level character movement use BVH-backed collision candidates instead of brute-force
     iteration over every triangle.

2. `Phase8B-CapsuleCharacterController.md`
   - Activate `CharacterControllerConfig::height` and upgrade from sphere-only movement toward a
     conservative vertical capsule controller.

3. `Phase8C-AuthoritativeMovementSimulation.md`
   - Add a small server-authoritative movement simulation seam over `RuntimeWorld` and
     `CharacterController` without adding full networking or ECS.

4. `Phase8D-CollisionAuthoringValidation.md`
   - Add deterministic collision asset validation and authoring diagnostics for glTF-derived
     collision data.

5. `Phase8E-MovementTriggerIntegration.md`
   - Connect authoritative movement results to runtime trigger overlap events without scripts/ECS.

6. `Phase8F-CollisionPerformanceRegression.md`
   - Add display-free performance/regression coverage for collision query pruning and movement
     scalability.

## Global validation baseline

Run this at the end of each phase unless the phase provides a narrower focused subset first:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Keep OpenGL/Vulkan context tests opt-in only.

---

<!-- Phase8A-BroadphaseBackedCharacterQueries.md -->

# Phase 8A: Broadphase-Backed Character Controller Queries

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: physics/collision correctness + scalability
- Depends on: Phase 7B and Phase 7E completion
- Can run in parallel with: Phase 8D
- Conditional parallel with: Phase 8C, only if Phase 8C treats physics APIs as black-box
- Must not run in parallel with: Phase 8B
- Do not commit unless explicitly instructed by the user.

## Objective

Make robust high-level character movement benefit from the Phase 7E static BVH without changing
observable collision behavior.

The current `CollisionWorld` has a BVH, but `src/physics/CharacterController.cpp` still iterates
directly over `world.asset().meshes` and every triangle for recovery and slide movement. This phase
adds a small candidate-query seam and refactors character movement to query only relevant triangles
while preserving deterministic results.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/Archived/Phase7B-RobustStaticCharacterMovement.md`
- `Plans/Archived/Phase7E-CollisionBroadphaseAndDiagnostics.md`
- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `include/stellar/physics/CharacterController.hpp`
- `src/physics/CharacterController.cpp`
- `tests/physics/CollisionWorld.cpp`
- `tests/physics/CharacterController.cpp`
- `CMakeLists.txt`

## Scope

Implement a narrow, deterministic way for higher-level collision code to request BVH-pruned triangle
candidates.

Preferred public API shape:

```cpp
namespace stellar::physics {

/** @brief Axis-aligned query bounds in world space. */
struct CollisionQueryAabb {
    std::array<float, 3> min{};
    std::array<float, 3> max{};
};

/** @brief Stable triangle index returned by a static collision candidate query. */
struct CollisionTriangleCandidate {
    std::size_t mesh_index = 0;
    std::size_t triangle_index = 0;
};

/** @brief Return static collision triangles whose bounds intersect the supplied AABB. */
[[nodiscard]] std::vector<CollisionTriangleCandidate>
CollisionWorld::query_triangles(CollisionQueryAabb bounds) const;
}
```

Acceptable alternative: keep the query helper internal if `CharacterController` and `CollisionWorld`
can share it cleanly without exposing broadphase internals. Do not expose the BVH node format.

## Implementation steps

1. Add a small query-bounds/candidate representation.
   - Keep it backend-neutral.
   - Use Doxygen `@brief` on public types/functions.
   - Return stable mesh/triangle indices, not raw renderer handles.
   - Sort or emit candidates in deterministic mesh/triangle order.

2. Route AABB candidate queries through the existing BVH.
   - Empty worlds return an empty vector.
   - Degenerate or non-finite AABBs return no candidates or are sanitized predictably.
   - Preserve `CollisionWorld::stats()` semantics.
   - If diagnostics are expanded, add fields without removing existing ones.

3. Refactor `CharacterController` recovery.
   - Build an AABB around the current character sphere/capsule approximation.
   - Query candidate triangles.
   - Iterate candidates in mesh/triangle order.
   - Preserve existing recovery behavior and result flags.

4. Refactor `CharacterController` slide/sweep.
   - Build a swept AABB from start, displacement, and radius.
   - Query candidate triangles for each fixed slide iteration.
   - Preserve fixed iteration limits, finite output guards, slope behavior, ground snap, and step-up.

5. Keep `CollisionWorld::asset()` available.
   - Existing tests and future systems may still need immutable asset access.
   - Do not make callers depend on BVH internals.

6. Update diagnostics cautiously.
   - Add a way to prove character queries prune candidates, either through existing stats or a new
     display-free diagnostic.
   - Avoid brittle tests that depend on exact BVH node counts unless that layout is intentionally
     stable.

## Non-goals

Do not implement:

- Capsule height behavior. That is Phase 8B.
- Dynamic bodies or dynamic broadphase updates.
- Third-party physics.
- Networking, ECS, or server movement.
- Debug rendering.
- Any OpenGL/Vulkan context requirement.

## Required tests

Update or add display-free tests.

Add to `tests/physics/CollisionWorld.cpp`:

1. `query_triangles_empty_world_returns_empty`
2. `query_triangles_returns_only_intersecting_bounds`
3. `query_triangles_order_is_deterministic`
4. `query_triangles_handles_degenerate_bounds`

Add to `tests/physics/CharacterController.cpp`:

5. Existing character controller tests still pass.
6. Character movement through a many-triangle world returns the same position as a brute-force helper.
7. Character movement reports fewer narrow/candidate triangle tests in a prunable many-triangle scene.
8. Degenerate triangles remain finite and do not crash candidate queries.
9. Step-up and ground snap still work after candidate pruning.

## CMake guidance

If no new test target is needed, only existing test sources should change.

If a new focused target is easier to maintain:

```cmake
add_executable(stellar_collision_query_candidates_test
    tests/physics/CollisionQueryCandidates.cpp
)

target_include_directories(stellar_collision_query_candidates_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_collision_query_candidates_test PRIVATE
    stellar_physics
)

add_test(NAME collision_query_candidates COMMAND stellar_collision_query_candidates_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_world_test stellar_character_controller_test -j$(nproc)
ctest --test-dir build -R '^(collision_world|character_controller)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- `CharacterController` no longer brute-force scans all triangles for normal recovery and movement
  when a BVH candidate query can be used.
- Observable movement behavior remains deterministic and compatible with current tests.
- Candidate query order is stable.
- Large synthetic worlds show pruned candidate/narrowphase counts in tests.
- Public APIs have Doxygen `@brief`.
- Default CTest remains display-free.
- No third-party physics engine is added.

## Completion notes template

Append to this plan after implementation if the plan is copied into the repo:

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8A broadphase-backed character queries.
- Public/API changes: <describe candidate query API or internal helper>.
- Character controller changes: <describe recovery/sweep candidate usage>.
- Determinism: <describe ordering/tie behavior>.
- Diagnostics: <describe stats/candidate-count behavior>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Capsule height behavior remains Phase 8B.
  - Authoritative server movement remains Phase 8C.
```

---

<!-- Phase8B-CapsuleCharacterController.md -->

# Phase 8B: Conservative Capsule Character Controller

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Secondary design review: `@kojima` for gameplay feel only, through `@director` if routing is used
- Type: physics/collision correctness
- Depends on: Phase 8A preferred; Phase 7B minimum
- Can run in parallel with: Phase 8D
- Must not run in parallel with: Phase 8A
- Conditional parallel with: Phase 8C if Phase 8C treats `CharacterController` as black-box and
  does not assert sphere-specific behavior
- Do not commit unless explicitly instructed by the user.

## Objective

Activate `CharacterControllerConfig::height` and upgrade movement from sphere-only behavior to a
conservative vertical capsule approximation suitable for an actual player body.

The current public header reserves `height`, but states the implementation uses a sphere. This phase
makes the height meaningful while preserving the no-physics-engine, deterministic, static-world
design.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/Archived/Phase7B-RobustStaticCharacterMovement.md`
- `include/stellar/physics/CharacterController.hpp`
- `src/physics/CharacterController.cpp`
- `include/stellar/physics/CollisionWorld.hpp`
- `src/physics/CollisionWorld.cpp`
- `tests/physics/CharacterController.cpp`
- `tests/physics/CollisionWorld.cpp`
- `CMakeLists.txt`

## Scope

Implement capsule-aware collision for the existing `CharacterController` API.

Recommended interpretation:

- `position` remains the character center.
- `up` defines the capsule vertical axis.
- `radius` defines the capsule radius.
- `height` defines total capsule height, including hemispherical ends.
- Clamp effective height to at least `2 * radius`.
- Capsule segment endpoints are:
  - `center - up * max(0, height * 0.5 - radius)`
  - `center + up * max(0, height * 0.5 - radius)`

Keep `CharacterMoveInput` and `CharacterMoveResult` source-compatible unless a small documented API
addition is unavoidable.

## Implementation steps

1. Add internal capsule helpers.
   - `effective_capsule_half_segment(config, up)`
   - `capsule_endpoints(center, up, radius, height)`
   - closest point between capsule segment and triangle.
   - capsule-vs-triangle overlap.
   - conservative capsule sweep against candidate triangles.

2. Preserve sphere compatibility.
   - When `height <= 2 * radius + epsilon`, behavior should match the old sphere path closely.
   - Existing tests should not need broad rewrites.

3. Update start-overlap recovery.
   - Detect floor, wall, and ceiling penetration using the capsule, not only the center sphere.
   - Push out using stable deterministic normals.
   - Keep fixed recovery iterations and finite-output guards.

4. Update sweep/slide.
   - Use capsule overlap/sweep candidates.
   - Maintain fixed maximum iterations.
   - Preserve slide projection and deterministic tie handling.
   - If exact continuous capsule-triangle sweep is too large, use a conservative sampled/refined
     method and document it clearly.

5. Update ground snap and step-up.
   - Ground snap should use the lower capsule end, not the center sphere.
   - Step-up should validate that the capsule can occupy the raised and landed positions.
   - Do not let step-up pass through low ceilings.

6. Keep no-op behavior for missing collision world.
   - `world_ == nullptr` still applies displacement unchanged.

## Non-goals

Do not implement:

- Dynamic rigid bodies.
- Mass, forces, friction simulation, or impulses.
- Crouch/prone state machines.
- Arbitrary collider shapes.
- Full exact continuous collision detection if a conservative deterministic method is safer.
- Networking, ECS, animation, or rendering.
- Third-party physics.

## Required tests

Extend `tests/physics/CharacterController.cpp`.

Required coverage:

1. Existing tests still pass with default config.
2. `height_equal_to_two_radii_matches_sphere_like_behavior`
3. `capsule_stands_on_floor_with_center_above_ground`
4. `capsule_wall_collision_uses_body_height_not_only_center`
5. `capsule_ceiling_blocks_upward_movement`
6. `capsule_start_overlap_with_ceiling_recovers_down_or_stably_out`
7. `capsule_ground_snap_uses_bottom_endpoint`
8. `capsule_step_up_does_not_enter_low_ceiling`
9. `capsule_too_tall_for_tunnel_is_blocked`
10. `capsule_degenerate_height_and_radius_remain_finite`
11. `capsule_slide_into_corner_stops_cleanly`

Use synthetic triangle scenes only; no display/GPU tests.

## CMake guidance

No new target is required unless the existing test file becomes too large. If adding a target, keep it
linked only against `stellar_physics`.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_world_test stellar_character_controller_test -j$(nproc)
ctest --test-dir build -R '^(collision_world|character_controller)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- `CharacterControllerConfig::height` affects collision as a vertical capsule.
- Existing sphere-like behavior is preserved when height collapses to a sphere.
- Floor, wall, ceiling, slope, snap, and step-up behavior remain deterministic.
- Tests cover capsule-specific failures that a center sphere would miss.
- Public docs no longer state that height is merely reserved, unless an explicit fallback remains.
- Default CTest remains display-free.
- No third-party physics engine is added.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8B conservative capsule character controller.
- Shape behavior: <describe radius/height/up interpretation>.
- Movement behavior: <describe overlap, sweep, snap, step, ceiling handling>.
- Compatibility: <describe sphere-like fallback/old test preservation>.
- Limitations: <document conservative sampling or remaining non-goals>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Server-authoritative movement remains Phase 8C if not complete.
  - Performance regression coverage remains Phase 8F if not complete.
```

---

<!-- Phase8C-AuthoritativeMovementSimulation.md -->

# Phase 8C: Server-Authoritative Movement Simulation Seam

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Gameplay review: `@kojima` only through `@director` if tuning decisions are needed
- Type: backend-neutral runtime/gameplay integration
- Depends on: Phase 7C and Phase 7B; Phase 8A/8B preferred but not mandatory
- Can run in parallel with: Phase 8D
- Conditional parallel with: Phase 8A/8B only if this phase treats `CharacterController` as a
  black-box dependency and does not edit physics internals
- Must not run in parallel with: Phase 8E
- Do not commit unless explicitly instructed by the user.

## Objective

Add a small, deterministic, backend-neutral movement simulation seam that represents the
server-authoritative use of collision and character movement.

The design says the server owns game state, game rules, physics/collision decisions, and validation.
The repository does not yet have a real `stellar-server` target. This phase should create the smallest
useful server-side movement library without adding networking or a full ECS.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `AGENTS.md`
- `include/stellar/world/RuntimeWorld.hpp`
- `src/world/RuntimeWorld.cpp`
- `include/stellar/physics/CharacterController.hpp`
- `include/stellar/physics/CollisionWorld.hpp`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `tests/world/RuntimeWorld.cpp`
- `tests/physics/CharacterController.cpp`
- `CMakeLists.txt`

## Scope

Create a backend-neutral movement simulation module.

Preferred new files:

- `include/stellar/server/MovementSimulation.hpp`
- `src/server/MovementSimulation.cpp`
- `tests/server/MovementSimulation.cpp`

Preferred CMake target:

- `stellar_server_core` library
- `stellar_server_movement_simulation_test`
- CTest name: `server_movement_simulation`

If the project owner prefers avoiding `server/` until a server executable exists, use
`include/stellar/gameplay/MovementSimulation.hpp` and `stellar_gameplay`. Keep the semantics
server-authoritative either way.

## Recommended public API

Keep data plain and serializable.

```cpp
namespace stellar::server {

/** @brief Tunable deterministic movement settings owned by authoritative simulation. */
struct MovementSimulationConfig {
    float max_speed = 6.0F;
    float acceleration = 40.0F;
    float gravity = 24.0F;
    float terminal_fall_speed = 50.0F;
    float fixed_dt = 1.0F / 60.0F;
    stellar::physics::CharacterControllerConfig character{};
};

/** @brief Client-requested movement input after server-side validation. */
struct MovementCommand {
    std::array<float, 3> wish_direction{};
    bool jump = false;
};

/** @brief Authoritative kinematic character state. */
struct MovementState {
    std::array<float, 3> position{};
    std::array<float, 3> velocity{};
    bool grounded = false;
};

/** @brief Result of one authoritative movement tick. */
struct MovementTickResult {
    MovementState state{};
    stellar::physics::CharacterMoveResult collision{};
    bool command_was_sanitized = false;
};

/** @brief Initialize movement state from the first player spawn, or origin if absent. */
[[nodiscard]] MovementState make_spawn_movement_state(const stellar::world::RuntimeWorld& world);

/** @brief Advance authoritative movement by one fixed tick. */
[[nodiscard]] MovementTickResult simulate_movement_tick(
    const stellar::world::RuntimeWorld& world,
    const MovementState& previous,
    const MovementCommand& command,
    const MovementSimulationConfig& config) noexcept;

} // namespace stellar::server
```

Exact names can differ, but preserve these principles:

- The movement state is authoritative, plain data.
- Inputs are requests that are sanitized/clamped.
- Collision is queried from `RuntimeWorld::collision_world` when present.
- Empty/no-collision worlds move deterministically without crashing.
- No renderer/audio handles enter state.

## Implementation steps

1. Create a minimal movement library.
   - Add source/header/test files.
   - Link against `stellar_world` and `stellar_physics`.
   - Do not link graphics/audio/platform unless unavoidable.

2. Add spawn initialization.
   - Use `stellar::world::find_player_spawn(world)`.
   - If missing, return origin and optionally a deterministic warning in tests.
   - Do not mutate `RuntimeWorld`.

3. Add input sanitization.
   - Replace NaN/Inf with zero.
   - Clamp wish direction length to 1.
   - Clamp speed/acceleration/gravity/fixed_dt to safe finite ranges.
   - Record `command_was_sanitized` when input was modified.

4. Add fixed-tick velocity update.
   - Accelerate toward requested horizontal direction.
   - Apply gravity when not grounded.
   - Clamp terminal fall speed.
   - Keep jump behavior disabled or trivial unless a small deterministic jump can be tested clearly.
   - Use a stable up vector `{0, 1, 0}` unless Phase 8B already supports arbitrary `up`.

5. Call `CharacterController`.
   - Build a `CharacterMoveInput` from previous position and tick displacement.
   - Use configured character shape.
   - If no collision world exists, apply displacement directly and set grounded false unless a prior
     state policy says otherwise.
   - Update state from `CharacterMoveResult`.

6. Preserve server authority.
   - Do not trust client-provided position, velocity, or grounded state.
   - The command contains intent only.

## Non-goals

Do not implement:

- Full ECS/component storage.
- Network transport or client prediction.
- Animation, rendering, audio, or camera.
- Dynamic bodies or rigid-body physics.
- Game-specific abilities beyond minimal movement.
- Trigger events. That is Phase 8E.
- Saving/loading snapshots unless a tiny plain-data helper is unavoidable.

## Required tests

Create `tests/server/MovementSimulation.cpp` or equivalent.

Required coverage:

1. `spawn_state_uses_player_spawn_marker`
2. `spawn_state_defaults_to_origin_without_player_spawn`
3. `empty_world_moves_without_collision`
4. `floor_world_applies_gravity_and_becomes_grounded`
5. `wall_world_blocks_authoritative_movement`
6. `wish_direction_is_clamped`
7. `nan_input_is_sanitized`
8. `terminal_fall_speed_is_clamped`
9. `client_position_is_not_part_of_command`
10. `runtime_world_collision_optional_is_handled`
11. `fixed_dt_repeatability_same_inputs_same_outputs`

Use synthetic `SceneAsset`/`RuntimeWorld` construction where possible.

## CMake guidance

Preferred CMake addition:

```cmake
add_library(stellar_server_core
    src/server/MovementSimulation.cpp
)

target_include_directories(stellar_server_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_server_core PUBLIC
    stellar_world
    stellar_physics
)

add_executable(stellar_server_movement_simulation_test
    tests/server/MovementSimulation.cpp
)

target_include_directories(stellar_server_movement_simulation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_server_movement_simulation_test PRIVATE
    stellar_server_core
)

add_test(NAME server_movement_simulation COMMAND stellar_server_movement_simulation_test)
```

Do not add a `stellar-server` executable in this phase unless the user explicitly wants it.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_server_movement_simulation_test -j$(nproc)
ctest --test-dir build -R '^server_movement_simulation$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- A backend-neutral authoritative movement seam exists.
- Movement uses `RuntimeWorld` collision when present.
- Client input is treated as intent and sanitized.
- The state/output is plain, serializable data.
- Tests cover spawn, gravity, wall collision, optional collision, and input validation.
- No graphics/audio/platform dependency is introduced.
- Default CTest remains display-free.
- No full ECS, networking, or third-party physics is added.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8C server-authoritative movement simulation seam.
- Public API: <describe config/command/state/result>.
- Collision integration: <describe RuntimeWorld/CharacterController use>.
- Authority validation: <describe input sanitization and client-trust boundaries>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Trigger event handoff remains Phase 8E.
  - Full ECS/network integration remains future work.
```

---

<!-- Phase8D-CollisionAuthoringValidation.md -->

# Phase 8D: Collision Authoring Validation and Diagnostics

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` if importer/world diagnostics need policy decisions
- Type: asset/import/runtime validation
- Depends on: Phase 6A and Phase 7C
- Can run in parallel with: Phase 8A, Phase 8B, or Phase 8C if this phase avoids changing
  `CollisionAsset` layout
- Must not run in parallel with another task that reshapes `LevelCollisionAsset`
- Do not commit unless explicitly instructed by the user.

## Objective

Add deterministic validation and diagnostics for glTF-derived static collision so authored levels can
fail clearly or warn predictably before collision bugs become movement bugs.

Current collision extraction and runtime assembly exist, but there is no standalone validation layer
for malformed collision triangles, suspicious bounds, missing playable collision, or convention
mistakes beyond existing importer regressions.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/Archived/Phase6A-LevelCollisionExtraction.md`
- `Plans/Archived/Phase7A-StatusAlignment-RenderFiltering-Hardening.md`
- `Plans/Archived/Phase7C-RuntimeWorldAssembly.md`
- `include/stellar/assets/CollisionAsset.hpp`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/world/RuntimeWorld.hpp`
- `src/import/gltf/CollisionImport.cpp`
- `src/import/gltf/SceneImport.cpp`
- `src/world/RuntimeWorld.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/world/RuntimeWorld.cpp`
- `CMakeLists.txt`

## Scope

Add a standalone collision validation API that can be used by import tests, runtime world diagnostics,
and future tools.

Preferred new files:

- `include/stellar/assets/CollisionValidation.hpp` or `include/stellar/world/CollisionValidation.hpp`
- `src/world/CollisionValidation.cpp` if placed under world
- `tests/world/CollisionValidation.cpp`

Recommended API:

```cpp
namespace stellar::world {

/** @brief Severity for authored collision validation messages. */
enum class CollisionValidationSeverity {
    kWarning,
    kError,
};

/** @brief One deterministic collision validation finding. */
struct CollisionValidationFinding {
    CollisionValidationSeverity severity = CollisionValidationSeverity::kWarning;
    std::string code;
    std::string message;
    std::size_t mesh_index = 0;
    std::size_t triangle_index = 0;
};

/** @brief Summary of static collision validation results. */
struct CollisionValidationReport {
    std::vector<CollisionValidationFinding> findings;
    bool has_errors = false;
};

/** @brief Validate backend-neutral static collision data for authoring and runtime use. */
[[nodiscard]] CollisionValidationReport validate_level_collision(
    const stellar::assets::LevelCollisionAsset& collision) noexcept;

} // namespace stellar::world
```

If `noexcept` is not practical due to string allocation, omit it and keep behavior deterministic.

## Validation checks

Implement checks in small, testable helpers.

Required checks:

1. Non-finite triangle vertex values.
2. Non-finite or near-zero normals.
3. Zero-area or near-zero-area triangles.
4. Mesh bounds that do not contain triangle vertices.
5. Aggregate bounds that do not contain mesh bounds.
6. Empty `LevelCollisionAsset` should produce a warning, not an error.
7. Mesh with no triangles should produce a warning.
8. Extremely large bounds should produce a warning with a documented threshold.
9. Duplicate collision mesh names should produce a warning.
10. Downward-only or no walkable upward-facing surfaces should produce a warning for playable worlds.

Optional checks if cheap:

- Triangle winding inconsistent with normal.
- Excessively tiny triangles below a threshold.
- Suspicious trigger/collision naming overlap discovered during import regression tests.

## RuntimeWorld integration

Keep the first implementation standalone to remain parallel-safe.

Optional after tests pass:

- Append validation warning strings into `RuntimeWorldDiagnostics::warnings`.
- Do not change the `RuntimeWorld` public shape unless necessary.
- Do not make every validation warning fatal for runtime assembly.

## Importer integration

Do not reject glTF files only because validation warnings exist.

Only convert to hard importer errors when:

- Collision-marked geometry cannot produce triangle data.
- Existing importer policy already treats the condition as fatal.
- The validation issue is unsafe/non-finite and would poison runtime queries.

## Non-goals

Do not implement:

- New glTF extensions.
- A JSON schema.
- Editor UI.
- Runtime debug rendering.
- Surface materials/friction gameplay.
- Dynamic collision.
- A full content pipeline.

## Required tests

Create `tests/world/CollisionValidation.cpp` or add to `tests/world/RuntimeWorld.cpp` if small.

Required coverage:

1. Empty collision asset returns warning but no error.
2. Valid floor mesh returns no findings.
3. Zero-area triangle returns error or documented warning.
4. Non-finite vertex returns error.
5. Non-finite normal returns finding.
6. Mesh bounds mismatch returns finding.
7. Aggregate bounds mismatch returns finding.
8. Duplicate mesh names return deterministic warnings.
9. No walkable surfaces returns warning.
10. Findings are emitted in deterministic mesh/triangle/code order.

Add importer regression only if it proves convention mistakes that direct asset tests cannot cover.

## CMake guidance

If standalone target:

```cmake
add_executable(stellar_collision_validation_test
    tests/world/CollisionValidation.cpp
)

target_include_directories(stellar_collision_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_collision_validation_test PRIVATE
    stellar_world
)

add_test(NAME collision_validation COMMAND stellar_collision_validation_test)
```

If `CollisionValidation.cpp` is added under `src/world`, include it in `stellar_world`.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_validation_test -j$(nproc)
ctest --test-dir build -R '^collision_validation$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If no standalone target is added, adjust the focused build/test names and record the actual commands.

## Acceptance criteria

- Static collision validation exists as backend-neutral code.
- Findings are deterministic and display-free.
- Validation can distinguish warnings from errors.
- Runtime/import behavior does not become overly strict.
- No collision asset layout change is required.
- Default CTest remains display-free.
- Public APIs have Doxygen `@brief`.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8D collision authoring validation and diagnostics.
- Public API: <describe report/finding/severity>.
- Checks: <summarize implemented validation checks>.
- Runtime/import integration: <describe standalone vs RuntimeWorld warning integration>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Surface gameplay/material metadata remains future work.
  - Editor/tool UI remains future work.
```

---

<!-- Phase8E-MovementTriggerIntegration.md -->

# Phase 8E: Authoritative Movement and Trigger Event Integration

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Gameplay review: `@kojima` through `@director` if event semantics need tuning
- Type: runtime/gameplay integration
- Depends on: Phase 7D and Phase 8C
- Can run in parallel with: Phase 8F after Phase 8C lands
- Must not run in parallel with: Phase 8C
- Do not commit unless explicitly instructed by the user.

## Objective

Connect authoritative movement state to runtime trigger overlap events in a backend-neutral way.

Phase 7D created `TriggerSystem`, and Phase 8C creates an authoritative movement seam. This phase
lets gameplay/server code update trigger events from movement results without adding scripts, ECS,
networking, rendering, or audio.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/Archived/Phase7D-RuntimeTriggersAndMetadataHooks.md`
- `include/stellar/world/TriggerSystem.hpp`
- `src/world/TriggerSystem.cpp`
- `include/stellar/world/RuntimeWorld.hpp`
- `include/stellar/physics/CharacterController.hpp`
- Phase 8C implementation files
- `tests/world/TriggerSystem.cpp`
- Phase 8C movement tests
- `CMakeLists.txt`

## Scope

Add a small module that owns trigger state for an authoritative moving character.

Preferred files:

- `include/stellar/server/MovementTriggerIntegration.hpp`
- `src/server/MovementTriggerIntegration.cpp`
- `tests/server/MovementTriggerIntegration.cpp`

If Phase 8C used `stellar::gameplay`, mirror that namespace instead.

Recommended API:

```cpp
namespace stellar::server {

/** @brief Authoritative trigger event associated with a movement tick. */
struct MovementTriggerEvent {
    std::string trigger_name;
    bool entered = false;
    bool stayed = false;
    bool exited = false;
};

/** @brief Stateful trigger tracker for one authoritative moving character. */
class MovementTriggerTracker {
public:
    /** @brief Build trigger volumes from the runtime world and reset overlap state. */
    void reset_from_world(const stellar::world::RuntimeWorld& world);

    /** @brief Update trigger state from an authoritative character position/radius. */
    [[nodiscard]] std::vector<MovementTriggerEvent> update(
        std::array<float, 3> position,
        float radius) noexcept;
};

} // namespace stellar::server
```

Alternative: add a free function that combines `TriggerSystem` with movement state if a class is
unnecessary. Keep ownership and reset semantics clear.

## Implementation steps

1. Build trigger volumes from `RuntimeWorld`.
   - Use existing `build_trigger_volumes(world)`.
   - Keep trigger state outside `RuntimeWorld`.
   - Deterministic ordering should follow `TriggerSystem`.

2. Connect movement output.
   - Use authoritative final position from Phase 8C.
   - Use `CharacterControllerConfig::radius`; if Phase 8B capsule is complete, document whether
     trigger tests use capsule radius at center or a future capsule-trigger shape.
   - Do not trust client position.

3. Translate trigger overlaps.
   - Convert `TriggerOverlap` to server/gameplay event structs.
   - Preserve enter/stay/exit semantics.
   - Keep strings stable.

4. Add optional one-tick helper.
   - A helper may call `simulate_movement_tick` and then update triggers.
   - Keep it explicit to avoid hidden state.

5. Keep future audio/render hooks out.
   - Server may emit events later, but this phase only returns plain data.

## Non-goals

Do not implement:

- Script execution or callbacks.
- ECS event bus.
- Network replication.
- Audio playback or VFX.
- Portal behavior.
- Rotated trigger boxes.
- Collision response to triggers.
- Dynamic trigger volumes.

## Required tests

Create `tests/server/MovementTriggerIntegration.cpp` or equivalent.

Required coverage:

1. `reset_from_world_builds_trigger_volumes`
2. `moving_into_trigger_emits_enter`
3. `remaining_inside_trigger_emits_stay`
4. `moving_out_of_trigger_emits_exit`
5. `multiple_triggers_emit_deterministic_order`
6. `reset_clears_previous_overlap_state`
7. `world_without_triggers_emits_no_events`
8. `movement_tick_then_trigger_update_uses_authoritative_final_position`
9. `trigger_update_uses_radius_from_config_or_documented_policy`
10. `client_input_cannot_directly_trigger_without_authoritative_movement`

## CMake guidance

Add source to the same library created in Phase 8C, such as `stellar_server_core`.

Preferred test target:

```cmake
add_executable(stellar_movement_trigger_integration_test
    tests/server/MovementTriggerIntegration.cpp
)

target_include_directories(stellar_movement_trigger_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_movement_trigger_integration_test PRIVATE
    stellar_server_core
)

add_test(NAME movement_trigger_integration COMMAND stellar_movement_trigger_integration_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_movement_trigger_integration_test -j$(nproc)
ctest --test-dir build -R '^movement_trigger_integration$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Authoritative movement can drive trigger enter/stay/exit events.
- Trigger state is explicit and resettable.
- RuntimeWorld remains a passive assembled data object.
- No scripts, ECS, networking, rendering, or audio are added.
- Tests are deterministic and display-free.
- Public APIs have Doxygen `@brief`.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8E movement-trigger integration.
- Public API: <describe tracker/event types>.
- Runtime behavior: <describe reset/update semantics and ordering>.
- Authority: <describe use of authoritative final position>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - ECS event bus and gameplay callbacks remain future work.
  - Network replication remains future work.
  - Audio/VFX presentation remains future work.
```

---

<!-- Phase8F-CollisionPerformanceRegression.md -->

# Phase 8F: Collision Performance Regression and Diagnostics Harness

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: tests/diagnostics/performance guardrails
- Depends on: Phase 8A for character candidate pruning; Phase 7E minimum for `CollisionWorld` stats
- Can run in parallel with: Phase 8D; Phase 8E after Phase 8C lands
- Must not run in parallel with: Phase 8A before the candidate API shape is known
- Do not commit unless explicitly instructed by the user.

## Objective

Add deterministic, display-free regression coverage that prevents future collision changes from
silently falling back to brute-force query behavior or degrading movement scalability.

This is not a micro-optimization phase. It creates guardrails and diagnostics that AI agents can run
while changing collision code.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `Plans/Archived/Phase7E-CollisionBroadphaseAndDiagnostics.md`
- Phase 8A implementation notes if available
- `include/stellar/physics/CollisionWorld.hpp`
- `include/stellar/physics/CharacterController.hpp`
- `tests/physics/CollisionWorld.cpp`
- `tests/physics/CharacterController.cpp`
- `CMakeLists.txt`

## Scope

Add synthetic collision scenes and tests that verify pruning, determinism, and finite outputs.

Preferred new file:

- `tests/physics/CollisionPerformanceRegression.cpp`

Preferred target:

- `stellar_collision_performance_regression_test`
- CTest name: `collision_performance_regression`

These should be deterministic unit/regression tests, not timing-sensitive benchmarks.

## Implementation steps

1. Add reusable synthetic scene builders.
   - Grid floor with many triangles.
   - Distant triangle cloud outside query bounds.
   - Corridor/wall scene.
   - Degenerate triangle set.
   - Optional stair scene.

2. Verify raycast pruning.
   - Query through a local region in a large scene.
   - Assert `last_query_triangle_tests` is far below total triangle count.
   - Use ratios with generous thresholds, not exact numbers.

3. Verify movement pruning.
   - Use `CharacterController` after Phase 8A.
   - Assert movement results are correct and candidate/narrowphase work is pruned.
   - Avoid wall-clock timing assertions.

4. Verify determinism.
   - Run identical queries multiple times.
   - Assert identical hit triangle indices, positions within tolerance, and result flags.
   - Build scenes with intentionally equal-distance hits to preserve tie rules.

5. Add optional diagnostic string helpers only if useful.
   - Keep public diagnostics small.
   - Do not expose BVH internals.

## Non-goals

Do not implement:

- Timing benchmarks that fail on slow machines.
- Profiling UI.
- GPU queries.
- SIMD/multithreaded optimization.
- Dynamic broadphase updates.
- New physics features.

## Required tests

Required coverage:

1. `raycast_large_scene_prunes_triangle_tests`
2. `raycast_equal_hits_remain_deterministic`
3. `character_move_large_scene_prunes_candidate_tests`
4. `character_move_result_stable_across_repeated_runs`
5. `degenerate_triangle_cloud_does_not_poison_stats`
6. `empty_world_stats_are_stable`
7. `single_triangle_world_stats_are_stable`
8. `distant_geometry_does_not_affect_local_movement_result`

Use count thresholds such as:

- `last_query_triangle_tests < total_triangle_count / 4` for obviously prunable scenes.
- Never assert exact BVH node counts unless the builder intentionally guarantees them.

## CMake guidance

```cmake
add_executable(stellar_collision_performance_regression_test
    tests/physics/CollisionPerformanceRegression.cpp
)

target_include_directories(stellar_collision_performance_regression_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_collision_performance_regression_test PRIVATE
    stellar_physics
)

add_test(NAME collision_performance_regression COMMAND stellar_collision_performance_regression_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_collision_performance_regression_test -j$(nproc)
ctest --test-dir build -R '^collision_performance_regression$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Large synthetic scenes prove broadphase/candidate pruning without timing assumptions.
- Repeated collision and movement queries are deterministic.
- Degenerate input remains finite and stable.
- Tests are display-free.
- Public diagnostics remain small and backend-neutral.
- No new physics feature scope is added.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 8F collision performance regression and diagnostics harness.
- Test scenes: <describe synthetic builders>.
- Pruning checks: <describe thresholds and stats used>.
- Determinism checks: <describe repeated/equal-hit behavior>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Real profiling benchmark target remains future work.
  - Dynamic broadphase remains future work.
```
