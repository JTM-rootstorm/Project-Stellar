---
project: Project Stellar
branch: client-server-split
plan_family: client_server_decoupling
agent_optimized_for: Kilo Code / Codex-style implementation agents
status: proposed_handoff
generated: 2026-05-03
---

# Project Stellar — Client/Server Decoupling Architecture Plan

## Intake Summary

The current implementation has the right first-generation networking pieces, but the ownership boundaries are inverted in several places:

- `stellar-client --connect HOST:PORT` already behaves like a presentation-only remote client.
- `stellar-server` already behaves like a headless authoritative dedicated server.
- Local mapped play still makes the client own a transport-backed server bridge and authoritative runtime setup.
- Network protocol types still depend directly on `stellar::server` and `stellar::scripting` implementation types.
- Client startup/config code and dedicated server startup duplicate BSP/script/authority preparation logic.
- `LocalServerBridge` lives in `stellar::client` even though it owns authoritative server session state.

This plan decouples those pieces without discarding the existing socket/session work.

The target architecture has four explicit runtime modes:

1. **Single-player client**
   - `stellar-client --map path/to/map.bsp`
   - No socket listener.
   - No `ClientHello` / `ServerWelcome`.
   - No dedicated or listen server instance is started.
   - The client owns a local single-player session wrapper over shared authoritative simulation code.

2. **Remote client**
   - `stellar-client --connect HOST:PORT`
   - Presentation/input/network client only.
   - Does not load gameplay scripts.
   - Does not create local authority.
   - Does not own server/session internals.

3. **Listen-server host client**
   - Proposed CLI: `stellar-client --host --map path/to/map.bsp [--listen 127.0.0.1:29070]`
   - Starts an in-process server host whose lifetime is tied to the host client.
   - Local host player connects through the same client-facing network/session interface used by remote clients.
   - When the host client exits, the server host terminates and closes transports.

4. **Dedicated server**
   - `stellar-server --map path/to/map.bsp --listen HOST:PORT`
   - Headless server process.
   - Exists independently of clients.
   - Uses the same shared server-host/runtime code as listen-server mode.

## Non-Negotiable Invariants

- Server authority remains mandatory for networked/listen/dedicated play.
- Single-player mode may run authoritative simulation in-process, but it must not start a network server or listen socket.
- Remote client mode must not load gameplay scripts, construct `WorldSession`, construct `ScriptedWorldSession`, or own authoritative runtime state.
- Rendering, audio, HUD, and UI remain presentation-only.
- BSP remains the canonical playable level format.
- Lua remains sandboxed and authoritative-side only.
- Default validation and tests remain display-free.
- Existing socket protocol behavior should be preserved unless a step explicitly replaces it.
- Do not add prediction, reconciliation, interpolation, map transfer, UDP, authentication, matchmaking, or true simultaneous multiplayer unless separately scoped.

---

# Current State Audit

## Existing pieces to preserve

- `stellar-server` exists and owns BSP/script loading, player assignment, authoritative ticks, snapshots/deltas, and gameplay event emission.
- `RemoteClientRuntime` exists and owns only socket client transport, session state, input command encoding, and `ClientWorldReceiver`.
- `NetworkedClientRuntime` exists for local mapped play over `LoopbackTransportPair + LocalServerBridge + ClientWorldReceiver`.
- The live app already renders both local-networked and remote snapshots through `NetworkWorldSnapshot`.
- Socket transport, `ClientHello`, `ServerWelcome`, `ClientWorldReceiver`, and snapshot/delta/event codecs are already implemented.

## Main coupling problems to remove

1. **Client namespace owns server adapter**
   - `include/stellar/client/LocalServerBridge.hpp`
   - `src/client/LocalServerBridge.cpp`
   - This code owns `WorldSession` / `ScriptedWorldSession`, validates hello/map/protocol, advances fixed ticks, and emits server packets. That is server-host behavior, not client behavior.

2. **Client runtime target links too much**
   - Current `stellar_client_runtime` includes:
     - `LocalServerBridge.cpp`
     - `NetworkedClientRuntime.cpp`
     - `LocalLoopbackRuntime.cpp`
     - presentation helpers
     - script registry loader
   - It links `stellar_server_core`, `stellar_scripting`, `stellar_network`, and `stellar_graphics`.
   - This makes “client runtime” mean presentation + authority + scripting + server bridge.

3. **Network protocol depends on server and scripting implementation types**
   - `include/stellar/network/Messages.hpp` includes:
     - `stellar/server/WorldSession.hpp`
     - `stellar/scripting/ScriptCommandProcessor.hpp`
     - `stellar/scripting/ScriptError.hpp`
     - `stellar/scripting/ScriptValue.hpp`
   - Protocol DTOs use `stellar::server::*` directly.
   - This prevents a truly standalone client protocol layer.

