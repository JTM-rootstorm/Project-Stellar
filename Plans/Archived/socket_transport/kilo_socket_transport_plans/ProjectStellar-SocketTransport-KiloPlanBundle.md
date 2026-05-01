# Project Stellar — Socket Transport Kilo Plan Bundle

This bundle concatenates the master plan and phases ST-2 through ST-7 for easy AI-agent intake.


<!-- BEGIN 00-MASTER-KILO-SOCKET-TRANSPORT-PLAN.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Project Stellar — Socket Transport and Networked Session Plan

## Kilo intake summary

Implement the next development stage on branch `socket-transport`: move the live client onto the existing transport/receiver path, add a real connection/session lifecycle, implement Linux-first remote socket transport, add a minimal dedicated server entry point, add client connect mode, then harden, document, and archive the run.

This plan implements the previously suggested phases **2 through 7**:

- **Phase ST-2** — Live client over local networked transport path.
- **Phase ST-3** — Connection and session lifecycle.
- **Phase ST-4** — Remote socket transport.
- **Phase ST-5** — Dedicated server entry point.
- **Phase ST-6** — Client connect mode.
- **Phase ST-7** — Hardening, documentation, validation, and archival.

## Source-of-truth state at branch start

Branch `socket-transport` exists and starts clean from updated `main`.

Current repo facts to preserve:

- `Plans/NEXT.md` says BSP presentation/networking polish is complete and lists remote socket transport plus real multiplayer/session lifecycle as the first next option.
- `docs/ImplementationStatus.md` marks PN-0 through PN-6 complete and records final PN validation at 48/48 default tests.
- `ClientTransport` / `ServerTransport` already exist as transport-neutral packet seams.
- `LoopbackTransportPair` already exists for in-memory local client/server packet exchange.
- `NetworkPlayerCommand`, `NetworkWorldSnapshot`, `GameplayEvent`, snapshot codec, and snapshot delta contracts already exist.
- `LocalServerBridge` already decodes client command packets, advances plain or scripted authority, and emits full snapshots, deltas, and gameplay events.
- `ClientWorldReceiver` already accepts full snapshots, applies deltas to a baseline, queues gameplay events, and rejects malformed packets.
- The live client still uses `LocalLoopbackRuntime` directly for mapped play. This plan changes that before adding sockets.

## Non-negotiable invariants

- BSP remains the canonical playable level format.
- Server authority remains mandatory.
- Lua scripting remains mandatory, sandboxed, and server-authoritative.
- Import never executes scripts.
- Runtime collision, movement, triggers, object colliders, scripting, and networking contracts remain backend-neutral.
- Default tests remain display-free and must not require a GPU or external network.
- OpenGL/Vulkan remain runtime-selectable through the shared graphics abstraction.
- Rendering, audio, HUD, and UI remain presentation only and never become sources of gameplay truth.
- Do not add Source/VBSP, dynamic rigid bodies, moving brush simulation, full PBR, client-side gameplay scripting, model/animation systems, or retired importer functionality.
- Do not modify `AGENTS.md` or `.kilo/agents/*.md` unless the user explicitly authorizes coordination-file edits. Use `docs/ImplementationStatus.md` and `Plans/NEXT.md` as the active branch source of truth.

## Scope definition

This is the first real remote transport stage. It should prove remote client/server gameplay over sockets without prematurely solving prediction/reconciliation or full multi-player simulation.

For this plan, “real networked play” means:

- A client can connect to a running server over a local or LAN socket.
- The server owns the map, authoritative runtime, scripts, player slot, and snapshots.
- The client sends input commands through the transport.
- The client renders from authoritative network snapshots received through `ClientWorldReceiver`.
- Script-approved gameplay events reach HUD/audio/presentation paths through the network event queue.
- Connection handshakes, protocol mismatches, map mismatches, disconnects, timeouts, and malformed packets produce deterministic diagnostics.

### Explicitly deferred

- Client prediction.
- Client/server reconciliation.
- Snapshot interpolation beyond “latest authoritative snapshot”.
- True simultaneous multi-player movement simulation if `WorldSession` remains single-player. The protocol should be multi-client-ready, but do not claim simultaneous multi-player gameplay until server simulation supports it.
- UDP/unreliable transport if the first socket backend uses TCP.
- NAT traversal, matchmaking, encryption, compression, authentication, account identity, lobby systems, chat, and public Internet deployment.
- Production miniaudio playback beyond existing audio event routing/no-op sink.
- Rich HUD/VFX/sprite animation beyond server-approved event display already implemented.

## Recommended branch file plan

At phase start, create active root-level handoff files in the repo:

