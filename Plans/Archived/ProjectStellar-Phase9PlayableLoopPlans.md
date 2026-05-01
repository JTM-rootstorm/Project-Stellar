# Project Stellar Phase 9 Playable Loop Plan Pack


<!-- README-Phase9PlayableLoopRoadmap.md -->

# Project Stellar `collision-movement`: Phase 9 Playable Loop Plan Pack

Prepared for implementation AI agents working in `JTM-rootstorm/Project-Stellar` on the
`collision-movement` branch.

Do not commit directly to the branch unless the user explicitly asks. These files are planning
handoffs only.

## Current evidence and branch posture

`docs/ImplementationStatus.md` now reports Phase 8A through Phase 8F complete. Do not restart Phase
6, 7, or 8 work.

What Phase 8 added or confirmed:

- `CollisionWorld::query_triangles` now exposes deterministic BVH-backed static triangle candidates.
- `CharacterControllerConfig::height` is active as a vertical capsule height, with sphere-like
  behavior when height collapses to two radii.
- `stellar_server_core` exists and provides a backend-neutral authoritative movement simulation seam.
- `MovementTriggerTracker` and `simulate_movement_tick_and_update_triggers` connect authoritative
  movement output to trigger events.
- `validate_level_collision` provides static collision authoring diagnostics.
- Collision performance regression coverage exists for BVH/candidate pruning and deterministic
  movement stability.

Important current gaps:

- `stellar_server_core` is still a library, not a running world/session abstraction or server
  executable.
- There is no authoritative world session object that owns player state, tick sequencing, trigger
  tracking, snapshots, and reset/load lifecycle in one place.
- `stellar-client` still renders an imported scene; it does not yet drive movement through the
  authoritative server seam.
- `stellar::platform::Input` is a low-level SDL keyboard-state wrapper. There is no display-free
  command mapper from input state to server movement intent.
- Runtime trigger integration currently uses a sphere centered at the character position, even though
  character movement is capsule-aware. That is acceptable for Phase 8E, but should be explicitly
  hardened before gameplay grows.
- Collision validation exists, but world metadata validation for player spawns, entity spawns,
  triggers, sprites, and authoring conventions is still missing.
- There is no end-to-end display-free playable-level fixture covering import -> runtime world ->
  authoritative ticks -> triggers -> snapshot output.

## Phase 9 goal

Move from "backend-neutral collision and movement primitives exist" to "a local authoritative
playable loop can be assembled, tested, and later presented by the client."

This is still not the networking phase. The server remains authoritative, but Phase 9 should use a
local loopback/session seam before adding remote transport, prediction, rollback, or delta
compression.

## Recommended phase order

```text
Authority/runtime lane:
  Phase 9A -> Phase 9D -> Phase 9F

Input/client seam lane:
  Phase 9C -> Phase 9D

Presentation lane:
  Phase 9E, after Phase 9A snapshots exist

Authoring/tooling lane:
  Phase 9B, parallel-safe with Phase 9A and Phase 9C
```

## Safe parallelization

- **Phase 9A and Phase 9B can run in parallel.** Phase 9A owns runtime/server session state; Phase 9B
  owns metadata authoring diagnostics.
- **Phase 9C can run in parallel with Phase 9A** if it only creates input-to-command mapping and does
  not create the loopback session.
- **Phase 9E can start after Phase 9A defines snapshot/presentation data.** It can run in parallel
  with Phase 9D if both avoid editing the same client loop files.
- **Phase 9D should wait for Phase 9A and Phase 9C.** It bridges authoritative session ticks into the
  local client/application path.
- **Phase 9F should run last.** It is the end-to-end fixture/CLI hardening pass and depends on the
  previous seams being stable.

## Plan files

1. `Phase9A-AuthoritativeWorldSessionAndSnapshots.md`
   - Create a backend-neutral world session that owns movement state, trigger tracking, fixed tick
     updates, and stable snapshots.

2. `Phase9B-WorldMetadataAuthoringValidation.md`
   - Add validation for player spawns, entity spawns, triggers, sprite markers, and authored metadata
     conventions.

3. `Phase9C-InputCommandMapping.md`
   - Add display-free client/platform command mapping from keyboard/input state to
     `stellar::server::MovementCommand`.

4. `Phase9D-LocalLoopbackClientRuntime.md`
   - Add a local authoritative runtime bridge that lets the client step `stellar_server_core` without
     networking and without making the client authoritative.

5. `Phase9E-PlayerPresentationAndCamera.md`
   - Add backend-neutral player presentation and camera-follow data derived from authoritative
     snapshots, keeping render backend work optional and tested separately.

6. `Phase9F-PlayableWorldFixtureAndHeadlessSmoke.md`
   - Add an end-to-end display-free fixture and optional headless server smoke executable for import,
     validation, runtime session ticks, snapshots, and trigger events.

## Global rules for every phase

- Implement one phase per PR/branch slice.
- Keep default CTest display-free.
- Do not add a third-party physics engine.
- Do not add remote networking, prediction, rollback, full ECS, scripts, or dynamic rigid bodies
  unless a later explicit plan asks for them.
- Keep renderer/audio handles out of authoritative state.
- Public APIs require Doxygen `@brief`.
- Keep data plain and serializable where practical.
- Add completion notes to the phase plan after implementation if copied into the repo.
- Update `docs/ImplementationStatus.md` after each completed phase.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

## Baseline validation

Run this at the end of each phase unless the phase gives a narrower focused subset first:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

OpenGL/Vulkan context tests must remain opt-in.

---

<!-- Phase9A-AuthoritativeWorldSessionAndSnapshots.md -->

# Phase 9A: Authoritative World Session and Snapshots

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Type: backend-neutral server/runtime integration
- Depends on: Phase 8C and Phase 8E
- Can run in parallel with: Phase 9B; Phase 9C if Phase 9C only maps input to commands
- Must not run in parallel with: Phase 9D
- Do not commit unless explicitly instructed by the user.

## Objective

Create a small backend-neutral authoritative world session that owns player movement state, trigger
tracking, fixed tick sequencing, and snapshot output.

Phase 8C gave the repo one authoritative movement tick function. Phase 8E added movement-trigger
handoff. Phase 9A should wrap those primitives in a stable session API so future client, local
loopback, headless server, and tests can use one authoritative path instead of manually wiring
movement state and trigger trackers each time.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `AGENTS.md`
- `include/stellar/server/MovementSimulation.hpp`
- `src/server/MovementSimulation.cpp`
- `include/stellar/server/MovementTriggerIntegration.hpp`
- `src/server/MovementTriggerIntegration.cpp`
- `include/stellar/world/RuntimeWorld.hpp`
- `src/world/RuntimeWorld.cpp`
- `include/stellar/physics/CharacterController.hpp`
- `tests/server/MovementSimulation.cpp`
- `tests/server/MovementTriggerIntegration.cpp`
- `CMakeLists.txt`