4. **Authority preparation is duplicated**
   - Client startup code loads/validates maps, builds runtime worlds, loads script registries, and constructs scripted authority for local play.
   - Dedicated server startup repeats similar map/script/world/session preparation.
   - This should be factored into one authority bootstrap module.

5. **Application mode branching is duplicated**
   - `Application::run()` has separate but nearly identical branches for `networked_runtime` and `remote_runtime`.
   - A single client-facing runtime interface should drive presentation.

6. **Single-player is currently modeled as local networked client/server**
   - That was valuable for socket-transport bring-up.
   - Going forward, normal single-player should not start a server bridge or perform session handshakes.
   - Single-player should still use authoritative simulation code, but through a local session facade rather than through network transport.

---

# Target Module Layout

## Proposed production targets

### `stellar_protocol`

Pure transport/protocol DTOs and codecs.

Owns:

- `ProtocolVersion`
- `SessionId`
- `ConnectionId`
- `PlayerId` if kept protocol-level
- `ClientHello`
- `ServerWelcome`
- `NetworkPlayerCommand`
- `NetworkWorldSnapshot`
- `NetworkGameplayEntity`
- `GameplayEvent`
- snapshot/delta/event/session codecs

Must not depend on:

- `stellar_server_core`
- `stellar_scripting`
- `stellar_client_runtime`
- `stellar_graphics`
- `stellar_audio`
- `stellar_platform`

Possible implementation path:

- Either rename/split current `stellar_network`, or introduce `stellar_protocol` first and migrate message/codec ownership gradually.
- Keep transport classes in `stellar_network` or move them to `stellar_transport`.

### `stellar_transport`

Transport interfaces and implementations.

Owns:

- `ClientTransport`
- `ServerTransport`
- `LoopbackTransportPair`
- TCP/POSIX socket transport
- endpoint parsing/formatting
- packet envelope

Depends on:

- `stellar_protocol` only, plus low-level platform/system headers.

Must not depend on:

- server runtime/session
- scripting
- client presentation

### `stellar_authority`

Backend-neutral authoritative gameplay runtime and bootstrapping.

Owns:

- shared BSP map validation/loading for authority
- shared script-root resolution and script registry loading
- `AuthorityLoadConfig`
- `PreparedAuthority`
- `AuthorityDiagnostics`
- `AuthoritySession`
- server-side conversion from `WorldSnapshot` / script results to protocol snapshots/events

Depends on:

- `stellar_world`
- `stellar_server_core`
- `stellar_scripting`
- `stellar_import_bsp`
- `stellar_protocol`

Must not depend on:

- `stellar_client_runtime`
- graphics
- audio
- platform window/input

### `stellar_server_runtime`

Network server host/session lifecycle.

Owns:

- `ServerRuntime`
- `ServerHost`
- session acceptance/rejection
- connection state
- snapshot scheduling
- command ingestion
- client assignment
- server pump loop primitives
- listen-server and dedicated-server reusable core

Depends on:

- `stellar_authority`
- `stellar_transport`
- `stellar_protocol`

Must not depend on:

- client presentation
- graphics
- audio
- window/input

### `stellar_dedicated_server`

Thin wrapper library around `stellar_server_runtime`.

Owns:

- `DedicatedServerConfig`
- `DedicatedServer`
- CLI parse helpers for `stellar-server`

Depends on:

- `stellar_server_runtime`

### `stellar_single_player`

Client-process local authoritative session for single-player.

Owns:

- `SinglePlayerRuntime`
- no socket listener
- no `ClientHello`/`ServerWelcome`
- direct authoritative simulation stepping
- local snapshot/event extraction for presentation

Depends on:

- `stellar_authority`
- `stellar_protocol`
- input command conversion if needed

Must not depend on:

- socket transport
- dedicated server CLI
- remote connection lifecycle

### `stellar_client_net`

Remote/listen client runtime.

Owns:

- `RemoteClientRuntime`
- client-side session state
- socket client transport connection
- client command sequencing
- `ClientWorldReceiver`

Depends on:

- `stellar_transport`
- `stellar_protocol`
- client input mapper

Must not depend on:

- `stellar_server_core`
- `stellar_scripting`
- `stellar_authority`

### `stellar_client_presentation`

Presentation-only client helpers.

Owns:

- `PlayerPresentation`
- `GameplayPresentation`
- `HudPresentation`
- audio event routing bridge if still client-facing
- render-view setup helpers