- `Plans/SocketTransport-AgentPlan.md` — concise active agent handoff.
- `Plans/ProjectStellar-SocketTransport-AgentPlan.md` — detailed master plan, adapted from this package.

At finalization, archive those active files under:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`

Do not move or rewrite completed historical packages under:

- `Plans/Archived/bsp_gameplay_loop/`
- `Plans/Archived/bsp_presentation_networking_polish/`
- Existing BSP migration/hardening archived folders.

## Phase dependency graph

```text
ST-2 local networked live-client mode
  └─ ST-3 session lifecycle
       ├─ ST-4 socket transport
       │    ├─ ST-5 dedicated server entry point
       │    │    └─ ST-6 client connect mode
       │    └─ ST-7 final hardening/docs/archive
       └─ ST-7 final hardening/docs/archive
```

Potential parallel work:

- ST-2 presentation adapters and ST-2 runtime wrapper can be implemented in parallel after API naming is agreed.
- ST-3 message codec extensions and connection-state tests can be implemented in parallel.
- ST-4 endpoint parsing and POSIX socket transport can be implemented in parallel with ST-3, but integration waits for ST-3 message lifecycle.
- ST-5 CLI parsing and server validation mode can start before ST-4 completes; live serving waits for ST-4.
- ST-6 client CLI plumbing can start after ST-3, but remote connect integration waits for ST-4/ST-5.

## Agent routing

Use existing Kilo agents only.

- `@director`: phase sequencing, architecture decisions, integration review, docs/status, and conflict resolution.
- `@carmack`: primary owner for network contracts, socket transport, server loop, CMake targets, deterministic tests, and validation.
- `@miyamoto`: consult only when client rendering/presentation wiring touches `LevelRenderer`, `GameplayPresentation`, or player camera presentation.
- `@suzuki`: consult only if audio routing behavior changes. No production miniaudio work in this plan.
- `@kojima`: consult only if gameplay behavior/archetype semantics change. Avoid gameplay-rule expansion in this plan.

## Implementation target architecture

```text
stellar-server
  ├─ loads/validates BSP map
  ├─ builds RuntimeWorld
  ├─ loads server-authoritative Lua scripts
  ├─ owns WorldSession or ScriptedWorldSession
  ├─ accepts socket connection(s)
  ├─ receives NetworkPlayerCommand packets
  ├─ validates assigned player slot
  ├─ advances authoritative ticks
  └─ emits Welcome, WorldSnapshot, WorldDelta, GameplayEvent packets

stellar-client --connect HOST:PORT
  ├─ opens socket ClientTransport
  ├─ sends ClientHello
  ├─ receives Welcome/session assignment
  ├─ sends NetworkPlayerCommand packets from local input
  ├─ drains ClientWorldReceiver
  ├─ renders player/camera/gameplay presentation from NetworkWorldSnapshot
  └─ routes server-approved GameplayEvent records to HUD/audio/presentation caches
```

Local mapped play should also use the transport path:

```text
stellar-client --map path/to/map.bsp
  ├─ creates LoopbackTransportPair
  ├─ owns LocalServerBridge
  ├─ sends local input as encoded NetworkPlayerCommand
  ├─ drains ClientWorldReceiver
  └─ renders from received NetworkWorldSnapshot
