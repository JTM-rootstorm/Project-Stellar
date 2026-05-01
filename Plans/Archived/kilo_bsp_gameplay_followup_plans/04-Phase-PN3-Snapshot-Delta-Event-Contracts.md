# Phase PN-3 — Snapshot, Delta, and Event Transport Contracts

Optimized for Kilo Code intake.

## Owner

- Primary: `@carmack`
- Gameplay semantics review: `@kojima`
- Coordinator: `@director`

## Goal

Make authoritative snapshots and gameplay/presentation events transport-ready before adding remote transport.

This phase defines deterministic, serializable message/data contracts. It does not need to open sockets or implement prediction/reconciliation.

## Required reading

- `docs/Design.md`, especially networking and client-server sections
- `include/stellar/server/WorldSession.hpp`
- `include/stellar/server/GameplayWorld.hpp`
- `src/scripting/ScriptedWorldSession.cpp`
- `src/scripting/ScriptCommandProcessor.cpp`
- Phase PN-1 and PN-2 results

## Scope

Add a network/message contract layer that can represent:

- Client input commands.
- Server authoritative snapshots.
- Gameplay entity state.
- Presentation-approved events.
- Snapshot baselines and deltas.
- Serialization round-trips.

Do not add remote sockets in this phase. Do not implement client prediction yet.

## Recommended file layout

If no network directory exists, create:

```text
include/stellar/network/
src/network/
tests/network/
```

Suggested files:

```text
include/stellar/network/Messages.hpp
include/stellar/network/SnapshotCodec.hpp
include/stellar/network/SnapshotDelta.hpp
src/network/SnapshotCodec.cpp
src/network/SnapshotDelta.cpp
tests/network/SnapshotCodec.cpp
tests/network/SnapshotDelta.cpp
```

If an existing serialization/network layer exists, integrate there instead of creating duplicates.

## Message contract

Recommended initial message categories:

```cpp
enum class ClientMessageType {
    kInputCommand,
    kClientHello,
};

enum class ServerMessageType {
    kWelcome,
    kWorldSnapshot,
    kWorldDelta,
    kGameplayEvent,
};
```

Recommended payloads:

```cpp
struct NetworkPlayerCommand {
    std::uint32_t player_id;
    std::uint64_t command_sequence;
    server::MovementCommand movement;
};

struct NetworkGameplayEntity {
    server::EntityId id;
    server::EntityKind kind;
    server::TransformComponent transform;
    server::GameplayEntityMetadata metadata;
    bool active;
    bool open;
};

struct NetworkWorldSnapshot {
    std::uint64_t tick;
    std::vector<server::PlayerSnapshot> players;
    std::vector<NetworkGameplayEntity> entities;
};

enum class GameplayEventKind {
    kPickupCollected,
    kDoorStateChanged,
    kScriptCommandApplied,
    kScriptError,
};

struct GameplayEvent {
    GameplayEventKind kind;
    std::uint64_t tick;
    std::uint32_t entity_id;
    std::uint32_t player_id;
    std::string code;
    std::string message;
};
```

Keep strings bounded/validated in serialization.

## Delta rules

Implement deterministic deltas over `NetworkWorldSnapshot`:

- Baseline tick and target tick are explicit.
- Entity identity uses stable `EntityId`.
- Player identity uses stable `PlayerId`.
- Delta can include:
  - Added entities
  - Updated entities
  - Removed entities or inactive state changes
  - Updated players
- Preserve deterministic ordering by id.
- Applying a delta to its baseline should produce the target snapshot exactly.

Avoid compression algorithms in this phase. This is structural delta, not bandwidth optimization.

## Serialization strategy

Use the simplest deterministic binary or text format already present in the repo. If none exists, add a narrow local binary codec:

- Explicit little-endian integer writes.
- Finite float validation.
- Length-prefixed strings/vectors with reasonable maximums.
- `std::expected<T, Error>` for decode failure.
- No exceptions.
- No external dependency.

Recommended codec tests:

1. Snapshot round-trip with one player and no entities.
2. Snapshot round-trip with sprite/pickup/door entities.
3. Delta round-trip and apply.
4. Invalid/truncated data fails cleanly.
5. Oversized string/vector fails cleanly.
6. NaN/infinite floats fail decode or sanitize deterministically according to chosen policy.
7. Deterministic byte output for identical input.

## Event production

Add conversion helpers from runtime/script frame results to transport events:

- Pickup collected -> `kPickupCollected`
- Door/gate open state changed -> `kDoorStateChanged`
- Accepted script command -> `kScriptCommandApplied`
- Script error -> `kScriptError`

Events remain server-authored. Client presents them, but does not trust or mutate authority from them.

## Tests

Create or update:

```text
tests/network/SnapshotCodec.cpp
tests/network/SnapshotDelta.cpp
tests/server/WorldSession.cpp
tests/scripting/ScriptedWorldSession.cpp
```

## Validation commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target \
  stellar_snapshot_codec_test \
  stellar_snapshot_delta_test \
  stellar_server_world_session_test \
  stellar_scripted_world_session_test \
  -j$(nproc)

ctest --test-dir build -R '^(snapshot_codec|snapshot_delta|server_world_session|scripted_world_session)$' --output-on-failure
```

If target names differ, use the repo's test naming convention.

Then:

```bash
ctest --test-dir build --output-on-failure
```

## Acceptance criteria

- `WorldSnapshot` can convert to a transport-ready snapshot.
- Snapshot codec round-trips deterministically.
- Delta apply produces exact target snapshots in tests.
- Server-approved gameplay events are representable without renderer/audio dependencies.
- No remote transport or prediction has been added prematurely.
- Docs describe snapshot/delta/event contract as the basis for later remote networking.