Depends on:

- `stellar_protocol`
- `stellar_graphics`
- `stellar_audio` as needed
- no authoritative runtime ownership

### `stellar_client_app`

Top-level client application.

Owns:

- CLI/application config
- mode selection
- platform window/input
- presentation frame loop
- selection between:
  - `SinglePlayerRuntime`
  - `RemoteClientRuntime`
  - `ListenHostClientRuntime`
  - static-less/no-map fallback

May link:

- `stellar_single_player`
- `stellar_client_net`
- `stellar_listen_server`
- `stellar_client_presentation`

But it should not contain map/script/server bootstrap code directly.

### `stellar_listen_server`

Client-hosted server mode.

Owns:

- `ListenServerHost`
- local host-player connection setup
- host lifetime binding to client application
- optional local loopback/in-process transport for host player
- optional socket listener for remote clients, preserving the current single-client limit unless expanded

Depends on:

- `stellar_server_runtime`
- `stellar_transport`
- `stellar_client_net` only through an interface if needed

---

# Proposed Runtime Mode Contracts

## Single-player mode

Command:

```bash
stellar-client --map path/to/map.bsp
```

Behavior:

- Load BSP and scripts through shared `stellar_authority` bootstrap.
- Create `SinglePlayerRuntime`.
- Capture input and convert to authoritative local command.
- Tick authoritative simulation directly in-process.
- Produce `NetworkWorldSnapshot`/`GameplayEvent` DTOs for presentation, or a presentation-neutral snapshot with an adapter into the same DTO shape.
- Render static level geometry from the locally loaded BSP.
- Do not:
  - open a socket
  - create `ServerTransport`
  - send `ClientHello`
  - wait for `ServerWelcome`
  - instantiate `RemoteClientRuntime`
  - instantiate `LocalServerBridge`

Acceptance test:

- `stellar-client --map fixture.bsp --validate-config` validates map/script config display-free.
- Preparing single-player runtime does not construct transport/session handshake objects.
- A single frame produces player/camera presentation data from local authoritative simulation.

## Remote client mode

Command:

```bash
stellar-client --connect 127.0.0.1:29070 --client-name Mike
```

Behavior:

- Create socket client transport.
- Send `ClientHello`.
- Wait for accepted `ServerWelcome`.
- Send input commands only after acceptance.
- Drain snapshots/events into `ClientWorldReceiver`.
- Render dynamic/fallback state until future presentation-map or map transfer work exists.
- Do not:
  - load BSP as authoritative map
  - load Lua scripts
  - build `RuntimeWorld`
  - build `WorldSession`
  - build `ScriptedWorldSession`
  - link to server/scripting in the remote client runtime target

Acceptance test:

- Link-audit or compile-boundary test proves `stellar_client_net` does not depend on `stellar_server_core` or `stellar_scripting`.
- `--connect` plus `--script-root` remains invalid.
- `--connect` plus `--map` remains invalid unless a later explicit `--presentation-map` is introduced.

## Listen-server mode

Proposed command:

```bash
stellar-client --host --map path/to/map.bsp --listen 127.0.0.1:29070
```

Behavior:

- Start `ListenServerHost` in the client process.
- Server host uses the same `stellar_server_runtime` core as `stellar-server`.
- Host client connects through client-facing session/runtime APIs.
- Server lifetime is bound to the host client process.
- When host client exits:
  - listener closes
  - accepted clients disconnect
  - server runtime is destroyed
- Preserve one active authoritative player/client limit unless explicitly expanded.

Initial implementation choice:

- For the host player, prefer an in-process transport pair rather than localhost socket to avoid flaky self-connect tests.
- For remote clients, reuse TCP listener when `--listen` is provided.
- If current transport supports only one accepted TCP client, document that initial listen mode supports either host-only or host-as-the-single-connected-client until multi-client is scoped.

Acceptance test:

- `stellar-client --host --map fixture.bsp --listen 127.0.0.1:0` can prepare a listen host display-free or in bounded integration form.
- Host shutdown destroys the server host.
- Dedicated server and listen server share the same authority prep and session lifecycle code.

## Dedicated server mode

Command:

```bash
stellar-server --map path/to/map.bsp --listen 0.0.0.0:29070
```

Behavior:

- Thin CLI over `stellar_server_runtime`.
- Uses shared `stellar_authority` bootstrap.
- Exists independently of clients.
- Does not link client runtime/presentation.
- Keeps existing validate-only behavior.

Acceptance test:

- `stellar-server --validate-config --map fixture.bsp` still works display-free.
- Link-audit test proves `stellar_dedicated_server` does not depend on `stellar_client_runtime`.

---

# Implementation Phases

## Phase CS-0 — Architecture baseline and guardrails

Goal: mark this branch’s active work as client/server decoupling and add dependency guardrails before moving code.

Tasks:

1. Create `Plans/ClientServerSplit-AgentPlan.md` from this proposal.
2. Update `Plans/NEXT.md`:
   - active focus is client/server decoupling
   - completed socket transport work remains historical and should not be restarted
   - target runtime modes are single-player, remote client, listen server, and dedicated server
3. Update `docs/ImplementationStatus.md` with a new active scope section:
   - current problem statement
   - intended module boundaries
   - phase checklist
4. Add a CMake/dependency audit note to docs:
   - protocol must not link server/scripting/client
   - remote client runtime must not link authority/server/scripting
   - dedicated server must not link client runtime/presentation
5. Add TODO-free or tracked TODO conventions in the plan. Do not scatter broad TODO comments through source.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Parallel-safe:

- Documentation updates can run in parallel with read-only build graph audit.

---

## Phase CS-1 — Extract protocol DTOs away from server/scripting implementation types

Goal: make the protocol layer standalone.

Tasks:

1. Introduce `include/stellar/protocol/Types.hpp` or `include/stellar/network/ProtocolTypes.hpp`.
2. Move or define plain DTOs:
   - `PlayerId`
   - `EntityId`
   - `EntityKind`
   - `TransformState`
   - `MovementCommand`
   - `PlayerSnapshot`
   - `GameplayEntityMetadata`
   - `NetworkGameplayEntity`
   - `NetworkWorldSnapshot`
   - `GameplayEvent`
3. Update `Messages.hpp` to stop including:
   - `stellar/server/WorldSession.hpp`
   - scripting headers
4. Move conversion functions out of the protocol header:
   - `make_network_snapshot(WorldSnapshot)` moves to `stellar_authority` or `stellar_server_runtime`.
   - `make_gameplay_events(script_events, command_results, script_errors)` moves to authority-side event conversion.
5. Keep wire encoding/decoding stable.
6. Update codec tests to use protocol DTOs directly.
7. Update server-side code to convert authoritative snapshots/events into protocol DTOs before encoding.
8. Update presentation code to consume protocol DTOs without server includes.

Target build boundary:

```cmake
target_link_libraries(stellar_protocol PUBLIC stellar_assets_if_needed_only)
target_link_libraries(stellar_transport PUBLIC stellar_protocol)
```

Do not link `stellar_protocol` to `stellar_server_core` or `stellar_scripting`.

Validation:

```bash
cmake --build build --target stellar_snapshot_codec_test stellar_network_session_test stellar_client_world_receiver_test -j$(nproc)
ctest --test-dir build -R '^(snapshot_codec|network_session|client_world_receiver|snapshot_delta)' --output-on-failure
```

Parallel-safe:

- Codec tests and DTO mechanical migration can run in parallel with authority conversion implementation after names are agreed.

---

## Phase CS-2 — Extract shared authority bootstrap

Goal: remove duplicated map/script/runtime preparation from client config and dedicated server implementation.

Tasks:

1. Add:
   - `include/stellar/server/AuthorityBootstrap.hpp` or `include/stellar/authority/AuthorityBootstrap.hpp`
   - `src/server/AuthorityBootstrap.cpp` or `src/authority/AuthorityBootstrap.cpp`
2. Define:
   - `AuthorityLoadConfig`
   - `AuthorityValidationResult`
   - `PreparedAuthority`
   - `AuthorityScriptLoadResult`
   - `AuthorityError` or use existing `stellar::platform::Error`
3. Move shared logic from:
   - `src/client/ApplicationConfig.cpp`
   - `src/server/DedicatedServer.cpp`
4. Shared bootstrap must own:
   - `.bsp` extension validation
   - `import::bsp::validate_level`
   - `RuntimeWorld` construction
   - map identity creation
   - script id discovery
   - script path sandbox checks
   - script source loading
   - `WorldSession` / `ScriptedWorldSession` construction
   - diagnostics for loaded scripts and script errors
5. Client config should only parse and validate mode-level rules.
6. Dedicated server should call the shared bootstrap.
7. Single-player/listen server work will reuse this bootstrap later.

Important detail:

- Do not put this bootstrap in `stellar_client_runtime`.
- Do not make it depend on graphics, audio, SDL, or client presentation.

Validation:

```bash
cmake --build build --target stellar_dedicated_server_test stellar_client_map_validation_smoke -j$(nproc)
ctest --test-dir build -R '^(dedicated_server|client_map_validation_smoke|client_cli_map_validation|bsp_validation)' --output-on-failure
```

Parallel-safe:

- Dedicated server refactor and client config refactor can proceed in parallel if the shared `AuthorityLoadConfig` API is landed first.

---

## Phase CS-3 — Move `LocalServerBridge` out of the client and generalize it as server runtime

Goal: remove server authority ownership from the client namespace.

Tasks:

1. Rename/move:
   - `include/stellar/client/LocalServerBridge.hpp`
   - `src/client/LocalServerBridge.cpp`

   to something like:

   - `include/stellar/server/ServerRuntime.hpp`
   - `src/server/ServerRuntime.cpp`

   or:

   - `include/stellar/server/LoopbackServerHost.hpp`
   - `src/server/LoopbackServerHost.cpp`

2. Replace `LocalServerBridgeConfig` with server-owned config:
   - `ServerRuntimeConfig`
   - `ServerSessionConfig`
   - `snapshot_rate`
   - `max_ticks_per_pump`
   - `emit_deltas`
   - map identity
3. Preserve current handshake behavior:
   - reject protocol mismatch
   - reject explicit map mismatch
   - assign authoritative player id
   - ignore/reject input before welcome
   - emit first full snapshot before deltas
4. Move snapshot/event conversion into server runtime or authority conversion module.
5. Update existing tests:
   - old client `networked_client_runtime` tests should not be the only coverage for server loop behavior.
   - add `server_runtime` tests under `tests/server`.
6. Make dedicated server use this runtime rather than its own copy of handshake/pump/snapshot logic.
7. Keep single-client limitation explicit.

Acceptance criteria:

- No `LocalServerBridge` symbol remains in `include/stellar/client` or `src/client`.
- No server-owned `WorldSession` / `ScriptedWorldSession` variant lives under `stellar::client`.
- Dedicated server and listen/local server share one server runtime implementation.

Validation:

```bash
cmake --build build --target stellar_server_runtime_test stellar_dedicated_server_test stellar_networked_client_runtime_test -j$(nproc)
ctest --test-dir build -R '^(server_runtime|dedicated_server|networked_client_runtime|network_session|snapshot_)' --output-on-failure
```

Parallel-safe:

- Server runtime tests can be written while production code is being moved.
- Dedicated-server integration should wait until server runtime API stabilizes.

---

## Phase CS-4 — Introduce a single client-facing runtime interface

Goal: make the application loop consume one presentation-facing interface regardless of mode.

Tasks:

1. Add `include/stellar/client/ClientRuntime.hpp`.
2. Define a non-authoritative client-facing interface or value-wrapper pattern:

```cpp
enum class ClientRuntimeMode {
    kNone,
    kSinglePlayer,
    kRemoteClient,
    kListenHost,
};

struct ClientRuntimeFrame {
    std::optional<stellar::protocol::NetworkWorldSnapshot> snapshot;
    std::vector<stellar::protocol::GameplayEvent> events;
    stellar::protocol::PlayerId local_player_id = 0;
    stellar::network::SessionState session_state = stellar::network::SessionState::kDisconnected;
    std::vector<std::string> diagnostics;
};

class IClientRuntime {
public:
    virtual ~IClientRuntime() = default;
    virtual ClientRuntimeFrame update(const stellar::platform::Input& input,
                                      float delta_seconds) noexcept = 0;
    virtual ClientRuntimeMode mode() const noexcept = 0;
};
```

3. Adapt existing runtime wrappers:
   - `RemoteClientRuntime` implements or adapts to `IClientRuntime`.
   - `NetworkedClientRuntime` is temporarily adapted, then removed/replaced by single-player/listen implementations.
4. Refactor `Application::run()`:
   - one update path
   - one snapshot-to-camera path
   - one gameplay event path
   - one presentation-state path
5. Keep no-map/static-less fallback as `runtime == nullptr` or `ClientRuntimeMode::kNone`.

Acceptance criteria:

- `Application::run()` no longer has duplicated `networked_runtime` and `remote_runtime` branches.
- Client presentation never needs to know whether state came from single-player, listen, or remote.

Validation:

```bash
cmake --build build --target stellar-client stellar_gameplay_presentation_test stellar_player_presentation_test stellar_hud_presentation_test -j$(nproc)
ctest --test-dir build -R '^(gameplay_presentation|player_presentation|hud_presentation|client_connect|networked_client_runtime)' --output-on-failure
```

Parallel-safe:

- Interface/header work can run in parallel with application-loop cleanup once DTO names are stable.