```

## Naming guidance

Prefer these names unless the existing tree makes a better local convention obvious:

- `include/stellar/client/NetworkedClientRuntime.hpp`
- `src/client/NetworkedClientRuntime.cpp`
- `tests/client/NetworkedClientRuntime.cpp`
- `include/stellar/network/Session.hpp`
- `src/network/Session.cpp`
- `tests/network/Session.cpp`
- `include/stellar/network/SocketTransport.hpp`
- `src/network/SocketTransport.cpp`
- `tests/network/SocketTransport.cpp`
- `src/server/main.cpp`
- `include/stellar/server/DedicatedServer.hpp`
- `src/server/DedicatedServer.cpp`
- `tests/server/DedicatedServer.cpp`

If different names are chosen, update all plan/status docs with the final names.

## Validation policy

Every phase must run the smallest useful focused validation plus a full default CTest before being marked complete. Keep tests display-free. Localhost socket tests may use loopback only; never require an external network or public port.

Baseline validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Final-focused validation should include at least:

```bash
ctest --test-dir build -R '^(networked_client_runtime|network_session|socket_transport|dedicated_server|client_connect|client_world_receiver|loopback_transport|snapshot_|bsp_|scripted_world_session|gameplay_presentation|hud_presentation|audio_event_router)' --output-on-failure
```

## Completion criteria for the whole plan

The implementation is complete when:

- Live local mapped client play can run through `LoopbackTransportPair + LocalServerBridge + ClientWorldReceiver`.
- The direct `LocalLoopbackRuntime` path is no longer the only live mapped client path.
- Handshake/welcome/session state exists and rejects mismatched or malformed clients deterministically.
- A remote socket `ClientTransport` can connect to a remote socket server endpoint over localhost.
- A `stellar-server` target can validate and host a BSP map.
- A `stellar-client --connect HOST:PORT` mode can connect and render from authoritative network snapshots.
- Scripted pickup/door events can travel through the network path into client HUD/audio/presentation queues.
- Docs accurately state what is implemented and what remains deferred.
- Active socket transport plans are archived after completion.
- Full default tests pass.


<!-- END 00-MASTER-KILO-SOCKET-TRANSPORT-PLAN.md -->


<!-- BEGIN 02-Phase-ST2-Live-Client-Transport-Mode.md -->

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


<!-- END 02-Phase-ST2-Live-Client-Transport-Mode.md -->


<!-- BEGIN 03-Phase-ST3-Connection-Session-Lifecycle.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-3 — Connection and Session Lifecycle

## Goal

Add deterministic connection/session lifecycle messages and state machines over existing transports before implementing real sockets.

This phase establishes how a client joins a server, how the server assigns authority, how protocol/map mismatches are rejected, and when input is accepted.

## Primary owner

`@carmack`, with `@director` architecture review.

## Design scope

Support a first-stage networked session with one active local player slot unless `WorldSession` has already been expanded for multiple players.

The protocol should be multi-client-ready, but implementation must not claim simultaneous multi-player gameplay until server simulation actually supports multiple authoritative players.

## Implementation tasks

### ST-3.1 Add session protocol types

Add a new public header or extend existing message headers. Preferred:

- `include/stellar/network/Session.hpp`
- `src/network/Session.cpp`
- `tests/network/Session.cpp`

Required concepts:

```cpp
using ProtocolVersion = std::uint32_t;
using ConnectionId = std::uint32_t;
using SessionId = std::uint64_t;

enum class SessionState : std::uint8_t {
    kDisconnected,
    kConnecting,
    kConnected,
    kRejected,
    kTimedOut,
};

struct ClientHello {
    ProtocolVersion protocol_version;
    std::string client_name;
    std::string requested_map_id;
    std::uint64_t client_nonce;
};