## Scope

Add a session layer to `stellar_server_core`.

Preferred new files:

- `include/stellar/server/WorldSession.hpp`
- `src/server/WorldSession.cpp`
- `tests/server/WorldSession.cpp`

Preferred CMake target/test:

- Add `src/server/WorldSession.cpp` to `stellar_server_core`.
- Add `stellar_server_world_session_test`.
- CTest name: `server_world_session`.

## Recommended public API

Use plain data. Keep it serializable and renderer-free.

```cpp
namespace stellar::server {

/** @brief Stable identifier for a local authoritative player slot. */
using PlayerId = std::uint32_t;

/** @brief Server-owned command for one simulation tick. */
struct PlayerCommand {
    PlayerId player_id = 0;
    MovementCommand movement{};
};

/** @brief Authoritative player state exposed in snapshots. */
struct PlayerSnapshot {
    PlayerId player_id = 0;
    std::array<float, 3> position{};
    std::array<float, 3> velocity{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    bool grounded = false;
};

/** @brief Snapshot emitted by the authoritative world session after a tick. */
struct WorldSnapshot {
    std::uint64_t tick = 0;
    std::vector<PlayerSnapshot> players;
    std::vector<MovementTriggerEvent> trigger_events;
};

/** @brief Deterministic config for a local authoritative world session. */
struct WorldSessionConfig {
    MovementSimulationConfig movement{};
    PlayerId local_player_id = 1;
};

/** @brief Backend-neutral authoritative session over one runtime world. */
class WorldSession {
public:
    explicit WorldSession(const stellar::world::RuntimeWorld& world,
                          WorldSessionConfig config = {});

    void reset(const stellar::world::RuntimeWorld& world, WorldSessionConfig config = {});

    [[nodiscard]] WorldSnapshot snapshot() const;

    [[nodiscard]] WorldSnapshot tick(std::span<const PlayerCommand> commands) noexcept;

    [[nodiscard]] std::uint64_t tick_index() const noexcept;
};

} // namespace stellar::server
```

Acceptable simplification for this phase: support exactly one player internally, but make the data
shape extensible through `PlayerId` and vectors so future ECS/network work does not have to break the
public snapshot contract immediately.

## Implementation steps

1. Add session data structures.
   - Use Doxygen `@brief` for public types and methods.
   - Keep renderer/audio/platform handles out.
   - Store a pointer/reference to caller-owned `RuntimeWorld` with documented lifetime rules.

2. Initialize from runtime world.
   - Use `make_spawn_movement_state(world)`.
   - Reset `MovementTriggerTracker` from the runtime world.
   - Initialize tick index to zero.

3. Implement command selection.
   - For now, accept one command for `local_player_id`.
   - Missing command means zero movement intent.
   - Commands for unknown player ids are ignored deterministically.
   - Sanitization remains inside `simulate_movement_tick`.

4. Implement `tick`.
   - Call `simulate_movement_tick_and_update_triggers`.
   - Update server-owned movement state.
   - Increment tick after applying the command.
   - Emit a `WorldSnapshot`.

5. Implement `snapshot`.
   - Return current authoritative state without ticking.
   - Trigger events should be empty for pure snapshot calls unless the session explicitly records last
     tick events. Prefer empty to avoid replaying events.

6. Keep it deterministic.
   - No wall-clock time, SDL, networking, rendering, or random numbers.
   - Fixed tick timing comes from `MovementSimulationConfig::fixed_dt`.

## Non-goals

Do not implement:

- Remote networking or packet formats.
- Client prediction/reconciliation.
- Full ECS.
- Multiple dynamic entities beyond one local player slot.
- Serialization to binary/wire format.
- Rendering, audio, camera, or animation integration.
- Trigger scripts or callbacks.
- Dynamic physics.

## Required tests

Create `tests/server/WorldSession.cpp`.

Required coverage:

1. `session_initial_snapshot_uses_player_spawn`
2. `session_defaults_to_origin_without_player_spawn`
3. `tick_without_command_advances_deterministically`
4. `tick_with_unknown_player_command_is_ignored`
5. `tick_with_local_player_command_updates_player_snapshot`
6. `wall_collision_is_authoritative_in_snapshot`
7. `trigger_enter_stay_exit_events_are_reported_once_per_tick`
8. `snapshot_does_not_replay_previous_trigger_events`
9. `reset_reinitializes_spawn_state_tick_and_trigger_tracker`
10. `same_inputs_produce_same_snapshots`

Use synthetic `SceneAsset`/`RuntimeWorld` construction. No display or graphics context.

## CMake guidance

```cmake
target_sources(stellar_server_core PRIVATE
    src/server/WorldSession.cpp
)

add_executable(stellar_server_world_session_test
    tests/server/WorldSession.cpp
)

target_include_directories(stellar_server_world_session_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_server_world_session_test PRIVATE
    stellar_server_core
)

add_test(NAME server_world_session COMMAND stellar_server_world_session_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_server_world_session_test \
    stellar_server_movement_simulation_test \
    stellar_movement_trigger_integration_test -j$(nproc)
ctest --test-dir build -R '^(server_world_session|server_movement_simulation|movement_trigger_integration)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- A backend-neutral authoritative session API exists.
- The session owns player movement state and trigger tracking.
- Ticks emit stable snapshots and deterministic trigger events.
- No graphics/audio/platform dependency is introduced.
- Tests cover spawn, ticking, command selection, wall collision, triggers, reset, and determinism.
- Public APIs have Doxygen `@brief`.
- Default CTest remains display-free.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9A authoritative world session and snapshots.
- Public API: <describe session/config/command/snapshot types>.
- Movement integration: <describe MovementSimulation use>.
- Trigger integration: <describe MovementTriggerTracker ownership/event behavior>.
- Determinism/lifetime: <describe tick ordering and RuntimeWorld lifetime assumptions>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Client local loopback remains Phase 9D.
  - Remote networking remains future work.
  - Full ECS remains future work.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9A authoritative world session and snapshots.
- Public API: added `PlayerId`, `PlayerCommand`, `PlayerSnapshot`, `WorldSnapshot`,
  `WorldSessionConfig`, and `WorldSession` in `include/stellar/server/WorldSession.hpp`.
- Movement integration: `WorldSession` initializes from `make_spawn_movement_state(world)` and ticks
  through `simulate_movement_tick_and_update_triggers`.
- Trigger integration: the session owns a `MovementTriggerTracker`; tick snapshots include that tick's
  events, while pure `snapshot()` calls do not replay previous events.
- Determinism/lifetime: one local player slot is supported, missing commands become zero intent,
  unknown player IDs are ignored, ticks increment after simulation, and the caller owns the
  referenced `RuntimeWorld` lifetime.
- Tests added/updated: `tests/server/WorldSession.cpp`, CTest `server_world_session`.
- Validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build --target stellar_server_world_session_test stellar_server_movement_simulation_test stellar_movement_trigger_integration_test -j$(nproc)`
  - `ctest --test-dir build -R '^(server_world_session|server_movement_simulation|movement_trigger_integration)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed.
- Deferred follow-up:
  - Remote networking remains future work.
  - Full ECS remains future work.

---

<!-- Phase9B-WorldMetadataAuthoringValidation.md -->

# Phase 9B: World Metadata Authoring Validation

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack`
- Cross-domain review: `@director` if validation policy conflicts with content goals
- Type: asset/world authoring diagnostics
- Depends on: Phase 6D and Phase 7C; complements Phase 8D
- Can run in parallel with: Phase 9A and Phase 9C
- Must not run in parallel with: another task reshaping `WorldMetadataAsset`
- Do not commit unless explicitly instructed by the user.