---

## Phase CS-5 — Add true single-player runtime without server startup

Goal: make `stellar-client --map` a standard single-player client mode that does not start a server/listener/session handshake.

Tasks:

1. Add:
   - `include/stellar/client/SinglePlayerRuntime.hpp`
   - `src/client/SinglePlayerRuntime.cpp`
   or place under `stellar_single_player`.
2. `SinglePlayerRuntime` should:
   - own `PreparedAuthority` or a smaller `AuthoritySession`
   - convert local input to authoritative commands
   - tick simulation directly
   - collect script events/errors/command results
   - expose protocol snapshot/event DTOs to presentation
3. It must not:
   - use `ClientTransport`
   - use `ServerTransport`
   - send `ClientHello`
   - receive `ServerWelcome`
   - bind/listen sockets
   - create a server host object
4. Update CLI mode rules:
   - `--map` alone = single-player
   - `--host --map` = listen server
   - `--connect` = remote
   - `--map --connect` remains invalid unless future `--presentation-map` is added
   - `--script-root` valid for `--map` and `--host --map`, invalid for `--connect`
5. Update `prepare_application_runtime()` or replace it with `create_client_runtime(config)`.
6. Remove `NetworkedClientRuntime` from the default single-player path.

Acceptance tests:

- `client_single_player_runtime`:
  - first frame emits snapshot
  - movement input changes player snapshot
  - scripted pickup/door events surface to presentation events
  - no session handshake diagnostics are generated
- `client_map_validation_smoke`:
  - mapped runtime mode is single-player
  - no-map fallback creates no runtime
- A lightweight instrumentation/test seam proves no `ClientHello` is sent in single-player.

Validation:

```bash
cmake --build build --target stellar-client stellar_client_single_player_runtime_test stellar_client_map_validation_smoke -j$(nproc)
ctest --test-dir build -R '^(client_single_player_runtime|client_map_validation_smoke|client_cli_map_validation|scripted_world_session|server_world_session)' --output-on-failure
```

Parallel-safe:

- Single-player runtime can be implemented while listen-server mode is designed, because it depends only on shared authority bootstrap and client runtime interface.

---

## Phase CS-6 — Add listen-server host mode

Goal: support client-hosted server mode with server lifetime tied to the host client.

Tasks:

1. Add:
   - `include/stellar/server/ListenServerHost.hpp` or `include/stellar/client/ListenHostRuntime.hpp`
   - `src/server/ListenServerHost.cpp`
   - optional `src/client/ListenHostRuntime.cpp`
2. Reuse:
   - `stellar_authority`
   - `stellar_server_runtime`
   - `stellar_transport`
   - `RemoteClientRuntime` / client network runtime for host-facing client side
3. Add CLI:
   - `--host`
   - `--listen HOST:PORT` optional, default `127.0.0.1:29070`
   - `--host` requires `--map`
   - `--host` conflicts with `--connect`
4. Initial host-player connection strategy:
   - Prefer in-process loopback transport for host player.
   - If current server transport can only accept one client, document whether host consumes that slot.
5. Host lifetime behavior:
   - Client app owns `ListenHostRuntime`.
   - Destroying client runtime destroys server host and closes listener.
   - Server errors propagate as client diagnostics.
6. Keep single-client/single-active-player limitation explicit.
7. Add status reporting:
   - bound endpoint
   - accepted clients
   - session state
   - map id
   - server tick

Acceptance tests:

- `listen_server_host`:
  - validates host config
  - starts on ephemeral local endpoint
  - host player receives welcome/snapshot
  - input updates authoritative state
  - host destruction closes transport
- CLI conflict tests:
  - `--host` without `--map` fails
  - `--host --connect` fails
  - `--host --script-root` succeeds when map scripts require it

Validation:

```bash
cmake --build build --target stellar-client stellar_listen_server_host_test stellar_client_connect_test stellar_dedicated_server_test -j$(nproc)
ctest --test-dir build -R '^(listen_server_host|client_connect|dedicated_server|server_runtime|socket_transport|network_session)' --output-on-failure
```

Parallel-safe:

- CLI parsing tests can be written in parallel with listen host implementation.
- Host-runtime rendering integration should wait for the client runtime interface from CS-4.

---

## Phase CS-7 — Remove legacy local loopback runtime and client-owned authority leftovers

Goal: delete or archive code that was only useful for the old coupled local path.

Tasks:

1. Audit active source for:
   - `LocalLoopbackRuntime`
   - `LocalServerBridge`
   - `networked_runtime` as default local mapped path
   - client includes of `WorldSession.hpp`
   - client includes of `ScriptedWorldSession.hpp`
   - protocol includes of server/scripting headers