struct ServerWelcome {
    bool accepted;
    ProtocolVersion protocol_version;
    SessionId session_id;
    stellar::server::PlayerId assigned_player_id;
    std::string map_id;
    std::string rejection_code;
    std::string message;
};
```

Use final field names that fit project style. All public APIs require Doxygen `@brief`.

### ST-3.2 Define map identity

Add a deterministic map identity helper for session compatibility.

Minimum viable map identity:

- source path basename or normalized source URI
- optional content hash if available or easy to compute deterministically
- imported level source URI
- script identity is not required for this phase, but script load failures must still reject startup

Preferred helper:

```cpp
struct MapIdentity {
    std::string map_id;
    std::string source_uri;
    std::uint64_t content_hash = 0;
};
```

Do not block the phase on cryptographic hashing. A deterministic FNV-1a-style byte hash is acceptable if no project hash helper exists.

### ST-3.3 Extend binary codec

Add encode/decode support for:

- `ClientHello`
- `ServerWelcome`
- optional `ClientDisconnect`
- optional `ServerDisconnect`
- session rejection diagnostics

Requirements:

- Explicit little-endian fields like existing codec.
- Bounded strings.
- Clean `std::expected` failures for malformed/truncated/oversized messages.
- Protocol version mismatch produces deterministic rejection.
- Unknown message types are rejected without crashing.

### ST-3.4 Add session state machines

Add small state machines:

- client-side session state for hello/welcome/rejected/disconnected
- server-side pending/accepted session state

Requirements:

- Input commands before accepted welcome are rejected or ignored deterministically.
- Server overwrites/validates requested player id with assigned player id.
- The initial server snapshot is sent only after welcome acceptance.
- A rejected client receives an explanatory `ServerWelcome{accepted=false}` or equivalent.

### ST-3.5 Integrate session lifecycle with local networked runtime

Update ST-2 local networked runtime:

- On create, send `ClientHello`.
- Pump bridge/session until `ServerWelcome` accepted.
- Store assigned player id.
- Use assigned player id for outgoing commands and presentation extraction.
- Expose connection/session diagnostics in frame results.

For local mapped play, map identity should match automatically.

### ST-3.6 Tests

Add display-free tests:

- Client hello round-trip.
- Welcome accepted with assigned player id.
- Protocol version mismatch rejected.
- Map mismatch rejected.
- Input before welcome ignored/rejected.
- After welcome, input commands affect authoritative snapshot.
- Malformed session packets are rejected without crashing.
- Assigned player id is used for presentation extraction.

## Out of scope

- Remote sockets.
- Dedicated server CLI.
- Timeout based on real wall clock unless simple deterministic tick/frame counters are already available.
- Multi-player simulation expansion.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_network_session_test stellar_snapshot_codec_test stellar_networked_client_runtime_test stellar_client_world_receiver_test -j$(nproc)
ctest --test-dir build -R '^(network_session|snapshot_codec|networked_client_runtime|client_world_receiver|loopback_transport)$' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: describe session lifecycle and first-stage active-player limit.
- `docs/ImplementationStatus.md`: record ST-3 completion notes and validation.
- `Plans/NEXT.md`: keep ST scope active; do not switch to post-ST yet.

## Completion checklist

- [ ] Session protocol types exist.
- [ ] Codec handles hello/welcome/reject deterministically.
- [ ] Local networked runtime uses handshake before input.
- [ ] Server authority assigns player id.
- [ ] Mismatched protocol/map is rejected.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.


<!-- END 03-Phase-ST3-Connection-Session-Lifecycle.md -->


<!-- BEGIN 04-Phase-ST4-Remote-Socket-Transport.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-4 — Remote Socket Transport

## Goal

Implement Linux-first remote socket transport behind the existing transport abstraction so client/server packets can move over localhost or LAN without changing gameplay/message contracts.

## Primary owner

`@carmack`.

## Transport strategy

Use a minimal, deterministic, testable socket backend. The recommended first implementation is **TCP with length-prefixed packets**.

Rationale:

- The existing transport interface already models reliable FIFO delivery.
- TCP lets this stage prove real remote client/server gameplay without building reliability/ordering/loss recovery.
- The `TransportChannel` field should still be preserved in the packet envelope for future UDP/unreliable work.

True UDP/unreliable networking remains deferred unless the user explicitly scopes it.

## Implementation tasks

### ST-4.1 Add socket endpoint parsing

Add:

- `include/stellar/network/SocketAddress.hpp` or inside `SocketTransport.hpp`
- parser for `host:port`
- validation for empty host, invalid port, out-of-range port
- deterministic error codes/messages

Examples:

- `127.0.0.1:29070`
- `localhost:29070`
- `0.0.0.0:29070` for server bind

Tests cover valid and invalid endpoints.

### ST-4.2 Add TCP packet envelope

Define a compact packet envelope:

```text
magic/version
channel byte
payload length uint32 little-endian
payload bytes
```

Requirements:

- Maximum payload size constant.
- Reject oversized payloads before allocation.
- Handle partial reads/writes.
- Preserve packet boundaries.
- Decode errors do not crash or throw.
- Public APIs use `std::expected`.

### ST-4.3 Implement client socket transport

Preferred files:

- `include/stellar/network/SocketTransport.hpp`
- `src/network/SocketTransport.cpp`
- `tests/network/SocketTransport.cpp`

Preferred API:

```cpp
struct SocketTransportConfig {
    std::size_t max_packet_bytes = ...;
    bool nonblocking = true;
};