## Objective

Add deterministic validation for authored world metadata markers: player spawns, entity spawns,
triggers, sprite markers, and portal markers.

Phase 8D validates static collision geometry. Metadata validation is the matching authoring layer for
the rest of the playable-world conventions. It should catch ambiguous or suspicious authored content
before runtime movement/session work depends on it.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/assets/WorldMetadataAsset.hpp`
- `include/stellar/assets/SceneAsset.hpp`
- `include/stellar/world/RuntimeWorld.hpp`
- `include/stellar/world/CollisionValidation.hpp`
- `src/world/CollisionValidation.cpp`
- `src/import/gltf/WorldMetadataImport.cpp`
- `tests/world/CollisionValidation.cpp`
- `tests/import/gltf/ImporterRegression.cpp`
- `CMakeLists.txt`

## Scope

Add standalone metadata validation, preferably under `stellar::world`.

Preferred new files:

- `include/stellar/world/WorldMetadataValidation.hpp`
- `src/world/WorldMetadataValidation.cpp`
- `tests/world/WorldMetadataValidation.cpp`

Preferred CMake target/test:

- Add source to `stellar_world`.
- Add `stellar_world_metadata_validation_test`.
- CTest name: `world_metadata_validation`.

## Recommended public API

Mirror the style of `CollisionValidation` without forcing the two reports to share types unless it is
clean.

```cpp
namespace stellar::world {

/** @brief Severity for authored world metadata validation messages. */
enum class WorldMetadataValidationSeverity {
    kWarning,
    kError,
};

/** @brief One deterministic world metadata validation finding. */
struct WorldMetadataValidationFinding {
    WorldMetadataValidationSeverity severity = WorldMetadataValidationSeverity::kWarning;
    std::string code;
    std::string message;
    std::size_t marker_index = kWorldMetadataValidationInvalidIndex;
};

/** @brief Summary of authored world metadata validation results. */
struct WorldMetadataValidationReport {
    std::vector<WorldMetadataValidationFinding> findings;
    bool has_errors = false;
};

/** @brief Configurable policy for metadata authoring validation. */
struct WorldMetadataValidationConfig {
    bool require_player_spawn = true;
    bool warn_multiple_player_spawns = true;
    bool warn_empty_trigger_extents = true;
    bool warn_empty_sprite_names = true;
    bool warn_empty_entity_archetypes = true;
};

/** @brief Validate backend-neutral world metadata markers for runtime use. */
[[nodiscard]] WorldMetadataValidationReport validate_world_metadata(
    const stellar::assets::WorldMetadataAsset& metadata,
    const WorldMetadataValidationConfig& config = {});

} // namespace stellar::world
```

## Validation policy

Recommended deterministic findings:

Errors:

- Non-finite marker position.
- Non-finite marker rotation.
- Non-finite marker scale.
- Negative or non-finite trigger scale if the runtime policy cannot safely absolute it.
- Missing player spawn when `require_player_spawn` is true.
- Entity spawn marker with an empty archetype if config requires usable gameplay spawns.

Warnings:

- Multiple player spawns.
- Duplicate trigger names.
- Duplicate sprite names when names are intended to bind content.
- Empty trigger name.
- Empty sprite name.
- Empty portal name.
- Empty/zero trigger extents.
- Very large trigger extents.
- Portal marker present even though portal runtime behavior is deferred.
- Raw `extras_json` present but not parsed, if useful for author feedback.

Keep marker ordering deterministic: findings should be sorted by marker index, then stable code.

## Integration guidance

Phase 9B should remain standalone, but it may optionally add convenience helpers:

```cpp
/** @brief Validate world metadata copied into a RuntimeWorld. */
[[nodiscard]] WorldMetadataValidationReport validate_world_metadata(
    const stellar::world::RuntimeWorld& world,
    const WorldMetadataValidationConfig& config = {});
```

Do not make runtime world construction fail by default. This phase should report diagnostics; policy
for blocking load can come later.

## Non-goals

Do not implement:

- ECS spawning.
- Trigger behavior or callbacks.
- Sprite texture/material binding.
- Portal traversal.
- JSON schema parsing for `extras_json`.
- glTF importer changes unless tests expose a clear bug.
- Combined asset validation CLI; that belongs to Phase 9F.

## Required tests

Create `tests/world/WorldMetadataValidation.cpp`.

Required coverage:

1. `empty_metadata_requires_player_spawn_by_default`
2. `missing_player_spawn_can_be_warning_free_when_policy_disabled`
3. `single_player_spawn_passes`
4. `multiple_player_spawns_warn`
5. `entity_spawn_empty_archetype_reports_finding`
6. `trigger_empty_name_reports_finding`
7. `trigger_duplicate_names_warn`
8. `trigger_zero_extents_warn_under_policy`
9. `sprite_empty_name_reports_finding`
10. `portal_marker_warns_runtime_deferred`
11. `non_finite_position_rotation_scale_report_errors`
12. `findings_are_deterministically_ordered`

Optionally extend importer regression tests only if an authored glTF fixture is needed to prove the
importer stores metadata in the expected form.

## CMake guidance

```cmake
target_sources(stellar_world PRIVATE
    src/world/WorldMetadataValidation.cpp
)

