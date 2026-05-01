---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-2 — Live Client over Local Networked Transport Path

## Goal

Make the live mapped client use the transport/receiver path locally before implementing real sockets.

The client should no longer render mapped gameplay by directly reading `LocalLoopbackRuntime::latest_snapshot()` as its only path. Instead, mapped local play should send input through a `ClientTransport`, pump `LocalServerBridge`, drain `ClientWorldReceiver`, and render from the latest received authoritative `NetworkWorldSnapshot`.

## Why this phase comes first

The repo already has transport-neutral messages, local loopback transport, local authoritative bridge, and client receiver. The live client is the remaining direct-runtime consumer. Moving it to the networked path first reduces risk before sockets and ensures ST-3/ST-4 exercise the real presentation path.

## Primary owner

`@carmack`, with `@director` integration review. Ask `@miyamoto` only if render API changes become necessary.

## Input state

Inspect before coding:

- `include/stellar/client/Application.hpp`
- `src/client/Application.cpp`
- `src/client/ApplicationConfig.cpp`
- `include/stellar/client/LocalLoopbackRuntime.hpp`
- `include/stellar/client/LocalServerBridge.hpp`
- `include/stellar/client/ClientWorldReceiver.hpp`
- `include/stellar/network/Transport.hpp`
- `include/stellar/network/Messages.hpp`
- `include/stellar/network/SnapshotCodec.hpp`
- `include/stellar/network/SnapshotDelta.hpp`
- `include/stellar/client/GameplayPresentation.hpp`
- `include/stellar/client/PlayerPresentation.hpp`

## Implementation tasks

### ST-2.1 Active handoff/docs lock

Create or update root-level active socket transport plan files:

- `Plans/SocketTransport-AgentPlan.md`
- `Plans/ProjectStellar-SocketTransport-AgentPlan.md`

Update:

- `Plans/NEXT.md`
- `docs/ImplementationStatus.md`

Required docs content:

- Branch target is `socket-transport`.
- Active scope is remote socket transport and networked session lifecycle.
- Completed BSP gameplay-loop and PN scopes remain archived and must not be restarted.
- Phase ST-2 is active.
- Local mapped client should move to transport/receiver snapshots before socket work.

Do **not** modify `AGENTS.md` unless the user explicitly approves coordination-file edits.

### ST-2.2 Add network snapshot presentation adapters

Current presentation helpers consume `server::WorldSnapshot`. Add presentation-safe support for `network::NetworkWorldSnapshot`.

Preferred minimal API shape:

```cpp
[[nodiscard]] std::optional<PlayerPresentationState> make_player_presentation_state(
    const stellar::network::NetworkWorldSnapshot& snapshot,
    stellar::server::PlayerId player_id);

[[nodiscard]] GameplayPresentationFrame make_gameplay_presentation_frame(
    const stellar::network::NetworkWorldSnapshot& snapshot,
    const GameplayPresentationConfig& config = {}) noexcept;
```

Alternative acceptable shape:

- Add explicit conversion from `NetworkWorldSnapshot` to presentation-only structs.
- Do not reintroduce authoritative mutation on the client.
- Do not require GPU handles in network snapshots.
- Do not make renderer/audio/HUD state authoritative.

Tests:

- Player camera state can be extracted from a network snapshot.
- Active sprite/pickup entities become billboards.
- Inactive pickups are filtered.
- Door debug markers remain opt-in.
- Non-finite/suspicious values are sanitized exactly like existing presentation helpers.

### ST-2.3 Add a networked local runtime wrapper

Add a client-owned runtime wrapper, preferred name:

```cpp
class NetworkedClientRuntime;
```

It should own or reference:

- `LoopbackTransportPair`
- `LocalServerBridge`
- `ClientWorldReceiver`
- command sequence counter
- assigned/local player id state for this phase
- `MovementInputMapperConfig`

Frame update behavior:

1. Convert `platform::Input` to `server::MovementCommand`.
2. Build `NetworkPlayerCommand` with monotonic `command_sequence`.
3. Encode/send the command to the client transport.
4. Pump `LocalServerBridge` by `delta_seconds`.
5. Drain `ClientWorldReceiver`.
6. Return a frame result with latest snapshot/events/diagnostics.

Do not add prediction/reconciliation. The latest accepted authoritative network snapshot is the presentation source.

Suggested public API:

```cpp
struct NetworkedClientFrameResult {
    int ticks_run = 0;
    std::uint32_t rejected_packets = 0;
    bool dropped_excess_time = false;
    std::vector<stellar::network::GameplayEvent> events;
    std::vector<std::string> diagnostics;
};

class NetworkedClientRuntime {
public:
    static std::expected<NetworkedClientRuntime, stellar::platform::Error> create_local(
        const stellar::world::RuntimeWorld& world,
        /* existing script/session config inputs */);

    [[nodiscard]] NetworkedClientFrameResult update(
        const stellar::platform::Input& input,
        float delta_seconds) noexcept;

    [[nodiscard]] const std::optional<stellar::network::NetworkWorldSnapshot>&
    latest_snapshot() const noexcept;

    [[nodiscard]] stellar::server::PlayerId local_player_id() const noexcept;
};
```

Use project conventions and existing error types where possible.

### ST-2.4 Update `PreparedApplicationRuntime`

Extend `PreparedApplicationRuntime` so mapped local play can own the networked runtime path.

Acceptable migration strategy:

- Keep `LocalLoopbackRuntime` in the codebase for low-level tests and legacy fallback.
- Add `std::unique_ptr<NetworkedClientRuntime> networked_runtime`.
- Have `prepare_application_runtime()` construct `networked_runtime` for maps by default.
- Keep no-map debug cube fallback unchanged.

Scripted maps:

- Preserve existing script registry loading.
- If map metadata has trigger/object-collider script bindings, the server side of the local bridge must use `ScriptedWorldSession`.
- Script load failures must still fail preparation deterministically.

### ST-2.5 Update live client frame loop

In `Application::run()`:

- For mapped play, update `networked_runtime`.
- Render camera/player presentation from `NetworkWorldSnapshot`, not direct `WorldSession`.
- Route received gameplay events to existing HUD/audio caches only if those systems are already part of the live app. If live HUD/audio is not in the app yet, preserve event access for tests and do not invent new UI.
- Clear render view and presentation state when there is no networked snapshot.
- Keep no-map fallback behavior unchanged.

### ST-2.6 Tests

Add or update display-free tests:

- `networked_client_runtime`:
  - local runtime emits a first full snapshot after a frame.
  - movement input changes authoritative network snapshot player position.
  - script events and command results become queued gameplay events through receiver.
  - malformed packets are rejected without crashing.
  - command sequence increments deterministically.
- `client_map_validation_smoke`:
  - mapped runtime preparation creates networked runtime.
  - no-map fallback creates no networked runtime.
  - scripted maps report scripted runtime enabled.
- `gameplay_presentation` / `player_presentation`:
  - network snapshot overloads match existing server snapshot behavior.

## Out of scope

- Remote sockets.
- Connection handshake state.
- Dedicated server target.
- Prediction/reconciliation.
- Real multi-client simulation.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar_networked_client_runtime_test stellar_client_map_validation_smoke stellar_gameplay_presentation_test stellar_player_presentation_test -j$(nproc)
ctest --test-dir build -R '^(networked_client_runtime|client_map_validation_smoke|client_cli_map_validation|gameplay_presentation|player_presentation|client_world_receiver|loopback_transport|snapshot_)' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Completion checklist

- [ ] `Plans/NEXT.md` identifies socket transport as the active scope.
- [ ] `docs/ImplementationStatus.md` records ST-2 active/completed status.
- [ ] Live mapped client uses networked local path by default.
- [ ] Presentation can consume `NetworkWorldSnapshot`.
- [ ] Scripted local maps still work through server-authoritative scripting.
- [ ] No-map debug fallback still works.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.