[[nodiscard]] std::expected<std::unique_ptr<ClientTransport>, TransportError>
connect_tcp_client(std::string_view endpoint, SocketTransportConfig config = {});
```

Client behavior:

- Connects to server endpoint.
- `send_to_server()` writes one encoded packet envelope.
- `receive_from_server()` drains all available complete packets.
- Clean disconnect is reported deterministically.
- Nonblocking mode must not spin or block tests indefinitely.

### ST-4.4 Implement server socket transport

Because the existing `ServerTransport` is single-client, pick one of these approaches:

Preferred for this phase:

1. Implement a single-client `TcpServerTransport` that satisfies `ServerTransport`.
2. Add a clearly named future-ready multi-client facade only if needed by ST-5.

Minimum viable API:

```cpp
[[nodiscard]] std::expected<std::unique_ptr<ServerTransport>, TransportError>
listen_tcp_server_once(std::string_view bind_endpoint, SocketTransportConfig config = {});
```

Behavior:

- Bind/listen on configured address.
- Accept one client connection.
- Drain client packets.
- Send server packets.
- Detect disconnect.
- No external network required for tests.

If a multi-client server transport is added, keep it additive and do not break existing `ServerTransport`.

### ST-4.5 CMake and platform guards

- Linux/POSIX sockets are acceptable for this branch.
- Add compile-time checks or platform guards for non-POSIX platforms.
- Keep default build working on Linux.
- Do not introduce new third-party networking dependencies unless the user approves.

### ST-4.6 Socket tests

Add local-only tests using loopback and ephemeral ports where possible.

Test cases:

- Client connects to local server.
- Reliable packet arrives FIFO.
- Multiple packets preserve boundaries.
- Partial write/read handling works.
- Oversized payload rejected.
- Invalid endpoint rejected.
- Disconnect is surfaced without crash.
- Session hello/welcome can cross real socket transport.
- Snapshot packet can cross real socket transport and decode through `ClientWorldReceiver`.

Avoid timing-fragile sleeps. Prefer nonblocking pump loops with small bounded iteration counts.

## Out of scope

- UDP.
- Packet loss simulation.
- Encryption/TLS.
- NAT traversal.
- Public Internet server discovery.
- Multi-player server simulation.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_socket_transport_test stellar_network_session_test stellar_snapshot_codec_test -j$(nproc)
ctest --test-dir build -R '^(socket_transport|network_session|snapshot_codec|loopback_transport)$' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: socket transport is TCP-first, Linux-first, transport-neutral, and not prediction/reconciliation.
- `docs/ImplementationStatus.md`: ST-4 notes and validation.
- `Plans/NEXT.md`: active phase moves to dedicated server entry point.

## Completion checklist

- [ ] Socket endpoint parser exists and is tested.
- [ ] Client socket transport implements `ClientTransport`.
- [ ] Server socket transport implements `ServerTransport`.
- [ ] Packet envelope handles partial read/write and bounds.
- [ ] Hello/welcome crosses real socket transport.
- [ ] Snapshot packet crosses real socket transport.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.


<!-- END 04-Phase-ST4-Remote-Socket-Transport.md -->


<!-- BEGIN 05-Phase-ST5-Dedicated-Server-Entry-Point.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-5 — Dedicated Server Entry Point

## Goal

Add a minimal `stellar-server` executable that loads a BSP map, owns authoritative runtime/session state, accepts a socket client, and emits authoritative snapshots/deltas/events.

## Primary owner

`@carmack`, with `@director` integration review.

## First-stage server scope

The server should support one active player connection unless `WorldSession` has been explicitly expanded for multiple authoritative players.

Required behavior:

- Server owns map loading.
- Server owns script loading.
- Server owns player id assignment.
- Server owns authoritative ticks.
- Server emits snapshots/deltas/gameplay events over transport.
- Client never owns gameplay truth.

Do not claim full simultaneous multi-player gameplay unless implemented and tested.

## Implementation tasks

### ST-5.1 Add server runtime abstraction

Preferred files:

- `include/stellar/server/DedicatedServer.hpp`
- `src/server/DedicatedServer.cpp`
- `tests/server/DedicatedServer.cpp`

Suggested types:

```cpp
struct DedicatedServerConfig {
    std::string map_path;
    std::optional<std::string> script_root;
    std::string listen_endpoint = "127.0.0.1:29070";
    int tick_rate = 60;
    int snapshot_rate = 20;
    int max_clients = 1;
    bool validate_only = false;
};

struct DedicatedServerStatus {
    bool running = false;
    std::string map_id;
    std::uint64_t tick = 0;
    std::uint32_t connected_clients = 0;
};

class DedicatedServer {
public:
    static std::expected<DedicatedServer, stellar::platform::Error> create(
        DedicatedServerConfig config);