2. Remove or move:
   - `include/stellar/client/LocalLoopbackRuntime.hpp`
   - `src/client/LocalLoopbackRuntime.cpp`
   - old tests if fully replaced
3. Rename `NetworkedClientRuntime` if it now only means remote network client:
   - `RemoteClientRuntime`
   - `ClientNetworkRuntime`
   - `ConnectedClientRuntime`
4. Ensure `stellar_client_net` does not link server/scripting.
5. Ensure `stellar_client_presentation` does not link server/scripting.
6. Keep tests for loopback transport under network/transport, not client runtime.
7. Keep any useful local transport tests independent of client authority.

Audit commands:

```bash
git grep -n 'LocalLoopbackRuntime\|LocalServerBridge\|networked_runtime' -- include src tests ':!Plans/Archived/**'
git grep -n 'stellar/server/WorldSession.hpp\|stellar/scripting/ScriptedWorldSession.hpp' -- include/stellar/client src/client include/stellar/network src/network
```

Validation:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Parallel-safe:

- Removal should not start until CS-5 and CS-6 tests cover the replacement behavior.

---

## Phase CS-8 — Build graph enforcement

Goal: make the new architecture hard to regress.

Tasks:

1. Update root `CMakeLists.txt` target graph.
2. Split current targets approximately as:

```cmake
add_library(stellar_protocol ...)
add_library(stellar_transport ...)
add_library(stellar_authority ...)
add_library(stellar_server_runtime ...)
add_library(stellar_dedicated_server ...)
add_library(stellar_single_player ...)
add_library(stellar_client_net ...)
add_library(stellar_client_presentation ...)
add_library(stellar_client_app ...)
```

3. Add optional CMake assertions or documented grep checks:
   - `stellar_protocol` has no server/scripting/client link libraries.
   - `stellar_client_net` has no authority/server/scripting link libraries.
   - `stellar_dedicated_server` has no client link libraries.
4. Add CI-friendly script if desired:

```bash
tools/dev/check_target_boundaries.sh
```

Suggested checks:

```bash
cmake --build build --target help | grep -E 'stellar_(protocol|transport|authority|server_runtime|client_net|single_player)'
git grep -n 'stellar/server/' -- include/stellar/network src/network include/stellar/client src/client
git grep -n 'stellar/scripting/' -- include/stellar/network src/network include/stellar/client src/client
```

Acceptance criteria:

- Remote-client runtime target builds without server/scripting.
- Dedicated-server target builds without client runtime.
- Protocol/transport tests build before server/client app targets.

Validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_protocol_test stellar_transport_test stellar_client_connect_test stellar_dedicated_server_test -j$(nproc)
ctest --test-dir build -R '^(protocol|transport|client_connect|dedicated_server|server_runtime|client_single_player_runtime|listen_server_host)' --output-on-failure
```

Parallel-safe:

- Build graph split should be serialized with source moves to avoid CMake churn conflicts.

---

## Phase CS-9 — Documentation update and final handoff

Goal: make docs match the new architecture and mode behavior.

Docs to update:

1. `docs/Design.md`
   - Project overview: client/server architecture now has explicit runtime modes.
   - Documentation authority: current branch should be `client-server-split`.
   - Current Branch Direction: replace stale TrenchBroom branch direction with client/server split scope.
   - System Architecture: show client presentation, shared protocol/transport, authority runtime, server host, dedicated server.
   - Core Engine / Application and Server Entry Points:
     - client entry
     - single-player entry
     - listen server
     - dedicated server
   - Networking and Client-Server Model:
     - remote client mode
     - listen server mode
     - dedicated server mode
     - single-player no-server mode
     - explicit deferred items

2. `docs/ImplementationStatus.md`
   - Add active completed/ongoing section:
     - CS-0 through CS-9
     - validation results
     - final architecture summary

3. `Plans/NEXT.md`
   - Point to the new active/archived plan.
   - List next follow-up options after decoupling:
     - presentation-map workflow
     - interpolation
     - prediction/reconciliation
     - true multiplayer
     - UDP
     - map transfer/caching

4. New optional doc: `docs/ClientServerArchitecture.md`
   - Recommended because this architecture is now important enough to deserve a focused reference.
   - Include:
     - mode matrix
     - ownership table
     - target graph
     - CLI examples
     - deferred features
     - testing strategy

Suggested mode matrix:

| Mode | Command | Authority owner | Socket listener | Client loads scripts | Lifetime |
|---|---|---|---:|---:|---|
| No-map/static-less | `stellar-client` | none | no | no | client |
| Single-player | `stellar-client --map map.bsp` | in-process single-player authority facade | no | authority-side in process | client |
| Remote client | `stellar-client --connect host:port` | remote server process | no | no | client |
| Listen server | `stellar-client --host --map map.bsp --listen host:port` | in-process server host | yes | authority-side in host | host client |
| Dedicated server | `stellar-server --map map.bsp --listen host:port` | server process | yes | authority-side in server | server process |

Final validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

ctest --test-dir build -R '^(protocol|transport|network_session|socket_transport|server_runtime|dedicated_server|listen_server_host|client_single_player_runtime|client_connect|client_map_validation_smoke|client_cli_map_validation|client_world_receiver|gameplay_presentation|player_presentation|hud_presentation|audio_event_router|bsp_|runtime_world|server_world_session|scripted_world_session)' --output-on-failure

git grep -n 'LocalServerBridge\|LocalLoopbackRuntime' -- include src tests ':!Plans/Archived/**'
git grep -n 'stellar/server/WorldSession.hpp\|stellar/scripting/ScriptedWorldSession.hpp' -- include/stellar/network src/network include/stellar/client src/client ':!Plans/Archived/**'
```