add_executable(stellar_world_metadata_validation_test
    tests/world/WorldMetadataValidation.cpp
)

target_include_directories(stellar_world_metadata_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_world_metadata_validation_test PRIVATE
    stellar_world
)

add_test(NAME world_metadata_validation COMMAND stellar_world_metadata_validation_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_world_metadata_validation_test \
    stellar_collision_validation_test \
    stellar_runtime_world_test -j$(nproc)
ctest --test-dir build -R '^(world_metadata_validation|collision_validation|runtime_world)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- World metadata validation exists as a backend-neutral standalone API.
- Player spawn, entity spawn, trigger, sprite, portal, and non-finite data diagnostics are covered.
- Validation reports are deterministic.
- Runtime world construction is not blocked by default.
- Public APIs have Doxygen `@brief`.
- Default CTest remains display-free.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9B world metadata authoring validation.
- Public API: <describe report/config/finding types>.
- Validation policy: <summarize errors/warnings>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Combined import/runtime validation remains Phase 9F.
  - ECS spawning and sprite binding remain future work.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9B world metadata authoring validation.
- Public API: added metadata validation severity, finding, report, config, invalid-index sentinel,
  asset overload, and `RuntimeWorld` convenience overload in
  `include/stellar/world/WorldMetadataValidation.hpp`.
- Validation policy: errors for missing required player spawn, non-finite marker transforms, and empty
  entity archetypes; warnings for multiple player spawns, duplicate/empty trigger or sprite names,
  empty portal names, zero or large trigger extents, deferred portal runtime behavior, and unparsed
  `extras_json`.
- Tests added/updated: `tests/world/WorldMetadataValidation.cpp`, CTest
  `world_metadata_validation`.
- Validation:
  - `cmake --build build --target stellar_world_metadata_validation_test stellar_collision_validation_test stellar_runtime_world_test -j$(nproc)`
  - `ctest --test-dir build -R '^(world_metadata_validation|collision_validation|runtime_world)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed.
- Deferred follow-up:
  - ECS spawning, portal traversal, trigger callbacks, and sprite binding remain future work.

---

<!-- Phase9C-InputCommandMapping.md -->

# Phase 9C: Input Command Mapping

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack` for backend-neutral command semantics
- Consult: `@miyamoto` only if client/application ownership becomes unclear
- Type: client/platform seam, display-free where possible
- Depends on: Phase 8C
- Can run in parallel with: Phase 9A if it does not instantiate a world session
- Must not run in parallel with: Phase 9D if both edit the same client runtime files
- Do not commit unless explicitly instructed by the user.

## Objective

Create a deterministic mapping from client input state to server-authoritative movement commands.

The design says the client captures input and forwards it to the server; it must not own gameplay
state. Phase 9C should make that boundary explicit by producing `stellar::server::MovementCommand`
from input, without applying movement on the client.

## Required reading

- `docs/Design.md`
- `AGENTS.md`
- `include/stellar/platform/Input.hpp`
- `src/platform/Input.cpp`
- `include/stellar/server/MovementSimulation.hpp`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/Application.cpp`
- `tests/graphics/BackendSelection.cpp` for test style if needed
- `CMakeLists.txt`

## Scope

Add a small input-command mapping module.

Preferred new files:

- `include/stellar/client/MovementInputMapper.hpp`
- `src/client/MovementInputMapper.cpp`
- `tests/client/MovementInputMapper.cpp`

Preferred CMake target/test:

- Add a small `stellar_client_runtime` or add the mapper to `stellar_client_config` only if that
  target is still a good semantic fit.
- Add `stellar_client_movement_input_mapper_test`.
- CTest name: `client_movement_input_mapper`.

If adding a new library, prefer:

```cmake
add_library(stellar_client_runtime
    src/client/MovementInputMapper.cpp
)
target_link_libraries(stellar_client_runtime PUBLIC
    stellar_platform
    stellar_server_core
)
```

If `stellar_platform` creates SDL test friction, make the mapper consume a plain key-state struct and
add a thin adapter for `stellar::platform::Input`.

## Recommended public API

```cpp
namespace stellar::client {

/** @brief Configurable key bindings for local movement command generation. */
struct MovementInputBindings {
    SDL_Scancode forward = SDL_SCANCODE_W;
    SDL_Scancode backward = SDL_SCANCODE_S;
    SDL_Scancode left = SDL_SCANCODE_A;
    SDL_Scancode right = SDL_SCANCODE_D;
    SDL_Scancode jump = SDL_SCANCODE_SPACE;
};

/** @brief Options for converting client input state to server movement intent. */
struct MovementInputMapperConfig {
    MovementInputBindings bindings{};
    bool normalize_diagonal = true;
};

/** @brief Convert local input state into a server-owned movement command intent. */
[[nodiscard]] stellar::server::MovementCommand make_movement_command(
    const stellar::platform::Input& input,
    const MovementInputMapperConfig& config = {}) noexcept;

} // namespace stellar::client
```

Alternative if avoiding SDL in public client mapper headers:

```cpp
struct MovementInputState {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jump = false;
};

[[nodiscard]] stellar::server::MovementCommand make_movement_command(
    const MovementInputState& state,
    const MovementInputMapperConfig& config = {}) noexcept;
```

Recommended: implement the plain state overload first, then add an `Input` adapter.

## Direction convention

Use the same world axes as current movement simulation:

- Forward: negative Z, unless existing render/camera conventions clearly prefer positive Z.
- Backward: positive Z.
- Left: negative X.
- Right: positive X.
- Y must remain zero; server sanitization will enforce this too.

Document the convention in the public API.

## Non-goals

Do not implement:

- Movement simulation.
- Client prediction.
- Networking.
- Camera-relative input unless a small config option makes it trivial and testable.
- Mouse look.
- Rebinding UI.
- Gamepad input.
- Full client runtime loop. That is Phase 9D.
- Rendering changes.

## Required tests

Create `tests/client/MovementInputMapper.cpp`.

Required coverage:

1. `no_keys_produces_zero_command`
2. `forward_maps_to_world_forward`
3. `backward_maps_to_world_backward`
4. `left_and_right_cancel`
5. `forward_and_backward_cancel`
6. `diagonal_input_is_normalized_when_enabled`
7. `diagonal_input_not_normalized_when_disabled`
8. `jump_key_sets_jump_intent`
9. `plain_state_and_platform_input_adapter_match`

For `platform::Input` adapter tests, synthesize SDL keydown/keyup events and call
`Input::process_event`. Do not create a window or graphics context.

## CMake guidance

A new test may need `stellar_platform`, `stellar_server_core`, and the client mapper library.

```cmake
add_executable(stellar_client_movement_input_mapper_test
    tests/client/MovementInputMapper.cpp
)

target_include_directories(stellar_client_movement_input_mapper_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_movement_input_mapper_test PRIVATE
    stellar_client_runtime
)

add_test(NAME client_movement_input_mapper COMMAND stellar_client_movement_input_mapper_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_client_movement_input_mapper_test \
    stellar_server_movement_simulation_test -j$(nproc)
ctest --test-dir build -R '^(client_movement_input_mapper|server_movement_simulation)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- Client input maps to `MovementCommand` intent without applying movement locally.
- Direction conventions are documented and tested.
- Diagonal normalization behavior is deterministic.
- Jump intent is passed through but still applied only by server logic when/if implemented.
- Tests are display-free.
- No rendering/networking dependency is added.
- Public APIs have Doxygen `@brief`.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9C input command mapping.
- Public API: <describe mapper/input-state/config>.
- Direction policy: <describe world axes and diagonal behavior>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Local loopback client runtime remains Phase 9D.
  - Camera-relative input, mouse look, gamepad, and rebinding remain future work.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9C input command mapping.
- Public API: added `MovementInputState`, `MovementInputBindings`, `MovementInputMapperConfig`, and
  `make_movement_command` overloads for plain input state and `platform::Input` in
  `include/stellar/client/MovementInputMapper.hpp`.
- Direction policy: forward maps to world `-Z`, backward to `+Z`, left to `-X`, right to `+X`, `Y`
  remains zero, diagonal normalization is deterministic/configurable, and jump remains command intent
  only.
- Tests added/updated: `tests/client/MovementInputMapper.cpp`, CTest
  `client_movement_input_mapper`.
- Validation:
  - `cmake --build build --target stellar_client_movement_input_mapper_test stellar_server_movement_simulation_test -j$(nproc)`
  - `ctest --test-dir build -R '^(client_movement_input_mapper|server_movement_simulation)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed.
- Deferred follow-up:
  - Camera-relative input, mouse look, gamepad, and rebinding remain future work.

---

<!-- Phase9D-LocalLoopbackClientRuntime.md -->

# Phase 9D: Local Loopback Client Runtime

## Task card

- Branch: `collision-movement`
- Primary agent: `@director` for integration routing; implementation likely `@carmack` with
  `@miyamoto` review if client render loop is touched
- Type: client/server-authority integration
- Depends on: Phase 9A and Phase 9C
- Can run in parallel with: Phase 9E only if Phase 9E avoids editing the same client runtime path
- Must not run in parallel with: Phase 9A or Phase 9C
- Do not commit unless explicitly instructed by the user.

## Objective

Add a local loopback runtime path that lets the client submit input commands to an authoritative
server session in-process, then consume snapshots.

This is not networking and not client prediction. It is the single-player/local authoritative path
described by the design: client captures input, server-owned code advances state, client receives a
snapshot for presentation.

## Required reading

- `docs/Design.md`
- `docs/ImplementationStatus.md`
- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/ApplicationConfig.cpp`
- `src/client/Application.cpp`
- `include/stellar/client/MovementInputMapper.hpp` from Phase 9C
- `include/stellar/server/WorldSession.hpp` from Phase 9A
- `include/stellar/world/RuntimeWorld.hpp`
- `include/stellar/graphics/SceneRenderer.hpp`
- `include/stellar/platform/Input.hpp`
- `CMakeLists.txt`

## Scope

Add a minimal local runtime bridge. Prefer keeping most of it display-free and outside the renderer.

Preferred new files:

- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `src/client/LocalLoopbackRuntime.cpp`
- `tests/client/LocalLoopbackRuntime.cpp`

Optional application integration after the bridge is tested:

- Extend `ApplicationConfig` with a flag like `enable_local_loopback`.
- In `Application::run`, when a scene is loaded and loopback is enabled:
  - build `RuntimeWorld`,
  - construct `server::WorldSession`,
  - map input to commands,
  - tick the session on fixed steps,
  - store the latest snapshot for presentation.

## Recommended public API

```cpp
namespace stellar::client {

/** @brief Config for an in-process authoritative local server runtime. */
struct LocalLoopbackRuntimeConfig {
    stellar::server::WorldSessionConfig session{};
    MovementInputMapperConfig input_mapper{};
    int max_ticks_per_frame = 4;
};

/** @brief Result of advancing the local authoritative runtime for one client frame. */
struct LocalLoopbackFrameResult {
    stellar::server::WorldSnapshot snapshot{};
    int ticks_run = 0;
    bool dropped_excess_time = false;
};

/** @brief In-process local authoritative runtime used by single-player/client presentation. */
class LocalLoopbackRuntime {
public:
    explicit LocalLoopbackRuntime(const stellar::world::RuntimeWorld& world,
                                  LocalLoopbackRuntimeConfig config = {});

    [[nodiscard]] const stellar::server::WorldSnapshot& latest_snapshot() const noexcept;

    [[nodiscard]] LocalLoopbackFrameResult update(
        const stellar::platform::Input& input,
        float delta_seconds) noexcept;
};

} // namespace stellar::client
```

Implementation details:

- Accumulate client frame time.
- Tick the authoritative session using fixed `MovementSimulationConfig::fixed_dt`.
- Clamp or drop huge accumulated time deterministically to avoid spiral-of-death behavior.
- Never use renderer state as gameplay truth.

## Non-goals

Do not implement:

- Remote networking.
- Client prediction/reconciliation.
- Multiplayer.
- Full ECS.
- Save/load.
- Rendering a player sprite or camera follow. That is Phase 9E.
- Audio events.
- Trigger scripting.
- Real-time wall-clock ownership in tests.

## Required tests

Create `tests/client/LocalLoopbackRuntime.cpp`.

Required coverage:

1. `initial_snapshot_matches_spawn`
2. `zero_delta_runs_no_ticks`
3. `small_delta_accumulates_until_fixed_tick`
4. `one_fixed_tick_applies_input_command`
5. `large_delta_clamps_ticks_per_frame`
6. `loopback_uses_authoritative_collision`
7. `trigger_events_are_forwarded_in_frame_result`
8. `latest_snapshot_updates_after_ticks`
9. `missing_collision_world_still_ticks_deterministically`
10. `same_input_sequence_same_snapshots`

Use synthetic `RuntimeWorld` fixtures and simulated input events. No display or graphics context.

## CMake guidance

If Phase 9C added `stellar_client_runtime`, add this source to it:

```cmake
target_sources(stellar_client_runtime PRIVATE
    src/client/LocalLoopbackRuntime.cpp
)

target_link_libraries(stellar_client_runtime PUBLIC
    stellar_server_core
    stellar_world
    stellar_platform
)
```

Add a test target:

```cmake
add_executable(stellar_client_local_loopback_runtime_test
    tests/client/LocalLoopbackRuntime.cpp
)

target_include_directories(stellar_client_local_loopback_runtime_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_client_local_loopback_runtime_test PRIVATE
    stellar_client_runtime
)

add_test(NAME client_local_loopback_runtime COMMAND stellar_client_local_loopback_runtime_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_client_local_loopback_runtime_test \
    stellar_server_world_session_test \
    stellar_client_movement_input_mapper_test -j$(nproc)
ctest --test-dir build -R '^(client_local_loopback_runtime|server_world_session|client_movement_input_mapper)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If `Application::run` is touched, also run:

```bash
cmake --build build --target stellar_client_asset_validation_smoke stellar-client -j$(nproc)
ctest --test-dir build -R '^(client_asset_validation_smoke|client_cli_asset_validation)$' --output-on-failure
```

## Acceptance criteria

- A local loopback runtime advances authoritative server state in-process.
- Client input is translated to server commands; client does not apply movement directly.
- Fixed-tick accumulation is deterministic and bounded.
- Runtime snapshots and trigger events are available for presentation.
- Tests are display-free.
- Application integration, if added, does not require GPU/display for validation paths.
- Public APIs have Doxygen `@brief`.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9D local loopback client runtime.
- Public API: <describe runtime/config/frame result>.
- Authority boundary: <describe command -> session -> snapshot flow>.
- Application integration: <describe if ApplicationConfig/Application changed>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Player presentation and camera follow remain Phase 9E if not complete.
  - Remote networking remains future work.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9D local loopback client runtime.
- Public API: added `LocalLoopbackRuntimeConfig`, `LocalLoopbackFrameResult`, and
  `LocalLoopbackRuntime` in `include/stellar/client/LocalLoopbackRuntime.hpp`.
- Authority boundary: client input is translated to `PlayerCommand`; only the in-process authoritative
  `WorldSession` applies movement; fixed ticks are accumulated and bounded by `max_ticks_per_frame`,
  with snapshots and trigger events returned for presentation.
- Application integration: deferred; `Application::run` was not modified in this display-free slice.
- Tests added/updated: `tests/client/LocalLoopbackRuntime.cpp`, CTest
  `client_local_loopback_runtime`.
- Validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build --target stellar_client_local_loopback_runtime_test stellar_server_world_session_test stellar_client_movement_input_mapper_test -j$(nproc)`
  - `ctest --test-dir build -R '^(client_local_loopback_runtime|server_world_session|client_movement_input_mapper)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed.
- Deferred follow-up:
  - Remote networking remains future work.

---

<!-- Phase9E-PlayerPresentationAndCamera.md -->

# Phase 9E: Player Presentation and Camera Snapshot Data

## Task card

- Branch: `collision-movement`
- Primary agent: `@miyamoto` for graphics/presentation data
- Coordination: `@director` because authoritative server snapshots are involved
- Type: backend-neutral presentation data and optional renderer hook
- Depends on: Phase 9A
- Can run in parallel with: Phase 9B; Phase 9D only if client loop files do not conflict
- Must not run in parallel with: tasks reshaping `WorldSnapshot`
- Do not commit unless explicitly instructed by the user.

## Objective

Create presentation-facing data derived from authoritative snapshots: player pose, optional
billboard sprite submission, and a camera follow target.

The client needs a way to show movement without making rendering authoritative. Phase 9E should add
backend-neutral presentation assembly and camera math first. Renderer backend changes should be small
or deferred unless they are already cleanly supported.

## Required reading

- `docs/Design.md`
- `include/stellar/server/WorldSession.hpp` from Phase 9A
- `include/stellar/graphics/BillboardSprite.hpp`
- `include/stellar/graphics/RenderScene.hpp`
- `include/stellar/graphics/SceneRenderer.hpp`
- `src/graphics/BillboardSprite.cpp`
- `src/graphics/RenderScene.cpp`
- `src/graphics/SceneRenderer.cpp`
- `tests/graphics/RenderSceneInspection.cpp`
- `tests/graphics/RenderSceneUpload.cpp`
- `CMakeLists.txt`

## Scope

Add backend-neutral presentation helpers that convert authoritative state to render-ready data.

Preferred new files:

- `include/stellar/client/PlayerPresentation.hpp` or `include/stellar/graphics/PlayerPresentation.hpp`
- `src/client/PlayerPresentation.cpp` or `src/graphics/PlayerPresentation.cpp`
- `tests/client/PlayerPresentation.cpp` or `tests/graphics/PlayerPresentation.cpp`

Pick location based on dependency direction:

- Use `client/` if consuming `server::WorldSnapshot`.
- Use `graphics/` only if the API is generic and does not depend on server types.

## Recommended public API

```cpp
namespace stellar::client {

/** @brief Camera follow settings derived from authoritative player snapshots. */
struct PlayerCameraConfig {
    std::array<float, 3> follow_offset{0.0F, 2.0F, 6.0F};
    std::array<float, 3> look_at_offset{0.0F, 0.8F, 0.0F};
    float near_plane = 0.1F;
    float far_plane = 250.0F;
};

/** @brief Presentation state derived from one authoritative player snapshot. */
struct PlayerPresentationState {
    std::array<float, 3> position{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    bool grounded = false;
};

/** @brief Camera parameters for rendering the current authoritative player snapshot. */
struct PlayerCameraFrame {
    std::array<float, 3> eye{};
    std::array<float, 3> target{};
    float near_plane = 0.1F;
    float far_plane = 250.0F;
};

/** @brief Extract player presentation state from the latest authoritative world snapshot. */
[[nodiscard]] std::optional<PlayerPresentationState> make_player_presentation_state(
    const stellar::server::WorldSnapshot& snapshot,
    stellar::server::PlayerId player_id);

/** @brief Compute a deterministic camera frame from player presentation state. */
[[nodiscard]] PlayerCameraFrame make_player_camera_frame(
    const PlayerPresentationState& player,
    const PlayerCameraConfig& config = {}) noexcept;

} // namespace stellar::client
```

Optional if `BillboardSprite` has enough texture/material abstraction:

```cpp
/** @brief Build a placeholder player billboard from authoritative presentation state. */
[[nodiscard]] stellar::graphics::BillboardSprite make_player_billboard(
    const PlayerPresentationState& player,
    const PlayerBillboardConfig& config = {}) noexcept;
```

If texture/material binding is not ready, expose a placeholder-free presentation state only and
defer actual sprite rendering.

## Renderer integration guidance

Preferred minimal renderer hook:

- Add an optional camera override to `SceneRenderer` or `RenderScene` if the current renderer only
  uses scene bounds fit.
- Keep it backend-neutral.
- Add display-free tests for camera math and render-scene data.
- Do not require OpenGL/Vulkan context in default tests.

Avoid broad backend rewrites. If renderer integration is larger than expected, stop after presentation
data/camera math and document the renderer hook as deferred.

## Non-goals

Do not implement:

- Client prediction.
- Animation state machine.
- Sprite atlas binding or authored character materials.
- Multiplayer camera selection.
- Third-person collision/camera obstruction.
- Mouse look.
- UI/HUD.
- OpenGL/Vulkan context tests as default tests.
- Gameplay authority in the client.

## Required tests

Create display-free tests.

Required coverage:

1. `missing_player_snapshot_returns_nullopt`
2. `player_snapshot_extracts_position_rotation_grounded`
3. `camera_frame_uses_follow_and_look_offsets`
4. `camera_frame_sanitizes_non_finite_input`
5. `camera_near_far_are_clamped_or_preserved_by_documented_policy`
6. `player_billboard_preserves_world_position_and_size` if billboard helper is added
7. `renderer_camera_override_does_not_change_scene_bounds` if renderer hook is added
8. `snapshot_presentation_does_not_mutate_authoritative_snapshot`

## CMake guidance

If implemented under `stellar_client_runtime`:

```cmake
target_sources(stellar_client_runtime PRIVATE
    src/client/PlayerPresentation.cpp
)
target_link_libraries(stellar_client_runtime PUBLIC
    stellar_server_core
    stellar_graphics
)
```

Test target:

```cmake
add_executable(stellar_player_presentation_test
    tests/client/PlayerPresentation.cpp
)

target_include_directories(stellar_player_presentation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar_player_presentation_test PRIVATE
    stellar_client_runtime
)

add_test(NAME player_presentation COMMAND stellar_player_presentation_test)
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_player_presentation_test \
    stellar_server_world_session_test \
    stellar_render_scene_inspection_test -j$(nproc)
ctest --test-dir build -R '^(player_presentation|server_world_session|render_scene_inspection)$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Only run optional graphics context tests if backend code is changed and an appropriate environment is
available.

## Acceptance criteria

- Authoritative snapshots can produce backend-neutral presentation state.
- Camera follow math is deterministic and display-free tested.
- Renderer integration, if added, keeps OpenGL/Vulkan parity assumptions intact.
- Client presentation data does not become gameplay truth.
- Public APIs have Doxygen `@brief`.
- Default CTest remains display-free.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9E player presentation and camera snapshot data.
- Public API: <describe presentation/camera/billboard helpers>.
- Authority boundary: <describe snapshot-only data flow>.
- Renderer integration: <describe added hook or deferred status>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Sprite atlas/material binding remains future work if not complete.
  - Client prediction and camera obstruction remain future work.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9E player presentation and camera snapshot data.
- Public API: added `PlayerCameraConfig`, `PlayerPresentationState`, `PlayerCameraFrame`,
  `make_player_presentation_state`, and `make_player_camera_frame` in
  `include/stellar/client/PlayerPresentation.hpp`.
- Authority boundary: presentation copies data from authoritative snapshots only and does not mutate
  or own gameplay state.
- Renderer integration: deferred; no backend renderer changes or billboard helper were added because
  authored player material/texture binding is not scoped yet.
- Tests added/updated: `tests/client/PlayerPresentation.cpp`, CTest `player_presentation`.
- Validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build --target stellar_player_presentation_test stellar_server_world_session_test stellar_render_scene_inspection_test -j$(nproc)`
  - `ctest --test-dir build -R '^(player_presentation|server_world_session|render_scene_inspection)$' --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed.
- Deferred follow-up:
  - Sprite atlas/material binding, renderer camera override, camera obstruction, interpolation, and
    prediction remain future work.

---

<!-- Phase9F-PlayableWorldFixtureAndHeadlessSmoke.md -->

# Phase 9F: Playable World Fixture and Headless Smoke

## Task card

- Branch: `collision-movement`
- Primary agent: `@carmack` for headless/server validation
- Coordinate with: `@miyamoto` if glTF fixture/render import assumptions are touched
- Type: end-to-end display-free validation and optional CLI
- Depends on: Phase 9A; Phase 9B recommended; Phase 9D/9E optional but useful
- Can run in parallel with: no other Phase 9 work unless the fixture/API surfaces are frozen
- Do not commit unless explicitly instructed by the user.

## Objective

Add an end-to-end display-free playable-world validation path that proves authored glTF content can
flow through import, collision/metadata validation, runtime world assembly, authoritative session
ticks, snapshots, and trigger events.

This phase should catch integration regressions that isolated unit tests miss.

## Required reading

- `docs/ImplementationStatus.md`
- `include/stellar/import` and `src/import/gltf` relevant importer files
- `include/stellar/world/RuntimeWorld.hpp`
- `include/stellar/world/CollisionValidation.hpp`
- `include/stellar/world/WorldMetadataValidation.hpp` from Phase 9B if present
- `include/stellar/server/WorldSession.hpp` from Phase 9A
- `include/stellar/client/LocalLoopbackRuntime.hpp` from Phase 9D if present
- `tests/import/gltf/ImporterRegression.cpp`
- `tests/client/AssetValidationSmoke.cpp`
- `tests/server/WorldSession.cpp`
- `CMakeLists.txt`

## Scope

Add a generated or static fixture plus an end-to-end test. Optionally add a headless server smoke
executable if the project owner wants a CLI artifact.

Preferred new files:

- `tests/fixtures/gltf/playable_collision_room.gltf` if static fixture is practical.
- Or fixture generation inside `tests/import/gltf` helpers if current test style prefers generated
  glTF.
- `tests/integration/PlayableWorldSmoke.cpp`

Optional executable:

- `src/server/main.cpp`
- `stellar-server` executable that loads an asset, validates it, runs deterministic scripted commands,
  and prints a concise text summary.
- Keep this executable headless; no networking required.

## Fixture requirements

The fixture should include:

- Visible render geometry.
- Collision-only floor/walls using existing conventions:
  - `COL_*`
  - `Collision_*`
  - or descendants under exact `Collision`.
- One `SPAWN_Player`.
- At least one `TRIGGER_*` volume placed along a deterministic movement path.
- At least one `SPRITE_*` marker for future presentation checks.
- One ordinary render mesh that is not collision-only.
- Simple transforms that test parent/child world transform composition.

Keep the asset small and deterministic. Avoid textures unless importer fixtures already handle them
cleanly.

## Integration test path

The test should:

1. Import the playable glTF fixture with `STELLAR_ENABLE_GLTF=ON`.
2. Validate collision with `validate_level_collision`.
3. Validate metadata with `validate_world_metadata` if Phase 9B is present.
4. Build a `RuntimeWorld`.
5. Create a `server::WorldSession`.
6. Confirm the initial snapshot uses the player spawn.
7. Tick movement commands that walk into/along static collision.
8. Confirm authoritative position changes but does not pass through a wall.
9. Confirm a trigger enter event fires when expected.
10. Confirm a later tick produces stay or exit depending on scripted path.
11. Confirm collision-only nodes were not added as normal render mesh submissions, if the current
    fixture/test helpers can check this display-free.
12. Confirm no graphics context is required.

## Optional headless executable behavior

If adding `stellar-server`, keep it minimal.

Suggested CLI shape:

```bash
stellar-server --validate-world --asset tests/fixtures/gltf/playable_collision_room.gltf
stellar-server --smoke-script tests/fixtures/scripts/walk_into_trigger.txt --asset <path>
```

For this phase, a simple hard-coded smoke script inside the executable is acceptable if a script
parser would be too broad.

Output should be deterministic text, for example:

```text
world: ok
collision_meshes: 3
collision_triangles: 42
markers: 3
spawn: yes
ticks: 120
final_position: 1.000 0.900 -3.500
trigger_events: DoorOpen:enter, DoorOpen:stay
```

Do not add networking, sockets, or long-running server loops in this phase.

## Non-goals

Do not implement:

- Remote networking.
- Asset editor tooling.
- Full ECS spawning.
- Runtime scripts.
- Rendering validation.
- Pixel readback.
- Audio.
- Large benchmark framework.

## Required tests

Create `tests/integration/PlayableWorldSmoke.cpp`.

Required coverage:

1. `playable_fixture_imports_successfully`
2. `playable_fixture_collision_validation_passes_or_reports_only_expected_warnings`
3. `playable_fixture_metadata_validation_passes`
4. `runtime_world_has_collision_spawn_trigger_and_sprite_marker`
5. `server_session_initial_snapshot_uses_fixture_spawn`
6. `scripted_movement_hits_wall_without_tunneling`
7. `scripted_movement_enters_trigger`
8. `scripted_movement_trigger_events_are_deterministic`
9. `repeat_script_same_snapshot_and_events`
10. `test_does_not_create_window_or_graphics_context`

If adding `stellar-server`, add a CTest smoke that runs validation mode only.

## CMake guidance

```cmake
if(STELLAR_ENABLE_GLTF)
    add_executable(stellar_playable_world_smoke_test
        tests/integration/PlayableWorldSmoke.cpp
    )

    target_include_directories(stellar_playable_world_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(stellar_playable_world_smoke_test PRIVATE
        stellar_import_gltf
        stellar_world
        stellar_server_core
    )

    add_test(NAME playable_world_smoke COMMAND stellar_playable_world_smoke_test)
endif()
```

Optional executable:

```cmake
add_executable(stellar-server
    src/server/main.cpp
)

target_include_directories(stellar-server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stellar-server PRIVATE
    stellar_server_core
    stellar_world
)

if(STELLAR_ENABLE_GLTF)
    target_compile_definitions(stellar-server PRIVATE STELLAR_ENABLE_GLTF=1)
    target_link_libraries(stellar-server PRIVATE stellar_import_gltf)
endif()
```

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON
cmake --build build --target stellar_playable_world_smoke_test -j$(nproc)
ctest --test-dir build -R '^playable_world_smoke$' --output-on-failure
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

If adding `stellar-server`:

```bash
cmake --build build --target stellar-server -j$(nproc)
ctest --test-dir build -R 'server.*smoke|playable_world_smoke' --output-on-failure
```

## Acceptance criteria

- A small playable fixture validates import -> runtime world -> authoritative movement -> triggers.
- The smoke test is display-free and deterministic.
- Collision and metadata validation are part of the end-to-end path.
- The fixture proves collision-only nodes remain collision-only.
- Optional `stellar-server` remains headless and does not add networking.
- Default CTest passes with `STELLAR_ENABLE_GLTF=ON`.

## Completion notes template

```md
## Completion Notes (YYYY-MM-DD)

- Implemented: Phase 9F playable world fixture and headless smoke.
- Fixture: <describe authored glTF content and conventions>.
- Integration path: <describe import/validation/runtime/session/tick/trigger flow>.
- Optional CLI: <describe stellar-server if added, or deferred>.
- Tests added/updated: <list tests>.
- Validation:
  - `<commands>`
  - Result: <pass/fail>
- Deferred follow-up:
  - Remote networking remains future work.
  - Full ECS/entity spawning remains future work.
  - Rendered player sprite/camera polish remains future work if not complete.
```

## Completion Notes (2026-04-30)

- Implemented: Phase 9F playable world fixture and headless smoke.
- Fixture: `tests/integration/PlayableWorldSmoke.cpp` generates a temporary embedded-buffer glTF with
  visible render geometry, collision-only floor/walls using existing conventions, `SPAWN_Player`,
  `TRIGGER_DoorOpen`, `SPRITE_Guide`, and metadata transform composition.
- Integration path: the smoke test imports the fixture, validates collision, validates metadata,
  builds `RuntimeWorld`, creates `server::WorldSession`, runs scripted authoritative movement, checks
  wall collision, trigger event determinism, repeated snapshot determinism, and collision-only render
  filtering without a graphics context.
- Optional CLI: deferred; no `stellar-server` executable was added.
- Tests added/updated: `tests/integration/PlayableWorldSmoke.cpp`, CTest `playable_world_smoke` gated
  by `STELLAR_ENABLE_GLTF`.
- Validation:
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSTELLAR_ENABLE_GLTF=ON`
  - `cmake --build build --target stellar_playable_world_smoke_test -j$(nproc)`
  - `ctest --test-dir build -R playable_world_smoke --output-on-failure`
  - `cmake --build build -j$(nproc)`
  - `ctest --test-dir build --output-on-failure`
  - Result: passed, 21/21 tests.
- Deferred follow-up:
  - Remote networking, full ECS/entity spawning, and rendered player sprite/camera polish remain
    future work.

---