    [[nodiscard]] std::expected<void, stellar::platform::Error> run();
    [[nodiscard]] DedicatedServerStatus status() const noexcept;
};
```

Adapt names to project style.

### ST-5.2 Reuse existing map/script prep

Do not duplicate BSP/script loading logic if it can be factored safely from client prep.

Acceptable refactor:

- Move shared map/script/runtime preparation into a backend-neutral helper under `stellar::server` or `stellar::world`.
- Client and server call the same helper.
- Keep platform/window/graphics dependencies out of server code.

The server must fail deterministically for:

- missing map
- non-BSP extension
- invalid BSP
- script path escape
- missing script source
- script load failure

### ST-5.3 Integrate session lifecycle and socket transport

Server run loop responsibilities:

1. Create socket server transport.
2. Accept/connect one client.
3. Wait for `ClientHello`.
4. Validate protocol and map identity.
5. Send `ServerWelcome`.
6. Receive client input commands.
7. Advance authoritative tick at fixed rate.
8. Send full snapshot first, then deltas when available.
9. Send server-approved gameplay events.
10. Detect disconnect and shut down or return to waiting state.

For tests, expose a bounded `pump_once(delta_seconds)` path rather than requiring an infinite loop.

### ST-5.4 Add `stellar-server` executable

Add `src/server/main.cpp` or equivalent and CMake target:

```bash
stellar-server --map path/to/map.bsp --listen 127.0.0.1:29070
stellar-server --validate-config --map path/to/map.bsp
stellar-server --map path/to/map.bsp --script-root path/to/scripts --listen 0.0.0.0:29070
```

CLI requirements:

- Unknown args fail deterministically.
- `--map` is required except maybe help mode.
- `--validate-config` loads/validates map/scripts and exits before opening socket.
- `--listen` parses endpoint.
- `--tick-rate` and `--snapshot-rate` validate finite positive integer ranges.
- `--max-clients` defaults to 1 for this phase.

### ST-5.5 Tests

Display-free tests:

- Server config parse success/failure.
- Validate-only map path succeeds for generated BSP fixture.
- Invalid map path fails.
- Missing script fails when map references script.
- Server accepts local socket client hello and sends welcome.
- Server sends first full snapshot.
- Server receives input command and updates snapshot.
- Scripted pickup/door event crosses server runtime into gameplay event packets.
- Disconnect does not crash.

Use generated BSP fixtures already in tests. Do not require a display/GPU.

## Out of scope

- Running a public Internet server.
- Multi-process flaky integration tests unless deterministic and bounded.
- Multi-player simulation beyond the current session implementation.
- Server UI/admin console.
- Hot map reload.
- Persistence/save games.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-server stellar_dedicated_server_test stellar_socket_transport_test stellar_network_session_test -j$(nproc)
ctest --test-dir build -R '^(dedicated_server|socket_transport|network_session|snapshot_|scripted_world_session|bsp_)' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: document `stellar-server` as authoritative entry point.
- `docs/ImplementationStatus.md`: ST-5 completion notes.
- `Plans/NEXT.md`: active phase moves to client connect mode.
- Optional new `docs/Networking.md` if the implementation details need a focused reference.

## Completion checklist

- [ ] `stellar-server` target exists.
- [ ] `--validate-config --map` works display-free.
- [ ] Server can accept one socket client.
- [ ] Server owns map/script/runtime/session.
- [ ] Welcome and first snapshot sent.
- [ ] Input updates authoritative snapshot.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.


<!-- END 05-Phase-ST5-Dedicated-Server-Entry-Point.md -->


<!-- BEGIN 06-Phase-ST6-Client-Connect-Mode.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-6 — Client Connect Mode

## Goal

Add a `stellar-client --connect HOST:PORT` mode that connects to `stellar-server`, performs session handshake, sends input commands, receives authoritative snapshots/deltas/events, and renders from the received network state.

## Primary owner

`@carmack` for client/server/network integration. Consult `@miyamoto` only if renderer APIs must change.

## Runtime behavior

`stellar-client --connect HOST:PORT`:

- Does not require `--map` for authoritative gameplay state.
- Opens socket `ClientTransport`.
- Sends `ClientHello`.
- Receives `ServerWelcome`.
- Stores assigned player id.
- Sends input commands with monotonic command sequence.
- Drains `ClientWorldReceiver`.
- Renders camera and gameplay billboards from `NetworkWorldSnapshot`.
- Routes server-approved gameplay events to HUD/audio/presentation caches if live app already owns those caches.
- Rejects malformed or mismatched packets without crashing.

A combined local-host development command should work:

```bash
stellar-server --map path/to/map.bsp --listen 127.0.0.1:29070
stellar-client --connect 127.0.0.1:29070
```

## Implementation tasks

### ST-6.1 Extend application config and CLI

Update:

- `include/stellar/client/ApplicationConfig.hpp`
- `src/client/main.cpp`
- `src/client/ApplicationConfig.cpp`
- `src/client/Application.cpp`

Add config fields similar to:

```cpp
std::optional<std::string> connect_endpoint;
std::string client_name = "local";
```

CLI:

```bash
stellar-client --connect 127.0.0.1:29070
stellar-client --connect localhost:29070 --client-name Mike
stellar-client --validate-config --connect 127.0.0.1:29070
```

Conflict rules:

- `--map` without `--connect`: local networked runtime mode from ST-2.
- `--connect` without `--map`: remote client mode.
- `--map` plus `--connect`: either reject as ambiguous or treat map as optional compatibility expectation. Prefer reject unless Design.md says otherwise.
- `--script-root` only applies to local map/server-authority prep; remote client must not load gameplay scripts.

### ST-6.2 Add remote client runtime

Extend `NetworkedClientRuntime` or add `RemoteClientRuntime`.

Responsibilities:

- Own socket `ClientTransport`.
- Send hello and process welcome.
- Convert local input to `NetworkPlayerCommand`.
- Send commands only after accepted welcome.
- Drain `ClientWorldReceiver`.
- Expose latest snapshot, events, connection state, diagnostics, assigned player id.

Do not include prediction/reconciliation.

### ST-6.3 Update live render path

In remote connect mode:

- Use assigned player id from welcome.
- Extract camera from latest `NetworkWorldSnapshot`.
- Build gameplay billboard presentation from latest `NetworkWorldSnapshot`.
- If no snapshot exists yet, clear render view/presentation or show fallback camera safely.
- Do not create authoritative local runtime or load gameplay scripts on the client.

Renderer still needs a level to draw static BSP geometry. Pick one strategy and document it clearly:

Preferred minimal strategy:

1. Server welcome includes map identity only.
2. Remote client initially renders dynamic sprites/camera fallback until a future map distribution/caching phase.
3. For developer flow, allow optional `--presentation-map path/to/map.bsp` that loads the same BSP for static presentation only and validates map identity against welcome.

Alternative:

- Keep `--map` + `--connect` as presentation-only map path, explicitly documented as non-authoritative.

Do not implement map file transfer in this phase.

### ST-6.4 Route gameplay events

Connect received `GameplayEvent` records to existing presentation sinks:

- HUD cache.
- Audio event router/no-op sink.
- Tests should verify events arrive; live UI display can remain minimal if no HUD renderer exists.

### ST-6.5 Tests

Display-free tests:

- CLI parse accepts `--connect`.
- CLI parse rejects ambiguous or invalid combinations.
- Remote runtime sends hello.
- Remote runtime accepts welcome.
- Remote runtime sends input only after welcome.
- Remote runtime receives snapshot and exposes player presentation.
- Remote runtime receives gameplay events and drains them.
- Connect mode does not construct local authoritative runtime.
- Presentation-map behavior, if implemented, validates map identity or fails deterministically.

A bounded localhost integration test may start a server runtime in-process, not necessarily as a child process.

## Out of scope

- Map download/transfer.
- Prediction/reconciliation.
- Interpolation.
- Rich connection UI.
- Passwords/authentication.
- Public server browser.
- Hot reconnect after process restart.

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar-client stellar-server stellar_client_connect_test stellar_dedicated_server_test stellar_socket_transport_test -j$(nproc)
ctest --test-dir build -R '^(client_connect|dedicated_server|socket_transport|network_session|networked_client_runtime|client_world_receiver|gameplay_presentation|player_presentation)' --output-on-failure
```

