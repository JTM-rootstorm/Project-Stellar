---
project: Project Stellar
branch: audio-impl
plan_family: audio_footsteps
agent_optimized_for: Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-04
---

# Phase AF-2 — Authoritative Footstep Cadence and Protocol Events

## Goal

Emit deterministic server-approved footstep events when the authoritative player is grounded and has moved far enough across a floor surface.

This phase should make footsteps visible as queued protocol/presentation events. It does not require audible playback yet.

## Primary owner

`@carmack` / server-protocol Codex agent.

## Consult

`@kojima` for cadence feel only.

## Depends on

AF-1.

## Can run in parallel with

AF-3 after `GameplayEventKind::kFootstep` and sound-id rules are agreed.

## Must finish before

AF-4.

## Event contract

Extend `stellar::protocol::GameplayEventKind`:

```cpp
enum class GameplayEventKind : std::uint8_t {
    kPickupCollected = 1,
    kDoorStateChanged = 2,
    kScriptCommandApplied = 3,
    kScriptError = 4,
    kFootstep = 5,
};
```

Use existing `GameplayEvent` fields:

```text
kind      = kFootstep
tick      = authoritative tick
entity_id = player entity id if known, otherwise 0
player_id = authoritative player id
code      = footstep surface id, e.g. "wood", "metal", "generic"
message   = optional source material name, e.g. "dev/grid_32"
```

Keep strings bounded by the existing codec limits.

## Add a footstep tracker

Preferred new files:

```text
include/stellar/server/FootstepTracker.hpp
src/server/FootstepTracker.cpp
tests/server/FootstepTracker.cpp
```

Suggested API:

```cpp
namespace stellar::server {

struct FootstepTrackerConfig {
    float min_horizontal_speed = 24.0F;
    float walk_step_distance = 56.0F;
    float run_step_distance = 44.0F;
    bool emit_on_landing = false;
};

struct FootstepSurfaceHit {
    bool valid = false;
    std::string surface_id = "generic";
    std::string source_material_name;
};

struct FootstepTrackerInput {
    PlayerId player_id = 0;
    EntityId entity_id = 0;
    std::uint64_t tick = 0;
    std::array<float, 3> previous_position{};
    std::array<float, 3> current_position{};
    std::array<float, 3> current_velocity{};
    bool grounded = false;
    FootstepSurfaceHit surface{};
};

class FootstepTracker {
public:
    explicit FootstepTracker(FootstepTrackerConfig config = {});
    void reset(std::array<float, 3> position = {}) noexcept;
    [[nodiscard]] std::optional<stellar::protocol::GameplayEvent>
    update(const FootstepTrackerInput& input);
};

}
```

Cadence:

- Accumulate horizontal distance in the X/Y gameplay plane if Z is vertical in current core axes, or use the project's world-up helper if available.
- Use the same axis convention as movement.
- Emit only when grounded.
- Emit only when horizontal speed is above threshold.
- Reset accumulated distance on teleport/spawn/reset.
- Use distance, not wall-clock randomness.
- Do not emit while airborne.
- If no valid surface metadata exists, emit `generic`.
- Do not introduce randomness in authority.

Suggested first tuning for inch-scale retro movement:

```text
min_horizontal_speed = 24 inches/sec
walk_step_distance   = 56 inches
run_step_distance    = 44 inches
```

## Ground material lookup

In `WorldSession::tick` or a small helper owned by server core:

1. After authoritative movement, if the player is grounded:
   - run a filtered `CollisionWorld::probe_ground` from the player center or capsule bottom.
   - use the same `RuntimeCollisionState` filter/translation overlay used by movement.
   - read `hit.raycast.mesh_index` and `hit.raycast.triangle_index`.
   - map to `CollisionTriangle.surface.footstep_surface_id`.
2. Keep this helper backend-neutral:
   - no audio includes
   - no renderer includes
   - no client includes

Preferred helper:

```cpp
[[nodiscard]] FootstepSurfaceHit resolve_authoritative_ground_surface(
    const stellar::world::RuntimeWorld& world,
    const stellar::world::RuntimeCollisionState& collision_state,
    const MovementState& state,
    const MovementSimulationConfig& config) noexcept;
```

## Event propagation

Find the authority path that converts server/script results into `network::GameplayEvent` packets.

Add footstep events to the same server-approved event queue used by pickups, doors, and script events.

Likely touch points:

```text
include/stellar/server/WorldSession.hpp
src/server/WorldSession.cpp
include/stellar/server/ServerRuntime.hpp
src/server/ServerRuntime.cpp
include/stellar/authority/NetworkConversion.hpp
src/authority/NetworkConversion.cpp
include/stellar/client/SinglePlayerRuntime.hpp
src/client/SinglePlayerRuntime.cpp
tests/client/ClientWorldReceiver.cpp
```

Do not create a second event pipeline just for footsteps.

## Protocol codec tasks

Update:

```text
include/stellar/protocol/Types.hpp
src/network/SnapshotCodec.cpp
tests/network/SnapshotCodec.cpp
```

Required codec changes:

1. Add enum value `kFootstep = 5`.
2. Update `valid_event_kind` to include `kFootstep`.
3. Add round-trip test:
   ```cpp
   GameplayEvent{.kind = GameplayEventKind::kFootstep,
                 .tick = 123,
                 .entity_id = player_entity,
                 .player_id = 7,
                 .code = "wood",
                 .message = "wood/plank_01"}
   ```
4. Add invalid enum test if current test style supports it.

## Tests

Add/extend:

```text
tests/server/FootstepTracker.cpp
tests/server/WorldSession.cpp
tests/network/SnapshotCodec.cpp
tests/client/ClientWorldReceiver.cpp
tests/client/SinglePlayerRuntime.cpp
tests/server/ServerRuntime.cpp
```

Required coverage:

1. Tracker emits after enough grounded horizontal travel.
2. Tracker does not emit when idle.
3. Tracker does not emit while airborne.
4. Tracker resets on session reset.
5. Unknown surface falls back to generic.
6. `kFootstep` codec round-trips.
7. `ClientWorldReceiver` queues a `kFootstep` event for presentation.
8. Server/single-player runtime forwards footstep events through the existing event path.

## Acceptance criteria

- Server-approved footstep events are emitted deterministically.
- Ground material surface id comes from authoritative collision metadata.
- Client can receive/queue `kFootstep` events.
- No audible playback is required yet.
- No audio dependency is introduced into protocol/server/collision targets.

## Focused validation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build --target \
  stellar_snapshot_codec_test \
  stellar_client_world_receiver_test \
  stellar_server_world_session_test \
  stellar_server_runtime_test \
  stellar_client_single_player_runtime_test \
  -j$(nproc)
ctest --test-dir build -R '^(footstep_tracker|snapshot_codec|client_world_receiver|server_world_session|server_runtime|client_single_player_runtime)$' --output-on-failure
tools/dev/check_target_boundaries.sh .
```

## COMMIT CHECKPOINT

Suggested commit message:

```bash
git add include/stellar/protocol/Types.hpp src/network/SnapshotCodec.cpp tests/network/SnapshotCodec.cpp \
        include/stellar/server/FootstepTracker.hpp src/server/FootstepTracker.cpp \
        include/stellar/server/WorldSession.hpp src/server/WorldSession.cpp \
        src/server src/client src/authority tests/server tests/client docs/ImplementationStatus.md CMakeLists.txt
git commit -m "Emit authoritative footstep events"
```

Do not push.
## Global invariants for every phase

- Target branch: `audio-impl`.
- Do not push. Local commits are allowed only at the explicit checkpoints in each phase.
- Keep server authority intact. Footsteps are server-approved presentation events, not client guesses.
- Keep renderer, HUD, and audio presentation-only. They must not become sources of gameplay truth.
- Keep collision/runtime/audio contracts backend-neutral unless the phase explicitly works on a presentation sink.
- Keep default tests display-free. Real audio device checks must be opt-in/manual.
- BSP remains the canonical playable level format.
- Scope stays retro and practical: implement footsteps only, with generated one-shot sounds for now.
- Do not add full material gameplay, dynamic rigid bodies, animation systems, prediction/reconciliation, or a broad ECS rewrite.
- Public APIs need Doxygen `@brief` comments.
- Preserve deterministic ordering and deterministic event selection.
- Update `docs/ImplementationStatus.md` after each implemented phase.
- Update `docs/Design.md` or `docs/BspAuthoring.md` only when a broad architecture or authoring convention changes.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` without explicit user approval.

## Standard safe local commit checkpoint

At the end of any phase or subphase that says `COMMIT CHECKPOINT`, run:

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DSTELLAR_ENABLE_GLTF=ON \
  -DSTELLAR_ENABLE_LUA_SCRIPTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
tools/dev/check_target_boundaries.sh .
git diff --check
git status --short
```

If all relevant validation passes, make a local commit only:

```bash
git add <phase files>
git commit -m "<phase-specific message>"
```

Do **not** run `git push`.