Completion checklist:

- [ ] `stellar_protocol` has no dependency on server/scripting/client.
- [ ] `stellar_transport` has no dependency on server/scripting/client.
- [ ] Dedicated server and listen server share server runtime/session lifecycle code.
- [ ] Shared authority bootstrap removes duplicated map/script preparation.
- [ ] `stellar-client --map` uses single-player runtime with no server/listener/handshake.
- [ ] `stellar-client --connect` remains presentation-only and does not link authority/server/scripting through its runtime target.
- [ ] `stellar-client --host --map` starts listen server host whose lifetime is tied to the client.
- [ ] `stellar-server` remains independent of clients.
- [ ] `Application::run()` uses one client-facing runtime frame path.
- [ ] Legacy `LocalServerBridge` and `LocalLoopbackRuntime` are removed or moved out of client namespace.
- [ ] Docs and Plans/NEXT accurately describe runtime modes and deferred work.
- [ ] Full default CTest passes.

---

# Recommended Order for Agents

Use this order unless implementation reality forces a small detour:

1. CS-0 docs/guardrails.
2. CS-1 protocol DTO split.
3. CS-2 shared authority bootstrap.
4. CS-3 server runtime extraction.
5. CS-4 client runtime interface.
6. CS-5 single-player runtime.
7. CS-6 listen-server host mode.
8. CS-7 legacy removal.
9. CS-8 build graph enforcement.
10. CS-9 docs/final handoff.

Do not begin CS-7 deletion until CS-5 and CS-6 tests are passing.

---

# Risk Notes

## Biggest risk: protocol DTO migration

The protocol layer currently reuses server and scripting types directly. Untangling this may touch many files. Keep wire encoding stable and write codec/presentation tests first.

## Biggest design decision: single-player snapshot type

Two viable options:

1. Single-player runtime produces `NetworkWorldSnapshot` DTOs even without networking.
   - Pro: one presentation path.
   - Con: name is network-specific.
2. Rename DTOs to `WorldSnapshotDto` / `PresentationWorldSnapshot` and let network encode them.
   - Pro: cleaner long-term naming.
   - Con: larger rename.

Recommendation: keep `NetworkWorldSnapshot` through the first refactor to reduce churn, then optionally rename after behavior is stable.

## Listen-server host slot behavior

Current implementation is single-client/single-active-player. Decide early whether the host local player consumes the only client slot or whether listen mode is host-only until multiplayer is scoped. Document this explicitly.

Recommendation: initial CS-6 listen mode may support host-only or one connected host client. Do not imply true multiplayer until `WorldSession` supports multiple authoritative players.

## Map presentation for remote clients

Do not accidentally solve map transfer in this plan. Remote clients should keep the existing static-less/dynamic fallback unless a future `--presentation-map` or map cache plan is selected.

---

# Out of Scope

- True simultaneous multiplayer simulation.
- Multi-player spawn/team/session management.
- Client prediction.
- Reconciliation.
- Interpolation.
- UDP/unreliable transport.
- Authentication/encryption.
- Public Internet deployment.
- Matchmaking/lobbies.
- Map transfer/download/cache.
- Presentation-map workflow, unless added as a separate focused phase.
- Client-side gameplay scripting.
- Renderer/audio scripting bindings.
- Dynamic rigid bodies or third-party physics.
- Moving brush classes beyond current `func_door`/`func_button`.