Full:

```bash
ctest --test-dir build --output-on-failure
```

## Documentation updates

Update:

- `docs/Design.md`: remote client mode and presentation-map policy.
- `docs/ImplementationStatus.md`: ST-6 completion notes.
- `Plans/NEXT.md`: active phase moves to hardening/docs/archive.
- `docs/BspAuthoring.md`: only if map identity/presentation-map workflow affects authoring validation.

## Completion checklist

- [ ] `stellar-client --connect HOST:PORT` exists.
- [ ] Remote client performs hello/welcome.
- [ ] Remote client sends input through socket transport.
- [ ] Remote client renders from network snapshot.
- [ ] Gameplay events reach client presentation queues.
- [ ] Client does not load gameplay scripts in remote mode.
- [ ] Static map presentation policy is documented.
- [ ] Focused tests pass.
- [ ] Full default CTest passes.


<!-- END 06-Phase-ST6-Client-Connect-Mode.md -->


<!-- BEGIN 07-Phase-ST7-Hardening-Docs-Validation.md -->

---
project: Project Stellar
branch: socket-transport
plan_family: socket_transport
agent_optimized_for: Kilo Code
status: draft_handoff
generated: 2026-05-01
---

# Phase ST-7 — Hardening, Documentation, Validation, and Archival

## Goal

Close the `socket-transport` implementation run with deterministic validation, accurate documentation, and archived active plans.

## Primary owner

`@director` for docs/integration review. `@carmack` fixes network/server/test blockers.

## Implementation tasks

### ST-7.1 Protocol and authority audit

Run code review and grep audits for these risks:

- client creates or mutates authoritative gameplay state in remote mode
- client loads gameplay scripts in remote mode
- renderer/audio/HUD/UI marked or treated as authoritative
- input accepted before server welcome
- player id trusted from client rather than assigned by server
- socket tests requiring external network
- blocking loops with no bounded exit in tests
- map mismatch ignored silently
- protocol mismatch ignored silently

Suggested greps:

```bash
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'TODO.*prediction\|TODO.*reconciliation\|predict' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'LocalLoopbackRuntime' -- src/client include/stellar/client tests/client ':!build*/**'
```

Interpretation matters: documented prohibitions and tests may be valid hits. Source authority violations are not.

### ST-7.2 Network robustness audit

Verify:

- malformed packets are rejected with deterministic errors
- oversized packets are rejected before large allocation
- partial reads/writes are handled
- disconnects do not crash
- connection state recovers or fails cleanly
- server validates assigned player id
- full snapshots reset receiver baseline
- deltas require baseline
- gameplay events are queued/drained once
- socket resources close in destructors
- tests do not hang if sockets fail

### ST-7.3 Documentation finalization

Update:

- `docs/ImplementationStatus.md`
  - Add completed socket transport scope section at the top.
  - Record ST-2 through ST-7 completion notes.
  - Include validation commands and results.
  - List known deferred post-ST items.

- `Plans/NEXT.md`
  - Mark socket transport/session lifecycle complete.
  - Point to next recommended options:
    - client interpolation
    - client prediction/reconciliation
    - true multi-player simulation if still deferred
    - UDP/unreliable transport
    - map distribution/caching
    - richer HUD/UI/VFX
    - miniaudio playback
    - sprite atlas/sheet animation

- `docs/Design.md`
  - Describe actual socket transport implementation.
  - State TCP-first or actual chosen protocol honestly.
  - Describe `stellar-server`.
  - Describe `stellar-client --connect`.
  - Clarify what is not implemented.

- Optional `docs/Networking.md`
  - Add only if networking behavior is now detailed enough to deserve a focused doc.
  - Keep it factual and aligned with code.

- `docs/BspAuthoring.md`
  - Update only if server/client workflows changed map validation or presentation-map expectations.

Do not alter `AGENTS.md` without explicit user approval.

### ST-7.4 Plan archival

Move active socket transport plans:

From:

- `Plans/SocketTransport-AgentPlan.md`
- `Plans/ProjectStellar-SocketTransport-AgentPlan.md`

To:

- `Plans/Archived/socket_transport/SocketTransport-AgentPlan.md`
- `Plans/Archived/socket_transport/ProjectStellar-SocketTransport-AgentPlan.md`

If Kilo imported this package into additional `Plans/` files, archive those too under:

- `Plans/Archived/socket_transport/kilo_socket_transport_plans/`

Ensure root `Plans/` contains only current/next active handoff files, not completed ST phase plans.

### ST-7.5 Final validation

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(networked_client_runtime|network_session|socket_transport|dedicated_server|client_connect|client_world_receiver|loopback_transport|snapshot_|bsp_|runtime_world|server_world_session|scripted_world_session|script_command_processor|gameplay_presentation|player_presentation|hud_presentation|audio_event_router)' --output-on-failure
git grep -n -i 'STELLAR_ENABLE_GLTF\|cgltf\|SceneAsset\|gltf' -- . ':!Plans/Archived/**' ':!build*/**'
git grep -n -i 'client-side gameplay scripting\|client gameplay script\|renderer-owned gameplay\|audio-owned gameplay' -- docs include src tests ':!Plans/Archived/**' ':!build*/**'
git grep -n 'SocketTransport-AgentPlan\|ProjectStellar-SocketTransport-AgentPlan' -- Plans docs ':!Plans/Archived/**'
```

Expected final audit interpretation:

- Retired importer hits should be documentation-only or absent.
- Authority-violation hits should be prohibitions/docs only, not active implementation.
- Socket plan filename search should not find root active plan files after archival, except documented historical references.

### ST-7.6 Final status summary

At the end of implementation, produce a concise status in `docs/ImplementationStatus.md`:

- What was implemented.
- What validation passed.
- What remains deferred.
- Any known manual validation not run.
- Any socket platform limitations.

## Out of scope

- Starting the next implementation scope.
- Adding features to make tests pass if those features belong to deferred work.
- Updating AGENTS coordination rules without explicit user approval.

## Completion checklist

- [ ] ST-2 through ST-6 completion notes exist.
- [ ] Socket transport plans are archived.
- [ ] `Plans/NEXT.md` points to post-ST options.
- [ ] `docs/Design.md` accurately describes implemented networking.
- [ ] `docs/ImplementationStatus.md` is the branch source of truth.
- [ ] Full default CTest passes.
- [ ] Focused ST/BSP/runtime/client/server tests pass.
- [ ] Audits interpreted and recorded.
- [ ] Known deferred work listed honestly.


<!-- END 07-Phase-ST7-Hardening-Docs-Validation.md -->
