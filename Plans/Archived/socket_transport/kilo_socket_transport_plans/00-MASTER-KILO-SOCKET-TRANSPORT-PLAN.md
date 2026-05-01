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
